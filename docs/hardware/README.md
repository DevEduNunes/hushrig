# 🎛️ HushRig Hardware — pedal com ESP32-S3

```
Guitarra ──► [buffer] ──► ADC ──► ESP32-S3 (gate + pedais) ──► DAC ──► [nível] ──► Amplificador
                                        ▲
                       Celular (WiFi) ──┘   página de controle servida pelo próprio pedal
```

O mesmo DSP do app (`src/dsp/`: gate, overdrive, EQ, chorus, delay, reverb) roda no ESP32 **sem cópia**: o firmware inclui os mesmos headers. Quem toca na guitarra ouve o noise gate de latência zero na frente do amplificador.

> **Status:** o núcleo (`firmware/src/Rig.h`) e a página de controle estão testados no PC (`tests/RigTests.cpp` e um teste de navegador). O **firmware ESP-IDF ainda não foi compilado nem rodado em hardware**: o ambiente onde foi escrito não alcança os servidores do PlatformIO. O workflow `Firmware` (GitHub Actions) compila a cada push; espere ajustes na primeira montagem.

## O que dá e o que não dá

| | |
|---|---|
| ✅ Gate, overdrive, EQ, chorus, delay, reverb | custo de CPU baixo: cabe folgado em um núcleo de 240 MHz |
| ✅ Latência total ~3 ms (ver abaixo) | |
| ✅ Controle pelo Android, sem app | WiFi próprio do pedal + página web (WebSocket) |
| ✅ 4 presets salvos na memória, footswitch, LED, bateria | |
| ⚠️ Amp sim NAM (`.nam`) | **Fora do escopo.** Modelos NAM padrão são pesados demais para o ESP32. Também não faz falta: o amplificador de verdade já é o amp e o falante. |
| ⚠️ Bluetooth | A v1 usa WiFi. BLE (Web Bluetooth) é viável e está no roadmap. |

## Lista de peças (~US$ 30–40, preços aproximados)

| Qtd | Peça | Para quê |
|---|---|---|
| 1 | **ESP32-S3-DevKitC-1 N16R8** (16 MB flash + 8 MB PSRAM) | cérebro. A PSRAM é necessária (buffer do delay = 384 KB). |
| 1 | Módulo ADC **PCM1808** (I2S, 24 bits) | entrada de áudio |
| 1 | Módulo DAC **PCM5102A** (I2S, 24 bits) | saída de áudio |
| 1 | **MCP6022** (ou outro ampop CMOS rail-to-rail de baixo ruído, ex. TLC2272) | buffer de alta impedância para a guitarra |
| 1 | Bateria **18650** + suporte | |
| 1 | Módulo carregador **TP4056 com proteção (DW01)**, USB-C | carrega a bateria |
| 1 | Conversor **boost 5 V** (MT3608 ou módulo 18650→5 V) | alimenta os conversores de áudio (PCM1808 precisa de 5 V analógico) |
| 2 | Jack **P10 (6,35 mm) mono** | entrada e saída |
| 1 | Footswitch momentâneo (pedal de pisar) + LED 3 mm + resistor 1 kΩ | liga/desliga |
| 1 | Potenciômetro **10 kΩ** log | volume de saída (hardware) |
| — | Resistores (2× 100 kΩ, 1 MΩ, 2× 10 kΩ, 1 kΩ), capacitores (1 µF filme, 2× 10 µF, 100 µF, 100 nF), chave liga/desliga, caixa metálica (blindagem), fio | |

## Ligações

### I2S (ESP32-S3 ↔ conversores) — pinos em `firmware/src/config.h`

| ESP32-S3 | PCM1808 (ADC) | PCM5102A (DAC) |
|---|---|---|
| GPIO4 (MCLK) | SCK / SCKI | — (SCK do DAC no **GND**: PLL interno) |
| GPIO5 (BCLK) | BCK | BCK |
| GPIO6 (WS) | LRCK | LRCK |
| GPIO7 (DOUT) | — | DIN |
| GPIO8 (DIN) | OUT | — |
| GND | GND | GND |

- **PCM1808:** pinos **MD0, MD1 e FMT em GND** → modo escravo, formato I2S. Alimentação **5 V** (analógico) e **3,3 V** (digital); a maioria dos módulos já trata isso, confira o seu.
- **PCM5102A:** nos pads do módulo: **FLT → H** (filtro de baixa latência), **DEMP → L**, **XSMT → H** (sem mute), **FMT → L** (I2S). Alimentação 3,3 V ou 5 V conforme o módulo.
- Guitarra em **L IN** do PCM1808; saída em **L OUT** do PCM5102A (o firmware duplica o mono nos dois canais).

### Controles e bateria

| ESP32-S3 | Liga em |
|---|---|
| GPIO9 | footswitch → GND (pull-up interno) |
| GPIO10 | LED + resistor 1 kΩ → GND |
| GPIO1 | divisor 100 kΩ/100 kΩ a partir do **+ da bateria** (lê a carga) |

### Estágio de entrada (obrigatório)

Captador de guitarra precisa ver ≥ 1 MΩ; o PCM1808 tem entrada de ~20 kΩ. Ligar direto **mata os agudos** e deixa o som “abafado”. Um seguidor de tensão resolve:

