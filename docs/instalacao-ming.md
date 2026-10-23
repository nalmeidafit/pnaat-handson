# Instalação completa do MING na Raspberry Pi 5

Este procedimento parte de uma **Raspberry Pi 5 nova**, com Raspberry Pi OS 64 bits instalado, rede configurada e acesso ao terminal. O objetivo é chegar ao final com **Mosquitto, InfluxDB, Node-RED e Grafana funcionando em Docker Compose**, sem depender de outro tutorial.

> Os comandos abaixo foram preparados para Raspberry Pi OS/Debian 64 bits. Execute-os na ordem apresentada.

## 1. Confirmar o sistema

Atualize a lista de pacotes e o sistema:

```bash
sudo apt update
sudo apt full-upgrade -y
sudo reboot
```

Após reconectar à Raspberry Pi, confirme a arquitetura:

```bash
uname -m
```

Resultado esperado na Raspberry Pi 5 64 bits:

```text
aarch64
```

Confira também a versão do sistema:

```bash
cat /etc/os-release
```

## 2. Instalar ferramentas básicas

Instale Git, curl, certificados, editor, OpenSSL e utilitários básicos:

```bash
sudo apt update
sudo apt install -y \
  git \
  curl \
  ca-certificates \
  nano \
  openssl
```

Valide:

```bash
git --version
curl --version
```

## 3. Instalar o Docker Engine

Baixe o instalador oficial de conveniência do Docker:

```bash
cd ~
curl -fsSL https://get.docker.com -o get-docker.sh
```

Execute:

```bash
sudo sh get-docker.sh
```

Confirme que o serviço está ativo:

```bash
sudo systemctl enable --now docker
sudo systemctl status docker --no-pager
```

Adicione o usuário atual ao grupo `docker`:

```bash
sudo usermod -aG docker "$USER"
```

**Encerre a sessão SSH/terminal e entre novamente** para que o novo grupo seja aplicado.

Depois da nova sessão, verifique:

```bash
groups
docker --version
docker compose version
```

O grupo `docker` deve aparecer na saída de `groups`.

Teste o Docker sem `sudo`:

```bash
docker run --rm hello-world
```

Se esse teste funcionar, o Docker está pronto.

## 4. Obter o repositório do workshop

Escolha uma pasta de trabalho:

```bash
cd ~
```

Clone:

```bash
git clone https://github.com/nalmeidafit/pnaat-handson.git
```

Entre no projeto:

```bash
cd ~/pnaat-handson
```

Verifique:

```bash
git status
git log -1 --oneline
```

## 5. Entrar na stack MING

Acesse:

```bash
cd ~/pnaat-handson/docker
```

Confira os arquivos:

```bash
ls -la
ls -la mosquitto/config
```

A estrutura utilizada é:

```text
docker/
├── .env.example
├── compose.yaml
└── mosquitto/
    └── config/
        └── mosquitto.conf
```

O arquivo `password.txt` **não é versionado** e será criado nesta instalação.

## 6. Criar o arquivo .env

Crie uma cópia local:

```bash
cp .env.example .env
```

Gere um token aleatório para o InfluxDB:

```bash
openssl rand -hex 32
```

Copie o valor gerado e edite:

```bash
nano .env
```

Preencha todos os campos:

```text
INFLUXDB_USERNAME=pnaat
INFLUXDB_PASSWORD=TROQUE_ESTA_SENHA
INFLUXDB_ORG=PNAAT
INFLUXDB_BUCKET=fabrica_futuro
INFLUXDB_TOKEN=COLE_AQUI_O_TOKEN_GERADO

GRAFANA_USER=admin
GRAFANA_PASSWORD=TROQUE_ESTA_SENHA
```

Salve com `Ctrl+O`, confirme com `Enter` e saia com `Ctrl+X`.

Proteja o arquivo:

```bash
chmod 600 .env
```

Valide apenas os nomes das variáveis, sem exibir seus valores:

```bash
cut -d= -f1 .env
```

