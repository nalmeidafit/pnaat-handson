# Node-RED → MQTT → InfluxDB

Este guia parte de uma instalação MING já concluída conforme [instalacao-ming.md](instalacao-ming.md). O objetivo é configurar o Node-RED desde uma instalação vazia até receber mensagens MQTT do projeto e gravá-las no InfluxDB.

Ao final deste procedimento, o fluxo validado será:

```text
ESP32 / Visão
     ↓ MQTT
Mosquitto
     ↓
Node-RED
     ↓ HTTP API v2
InfluxDB
```

A integração com o InfluxDB usa a **API HTTP v2**, sem instalar nodes adicionais.

## 1. Pré-requisitos

Na Raspberry Pi:

```bash
cd ~/pnaat-handson/docker
docker compose ps
```

Os serviços abaixo devem estar em execução:

```text
pnaat-mosquitto
pnaat-influxdb
pnaat-nodered
pnaat-grafana
```

Confirme o InfluxDB:

```bash
curl -fsS http://localhost:8086/health
```

A resposta deve indicar:

```text
"status":"pass"
```

Confirme o Node-RED:

```bash
curl -I http://localhost:1880
```

Descubra o IP da Raspberry Pi:

```bash
hostname -I
```

Em um navegador de outro computador da mesma rede, abra:

```text
http://IP_DA_RASPBERRY:1880
```

## 2. Entender as variáveis já fornecidas ao Node-RED

O `compose.yaml` envia estas variáveis para o container Node-RED:

```text
INFLUXDB_URL=http://influxdb:8086
INFLUXDB_ORG=PNAAT
INFLUXDB_BUCKET=fabrica_futuro
INFLUXDB_TOKEN=<token definido no .env>
```

Dentro da rede Docker, use:

```text
http://influxdb:8086
```

e **não**:

```text
http://localhost:8086
```

Dentro do container Node-RED, `localhost` significa o próprio Node-RED, não o container InfluxDB.

Para conferir que as variáveis chegaram ao container sem mostrar o token:

```bash
docker exec pnaat-nodered sh -c \
  'printf "URL=%s\nORG=%s\nBUCKET=%s\nTOKEN_CONFIGURADO=" "$INFLUXDB_URL" "$INFLUXDB_ORG" "$INFLUXDB_BUCKET"; [ -n "$INFLUXDB_TOKEN" ] && echo SIM || echo NAO'
```

Resultado esperado:

```text
URL=http://influxdb:8086
ORG=PNAAT
BUCKET=fabrica_futuro
TOKEN_CONFIGURADO=SIM
```

## 3. Primeiro teste: Node-RED → InfluxDB sem MQTT

Antes de integrar o broker, valide somente a gravação no banco.

No editor do Node-RED, crie este fluxo:

```text
Inject → Function → HTTP Request → Debug
```

### 3.1 Adicionar o node Inject

Na coluna da esquerda:

1. localize **Inject**;
2. arraste-o para a área de trabalho;
3. dê duplo clique;
4. em **Name**, use `Teste InfluxDB`;
5. não é necessário alterar o payload;
6. clique em **Done**.

### 3.2 Adicionar o node Function

1. arraste um node **Function**;
2. dê duplo clique;
3. em **Name**, use `Gerar dado de teste`;
4. substitua o conteúdo pelo código abaixo:

```javascript
let id = context.get("id") || 0;
id++;
context.set("id", id);

const influxUrl = env.get("INFLUXDB_URL");
const org = env.get("INFLUXDB_ORG");
const bucket = env.get("INFLUXDB_BUCKET");
const token = env.get("INFLUXDB_TOKEN");

const aprovada = Math.random() > 0.20;
const confianca = 0.85 + Math.random() * 0.14;

msg.method = "POST";

msg.url =
    `${influxUrl}/api/v2/write` +
    `?org=${encodeURIComponent(org)}` +
    `&bucket=${encodeURIComponent(bucket)}` +
    `&precision=ms`;

msg.headers = {
    "Authorization": `Token ${token}`,
    "Content-Type": "text/plain; charset=utf-8"
};

msg.payload =
    `pecas_producao,linha=esteira1 ` +
    `id=${id}i,` +
    `aprovada=${aprovada},` +
    `confianca=${confianca}`;

return msg;
```

5. clique em **Done**.

### 3.3 Adicionar o node HTTP Request

