# Grafana — criação completa do dashboard da Fábrica do Futuro

Este guia parte do MING já instalado e da integração Node-RED → InfluxDB validada. Ele descreve **todos os passos necessários para criar manualmente o dashboard usado no workshop**, partindo de um Grafana sem datasource e sem dashboard.

O dashboard de referência foi criado no Grafana **12.4.11** e utiliza o datasource InfluxDB com linguagem Flux.

## 1. O que deve existir antes de começar

A cadeia abaixo precisa estar funcionando:

```text
ESP32 / Visão
     ↓ MQTT
Mosquitto
     ↓
Node-RED
     ↓
InfluxDB
     ↓
Grafana
```

Na Raspberry Pi:

```bash
cd ~/pnaat-handson/docker
docker compose ps
```

Confirme que estes containers estão em execução:

```text
pnaat-mosquitto
pnaat-influxdb
pnaat-nodered
pnaat-grafana
```

Teste o InfluxDB:

```bash
curl -fsS http://localhost:8086/health
```

A resposta deve indicar:

```text
"status":"pass"
```

Teste o Grafana:

```bash
curl -fsS http://localhost:3000/api/health
```

Descubra o IP da Raspberry Pi:

```bash
hostname -I
```

Anote o IPv4 da rede local.

## 2. Confirmar que o InfluxDB já possui os dados usados pelo dashboard

Antes de configurar o Grafana, abra:

```text
http://IP_DA_RASPBERRY:8086
```

Entre no InfluxDB com as credenciais da bancada.

No **Data Explorer**, confirme a existência destas duas measurements:

```text
pecas_producao
contagem_esteira
```

Em `pecas_producao`, devem existir:

```text
Fields:
  id
  id_camera
  aprovada
  confianca

Tag:
  linha=esteira1
```

Em `contagem_esteira`, devem existir:

```text
Field:
  count

Tag:
  origem=sensor_final
```

Se `contagem_esteira` não existir, execute primeiro a seção de contagem física do tutorial:

```text
docs/node-red-influxdb.md
```

O Grafana não cria estes dados; ele apenas consulta o que já foi gravado no InfluxDB.

## 3. Abrir o Grafana

No navegador:

```text
http://IP_DA_RASPBERRY:3000
```

Use as credenciais definidas em:

```text
~/pnaat-handson/docker/.env
```

As variáveis são:

```text
GRAFANA_USER
GRAFANA_PASSWORD
```

Para conferir somente o usuário:

```bash
cd ~/pnaat-handson/docker
grep '^GRAFANA_USER=' .env
```

Não registre a senha real no Git, na apostila ou em screenshots públicos.

## 4. Obter os parâmetros do InfluxDB

Ainda na Raspberry Pi:

```bash
cd ~/pnaat-handson/docker

grep '^INFLUXDB_ORG=' .env
grep '^INFLUXDB_BUCKET=' .env
grep '^INFLUXDB_TOKEN=' .env
```

A configuração esperada é:

```text
Organization: PNAAT
Bucket:       fabrica_futuro
Token:        valor de INFLUXDB_TOKEN
```

Copie o token apenas para a configuração do datasource.

## 5. Criar o datasource InfluxDB no Grafana

Na interface do Grafana:

1. abra **Connections**;
2. entre em **Data sources**;
3. clique em **Add new data source** ou **Add data source**;
4. escolha **InfluxDB**.

Configure:

```text
Name:            InfluxDB PNAAT
Query language:  Flux
URL:             http://influxdb:8086
```

### Por que usar influxdb e não localhost

Grafana e InfluxDB estão em containers diferentes.

Dentro do container Grafana:

```text
localhost
```

significa o próprio Grafana.

Por isso a URL correta é:

```text
http://influxdb:8086
```

Na seção de detalhes do InfluxDB configure:

```text
Organization:    PNAAT
Token:           valor de INFLUXDB_TOKEN
Default Bucket:  fabrica_futuro
```

Para esta configuração, deixe Basic Auth desabilitado.

Clique em:

```text
Save & test
```

O Grafana deve informar que o datasource está funcionando.

### Se ocorrer 401 Unauthorized

O token configurado não corresponde ao token válido do InfluxDB.

### Se ocorrer connection refused ou Bad Gateway

Confira:

```text
URL = http://influxdb:8086
```

e depois:

```bash
cd ~/pnaat-handson/docker
docker compose ps
docker compose logs --tail=100 influxdb
docker compose logs --tail=100 grafana
```

