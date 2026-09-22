# PNAAT Hands-on — Fábrica do Futuro

Projeto de apoio ao workshop de habilitação de Lideranças Técnicas Regionais e monitores do Laboratório PNAAT.

## Desafio

Construir uma célula didática de manufatura inteligente para inspeção de engrenagens produzidas por impressão 3D antes de uma etapa de sinterização.

Fluxo previsto:

```text
Esteira + sensores S1/S2/S3
        ↓
      ESP32
        ↓ MQTT
    Mosquitto
        ↓
     Node-RED
        ↓
 ┌───────────────┐
 │ Raspberry Pi 5│
 │ câmera + OpenCV
 └───────────────┘
        ↓
     Node-RED
        ↓
    InfluxDB
        ↓
      Grafana
```

## Estado atual

- Raspberry Pi 5 configurada e acessada remotamente pelo VS Code.
- Stack MING em Docker Compose funcionando:
  - Mosquitto 2.1.2
  - InfluxDB 2.9.1
  - Node-RED 5.0.7
  - Grafana 12.4.11
- MQTT validado.
- Node-RED gravando no InfluxDB pela API HTTP v2.
- Grafana exibindo:
  - total de peças;
  - aprovadas;
  - rejeitadas;
  - confiança média;
  - peças inspecionadas ao longo do tempo;
  - última peça inspecionada.
- Câmera OV5647 reconhecida pela Raspberry Pi 5.
- Captura com Picamera2/OpenCV validada em 1296 × 972.
- Inspeção geométrica em desenvolvimento:
  - v0.1: primeiro contorno funcional;
  - v0.2: regressão de segmentação identificada;
  - v0.3: correção de segmentação, ROI e validações básicas.

## Estrutura

```text
pnaat-handson/
├── README.md
├── .gitignore
├── docker/
├── node-red/
├── grafana/
├── vision/
├── docs/
└── assets/
```

## Documentação

- [Instalação do MING](docs/instalacao-ming.md)
- [Node-RED → InfluxDB](docs/node-red-influxdb.md)
- [Dashboard Grafana](docs/grafana.md)
- [Visão computacional](docs/visao-computacional.md)
- [Módulo de visão](vision/README.md)

## Segurança e arquitetura

O ESP32 deve executar as ações físicas imediatas da esteira, incluindo a parada associada aos sensores. MQTT, Node-RED e visão computacional realizam integração, supervisão e decisão de processo, mas não devem ser a única camada de segurança da máquina.

## Credenciais

Arquivos reais de credenciais não devem ser versionados. Use `.env.example` como referência e mantenha `.env` apenas no host.

## Status

Projeto em desenvolvimento para preparação e validação do hands-on PNAAT.
