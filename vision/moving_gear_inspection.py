#!/usr/bin/env python3
"""
moving_gear_inspection.py

PNAAT - rastreamento diagnóstico de engrenagens em movimento.

Objetivo desta etapa:
- detectar a engrenagem azul enquanto cruza a ROI;
- tratar vários frames como uma única peça;
- incrementar a contagem da câmera uma única vez por passagem;
- calcular métricas geométricas pela mediana dos frames válidos;
- publicar contagem e medições via MQTT;
- NÃO classificar APROVADA/REJEITADA ainda.

A classificação será adicionada somente depois da calibração com a estação
óptica definitiva e várias passagens de peças boas/defeituosas.
"""

from __future__ import annotations

import argparse
import json
import os
import signal
import time
from dataclasses import dataclass
from pathlib import Path
from statistics import median

import cv2
import numpy as np
from picamera2 import Picamera2

try:
    import paho.mqtt.client as mqtt
except ImportError:
    mqtt = None


HSV_LOWER = np.array([70, 45, 20], dtype=np.uint8)
HSV_UPPER = np.array([115, 255, 255], dtype=np.uint8)

ROI_X1 = 0.20
ROI_Y1 = 0.15
ROI_X2 = 0.80
ROI_Y2 = 0.95

MIN_OUTER_AREA = 3000
MAX_FRAME_AREA_FRACTION = 0.30

DEFAULT_WIDTH = 1296
DEFAULT_HEIGHT = 972
DEFAULT_FPS = 20.0
DEFAULT_EXPOSURE_US = 5000

START_DETECTED_FRAMES = 3
END_MISSING_FRAMES = 6
MIN_VALID_SAMPLES = 5

STATE_FILE = Path(__file__).with_name("vision_state.json")


@dataclass
class FrameMetrics:
    area_px2: float
    radius_px: float
    circularity: float
    aspect_ratio: float
    center_x_px: int
    center_y_px: int


def build_roi_mask(shape):
    height, width = shape[:2]

    x1 = int(width * ROI_X1)
    y1 = int(height * ROI_Y1)
    x2 = int(width * ROI_X2)
    y2 = int(height * ROI_Y2)

    mask = np.zeros((height, width), dtype=np.uint8)
    mask[y1:y2, x1:x2] = 255

    return mask, (x1, y1, x2, y2)


def touches_roi_border(bbox, roi, margin=3):
    x, y, w, h = bbox
    rx1, ry1, rx2, ry2 = roi

    return (
        x <= rx1 + margin
        or y <= ry1 + margin
        or (x + w) >= rx2 - margin
        or (y + h) >= ry2 - margin
    )


def choose_gear_contour(contours, frame_shape, roi):
    frame_h, frame_w = frame_shape[:2]
    frame_area = frame_h * frame_w
    candidates = []

    for contour in contours:
        area = cv2.contourArea(contour)

        if area < MIN_OUTER_AREA:
            continue

        if area > frame_area * MAX_FRAME_AREA_FRACTION:
            continue

        bbox = cv2.boundingRect(contour)

        if touches_roi_border(bbox, roi):
            continue

        _, _, w, h = bbox
        aspect = w / h if h else 0.0

        if not (0.60 <= aspect <= 1.40):
            continue

        candidates.append((area, contour))

    if not candidates:
        return None

    return max(candidates, key=lambda item: item[0])[1]


