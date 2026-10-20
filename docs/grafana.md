# Grafana — Dashboard da Fábrica do Futuro

Este guia parte do MING já instalado e da integração Node-RED → InfluxDB validada. O objetivo é configurar o Grafana desde o primeiro acesso até um dashboard funcional, sem assumir etapas intermediárias.

O fluxo esperado antes de iniciar é:

```text
MQTT → Node-RED → InfluxDB
                    ↓
                  Grafana
```

O bucket utilizado é:

```text
fabrica_futuro
```

e a measurement esperada é:

```text
pecas_producao
```

## 1. Pré-requisitos

Na Raspberry Pi:

```bash
cd ~/pnaat-handson/docker
docker compose ps
```

Confirme que estão em execução:

```text
pnaat-influxdb
pnaat-grafana
```

Teste o InfluxDB:

```bash
curl -fsS http://localhost:8086/health
```

Teste o Grafana:

```bash
curl -fsS http://localhost:3000/api/health
```

Descubra o IP:

```bash
hostname -I
```

Abra no navegador:

```text
http://IP_DA_RASPBERRY:3000
```

## 2. Entrar no Grafana

Use as credenciais definidas no arquivo:

```text
~/pnaat-handson/docker/.env
```

As variáveis são:

```text
GRAFANA_USER
GRAFANA_PASSWORD
```

Para conferir somente o nome do usuário:

```bash
cd ~/pnaat-handson/docker
grep '^GRAFANA_USER=' .env
```

Não copie a senha para documentação, Git ou prints públicos.

## 3. Confirmar que existem dados antes de criar o dashboard

Antes de configurar o Grafana, abra:

```text
http://IP_DA_RASPBERRY:8086
```

Entre no InfluxDB e use **Data Explorer**.

Confirme:

```text
Bucket:       fabrica_futuro
Measurement:  pecas_producao
Fields:
  id
  id_camera
  aprovada
  confianca
```

Se ainda não houver dados, volte ao tutorial [node-red-influxdb.md](node-red-influxdb.md) e execute as publicações manuais de teste.

O Grafana não mostrará valores se o InfluxDB estiver vazio.

## 4. Obter Organization, Bucket e Token

A configuração esperada está no `.env` da stack.

Na Raspberry Pi:

```bash
cd ~/pnaat-handson/docker
grep '^INFLUXDB_ORG=' .env
grep '^INFLUXDB_BUCKET=' .env
```

Resultado esperado:

```text
INFLUXDB_ORG=PNAAT
INFLUXDB_BUCKET=fabrica_futuro
```

Para usar o token no Grafana, consulte-o localmente:

```bash
grep '^INFLUXDB_TOKEN=' .env
```

Copie o valor apenas para a configuração do datasource.

## 5. Criar o datasource InfluxDB

Na interface do Grafana:

1. abra **Connections**;
2. entre em **Data sources** ou **Add new connection**, conforme a interface exibida;
3. escolha **InfluxDB**;
4. crie um novo datasource.

Configure:

```text
Name:          InfluxDB PNAAT
Query language: Flux
URL:           http://influxdb:8086
```

> Use `influxdb`, não `localhost`. Grafana e InfluxDB estão em containers diferentes na mesma rede Docker.

Na seção de autenticação/InfluxDB Details, configure:

```text
Organization:  PNAAT
Token:         valor de INFLUXDB_TOKEN
Default Bucket: fabrica_futuro
```

Não habilite Basic Auth para esta configuração.

Clique em **Save & test**.

O resultado deve indicar que o datasource está funcionando.

### Se aparecer 401 Unauthorized

O token está incorreto ou não corresponde ao token criado na primeira inicialização do InfluxDB.

### Se aparecer Bad Gateway / connection refused

Verifique:

```text
URL = http://influxdb:8086
```

e:

```bash
cd ~/pnaat-handson/docker
docker compose ps
docker compose logs --tail=100 influxdb
docker compose logs --tail=100 grafana
```

## 6. Fazer uma consulta de teste no Grafana

Antes de construir painéis, abra **Explore**.

