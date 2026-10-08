# 🎛️ HushRig Hardware — pedal com ESP32

```
Guitarra ──► [buffer] ──► ADC ──► ESP32 (gate + pedais) ──► DAC ──► [nível] ──► Amplificador
                                      ▲
                     Celular (Bluetooth LE) ──┘   página de controle no navegador do Android
```

O mesmo DSP do app (`src/dsp/`: gate, overdrive, EQ, chorus, delay, reverb) roda no ESP32 **sem cópia**: o firmware inclui os mesmos headers. Quem toca na guitarra ouve o noise gate de latência zero na frente do amplificador.

Dois alvos, o mesmo código:

| Alvo | Placa | Controle | Para quê |
|---|---|---|---|
| **`esp32`** | ESP32 comum (DevKit V1, ~US$ 3) | **Bluetooth LE** | o **barato e compacto** (recomendado) |
| `esp32s3` | ESP32-S3 | BLE **e** WiFi (página servida pelo próprio pedal) | quem quer controlar sem depender de internet/HTTPS |

> **Status:** o núcleo (`firmware/src/Rig.h`) e a página de controle (inclusive o caminho BLE, com um GATT simulado) estão testados no PC. O **firmware ESP-IDF ainda não foi compilado nem rodou em hardware**: o ambiente onde foi escrito não alcança os servidores do PlatformIO. O workflow `Firmware` (GitHub Actions) compila os dois alvos a cada push; espere ajustes na primeira montagem.

## O que dá e o que não dá

| | |
|---|---|
| ✅ Gate, overdrive, EQ, chorus, delay, reverb | custo de CPU baixo: cabe folgado em um núcleo de 240 MHz |
| ✅ Latência total ~3 ms (ver abaixo) | |
| ✅ Controle pelo Android, sem instalar app | página web + Bluetooth LE (Web Bluetooth) |
| ✅ 4 presets salvos na memória, footswitch, LED, bateria | |
| ⚠️ Amp sim NAM (`.nam`) | **Fora do escopo.** Modelos NAM padrão são pesados demais para o ESP32. Também não faz falta: o amplificador de verdade já é o amp e o falante. |
| ⚠️ iPhone | Safari não tem Web Bluetooth; use um navegador como o Bluefy. No Android, Chrome/Edge. |
| ⚠️ Delay | até **700 ms** (para caber na RAM interna, sem PSRAM). O app de PC vai até 2 s. |

## Lista de peças — versão mais barata (~US$ 14, preços aproximados)

| Qtd | Peça | ~US$ | Para quê |
|---|---|---|---|
| 1 | **ESP32 DevKit V1** (WROOM-32, 30 pinos) | 3,5 | cérebro. Serve qualquer ESP32 clássico que exponha os GPIO 0, 2, 25, 26, 27, 32, 33 e 34. |
| 1 | Módulo ADC **PCM1808** (I2S, 24 bits) | 2,5 | entrada de áudio |
| 1 | Módulo DAC **PCM5102A** (I2S, 24 bits) | 2,5 | saída de áudio |
| 1 | **MCP6022** (ou ampop CMOS rail-to-rail de baixo ruído, ex. TLC2272) | 1 | buffer de alta impedância para a guitarra |
| 2 | Jack **P10 (6,35 mm) mono** | 1,5 | entrada e saída |
| 1 | Footswitch momentâneo (pedal de pisar) | 1 | liga/desliga |
| 1 | Potenciômetro **10 kΩ** log | 0,5 | volume de saída (hardware) |
| — | Resistores (1 MΩ, 2× 10 kΩ, 2× 1 kΩ), capacitores (1 µF filme, 2× 10 µF, 100 µF, 100 nF, 100 pF), placa perfurada | 1,5 | |

**Alimentação mais barata e compacta: um power bank USB (5 V) no próprio USB do ESP32.** Zero peças extras. O PCM1808 já pega os 5 V do pino 5V/VIN da placa.

**Quer bateria dentro da caixa?** +~US$ 3: célula **18650** + módulo **TP4056 com proteção** (USB-C) + boost **MT3608** ajustado em 5 V + chave, e um divisor 100 kΩ/100 kΩ do + da bateria até o GPIO34 (a página mostra a carga). Sem bateria, a página mostra “🔌 USB”.