def analyse_frame(frame):
    # Picamera2 RGB888 entrega [B, G, R], formato conveniente para OpenCV.
    hsv = cv2.cvtColor(frame, cv2.COLOR_BGR2HSV)

    mask = cv2.inRange(hsv, HSV_LOWER, HSV_UPPER)

    roi_mask, roi = build_roi_mask(frame.shape)
    mask = cv2.bitwise_and(mask, roi_mask)

    kernel = np.ones((3, 3), np.uint8)

    mask = cv2.morphologyEx(
        mask,
        cv2.MORPH_OPEN,
        kernel,
        iterations=1,
    )

    mask = cv2.morphologyEx(
        mask,
        cv2.MORPH_CLOSE,
        kernel,
        iterations=1,
    )

    contours, _ = cv2.findContours(
        mask,
        cv2.RETR_EXTERNAL,
        cv2.CHAIN_APPROX_SIMPLE,
    )

    contour = choose_gear_contour(
        contours,
        frame.shape,
        roi,
    )

    if contour is None:
        return None, mask, roi, None

    area = cv2.contourArea(contour)
    perimeter = cv2.arcLength(contour, True)

    x, y, w, h = cv2.boundingRect(contour)
    (_, _), radius = cv2.minEnclosingCircle(contour)

    moments = cv2.moments(contour)

    if moments["m00"] != 0:
        cx = int(moments["m10"] / moments["m00"])
        cy = int(moments["m01"] / moments["m00"])
    else:
        cx = x + w // 2
        cy = y + h // 2

    circularity = (
        4.0 * np.pi * area / (perimeter * perimeter)
        if perimeter > 0
        else 0.0
    )

    metrics = FrameMetrics(
        area_px2=float(area),
        radius_px=float(radius),
        circularity=float(circularity),
        aspect_ratio=float(w / h if h else 0.0),
        center_x_px=cx,
        center_y_px=cy,
    )

    return metrics, mask, roi, contour


def summarize_samples(samples):
    return {
        "area_px2": float(
            median(s.area_px2 for s in samples)
        ),
        "radius_px": float(
            median(s.radius_px for s in samples)
        ),
        "circularity": float(
            median(s.circularity for s in samples)
        ),
        "aspect_ratio": float(
            median(s.aspect_ratio for s in samples)
        ),
        "center_x_px": int(
            round(median(s.center_x_px for s in samples))
        ),
        "center_y_px": int(
            round(median(s.center_y_px for s in samples))
        ),
    }


def load_count():
    try:
        data = json.loads(
            STATE_FILE.read_text(encoding="utf-8")
        )
        return max(
            0,
            int(data.get("camera_count", 0)),
        )
    except (
        FileNotFoundError,
        ValueError,
        TypeError,
        json.JSONDecodeError,
    ):
        return 0


def save_count(count):
    temp = STATE_FILE.with_suffix(".tmp")

    temp.write_text(
        json.dumps(
            {"camera_count": count},
            indent=2,
        ),
        encoding="utf-8",
    )

    temp.replace(STATE_FILE)


class MqttPublisher:
    def __init__(self, enabled):
        self.enabled = enabled
        self.client = None
        self.prefix = os.getenv(
            "PNAAT_TOPIC_PREFIX",
            "pnaat",
        )

        if not enabled:
            print("[MQTT] desabilitado por --no-mqtt")
            return

        if mqtt is None:
            raise RuntimeError(
                "paho-mqtt não está instalado. "
                "Instale python3-paho-mqtt ou use --no-mqtt."
            )

        host = os.getenv(
            "PNAAT_MQTT_HOST",
            "127.0.0.1",
        )

        port = int(
            os.getenv("PNAAT_MQTT_PORT", "1883")
        )

        username = os.getenv(
            "PNAAT_MQTT_USERNAME",
            "",
        )

        password = os.getenv(
            "PNAAT_MQTT_PASSWORD",
            "",
        )

        try:
            self.client = mqtt.Client(
                mqtt.CallbackAPIVersion.VERSION2,
                client_id="pnaat-vision-moving",
            )
        except Exception:
            self.client = mqtt.Client(
                client_id="pnaat-vision-moving",
            )

        if username:
            self.client.username_pw_set(
                username,
                password,
            )

        self.client.connect(host, port, 30)
        self.client.loop_start()

        print(
            f"[MQTT] conectado em {host}:{port}"
        )

    def publish_json(
        self,
        suffix,
        payload,
        retain=False,
    ):
        if not self.enabled or self.client is None:
            return

        topic = f"{self.prefix}/{suffix}"

        info = self.client.publish(
            topic,
            json.dumps(
                payload,
                ensure_ascii=False,
            ),
            qos=1,
            retain=retain,
        )

        info.wait_for_publish()

    def publish_count(self, count):
        self.publish_json(
            "contagem/camera",
            {
                "count": count,
                "source": "camera",
            },
            retain=True,
        )

    def publish_measurement(
        self,
        count,
        samples,
        metrics,
    ):
        self.publish_json(
            "visao/medicao",
            {
                "id_camera": count,
                "source": "camera",
                "samples": samples,
                "metrics": metrics,
                "classificacao":
                    "PENDENTE_CALIBRACAO",
            },
            retain=False,
        )

    def close(self):
        if self.client is not None:
            self.client.loop_stop()
            self.client.disconnect()