Selecione:

```text
InfluxDB PNAAT
```

Use a consulta:

```flux
from(bucket: "fabrica_futuro")
  |> range(start: -1h)
  |> filter(fn: (r) => r["_measurement"] == "pecas_producao")
```

Execute.

Se não aparecer nada:

1. aumente o intervalo para `-24h`;
2. confirme no InfluxDB Data Explorer que existem dados;
3. confira o bucket;
4. confira o datasource.

Só prossiga quando esta consulta retornar registros.

## 7. Criar o dashboard

Na interface do Grafana:

1. abra **Dashboards**;
2. clique em **New**;
3. escolha **New dashboard**;
4. clique em **Add visualization**;
5. escolha o datasource `InfluxDB PNAAT`.

Crie os painéis abaixo.

> Durante os testes, use no canto superior direito um intervalo como **Last 15 minutes**, **Last 1 hour** ou **Last 24 hours** que inclua os dados recém-gravados. Um intervalo incorreto é uma causa comum de painel vazio.

## 8. Painel — Total de peças

Visualização:

```text
Stat
```

Título:

```text
Total de peças
```

Consulta:

```flux
from(bucket: v.defaultBucket)
  |> range(start: v.timeRangeStart, stop: v.timeRangeStop)
  |> filter(fn: (r) => r["_measurement"] == "pecas_producao")
  |> filter(fn: (r) => r["_field"] == "id")
  |> count()
```

Em **Value options / Calculation**, use o último valor retornado se necessário.

Salve o painel com **Apply**.

## 9. Painel — Peças aprovadas

Adicione nova visualização **Stat**.

Título:

```text
Peças aprovadas
```

Consulta:

```flux
from(bucket: v.defaultBucket)
  |> range(start: v.timeRangeStart, stop: v.timeRangeStop)
  |> filter(fn: (r) => r["_measurement"] == "pecas_producao")
  |> filter(fn: (r) => r["_field"] == "aprovada")
  |> filter(fn: (r) => r["_value"] == true)
  |> count()
```

Clique em **Apply**.

## 10. Painel — Peças rejeitadas

Visualização:

```text
Stat
```

Título:

```text
Peças rejeitadas
```

Consulta:

```flux
from(bucket: v.defaultBucket)
  |> range(start: v.timeRangeStart, stop: v.timeRangeStop)
  |> filter(fn: (r) => r["_measurement"] == "pecas_producao")
  |> filter(fn: (r) => r["_field"] == "aprovada")
  |> filter(fn: (r) => r["_value"] == false)
  |> count()
```

Clique em **Apply**.

## 11. Painel — Confiança média

Visualização:

```text
Stat
```

Título:

```text
Confiança média
```

Consulta:

```flux
from(bucket: v.defaultBucket)
  |> range(start: v.timeRangeStart, stop: v.timeRangeStop)
  |> filter(fn: (r) => r["_measurement"] == "pecas_producao")
  |> filter(fn: (r) => r["_field"] == "confianca")
  |> mean()
```

Em **Standard options → Unit**, escolha:

```text
Percent (0.0-1.0)
```

Por exemplo, `0.93` será exibido como aproximadamente `93%`.

Clique em **Apply**.

## 12. Painel — Peças ao longo do tempo

Crie uma visualização:

```text
Time series
```

Título:

```text
Peças ao longo do tempo
```

Use duas queries.

### Query A — Aprovadas

```flux
from(bucket: v.defaultBucket)
  |> range(start: v.timeRangeStart, stop: v.timeRangeStop)
  |> filter(fn: (r) => r["_measurement"] == "pecas_producao")
  |> filter(fn: (r) => r["_field"] == "aprovada")
  |> filter(fn: (r) => r["_value"] == true)
  |> aggregateWindow(
      every: v.windowPeriod,
      fn: count,
      createEmpty: false
  )
  |> yield(name: "Aprovadas")
```

### Query B — Rejeitadas

