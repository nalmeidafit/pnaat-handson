from picamera2 import Picamera2
from pathlib import Path
from ultralytics import YOLO


import cv2
import time

# ==================================================
# PASTAS
# ==================================================

modelo = YOLO("best.pt")
classes = modelo.names 

ok_dir = Path("OK")
nok_dir = Path("NOK")

ok_dir.mkdir(exist_ok=True)
nok_dir.mkdir(exist_ok=True)

# ==================================================
# CAMERA
# ==================================================

picam2 = Picamera2()
config = picam2.create_preview_configuration(
    main={"size": (640, 480)}
)

picam2.configure(config)

picam2.set_controls({
    "ExposureTime": 7000,
    "AnalogueGain": 4.0,
    "AeEnable": False,
    "AwbEnable": False
})


print("Iniciando camera...")

picam2.start()

time.sleep(2)

# ==================================================
# ROI PRINCIPAL
# ==================================================

ROI_X = 250
ROI_Y = 135
ROI_W = 150
ROI_H = 255


# ==================================================
# ROI TRIGGER
# ==================================================

ROI_CONT_X = 130
ROI_CONT_Y = 65
ROI_CONT_W = 1
ROI_CONT_H = 140

# ==================================================
# PARAMETROS
# ==================================================

THRESHOLD = 80
TRIGGER_PIXELS = 10

# ==================================================
# FPS
# ==================================================

tempo_anterior = time.time()

# ==================================================
# ESTADOS
# ==================================================

ESTADO_ARMADO = 0
ESTADO_AGUARDANDO_CLASSIFICACAO = 1

estado = ESTADO_ARMADO

imagem_capturada = None

# ==================================================
# LOOP
# ==================================================

while True:

    frame = picam2.capture_array()

    # ==============================================
    # FPS
    # ==============================================

    tempo_atual = time.time()

    fps = 1.0 / (tempo_atual - tempo_anterior)

    tempo_anterior = tempo_atual

    # ==============================================
    # ROI
    # ==============================================

    roi = frame[
        ROI_Y:ROI_Y + ROI_H,
        ROI_X:ROI_X + ROI_W
    ]

    gray = cv2.cvtColor(
        roi,
        cv2.COLOR_RGB2GRAY
    )

    _, mascara = cv2.threshold(
        gray,
        THRESHOLD,
        255,
        cv2.THRESH_BINARY
    )

    # ==============================================
    # ROI DE TRIGGER
    # ==============================================

    roi_contagem = mascara[
        ROI_CONT_Y:ROI_CONT_Y + ROI_CONT_H,
        ROI_CONT_X:ROI_CONT_X + ROI_CONT_W
    ]

    pixels_brancos = cv2.countNonZero(
        roi_contagem
    )

    pixels_totais = roi_contagem.size

    pixels_pretos = (
        pixels_totais -
        pixels_brancos
    )

    # ==============================================
    # CAPTURA AUTOMATICA
    # ==============================================

    if (
        estado == ESTADO_ARMADO and
        pixels_pretos > TRIGGER_PIXELS
    ):

        imagem_capturada = gray.copy()

        estado = ESTADO_AGUARDANDO_CLASSIFICACAO

        #print()
        #print("================================")
        #print("PECA DETECTADA")
        #print("Pressione O = OK")
        #print("Pressione N = NOK")
        #rint("================================")


        # 1. Passar a imagem direto para a IA (O YOLO redimensiona e trata tudo sozinho!)
        resultados = modelo(gray, verbose=False)
        
        # 2. Extrair os resultados
        probas = resultados[0].probs
        id_classe = probas.top1              # ID da classe com maior certeza
        confianca = probas.top1conf.item()   # Porcentagem de certeza (0.0 a 1.0)

        # 3. Exibir o resultado direto no texto do terminal
        print(f"Predição: {classes[id_classe]} | Confiança: {confianca*100:.1f}%", end="\r")


        cv2.putText(
        gray,
        f"{classes[id_classe]}",
        (10, 70),
        cv2.FONT_HERSHEY_SIMPLEX,
        0.8,
        (255, 255, 0),
        2
        )


        cv2.imshow(
        "Resultado",
        gray
        )


        imagem_capturada = None

    # ==============================================
    # REARME
    # ==============================================

    if (
        estado == ESTADO_AGUARDANDO_CLASSIFICACAO
        and
        imagem_capturada is None
        and
        pixels_pretos == 0
    ):

        estado = ESTADO_ARMADO
        #print("Sistema armado novamente")

    # ==============================================
    # DISPLAY
    # ==============================================

    frame_display = frame.copy()

    cv2.rectangle(
        frame_display,
        (ROI_X, ROI_Y),
        (ROI_X + ROI_W, ROI_Y + ROI_H),
        (0, 255, 0),
        2
    )

    cv2.rectangle(
        frame_display,
        (
            ROI_X + ROI_CONT_X,
            ROI_Y + ROI_CONT_Y
        ),
        (
            ROI_X + ROI_CONT_X + ROI_CONT_W,
            ROI_Y + ROI_CONT_Y + ROI_CONT_H
        ),
       (0, 0, 255),
        2
    )

    cv2.putText(
        frame_display,
        f"FPS: {fps:.1f}",
        (10, 30),
        cv2.FONT_HERSHEY_SIMPLEX,
       0.8,
        (0, 255, 0),
        2
    )

    cv2.putText(
        frame_display,
        f"Pixels Pretos: {pixels_pretos}",
        (10, 70),
        cv2.FONT_HERSHEY_SIMPLEX,
        0.8,
        (0, 255, 0),
        2
    )

    if estado == ESTADO_ARMADO:

        texto = "ARMADO"
        cor = (0, 255, 0)

    else:

        texto = "AGUARDANDO O/N"
        cor = (0, 0, 255)

    cv2.putText(
        frame_display,
        texto,
        (10, 110),
        cv2.FONT_HERSHEY_SIMPLEX,
        0.8,
        cor,
        2
    )

    cv2.imshow(
        "Camera",
        frame_display
    )


    # ==============================================
    # TECLADO
    # ==============================================

    tecla = cv2.waitKey(1) & 0xFF

    # ----------------------------------------------
    # SALVA OK
    # ----------------------------------------------

    #if (
    #    tecla == ord('o')
    #    and
    #    imagem_capturada is not None
    #):

    #    nome = ok_dir / (
    #        f"OK_{time.strftime('%Y%m%d_%H%M%S')}.jpg"
    #    )

    #    cv2.imwrite(
    #        str(nome),
    #        imagem_capturada
    #    )

    #   print(f"Salvo OK: {nome}")

    #   imagem_capturada = None

    # ----------------------------------------------
    # SALVA NOK
    # ----------------------------------------------

    #if (
    #    tecla == ord('n')
    #    and
    #    imagem_capturada is not None
    #):

    #    nome = nok_dir / (
    #        f"NOK_{time.strftime('%Y%m%d_%H%M%S')}.jpg"
    #    )

    #    cv2.imwrite(
    #        str(nome),
    #        imagem_capturada
    #    )

    #    print(f"Salvo NOK: {nome}")

    #   imagem_capturada = None

    # ----------------------------------------------
    # ESC
    # ----------------------------------------------

    if tecla == 27:
        break

# ==================================================
# FINALIZACAO
# ==================================================

picam2.stop()

cv2.destroyAllWindows()