## 6. Validar o datasource usando Explore

Não comece a criar os painéis antes deste teste.

No Grafana:

1. abra **Explore**;
2. selecione o datasource `InfluxDB PNAAT`;
3. use o editor Flux;
4. execute:

```flux
from(bucket: "fabrica_futuro")
  |> range(start: -6h)
  |> filter(fn: (r) => r["_measurement"] == "pecas_producao")
```

Devem aparecer registros.

Depois teste a contagem física:

```flux
from(bucket: "fabrica_futuro")
  |> range(start: -6h)
  |> filter(fn: (r) => r["_measurement"] == "contagem_esteira")
```

Se não aparecer nada:

1. aumente temporariamente para `-24h`;
2. confirme os dados no Data Explorer do InfluxDB;
3. confira o bucket;
4. confira o datasource;
5. confira se o Node-RED está gravando os dois tipos de dados.

Só prossiga quando as consultas funcionarem no Explore.

## 7. Criar um dashboard novo

Na interface do Grafana:

1. abra **Dashboards**;
2. clique em **New**;
3. escolha **New dashboard**;
4. clique em **Add visualization**;
5. escolha o datasource `InfluxDB PNAAT`.

O dashboard de referência possui **7 painéis**.

Ao final teremos:

```text
┌───────────────────────────────────────────────────────────────┐
│ Peças       │ Peças       │ Total de    │                     │
│ Aprovadas   │ Rejeitadas  │ Peças       │                     │
├─────────────┴──────────────┴─────────────┤ Peças inspecionadas │
│ Confiança Média │ Última Peça            │ ao longo do tempo   │
│                 │ Inspecionada           │                     │
├─────────────────┴────────────────────────┴─────────────────────┤
│ Contagem — Sensor físico                                      │
└───────────────────────────────────────────────────────────────┘
```

## 8. Ajustar o intervalo de tempo do dashboard

O dashboard de referência usa:

```text
From: now-6h
To:   now
Timezone: Browser
```

No seletor de tempo do Grafana escolha:

```text
Last 6 hours
```

Isso é importante porque várias queries usam:

```flux
v.timeRangeStart
v.timeRangeStop
```

Se o intervalo selecionado não contiver dados, o painel ficará vazio mesmo estando configurado corretamente.

## 9. Painel 1 — Peças Aprovadas

Crie uma nova visualização.

### 9.1 Datasource

Selecione:

```text
InfluxDB PNAAT
```

### 9.2 Tipo de visualização

Escolha:

```text
Stat
```

### 9.3 Título

Use:

```text
Peças Aprovadas
```

### 9.4 Query Flux

No editor da query, use:

```flux
from(bucket: v.defaultBucket)
  |> range(start: v.timeRangeStart, stop: v.timeRangeStop)
  |> filter(fn: (r) => r["_measurement"] == "pecas_producao")
  |> filter(fn: (r) => r["_field"] == "aprovada")
  |> filter(fn: (r) => r["_value"] == true)
  |> count()
```

### 9.5 Value options

Configure a redução como:

```text
Calculation: Last not null
```

### 9.6 Aparência

Configure:

```text
Color mode:  Value
Graph mode:  Area
Text mode:   Auto
Orientation: Auto
```

Em **Thresholds**, mantenha apenas o valor base em verde:

```text
Base / 0 → Green
```

Clique em **Apply**.

## 10. Painel 2 — Peças Rejeitadas

Adicione nova visualização.

Tipo:

```text
Stat
```

Título:

```text
Peças Rejeitadas
```

Query:

```flux
from(bucket: v.defaultBucket)
  |> range(start: v.timeRangeStart, stop: v.timeRangeStop)
  |> filter(fn: (r) => r["_measurement"] == "pecas_producao")
  |> filter(fn: (r) => r["_field"] == "aprovada")
  |> filter(fn: (r) => r["_value"] == false)
  |> count()
```

Value options:

```text
Calculation: Last not null
```

Aparência:

```text
Color mode: Value
Graph mode: Area
Text mode:  Auto
```

Threshold base:

```text
Base / 0 → Dark red
```

Clique em **Apply**.

## 11. Painel 3 — Total de Peças

Adicione nova visualização.

Tipo:

```text
Stat
```

Título:

```text
Total de Peças
```

Query:

```flux
from(bucket: v.defaultBucket)
  |> range(start: v.timeRangeStart, stop: v.timeRangeStop)
  |> filter(fn: (r) => r["_measurement"] == "pecas_producao")
  |> filter(fn: (r) => r["_field"] == "id")
  |> count()
```

Value options:

```text
Calculation: Last not null
```

Aparência:

```text
Color mode: Value
Graph mode: Area
Text mode:  Auto
```

Threshold base:

```text
Base / 0 → Green
```

Clique em **Apply**.

## 12. Painel 4 — CONFIANÇA MÉDIA

Adicione nova visualização.

Tipo:

```text
Gauge
```

Título:

```text
CONFIANÇA MÉDIA
```

Query:

```flux
from(bucket: v.defaultBucket)
  |> range(start: v.timeRangeStart, stop: v.timeRangeStop)
  |> filter(fn: (r) => r["_measurement"] == "pecas_producao")
  |> filter(fn: (r) => r["_field"] == "confianca")
  |> mean()
```

Em **Value options**:

```text
Calculation: Last not null
```

Em **Standard options**:

```text
Unit: Percent (0.0-1.0)
Min:  0
Max:  1
```

Exemplo:

```text
0.93 → 93%
```

### 12.1 Threshold recomendado

O JSON de referência possuía um threshold `80`, porém o valor de confiança está na escala `0.0 ... 1.0`.

Para representar corretamente 80%, configure:

```text
0.00 → Red
0.80 → Green
```

Assim:

```text
confiança < 80%  → vermelho
confiança ≥ 80%  → verde
```

Em Gauge options:

```text
Show threshold markers: habilitado
Show threshold labels:  opcional/desabilitado
Orientation:             Auto
```

Clique em **Apply**.

## 13. Painel 5 — Última Peça Inspecionada

Adicione nova visualização.

Tipo:

```text
Stat
```

Título:

```text
Última Peça Inspecionada
```

Query:

```flux
from(bucket: v.defaultBucket)
  |> range(start: -30d)
  |> filter(fn: (r) => r["_measurement"] == "pecas_producao")
  |> filter(fn: (r) => r["_field"] == "aprovada")
  |> last()
  |> map(fn: (r) => ({
      r with
      _value: if r._value then 1 else 0
  }))
```

Observe que este painel usa:

```text
range(start: -30d)
```

e não o intervalo global de 6 horas. Isso permite mostrar a última peça mesmo que ela tenha sido inspecionada antes do intervalo atual do dashboard.

### 13.1 Value options

Configure:

```text
Calculation: Last not null
```

### 13.2 Value mappings

Em **Value mappings**, crie dois mapeamentos do tipo **Value**.

Primeiro:

```text
Value: 1
Text:  APROVADA
Color: Dark green
```

Segundo:

```text
Value: 0
Text:  REJEITADA
Color: Dark red
```

### 13.3 Aparência

Configure:

```text
Color mode: Background solid
Text mode:  Value
Graph mode: Area
```

O resultado esperado é um painel com fundo verde para `APROVADA` e fundo vermelho para `REJEITADA`.

Clique em **Apply**.

## 14. Painel 6 — Peças Inspecionadas ao Longo do Tempo

Adicione nova visualização.

Tipo:

```text
Time series
```

Título:

```text
Peças Inspecionadas ao Longo do Tempo
```

Este painel usa duas queries.

### 14.1 Query A — Aprovadas

```flux
from(bucket: v.defaultBucket)
  |> range(start: v.timeRangeStart, stop: v.timeRangeStop)
  |> filter(fn: (r) => r["_measurement"] == "pecas_producao")
  |> filter(fn: (r) => r["_field"] == "aprovada")
  |> filter(fn: (r) => r["_value"] == true)
  |> aggregateWindow(
      every: 10s,
      fn: count,
      createEmpty: false
  )
  |> yield(name: "Aprovadas")
```

RefId:

```text
A
```

### 14.2 Query B — Rejeitadas

Clique em **Add query**.

Use:

```flux
from(bucket: v.defaultBucket)
  |> range(start: v.timeRangeStart, stop: v.timeRangeStop)
  |> filter(fn: (r) => r["_measurement"] == "pecas_producao")
  |> filter(fn: (r) => r["_field"] == "aprovada")
  |> filter(fn: (r) => r["_value"] == false)
  |> aggregateWindow(
      every: 10s,
      fn: count,
      createEmpty: false
  )
  |> yield(name: "Rejeitadas")
```

RefId:

```text
B
```