> O `.env` real contém credenciais e não deve ser enviado ao Git.

## 7. Criar e validar mosquitto.conf

Mesmo que o clone já forneça o arquivo, o conteúdo esperado é reproduzido abaixo para que a instalação possa ser refeita sem procurar outra referência.

Garanta a pasta:

```bash
mkdir -p mosquitto/config
```

Crie ou substitua o arquivo:

```bash
cat > mosquitto/config/mosquitto.conf <<'EOF'
listener 1883 0.0.0.0

allow_anonymous false
password_file /mosquitto/config/password.txt

persistence true
persistence_location /mosquitto/data/

log_dest stdout
EOF
```

Confira:

```bash
cat mosquitto/config/mosquitto.conf
```

## 8. Gerar password.txt do Mosquitto

O usuário MQTT utilizado no projeto é:

```text
pnaat
```

Crie o arquivo de senha de forma interativa:

```bash
docker run --rm -it \
  -v "$PWD/mosquitto/config:/mosquitto/config" \
  eclipse-mosquitto:2.1.2-alpine \
  mosquitto_passwd -c \
  /mosquitto/config/password.txt \
  pnaat
```

Digite a senha MQTT quando solicitado e confirme-a.

Verifique que os dois arquivos existem:

```bash
ls -l mosquitto/config
```

Devem existir:

```text
mosquitto.conf
password.txt
```

## 9. Corrigir permissões do Mosquitto

Garanta permissão de travessia das pastas e leitura dos arquivos:

```bash
chmod 755 mosquitto
chmod 755 mosquitto/config
chmod 644 mosquitto/config/mosquitto.conf
sudo chmod 644 mosquitto/config/password.txt
```

Confira:

```bash
ls -ld mosquitto mosquitto/config
ls -l mosquitto/config
```

Resultado esperado para os arquivos:

```text
-rw-r--r-- ... mosquitto.conf
-rw-r--r-- ... password.txt
```

Essa etapa é importante porque uma permissão incorreta no `password.txt` pode fazer o contêiner Mosquitto reiniciar continuamente.

## 10. Conferir o Docker Compose

Valide a sintaxe e a expansão das variáveis:

```bash
docker compose config
```

Esse comando deve terminar sem erro.

A stack utiliza:

| Serviço | Imagem | Porta |
|---|---|---:|
| Mosquitto | eclipse-mosquitto:2.1.2-alpine | 1883 |
| InfluxDB | influxdb:2.9.1 | 8086 |
| Node-RED | nodered/node-red:5.0.7 | 1880 |
| Grafana | grafana/grafana:12.4.11 | 3000 |

## 11. Baixar as imagens

```bash
docker compose pull
```

A primeira execução pode demorar alguns minutos.

## 12. Iniciar o MING

```bash
docker compose up -d
```

Confira:

```bash
docker compose ps
```

Os quatro contêineres devem permanecer em execução:

```text
pnaat-mosquitto
pnaat-influxdb
pnaat-nodered
pnaat-grafana
```

Se algum serviço estiver reiniciando ou parado:

```bash
docker compose logs --tail=100 NOME_DO_SERVICO
```

Exemplos:

```bash
docker compose logs --tail=100 mosquitto
docker compose logs --tail=100 influxdb
docker compose logs --tail=100 nodered
docker compose logs --tail=100 grafana
```

## 13. Testar o Mosquitto

Nos **dois comandos abaixo**, substitua `SUA_SENHA_MQTT` pela mesma senha definida para o usuário MQTT `pnaat` no item 8.

Abra um terminal e execute:

```bash
docker exec -it pnaat-mosquitto \
  mosquitto_sub \
  -h localhost \
  -p 1883 \
  -u pnaat \
  -P SUA_SENHA_MQTT \
  -t 'pnaat/teste' \
  -v
```

Deixe esse terminal aguardando.

Em outro terminal publique:

