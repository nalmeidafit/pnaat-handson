from picamera2 import Picamera2
import cv2
import time

picam2 = Picamera2()

config = picam2.create_still_configuration(
    main={
        "size": (1296, 972),
        "format": "RGB888"
    }
)

picam2.configure(config)

print("Iniciando camera...")
picam2.start()

# Tempo para autoexposição e balanço de branco estabilizarem.
time.sleep(2)

frame = picam2.capture_array()
picam2.stop()

arquivo = "teste_opencv.jpg"
cv2.imwrite(arquivo, frame)

print("Imagem capturada com sucesso.")
print(f"Resolucao: {frame.shape[1]} x {frame.shape[0]}")
print(f"Arquivo: {arquivo}")
