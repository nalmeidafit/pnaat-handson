# Classificador de engrenagens — referência dos arquivos

Este diretório contém os arquivos usados pelo classificador YOLO do PNAAT Hands-on.

> **Para instalar e executar a visão de máquina durante o workshop, não comece por este arquivo.**  
> O roteiro oficial e completo está em:
>
> [**../README.md — Visão de máquina: instalação e operação**](../README.md)

## Arquivos principais

```text
classificador_engrenagens/
├── gear_image_class.py
├── gear_image_class_noScreen.py
├── requirements.txt
├── .env.example
├── best.pt
└── best.pt.sha256
```

### gear_image_class_noScreen.py

Versão recomendada para operação normal, SSH e execução sem ambiente gráfico.

Ela executa:

- captura pela OV5647 usando Picamera2;
- calibração automática do fundo;
- detecção por ROI/trigger;
- classificação YOLO;
- persistência da contagem da câmera;
- publicação MQTT;
- rearme para a peça seguinte.

### gear_image_class.py

Versão de bancada com visualização.

Ela usa a mesma lógica do programa sem tela, mas também apresenta:

- imagem ao vivo;
- ROI;
- linha/região de trigger;
- FPS;
- pixels pretos;
- baseline;
- delta;
- estado do detector;
- contagem;
- última classificação.

Use esta versão somente em sessão gráfica disponível na Raspberry Pi.

## Modelo YOLO

O modelo operacional é:

```text
best.pt
```

O arquivo deve permanecer na mesma pasta dos scripts.

SHA-256 esperado:

```text
fd7c021806c16d44fb26c76eaeb2a4ba469627a9bce12da198bc98acf7ea2ed2
```

Classes:

```text
NOK
OK
```

Mapeamento usado pelo sistema:

```text
OK  → APROVADA
NOK → REJEITADA
```

## Configuração local

O arquivo:

```text
.env.example
```

é apenas um modelo.

Durante a instalação, o guia principal orienta criar:

```text
.env
```

Esse arquivo contém a configuração MQTT local e é ignorado pelo Git.

## Tópicos publicados

```text
pnaat/contagem/camera
pnaat/visao/resultado
```

O ESP32 usa principalmente:

```text
id_camera
resultado
confianca
```

do payload publicado em `pnaat/visao/resultado`.

## Instalação e execução

Todo o procedimento, incluindo câmera, ambiente virtual, requirements, modelo, configuração MQTT, execução sem tela, execução visual e teste integrado está concentrado em:

[**Visão de máquina — instalação e operação**](../README.md)

Manter um único roteiro de instalação evita que versões diferentes do procedimento sejam seguidas durante o workshop.