1. arraste um node **HTTP Request**;
2. dê duplo clique;
3. em **Method**, selecione a opção equivalente a **use msg.method / set by msg.method**;
4. deixe o campo **URL completamente vazio**;
5. em **Return**, selecione **a UTF-8 string**;
6. em **Name**, use `Gravar no InfluxDB`;
7. clique em **Done**.

> Não escreva `msg.url` no campo URL. O endereço já é criado pela Function e colocado na propriedade `msg.url`.

Se for escrito literalmente `msg.url`, pode ocorrer:

```text
getaddrinfo ENOTFOUND msg.url
```

### 3.4 Adicionar o node Debug

1. arraste um node **Debug**;
2. configure a saída para **complete msg object**;
3. dê o nome `Resposta InfluxDB`;
4. clique em **Done**.

### 3.5 Ligar os nodes

Conecte:

```text
Inject
   ↓
Function
   ↓
HTTP Request
   ↓
Debug
```

Clique em **Deploy** no canto superior direito.

## 4. Executar o primeiro teste

Abra a aba **Debug** no painel direito.

Clique no botão do node Inject.

O HTTP Request deve retornar:

```text
statusCode: 204
payload: ""
```

`204 No Content` significa que a gravação foi aceita.

Se aparecer:

```text
401 Unauthorized
```

verifique `INFLUXDB_TOKEN`.

Se aparecer erro de conexão para `localhost:8086`, confira se `INFLUXDB_URL` é:

```text
http://influxdb:8086
```

## 5. Confirmar que o dado realmente entrou no InfluxDB

Abra:

```text
http://IP_DA_RASPBERRY:8086
```

Entre com o usuário e senha configurados no `.env`.

Na interface do InfluxDB:

1. abra **Data Explorer**;
2. selecione o bucket `fabrica_futuro`;
3. selecione a measurement `pecas_producao`;
4. escolha um intervalo que inclua os últimos minutos;
5. execute a consulta.

Devem aparecer os campos:

```text
id
aprovada
confianca
```

e a tag:

```text
linha=esteira1
```

Só prossiga para MQTT depois que este teste funcionar.

## 6. Criar a conexão MQTT no Node-RED

O resultado final da peça é publicado pelo ESP32 em:

```text
pnaat/peca/resultado_final
```

Exemplo:

```json
{
  "sensor_count": 17,
  "id_camera": 17,
  "resultado": "APROVADA",
  "confianca": 0.93,
  "source": "esp32_matcher"
}
```

No Node-RED, adicione um node **mqtt in**.

Dê duplo clique e configure:

```text
Topic: pnaat/peca/resultado_final
QoS:   1
```

No campo **Server**, clique no botão de edição/criação do broker.

Configure o broker:

```text
Name:   Mosquitto PNAAT
Server: mosquitto
Port:   1883
TLS:    desabilitado
```

Na aba de segurança do broker:

```text
Username: pnaat
Password: a mesma senha usada para gerar docker/mosquitto/config/password.txt
```

Não use o IP da Raspberry Pi dentro do Node-RED. Como Node-RED e Mosquitto estão no mesmo Docker Compose, o hostname correto é:

```text
mosquitto
```

Clique em **Add/Update** e depois em **Done**.

Depois de **Deploy**, o node MQTT deve mostrar estado semelhante a:

```text
connected
```

Se mostrar `disconnected`, verifique:

```bash
cd ~/pnaat-handson/docker
docker compose logs --tail=100 mosquitto
docker compose logs --tail=100 nodered
```

## 7. Montar o fluxo MQTT → InfluxDB

Crie:

```text
MQTT IN
   ↓
Function
   ↓
HTTP Request
   ↓
Debug
```

### 7.1 Function de conversão

Crie uma Function chamada:

```text
Resultado final → Line Protocol
```

Use:

