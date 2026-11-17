# ESP32-S3 — sensor final, associação com visão, LEDs e OTA

Projeto do **PNAAT Hands-on** para **ESP-IDF v5.5.2** e **ESP32-S3**.

O firmware foi organizado para permitir habilitar ou desabilitar individualmente sensor, LEDs e controle da esteira pelo `menuconfig`. As opções desabilitadas não impedem a compilação das demais funções.

## Funções atuais

- target fixo em ESP32-S3;
- flash configurada para 8 MB;
- E18-D80NK como sensor final;
- debounce, intervalo mínimo entre contagens e contagem por transição livre → peça presente;
- persistência da contagem física em NVS;
- publicação de presença e contagem por MQTT;
- recepção do resultado da visão por MQTT;
- associação FIFO entre resultado da câmera e passagem no sensor final;
- tolerância curta à inversão de chegada entre mensagem da câmera e sensor;
- rejeição de `id_camera` duplicado recebido via MQTT QoS 1;
- descarte de associações antigas por timeout;
- LED de peça aprovada e LED de peça rejeitada;
- validação de conflito entre GPIOs habilitados;
- controle RUN/STOP opcional da esteira;
- OTA de desenvolvimento via MQTT + HTTP local.

## Sequência Wi-Fi → MQTT

O cliente MQTT é criado durante a inicialização, mas **não tenta abrir conexão antes de o ESP32 receber um endereço IP**.

A sequência de boot é:

```text
Wi-Fi STA inicia
      ↓
autenticação/associação
      ↓
DHCP
      ↓
IP_EVENT_STA_GOT_IP
      ↓
APP_EVENT_WIFI_GOT_IP
      ↓
mqtt_manager_start()
      ↓
conexão MQTT
      ↓
subscriptions
```

Se o Wi-Fi cair, o firmware pausa o cliente MQTT e mantém a tentativa de reconexão Wi-Fi. Quando um novo endereço IP é obtido, o MQTT é iniciado novamente. Isso evita mensagens de `Host is unreachable` causadas por tentativas MQTT durante os primeiros instantes do boot, antes de existir rota IP.

## Tópicos MQTT

### Publicados pelo ESP32

```text
pnaat/sistema/status
pnaat/sensor/presenca
pnaat/contagem/sensor
pnaat/esteira/estado
pnaat/peca/resultado_final
pnaat/ota/status
```

Exemplo de presença:

```json
{"presente":true,"source":"sensor_final"}
```

Exemplo de contagem:

```json
{"count":17,"source":"sensor_final"}
```

Quando câmera e sensor são associados:

```json
{
  "sensor_count":17,
  "id_camera":17,
  "resultado":"APROVADA",
  "confianca":0.93,
  "source":"esp32_matcher"
}
```

### Assinados pelo ESP32

```text
pnaat/esteira/comando
pnaat/visao/resultado
```

O resultado da visão aceita o campo principal `resultado` e, para compatibilidade, também `classificacao`.

Exemplos aceitos:

```json
{"id_camera":18,"resultado":"APROVADA","confianca":0.93}
```

```json
{"id_camera":19,"resultado":"REJEITADA","confianca":0.88}
```

Também são aceitos `OK` e `NOK`. Estados como `PENDENTE_CALIBRACAO`, `NAO_CALIBRADO` ou `INCONCLUSIVA` são preservados como `INCONCLUSIVA`; nesse caso a associação com a peça é mantida, os dois LEDs ficam apagados e o resultado final continua sendo publicado sem deslocar a sequência FIFO.

## Associação câmera ↔ sensor final

Por padrão:

```text
CONFIG_PNAAT_MATCH_VISION_WITH_SENSOR=y
```

A câmera fica a montante do sensor final. Normalmente o fluxo será:

```text
câmera classifica
      ↓
resultado entra na fila
      ↓
peça chega ao E18-D80NK
      ↓
ESP32 associa os eventos em ordem
      ↓
LED correspondente
      ↓
pnaat/peca/resultado_final
```

Como a câmera está fisicamente antes do sensor final, o fluxo esperado é que o resultado de visão chegue primeiro ao ESP32.

Se o sensor chegar primeiro por pequena latência de rede, o evento físico aguarda somente:

```text
500 ms
```