### 14.3 Aparência

Configure:

```text
Draw style:          Lines
Line interpolation: Linear
Line width:          1
Fill opacity:        0
Show points:         Auto
Point size:          5
Stacking:            Off / None
Color scheme:        Classic palette
```

### 14.4 Legend

Configure:

```text
Show legend:   habilitado
Mode:          List
Placement:     Bottom
```

### 14.5 Tooltip

Configure:

```text
Mode: Single
```

Clique em **Apply**.

## 15. Painel 7 — Contagem — Sensor físico

Este painel depende do segundo fluxo documentado em `docs/node-red-influxdb.md`, que grava `pnaat/contagem/sensor` na measurement `contagem_esteira`.

Adicione nova visualização.

Tipo:

```text
Stat
```

Título:

```text
Contagem — Sensor físico
```

Query:

```flux
from(bucket: v.defaultBucket)
  |> range(start: v.timeRangeStart, stop: v.timeRangeStop)
  |> filter(fn: (r) => r["_measurement"] == "contagem_esteira")
  |> filter(fn: (r) => r["_field"] == "count")
  |> filter(fn: (r) => r["origem"] == "sensor_final")
  |> last()
```

Value options:

```text
Calculation: Last
```

Standard options:

```text
Unit:     none
Decimals: 0
```

Aparência:

```text
Color mode: Value
Graph mode: Area
Text mode:  Auto
```

Threshold base:

```text
Base / 0 → Green
```

Clique em **Apply**.

## 16. Organizar os painéis como no dashboard de referência

O Grafana usa uma grade de 24 colunas.

A disposição do dashboard de referência é:

| Painel | X | Y | Largura | Altura |
|---|---:|---:|---:|---:|
| Peças Aprovadas | 0 | 0 | 5 | 5 |
| Peças Rejeitadas | 5 | 0 | 5 | 5 |
| Total de Peças | 10 | 0 | 5 | 5 |
| Peças Inspecionadas ao Longo do Tempo | 15 | 0 | 9 | 10 |
| CONFIANÇA MÉDIA | 0 | 5 | 5 | 5 |
| Última Peça Inspecionada | 5 | 5 | 10 | 5 |
| Contagem — Sensor físico | 0 | 10 | 5 | 5 |

Na interface gráfica, basta arrastar e redimensionar os painéis até reproduzir essa disposição.

Não é necessário informar manualmente X/Y; esses valores são gerados pelo Grafana.

## 17. Salvar o dashboard

Depois de montar todos os painéis:

1. clique em **Save dashboard**;
2. informe o nome:

```text
Fábrica do Futuro — PNAAT
```

3. selecione/crie uma pasta do Grafana, se desejar;
4. clique em **Save**.

> **Apply** salva a edição de um painel no dashboard atual.  
> **Save dashboard** persiste o dashboard completo.

## 18. Teste completo — peça aprovada

Com o dashboard aberto, publique manualmente uma peça aprovada:

```bash
cd ~/pnaat-handson/docker

docker exec -it pnaat-mosquitto \
  mosquitto_pub \
  -h localhost \
  -p 1883 \
  -u pnaat \
  -P SUA_SENHA_MQTT \
  -t 'pnaat/peca/resultado_final' \
  -m '{"sensor_count":2001,"id_camera":2001,"resultado":"APROVADA","confianca":0.97,"source":"teste_grafana"}'
```

Também publique a contagem física correspondente:

```bash
docker exec -it pnaat-mosquitto \
  mosquitto_pub \
  -h localhost \
  -p 1883 \
  -u pnaat \
  -P SUA_SENHA_MQTT \
  -t 'pnaat/contagem/sensor' \
  -m '{"count":2001,"source":"sensor_final"}'
```

No Node-RED, os HTTP Request devem retornar:

```text
204
```

No Grafana, clique em **Refresh**.

Confirme:

```text
Total de Peças             → aumenta
Peças Aprovadas            → aumenta
Confiança Média            → inclui 97%
Última Peça Inspecionada   → APROVADA
Contagem — Sensor físico   → 2001
Time series                → registra uma aprovada
```

## 19. Teste completo — peça rejeitada

Publique:

```bash
docker exec -it pnaat-mosquitto \
  mosquitto_pub \
  -h localhost \
  -p 1883 \
  -u pnaat \
  -P SUA_SENHA_MQTT \
  -t 'pnaat/peca/resultado_final' \
  -m '{"sensor_count":2002,"id_camera":2002,"resultado":"REJEITADA","confianca":0.89,"source":"teste_grafana"}'
```