def configure_camera(args):
    picam2 = Picamera2()

    config = picam2.create_video_configuration(
        main={
            "size": (
                args.width,
                args.height,
            ),
            "format": "RGB888",
        },
        controls={
            "FrameRate": args.fps,
        },
        buffer_count=4,
    )

    picam2.configure(config)
    picam2.start()

    # Deixa a câmera estabilizar antes de fixar a exposição.
    time.sleep(1.5)

    if args.exposure_us > 0:
        picam2.set_controls(
            {
                "AeEnable": False,
                "ExposureTime":
                    int(args.exposure_us),
                "AnalogueGain":
                    float(args.analogue_gain),
            }
        )

        print(
            "[CAM] exposição fixa: "
            f"{args.exposure_us} us, "
            f"gain={args.analogue_gain:.2f}"
        )
    else:
        print("[CAM] exposição automática")

    return picam2


def annotate_frame(
    frame,
    roi,
    contour,
    state,
    count,
    detected_streak,
    missing_streak,
):
    out = frame.copy()

    rx1, ry1, rx2, ry2 = roi

    cv2.rectangle(
        out,
        (rx1, ry1),
        (rx2, ry2),
        (180, 180, 180),
        2,
    )

    if contour is not None:
        cv2.drawContours(
            out,
            [contour],
            -1,
            (0, 255, 0),
            2,
        )

    cv2.putText(
        out,
        f"estado={state} camera_count={count}",
        (25, 35),
        cv2.FONT_HERSHEY_SIMPLEX,
        0.8,
        (255, 255, 255),
        2,
        cv2.LINE_AA,
    )

    cv2.putText(
        out,
        (
            f"detect={detected_streak} "
            f"missing={missing_streak}"
        ),
        (25, 70),
        cv2.FONT_HERSHEY_SIMPLEX,
        0.7,
        (255, 255, 255),
        2,
        cv2.LINE_AA,
    )

    return out


def parse_args():
    parser = argparse.ArgumentParser(
        description=(
            "Rastreamento diagnóstico de "
            "engrenagens em movimento."
        )
    )

    parser.add_argument(
        "--width",
        type=int,
        default=DEFAULT_WIDTH,
    )

    parser.add_argument(
        "--height",
        type=int,
        default=DEFAULT_HEIGHT,
    )

    parser.add_argument(
        "--fps",
        type=float,
        default=DEFAULT_FPS,
    )

    parser.add_argument(
        "--exposure-us",
        type=int,
        default=DEFAULT_EXPOSURE_US,
        help=(
            "0 mantém autoexposição; "
            "valor >0 fixa ExposureTime."
        ),
    )

    parser.add_argument(
        "--analogue-gain",
        type=float,
        default=1.0,
    )

    parser.add_argument(
        "--start-frames",
        type=int,
        default=START_DETECTED_FRAMES,
    )

    parser.add_argument(
        "--end-frames",
        type=int,
        default=END_MISSING_FRAMES,
    )

    parser.add_argument(
        "--min-samples",
        type=int,
        default=MIN_VALID_SAMPLES,
    )

    parser.add_argument(
        "--show",
        action="store_true",
        help=(
            "Mostra janela OpenCV. "
            "Pressione Q para sair."
        ),
    )

    parser.add_argument(
        "--no-mqtt",
        action="store_true",
        help="Executa somente diagnóstico local.",
    )

    return parser.parse_args()


