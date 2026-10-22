# Classificador de engrenagens — YOLO + MQTT

## Modos de execução do classificador

A pasta contém duas versões do classificador, com a mesma lógica de detecção, calibração do fundo, rearme, contagem persistente e publicação MQTT:

```text
gear_image_class_noScreen.py
```

Versão recomendada para operação normal, SSH e execução sem ambiente gráfico.

```text
gear_image_class.py
```

Versão de bancada com visualização. Exibe a imagem da câmera, ROI principal, linha de trigger, FPS, pixels pretos, baseline, delta, estado do detector, contagem e a última classificação. Pressione `Esc` ou `Q` para encerrar.

A versão visual requer sessão gráfica disponível na Raspberry Pi. Para operação remota somente por terminal, utilize a versão `noScreen`.

O modelo consolidado `best.pt` é versionado nesta pasta. O arquivo `best.pt.sha256` permite validar sua integridade.


Este diretório contém a versão operacional do classificador usado no workshop, executada na Raspberry Pi 5 com câmera OV5647 e integrada ao Mosquitto.

## Modelo validado

O arquivo fornecido para a bancada foi verificado e possui:

```text
Arquivo:       best.pt
Tamanho:       aproximadamente 2,9 MB
Tipo:          Ultralytics ClassificationModel
Base:          yolov8n-cls
Ultralytics:   8.4.168
Task:          classify
Classe 0:      NOK
Classe 1:      OK
SHA-256:       fd7c021806c16d44fb26c76eaeb2a4ba469627a9bce12da198bc98acf7ea2ed2
```

A versão de Ultralytics usada pelo modelo coincide com a versão fixada em `requirements.txt`.

## Estrutura esperada

```text
classificador_engrenagens/
├── gear_image_class.py
├── gear_image_class_noScreen.py
├── requirements.txt
├── .env.example
├── best.pt
└── .venv/                  # criada localmente; não vai para o Git
```

O arquivo `best.pt` deve ficar **na mesma pasta do script**.

## 1. Partir de uma Raspberry Pi recém-instalada

Este procedimento considera uma Raspberry Pi 5 com Raspberry Pi OS 64 bits, rede configurada e acesso ao terminal.

Atualize o sistema:

```bash
sudo apt update
sudo apt full-upgrade -y
sudo reboot
```

Após o reboot, abra novamente o terminal.

## 2. Obter ou atualizar o repositório

Se o repositório ainda não foi clonado:

```bash
cd ~
git clone https://github.com/nalmeidafit/pnaat-handson.git
```

Como o repositório é público, **não é necessário fazer login no GitHub para o clone**.

Entre no projeto:

```bash
cd ~/pnaat-handson
```

Confirme:

```bash
git status
git log -1 --oneline
```

Se o repositório já estava clonado durante a instalação do MING, apenas atualize:

```bash
cd ~/pnaat-handson
git pull --ff-only origin main
```

## 3. Instalar as dependências de sistema

Instale os pacotes usados pela câmera, Picamera2, ambiente virtual e OpenCV:

```bash
sudo apt update
sudo apt install -y \
  rpicam-apps \
  python3-picamera2 \
  python3-venv \
  python3-opencv
```

O pacote `rpicam-apps` fornece os utilitários de teste da câmera utilizados nas etapas seguintes.

## 4. Confirmar que a câmera é reconhecida

Liste as câmeras:

```bash
rpicam-hello --list-cameras
```

A câmera OV5647 deve aparecer na lista.

Em uma sessão gráfica da Raspberry Pi, faça também um teste de preview:

```bash
rpicam-hello
```

Uma janela de visualização deve abrir por alguns segundos.

> Para executar a versão visual `gear_image_class.py`, é necessário estar em uma sessão gráfica da Raspberry Pi, por exemplo com monitor conectado ou acesso remoto a um desktop gráfico. SSH puro, sem encaminhamento gráfico, deve usar a versão `gear_image_class_noScreen.py`.

## 5. Entrar na pasta do classificador

```bash
cd ~/pnaat-handson/vision/classificador_engrenagens
```

Confira os arquivos:

```bash
ls -lh
```

Devem existir pelo menos:

```text
gear_image_class.py
gear_image_class_noScreen.py
requirements.txt
best.pt
best.pt.sha256
```

O modelo `best.pt` já faz parte da branch `main`; não é necessário copiá-lo manualmente para uma instalação nova.

## 6. Validar o modelo

Calcule o SHA-256:

```bash
sha256sum best.pt
```

Valor esperado:

```text
fd7c021806c16d44fb26c76eaeb2a4ba469627a9bce12da198bc98acf7ea2ed2
```

Se o hash for diferente, atualize o repositório antes de continuar.

## 7. Criar o ambiente virtual