E:

```bash
docker exec -it pnaat-mosquitto \
  mosquitto_pub \
  -h localhost \
  -p 1883 \
  -u pnaat \
  -P SUA_SENHA_MQTT \
  -t 'pnaat/contagem/sensor' \
  -m '{"count":2002,"source":"sensor_final"}'
```

Depois de atualizar o dashboard:

```text
Peças Rejeitadas           → aumenta
Última Peça Inspecionada   → REJEITADA
Contagem — Sensor físico   → 2002
Time series                → registra uma rejeitada
```

## 20. Diagnóstico de painel vazio

Se um painel não mostrar dados, siga esta ordem:

### 20.1 Confirmar o intervalo de tempo

Verifique se o dashboard está em:

```text
Last 6 hours
```

ou em outro intervalo que inclua os dados.

### 20.2 Testar no Explore

Para peças:

```flux
from(bucket: "fabrica_futuro")
  |> range(start: -24h)
  |> filter(fn: (r) => r["_measurement"] == "pecas_producao")
```

Para sensor:

```flux
from(bucket: "fabrica_futuro")
  |> range(start: -24h)
  |> filter(fn: (r) => r["_measurement"] == "contagem_esteira")
```

### 20.3 Testar no InfluxDB

Se o Explore não retornar registros, abra o Data Explorer do InfluxDB.

Se os dados não existirem também no InfluxDB, o problema está antes do Grafana.

### 20.4 Verificar o datasource

Confirme:

```text
Query language: Flux
URL: http://influxdb:8086
Organization: PNAAT
Default Bucket: fabrica_futuro
```

### 20.5 Verificar erros dos containers

```bash
cd ~/pnaat-handson/docker
docker compose logs --tail=100 influxdb
docker compose logs --tail=100 grafana
```

## 21. Persistência

O Grafana utiliza o volume:

```text
grafana_data
```

Não use na operação normal:

```bash
docker compose down -v
```

Esse comando remove os volumes persistentes.

## 22. Checklist final

Antes de considerar o dashboard pronto, confirme:

- [ ] `pecas_producao` existe no InfluxDB;
- [ ] `contagem_esteira` existe no InfluxDB;
- [ ] Grafana abre na porta 3000;
- [ ] login realizado;
- [ ] datasource `InfluxDB PNAAT` criado;
- [ ] Query language = Flux;
- [ ] URL = `http://influxdb:8086`;
- [ ] Organization = `PNAAT`;
- [ ] token configurado;
- [ ] Default Bucket = `fabrica_futuro`;
- [ ] Save & test funciona;
- [ ] Explore retorna `pecas_producao`;
- [ ] Explore retorna `contagem_esteira`;
- [ ] intervalo padrão configurado em Last 6 hours;
- [ ] painel Peças Aprovadas criado;
- [ ] painel Peças Rejeitadas criado;
- [ ] painel Total de Peças criado;
- [ ] painel CONFIANÇA MÉDIA criado;
- [ ] painel Última Peça Inspecionada criado;
- [ ] painel Peças Inspecionadas ao Longo do Tempo criado;
- [ ] painel Contagem — Sensor físico criado;
- [ ] mappings APROVADA/REJEITADA configurados;
- [ ] unidade da confiança configurada como Percent (0.0-1.0);
- [ ] threshold da confiança corrigido para escala 0.0–1.0;
- [ ] dashboard organizado;
- [ ] dashboard salvo;
- [ ] teste de peça APROVADA realizado;
- [ ] teste de peça REJEITADA realizado;
- [ ] teste da contagem física realizado;
- [ ] todos os painéis atualizam após Refresh.

Com todos os itens concluídos, o dashboard do workshop está reproduzido e a cadeia MQTT → Node-RED → InfluxDB → Grafana está validada.

## Próxima etapa — montagem física e validação final

A instalação lógica do sistema está concluída. Continue com os modelos e suportes mecânicos da bancada em:

[**Montagens mecânicas — modelos 3D**](../montagens-mecanicas/modelos-3d/README.md)

Depois da montagem física, retorne ao [README principal](../README.md) e execute a validação integrada da sequência completa:

```text
câmera → YOLO → MQTT → ESP32 → sensor/LEDs
                         ↓
                      Node-RED
                         ↓
                      InfluxDB
                         ↓
                       Grafana
```
