# ESP32-S3 — sensor final, LEDs de inspeção e OTA

Projeto para **ESP-IDF v5.5.2** e ESP32-S3.

## Funções atuais

- E18-D80NK no final da esteira;
- contagem física independente da câmera;
- publicação do estado de presença do sensor;
- recepção do resultado da visão por MQTT;
- LED de peça aprovada e LED de peça rejeitada;
- controle RUN/STOP da esteira;
- OTA somente para facilitar o desenvolvimento.

## Sensor final

O sensor publica dois tipos de informação:

```text
pnaat/sensor/presenca
pnaat/contagem/sensor
```

Exemplos:

```json
{"presente":true,"source":"sensor_final"}
```

```json
{"count":17,"source":"sensor_final"}
```

O contador é incrementado somente quando o sensor passa de livre para peça presente.

## Resultado da visão e LEDs

O ESP32 se inscreve em:

```text
pnaat/visao/resultado
```

Quando a Raspberry publicar, por exemplo:

```json
{"id_camera":18,"resultado":"APROVADA","confianca":0.93}
```

o LED APROVADA é ligado e o LED REJEITADA é desligado.

Para:

```json
{"id_camera":19,"resultado":"REJEITADA","confianca":0.88}
```

o LED REJEITADA é ligado e o LED APROVADA é desligado.

Os LEDs mostram o **último resultado recebido da câmera** e ficam nesse estado até chegar o próximo resultado. Se o resultado for desconhecido ou `NAO_CALIBRADO`, ambos são desligados.

## Configuração dos LEDs

Abra:

```powershell
idf.py menuconfig
```

e vá para:

```text
PNAAT Hands-on
  → Indicador visual da inspeção
```

Habilite o indicador e configure:

- GPIO do LED APROVADA;
- GPIO do LED REJEITADA;
- nível lógico ativo.

Os valores 6 e 7 existentes no menu são apenas defaults de configuração. Confirme GPIOs realmente livres na placa antes de conectar os LEDs.

Para LEDs comuns ligados do GPIO ao GND, use um resistor em série adequado e nível ativo `1`.

## ESP32-S3 fixo

O `CMakeLists.txt` força o target `esp32s3` para este projeto. Não é necessário executar `set-target` a cada compilação.

## Flash

O hardware usado no projeto foi detectado pelo próprio ESP32-S3 com **8 MB** de flash. O `sdkconfig.defaults` foi atualizado para 8 MB.

A tabela OTA continua usando:

```text
factory = 2 MB
ota_0   = 2 MB
ota_1   = 2 MB
```

## Primeira gravação após mudança de tabela/configuração

Se ainda estiver migrando de uma configuração anterior:

```powershell
idf.py fullclean
idf.py build
idf.py -p COMx erase-flash
idf.py -p COMx flash monitor
```

Depois disso as próximas versões podem seguir pelo OTA já configurado.

## OTA

```powershell
idf.py build
python tools\ota_deploy.py
```

O OTA permanece uma ferramenta de desenvolvimento e não faz parte do conteúdo do workshop.
