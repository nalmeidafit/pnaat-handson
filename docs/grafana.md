# Grafana — Dashboard da Fábrica do Futuro

Datasource utilizado:

- Tipo: InfluxDB OSS 2.x
- Query language: Flux
- URL interna: `http://influxdb:8086`
- Organization: `PNAAT`
- Default Bucket: `fabrica_futuro`

## Painéis validados

### Total de peças

```flux
from(bucket: "fabrica_futuro")
  |> range(start: v.timeRangeStart)
  |> filter(fn: (r) => r._measurement == "pecas_producao")
  |> filter(fn: (r) => r._field == "confianca")
  |> count()
  |> group()
  |> sum()
```

### Peças aprovadas

```flux
from(bucket: "fabrica_futuro")
  |> range(start: v.timeRangeStart, stop: v.timeRangeStop)
  |> filter(fn: (r) =>
      r._measurement == "pecas_producao" and
      r._field == "aprovada_num"
  )
  |> filter(fn: (r) => r._value == 1)
  |> group()
  |> count()
```

### Peças rejeitadas

```flux
from(bucket: "fabrica_futuro")
  |> range(start: v.timeRangeStart, stop: v.timeRangeStop)
  |> filter(fn: (r) =>
      r._measurement == "pecas_producao" and
      r._field == "aprovada_num"
  )
  |> filter(fn: (r) => r._value == 0)
  |> group()
  |> count()
```

### Confiança da ultima peça
```flux
from(bucket: "fabrica_futuro")
  |> range(start: -30d)
  |> filter(fn: (r) =>
      r._measurement == "pecas_producao" and
      r._field == "confianca"
  )
  |> group(columns: [])
  |> last()
```
### Confiança Média

```flux
from(bucket: v.defaultBucket)
  |> range(start: v.timeRangeStart, stop: v.timeRangeStop)
  |> filter(fn: (r) => r["_measurement"] == "pecas_producao")
  |> filter(fn: (r) => r["_field"] == "confianca")
  |> mean()
```

Use unidade `Percent (0.0-1.0)`.

### Peças ao longo do tempo

Use duas queries no mesmo painel `Time series`: aprovadas e rejeitadas, cada uma filtrando o booleano e aplicando `aggregateWindow(..., fn: count)`.

### Última peça inspecionada

Para manter o Stat robusto, converter booleano em 0/1 e usar Value mappings no Grafana:

```flux
from(bucket: "fabrica_futuro")
  |> range(start: -30d)
  |> filter(fn: (r) =>
      r._measurement == "pecas_producao" and
      r._field == "aprovada_num"
  )
  |> group(columns: [])
  |> last()
```

Value mappings:

- `1 → APROVADA`
- `0 → REJEITADA`

A cor pode ser associada aos mappings.
