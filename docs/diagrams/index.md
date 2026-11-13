# Figures

<!-- Written by kokeb-docs figures from docs/diagrams/figures/*.py. Do not edit by hand. -->

Every figure on this site is drawn by a Python module from facts extracted from the code, so it is rebuilt,
not redrawn, when the code changes. CI runs `kokeb-docs figures --check` and fails when a
committed figure no longer matches the code. Each figure below lists the files it is drawn from.

## Inside the ISF15ACP4

<!-- figure: chip-blocks -->
<picture>
  <source media="(prefers-color-scheme: dark)" srcset="chip-blocks.dark.svg">
  <img src="chip-blocks.svg"
    alt="Block diagram of the ISF15ACP4: its function blocks, how they connect and which package pins reach
    each block.">
</picture>
<!-- /figure -->

Hardware description: [`docs/hardware.yml`](../hardware.yml)

Drawn from:

- [`docs/hardware.yml`](../hardware.yml)
- [`inc/isf15acp4.hpp`](../../inc/isf15acp4.hpp)
- [`inc/isf15acp4_spi_interface.hpp`](../../inc/isf15acp4_spi_interface.hpp)

## ISF15ACP4: Typical application

<!-- figure: chip-circuit-typical -->
<picture>
  <source media="(prefers-color-scheme: dark)" srcset="chip-circuit-typical.dark.svg">
  <img src="chip-circuit-typical.svg"
    alt="Schematic of the ISF15ACP4 typical application: the chip with U2 MCU, J1 OLED supply, R1 resistor 10
    kΩ, R2 resistor 100 kΩ; the MCU wires carry the port hooks GpioSet(CtrlPin::Reset),
    GpioSet(CtrlPin::VccEnable), ReadSwitch(), Write(), Write(DataKind).">
</picture>
<!-- /figure -->

Hardware description: [`docs/hardware.yml`](../hardware.yml)

Drawn from:

- [`docs/hardware.yml`](../hardware.yml)
- [`inc/isf15acp4.hpp`](../../inc/isf15acp4.hpp)
- [`inc/isf15acp4_spi_interface.hpp`](../../inc/isf15acp4_spi_interface.hpp)

## ISF15ACP4 package, Module

<!-- figure: chip-outline -->
<picture>
  <source media="(prefers-color-scheme: dark)" srcset="chip-outline.dark.svg">
  <img src="chip-outline.svg"
    alt="Outline of the ISF15ACP4 in Module drawn to scale from above: body 23.13 x 23.0 x 18.0 mm, pitch 4.0
    mm, 10 pins.">
</picture>
<!-- /figure -->

Hardware description: [`docs/hardware.yml`](../hardware.yml)

Drawn from:

- [`docs/hardware.yml`](../hardware.yml)
- [`inc/isf15acp4.hpp`](../../inc/isf15acp4.hpp)
- [`inc/isf15acp4_spi_interface.hpp`](../../inc/isf15acp4_spi_interface.hpp)

## ISF15ACP4 pinout, Module

<!-- figure: chip-pinout -->
<picture>
  <source media="(prefers-color-scheme: dark)" srcset="chip-pinout.dark.svg">
  <img src="chip-pinout.svg"
    alt="Pinout of the ISF15ACP4 in Module, top view: 10 pins, each with its name, its electrical type as a
    colour and the driver hook that uses it.">
</picture>
<!-- /figure -->

Hardware description: [`docs/hardware.yml`](../hardware.yml)

Drawn from:

- [`docs/hardware.yml`](../hardware.yml)
- [`inc/isf15acp4.hpp`](../../inc/isf15acp4.hpp)
- [`inc/isf15acp4_spi_interface.hpp`](../../inc/isf15acp4_spi_interface.hpp)

## The layers, one of them yours

<!-- figure: driver-layers -->
<picture>
  <source media="(prefers-color-scheme: dark)" srcset="driver-layers.dark.svg">
  <img src="driver-layers.svg"
    alt="Layer diagram: your application calls the ISF15ACP4 core, which sends register reads and writes
    through the SPI port contract to the port class you write, which drives the chip; the core and the
    contract are the same on every MCU.">
</picture>
<!-- /figure -->

Figure module: [`docs/diagrams/figures/driver-layers.py`](../../docs/diagrams/figures/driver-layers.py)

Drawn from:

- `public calls` in [`inc/isf15acp4.hpp`](../../inc/isf15acp4.hpp)
- `SpiInterface` in [`inc/isf15acp4_spi_interface.hpp`](../../inc/isf15acp4_spi_interface.hpp)
