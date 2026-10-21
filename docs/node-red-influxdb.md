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





## Fluxo final

```text
MQTT in → Function → HTTP Request → Debug

Para facilitar, renomeie: 
MQTT in (pnaat/visao/resultado) → Function → HTTP Request (Gravar no Influx) → Debug(Resposta Influx)

```
'''
no MQTT in:

Servidor: Mosquitto PNAAT
Ação: Assinar um tópico unico
Tópico: pnaat/visao/resultado
QoS: 1
Saída: auto-detecção
Nome: nome

''''
Exemplo da Function:

```javascript
const influxUrl = env.get("INFLUXDB_URL");
const org = env.get("INFLUXDB_ORG");
const bucket = env.get("INFLUXDB_BUCKET");
const token = env.get("INFLUXDB_TOKEN");

let data = msg.payload;

// Se vier como Buffer
if (Buffer.isBuffer(data)) {
    data = data.toString("utf8");
}

// Se vier como string JSON
if (typeof data === "string") {
    try {
        data = JSON.parse(data);
    } catch (err) {
        node.error("Payload MQTT não é JSON válido", msg);
        return null;
    }
}

// Validação dos campos obrigatórios
if (
    data == null ||
    typeof data.id_camera !== "number" ||
    typeof data.resultado !== "string"
) {
    node.warn("Conteúdo recebido:");
    node.warn(data);

    node.error(
        "Payload sem id_camera ou resultado válido",
        msg
    );
    return null;
}

// Normaliza o resultado
const resultado = data.resultado.toUpperCase();

// Converte para inteiro
let aprovada;

if (
    resultado === "APROVADA" ||
    resultado === "APROVADO" ||
    resultado === "OK"
) {
    aprovada = 1;

} else if (
    resultado === "REJEITADA" ||
    resultado === "REJEITADO" ||
    resultado === "NOK"
) {
    aprovada = 0;

} else {
    node.warn(`Resultado inválido recebido: ${resultado}`);
    return null;
}

// Campos opcionais
const idCamera = Math.trunc(data.id_camera);

const confianca =
    typeof data.confianca === "number"
        ? data.confianca
        : 0.0;

const classeModelo =
    typeof data.classe_modelo === "string"
        ? data.classe_modelo.replace(/ /g, "_")
        : "desconhecida";

// Configuração HTTP
msg.method = "POST";

msg.url =
    `${influxUrl}/api/v2/write` +
    `?org=${encodeURIComponent(org)}` +
    `&bucket=${encodeURIComponent(bucket)}` +
    `&precision=ms`;

msg.headers = {
    Authorization: `Token ${token}`,
    "Content-Type": "text/plain; charset=utf-8"
};

// Line Protocol InfluxDB
msg.payload =
    `pecas_producao,linha=esteira1,id_camera=${idCamera},classe=${classeModelo} ` +
    `aprovada_num=${aprovada}i,` +
    `confianca=${confianca}`;

// Debug opcional
node.status({
    fill: "green",
    shape: "dot",
    text: `${resultado} (${confianca})`
});

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

10/21/2026, 2:30:33 PMnó: Resposta InfluxDB
pnaat/visao/resultado : msg : Object

O payload vazio é normal; `204 No Content` confirma a gravação.
