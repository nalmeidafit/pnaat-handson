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

// Buffer -> String
if (Buffer.isBuffer(data)) {
    data = data.toString("utf8");
}

// String JSON -> Objeto
if (typeof data === "string") {
    try {
        data = JSON.parse(data);
    } catch (err) {
        node.error("Payload não é JSON válido", msg);
        return null;
    }
}

let lines = [];

//
// CONTAGEM DA ESTEIRA
//
if (typeof data.count === "number") {
    const count = Math.trunc(data.count);

    lines.push(
        `contagem_esteira,origem=sensor_final count=${count}i`
    );
}

//
// CLASSIFICAÇÃO DAS PEÇAS
//
if (
    typeof data.id_camera === "number" &&
    typeof data.resultado === "string"
) {
    const resultado = data.resultado.toUpperCase();

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

    const idCamera = Math.trunc(data.id_camera);

    const confianca =
        typeof data.confianca === "number"
            ? data.confianca
            : 0.0;

    const classeModelo =
        typeof data.classe_modelo === "string"
            ? data.classe_modelo.replace(/ /g, "_")
            : "desconhecida";

    lines.push(
        `pecas_producao,linha=esteira1,id_camera=${idCamera},classe=${classeModelo} ` +
        `aprovada_num=${aprovada}i,confianca=${confianca}`
    );

    node.status({
        fill: "green",
        shape: "dot",
        text: `${resultado} (${confianca})`
    });
}

// Nada válido encontrado
if (lines.length === 0) {
    node.error(
        "Payload não contém dados válidos para gravação",
        msg
    );
    return null;
}

// Configuração HTTP para InfluxDB
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

// Múltiplas medições na mesma escrita
msg.payload = lines.join("\n");

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