O Picamera2 é fornecido pelo Raspberry Pi OS. Por isso, crie o ambiente virtual com acesso aos pacotes do sistema:

```bash
python3 -m venv --system-site-packages .venv
```

Ative:

```bash
source .venv/bin/activate
```

O prompt deve passar a mostrar `(.venv)`.

Confirme:

```bash
which python
python --version
```

O caminho do Python deve apontar para:

```text
.../classificador_engrenagens/.venv/bin/python
```

## 8. Instalar os requirements

Com o `.venv` ativado:

```bash
python -m pip install --upgrade pip
python -m pip install -r requirements.txt
```

O arquivo atual instala as dependências Python do classificador, incluindo Ultralytics e Paho MQTT.

## 9. Validar os imports

Ainda com o ambiente ativado:

```bash
python -c "import cv2; import paho.mqtt.client; import ultralytics; from picamera2 import Picamera2; print('Dependencias OK'); print('Ultralytics', ultralytics.__version__)"
```

Resultado esperado inclui:

```text
Dependencias OK
Ultralytics 8.4.168
```

Não prossiga enquanto este teste falhar.

## 10. Primeiro teste — câmera + YOLO sem MQTT

Mantenha a esteira **vazia durante a inicialização**, pois o programa usa os primeiros 30 frames para calibrar o fundo.

Para testar via terminal/SSH, sem abrir janelas:

```bash
python gear_image_class_noScreen.py --no-mqtt
```

O programa deve:

1. localizar `best.pt`;
2. carregar o modelo YOLO;
3. iniciar a OV5647;
4. calibrar o fundo com 30 frames;
5. restaurar a contagem local da câmera;
6. ficar aguardando uma peça.

Use `Ctrl+C` para encerrar.

## 11. Testar a versão com visualização

Em uma sessão gráfica da Raspberry Pi, ainda com o `.venv` ativado:

```bash
python gear_image_class.py --no-mqtt
```

A janela principal deve apresentar:

- imagem ao vivo da câmera;
- ROI principal;
- região/linha de trigger;
- FPS;
- pixels pretos;
- baseline;
- delta;
- estado do detector;
- contagem da câmera;
- última classificação.

Ao passar uma engrenagem, deve abrir também a janela de resultado com a classificação.

Para encerrar a versão visual, pressione:

```text
Esc
```

ou:

```text
Q
```

Somente depois de validar câmera, modelo, calibração e classificação local continue para a integração MQTT.

## 12. Configurar MQTT

O script lê:

```text
PNAAT_MQTT_HOST
PNAAT_MQTT_PORT
PNAAT_MQTT_USERNAME
PNAAT_MQTT_PASSWORD
PNAAT_TOPIC_PREFIX
PNAAT_MODEL_PATH
```

Como Mosquitto roda na mesma Raspberry Pi:

```bash
export PNAAT_MQTT_HOST=127.0.0.1
export PNAAT_MQTT_PORT=1883
export PNAAT_MQTT_USERNAME=pnaat
export PNAAT_TOPIC_PREFIX=pnaat
export PNAAT_MODEL_PATH="$PWD/best.pt"
```

Digite a senha sem deixá-la gravada no histórico:

```bash
read -s -p "Senha MQTT: " PNAAT_MQTT_PASSWORD
echo
export PNAAT_MQTT_PASSWORD
```

## 13. Abrir um monitor MQTT

Em outro terminal da Raspberry Pi, substitua `SUA_SENHA_MQTT` pela senha definida para o usuário MQTT `pnaat` durante a instalação do Mosquitto:

```bash
cd ~/pnaat-handson/docker

docker exec -it pnaat-mosquitto \
  mosquitto_sub \
  -h localhost \
  -p 1883 \
  -u pnaat \
  -P SUA_SENHA_MQTT \
  -t 'pnaat/#' \
  -v
```

Deixe esse terminal aberto.

## 14. Executar o sistema completo

Volte ao terminal do classificador.

Se abriu um novo terminal, reative o ambiente:

```bash
cd ~/pnaat-handson/vision/classificador_engrenagens
source .venv/bin/activate
```

Reconfigure as variáveis MQTT caso elas não existam nesse novo shell.

Execute:

```bash
python gear_image_class_noScreen.py
```

Ao detectar uma peça, o programa:

1. captura a ROI;
2. classifica a imagem;
3. converte `OK` em `APROVADA`;
4. converte `NOK` em `REJEITADA`;
5. incrementa e persiste o contador da câmera;
6. publica a contagem;
7. publica o resultado da classificação.

## 15. Rearme entre peças

Depois de classificar uma peça, o programa entra em estado de espera até confirmar que a região de detecção ficou livre.