Esse valor é configurável em:

```text
PNAAT Hands-on
  → Integração visão + sensor
    → Janela para visão chegar após o sensor (ms)
```

Depois dessa janela, a contagem física sem resultado de visão é descartada do matcher. Isso é intencional: é melhor deixar uma peça sem indicação do que associar a classificação da peça seguinte e deslocar toda a sequência.

Os resultados da câmera permanecem em FIFO e usam o timeout geral configurável, padrão de 30 s.

O matcher também ignora a repetição imediata do mesmo `id_camera`. Essa proteção evita que uma eventual entrega duplicada de uma mensagem MQTT QoS 1 gere duas entradas para a mesma peça.

## Sensor final

O contador é incrementado somente na transição:

```text
LIVRE → PEÇA PRESENTE
```

Além do debounce de 40 ms, existe um bloqueio de novas contagens após uma peça válida:

```text
Tempo mínimo entre contagens = 800 ms
```

Durante essa janela, novas transições do sensor podem ser registradas no log, mas não incrementam a contagem nem entram no matcher.

Isso evita que dentes, recortes ou oscilações do sensor durante a passagem da mesma engrenagem produzam duas peças físicas e desloquem a associação com a câmera.

O valor de 800 ms pode ser ajustado em:

```text
PNAAT Hands-on
  → Sensor final de contagem
    → Tempo mínimo entre contagens (ms)
```

O valor da contagem é salvo em NVS e restaurado após reinicialização.

O comando:

```json
{"cmd":"RESET_CONTAGEM_SENSOR"}
```

zera a contagem, limpa as filas de associação e apaga os LEDs.

## LEDs

Abra:

```powershell
idf.py menuconfig
```

e acesse:

```text
PNAAT Hands-on
  → Indicador visual da inspeção
```

Configure:

- habilitar LEDs;
- GPIO do LED APROVADA;
- GPIO do LED REJEITADA;
- nível lógico ativo.

Os GPIOs 6 e 7 são apenas valores iniciais de configuração. Devem ser confirmados na pinagem física da placa antes da montagem.

Quando a associação visão/sensor está habilitada, os LEDs só mudam após a peça física ser associada ao resultado da visão. Se uma nova peça chegar ao sensor e o resultado ainda não estiver disponível, os dois LEDs são apagados temporariamente para não exibir a classificação da peça anterior.

## Verificação de GPIOs

Na inicialização, o firmware verifica se funções habilitadas estão tentando usar o mesmo GPIO.

São verificados:

- sensor final;
- RUN/STOP da esteira;
- LED APROVADA;
- LED REJEITADA.

Em caso de conflito o firmware registra o erro e interrompe a inicialização por `ESP_ERROR_CHECK`.

## Controle da esteira

Com o controle habilitado:

```json
{"cmd":"START"}
```

```json
{"cmd":"STOP"}
```

Se o controle estiver desabilitado, `START` não publica falsamente o estado `RUNNING`.

Durante OTA, o firmware executa STOP antes da atualização.

## ESP32-S3 fixo

O projeto não é multi-target. O `CMakeLists.txt` contém:

```cmake
set(IDF_TARGET "esp32s3" CACHE STRING "ESP-IDF target" FORCE)
```

Não é necessário executar `idf.py set-target esp32s3` a cada compilação.

## Flash e OTA

A placa usada foi detectada com **8 MB de flash**.

A tabela de partições usa:

```text
factory = 2 MB
ota_0   = 2 MB
ota_1   = 2 MB
```

Para uma primeira gravação após mudanças de tabela/configuração:

```powershell
idf.py fullclean
idf.py build
idf.py -p COMx erase-flash
idf.py -p COMx flash monitor
```

Depois:

```powershell
idf.py build
python tools\ota_deploy.py
```

O OTA é uma ferramenta de desenvolvimento e não faz parte do conteúdo do workshop.

## Teste de compilação recomendado

Depois de atualizar o repositório:

```powershell
cd esp32
idf.py fullclean
idf.py reconfigure
idf.py build
```

Depois teste também as opções habilitadas em:

```text
idf.py menuconfig
  PNAAT Hands-on
```

Especialmente:

- Sensor final de contagem;
- Integração visão + sensor;
- Indicador visual da inspeção;
- Controle da esteira.
