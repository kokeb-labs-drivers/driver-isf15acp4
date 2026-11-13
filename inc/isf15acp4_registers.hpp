/**
 * @file isf15acp4_registers.hpp
 * @brief SSD1331 command set: opcodes, parameter fields, limits and timing (SSD1331 datasheet
 * Rev 1.2, sections 7 to 9 and 12; NKK ISF15ACP4 sheet E7).
 * @copyright Copyright (c) 2026 The driver-isf15acp4 authors. SPDX-License-Identifier: MIT
 *
 * @details
 * The display controller has no readable register: serial mode is write only (SSD1331 section 8.1,
 * "No data read is provided in serial mode operation"). This file is therefore a command map, not a
 * register map: every opcode with the number of parameter bytes that follows it, the fields inside
 * the parameter bytes, the reset values and the time limits. Section numbers refer to the SSD1331
 * datasheet Rev 1.2; "NKK" is the ISF15ACP4 sheet. Every parameter byte is sent with D/C# low, like
 * the opcode (Table 10).
 */
#ifndef ISF15ACP4_REGISTERS_HPP
#define ISF15ACP4_REGISTERS_HPP

#include <array>
#include <cstddef>
#include <cstdint>

#include "isf15acp4_detail.hpp"

/** @brief The SSD1331 command set. */
namespace isf15acp4::reg {

using detail::operator""_u8;
using detail::operator""_u32;

/**
 * @defgroup isf15acp4_registers Command set
 * @brief Opcodes, fields, limits and times.
 * @{
 */

/// @name Opcodes (Table 10)
/// @{
inline constexpr uint8_t kSetColumnAddress = 0x15; ///< 15h + start, end (section 9.1.1).
inline constexpr uint8_t kSetRowAddress = 0x75;    ///< 75h + start, end (section 9.1.2).
inline constexpr uint8_t kSetContrastA = 0x81;     ///< 81h + contrast of colour A (9.1.3).
inline constexpr uint8_t kSetContrastB = 0x82;     ///< 82h + contrast of colour B.
inline constexpr uint8_t kSetContrastC = 0x83;     ///< 83h + contrast of colour C.
inline constexpr uint8_t kMasterCurrent = 0x87;    ///< 87h + attenuation 0..15 (9.1.4).
inline constexpr uint8_t kPrechargeSpeedA =
    0x8A; ///< 8Ah + second pre-charge speed of colour A (9.1.5).
inline constexpr uint8_t kPrechargeSpeedB = 0x8B; ///< 8Bh + speed of colour B.
inline constexpr uint8_t kPrechargeSpeedC = 0x8C; ///< 8Ch + speed of colour C.
inline constexpr uint8_t kRemap = 0xA0;           ///< A0h + remap and colour depth (9.1.6).
inline constexpr uint8_t kStartLine = 0xA1;       ///< A1h + display start line (9.1.7).
inline constexpr uint8_t kDisplayOffset = 0xA2;   ///< A2h + display offset (9.1.8).
inline constexpr uint8_t kModeNormal = 0xA4;      ///< A4h: normal display (9.1.9).
inline constexpr uint8_t kModeAllOn = 0xA5;       ///< A5h: every pixel at GS63.
inline constexpr uint8_t kModeAllOff = 0xA6;      ///< A6h: every pixel at GS0.
inline constexpr uint8_t kModeInverse = 0xA7;     ///< A7h: GS0 <-> GS63.
inline constexpr uint8_t kMultiplex = 0xA8;       ///< A8h + N = multiplex ratio - 1 (9.1.10).
inline constexpr uint8_t kDimMode = 0xAB;         ///< ABh + five bytes (9.1.11).
inline constexpr uint8_t kMasterConfig = 0xAD;    ///< ADh + 8Eh: external VCC (9.1.12).
inline constexpr uint8_t kDisplayDim = 0xAC;      ///< ACh: display on in dim mode (9.1.13).
inline constexpr uint8_t kDisplayOff = 0xAE;      ///< AEh: display off, sleep.
inline constexpr uint8_t kDisplayOn = 0xAF;       ///< AFh: display on, normal brightness.
inline constexpr uint8_t kPowerSave = 0xB0;       ///< B0h + 1Ah enable or 0Bh disable (9.1.14).
inline constexpr uint8_t kPhasePeriods = 0xB1;    ///< B1h + phase 1 and 2 periods (9.1.15).
inline constexpr uint8_t kClockDivider = 0xB3;    ///< B3h + divide ratio and frequency (9.1.16).
inline constexpr uint8_t kGrayScaleTable = 0xB8;  ///< B8h + 32 pulse widths (9.1.17).
inline constexpr uint8_t kLinearGrayScale = 0xB9; ///< B9h: built-in linear table (9.1.18).
inline constexpr uint8_t kPrechargeLevel = 0xBB;  ///< BBh + pre-charge voltage (9.1.19).
inline constexpr uint8_t kNopBC = 0xBC;           ///< BCh: no operation (9.1.21).
inline constexpr uint8_t kNopBD = 0xBD;           ///< BDh: no operation.
inline constexpr uint8_t kPrechargeBit0 = 0x01;   ///< BBh bit 0: reserved; NKK Table 3 sets it.
inline constexpr uint8_t kVcomh = 0xBE;           ///< BEh + VCOMH level (9.1.20).
inline constexpr uint8_t kNopE3 = 0xE3;           ///< E3h: no operation.
inline constexpr uint8_t kCommandLock = 0xFD;     ///< FDh + 12h unlock or 16h lock (9.1.22).
inline constexpr uint8_t kDrawLine = 0x21;        ///< 21h + 7 bytes (9.2.1).
inline constexpr uint8_t kDrawRectangle = 0x22;   ///< 22h + 10 bytes (9.2.2).
inline constexpr uint8_t kCopy = 0x23;            ///< 23h + 6 bytes (9.2.3).
inline constexpr uint8_t kDimWindow = 0x24;       ///< 24h + 4 bytes (9.2.4).
inline constexpr uint8_t kClearWindow = 0x25;     ///< 25h + 4 bytes (9.2.5).
inline constexpr uint8_t kFillEnable = 0x26;      ///< 26h + fill and reverse-copy bits (9.2.6).
inline constexpr uint8_t kScrollSetup = 0x27;     ///< 27h + 5 bytes (9.2.7).
inline constexpr uint8_t kScrollOff = 0x2E;       ///< 2Eh: stop scrolling (9.2.8).
inline constexpr uint8_t kScrollOn = 0x2F;        ///< 2Fh: start scrolling (9.2.9).
/// @}

/// @name Parameter values
/// @{
inline constexpr uint8_t kMasterConfigExternalVcc =
    0x8E;                                          ///< ADh parameter, A[0] = 0 (Table 10 note 1).
inline constexpr uint8_t kPowerSaveEnable = 0x1A;  ///< B0h parameter: power save on (reset value).
inline constexpr uint8_t kPowerSaveDisable = 0x0B; ///< B0h parameter: power save off.
inline constexpr uint8_t kCommandUnlock = 0x12;    ///< FDh parameter: unlock (reset value).
inline constexpr uint8_t kCommandLockOn = 0x16;    ///< FDh parameter: lock.
/// @}

/// @name Limits
/// @{
inline constexpr uint8_t kRamColumns = 96;       ///< GDDRAM width: columns 0 to 95 (section 7.5.1).
inline constexpr uint8_t kRamRows = 64;          ///< GDDRAM height: rows 0 to 63.
inline constexpr uint8_t kMuxMin = 16;           ///< Lowest multiplex ratio (A8h: N from 15).
inline constexpr uint8_t kMuxMax = 64;           ///< Highest multiplex ratio, the reset value.
inline constexpr uint8_t kMasterCurrentMax = 15; ///< 87h: 1/16 to 16/16 (steps 0 to 15).
inline constexpr uint8_t kPrechargeSteps = 31;   ///< BBh, BEh, ABh E: A[5:1] 0 to 31.
inline constexpr uint8_t kPhaseMax = 15;         ///< B1h: 1 to 15 DCLK per phase.
inline constexpr std::size_t kGrayScaleEntries = 32; ///< B8h: GS1, GS3 ... GS63.
inline constexpr uint8_t kGrayScaleMax = 125;        ///< B8h: longest pulse width, in DCLK.
inline constexpr uint8_t kScrollColumnsMax = 95;     ///< 27h A: 0 to 95 columns per step.
inline constexpr uint8_t kScrollRowsMax = 63;        ///< 27h D: 0 to 63 rows per step.
/// @}

/// @name Remap and colour depth, A0h parameter (section 9.1.6)
/// @{
inline constexpr uint32_t kRemapVertical = 1U << 0;       ///< A[0]: 1 = vertical address increment.
inline constexpr uint32_t kRemapColumnReverse = 1U << 1;  ///< A[1]: 1 = RAM column 0 at SEG95.
inline constexpr uint32_t kRemapColourReverse = 1U << 2;  ///< A[2]: 1 = order SC, SB, SA.
inline constexpr uint32_t kRemapComSwap = 1U << 3;        ///< A[3]: 1 = left-right swap on COM.
inline constexpr uint32_t kRemapComScanReverse = 1U << 4; ///< A[4]: 1 = scan COM[N-1] to COM0.
inline constexpr uint32_t kRemapComSplit = 1U << 5;       ///< A[5]: 1 = odd-even split of COM.
/// A[7:6]: 00 = 256 colours, 01 = 65k format 1 (reset value), 10 = 65k format 2.
using RemapDepth = detail::Field<uint8_t, 6U, 2U>;
/// @}

/// @name Fill and copy, 26h parameter (section 9.2.6)
/// @{
inline constexpr uint32_t kFillRectangle = 1U << 0; ///< A[0]: fill the rectangle of 22h.
inline constexpr uint32_t kReverseCopy = 1U << 4;   ///< A[4]: swap GS0 <-> GS63 during 23h.
/// @}

/// @name Parameter fields
/// @{

/// B1h: phase 1 in bits 3..0, phase 2 in bits 7..4 (section 9.1.15).
using PhaseOne = detail::Field<uint8_t, 0U, 4U>;
/// B1h phase 2 (bits 7..4).
using PhaseTwo = detail::Field<uint8_t, 4U, 4U>;
/// B3h: divide ratio D - 1 in bits 3..0 (section 9.1.16).
using ClockDivide = detail::Field<uint8_t, 0U, 4U>;
/// B3h: oscillator frequency in bits 7..4.
using ClockFrequency = detail::Field<uint8_t, 4U, 4U>;
/// BBh and BEh: a 5-bit level in bits 5..1 (bit 0 is 0). (ABh E is a raw 5-bit level in bits 4..0.)
using Level5 = detail::Field<uint8_t, 1U, 5U>;
/// 27h E: the scroll interval in bits 1..0.
using ScrollInterval = detail::Field<uint8_t, 0U, 2U>;
/// @}

/// @name Reset values (section 7.4 and Table 10)
/// @{
inline constexpr uint8_t kResetContrast = 0x80;      ///< 81h, 82h, 83h after RES#.
inline constexpr uint8_t kResetMasterCurrent = 0x0F; ///< 87h after RES#.
inline constexpr uint8_t kResetRemap = 0x40;         ///< A0h after RES#: 65k, horizontal, normal.
inline constexpr uint8_t kResetPhases = 0x74;        ///< B1h after RES#.
inline constexpr uint8_t kResetClock = 0xD0;         ///< B3h after RES#.
inline constexpr uint8_t kResetPrecharge = 0x3E;     ///< BBh and BEh after RES#.
/// @}

/// @name Times (sections 7.9 and 12, Table 18; NKK sheet E7)
/// @{

/// RES# low pulse width, minimum (Table 18, `tRW`).
inline constexpr uint32_t kResetPulseMinUs = 3;
/// Reset completion time, maximum (Table 18).
inline constexpr uint32_t kResetCompleteMaxUs = 2;
/// RES# low to VCC on, minimum (section 7.9, t2).
inline constexpr uint32_t kResetToVccMinUs = 3;
/// AFh to SEG/COM on, maximum (section 7.9, tAF).
inline constexpr uint32_t kDisplayOnDelayMs = 100;
/// VCC off to VDD off, minimum 0 and typical 100 ms (section 7.9, tOFF).
inline constexpr uint32_t kVccOffTypicalMs = 100;
/// @}

/** @} */

// Frame geometry and sizes the rest of the driver relies on.
static_assert((static_cast<uint32_t>(kRamColumns) * kRamRows) == 6144U,
              "96 x 64 pixels (section 2)");
static_assert(RemapDepth::kMax == 3U, "A0h A[7:6] is a 2-bit field");
static_assert(Level5::kMask == 0x3EU, "BBh, BEh: A[5:1], bit 0 clear");
static_assert(PhaseOne::kMask == 0x0FU, "B1h: phase 1 in bits 3..0");
static_assert(ClockFrequency::kMask == 0xF0U, "B3h: frequency in bits 7..4");

} // namespace isf15acp4::reg

#endif // ISF15ACP4_REGISTERS_HPP
