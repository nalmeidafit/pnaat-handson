# ESP32-S3 — esteira, contador físico e OTA de desenvolvimento

Projeto para **ESP-IDF v5.5.2** e **Heltec WiFi LoRa 32 V4**.

O OTA foi incluído somente para facilitar o desenvolvimento quando a esteira está longe do PC. Não faz parte do conteúdo do workshop.

## Primeira gravação após adicionar OTA

A tabela de partições mudou. Portanto, esta primeira instalação precisa ser feita por USB e com a flash apagada:

```powershell
idf.py set-target esp32s3
idf.py menuconfig
idf.py build
idf.py -p COMx erase-flash
idf.py -p COMx flash monitor
```

Depois dessa gravação, as próximas versões da aplicação podem ser enviadas por OTA.

> A Heltec WiFi LoRa 32 V4 possui 16 MB de flash; este projeto usa três partições de aplicação de 2 MB (`factory`, `ota_0`, `ota_1`).

## Preparar o PC para deploy OTA

Uma única vez:

```powershell
python -m pip install -r tools\requirements.txt
Copy-Item tools\ota_config.example.json tools\ota_config.json
```

Edite `tools\ota_config.json` e coloque o IP do Mosquitto, usuário e senha. Esse arquivo está no `.gitignore`.

## Atualizar por OTA

No terminal ESP-IDF dentro da pasta `esp32`:

```powershell
idf.py build
python tools\ota_deploy.py
```

O script descobre o IP local do PC, abre temporariamente a porta HTTP `8070`, serve `build/pnaat_esteira.bin` e publica via MQTT:

```json
{
  "cmd": "OTA_UPDATE",
  "url": "http://IP_DO_PC:8070/pnaat_esteira.bin"
}
```

O ESP32 para a esteira antes de iniciar a atualização, grava a nova imagem na partição OTA inativa e reinicia quando a validação do binário termina.

### VS Code

Abrindo a pasta `esp32` como workspace, use:

```text
Terminal → Run Task → PNAAT: OTA Deploy
```

Essa task executa primeiro o build e depois o deploy OTA.

## Status OTA

O ESP32 publica:

```text
pnaat/ota/status
```

Estados principais:

```text
IDLE
STARTING
SUCCESS_REBOOTING
FAILED
START_FAILED
```

## Observação de segurança

Esta revisão permite **HTTP sem TLS** exclusivamente para desenvolvimento em rede local. Para produção, o servidor deve usar HTTPS com verificação de certificado.
