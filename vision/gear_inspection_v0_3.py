#!/usr/bin/env python3
"""
gear_inspection_v0_3.py

Inspeção geométrica diagnóstica de engrenagens — PNAAT.

Mudanças da v0.3:
- Corrige a regressão da v0.2: o fundo claro estava entrando na máscara.
- Aumenta a saturação mínima de 30 para 45.
- Mantém V máximo em 255 para não perder regiões claras da peça.
- Aplica uma ROI central configurável para reduzir falsos contornos.
- Usa apenas contornos EXTERNOS para localizar a engrenagem nesta etapa.
- Adiciona verificações de sanidade para rejeitar contornos gigantes/bordas.
- Mantém perfil radial e métricas para calibração futura.
- A detecção do furo central fica adiada até a óptica definitiva estar fixa.

IMPORTANTE:
Esta versão NÃO classifica APROVADA/REJEITADA.
Os limites de aceitação serão calibrados depois que câmera, suporte,
altura, posição da peça e iluminação estiverem fixos.
"""

import argparse
import csv
import json
from pathlib import Path

import cv2
import numpy as np


HSV_LOWER = np.array([70, 45, 20], dtype=np.uint8)
HSV_UPPER = np.array([115, 255, 255], dtype=np.uint8)

MIN_OUTER_AREA = 5000
MAX_FRAME_AREA_FRACTION = 0.25
RADIAL_BINS = 360

ROI_X1 = 0.20
ROI_Y1 = 0.20
ROI_X2 = 0.80
ROI_Y2 = 0.95


def centroid(contour):
    m = cv2.moments(contour)
    if m["m00"] == 0:
        x, y, w, h = cv2.boundingRect(contour)
        return x + w // 2, y + h // 2

    cx = int(m["m10"] / m["m00"])
    cy = int(m["m01"] / m["m00"])
    return cx, cy


def radial_profile(contour, cx, cy, bins=360):
    profile = np.full(bins, np.nan, dtype=np.float64)
    points = contour.reshape(-1, 2)

    for x, y in points:
        dx = float(x - cx)
        dy = float(y - cy)
        radius = np.hypot(dx, dy)

        angle = np.degrees(np.arctan2(dy, dx))
        if angle < 0:
            angle += 360.0

        bin_idx = int(round(angle)) % bins

        if np.isnan(profile[bin_idx]) or radius > profile[bin_idx]:
            profile[bin_idx] = radius

    valid = ~np.isnan(profile)

    if valid.sum() >= 2:
        x = np.arange(bins)
        profile = np.interp(x, x[valid], profile[valid], period=bins)

    return profile


def build_roi_mask(shape):
    height, width = shape[:2]

    x1 = int(width * ROI_X1)
    y1 = int(height * ROI_Y1)
    x2 = int(width * ROI_X2)
    y2 = int(height * ROI_Y2)

    roi_mask = np.zeros((height, width), dtype=np.uint8)
    roi_mask[y1:y2, x1:x2] = 255

    return roi_mask, (x1, y1, x2, y2)


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


