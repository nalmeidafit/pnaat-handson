# Instalação do MING na Raspberry Pi 5

Este documento registra o procedimento utilizado e validado durante a preparação do hands-on.

## Stack

- Mosquitto 2.1.2
- InfluxDB 2.9.1
- Node-RED 5.0.7
- Grafana 12.4.11

Arquitetura:

```text
MQTT → Mosquitto → Node-RED → InfluxDB → Grafana
```

## Pré-requisitos

Raspberry Pi OS 64 bits e Docker Engine + Docker Compose Plugin.

Validar:

```bash
uname -m
docker --version
docker compose version
```

A arquitetura esperada é `aarch64`.

## Preparação

```bash
git clone https://github.com/nalmeidafit/pnaat-handson.git
cd pnaat-handson/docker
cp .env.example .env
```

Edite o `.env` e defina senhas reais.

## Mosquitto

Crie o arquivo de senhas:

```bash
docker run --rm \
  -v "$PWD/mosquitto/config:/mosquitto/config" \
  eclipse-mosquitto:2.1.2-alpine \
  mosquitto_passwd -b -c \
  /mosquitto/config/password.txt \
  pnaat SUA_SENHA_MQTT
```

Durante a preparação foi encontrado um erro de reinicialização do container causado pela permissão do `password.txt`.

A correção validada foi:

```bash
chmod 644 mosquitto/config/password.txt
chmod 644 mosquitto/config/mosquitto.conf
```

## Subir os serviços

```bash
docker compose pull
docker compose up -d
docker compose ps
```

Os quatro serviços devem ficar `Up`.

## Portas

| Serviço | Porta |
|---|---:|
| Mosquitto | 1883 |
| Node-RED | 1880 |
| InfluxDB | 8086 |
| Grafana | 3000 |

## InfluxDB

A configuração inicial usa:

- Organization: `PNAAT`
- Bucket: `fabrica_futuro`

Após o primeiro boot, crie um token real pela interface do InfluxDB e atualize `INFLUXDB_TOKEN` no `.env`.

> Alterar apenas `DOCKER_INFLUXDB_INIT_ADMIN_TOKEN` depois da inicialização não substitui automaticamente um token já criado no volume persistente.

## Persistência

A stack usa volumes Docker nomeados. Não use:

```bash
docker compose down -v
```

durante a operação normal, pois `-v` remove os volumes persistentes.
