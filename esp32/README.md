# ESP32-S3 — sensor final, associação com visão, LEDs e OTA

Projeto do **PNAAT Hands-on** para **ESP-IDF v5.5.2** e **ESP32-S3**.

## Pré-requisitos desta etapa

Antes de configurar o ESP32, conclua a instalação da infraestrutura na Raspberry Pi conforme:

[**Instalação completa do MING na Raspberry Pi 5**](../docs/instalacao-ming.md)

O ESP32 depende do broker MQTT já em execução. Antes de continuar, confirme que você possui:

- o endereço IPv4 da Raspberry Pi;
- o usuário MQTT `pnaat`;
- a senha MQTT definida durante a instalação do Mosquitto;
- o Mosquitto funcionando na porta `1883`.

A visão de máquina **ainda não precisa estar instalada**. Nesta etapa vamos deixar o ESP32 funcionando de forma independente, conectado ao Wi-Fi e ao Mosquitto, com o sensor físico validado. A integração real com a câmera será concluída na etapa seguinte.

O firmware foi organizado para permitir habilitar ou desabilitar individualmente sensor, LEDs e controle da esteira pelo `menuconfig`. As opções desabilitadas não impedem a compilação das demais funções.

## Preparar o ambiente de desenvolvimento no Windows

Durante o workshop, o firmware do ESP32-S3 será aberto, configurado, compilado e gravado usando **Visual Studio Code + extensão oficial Espressif IDF**.

O projeto foi validado com:

```text
ESP-IDF v5.5.2
Target: ESP32-S3
```

> Não instale automaticamente a versão estável mais recente do ESP-IDF. Para reproduzir o ambiente usado no projeto, selecione explicitamente a versão **v5.5.2**.

### 1. Instalar o Visual Studio Code

Instale o Visual Studio Code no notebook Windows.

Depois de instalado, abra o VS Code.

### 2. Instalar a extensão Espressif IDF

No VS Code:

1. abra **Extensions** com `Ctrl+Shift+X`;
2. procure por `ESP-IDF`;
3. instale a extensão oficial **Espressif IDF**.

### 3. Instalar o ESP-IDF v5.5.2

Abra a Command Palette:

```text
Ctrl+Shift+P
```

Procure e execute:

```text
ESP-IDF: Open ESP-IDF Installation Manager
```

No Installation Manager:

1. inicie a instalação;
2. escolha uma instalação normal/Easy Installation;
3. selecione explicitamente **ESP-IDF v5.5.2**;
4. conclua a instalação das ferramentas e do ambiente Python.

As versões atuais da extensão utilizam o **ESP-IDF Installation Manager (EIM)**. Não procure o comando antigo `ESP-IDF: Configure ESP-IDF Extension`, pois ele foi substituído pelo Installation Manager.

### 4. Confirmar a instalação

No VS Code, abra novamente:

```text
Ctrl+Shift+P
```

Execute:

```text
ESP-IDF: Open ESP-IDF Terminal
```

No terminal aberto pela extensão, execute:

```powershell
idf.py --version
```

O resultado deve indicar:

```text
ESP-IDF v5.5.2
```

Não prossiga enquanto outra versão estiver selecionada.

### 5. Abrir o projeto ESP32

Se o repositório já foi clonado no notebook, no VS Code escolha:

```text
File → Open Folder
```

e abra somente a pasta:

```text
pnaat-handson\esp32
```

Essa pasta contém o `CMakeLists.txt` raiz do projeto ESP-IDF.

O target já está fixado como `ESP32-S3` no projeto, portanto não é necessário executar `idf.py set-target esp32s3`.

### 6. Conectar a placa

Conecte o ESP32-S3 ao notebook através do cabo USB.

Se necessário, na Command Palette utilize os comandos ESP-IDF para selecionar a porta serial da placa.

A partir desse ponto o projeto pode ser configurado pelo `menuconfig`, compilado, gravado e monitorado pelo terminal ESP-IDF ou pelos comandos da extensão.


## Funções atuais

- target fixo em ESP32-S3;
- flash configurada para 8 MB;
- E18-D80NK como sensor final;
- debounce e contagem por transição livre → peça presente;
- persistência da contagem física em NVS;
- publicação de presença e contagem por MQTT;
- recepção do resultado da visão por MQTT;
- associação FIFO entre resultado da câmera e passagem no sensor final;
- tolerância a inversão de chegada entre mensagem da câmera e sensor;
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

Se houver atraso de rede e o sensor chegar primeiro, o evento físico também pode aguardar o resultado da visão.

As filas possuem profundidade padrão de 16 eventos e timeout padrão de 30 s. Isso evita associar uma mensagem antiga a uma peça nova.

## Sensor final

O contador é incrementado somente na transição:

```text
LIVRE → PEÇA PRESENTE
```

O valor é salvo em NVS e restaurado após reinicialização.

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


## Critério para concluir esta etapa

Antes de seguir para a visão de máquina, confirme:

- o firmware compila sem erro;
- o ESP32 conecta ao Wi-Fi;
- o ESP32 conecta ao Mosquitto da Raspberry Pi;
- o sensor final é reconhecido;
- cada passagem física incrementa a contagem uma única vez;
- `pnaat/contagem/sensor` é publicado no broker;
- `pnaat/sensor/presenca` muda de acordo com o estado do sensor.

Não é necessário validar ainda uma classificação real da câmera. Essa validação depende da etapa de visão de máquina.

## Próxima etapa — visão de máquina

Com o ESP32 operacional e o Mosquitto já funcionando, continue em:

[**Instalação e execução do classificador de engrenagens**](../vision/classificador_engrenagens/README.md)

Nessa etapa a Raspberry Pi será preparada para capturar as imagens da OV5647, executar o modelo YOLO, publicar `pnaat/visao/resultado` e finalmente testar a associação câmera ↔ sensor no ESP32.