def analyse_gear(image_path: Path):
    img = cv2.imread(str(image_path))

    if img is None:
        raise RuntimeError(f"Não foi possível abrir a imagem: {image_path}")

    hsv = cv2.cvtColor(img, cv2.COLOR_BGR2HSV)

    mask = cv2.inRange(hsv, HSV_LOWER, HSV_UPPER)

    roi_mask, roi = build_roi_mask(img.shape)
    mask = cv2.bitwise_and(mask, roi_mask)

    kernel = np.ones((3, 3), np.uint8)

    mask = cv2.morphologyEx(mask, cv2.MORPH_OPEN, kernel, iterations=1)
    mask = cv2.morphologyEx(mask, cv2.MORPH_CLOSE, kernel, iterations=1)

    contours, _ = cv2.findContours(
        mask,
        cv2.RETR_EXTERNAL,
        cv2.CHAIN_APPROX_SIMPLE
    )

    outer = choose_gear_contour(contours, img.shape, roi)

    if outer is None:
        raise RuntimeError(
            "Nenhuma engrenagem válida foi encontrada dentro da ROI."
        )

    outer_area = cv2.contourArea(outer)
    perimeter = cv2.arcLength(outer, True)

    x, y, w, h = cv2.boundingRect(outer)
    cx, cy = centroid(outer)

    (circle_x, circle_y), outer_radius = cv2.minEnclosingCircle(outer)

    circularity = 0.0
    if perimeter > 0:
        circularity = 4.0 * np.pi * outer_area / (perimeter * perimeter)

    aspect_ratio = w / h if h else 0.0

    profile = radial_profile(outer, cx, cy, bins=RADIAL_BINS)

    profile_mean = float(np.mean(profile))
    profile_std = float(np.std(profile))
    profile_min = float(np.min(profile))
    profile_max = float(np.max(profile))

    annotated = img.copy()

    rx1, ry1, rx2, ry2 = roi
    cv2.rectangle(annotated, (rx1, ry1), (rx2, ry2), (180, 180, 180), 1)

    cv2.drawContours(annotated, [outer], -1, (0, 255, 0), 3)
    cv2.rectangle(annotated, (x, y), (x + w, y + h), (255, 0, 0), 2)
    cv2.circle(annotated, (cx, cy), 5, (0, 0, 255), -1)

    cv2.circle(
        annotated,
        (int(circle_x), int(circle_y)),
        int(outer_radius),
        (0, 255, 255),
        2
    )

    metrics = {
        "version": "0.3",
        "image": str(image_path),
        "image_width_px": int(img.shape[1]),
        "image_height_px": int(img.shape[0]),
        "segmentation": {
            "hsv_lower": HSV_LOWER.tolist(),
            "hsv_upper": HSV_UPPER.tolist(),
            "roi_fraction": [ROI_X1, ROI_Y1, ROI_X2, ROI_Y2],
            "roi_px": {
                "x1": rx1,
                "y1": ry1,
                "x2": rx2,
                "y2": ry2
            }
        },
        "outer_contour": {
            "area_px2": float(outer_area),
            "perimeter_px": float(perimeter),
            "bounding_box": {
                "x": int(x),
                "y": int(y),
                "width_px": int(w),
                "height_px": int(h)
            },
            "center_x_px": int(cx),
            "center_y_px": int(cy),
            "min_enclosing_radius_px": float(outer_radius),
            "aspect_ratio": float(aspect_ratio),
            "circularity": float(circularity)
        },
        "central_hole": {
            "status": "not_evaluated",
            "reason": (
                "Detecção adiada até fixação da câmera, "
                "iluminação e posição definitivas."
            )
        },
        "radial_profile": {
            "bins": RADIAL_BINS,
            "mean_radius_px": profile_mean,
            "std_radius_px": profile_std,
            "min_radius_px": profile_min,
            "max_radius_px": profile_max,
            "peak_to_peak_px": profile_max - profile_min
        }
    }

    output_dir = image_path.parent / "diagnostico_v03"
    output_dir.mkdir(parents=True, exist_ok=True)

    mask_path = output_dir / "mask.png"
    annotated_path = output_dir / "contorno.jpg"
    json_path = output_dir / "metricas.json"
    csv_path = output_dir / "perfil_radial.csv"

    cv2.imwrite(str(mask_path), mask)
    cv2.imwrite(str(annotated_path), annotated)

    with json_path.open("w", encoding="utf-8") as f:
        json.dump(metrics, f, indent=2, ensure_ascii=False)

    with csv_path.open("w", newline="", encoding="utf-8") as f:
        writer = csv.writer(f)
        writer.writerow(["angulo_graus", "raio_px"])

        for angle, radius in enumerate(profile):
            writer.writerow([angle, f"{radius:.4f}"])

    print()
    print("=== INSPEÇÃO DA ENGRENAGEM — v0.3 ===")
    print(f"Imagem             : {img.shape[1]} x {img.shape[0]}")
    print(f"Área externa       : {outer_area:.1f} px²")
    print(f"Perímetro          : {perimeter:.1f} px")
    print(f"Bounding box       : {w} x {h} px")
    print(f"Centro da peça     : ({cx}, {cy})")
    print(f"Raio externo       : {outer_radius:.1f} px")
    print(f"Aspect ratio       : {aspect_ratio:.4f}")
    print(f"Circularidade      : {circularity:.4f}")

    print()
    print("--- Perfil radial ---")
    print(f"Raio médio         : {profile_mean:.2f} px")
    print(f"Desvio padrão      : {profile_std:.2f} px")
    print(f"Raio mínimo        : {profile_min:.2f} px")
    print(f"Raio máximo        : {profile_max:.2f} px")
    print(f"Pico a pico        : {profile_max - profile_min:.2f} px")

    print()
    print("Arquivos gerados:")
    print(f"  {mask_path}")
    print(f"  {annotated_path}")
    print(f"  {json_path}")
    print(f"  {csv_path}")

    print()
    print(
        "Observação: v0.3 ainda é diagnóstica. "
        "A classificação OK/NOK será calibrada "
        "com a estação óptica definitiva."
    )


def main():
    parser = argparse.ArgumentParser(
        description="Análise geométrica inicial de engrenagens — PNAAT."
    )

    parser.add_argument(
        "imagem",
        nargs="?",
        default="teste_opencv.jpg",
        help="Imagem a analisar (padrão: teste_opencv.jpg)"
    )

    args = parser.parse_args()
    analyse_gear(Path(args.imagem))


if __name__ == "__main__":
    main()