```javascript
const influxUrl = env.get("INFLUXDB_URL");
const org = env.get("INFLUXDB_ORG");
const bucket = env.get("INFLUXDB_BUCKET");
const token = env.get("INFLUXDB_TOKEN");

let data = msg.payload;

if (Buffer.isBuffer(data)) {
    data = data.toString("utf8");
}

if (typeof data === "string") {
    try {
        data = JSON.parse(data);
    } catch (err) {
        node.error("Payload MQTT não é JSON válido", msg);
        return null;
    }
}

if (
    data == null ||
    typeof data.sensor_count !== "number" ||
    typeof data.resultado !== "string"
) {
    node.error(
        "Payload sem sensor_count ou resultado válido",
        msg
    );
    return null;
}

const resultado = data.resultado.toUpperCase();

let aprovada;

if (
    resultado === "APROVADA" ||
    resultado === "APROVADO" ||
    resultado === "OK"
) {
    aprovada = true;
} else if (
    resultado === "REJEITADA" ||
    resultado === "REJEITADO" ||
    resultado === "NOK"
) {
    aprovada = false;
} else {
    node.warn(
        `Resultado ${data.resultado} não será gravado como aprovado/rejeitado`
    );
    return null;
}

const id = Math.trunc(data.sensor_count);
const idCamera =
    typeof data.id_camera === "number"
        ? Math.trunc(data.id_camera)
        : 0;

const confianca =
    typeof data.confianca === "number"
        ? data.confianca
        : 0.0;

msg.method = "POST";

msg.url =
    `${influxUrl}/api/v2/write` +
    `?org=${encodeURIComponent(org)}` +
    `&bucket=${encodeURIComponent(bucket)}` +
    `&precision=ms`;

msg.headers = {
    "Authorization": `Token ${token}`,
    "Content-Type": "text/plain; charset=utf-8"
};

msg.payload =
    `pecas_producao,linha=esteira1 ` +
    `id=${id}i,` +
    `id_camera=${idCamera}i,` +
    `aprovada=${aprovada},` +
    `confianca=${confianca}`;

return msg;
```

Use novamente um node **HTTP Request** com:

```text
Method: use msg.method / set by msg.method
URL:    vazio
Return: a UTF-8 string
```

Ligue um **Debug** na saída e faça **Deploy**.

## 8. Testar MQTT → Node-RED → InfluxDB sem depender do ESP32

Antes de usar a esteira, valide o caminho completo com uma mensagem MQTT manual.

Na Raspberry Pi:

```bash
cd ~/pnaat-handson/docker
```

Publique uma peça aprovada:

```bash
docker exec -it pnaat-mosquitto \
  mosquitto_pub \
  -h localhost \
  -p 1883 \
  -u pnaat \
  -P SUA_SENHA_MQTT \
  -t 'pnaat/peca/resultado_final' \
  -m '{"sensor_count":1001,"id_camera":1001,"resultado":"APROVADA","confianca":0.95,"source":"teste_manual"}'
```

No Debug do Node-RED deve aparecer:

```text
statusCode: 204
```

Publique uma rejeitada:

```bash
docker exec -it pnaat-mosquitto \
  mosquitto_pub \
  -h localhost \
  -p 1883 \
  -u pnaat \
  -P SUA_SENHA_MQTT \
  -t 'pnaat/peca/resultado_final' \
  -m '{"sensor_count":1002,"id_camera":1002,"resultado":"REJEITADA","confianca":0.88,"source":"teste_manual"}'
```

Novamente, espere:

```text
statusCode: 204
```

Depois confirme as duas amostras no Data Explorer do InfluxDB.

## 9. Testar com o ESP32 real

Com o ESP32 conectado ao MQTT, mantenha um terminal observando:

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

Quando câmera e sensor forem associados, deve aparecer:

```text
pnaat/peca/resultado_final {...}
```

No Node-RED:

1. o node MQTT deve continuar `connected`;
2. a Function deve receber a mensagem;
3. o HTTP Request deve retornar `204`;
4. o dado deve aparecer no bucket `fabrica_futuro`.

## 10. Gravar também a contagem física do sensor

O dashboard atual possui um painel chamado:

```text
Contagem — Sensor físico
```

Ele consulta a measurement:

```text
contagem_esteira
```

com:

```text
_field = count
origem = sensor_final
```

Portanto, além do fluxo de `pnaat/peca/resultado_final`, o Node-RED precisa gravar a contagem publicada pelo ESP32 em:

```text
pnaat/contagem/sensor
```

Exemplo de mensagem:

```json
{"count":82,"source":"sensor_final"}
```

### 10.1 Adicionar outro MQTT IN

Crie um segundo node **mqtt in** usando o mesmo broker `Mosquitto PNAAT`.

Configure:

```text
Topic: pnaat/contagem/sensor
QoS:   1
```

### 10.2 Criar a Function da contagem física

Crie uma Function chamada:

```text
Contagem física → Line Protocol
```

Use:

```javascript
const influxUrl = env.get("INFLUXDB_URL");
const org = env.get("INFLUXDB_ORG");
const bucket = env.get("INFLUXDB_BUCKET");
const token = env.get("INFLUXDB_TOKEN");

let data = msg.payload;

if (Buffer.isBuffer(data)) {
    data = data.toString("utf8");
}

if (typeof data === "string") {
    try {
        data = JSON.parse(data);
    } catch (err) {
        node.error(
            "Payload da contagem não é JSON válido",
            msg
        );
        return null;
    }
}

if (
    data == null ||
    typeof data.count !== "number"
) {
    node.error(
        "Payload sem campo count numérico",
        msg
    );
    return null;
}

const count = Math.trunc(data.count);

msg.method = "POST";

msg.url =
    `${influxUrl}/api/v2/write` +
    `?org=${encodeURIComponent(org)}` +
    `&bucket=${encodeURIComponent(bucket)}` +
    `&precision=ms`;

msg.headers = {
    "Authorization": `Token ${token}`,
    "Content-Type": "text/plain; charset=utf-8"
};

msg.payload =
    `contagem_esteira,origem=sensor_final ` +
    `count=${count}i`;

return msg;
```

### 10.3 Ligar o fluxo

Use outro node **HTTP Request** com:

```text
Method: use msg.method / set by msg.method
URL:    vazio
Return: a UTF-8 string
```

Conecte:

```text
MQTT IN pnaat/contagem/sensor
        ↓
Function Contagem física → Line Protocol
        ↓
HTTP Request
        ↓
Debug
```

Clique em **Deploy**.

### 10.4 Testar sem depender do ESP32

Na Raspberry Pi:

```bash
docker exec -it pnaat-mosquitto \
  mosquitto_pub \
  -h localhost \
  -p 1883 \
  -u pnaat \
  -P SUA_SENHA_MQTT \
  -t 'pnaat/contagem/sensor' \
  -m '{"count":123,"source":"sensor_final"}'
```

O Debug deve mostrar:

```text
statusCode: 204
```

No InfluxDB Data Explorer, confirme:

```text
Measurement: contagem_esteira
Field:       count
Tag:         origem=sensor_final
Value:       123
```

Essa gravação é necessária para o painel de contagem física do Grafana funcionar.

## 11. Salvar o fluxo


Depois que tudo funcionar:

1. clique em **Deploy**;
2. no menu do Node-RED, use **Export**;
3. exporte o fluxo atual em JSON;
4. salve uma cópia de backup fora do container.

O volume `nodered_data` mantém o fluxo entre reinicializações, mas um JSON exportado facilita restaurar a bancada ou replicá-la em outra Raspberry Pi.

## 12. Diagnóstico rápido

### MQTT não conecta

Confira:

```text
Server = mosquitto
Port = 1883
Username = pnaat
Password = mesma usada no password.txt
```

E:

```bash
docker compose logs --tail=100 mosquitto
docker compose logs --tail=100 nodered
```

### HTTP Request retorna 401

Token incorreto ou diferente do token usado na inicialização do InfluxDB.

### HTTP Request retorna ENOTFOUND msg.url

O campo URL do node HTTP Request foi preenchido com o texto `msg.url`. Deixe-o vazio.

### HTTP Request não alcança InfluxDB

Dentro do Node-RED use:

```text
http://influxdb:8086
```

### O fluxo recebe MQTT mas nada aparece no banco

Verifique o Debug imediatamente antes do HTTP Request e confirme que `msg.payload` virou Line Protocol.

## 13. Checklist de validação

- [ ] Node-RED abre na porta 1880;
- [ ] variáveis do InfluxDB estão disponíveis no container;
- [ ] teste Inject → Function → HTTP Request retorna 204;
- [ ] dado de teste aparece no Data Explorer;
- [ ] broker MQTT configurado como `mosquitto:1883`;
- [ ] autenticação MQTT configurada;
- [ ] node MQTT mostra `connected`;
- [ ] publicação manual em `pnaat/peca/resultado_final` chega ao Node-RED;
- [ ] publicação manual é gravada no InfluxDB;
- [ ] resultado real do ESP32 chega ao Node-RED;
- [ ] resultado real é gravado no bucket `fabrica_futuro`;
- [ ] `pnaat/contagem/sensor` é recebido pelo segundo fluxo;
- [ ] measurement `contagem_esteira` aparece no InfluxDB;
- [ ] field `count` com tag `origem=sensor_final` foi validado;
- [ ] fluxo foi salvo com Deploy;
- [ ] cópia JSON do fluxo foi exportada.

Com todos os itens marcados, a integração Node-RED → InfluxDB está pronta para o dashboard Grafana.
