"""Fonte única da verdade do HushRig Carrier v1: peças, ligações e posições.

O esquemático (gen_sch.py) e a PCB (gen_pcb.py) são gerados a partir daqui, e verify.py confere que os dois
resultam na mesma netlist.

A placa é uma "carrier": recebe um ESP32-DevKitC de 38 pinos e leva os sinais I2S aos módulos PCM1808 e
PCM5102A por cabinhos (cada módulo tem um pinout diferente, então a placa expõe um cabeçalho rotulado por
módulo). O estágio de entrada (buffer de alta impedância), a alimentação filtrada e os conectores de jacks,
footswitch, LED, potenciômetro e bateria ficam na placa.
"""

from dataclasses import dataclass, field

# Bibliotecas padrão do KiCad (footprints)
FP_R = "Resistor_THT:R_Axial_DIN0207_L6.3mm_D2.5mm_P7.62mm_Horizontal"
FP_C_DISC = "Capacitor_THT:C_Disc_D5.0mm_W2.5mm_P5.00mm"
FP_C_FILM = "Capacitor_THT:C_Rect_L7.2mm_W3.0mm_P5.00mm_FKS2_FKP2_MKS2_MKP2"
FP_CP5 = "Capacitor_THT:CP_Radial_D5.0mm_P2.00mm"
FP_CP63 = "Capacitor_THT:CP_Radial_D6.3mm_P2.50mm"
FP_DIP8 = "Package_DIP:DIP-8_W7.62mm_Socket"
FP_MH = "MountingHole:MountingHole_3.2mm_M3"


def fp_header(n):
    return f"Connector_PinHeader_2.54mm:PinHeader_1x{n:02d}_P2.54mm_Vertical"


def fp_socket(n):
    return f"Connector_PinSocket_2.54mm:PinSocket_1x{n:02d}_P2.54mm_Vertical"


@dataclass
class Part:
    ref: str
    lib: str            # símbolo do KiCad, ex. "Device:R"
    value: str
    footprint: str
    pins: dict          # número do pino -> rede ("" = sem ligação)
    note: str = ""      # texto extra para o esquemático (rótulo do bloco)


# ESP32-DevKitC (38 pinos), USB para baixo. J1 = lado esquerdo, J2 = direito, de cima para baixo.
ESP_LEFT = ["3V3", "EN", "SVP/IO36", "SVN/IO39", "IO34", "IO35", "IO32", "IO33", "IO25", "IO26",
            "IO27", "IO14", "IO12", "GND", "IO13", "SD2", "SD3", "CMD", "5V"]
ESP_RIGHT = ["GND", "IO23", "IO22", "TXD0", "RXD0", "IO21", "GND", "IO19", "IO18", "IO5",
             "IO17", "IO16", "IO4", "IO0", "IO2", "IO15", "SD1", "SD0", "CLK"]

j1 = {i + 1: "" for i in range(19)}
j2 = {i + 1: "" for i in range(19)}
j1[1] = "+3V3"
j1[19] = "+5V"
j1[14] = "GND"
j1[5] = "ADC_BAT"      # IO34
j1[7] = "SW_IN"        # IO32 (footswitch)
j1[8] = "I2S_DIN"      # IO33 <- PCM1808 OUT
j1[9] = "I2S_DOUT"     # IO25 -> PCM5102A DIN
j1[10] = "LRCK_ESP"    # IO26
j1[11] = "BCK_ESP"     # IO27
j2[1] = "GND"
j2[7] = "GND"
j2[14] = "MCLK_ESP"    # IO0
j2[15] = "LED_DRV"     # IO2

