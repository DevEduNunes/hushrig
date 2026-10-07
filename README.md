<div align="center">

# 🎸 HushRig

**Rig de guitarra open source para Windows — standalone e VST3.**
Toque direto no PC com baixa latência e sem chiado.

[![Build](https://github.com/DevEduNunes/hushrig/actions/workflows/build.yml/badge.svg)](https://github.com/DevEduNunes/hushrig/actions/workflows/build.yml)
![Plataforma](https://img.shields.io/badge/plataforma-Windows-0078D6)
![Formatos](https://img.shields.io/badge/formatos-Standalone%20%7C%20VST3-8A2BE2)
[![Licença](https://img.shields.io/badge/licen%C3%A7a-AGPL--3.0-green)](LICENSE)

<img src="docs/images/app-main.png" alt="Tela principal do HushRig" width="420">

</div>

---

## Por que o HushRig?

Guitarra ligada direto no PC costuma dar dois problemas: **chiado** (captadores single coil, ganho alto, ruído da USB) e **delay** (drivers de áudio padrão do Windows com buffers grandes).
O HushRig ataca os dois: um **noise gate de latência zero** feito para guitarra e um caminho de áudio pensado para rodar com **ASIO**.

> ⚠️ Projeto em desenvolvimento ativo (v0.2). Gate, pedais, presets e amp sim NAM já funcionam; a interface com pedais visuais está no [roadmap](#-roadmap).

## ✨ Recursos

- **Medidor de latência**: mostra em ms a latência real reportada pelo driver (entrada, saída, buffer, CPU) com dicas para reduzir
- **Medidores de nível** de entrada e saída, com marcador do threshold e indicador de gate aberto/fechado
- **Pedais** de overdrive, EQ, chorus, delay e reverb, com ordem ajustável (arraste) e **presets** (de fábrica e seus)
- **Amp sim** com modelos `.nam` do [Neural Amp Modeler](https://www.neuralampmodeler.com/): 0 ms de latência adicionada, controles de ganho, graves/médios/agudos e volume, normalização de volume entre modelos e medidor de CPU
- **Noise gate** com threshold, hold e release — sem lookahead, ou seja, **zero latência** adicionada
- **Gravação em WAV** (24 bits, áudio já processado) com um clique, salva em Documentos\HushRig
- **Tema escuro** (preto e roxo) com knobs rotativos
- **Ganho de entrada e de saída** com transição suave (sem cliques)
- **Standalone e VST3** com o mesmo código
- **Entrada mono** espelhada nos dois canais (guitarra em estéreo no fone)
- **Atualização dentro do app**: ao abrir, procura novas versões no GitHub; se houver, aparece um ícone de download na barra de título e um clique baixa e instala
- **Instalador completo** que já instala o driver ASIO [FlexASIO](https://github.com/dechamps/FlexASIO)

## 🖼️ Screenshots

<table>
  <tr>
    <td align="center"><img src="docs/images/app-main.png" alt="Tela principal" width="380"><br><sub>Tela principal</sub></td>
  </tr>
</table>

> As imagens são geradas automaticamente pelo CI a cada build (`tools/Screenshots.cpp`). Os valores de latência e de nível nelas são **de exemplo**.

## 📦 Instalação

1. Baixe o `HushRig-Setup-x.y.z.exe` na página de [Releases](https://github.com/DevEduNunes/hushrig/releases).
2. Execute o instalador. Você escolhe os componentes:
   - **HushRig** (aplicativo)
   - **Plugin VST3** (instalado em `Common Files\VST3`)
   - **FlexASIO** (driver ASIO de baixa latência)
3. Abra o HushRig.

> O Windows pode mostrar o aviso do SmartScreen ("editor desconhecido"), porque o instalador ainda não é assinado digitalmente. Clique em **Mais informações → Executar assim mesmo**.

## 🎚️ Como usar

1. Conecte a interface de áudio e a guitarra. **Use fones** para evitar microfonia.
2. Em **Options** (canto do app), escolha o driver **ASIO → FlexASIO**, a entrada e a saída. Desmarque *Mute audio input*.
3. Reduza o **buffer** (comece com 256 amostras e vá descendo até começar a estalar) para diminuir a latência.
4. Ajuste o **Gate Threshold** até o chiado sumir quando você não está tocando. Se o gate cortar o fim das notas, aumente o **Release**.

| Parâmetro | O que faz |
|---|---|
| **Input Gain** | Ganho aplicado antes do gate |
| **Gate Threshold** | Nível abaixo do qual o gate fecha |
| **Gate Hold** | Tempo que o gate segura aberto depois que o sinal cai |
| **Gate Release** | Quão rápido o gate fecha |
| **Output Gain** | Volume de saída |
| **Gate bypass** | Desliga o gate |

### Pedais e presets

Ligue cada pedal no botão do canto do bloco. **Arraste o título** de um bloco para mudar a ordem da cadeia. O seletor **Presets** traz alguns de fábrica e guarda os seus (**Salvar**/**Excluir**), em `Documentos\HushRig\Presets`. Um preset guarda os pedais, a ordem e o modelo do amp; ganhos e gate ficam como você deixou.

### Amp sim (modelos NAM)

1. No bloco **AMP (NAM)**, clique em **Carregar modelo...** e escolha um arquivo `.nam` (ou um `.wav` de IR). Há muitos modelos gratuitos feitos pela comunidade do [Neural Amp Modeler](https://www.neuralampmodeler.com/).
2. O bloco mostra quanto do tempo do buffer o modelo usa (**CPU**). Se ficar alto (laranja/vermelho) ou o som estalar, troque por um modelo mais leve ou aumente o buffer.
3. No card **AMP · GANHO E TOM** ajuste:

| Controle | O que faz |
|---|---|
| **Input** | Ganho antes do modelo (empurra o amp para mais ou menos distorção) |
| **Bass / Mid / Treble** | Tom, aplicado depois do modelo (±12 dB) |
| **Output** | Volume de saída do amp |
| **Normalizar volume** | Iguala o volume entre modelos usando o loudness gravado no `.nam` |

O amp **não adiciona latência**. Se o modelo foi treinado em uma taxa diferente da do dispositivo (por exemplo 48 kHz no modelo e 44,1 kHz no driver), o bloco avisa, porque o timbre muda: prefira rodar a 48 kHz.

### Dicas contra o chiado

O software ajuda, mas confira também a origem:
- Ganho do preamp da interface alto demais
- Captadores single coil (chegar perto do monitor aumenta o zumbido)
- Notebook ligado na fonte (teste na bateria) e outra porta USB
- Cabo da guitarra

## 🔄 Atualizações

No app standalone, clique em **Procurar atualizações**. Se houver uma versão nova, aparece o botão **Baixar e instalar**: o app baixa o instalador da release, confere o SHA-256 e reinicia já atualizado.

## 🗺️ Roadmap

- [x] Esqueleto JUCE (Standalone + VST3) e CI no GitHub Actions
- [x] Noise gate (threshold, hold, release) com testes
- [x] Instalador com FlexASIO e atualização dentro do app
- [x] Medidor de latência e medidores de nível
- [x] Pedais: overdrive, delay, reverb, chorus e EQ
- [x] Cadeia de pedais reordenável (arraste o título) e presets
- [x] Gravação em WAV
- [x] Amp sim com modelos [Neural Amp Modeler](https://www.neuralampmodeler.com/) (carregue um `.nam`; latência adicional zero)
- [ ] Interface própria com pedais visuais

## 🛠️ Compilando

O build oficial roda no GitHub Actions (`.github/workflows/build.yml`). Para compilar localmente você precisa do Visual Studio 2022 (carga de trabalho C++) e do CMake:

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

Os binários ficam em `build/HushRig_artefacts/Release/`.

```
src/dsp/        # DSP sem dependência do JUCE (testável)
src/update/     # verificação de versão, SHA-256 e atualizador
src/            # processador e editor do plugin
tests/          # testes (Catch2)
tools/          # gerador de screenshots do README e hushrig_ampbench (mede o custo de um modelo .nam)
installer/      # script do instalador (Inno Setup)
```

## 🤝 Contribuindo

Issues e pull requests são bem-vindos. Para lançar uma versão, crie uma tag `vX.Y.Z`: o CI compila e publica a release com o instalador.

### Medindo um modelo NAM

O `hushrig_ampbench` (compilado junto com o projeto) mostra quanto do tempo do buffer um modelo usa, em buffers de 64 a 512 amostras:

```bash
build/Release/hushrig_ampbench.exe meu-modelo.nam 48000
```

Abaixo de ~50% (p99) é seguro; acima disso prefira um modelo mais leve ou um buffer maior.

## 📄 Licença

[AGPL-3.0](LICENSE), compatível com a licença do [JUCE](https://juce.com).

O amp sim usa o [NeuralAmpModelerCore](https://github.com/sdatkinson/NeuralAmpModelerCore) (MIT), que inclui o [Eigen](https://eigen.tuxfamily.org) (MPL-2.0) e o [nlohmann/json](https://github.com/nlohmann/json) (MIT). Os modelos de teste em `tests/data/` vêm do repositório do NAM Core.
