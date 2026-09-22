# Node-RED → InfluxDB

A integração validada usa a API HTTP v2 do InfluxDB, sem instalar nodes adicionais no Node-RED.

## Variáveis de ambiente

O container Node-RED recebe:

```text
INFLUXDB_URL=http://influxdb:8086
INFLUXDB_ORG=PNAAT
INFLUXDB_BUCKET=fabrica_futuro
INFLUXDB_TOKEN=<token>
```

Dentro da rede Docker deve-se usar `influxdb` como hostname, e não `localhost`.

## Fluxo de teste

```text
Inject → Function → HTTP Request → Debug
```

Exemplo da Function:

```javascript
let id = context.get("id") || 0;
id++;
context.set("id", id);

const influxUrl = env.get("INFLUXDB_URL");
const org = env.get("INFLUXDB_ORG");
const bucket = env.get("INFLUXDB_BUCKET");
const token = env.get("INFLUXDB_TOKEN");

const aprovada = Math.random() > 0.20;
const confianca = (0.85 + Math.random() * 0.14).toFixed(2);

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
    `id=${id}i,aprovada=${aprovada},confianca=${confianca}`;

return msg;
```

## Configuração do HTTP Request

- Method: `set by msg.method`
- URL: **deixar o campo vazio**
- Return: `a UTF-8 string`

Um erro observado durante a preparação foi colocar literalmente `msg.url` no campo URL. Isso gerou:

```text
getaddrinfo ENOTFOUND msg.url
```

Com o campo vazio, o node usa corretamente a propriedade `msg.url`.

## Resultado esperado

O InfluxDB responde com:

```text
statusCode: 204
payload: ""
```

O payload vazio é normal; `204 No Content` confirma a gravação.
