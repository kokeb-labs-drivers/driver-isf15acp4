# Changelog

All notable changes to this repository are listed here. Versions follow [Semantic Versioning](https://semver.org/).
Entries under **Breaking** need source changes in code that used the affected API; any of them calls for a major
version bump (a minor bump before 1.0).

## [Unreleased]

- Review round 1: `DisplayOff()` is never refused; vertical scroll needs 64 MUX in `SetMultiplexRatio()` and
  `ApplyConfig()`; per-variant supply and pin documentation; the ESP32 example uses SPI DMA; the provenance of the
  initialisation values is stated as inherited and derived.
- Review round 4: every wait from a configuration time is clamped to its limit (the stop path runs on an unchecked
  configuration too); `SetGrayScaleTable()` no longer clears `linear_gray_scale`, and `SetPrechargeSpeed()` sets
  `send_precharge_speeds`, so a restart is documented and consistent; the debouncer is reset only when the switch
  settings change; a raw 2Fh/2Eh keeps `IsScrolling()` in step and a raw ADh takes only 8Eh; the ESP32 example
  restarts only after a controller transfer failed; the compile-fail tests share a resource lock.
- Review round 3: the stored configuration stays valid in every setter order (`SetDimSettings()` also checks the
  contrast in force; a failed `ApplyConfig()` leaves the dim limit); `ApplyConfig()` returns `ScrollActive` while
  scrolling (**Breaking**: it was sent); `Poll()` delivers a press on a locked controller without waking it; a raw FDh
  takes only 12h or 16h; the three second pre-charge speeds are pinned to one session by a test.
- NKK application notes pass: the start-up values are checked against Tables 1 to 3 (steps 4.1 to 4.18) by
  `tests/test_nkk_tables.cpp`; the ISC15ANP4 and ISC01P presets no longer send the second pre-charge speeds (not in
  Tables 1 and 2: `Config::send_precharge_speeds`); the ISF15ACP4 sends BBh 1Dh as Table 3 does
  (`Config::precharge_bit0`); a dim contrast above the start-up contrast is refused by `Validate()` and
  `SetDimSettings()`, and `SetContrast()` refuses a contrast below the dim contrast (**Breaking** for a caller that
  set a contrast below the dim values); the controller is identified from the notes; the supply, charge-pump enable,
  scrolling and CS# rules of the notes are in the docs.
- Round 2: `CopyWindow()` takes its destination as a `Point` (six parameters); the start-up pin failures, the stop
  path and the idle policy errors are pinned by tests; the model requires the whole start-up sequence before VCC.

### Added

- ISF15ACP4 driver, first version: a 96 x 64 colour OLED on write-only SPI and a debounced switch.
  - Power sequence: the OLED supply is never enabled before the controller is reset and configured; the power-down
    always runs.
  - The SSD1331 command set: display on, dim and off, remap and colour depth, contrast, master current, pre-charge,
    VCOMH, gray scale, pixel windows in three depths, the drawing engine, scrolling, the command lock.
  - Switch: debounce, press, release, long press and repeat; idle dim and sleep with wake on a press.
  - Presets for the ISC15ANP4 and ISC01P variants.
  - Tests against an independent model of the controller with fault injection, mutation checks of the safety
    logic, a fuzz target, and compile-fail tests of the port contract.
  - Not verified on hardware; see docs/limitations.md.