```bash
docker exec -it pnaat-mosquitto \
  mosquitto_pub \
  -h localhost \
  -p 1883 \
  -u pnaat \
  -P SUA_SENHA_MQTT \
  -t 'pnaat/teste' \
  -m 'mqtt_ok'
```

Resultado esperado no assinante:

```text
pnaat/teste mqtt_ok
```

> Em bancada compartilhada, prefira digitar a senha apenas em um terminal controlado e limpe o histórico se necessário. Nunca registre a senha real na apostila ou no Git.

## 14. Testar o InfluxDB

No próprio Raspberry Pi:

```bash
curl -fsS http://localhost:8086/health
```

A resposta deve indicar estado `pass`.

A configuração inicial usada no projeto é:

```text
Organization: PNAAT
Bucket: fabrica_futuro
```

O token definido em `.env` no primeiro boot é utilizado também pelo Node-RED.

## 15. Testar o Node-RED

```bash
curl -I http://localhost:1880
```

Também pode ser aberto em um navegador de outro computador da rede:

```text
http://IP_DA_RASPBERRY:1880
```

Dentro da rede Docker, o InfluxDB deve ser acessado por:

```text
http://influxdb:8086
```

Não use `localhost:8086` dentro do contêiner Node-RED, pois `localhost` apontaria para o próprio contêiner do Node-RED.

## 16. Testar o Grafana

```bash
curl -fsS http://localhost:3000/api/health
```

A interface pode ser acessada em:

```text
http://IP_DA_RASPBERRY:3000
```

As credenciais iniciais são as definidas em `.env`.

## 17. Descobrir o IP da Raspberry Pi

```bash
hostname -I
```

Anote o endereço IPv4 da rede local. Ele será utilizado pelo ESP32 como endereço do broker MQTT:

```text
mqtt://IP_DA_RASPBERRY:1883
```

## 18. Operação normal

Ver estado:

```bash
cd ~/pnaat-handson/docker
docker compose ps
```

Reiniciar:

```bash
docker compose restart
```

Parar sem apagar os dados:

```bash
docker compose down
```

Subir novamente:

```bash
docker compose up -d
```

Acompanhar logs:

```bash
docker compose logs -f
```

## 19. Preservar os dados da stack

Durante o workshop, para parar, reiniciar ou subir novamente a stack, utilize somente os comandos apresentados no item 18.

Evite opções de remoção de volumes persistentes. Esses volumes armazenam configurações e dados do InfluxDB, Node-RED, Grafana e Mosquitto e não devem ser apagados durante a operação normal.

## 20. Checklist de validação

Antes de seguir para ESP32 e visão computacional, confirme:

- [ ] `uname -m` retorna `aarch64`;
- [ ] `docker run --rm hello-world` funciona sem `sudo`;
- [ ] `docker compose version` funciona;
- [ ] repositório clonado;
- [ ] `.env` criado e protegido com `chmod 600`;
- [ ] `mosquitto.conf` criado;
- [ ] `password.txt` criado;
- [ ] permissões do Mosquitto corrigidas;
- [ ] `docker compose config` sem erros;
- [ ] quatro contêineres em execução;
- [ ] publicação/assinatura MQTT validada;
- [ ] InfluxDB responde em `/health`;
- [ ] Node-RED abre na porta 1880;
- [ ] Grafana responde na porta 3000;
- [ ] IP da Raspberry Pi anotado.

Com estes itens concluídos, a infraestrutura MING está pronta.

## Próxima etapa — ESP32-S3

Não avance para a visão ainda. O próximo módulo a ser instalado é o ESP32-S3, que depende do Mosquitto configurado nesta etapa.

Antes de sair deste guia, tenha em mãos:

- o endereço IPv4 da Raspberry Pi;
- o usuário MQTT `pnaat`;
- a senha MQTT criada no item 8;
- o Mosquitto respondendo ao teste de publicação/assinatura.

Continue em:

[**ESP32-S3 — instalação, configuração, compilação e gravação**](../esp32/README.md)

Ao final do guia do ESP32 haverá um link para a instalação da visão de máquina.