PARTS = [
    Part("J1", "Connector_Generic:Conn_01x19", "ESP32 DevKitC (esquerda)", fp_socket(19), j1,
         "ESP32-DevKitC 38 pinos, USB para baixo. Esquerda, de cima: 3V3 EN IO36 IO39 IO34 IO35 IO32 IO33 IO25 IO26 IO27 IO14 IO12 GND IO13 SD2 SD3 CMD 5V"),
    Part("J2", "Connector_Generic:Conn_01x19", "ESP32 DevKitC (direita)", fp_socket(19), j2,
         "Direita, de cima: GND IO23 IO22 TXD0 RXD0 IO21 GND IO19 IO18 IO5 IO17 IO16 IO4 IO0 IO2 IO15 SD1 SD0 CLK"),

    # --- cabeçalhos dos módulos (cabinhos Dupont) ---
    Part("J3", "Connector_Generic:Conn_01x09", "ADC PCM1808", fp_header(9),
         {1: "+5V", 2: "+3V3", 3: "GND", 4: "MCLK", 5: "BCK", 6: "LRCK", 7: "I2S_DIN", 8: "AIN", 9: "GND"},
         "Módulo ADC PCM1808: 5V, 3V3, GND, SCK(MCLK), BCK, LRCK, OUT, entrada L, GND da entrada"),
    Part("J4", "Connector_Generic:Conn_01x08", "DAC PCM5102A", fp_header(8),
         {1: "+5V", 2: "+3V3", 3: "GND", 4: "BCK", 5: "LRCK", 6: "I2S_DOUT", 7: "DAC_OUT", 8: "GND"},
         "Módulo DAC PCM5102A: VIN (5V ou 3V3, conforme o módulo), GND, BCK, LRCK, DIN, saída L, GND. SCK do módulo no GND."),

    # --- conectores para fora da placa ---
    Part("J5", "Connector_Generic:Conn_01x02", "ENTRADA (jack guitarra)", fp_header(2), {1: "IN_TIP", 2: "GND"},
         "Jack de entrada P10: 1 = ponta, 2 = luva"),
    Part("J6", "Connector_Generic:Conn_01x02", "SAIDA (jack amplificador)", fp_header(2), {1: "OUT_TIP", 2: "GND"},
         "Jack de saída P10: 1 = ponta, 2 = luva"),
    Part("J7", "Connector_Generic:Conn_01x03", "VOLUME (pot 10k)", fp_header(3), {1: "DAC_OUT", 2: "POT_WIPER", 3: "GND"},
         "Potenciômetro 10k log: 1 = entrada (DAC), 2 = cursor, 3 = GND"),
    Part("J8", "Connector_Generic:Conn_01x02", "FOOTSWITCH", fp_header(2), {1: "SW_IN", 2: "GND"},
         "Footswitch momentâneo para GND"),
    Part("J9", "Connector_Generic:Conn_01x02", "LED", fp_header(2), {1: "LED_A", 2: "GND"},
         "LED externo: 1 = anodo, 2 = catodo"),
    Part("J10", "Connector_Generic:Conn_01x02", "ALIM 5V", fp_header(2), {1: "+5V", 2: "GND"},
         "Entrada 5V externa (alternativa ao USB do DevKit; nunca os dois ao mesmo tempo)"),
    Part("J11", "Connector_Generic:Conn_01x02", "BATERIA (leitura)", fp_header(2), {1: "BAT_P", 2: "GND"},
         "Mede a bateria: + da célula e GND (opcional)"),

    # --- buffer de entrada ---
    Part("U1", "Amplifier_Operational:LM358", "MCP6022-I/P", FP_DIP8,
         {1: "BUF_OUT", 2: "BUF_OUT", 3: "IN_BUF", 4: "GND", 5: "VREF", 6: "BUFB", 7: "BUFB", 8: "5VA"},
         "Ampop dual rail-to-rail (MCP6022 ou TLC2272). A = buffer da guitarra; B = seguidor da referência (não pode ficar solto)"),
    Part("C1", "Device:C", "1u filme", FP_C_FILM, {1: "IN_TIP", 2: "IN_AC"}, "Acoplamento da guitarra"),
    Part("R1", "Device:R", "1M", FP_R, {1: "IN_AC", 2: "VREF"}, "Polarização: impedância de entrada 1 MOhm"),
    Part("R4", "Device:R", "1k", FP_R, {1: "IN_AC", 2: "IN_BUF"}, "Resistor de proteção de entrada"),
    Part("C2", "Device:C", "100p", FP_C_DISC, {1: "IN_BUF", 2: "GND"}, "Filtro de RF"),
    Part("R5", "Device:R", "1k", FP_R, {1: "BUF_OUT", 2: "AIN_DC"}, "Saída do buffer"),
    Part("C3", "Device:C_Polarized", "10u", FP_CP5, {1: "AIN_DC", 2: "AIN"}, "Acoplamento para o PCM1808 (dispense se o módulo já tiver)"),

    # --- referência 2,5 V e alimentação analógica ---
    Part("R2", "Device:R", "10k", FP_R, {1: "5VA", 2: "VREF"}, "Divisor da referência"),
    Part("R3", "Device:R", "10k", FP_R, {1: "VREF", 2: "GND"}, "Divisor da referência"),
    Part("C4", "Device:C_Polarized", "10u", FP_CP5, {1: "VREF", 2: "GND"}, "Filtro da referência"),
    Part("C7", "Device:C", "100n", FP_C_DISC, {1: "VREF", 2: "GND"}, ""),
    Part("R6", "Device:R", "22", FP_R, {1: "+5V", 2: "5VA"}, "Filtro RC do 5V analógico (22R + 100uF)"),
    Part("C5", "Device:C_Polarized", "100u", FP_CP63, {1: "5VA", 2: "GND"}, ""),
    Part("C6", "Device:C", "100n", FP_C_DISC, {1: "5VA", 2: "GND"}, "Desacoplamento junto ao ampop"),

    # --- barramento 5V / 3V3 ---
    Part("C8", "Device:C_Polarized", "100u", FP_CP63, {1: "+5V", 2: "GND"}, "Reservatório do 5V"),
    Part("C9", "Device:C", "100n", FP_C_DISC, {1: "+3V3", 2: "GND"}, ""),
    Part("C10", "Device:C", "100n", FP_C_DISC, {1: "+5V", 2: "GND"}, ""),
    Part("C11", "Device:C_Polarized", "10u", FP_CP5, {1: "+5V", 2: "GND"}, ""),
    Part("C12", "Device:C_Polarized", "10u", FP_CP5, {1: "+3V3", 2: "GND"}, ""),

    # --- clocks I2S com resistor em série (fios longos) ---
    Part("R7", "Device:R", "47", FP_R, {1: "MCLK_ESP", 2: "MCLK"}, "Resistores série nos clocks"),
    Part("R8", "Device:R", "47", FP_R, {1: "BCK_ESP", 2: "BCK"}, ""),
    Part("R9", "Device:R", "47", FP_R, {1: "LRCK_ESP", 2: "LRCK"}, ""),

    # --- saída ---
    Part("R13", "Device:R", "1k", FP_R, {1: "POT_WIPER", 2: "OUT_TIP"}, "Série na saída para o amplificador"),

    # --- controles ---
    Part("C13", "Device:C", "100n", FP_C_DISC, {1: "SW_IN", 2: "GND"}, "Debounce do footswitch"),
    Part("R10", "Device:R", "1k", FP_R, {1: "LED_DRV", 2: "LED_A"}, "Resistor do LED"),

    # --- bateria ---
    Part("R11", "Device:R", "100k", FP_R, {1: "BAT_P", 2: "ADC_BAT"}, "Divisor da bateria (÷2)"),
    Part("R12", "Device:R", "100k", FP_R, {1: "ADC_BAT", 2: "GND"}, ""),
    Part("C14", "Device:C", "100n", FP_C_DISC, {1: "ADC_BAT", 2: "GND"}, ""),
]


def nets():
    """rede -> lista ordenada de (ref, pino), só redes com 2+ pinos."""
    out = {}
    for p in PARTS:
        for pin, net in p.pins.items():
            if net:
                out.setdefault(net, []).append((p.ref, str(pin)))
    return {n: sorted(v) for n, v in out.items()}


if __name__ == "__main__":
    for n, pins in sorted(nets().items()):
        print(f"{n:10s} {len(pins):2d}  {' '.join(f'{r}.{p}' for r, p in pins)}")
    print(len(PARTS), "peças,", len(nets()), "redes")
