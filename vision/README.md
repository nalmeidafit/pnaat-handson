# Módulo de visão

Diretório destinado à inspeção de engrenagens pela Raspberry Pi 5.

## Arquivos

- `camera_test.py`: captura uma imagem 1296 × 972 com Picamera2.
- `gear_inspection_v0_3.py`: versão diagnóstica atual da inspeção geométrica.

## Execução

```bash
cd ~/pnaat-handson/vision
source .venv/bin/activate

python camera_test.py
python gear_inspection_v0_3.py teste_opencv.jpg
```

A inspeção v0.3 gera:

```text
diagnostico_v03/
├── mask.png
├── contorno.jpg
├── metricas.json
└── perfil_radial.csv
```

Esses arquivos são locais e estão ignorados pelo Git por padrão.

## Estado atual

A detecção externa é baseada em segmentação HSV da engrenagem azul e ROI. O sistema ainda não classifica peças automaticamente.

O foco imediato é tornar a estação S2 repetível:

- câmera perpendicular;
- altura fixa;
- suporte rígido;
- iluminação difusa;
- fundo uniforme/fosco;
- posição da peça repetível.

Depois disso serão calibrados os critérios OK/NOK.