Ao iniciar, mantenha a esteira vazia. O programa mede 30 frames e calcula a mediana de pixels pretos da região de trigger. Esse valor vira o `baseline` do fundo.

A lógica atual usa a diferença absoluta em relação ao baseline, com histerese e confirmação por vários frames:

```text
CALIBRATION_FRAMES    = 30
TRIGGER_DELTA_PIXELS  = 10
REARM_DELTA_PIXELS    = 3
REARM_FRAMES          = 5
```

Exemplo: se a esteira vazia produzir cerca de 30 pixels pretos, o programa passa a considerar `baseline=30`. Uma leitura de 31 tem `delta=1`, não uma peça.

Funcionamento:

```text
calibração com esteira vazia
        ↓
baseline calculado
        ↓
delta > 10
        ↓
peça detectada e classificada
        ↓
aguarda saída da peça
        ↓
delta <= 3 por 5 frames consecutivos
        ↓
sistema rearmado
        ↓
próxima peça pode ser detectada
```

Depois de cada classificação deve aparecer:

```text
[CAM] aguardando saída da peça para rearme
```

E, após a região ficar livre:

```text
[CAM] sistema rearmado
```

A confirmação por 5 frames evita rearme falso por ruído ou variação momentânea da imagem. O limite de rearme não exige mais zero pixels pretos, porque pequenas sombras ou ruídos da câmera podem impedir que a contagem chegue exatamente a zero.

## 16. Tópicos publicados

Contagem:

```text
pnaat/contagem/camera
```

Exemplo:

```json
{"count":17,"source":"camera_yolo"}
```

Resultado:

```text
pnaat/visao/resultado
```

Exemplo:

```json
{
  "id_camera":17,
  "resultado":"APROVADA",
  "confianca":0.9342,
  "classe_modelo":"OK",
  "source":"camera_yolo",
  "timestamp_utc":"2026-10-16T16:00:00.000+00:00"
}
```

Mapeamento:

```text
OK  -> APROVADA
NOK -> REJEITADA
outra classe -> INCONCLUSIVA
```

O ESP32 utiliza `id_camera`, `resultado` e `confianca` para associar a classificação à passagem da peça pelo sensor final.

## 17. Diagnóstico do detector

Durante a execução, o classificador imprime uma linha de diagnóstico aproximadamente uma vez por segundo:

```text
[DET] estado=ARMADO pixels_pretos=30 baseline=30 delta=0 trigger_delta=>10 rearme_delta=<=3 frames_livres=0/5
```

Quando a região de contagem cruza o limiar de detecção, aparece imediatamente:

```text
[DET] trigger detectado: pixels_pretos=52 baseline=30 delta=22 > 10; iniciando inferência YOLO
[AI] inferência concluída em 85.4 ms
```

Interpretação:

- se o `delta` nunca ultrapassar `10` durante a passagem da peça, a peça não está alterando suficientemente a região de contagem; nesse caso, ajuste a ROI/posição física antes de alterar o limiar;
- se aparecer `trigger detectado` mas não aparecer `inferência concluída`, o problema está na execução do modelo;
- se a inferência terminar mas o sistema ficar em `AGUARDANDO_REARME`, observe `pixels_pretos` e `frames_livres`;
- o sistema volta a `ARMADO` depois de cinco frames consecutivos com até três pixels pretos.

## 18. Resultado esperado no monitor MQTT

Ao passar uma engrenagem:

```text
pnaat/contagem/camera ...
pnaat/visao/resultado ...
```

Depois, quando a mesma peça chegar ao sensor final do ESP32:

```text
pnaat/peca/resultado_final ...
```

## 19. Encerramento

Use:

```text
Ctrl+C
```

O script encerra a câmera e a conexão MQTT de forma controlada.

## 20. Checklist

- [ ] OV5647 reconhecida por `rpicam-hello --list-cameras`;
- [ ] `best.pt` presente na pasta;
- [ ] SHA-256 do modelo confere;
- [ ] `.venv` criado com `--system-site-packages`;
- [ ] `.venv` ativado;
- [ ] requirements instalados;
- [ ] imports validados;
- [ ] Ultralytics 8.4.168 confirmado;
- [ ] execução com `--no-mqtt` funciona;
- [ ] Mosquitto está em execução;
- [ ] variáveis MQTT configuradas;
- [ ] conexão MQTT confirmada;
- [ ] `pnaat/contagem/camera` publicado;
- [ ] `pnaat/visao/resultado` publicado;
- [ ] após retirar a peça aparece `[CAM] sistema rearmado`;
- [ ] pelo menos duas peças consecutivas são detectadas e classificadas;
- [ ] ESP32 recebe o resultado;
- [ ] associação visão/sensor é concluída;
- [ ] LEDs APROVADA/REJEITADA respondem conforme a classificação.