### Compacto

- Tudo cabe em **uma placa perfurada de ~5 × 7 cm**: o DevKit (52 × 28 mm) e os dois módulos (~30 × 15 mm) lado a lado, o ampop num soquete.
- Caixa de alumínio **1590B** (112 × 60 mm) comporta tudo com o footswitch em cima e os jacks nas laterais; a **1590A** (92 × 38 mm) só se você tirar o DevKit da placa e soldar um módulo ESP32 direto (o resto é igual).
- Variante ainda menor/barata: **ESP32-S3 Super Mini** ou módulos ESP32 sem placa de desenvolvimento, desde que o pinout seja ajustado em `config.h`.

## Ligações

### I2S (ESP32 ↔ conversores) — pinos em `firmware/src/config.h`

| Sinal | **ESP32** (alvo `esp32`) | ESP32-S3 | PCM1808 (ADC) | PCM5102A (DAC) |
|---|---|---|---|---|
| MCLK | **GPIO0** ¹ | GPIO4 | SCK / SCKI | — (SCK do DAC no **GND**: PLL interno) |
| BCLK | GPIO27 | GPIO5 | BCK | BCK |
| WS | GPIO26 | GPIO6 | LRCK | LRCK |
| DOUT | GPIO25 | GPIO7 | — | DIN |
| DIN | GPIO33 | GPIO8 | OUT | — |
| GND | GND | GND | GND | GND |

¹ No ESP32 clássico o MCLK do I2S só sai nos GPIO0, 1 ou 3. O GPIO0 é o pino de boot: não ligue nada que o puxe para GND na hora de ligar. Para gravar, o USB faz o reset automaticamente; em algumas placas é preciso segurar BOOT.

- **PCM1808:** pinos **MD0, MD1 e FMT em GND** → modo escravo, formato I2S. Alimentação **5 V** (analógico) e **3,3 V** (digital); a maioria dos módulos já trata isso, confira o seu.
- **PCM5102A:** nos pads do módulo: **FLT → H** (filtro de baixa latência), **DEMP → L**, **XSMT → H** (sem mute), **FMT → L** (I2S). Alimentação 3,3 V ou 5 V conforme o módulo.
- Guitarra em **L IN** do PCM1808; saída em **L OUT** do PCM5102A (o firmware duplica o mono nos dois canais).

### Controles e bateria

| ESP32 / ESP32-S3 | Liga em |
|---|---|
| GPIO32 / GPIO9 | footswitch → GND (pull-up interno) |
| GPIO2 / GPIO10 | LED + resistor 1 kΩ → GND (no ESP32 o GPIO2 já é o LED azul da placa) |
| GPIO34 / GPIO1 | divisor 100 kΩ/100 kΩ a partir do **+ da bateria** (só se houver bateria) |

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
Power bank USB ► USB do ESP32 ─┬► 3,3 V (regulador da placa) ► lógica
                               └► 5 V ─┬► PCM1808 / PCM5102A
                                       └► ampop (via ferrite + 100 µF)
