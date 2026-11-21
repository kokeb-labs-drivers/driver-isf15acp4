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

## What the ISF15ACP4 does

<!-- figure: chip-capabilities -->
<picture>
  <source media="(prefers-color-scheme: dark)" srcset="chip-capabilities.dark.svg">
  <img src="chip-capabilities.svg"
    alt="Capability map of the ISF15ACP4: Colour display (supported): Writes windows, single pixels and whole
    frames in 65,536 or 256 colours on the 96 x 64 panel; Drawing engine (supported): Draws lines and
    rectangles, copies, dims and clears a window with the controller's own engine; Scrolling
    (supported): Scrolls a window of the display horizontally and vertically at a set speed; Contrast
    and current (supported): Sets the contrast of each colour, the master current and a dim mode; Power
    sequence (supported): Resets and configures the controller with the 15 V supply off, then enables it
    and turns the panel on; Sleep (supported): Puts the display to sleep, 10 µA at most on each supply,
    and wakes it on the next press; Controller settings (supported): Remap, start line, clock, phase
    periods, gray scale table, pre-charge and VCOMH of the controller; SPST switch (supported):
    Debounces the contact and reports press, release, long press and repeat events; Smaller modules
    (supported): Presets for the ISC15ANP4 (64 x 48, with switch) and ISC01P (52 x 36, display only).">
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

## ISF15ACP4: Display window commands

<!-- figure: chip-frame-window -->
<picture>
  <source media="(prefers-color-scheme: dark)" srcset="chip-frame-window.dark.svg">
  <img src="chip-frame-window.svg"
    alt="Display window commands of the ISF15ACP4, 24 bits, most significant bit first. Column: 15h (bits 23
    to 16), * (bit 15), start A6 to A0 (bits 14 to 8), * (bit 7), end B6 to B0 (bits 6 to 0); Row: 75h
    (bits 23 to 16), * * (bits 15 to 14), start A5 to A0 (bits 13 to 8), * * (bits 7 to 6), end B5 to B0
    (bits 5 to 0).">
</picture>
<!-- /figure -->

Hardware description: [`docs/hardware.yml`](../hardware.yml)

Drawn from:

- [`docs/hardware.yml`](../hardware.yml)
- [`inc/isf15acp4.hpp`](../../inc/isf15acp4.hpp)
- [`inc/isf15acp4_spi_interface.hpp`](../../inc/isf15acp4_spi_interface.hpp)

## ISF15ACP4

<!-- figure: chip-hero -->
<picture>
  <source media="(prefers-color-scheme: dark)" srcset="chip-hero.dark.svg">
  <img src="chip-hero.svg"
    alt="Overview of the ISF15ACP4, a pushbutton with a 96 RGB x 64 colour OLED in its cap and a normally
    open switch under it: the display takes commands and pixels over a write-only SPI bus, the switch is
    a plain contact on two pins. A MCU talks to it over SPI, write only; it drives Colour OLED; it reads
    back Switch. Headline figures: 96 RGB x 64 pixels in the cap; 65,536 colours in 16-bit mode; 1
    million actuations at least; 50,000 h display life at least.">
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

## Wiring: Typical application

<!-- figure: chip-wiring-typical -->
<picture>
  <source media="(prefers-color-scheme: dark)" srcset="chip-wiring-typical.dark.svg">
  <img src="chip-wiring-typical.svg"
    alt="Wiring of the ISF15ACP4 for the Typical application: MCU SPI SCK to SCK (Write()); MCU SPI MOSI to
    SDI (Write()); MCU GPIO / CS to SS (Write()); MCU GPIO to DC (Write(DataKind)); MCU GPIO to RES
    (GpioSet(CtrlPin::Reset)); MCU GPIO (input) to SW1 (ReadSwitch()). Supplies, grounds and the support
    parts are listed under the drawing.">
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

## SPI write timing

<!-- figure: spi-timing -->
<picture>
  <source media="(prefers-color-scheme: dark)" srcset="spi-timing.dark.svg">
  <img src="spi-timing.svg"
    alt="SPI write timing of the ISF15ACP4's SSD1331 controller in mode 0: CS# low frames one byte, D/C#
    selects command or data, and SDIN is read on each rising SCLK edge, most significant bit first, with
    the times of Table 21.">
</picture>
<!-- /figure -->

Hardware description: [`docs/model/spi-timing.timing.yml`](../model/spi-timing.timing.yml)

Drawn from:

- [`docs/model/spi-timing.timing.yml`](../model/spi-timing.timing.yml)

## From start-up to a drawn rectangle

<!-- figure: start-up-and-draw -->
<picture>
  <source media="(prefers-color-scheme: dark)" srcset="start-up-and-draw.dark.svg">
  <img src="start-up-and-draw.svg"
    alt="From start-up to a drawn rectangle: initialisation powers and resets the display and clears its
    memory, DisplayOn raises the panel supply and sends the display-on command, and DrawRectangle sends
    one drawing command.">
</picture>
<!-- /figure -->

Hardware description: [`docs/model/start-up-and-draw.seq.yml`](../model/start-up-and-draw.seq.yml)

Drawn from:

- [`docs/model/start-up-and-draw.seq.yml`](../model/start-up-and-draw.seq.yml)