```
 5 V ──┬─[10k]──┬──[10k]── GND          Vref ≈ 2,5 V (filtrada com 10 µF)
       │        └── Vref ──[ 1 MΩ ]──┐
                                      │
 Jack P10 (tip) ──[1 µF filme]───┬────┴──► (+) MCP6022 ──┬──► [1 kΩ] ──[10 µF]──► PCM1808 L IN
        (sleeve → GND)           │                (−)───┘
                              [1 kΩ + 100 pF para GND: filtro de RF]
```

- O ampop é alimentado pelos **5 V** (filtrados: ferrite + 100 µF + 100 nF), não pelos 3,3 V.
- Se o seu módulo PCM1808 já tem capacitor de acoplamento na entrada, dispense o de 10 µF.

### Saída para o amplificador

O PCM5102A entrega ~2 Vrms (nível de linha), **muito mais que uma guitarra**. Ligue a saída num potenciômetro de 10 kΩ (extremos: sinal e GND; cursor → jack de saída) e **comece com ele no mínimo**. O firmware já sai com volume de −12 dB.

### Alimentação e ruído (a parte que mais importa)

```
USB-C ► TP4056 (carga+proteção) ► 18650 ► chave ► boost 5 V ─┬► 5V do ESP32-S3 DevKit
                                                             ├► PCM1808 (5 V) / PCM5102A
                                                             └► ampop (via ferrite + 100 µF)
                   + divisor 100k/100k  ► GPIO1
```

Conversores chaveados (boost) deixam **zumbido agudo** no áudio. Boas práticas: capacitores de 100 µF + 100 nF perto de cada módulo, ferrite no 5 V do ampop, **terra em estrela** (um ponto só, junto ao jack de entrada), fios de áudio curtos e longe da antena do ESP32, caixa metálica aterrada. Se ainda chiar, o próprio **noise gate do HushRig** ajuda, mas o ideal é resolver na origem. Enquanto carrega no USB, o carregador também injeta ruído: teste na bateria.

## Latência (estimativa)

| Etapa | Tempo |
|---|---|
| Filtro do ADC (PCM1808) | ~0,4 ms |
| Bloco de entrada (32 amostras @ 48 kHz) | 0,67 ms |
| Processamento (DSP) | < 0,3 ms (a página mostra “DSP %”) |
| Fila de saída (I2S DMA) | ~1,3 ms |
| Filtro do DAC (PCM5102A, baixa latência) | ~0,3 ms |
| **Total** | **~3 ms** |

Se ouvir estalos, a página mostra “falhas” no medidor DSP: aumente `kBlockFrames` para 64 em `config.h`.

## Autonomia

Estimativa: ESP32-S3 com WiFi (~120 mA @ 3,3 V) + conversores (~50 mA @ 5 V) ≈ 0,7 W. Com uma 18650 de 2500 mAh e boost de ~85%: **~8–10 h**. É uma conta de papel, meça a sua. Para economizar, um “modo palco” que desliga o WiFi depois de configurar está no roadmap.

## Gravando e usando

```bash
pip install platformio
cd firmware
pio run -t upload        # conecte a placa pela porta USB (a "UART")
pio device monitor
```

1. Antes de usar fora de casa, **troque a senha do WiFi** em `firmware/src/config.h`.
2. Ligue o pedal. No celular, conecte à rede **HushRig** (senha padrão `hushrig123`). O Android pode avisar “sem internet”: **mantenha a conexão**.
3. Abra **http://192.168.4.1** no navegador.
4. Ajuste gate, pedais e ordem; salve em um dos 4 slots (A–D). O footswitch liga/desliga o efeito (bypass digital, sem estalo).

<img src="../images/hardware-ui.png" alt="Página de controle (captura com dados simulados)" width="320">

*Página de controle, capturada com dados simulados.*

## Como o código está organizado

```
firmware/
  platformio.ini, sdkconfig.defaults, partitions.csv
  src/
    Rig.h        parâmetros + cadeia mono (sem ESP-IDF; testado em tests/RigTests.cpp)
    audio.cpp    I2S full-duplex e tarefa de áudio no núcleo 1
    net.cpp      WiFi AP, servidor HTTP e WebSocket (núcleo 0)
    storage.cpp  4 presets na NVS
    battery.cpp  leitura da bateria
    web/index.html  página de controle (sem internet, sem CDN)
```

Protocolo WebSocket (JSON): o celular manda `{"set":"odDrive","v":20}`, `{"on":false}`, `{"order":[…]}`, `{"save":1}`, `{"load":2}`, `{"reset":1}`; o pedal responde com o estado completo (valores + faixas) e, 10×/s, `{"m":[entrada, saída, gate, bateria %, carga DSP, falhas]}`. Os ids dos parâmetros são os mesmos do plugin.

## Roadmap

- [ ] Primeira montagem e ajuste do firmware em hardware real
- [ ] Controle por BLE (Web Bluetooth), sem trocar de WiFi
- [ ] Modo palco: desliga o WiFi sem clientes; segurar o footswitch reativa
- [ ] Relé para *true bypass* analógico (hoje o bypass é digital, passa pelos conversores)
- [ ] Afinador
- [ ] PCB própria
