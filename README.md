# HushRig

Rig de guitarra open source para Windows: **standalone + VST3**, com foco em baixa latência e em tirar o chiado da guitarra ligada direto no PC.

> Status: v0.1 (esqueleto). Já tem ganho de entrada/saída, **noise gate de latência zero** e testes de DSP.

## Roadmap

- [x] Esqueleto JUCE (Standalone + VST3), CI no GitHub Actions
- [x] Noise gate (threshold, hold, release) com testes
- [ ] Medidor de latência e guia de configuração ASIO (FlexASIO / ASIO4ALL)
- [ ] Pedais: overdrive, delay, reverb, chorus, EQ
- [ ] Cadeia de pedais reordenável + presets
- [ ] Gravação em WAV
- [ ] Amp sim (modelos Neural Amp Modeler)
- [ ] GUI própria

## Build

O build roda no GitHub Actions (`.github/workflows/build.yml`); os artefatos
`HushRig-Standalone-Windows` e `HushRig-VST3-Windows` ficam disponíveis em cada execução.

Para compilar localmente (Visual Studio 2022 com C++ e CMake):

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

## Dicas de latência no Windows

O driver padrão do Windows (MME/DirectSound) tem buffers grandes. Use **ASIO** (ou WASAPI exclusivo).
Se a interface não tem driver ASIO próprio, instale o [FlexASIO](https://github.com/dechamps/FlexASIO) ou o ASIO4ALL.

## Licença

[AGPL-3.0](LICENSE), compatível com a licença do [JUCE](https://juce.com).