def main():
    args = parse_args()

    if (
        args.start_frames < 1
        or args.end_frames < 1
        or args.min_samples < 1
    ):
        raise SystemExit(
            "start-frames, end-frames e "
            "min-samples devem ser >= 1."
        )

    running = True

    def stop_handler(_signum, _frame):
        nonlocal running
        running = False

    signal.signal(
        signal.SIGINT,
        stop_handler,
    )

    signal.signal(
        signal.SIGTERM,
        stop_handler,
    )

    count = load_count()

    publisher = MqttPublisher(
        enabled=not args.no_mqtt
    )

    picam2 = configure_camera(args)

    print()
    print(
        "=== PNAAT — ENGRENAGEM EM MOVIMENTO ==="
    )
    print(
        f"Resolução          : "
        f"{args.width} x {args.height}"
    )
    print(
        f"FPS solicitado     : {args.fps}"
    )
    print(
        f"Contagem restaurada: {count}"
    )
    print(
        "Entrada da peça    : "
        f"{args.start_frames} frames "
        "detectados consecutivos"
    )
    print(
        "Saída da peça      : "
        f"{args.end_frames} frames "
        "ausentes consecutivos"
    )
    print(
        f"Amostras mínimas   : "
        f"{args.min_samples}"
    )
    print("[CAM] aguardando peça")
    print()

    publisher.publish_count(count)

    state = "WAITING"
    detected_streak = 0
    missing_streak = 0
    candidate_samples = []
    active_samples = []

    try:
        while running:
            frame = picam2.capture_array()

            metrics, _mask, roi, contour = (
                analyse_frame(frame)
            )

            detected = metrics is not None

            if state == "WAITING":
                missing_streak = 0

                if detected:
                    detected_streak += 1
                    candidate_samples.append(
                        metrics
                    )

                    if (
                        detected_streak
                        >= args.start_frames
                    ):
                        state = "TRACKING"

                        active_samples = list(
                            candidate_samples
                        )

                        candidate_samples.clear()
                        missing_streak = 0

                        print(
                            "[CAM] peça detectada; "
                            "iniciando rastreamento "
                            f"({len(active_samples)} "
                            "amostras)"
                        )
                else:
                    detected_streak = 0
                    candidate_samples.clear()

            elif state == "TRACKING":
                if detected:
                    active_samples.append(
                        metrics
                    )

                    missing_streak = 0

                    if (
                        len(active_samples) == 1
                        or len(active_samples) % 5 == 0
                    ):
                        print(
                            "[CAM] coletando frame "
                            "válido "
                            f"{len(active_samples)}"
                        )
                else:
                    missing_streak += 1

                    if (
                        missing_streak
                        >= args.end_frames
                    ):
                        if (
                            len(active_samples)
                            >= args.min_samples
                        ):
                            count += 1
                            save_count(count)

                            summary = (
                                summarize_samples(
                                    active_samples
                                )
                            )

                            print()
                            print(
                                f"[CAM] peça #{count} "
                                "concluída"
                            )
                            print(
                                "[CAM] frames válidos: "
                                f"{len(active_samples)}"
                            )
                            print(
                                "Área mediana       : "
                                f"{summary['area_px2']:.1f} "
                                "px²"
                            )
                            print(
                                "Raio mediano       : "
                                f"{summary['radius_px']:.2f} "
                                "px"
                            )
                            print(
                                "Circularidade      : "
                                f"{summary['circularity']:.4f}"
                            )
                            print(
                                "Aspect ratio       : "
                                f"{summary['aspect_ratio']:.4f}"
                            )
                            print(
                                "Centro mediano     : "
                                f"({summary['center_x_px']}, "
                                f"{summary['center_y_px']})"
                            )
                            print(
                                "[CAM] classificação: "
                                "PENDENTE_CALIBRACAO"
                            )
                            print()

                            publisher.publish_count(
                                count
                            )

                            publisher.publish_measurement(
                                count,
                                len(active_samples),
                                summary,
                            )
                        else:
                            print(
                                "[CAM] passagem "
                                "descartada: apenas "
                                f"{len(active_samples)} "
                                "amostras válidas."
                            )

                        state = "WAITING"
                        detected_streak = 0
                        missing_streak = 0
                        active_samples.clear()
                        candidate_samples.clear()

                        print(
                            "[CAM] aguardando "
                            "próxima peça"
                        )

            if args.show:
                annotated = annotate_frame(
                    frame,
                    roi,
                    contour,
                    state,
                    count,
                    detected_streak,
                    missing_streak,
                )

                cv2.imshow(
                    (
                        "PNAAT - engrenagem "
                        "em movimento"
                    ),
                    annotated,
                )

                key = cv2.waitKey(1) & 0xFF

                if key in (
                    ord("q"),
                    ord("Q"),
                ):
                    running = False

    finally:
        picam2.stop()
        publisher.close()

        if args.show:
            cv2.destroyAllWindows()

        print()
        print("[CAM] encerrado.")


if __name__ == "__main__":
    main()
