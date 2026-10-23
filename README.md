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

## Ordem recomendada de instalação

A instalação deve ser feita na ordem abaixo. A sequência foi definida para que cada módulo encontre suas dependências já instaladas e validadas antes de ser colocado em funcionamento.

```text
1. Raspberry Pi + MING
   └─ Mosquitto, InfluxDB, Node-RED e Grafana em execução
              ↓
2. ESP32-S3
   └─ Wi-Fi + MQTT + sensor final
              ↓
3. Visão de máquina
   └─ câmera + YOLO + publicação MQTT
              ↓
4. Node-RED → InfluxDB
   └─ recebe os dados reais e grava no banco
              ↓
5. Grafana
   └─ consulta o InfluxDB e apresenta o dashboard
```

O **Mosquitto precisa existir antes do ESP32 e da visão**, pois ambos usam o broker MQTT. O **Node-RED/InfluxDB é configurado depois do ESP32 e da visão**, quando os produtores de dados já podem ser testados. O **Grafana fica por último**, pois depende dos dados já gravados no InfluxDB.

### 1. Raspberry Pi + infraestrutura MING

Comece em:

[**Instalação completa do MING na Raspberry Pi 5**](docs/instalacao-ming.md)

Esse guia parte de uma Raspberry Pi 5 nova e conduz até:

- atualização do Raspberry Pi OS;
- instalação das ferramentas básicas;
- instalação do Docker Engine e Docker Compose;
- clone deste repositório;
- criação do `.env` da stack;
- configuração do Mosquitto;
- criação do usuário e senha MQTT;
- inicialização de Mosquitto, InfluxDB, Node-RED e Grafana;
- teste individual dos quatro serviços;
- identificação do IP da Raspberry Pi.

**Critério para avançar:** os quatro containers devem estar em execução e o teste de publicação/assinatura no Mosquitto deve funcionar.

Ao final desse guia, siga o link indicado para a instalação do ESP32-S3.

### 2. ESP32-S3

Depois que o Mosquitto estiver operacional, siga:

[**ESP32-S3 — instalação do ambiente, configuração, compilação e gravação**](esp32/README.md)

Essa etapa inclui:

- instalação do Visual Studio Code;
- instalação da extensão oficial Espressif IDF;
- instalação do ESP-IDF v5.5.2;
- abertura do projeto;
- configuração de Wi-Fi e MQTT;
- configuração do sensor físico;
- configuração dos LEDs;
- compilação e gravação do firmware;
- validação da conexão do ESP32 ao Mosquitto;
- teste da contagem física.

Neste momento a visão ainda não precisa estar instalada. O objetivo é deixar o ESP32 operacional e comunicando-se com o broker que já está rodando na Raspberry Pi.

**Critério para avançar:** ESP32 conectado ao Wi-Fi e MQTT, com o sensor físico publicando sua contagem no broker.

Ao final desse guia, siga o link indicado para a visão de máquina.

### 3. Visão de máquina

Com o MING e o ESP32 já funcionando, siga:

[**Instalação e execução do classificador de engrenagens**](vision/classificador_engrenagens/README.md)

O repositório já foi clonado na etapa 1. Não é necessário cloná-lo novamente.

O guia conduz por:

- instalação das dependências da OV5647 e Picamera2;
- teste da câmera;
- criação do ambiente virtual Python;
- instalação das dependências do classificador;
- validação do modelo `best.pt`;
- teste do classificador sem MQTT;
- teste da versão com visualização;
- criação da configuração MQTT local da visão;
- execução integrada com o Mosquitto;
- confirmação de que o ESP32 recebe o resultado da classificação;
- validação da associação câmera ↔ sensor e dos LEDs.

O arquivo [vision/README.md](vision/README.md) permanece como referência da arquitetura e dos demais scripts. O roteiro operacional de instalação é o `README.md` dentro de `vision/classificador_engrenagens/`.

**Critério para avançar:** a visão deve publicar `pnaat/visao/resultado` e o ESP32 deve associar a classificação à passagem física da peça, publicando `pnaat/peca/resultado_final`.

Ao final desse guia, siga o link indicado para Node-RED e InfluxDB.

### 4. Node-RED → InfluxDB

Com ESP32 e visão já produzindo dados MQTT reais, siga:

[**Configuração Node-RED → MQTT → InfluxDB**](docs/node-red-influxdb.md)

Nessa etapa são configurados os fluxos que:

- recebem `pnaat/peca/resultado_final`;
- recebem `pnaat/contagem/sensor`;
- transformam os payloads em Line Protocol;
- gravam `pecas_producao` e `contagem_esteira` no InfluxDB;
- validam a gravação tanto com mensagens manuais quanto com o sistema real.

**Critério para avançar:** as duas measurements devem existir no InfluxDB e receber dados reais do protótipo.

Ao final desse guia, siga o link indicado para o dashboard Grafana.

### 5. Grafana

Somente depois que o InfluxDB estiver recebendo dados, siga:

[**Criação do dashboard Grafana**](docs/grafana.md)

Essa etapa cria e valida os painéis da Fábrica do Futuro usando os dados produzidos nas etapas anteriores.

**Critério de conclusão:** todos os painéis devem apresentar corretamente dados de peças aprovadas, rejeitadas, confiança e contagem física.

### 6. Montagens mecânicas

Os arquivos STL/STEP consolidados ficam em:

```text
montagens-mecanicas/modelos-3d/
```

As montagens mecânicas devem ser realizadas de acordo com a configuração física da bancada e com os modelos disponibilizados nessa pasta.

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

- confirmar os GPIOs físicos definitivos dos LEDs em cada montagem;
- adicionar os arquivos STL/STEP definitivos;
- manter a apostila sincronizada com os procedimentos finais validados em bancada.

O acompanhamento da entrega está na issue **#1**.

## Segurança e credenciais

- arquivos reais de credenciais não devem ser versionados;
- `.env`, `password.txt` e senhas MQTT devem permanecer locais;
- o ESP32 deve continuar responsável pelas ações físicas imediatas da bancada;
- MQTT e supervisão não devem ser usados como única camada de segurança de máquina.

## Persistência

Na operação normal do MING, utilize somente os comandos documentados em [docs/instalacao-ming.md](docs/instalacao-ming.md) para iniciar, parar ou reiniciar a stack.

Os volumes persistentes armazenam configurações e dados do InfluxDB, Node-RED, Grafana e Mosquitto e não devem ser removidos durante o workshop.
