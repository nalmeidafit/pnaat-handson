# Visão computacional — Raspberry Pi 5 + OV5647

## Câmera validada

A Raspberry Pi 5 reconheceu:

```text
ov5647 [2592x1944 10-bit GBRG]
```

Modos observados:

- 640 × 480
- 1296 × 972
- 1920 × 1080
- 2592 × 1944

Para os primeiros testes foi escolhido `1296 × 972`, preservando o enquadramento 4:3.

## Instalação

```bash
sudo apt update
sudo apt install -y \
  python3-opencv \
  python3-picamera2 \
  python3-numpy \
  python3-paho-mqtt \
  v4l-utils
```

Ambiente Python:

```bash
python3 -m venv --system-site-packages .venv
source .venv/bin/activate
```

## Teste da câmera

```bash
rpicam-hello --list-cameras
```

Captura:

```bash
python camera_test.py
```

## Evolução da inspeção

### v0.1

Primeiro contorno externo funcional sobre a engrenagem azul.

### v0.2

Foi tentada uma segmentação mais permissiva:

```text
HSV lower = [70, 30, 20]
HSV upper = [115, 255, 255]
```

O fundo branco/ciano passou a entrar na máscara. O maior contorno passou a ser o fundo, e não a engrenagem.

### v0.3

Correções:

- saturação mínima voltou para 45;
- ROI central configurável;
- somente contornos externos para localizar a peça;
- rejeição de contornos muito grandes;
- rejeição de contornos que encostam na borda da ROI;
- perfil radial mantido para futura detecção de dentes.

A v0.3 ainda é **diagnóstica** e não deve classificar OK/NOK até a estação óptica definitiva estar montada.

## Próxima calibração

Após fixar suporte, altura, foco, iluminação e posição S2:

1. capturar pelo menos 10 imagens da mesma peça boa;
2. calcular variabilidade das métricas;
3. definir tolerâncias;
4. adicionar furo central;
5. adicionar análise de dentes;
6. criar peças defeituosas de referência;
7. validar taxa de acerto antes da integração final MQTT.
