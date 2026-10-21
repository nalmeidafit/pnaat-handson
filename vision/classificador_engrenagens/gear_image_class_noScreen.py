#!/usr/bin/env python3
"""Classificador YOLO de engrenagens com publicação MQTT para o PNAAT."""

from __future__ import annotations

import argparse
import json
import os
import signal
import threading
import time
from datetime import datetime, timezone
from pathlib import Path

import cv2
import paho.mqtt.client as mqtt
from picamera2 import Picamera2
from ultralytics import YOLO


BASE_DIR = Path(__file__).resolve().parent
STATE_FILE = BASE_DIR / "vision_state.json"

ROI_X = 250
ROI_Y = 135
ROI_W = 150
ROI_H = 255

ROI_CONT_X = 130
ROI_CONT_Y = 65
ROI_CONT_W = 1
ROI_CONT_H = 140

THRESHOLD = 80
TRIGGER_PIXELS = 10
REARM_PIXELS = 3
REARM_FRAMES = 5

ESTADO_ARMADO = 0
ESTADO_AGUARDANDO_REARME = 1


def utc_now_iso() -> str:
    return datetime.now(timezone.utc).isoformat(timespec="milliseconds")


def load_count() -> int:
    try:
        data = json.loads(STATE_FILE.read_text(encoding="utf-8"))
        return max(0, int(data.get("camera_count", 0)))
    except (FileNotFoundError, ValueError, TypeError, json.JSONDecodeError):
        return 0


def save_count(count: int) -> None:
    tmp = STATE_FILE.with_suffix(".tmp")
    tmp.write_text(
        json.dumps({"camera_count": count}, indent=2),
        encoding="utf-8",
    )
    tmp.replace(STATE_FILE)


def normalize_result(class_name: str) -> str:
    value = class_name.strip().upper()

    if value in {"OK", "APROVADA", "APROVADO"}:
        return "APROVADA"

    if value in {
        "NOK",
        "NAO_OK",
        "NÃO_OK",
        "REJEITADA",
        "REJEITADO",
    }:
        return "REJEITADA"

    return "INCONCLUSIVA"


class MqttPublisher:
    def __init__(self, enabled: bool) -> None:
        self.enabled = enabled
        self.client: mqtt.Client | None = None
        self.connected = threading.Event()

        self.host = os.getenv("PNAAT_MQTT_HOST", "127.0.0.1")
        self.port = int(os.getenv("PNAAT_MQTT_PORT", "1883"))
        self.username = os.getenv("PNAAT_MQTT_USERNAME", "pnaat")
        self.password = os.getenv("PNAAT_MQTT_PASSWORD", "pnaat123")
        self.prefix = os.getenv("PNAAT_TOPIC_PREFIX", "pnaat").strip("/")

        if not enabled:
            print("[MQTT] desabilitado por --no-mqtt")
            return

        try:
            self.client = mqtt.Client(
                mqtt.CallbackAPIVersion.VERSION2,
                client_id="pnaat-vision-yolo",
            )
        except Exception:
            self.client = mqtt.Client(client_id="pnaat-vision-yolo")

        self.client.on_connect = self._on_connect
        self.client.on_disconnect = self._on_disconnect
        self.client.reconnect_delay_set(min_delay=1, max_delay=10)

        if self.username:
            self.client.username_pw_set(self.username, self.password)

        self.client.connect(self.host, self.port, 30)
        self.client.loop_start()

        if not self.connected.wait(timeout=10.0):
            self.close()
            raise RuntimeError(
                f"MQTT não confirmou conexão com "
                f"{self.host}:{self.port} em 10 s."
            )

    def _on_connect(
        self,
        client,
        userdata,
        flags,
        reason_code,
        properties=None,
    ):
        del client, userdata, flags, properties

        code = getattr(reason_code, "value", reason_code)

        if code == 0:
            self.connected.set()
            print(
                f"[MQTT] conectado em "
                f"{self.host}:{self.port}"
            )
        else:
            self.connected.clear()
            print(
                "[MQTT] conexão recusada: "
                f"reason_code={reason_code}"
            )

    def _on_disconnect(
        self,
        client,
        userdata,
        disconnect_flags,
        reason_code,
        properties=None,
    ):
        del client, userdata, disconnect_flags, properties
        self.connected.clear()

        print(
            "[MQTT] desconectado: "
            f"reason_code={reason_code}"
        )

    def publish_json(
        self,
        suffix: str,
        payload: dict,
        retain: bool = False,
    ) -> bool:
        if not self.enabled or self.client is None:
            return False

        topic = f"{self.prefix}/{suffix}"

        if not self.connected.wait(timeout=5.0):
            print(
                "[MQTT] sem conexão; "
                f"publicação ignorada: {topic}"
            )
            return False

        info = self.client.publish(
            topic,
            json.dumps(
                payload,
                ensure_ascii=False,
            ),
            qos=1,
            retain=retain,
        )

        if info.rc != mqtt.MQTT_ERR_SUCCESS:
            print(
                f"[MQTT] falha ao publicar "
                f"{topic}: rc={info.rc}"
            )
            return False

        try:
            info.wait_for_publish(timeout=5.0)
        except RuntimeError as exc:
            print(
                "[MQTT] publicação não concluída "
                f"em {topic}: {exc}"
            )
            return False

        if not info.is_published():
            print(
                "[MQTT] timeout aguardando "
                f"confirmação de {topic}"
            )
            return False

        return True

    def publish_count(self, count: int) -> bool:
        return self.publish_json(
            "contagem/camera",
            {
                "count": count,
                "source": "camera_yolo",
            },
            retain=True,
        )

    def publish_result(
        self,
        count: int,
        result: str,
        confidence: float,
        model_class: str,
    ) -> bool:
        return self.publish_json(
            "visao/resultado",
            {
                "id_camera": count,
                "resultado": result,
                "confianca": round(
                    float(confidence),
                    6,
                ),
                "classe_modelo": model_class,
                "source": "camera_yolo",
                "timestamp_utc": utc_now_iso(),
            },
            retain=False,
        )

    def close(self) -> None:
        if self.client is not None:
            self.client.loop_stop()

            try:
                self.client.disconnect()
            except Exception:
                pass


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description=(
            "Classificação YOLO de "
            "engrenagens com MQTT."
        )
    )

    parser.add_argument(
        "--model",
        default=os.getenv(
            "PNAAT_MODEL_PATH",
            str(BASE_DIR / "best.pt"),
        ),
        help="Caminho para o modelo YOLO best.pt.",
    )

    parser.add_argument(
        "--no-mqtt",
        action="store_true",
        help=(
            "Executa classificação local "
            "sem publicar MQTT."
        ),
    )

    return parser.parse_args()