(opcional, com bateria)  18650 ► TP4056 ► chave ► boost 5 V ► mesmo ponto de 5 V
```

Power banks e conversores chaveados (boost) deixam **zumbido agudo** no áudio. Boas práticas: capacitores de 100 µF + 100 nF perto de cada módulo, ferrite no 5 V do ampop, **terra em estrela** (um ponto só, junto ao jack de entrada), fios de áudio curtos e longe da antena do ESP32, caixa metálica aterrada. Se ainda chiar, o próprio **noise gate do HushRig** ajuda, mas o ideal é resolver na origem. Enquanto carrega no USB, o carregador também injeta ruído: teste na bateria.

## Latência (estimativa)

| Etapa | Tempo |
|---|---|
| Filtro do ADC (PCM1808) | ~0,4 ms |
| Bloco de entrada (32 amostras @ 48 kHz) | 0,67 ms |
| Processamento (DSP) | < 0,3 ms estimado (a página mostra “DSP %”) |
| Fila de saída (I2S DMA) | ~1,3 ms |
| Filtro do DAC (PCM5102A, baixa latência) | ~0,3 ms |
| **Total** | **~3 ms** |

Se ouvir estalos, a página mostra “falhas” no medidor DSP: aumente `kBlockFrames` para 64 em `config.h`.

## Autonomia

Com BLE e sem WiFi, o ESP32 gasta pouco: ~80 mA @ 3,3 V + conversores ~50 mA @ 5 V ≈ 0,5 W. Um power bank de 10 000 mAh dura **dias**; uma 18650 de 2500 mAh com boost de ~85%, **~12–15 h**. São contas de papel: meça a sua.

## Gravando e usando

```bash
pip install platformio
cd firmware
pio run -e esp32 -t upload        # ESP32 comum (BLE). Para o S3: -e esp32s3
pio device monitor
```

### Controle por Bluetooth (alvo `esp32`)

Web Bluetooth só funciona em **HTTPS**, e o ESP32 não serve HTTPS. Por isso a página é publicada no **GitHub Pages** pelo workflow `Página de controle`:

1. **Uma vez só:** no GitHub, *Settings → Pages → Build and deployment → Source: GitHub Actions*. Depois rode o workflow (ou dê push em `firmware/src/web`).
2. No Android, abra a página no **Chrome** (`https://<seu-usuario>.github.io/hushrig/`) e toque em **Conectar via Bluetooth**; escolha **HushRig**. Dica: *Adicionar à tela inicial* para virar um atalho.
3. Ajuste gate, pedais e ordem; salve em um dos 4 slots (A–D). O footswitch liga/desliga o efeito (bypass digital, sem estalo).

Se o celular perder a conexão, a página reconecta sozinha. **Só um celular por vez**, e qualquer um por perto pode conectar enquanto o pedal estiver livre (não há pareamento com senha).

### Controle por WiFi (alvo `esp32s3`)

Troque a senha em `firmware/src/config.h`, ligue o pedal, conecte o celular à rede **HushRig** (senha padrão `hushrig123`; se o Android avisar “sem internet”, mantenha a conexão) e abra **http://192.168.4.1**. Funciona sem internet. O BLE também fica ativo.

<img src="../images/hardware-ui.png" alt="Página de controle (captura com dados simulados)" width="320">

*Página de controle, capturada com dados simulados.*

## Como o código está organizado

```
firmware/
  platformio.ini, sdkconfig.defaults[.esp32|.esp32s3], partitions.csv
  tools/check_web_params.py   confere página x Rig.h (roda no CI)
  src/
    Rig.h        parâmetros + cadeia mono (sem ESP-IDF; testado em tests/RigTests.cpp)
    audio.cpp    I2S full-duplex e tarefa de áudio no núcleo 1
    protocol.cpp mensagens JSON (iguais em BLE e WiFi)
    ble.cpp      NimBLE: serviço GATT com RX (escrita) e TX (notificação), fatiando em pedaços do MTU
    net.cpp      WiFi AP, servidor HTTP e WebSocket (só no esp32s3)
    storage.cpp  4 presets na NVS
    battery.cpp  leitura da bateria
    web/index.html  página de controle (sem CDN); vai ao GitHub Pages e também é embutida no firmware
```

Protocolo (JSON, uma mensagem por linha no BLE; um frame por mensagem no WebSocket): o celular manda `{"set":"odDrive","v":20}`, `{"on":false}`, `{"order":[…]}`, `{"save":1}`, `{"load":2}`, `{"reset":1}`; o pedal responde com o estado (valores **por posição**, na ordem de `Rig.h`) e, 10×/s, `{"m":[entrada, saída, gate, bateria %, carga DSP, falhas]}`. UUIDs do BLE: serviço `a7c9e0f1-0b3d-4e5a-9c1f-4d6b2e8a1001`, RX `…1002`, TX `…1003`.

## Roadmap

- [ ] Primeira montagem e ajuste do firmware em hardware real
- [x] Controle por BLE (Web Bluetooth) — falta validar com um pedal de verdade
- [ ] Relé para *true bypass* analógico (hoje o bypass é digital, passa pelos conversores)
- [ ] Afinador
- [ ] PCB própria (ESP32 + codec numa placa só)
