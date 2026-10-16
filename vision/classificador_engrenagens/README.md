# Classificador de engrenagens — YOLO + MQTT

Este diretório contém a versão operacional do classificador recebido para o workshop, adaptada para execução **sem interface gráfica** e integração com o broker MQTT do MING.

## Arquivos

```text
classificador_engrenagens/
├── gear_image_class_noScreen.py
├── requirements.txt
├── .env.example
└── best.pt                    # modelo treinado
```

O arquivo `best.pt` deve conter o modelo treinado com as classes esperadas `OK` e `NOK`.

## Preparar a Raspberry Pi

Instale Picamera2 e suporte a ambientes virtuais:

```bash
sudo apt update
sudo apt install -y \
  python3-picamera2 \
  python3-venv \
  python3-opencv
```

Entre na pasta:

```bash
cd ~/pnaat-handson/vision/classificador_engrenagens
```

Crie o ambiente:

```bash
python3 -m venv --system-site-packages .venv
source .venv/bin/activate
```

Instale as dependências:

```bash
python -m pip install --upgrade pip
pip install -r requirements.txt
```

## Configurar MQTT sem gravar senha no Git

O script lê:

```text
PNAAT_MQTT_HOST
PNAAT_MQTT_PORT
PNAAT_MQTT_USERNAME
PNAAT_MQTT_PASSWORD
PNAAT_TOPIC_PREFIX
PNAAT_MODEL_PATH
```

Exemplo:

```bash
export PNAAT_MQTT_HOST=127.0.0.1
export PNAAT_MQTT_PORT=1883
export PNAAT_MQTT_USERNAME=pnaat
export PNAAT_TOPIC_PREFIX=pnaat
export PNAAT_MODEL_PATH="$PWD/best.pt"

read -s -p "Senha MQTT: " PNAAT_MQTT_PASSWORD
echo
export PNAAT_MQTT_PASSWORD
```

## Testar sem MQTT

```bash
python gear_image_class_noScreen.py --no-mqtt
```

## Executar integrado ao MING

```bash
python gear_image_class_noScreen.py
```

Ao detectar uma peça, o programa:

1. captura a ROI;
2. classifica a imagem com o modelo YOLO;
3. incrementa o contador da câmera;
4. persiste o contador em `vision_state.json`;
5. publica a contagem;
6. publica o resultado da classificação.

## Tópicos publicados

Contagem:

```text
pnaat/contagem/camera
```

Exemplo:

```json
{"count":17,"source":"camera_yolo"}
```

Resultado:

```text
pnaat/visao/resultado
```

Exemplo:

```json
{
  "id_camera":17,
  "resultado":"APROVADA",
  "confianca":0.9342,
  "classe_modelo":"OK",
  "source":"camera_yolo",
  "timestamp_utc":"2026-10-16T16:00:00.000+00:00"
}
```

Mapeamento:

```text
OK  -> APROVADA
NOK -> REJEITADA
outra classe -> INCONCLUSIVA
```

O ESP32 já utiliza os campos `id_camera`, `resultado` e `confianca` para associar a classificação à passagem da peça pelo sensor final.

## Testar o MQTT

Em outro terminal:

```bash
docker exec -it pnaat-mosquitto \
  mosquitto_sub \
  -h localhost \
  -p 1883 \
  -u pnaat \
  -P SUA_SENHA_MQTT \
  -t 'pnaat/#' \
  -v
```

Passe uma engrenagem pela região de detecção e confirme a chegada de `pnaat/contagem/camera` e `pnaat/visao/resultado`.

## Encerramento

Use `Ctrl+C`. O script encerra a câmera e a conexão MQTT de forma controlada.
