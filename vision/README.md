# Módulo de visão

Diretório destinado à inspeção de engrenagens pela Raspberry Pi 5.

## Arquivos

- `camera_test.py`: captura uma imagem 1296 × 972 com Picamera2.
- `gear_inspection_v0_3.py`: análise diagnóstica de imagem parada.
- `moving_gear_inspection.py`: rastreamento de uma engrenagem em movimento.

## Ambiente

```bash
cd ~/pnaat-handson/vision

python3 -m venv --system-site-packages .venv
source .venv/bin/activate
```

Pacotes usados:

```bash
sudo apt install -y \
  python3-opencv \
  python3-picamera2 \
  python3-numpy \
  python3-paho-mqtt
```

## Teste 1 — movimento sem MQTT

Primeiro valide somente câmera, ROI e rastreamento:

```bash
python moving_gear_inspection.py --no-mqtt
```

Se houver interface gráfica disponível:

```bash
python moving_gear_inspection.py --no-mqtt --show
```

A janela mostra a ROI e o contorno detectado. Pressione `Q` para sair.

## Exposição

O valor inicial é:

```text
5000 us
```

Se a engrenagem estiver borrada em movimento, experimente:

```bash
python moving_gear_inspection.py \
  --no-mqtt \
  --exposure-us 2500
```

Se a imagem ficar escura, aumente a iluminação antes de aumentar muito a exposição.

Para voltar à autoexposição:

```bash
python moving_gear_inspection.py \
  --no-mqtt \
  --exposure-us 0
```

## Como uma peça é contada

O script não conta frames. Ele mantém uma pequena máquina de estados:

```text
WAITING
   |
   | 3 frames consecutivos detectando a peça
   v
TRACKING
   |
   | coleta vários frames da mesma engrenagem
   |
   | 6 frames consecutivos sem a peça
   v
FINALIZA UMA PEÇA
```

A passagem só é aceita quando houver pelo menos 5 amostras válidas.

Ao final, o terminal mostra as medianas de:

- área externa;
- raio externo;
- circularidade;
- aspect ratio;
- posição central.

Nesta fase o resultado permanece:

```text
PENDENTE_CALIBRACAO
```

Nenhuma peça é classificada como aprovada ou rejeitada ainda.

## MQTT

O script lê:

```text
PNAAT_MQTT_HOST
PNAAT_MQTT_PORT
PNAAT_MQTT_USERNAME
PNAAT_MQTT_PASSWORD
PNAAT_TOPIC_PREFIX
```

Exemplo, sem colocar senha no repositório:

```bash
export PNAAT_MQTT_HOST=127.0.0.1
export PNAAT_MQTT_PORT=1883
export PNAAT_MQTT_USERNAME=pnaat
export PNAAT_MQTT_PASSWORD='SUA_SENHA'
export PNAAT_TOPIC_PREFIX=pnaat

python moving_gear_inspection.py
```

A contagem da câmera é publicada com retain em:

```text
pnaat/contagem/camera
```

Exemplo:

```json
{"count":18,"source":"camera"}
```

As métricas de cada passagem são publicadas em:

```text
pnaat/visao/medicao
```

Exemplo:

```json
{
  "id_camera":18,
  "source":"camera",
  "samples":12,
  "metrics":{
    "area_px2":112000.0,
    "radius_px":208.0,
    "circularity":0.72,
    "aspect_ratio":0.98,
    "center_x_px":669,
    "center_y_px":607
  },
  "classificacao":"PENDENTE_CALIBRACAO"
}
```

O contador é persistido localmente em `vision_state.json`, que está ignorado pelo Git.

## Próxima etapa

Depois que o rastreamento estiver estável:

1. passar a mesma engrenagem boa pelo menos 10 vezes;
2. registrar a dispersão das métricas;
3. definir a referência da peça boa;
4. adicionar análise do perfil radial/dentes;
5. validar peças defeituosas;
6. somente então publicar `APROVADA` ou `REJEITADA` em `pnaat/visao/resultado`.
