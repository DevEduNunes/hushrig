# HushRig Carrier v1 — esquemático e PCB (KiCad 7)

Placa carrier para o pedal HushRig: recebe um **ESP32-DevKitC de 38 pinos** em soquetes, tem o **buffer de entrada** para a guitarra, a alimentação analógica filtrada e conectores para jacks, footswitch, LED, potenciômetro e bateria. Os módulos **PCM1808** (ADC) e **PCM5102A** (DAC) ligam por cabinhos (cada fabricante usa um pinout diferente). Detalhes, cabeamento e avisos: [docs/hardware/README.md](../docs/hardware/README.md).

```
kicad/      projeto do KiCad (hushrig_carrier.kicad_pro), esquemático (.kicad_sch + PDF) e PCB (.kicad_pcb)
gerbers/    gerbers + furação + zip pronto para a fábrica
bom.csv     lista de materiais agrupada
tools/      geradores e verificações (tudo sai de tools/netlist.py)
```

## Regenerar e conferir

```bash
sudo apt install kicad                  # KiCad 7 (kicad-cli e pcbnew em Python)
pip install kiutils shapely numpy cairosvg
cd hardware/tools
python3 gen_sch.py && python3 verify_sch.py     # esquemático; netlist do KiCad == netlist.py
python3 gen_pcb.py && python3 verify_pcb.py     # posiciona, roteia, preenche o GND; DRC e conectividade
./export.sh                                     # gerbers, furação, zip e bom.csv
```

`netlist.py` é a fonte única: peças, ligações e footprints. Para mover uma peça, edite `PLACE` em `gen_pcb.py`; para mudar uma ligação, edite `netlist.py` e rode tudo de novo.

## O que foi (e não foi) verificado

- ✅ Netlist do esquemático idêntica à de `netlist.py` (28 redes).
- ✅ DRC próprio (`verify_pcb.py`): folga cobre-cobre ≥ 0,2 mm, trilha ≥ 0,2 mm, anel de via ≥ 0,15 mm, folga entre furos ≥ 0,25 mm, borda ≥ 0,3 mm, todas as redes conectadas (inclui o preenchimento do GND).
- ⚠️ **ERC e DRC do KiCad não foram rodados** (o KiCad 7 só os oferece na interface gráfica; na linha de comando só no 8). Rode os dois antes de fabricar.
- ⚠️ **Nenhuma placa foi montada.** O pinout dos 38 pinos do DevKit segue o DevKitC/NodeMCU-32S; confira na sua placa.
