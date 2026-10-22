# Visão de máquina

A visão de máquina do workshop possui dois fluxos distintos:

1. **aquisição supervisionada de imagens**, usada para criar conjuntos `OK` e `NOK`;
2. **classificação operacional com YOLO**, usada durante a passagem das engrenagens na esteira e integrada ao MQTT.

Também permanecem no repositório os scripts anteriores de inspeção geométrica, úteis como diagnóstico e histórico de desenvolvimento.

## Estrutura

```text
vision/
├── README.md
├── aquisicao_imagens/
│   ├── README.md
│   ├── camera_test.py
│   ├── gear_image_aq.py
│   └── requirements.txt
├── classificador_engrenagens/
│   ├── README.md
│   ├── .env.example
│   ├── gear_image_class.py
│   ├── gear_image_class_noScreen.py
│   ├── requirements.txt
│   ├── best.pt
│   └── best.pt.sha256
├── camera_test.py
├── gear_inspection_v0_3.py
└── moving_gear_inspection.py
```

## Fluxo operacional atual

```text
OV5647
  ↓
Picamera2
  ↓
ROI / trigger
  ↓
modelo YOLO best.pt
  ↓
OK / NOK
  ↓
APROVADA / REJEITADA
  ↓
Mosquitto
  ↓
pnaat/visao/resultado
  ├── ESP32-S3 → associação com sensor final → LEDs
  └── Node-RED → InfluxDB → Grafana
```

## Por onde começar

Se o objetivo é **instalar e executar a visão de máquina durante o workshop**, não é necessário seguir os demais arquivos desta pasta.

Use diretamente:

[**classificador_engrenagens/README.md — instalação e execução passo a passo**](classificador_engrenagens/README.md)

Esse guia parte do ponto em que o repositório `pnaat-handson` já foi clonado pela etapa de instalação do MING e conduz até:

- instalação das dependências da câmera;
- teste da OV5647;
- criação do ambiente virtual;
- instalação das bibliotecas Python;
- validação do `best.pt`;
- execução do classificador sem MQTT;
- execução do classificador visual;
- integração MQTT.

A partir da raiz do repositório, a pasta correta é:

```bash
cd ~/pnaat-handson/vision/classificador_engrenagens
```

As demais seções deste README servem como referência sobre a estrutura e o histórico da parte de visão.

## Classificador usado na bancada

A versão principal para execução via SSH é:

```text
vision/classificador_engrenagens/gear_image_class_noScreen.py
```

Ela foi baseada no classificador fornecido para o workshop e atualizada para:

- não depender de interface gráfica;
- não armazenar credenciais MQTT no código;
- aguardar confirmação real da conexão MQTT;
- utilizar QoS 1;
- detectar falha de publicação;
- persistir a contagem da câmera;
- publicar contagem em `pnaat/contagem/camera`;
- publicar classificação em `pnaat/visao/resultado`;
- enviar `id_camera`, `resultado`, `confianca`, classe do modelo e origem.

Consulte:

```text
vision/classificador_engrenagens/README.md
```

## Payload de resultado

Exemplo:

```json
{
  "id_camera": 17,
  "resultado": "APROVADA",
  "confianca": 0.9342,
  "classe_modelo": "OK",
  "source": "camera_yolo",
  "timestamp_utc": "2026-10-16T16:00:00.000+00:00"
}
```

O firmware ESP32-S3 utiliza diretamente:

```text
id_camera
resultado
confianca
```

Mapeamento atual:

```text
OK  → APROVADA
NOK → REJEITADA
```

Qualquer classe não reconhecida é enviada como `INCONCLUSIVA`.

## Configuração MQTT

A visão lê estas variáveis de ambiente:

```text
PNAAT_MQTT_HOST
PNAAT_MQTT_PORT
PNAAT_MQTT_USERNAME
PNAAT_MQTT_PASSWORD
PNAAT_TOPIC_PREFIX
PNAAT_MODEL_PATH
```

A senha real não deve ser gravada no Git.

## Modelo YOLO

O classificador depende do arquivo:

```text
vision/classificador_engrenagens/best.pt
```

O código e a documentação já estão preparados para esse caminho. O modelo oficial da bancada foi validado como um `Ultralytics ClassificationModel` baseado em `yolov8n-cls`, treinado com Ultralytics 8.4.168 e classes `NOK`/ `OK`. O arquivo deve estar em `vision/classificador_engrenagens/best.pt`. O SHA-256 esperado é `fd7c021806c16d44fb26c76eaeb2a4ba469627a9bce12da198bc98acf7ea2ed2`.

## Aquisição de imagens

Para criar novas imagens supervisionadas:

```text
vision/aquisicao_imagens/gear_image_aq.py
```

Consulte:

```text
vision/aquisicao_imagens/README.md
```

## Scripts geométricos anteriores

Os arquivos:

```text
gear_inspection_v0_3.py
moving_gear_inspection.py
```

permanecem como ferramentas de diagnóstico e registro das etapas anteriores de desenvolvimento. Eles não são o classificador YOLO principal da bancada.
