# Aquisição de imagens para treinamento

Este diretório preserva o aquisitor de imagens recebido para o workshop. Ele é usado para montar conjuntos supervisionados de imagens `OK` e `NOK` antes do treinamento/classificação.

## Preparação

```bash
sudo apt update
sudo apt install -y \
  python3-picamera2 \
  python3-venv \
  python3-opencv

cd ~/pnaat-handson/vision/aquisicao_imagens

python3 -m venv --system-site-packages .venv
source .venv/bin/activate

python -m pip install --upgrade pip
pip install -r requirements.txt
```

## Teste da câmera

```bash
python camera_test.py
```

Resultado esperado:

```text
Iniciando camera...
Imagem capturada com sucesso.
```

## Executar o aquisitor

O programa possui interface OpenCV. Execute-o com monitor local ou sessão gráfica remota:

```bash
python gear_image_aq.py
```

Após detectar a peça:

- pressione `O` para salvar em `OK/`;
- pressione `N` para salvar em `NOK/`;
- pressione `Esc` para encerrar.

As pastas `OK/` e `NOK/` são dados de treinamento locais e não devem ser confundidas com o classificador operacional.
