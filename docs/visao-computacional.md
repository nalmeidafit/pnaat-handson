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

Para a inspeção atual foi escolhido `1296 × 972`.

## Segmentação atual

A engrenagem azul usa como ponto de partida:

```text
HSV lower = [70, 45, 20]
HSV upper = [115, 255, 255]
```

A ROI reduz falsos contornos e limita a região útil da esteira.

## Inspeção em movimento

O arquivo atual é:

```text
vision/moving_gear_inspection.py
```

Nesta fase ele é propositalmente diagnóstico:

- identifica a engrenagem enquanto ela cruza a ROI;
- agrupa vários frames como uma única peça;
- conta uma vez por passagem;
- calcula medianas das métricas;
- publica `pnaat/contagem/camera`;
- publica `pnaat/visao/medicao`;
- não classifica APROVADA/REJEITADA ainda.

O ESP32 e o sensor E18-D80NK continuam independentes da câmera.

## Estado da máquina de rastreamento

```text
WAITING
  -> 3 detecções consecutivas
TRACKING
  -> mede vários frames
  -> 6 ausências consecutivas
FINALIZA UMA PEÇA
  -> incrementa contagem da câmera
  -> publica métricas
  -> volta a WAITING
```

O mínimo inicial é de 5 frames válidos por peça.

## Exposição para movimento

Valor inicial:

```text
ExposureTime = 5000 us
FPS = 20
```

Se houver motion blur, testar `2500 us` e compensar com mais iluminação.

## Próxima calibração

Quando câmera, suporte, iluminação e velocidade da esteira estiverem fixos:

1. executar pelo menos 10 passagens da mesma peça boa;
2. medir variabilidade de área, raio, circularidade e aspect ratio;
3. adicionar perfil radial para avaliar dentes;
4. criar/usar peças defeituosas de referência;
5. definir tolerâncias;
6. validar a taxa de acerto;
7. só então habilitar o tópico `pnaat/visao/resultado` com APROVADA/REJEITADA.