```flux
from(bucket: v.defaultBucket)
  |> range(start: v.timeRangeStart, stop: v.timeRangeStop)
  |> filter(fn: (r) => r["_measurement"] == "pecas_producao")
  |> filter(fn: (r) => r["_field"] == "aprovada")
  |> filter(fn: (r) => r["_value"] == false)
  |> aggregateWindow(
      every: v.windowPeriod,
      fn: count,
      createEmpty: false
  )
  |> yield(name: "Rejeitadas")
```

Confirme que as duas séries aparecem e clique em **Apply**.

## 13. Painel — Última peça inspecionada

Crie uma visualização:

```text
Stat
```

Título:

```text
Última peça inspecionada
```

Consulta:

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

Depois configure **Value mappings**:

```text
1 → APROVADA
0 → REJEITADA
```

Opcionalmente associe uma cor a cada mapping.

Use **Calculation = Last** se a visualização solicitar uma redução.

Clique em **Apply**.

## 14. Organizar e salvar o dashboard

Depois de criar os painéis:

1. redimensione e organize os cards;
2. clique no ícone **Save dashboard**;
3. dê o nome:

```text
Fábrica do Futuro — PNAAT
```

4. escolha/crie uma pasta do Grafana se desejar;
5. confirme em **Save**.

Não confunda **Apply**, que salva alterações no painel dentro do editor, com **Save dashboard**, que persiste o dashboard completo.

## 15. Teste final com dados novos

Com o dashboard aberto, publique uma peça de teste no Mosquitto:

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

O Node-RED deve gravar o dado no InfluxDB.

No Grafana:

1. escolha um intervalo que inclua o momento atual;
2. clique em **Refresh**;
3. confira se `Total de peças` aumentou;
4. confira se `Peças aprovadas` aumentou;
5. confira se `Última peça inspecionada` mostra `APROVADA`.

Repita com:

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

Depois de atualizar o dashboard, a última peça deve indicar `REJEITADA`.

## 16. Problemas comuns

### Datasource funciona, mas painel está vazio

Verifique primeiro o intervalo de tempo do dashboard.

Depois use **Explore** para executar:

```flux
from(bucket: "fabrica_futuro")
  |> range(start: -24h)
  |> filter(fn: (r) => r["_measurement"] == "pecas_producao")
```

### Grafana não conecta ao InfluxDB

Não use:

```text
http://localhost:8086
```

Use:

```text
http://influxdb:8086
```

### 401 Unauthorized

Confira o `INFLUXDB_TOKEN` do `.env`.

### Painel de confiança mostra 0.93 em vez de 93%

Configure:

```text
Standard options → Unit → Percent (0.0-1.0)
```

### Última peça mostra 0 ou 1

Configure os Value mappings:

```text
1 → APROVADA
0 → REJEITADA
```

## 17. Persistência

O Grafana utiliza o volume Docker:

```text
grafana_data
```

Por isso, não use durante operação normal:

```bash
docker compose down -v
```

Esse comando remove volumes persistentes.

## 18. Checklist de validação

- [ ] InfluxDB contém dados em `pecas_producao`;
- [ ] Grafana abre na porta 3000;
- [ ] login realizado;
- [ ] datasource `InfluxDB PNAAT` criado;
- [ ] query language configurada como Flux;
- [ ] URL interna configurada como `http://influxdb:8086`;
- [ ] Organization configurada como `PNAAT`;
- [ ] Token configurado;
- [ ] Default Bucket configurado como `fabrica_futuro`;
- [ ] Save & test retorna sucesso;
- [ ] consulta no Explore retorna dados;
- [ ] painel Total de peças funcionando;
- [ ] painel Peças aprovadas funcionando;
- [ ] painel Peças rejeitadas funcionando;
- [ ] painel Confiança média funcionando;
- [ ] painel Peças ao longo do tempo funcionando;
- [ ] painel Última peça inspecionada funcionando;
- [ ] dashboard salvo como `Fábrica do Futuro — PNAAT`;
- [ ] nova publicação MQTT aparece no dashboard após Refresh.

Com todos os itens concluídos, a cadeia MQTT → Node-RED → InfluxDB → Grafana está validada.
