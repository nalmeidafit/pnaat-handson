# PNAAT Hands-on — Fábrica do Futuro

Projeto de apoio ao workshop de habilitação de Lideranças Técnicas Regionais e monitores do Laboratório PNAAT.

## Objetivo do protótipo

Construir uma célula didática de manufatura inteligente para inspeção de engrenagens produzidas por impressão 3D antes de uma etapa de sinterização.

A solução combina:

- esteira e sensor físico final;
- ESP32-S3;
- Raspberry Pi 5 + câmera OV5647;
- classificação por visão de máquina/YOLO;
- MQTT;
- Node-RED;
- InfluxDB;
- Grafana.

## Arquitetura atual

```text
                 RASPBERRY PI 5
        ┌────────────────────────────┐
        │ OV5647 → Picamera2 → YOLO │
        │             │              │
        │             └─ MQTT ───────┼─────────────┐
        │                            │             │
        │ Mosquitto ←───────────────┘             │
        │    │                                     │
        │    ├────────→ Node-RED → InfluxDB → Grafana
        │    │
        └────┼─────────────────────────────────────┘
             │ MQTT
             │
             ▼
          ESP32-S3
        ┌───────────────┐
        │ sensor final  │
        │ associação    │
        │ câmera/sensor │
        │ LED APROVADA  │
        │ LED REJEITADA │
        └───────────────┘
```

Tópicos principais:

```text
pnaat/contagem/camera
pnaat/visao/resultado
pnaat/contagem/sensor
pnaat/sensor/presenca
pnaat/peca/resultado_final
pnaat/esteira/comando
```

## Estrutura do repositório

```text
pnaat-handson/
├── README.md
├── docker/
│   ├── compose.yaml
│   ├── .env.example
│   └── mosquitto/
├── docs/
│   ├── instalacao-ming.md
│   ├── node-red-influxdb.md
│   ├── grafana.md
│   └── visao-computacional.md
├── esp32/
│   ├── README.md
│   ├── main/
│   └── tools/
├── vision/
│   ├── README.md
│   ├── aquisicao_imagens/
│   ├── classificador_engrenagens/
│   └── scripts de diagnóstico anteriores
└── montagens-mecanicas/
    └── modelos-3d/
```

## Ordem recomendada de preparação

### 1. Raspberry Pi / MING

Comece em:

[docs/instalacao-ming.md](docs/instalacao-ming.md)

Esse guia parte de uma Raspberry Pi 5 nova e inclui:

- atualização do Raspberry Pi OS;
- Git e ferramentas básicas;
- instalação do Docker Engine;
- Docker Compose;
- clone do repositório;
- criação do `.env`;
- criação do `mosquitto.conf`;
- geração do `password.txt`;
- correção de permissões;
- inicialização dos quatro serviços;
- testes de Mosquitto, InfluxDB, Node-RED e Grafana.

### 2. ESP32-S3

Consulte:

[esp32/README.md](esp32/README.md)

O firmware usa ESP-IDF v5.5.2 e possui:

- Wi-Fi;
- MQTT;
- sensor final de contagem;
- associação FIFO câmera ↔ sensor;
- LEDs APROVADA/REJEITADA;
- controle opcional de esteira;
- OTA de desenvolvimento.

### 3. Visão de máquina

Consulte:

[vision/README.md](vision/README.md)

O classificador operacional está em:

```text
vision/classificador_engrenagens/
```

Ele publica:

```text
pnaat/contagem/camera
pnaat/visao/resultado
```

Exemplo de resultado:

```json
{
  "id_camera": 17,
  "resultado": "APROVADA",
  "confianca": 0.9342,
  "classe_modelo": "OK",
  "source": "camera_yolo"
}
```

O classificador usa o modelo oficial `best.pt`, validado para Ultralytics 8.4.168 e classes `NOK`/ `OK`. Em cada clone da bancada, confirme que o arquivo está em:

```text
vision/classificador_engrenagens/best.pt
```

SHA-256 esperado:

```text
fd7c021806c16d44fb26c76eaeb2a4ba469627a9bce12da198bc98acf7ea2ed2
```

### 4. Dados e dashboard

- [Node-RED → InfluxDB](docs/node-red-influxdb.md)
- [Grafana](docs/grafana.md)

### 5. Montagens mecânicas

Os arquivos STL/STEP consolidados devem ficar em:

```text
montagens-mecanicas/modelos-3d/
```

## Estado atual

Já disponível:

- stack MING em Docker Compose;
- Mosquitto 2.1.2;
- InfluxDB 2.9.1;
- Node-RED 5.0.7;
- Grafana 12.4.11;
- firmware ESP32-S3 compilável em ESP-IDF v5.5.2;
- recepção de `pnaat/visao/resultado` pelo ESP32;
- associação câmera ↔ sensor físico;
- módulo de LEDs;
- aquisição supervisionada de imagens;
- classificador YOLO adaptado para MQTT;
- documentação de instalação completa do MING.

Pendências principais:

- garantir que o modelo `best.pt` validado esteja presente no clone usado em cada bancada;
- confirmar os GPIOs físicos definitivos dos LEDs;
- testar visão → MQTT → ESP32 → LEDs;
- validar o fluxo final no Node-RED/InfluxDB/Grafana;
- adicionar os arquivos STL/STEP definitivos;
- atualizar a apostila com os procedimentos finais.

O acompanhamento da entrega está na issue **#1**.

## Segurança e credenciais

- arquivos reais de credenciais não devem ser versionados;
- `.env`, `password.txt` e senhas MQTT devem permanecer locais;
- o ESP32 deve continuar responsável pelas ações físicas imediatas da bancada;
- MQTT e supervisão não devem ser usados como única camada de segurança de máquina.

## Persistência

Na operação normal do MING, não use:

```bash
docker compose down -v
```

O parâmetro `-v` remove volumes persistentes.