def main() -> None:
    args = parse_args()

    model_path = Path(
        args.model
    ).expanduser().resolve()

    if not model_path.exists():
        raise SystemExit(
            f"Modelo não encontrado: {model_path}"
        )

    print(
        f"[AI] carregando modelo: {model_path}"
    )

    model = YOLO(str(model_path))
    classes = model.names

    publisher = MqttPublisher(
        enabled=not args.no_mqtt
    )

    camera_count = load_count()
    publisher.publish_count(camera_count)

    picam2 = Picamera2()

    config = picam2.create_preview_configuration(
        main={"size": (640, 480)}
    )

    picam2.configure(config)

    picam2.set_controls(
        {
            "ExposureTime": 7000,
            "AnalogueGain": 4.0,
            "AeEnable": False,
            "AwbEnable": False,
        }
    )

    print("[CAM] iniciando câmera...")
    picam2.start()
    time.sleep(2)

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

    estado = ESTADO_ARMADO
    frames_livres = 0

    print(
        "[CAM] contador restaurado: "
        f"{camera_count}"
    )

    print(
        "[CAM] trigger: >"
        f"{TRIGGER_PIXELS} pixels pretos; "
        "rearme: <="
        f"{REARM_PIXELS} pixels pretos por "
        f"{REARM_FRAMES} frames consecutivos"
    )

    print(
        "[CAM] aguardando peça. "
        "Ctrl+C para encerrar."
    )

    try:
        while running:
            frame = picam2.capture_array()

            roi = frame[
                ROI_Y : ROI_Y + ROI_H,
                ROI_X : ROI_X + ROI_W,
            ]

            gray = cv2.cvtColor(
                roi,
                cv2.COLOR_RGB2GRAY,
            )

            _, mascara = cv2.threshold(
                gray,
                THRESHOLD,
                255,
                cv2.THRESH_BINARY,
            )

            roi_contagem = mascara[
                ROI_CONT_Y :
                    ROI_CONT_Y + ROI_CONT_H,
                ROI_CONT_X :
                    ROI_CONT_X + ROI_CONT_W,
            ]

            pixels_brancos = cv2.countNonZero(
                roi_contagem
            )

            pixels_pretos = (
                roi_contagem.size -
                pixels_brancos
            )

            if (
                estado == ESTADO_ARMADO
                and
                pixels_pretos > TRIGGER_PIXELS
            ):
                resultados = model(
                    gray,
                    verbose=False,
                )

                probas = resultados[0].probs

                if probas is None:
                    print(
                        "[AI] modelo não retornou "
                        "probabilidades; peça ignorada"
                    )

                    estado = (
                        ESTADO_AGUARDANDO_REARME
                    )
                    frames_livres = 0
                    print(
                        "[CAM] aguardando saída da peça "
                        "para rearme"
                    )
                    continue

                class_id = int(probas.top1)

                confidence = float(
                    probas.top1conf.item()
                )

                model_class = str(
                    classes[class_id]
                )

                result = normalize_result(
                    model_class
                )

                camera_count += 1
                save_count(camera_count)

                print(
                    f"[AI] peça #{camera_count}: "
                    f"classe={model_class} "
                    f"resultado={result} "
                    f"confiança="
                    f"{confidence * 100:.1f}%"
                )

                publisher.publish_count(
                    camera_count
                )

                published = (
                    publisher.publish_result(
                        camera_count,
                        result,
                        confidence,
                        model_class,
                    )
                )

                if published:
                    print(
                        "[MQTT] resultado publicado "
                        "em pnaat/visao/resultado"
                    )

                estado = (
                    ESTADO_AGUARDANDO_REARME
                )
                frames_livres = 0

                print(
                    "[CAM] aguardando saída da peça "
                    "para rearme"
                )

            if (
                estado ==
                    ESTADO_AGUARDANDO_REARME
            ):
                if pixels_pretos <= REARM_PIXELS:
                    frames_livres += 1
                else:
                    frames_livres = 0

                if frames_livres >= REARM_FRAMES:
                    estado = ESTADO_ARMADO
                    frames_livres = 0

                    print(
                        "[CAM] sistema rearmado"
                    )

    finally:
        picam2.stop()
        publisher.close()

        print("[CAM] encerrado")


if __name__ == "__main__":
    main()
