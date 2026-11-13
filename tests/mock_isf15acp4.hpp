/**
 * @file mock_isf15acp4.hpp
 * @brief An independent model of the SSD1331 display controller as wired in the ISF15ACP4: the
 * pins, the command decoder, GDDRAM, the drawing engine, scrolling and the power sequence rules.
 *
 * The model does not include the driver's headers: its command table is written from SSD1331
 * Rev 1.2 Table 10 (opcodes, parameter counts) and sections 7.4, 7.9 and 9. The serial interface is
 * write only, so the model has no read path either. It records a violation whenever the driver
 * breaks a rule of the datasheet or of the safety design (docs/contract.md): a supply enabled
 * before the controller is configured, a display on without supply, a supply off while the display
 * is on, a byte on the bus during reset, a reset pulse that is too short, a command while the
 * drawing engine is busy, an unknown opcode or a wrong parameter count, a logic supply cut while
 * the OLED supply is on.
 *
 * The controller's own timing is a model knob: `engine_busy_us` is how long a drawing command keeps
 * the engine busy. The datasheet gives no value (docs/limitations.md).
 */
#ifndef ISF15ACP4_MOCK_ISF15ACP4_HPP
#define ISF15ACP4_MOCK_ISF15ACP4_HPP

#include <array>
#include <cstddef>
#include <cstdint>

namespace isf15acp4_test {

/// Rules the model checks; each is one bit of `Ssd1331Model::violations()`.
inline constexpr uint32_t kVccBeforeConfig =
    1U << 0; ///< VCC enabled before the whole start-up sequence arrived since reset.
inline constexpr uint32_t kDisplayOnWithoutVcc = 1U << 1; ///< AFh or ACh while VCC is off.
inline constexpr uint32_t kVccOffWhileOn =
    1U << 2; ///< VCC disabled while the display is on (AEh must come first).
inline constexpr uint32_t kBusWhileReset = 1U << 3;   ///< A byte on the bus while RES# is low.
inline constexpr uint32_t kResetPulseShort = 1U << 4; ///< RES# low for less than 3 us.
inline constexpr uint32_t kVccTooSoon = 1U << 5; ///< VCC enabled less than 3 us after RES# fell.
inline constexpr uint32_t kEngineBusy = 1U
                                        << 6; ///< A byte while a drawing command is still running.
inline constexpr uint32_t kUnknownOpcode = 1U << 7; ///< An opcode that is not in Table 10.
inline constexpr uint32_t kVddCutWithVcc = 1U << 8; ///< The logic supply was cut while VCC was on.
inline constexpr uint32_t kByteWithoutCs = 1U << 9; ///< A byte clocked while CS# was high.
inline constexpr uint32_t kOnWithUndefinedRam =
    1U << 10; ///< The display was turned on before GDDRAM was written since reset.
inline constexpr uint32_t kParameterRange = 1U << 11; ///< A parameter outside its documented range.
inline constexpr uint32_t kVccWhileReset = 1U << 12;  ///< VCC enabled while RES# was low.
inline constexpr uint32_t kBusAfterReset =
    1U << 13; ///< A byte less than 2 us after RES# rose (reset completes within 2 us).

enum class Display : uint8_t { Off, On, Dim };

class Ssd1331Model {
public:
  static constexpr int kCols = 96;
  static constexpr int kRows = 64;
  static constexpr uint32_t kUndefined = 0x10000U; ///< RAM word of a cell not written since reset.

  Ssd1331Model() noexcept {
    resetRegisters();
    powerOnReset();
  }

  /// Knobs.
  uint32_t engine_busy_us{1000};
  bool segment_a_is_red{true};

  // --- time -------------------------------------------------------------------------------------
  void Elapse(uint32_t us) noexcept {
    now_us_ += us;
  }
  [[nodiscard]] uint64_t NowUs() const noexcept {
    return now_us_;
  }

  // --- pins -------------------------------------------------------------------------------------
  void SetRes(bool low) noexcept {
    if (low && !res_low_) {
      res_low_ = true;
      res_fell_at_ = now_us_;
    } else if (!low && res_low_) {
      res_low_ = false;
      res_rose_at_ = now_us_;
      res_rose_ = true;
      if ((now_us_ - res_fell_at_) < 3U) {
        violations_ |= kResetPulseShort;
      }
      resetRegisters();
      resets_++;
    } else {
      // No edge.
    }
  }
  void SetVcc(bool on) noexcept {
    if (on == vcc_) {
      return;
    }
    vcc_ = on;
    if (on) {
      vcc_on_count_++;
      if (res_low_) {
        violations_ |= kVccWhileReset;
      }
      if ((now_us_ - res_fell_at_) < 3U) {
        violations_ |= kVccTooSoon;
      }
      if (!configComplete()) {
        violations_ |= kVccBeforeConfig;
      }
    } else {
      vcc_off_at_ = now_us_;
      if (display_ != Display::Off) {
        violations_ |= kVccOffWhileOn;
      }
    }
  }
  void SetCs(bool low) noexcept {
    cs_low_ = low;
    if (low) {
      transfers_++;
    }
  }
  void SetDc(bool high) noexcept {
    dc_high_ = high;
  }
  /// The logic supply goes away (a test action): the chip loses everything.
  void CutLogicSupply() noexcept {
    if (vcc_) {
      violations_ |= kVddCutWithVcc;
    }
    vdd_off_at_ = now_us_;
    vdd_cuts_++;
    powerOnReset();
  }
  /// A brown-out of the logic supply: the chip resets with no RES# edge, VCC is untouched.
  void BrownOut() noexcept {
    resetRegisters();
    resets_++;
  }

  /// One byte clocked in with the current CS# and D/C#.
  void Byte(uint8_t b) noexcept {
    if (!cs_low_) {
      violations_ |= kByteWithoutCs;
      return;
    }
    if (res_low_) {
      violations_ |= kBusWhileReset;
      return;
    }
    if (res_rose_ && ((now_us_ - res_rose_at_) < 2U)) {
      violations_ |= kBusAfterReset;
    }
    if (now_us_ < busy_until_us_) {
      violations_ |= kEngineBusy;
    }
    if (dc_high_) {
      data_bytes_++;
      if (!locked_) {
        dataByte(b);
      }
    } else {
      command_bytes_++;
      commandByte(b);
    }
  }

  // --- observation ------------------------------------------------------------------------------
  [[nodiscard]] uint32_t Violations() const noexcept {
    return violations_;
  }
  void ClearViolations() noexcept {
    violations_ = 0;
  }
  [[nodiscard]] bool Vcc() const noexcept {
    return vcc_;
  }
  [[nodiscard]] bool ResLow() const noexcept {
    return res_low_;
  }
  [[nodiscard]] Display DisplayState() const noexcept {
    return display_;
  }
  [[nodiscard]] bool Locked() const noexcept {
    return locked_;
  }
  [[nodiscard]] bool Scrolling() const noexcept {
    return scrolling_;
  }
  [[nodiscard]] bool Lit() const noexcept {
    return vcc_ && (display_ != Display::Off);
  }
  [[nodiscard]] uint32_t Resets() const noexcept {
    return resets_;
  }
  [[nodiscard]] uint32_t VddCuts() const noexcept {
    return vdd_cuts_;
  }
  [[nodiscard]] uint32_t VccOnCount() const noexcept {
    return vcc_on_count_;
  }
  [[nodiscard]] uint64_t CommandBytes() const noexcept {
    return command_bytes_;
  }
  [[nodiscard]] uint64_t DataBytes() const noexcept {
    return data_bytes_;
  }
  [[nodiscard]] uint32_t Transfers() const noexcept {
    return transfers_;
  }
  [[nodiscard]] uint64_t VccOffAtUs() const noexcept {
    return vcc_off_at_;
  }
  [[nodiscard]] uint32_t UnknownOpcodes() const noexcept {
    return unknown_;
  }
  [[nodiscard]] uint8_t LastOpcode() const noexcept {
    return last_opcode_;
  }
  /// The first opcode received since the last reset (0 when none).
  [[nodiscard]] uint8_t FirstCommand() const noexcept {
    return first_command_;
  }

  [[nodiscard]] uint8_t Reg(uint8_t opcode) const noexcept {
    return regs_[opcode];
  }
  [[nodiscard]] uint8_t Contrast(int colour) const noexcept {
    return regs_[0x81U + static_cast<unsigned>(colour)];
  }
  [[nodiscard]] uint8_t Gray(int i) const noexcept {
    return gray_[static_cast<unsigned>(i)];
  }
  [[nodiscard]] bool FillEnabled() const noexcept {
    return fill_;
  }
  [[nodiscard]] uint8_t Mode() const noexcept {
    return mode_;
  }
  [[nodiscard]] int Mux() const noexcept {
    return mux_;
  }
  [[nodiscard]] bool RamDefined() const noexcept {
    return ram_defined_;
  }
  /// The transfer (CS# low period, counted from 1) in which the opcode last executed; 0 = never.
  [[nodiscard]] uint32_t OpTransfer(uint8_t opcode) const noexcept {
    return op_transfer_[opcode];
  }
  /// Whether the opcode has been executed since the last reset.
  [[nodiscard]] bool Seen(uint8_t opcode) const noexcept {
    return seen_[opcode];
  }
  /// The ABh parameters: the three dim contrasts and the dim pre-charge.
  [[nodiscard]] std::array<uint8_t, 4> Dim() const noexcept {
    return dim_;
  }
  [[nodiscard]] std::array<uint8_t, 5> ScrollParams() const noexcept {
    return scroll_;
  }

  /// RAM word of a cell: C[15:11] B[10:5] A[4:0], or `kUndefined`.
  [[nodiscard]] uint32_t Ram(int col, int row) const noexcept {
    return ram_[static_cast<unsigned>((row * kCols) + col)];
  }
  /// What the pixel shows when lit, as an RGB565 word (segment A is red unless A0h bit 2 swaps it).
  [[nodiscard]] uint16_t Rgb565At(int col, int row) const noexcept {
    const uint32_t w = Ram(col, row) & 0xFFFFU;
    const uint32_t c = (w >> 11U) & 0x1FU;
    const uint32_t b = (w >> 5U) & 0x3FU;
    const uint32_t a = w & 0x1FU;
    const bool reversed = (regs_[0xA0] & 0x04U) != 0U;
    const bool a_is_red = (segment_a_is_red != reversed); // reversed order: C drives group A
    const uint32_t red = a_is_red ? a : c;
    const uint32_t blue = a_is_red ? c : a;
    return static_cast<uint16_t>((red << 11U) | (b << 5U) | blue);
  }
  /// Number of cells written since reset.
  [[nodiscard]] int DefinedCells() const noexcept {
    int n = 0;
    for (const uint32_t w : ram_) {
      n += (w != kUndefined) ? 1 : 0;
    }
    return n;
  }

private:
  void powerOnReset() noexcept {
    res_fell_at_ = 0;
    res_rose_ = false;
    vcc_ = false;
  }

  void resetRegisters() noexcept {
    // Section 7.4 and the Default column of Table 10.
    first_seen_ = false;
    regs_.fill(0);
    regs_[0x81] = 0x80;
    regs_[0x82] = 0x80;
    regs_[0x83] = 0x80;
    regs_[0x87] = 0x0F;
    regs_[0xA0] = 0x40;
    regs_[0xB1] = 0x74;
    regs_[0xB3] = 0xD0;
    regs_[0xBB] = 0x3E;
    regs_[0xBE] = 0x3E;
    for (int i = 0; i < 32; ++i) {
      gray_[static_cast<unsigned>(i)] = static_cast<uint8_t>((2 * i) + 1);
    }
    mux_ = 64;
    mode_ = 0xA4;
    display_ = Display::Off;
    locked_ = false;
    scrolling_ = false;
    fill_ = false;
    reverse_copy_ = false;
    master_config_ext_ = false;
    seen_.fill(false);
    op_transfer_.fill(0U);
    remap_seen_ = false;
    ram_defined_ = false;
    for (uint32_t& w : ram_) {
      w = kUndefined;
    }
    col_start_ = 0;
    col_end_ = 95;
    row_start_ = 0;
    row_end_ = 63;
    col_ = 0;
    row_ = 0;
    need_ = 0;
    got_ = 0;
    pending_ = 0;
    pend_data_ = 0;
    have_high_ = false;
    busy_until_us_ = 0;
  }

  // Parameter counts of Table 10; -1 for an opcode that is not in the table.
  static int paramCount(uint8_t op) noexcept {
    switch (op) {
    case 0x15:
    case 0x75:
      return 2;
    case 0x81:
    case 0x82:
    case 0x83:
    case 0x87:
    case 0x8A:
    case 0x8B:
    case 0x8C:
      return 1;
    case 0xA0:
    case 0xA1:
    case 0xA2:
    case 0xA8:
    case 0xAD:
    case 0xB0:
    case 0xB1:
    case 0xB3:
      return 1;
    case 0xBB:
    case 0xBE:
    case 0xFD:
    case 0x26:
      return 1;
    case 0xA4:
    case 0xA5:
    case 0xA6:
    case 0xA7:
    case 0xAC:
    case 0xAE:
    case 0xAF:
    case 0xB9:
      return 0;
    case 0xBC:
    case 0xBD:
    case 0xE3:
    case 0x2E:
    case 0x2F:
      return 0;
    case 0xAB:
    case 0x27:
      return 5;
    case 0xB8:
      return 32;
    case 0x21:
      return 7;
    case 0x22:
      return 10;
    case 0x23:
      return 6;
    case 0x24:
    case 0x25:
      return 4;
    default:
      return -1;
    }
  }

  void commandByte(uint8_t b) noexcept {
    if (need_ > 0) {
      params_[static_cast<unsigned>(got_++)] = b;
      if (got_ == need_) {
        need_ = 0;
        execute();
      }
      return;
    }
    if (locked_ && (b != 0xFDU)) {
      return; // the locked interface ignores everything but FDh (section 9.1.22)
    }
    const int n = paramCount(b);
    if (n < 0) {
      violations_ |= kUnknownOpcode;
      unknown_++;
      return;
    }
    pending_ = b;
    last_opcode_ = b;
    if (!first_seen_) {
      first_seen_ = true;
      first_command_ = b;
    }
    got_ = 0;
    if (n == 0) {
      execute();
    } else {
      need_ = n;
    }
  }

  /// The whole start-up sequence has arrived since reset: external VCC, remap, geometry, contrast,
  /// current, dim settings, phases, clock and the two levels.
  [[nodiscard]] bool configComplete() const noexcept {
    constexpr std::array<uint8_t, 15> kNeeded{0xA0, 0xA1, 0xA2, 0xA4, 0xA8, 0xAB, 0xAD, 0x81,
                                              0x82, 0x83, 0x87, 0xB1, 0xB3, 0xBB, 0xBE};
    for (const uint8_t op : kNeeded) {
      if (!seen_[op]) {
        return false;
      }
    }
    return master_config_ext_ && remap_seen_;
  }

  void execute() noexcept {
    seen_[pending_] = true;
    op_transfer_[pending_] = transfers_;
    const uint8_t p0 = params_[0];
    switch (pending_) {
    case 0x15:
      col_start_ = p0;
      col_end_ = params_[1];
      col_ = p0;
      if ((p0 > 95) || (params_[1] > 95)) {
        violations_ |= kParameterRange;
      }
      break;
    case 0x75:
      row_start_ = p0;
      row_end_ = params_[1];
      row_ = p0;
      if ((p0 > 63) || (params_[1] > 63)) {
        violations_ |= kParameterRange;
      }
      break;
    case 0x81:
    case 0x82:
    case 0x83:
    case 0x87:
    case 0x8A:
    case 0x8B:
    case 0x8C:
    case 0xA1:
    case 0xA2:
    case 0xB1:
    case 0xB3:
    case 0xBB:
    case 0xBE:
      regs_[pending_] = p0;
      if ((pending_ == 0x87) && (p0 > 15)) {
        violations_ |= kParameterRange;
      }
      if (((pending_ == 0xA1) || (pending_ == 0xA2)) && (p0 > 63)) {
        violations_ |= kParameterRange;
      }
      if ((pending_ == 0xB1) && (((p0 & 0x0FU) == 0U) || ((p0 >> 4U) == 0U))) {
        violations_ |= kParameterRange;
      }
      break;
    case 0xA0:
      regs_[0xA0] = p0;
      remap_seen_ = true;
      break;
    case 0xA4:
    case 0xA5:
    case 0xA6:
    case 0xA7:
      mode_ = pending_;
      break;
    case 0xA8:
      mux_ = (p0 & 0x3F) + 1;
      if (p0 < 15) {
        violations_ |= kParameterRange;
      }
      break;
    case 0xAB:
      regs_[0xAB] = 1;
      dim_ = {params_[1], params_[2], params_[3], params_[4]};
      if (params_[4] > 31) {
        violations_ |= kParameterRange;
      }
      break;
    case 0xAD:
      master_config_ext_ = (p0 == 0x8E);
      break;
    case 0xAE:
      display_ = Display::Off;
      break;
    case 0xAF:
      turnOn(Display::On);
      break;
    case 0xAC:
      turnOn(Display::Dim);
      break;
    case 0xB0:
      regs_[0xB0] = p0;
      break;
    case 0xB8:
      for (int i = 0; i < 32; ++i) {
        gray_[static_cast<unsigned>(i)] = params_[static_cast<unsigned>(i)];
      }
      break;
    case 0xB9:
      for (int i = 0; i < 32; ++i) {
        gray_[static_cast<unsigned>(i)] = static_cast<uint8_t>((2 * i) + 1);
      }
      break;
    case 0xFD:
      locked_ = (p0 == 0x16);
      if ((p0 != 0x12U) && (p0 != 0x16U)) {
        violations_ |= kParameterRange; // only 12h (unlock) and 16h (lock) are defined
      }
      break;
    case 0x26:
      fill_ = (p0 & 0x01U) != 0U;
      reverse_copy_ = (p0 & 0x10U) != 0U;
      break;
    case 0x27:
      scroll_ = {p0, params_[1], params_[2], params_[3], params_[4]};
      break;
    case 0x2E:
      scrolling_ = false;
      break;
    case 0x2F:
      scrolling_ = true;
      break;
    case 0x21:
      line();
      break;
    case 0x22:
      rectangle();
      break;
    case 0x23:
      copy();
      break;
    case 0x24:
      dim();
      break;
    case 0x25:
      clearWindow();
      break;
    default:
      break; // NOPs
    }
  }

  void turnOn(Display d) noexcept {
    if (!vcc_) {
      violations_ |= kDisplayOnWithoutVcc;
    }
    if (!ram_defined_) {
      violations_ |= kOnWithUndefinedRam;
    }
    display_ = d;
  }

  void touchRam() noexcept {
    int n = 0;
    for (const uint32_t w : ram_) {
      n += (w != kUndefined) ? 1 : 0;
    }
    ram_defined_ = (n == kCols * kRows);
  }

  void put(int col, int row, uint32_t w) noexcept {
    if ((col >= 0) && (col < kCols) && (row >= 0) && (row < kRows)) {
      ram_[static_cast<unsigned>((row * kCols) + col)] = w;
    }
  }

  void advance() noexcept {
    const bool vertical = (regs_[0xA0] & 0x01U) != 0U;
    if (!vertical) {
      if (col_ >= col_end_) {
        col_ = col_start_;
        row_ = (row_ >= row_end_) ? row_start_ : row_ + 1;
      } else {
        col_++;
      }
    } else if (row_ >= row_end_) {
      row_ = row_start_;
      col_ = (col_ >= col_end_) ? col_start_ : col_ + 1;
    } else {
      row_++;
    }
  }

  void dataByte(uint8_t b) noexcept {
    const unsigned depth = (static_cast<unsigned>(regs_[0xA0]) >> 6U) & 0x03U;
    uint32_t word = 0;
    if (depth == 1U) { // 65k format 1: C4..C0 B5..B3 | B2..B0 A4..A0
      if (!have_high_) {
        pend_data_ = b;
        have_high_ = true;
        return;
      }
      word = (static_cast<uint32_t>(pend_data_) << 8U) | b;
      have_high_ = false;
    } else if (depth == 0U) { // 256 colours: C4..C2 B5..B3 A4..A3, the rest copied (Figure 11)
      const uint32_t v = b;
      const uint32_t c = (v >> 5U) & 7U;
      const uint32_t bb = (v >> 2U) & 7U;
      const uint32_t a = v & 3U;
      const uint32_t c5 = (c << 2U) | (((c >> 2U) & 1U) * 3U);
      const uint32_t b6 = (bb << 3U) | (((bb >> 2U) & 1U) * 7U);
      const uint32_t a5 = (a << 3U) | (((a >> 1U) & 1U) * 7U);
      word = (c5 << 11U) | (b6 << 5U) | a5;
    } else { // 65k format 2: three bytes of six bits
      pend_bytes_[static_cast<unsigned>(have3_++)] = b;
      if (have3_ < 3) {
        return;
      }
      have3_ = 0;
      const uint32_t b0 = pend_bytes_[0];
      const uint32_t b1 = pend_bytes_[1];
      const uint32_t b2 = pend_bytes_[2];
      word = (((b0 >> 1U) & 0x1FU) << 11U) | ((b1 & 0x3FU) << 5U) | ((b2 >> 1U) & 0x1FU);
    }
    put(col_, row_, word);
    advance();
    touchRam();
  }

  [[nodiscard]] static uint32_t packColour(uint8_t c, uint8_t bb, uint8_t a) noexcept {
    const uint32_t c32 = c;
    const uint32_t b32 = bb;
    const uint32_t a32 = a;
    return (((c32 >> 1U) & 0x1FU) << 11U) | ((b32 & 0x3FU) << 5U) | ((a32 >> 1U) & 0x1FU);
  }

  void busy() noexcept {
    busy_until_us_ = now_us_ + engine_busy_us;
  }

  void line() noexcept {
    int x0 = params_[0];
    int y0 = params_[1];
    const int x1 = params_[2];
    const int y1 = params_[3];
    const uint32_t w = packColour(params_[4], params_[5], params_[6]);
    const int dx = x1 > x0 ? x1 - x0 : x0 - x1;
    const int dy = y1 > y0 ? y1 - y0 : y0 - y1;
    const int sx = x0 < x1 ? 1 : -1;
    const int sy = y0 < y1 ? 1 : -1;
    int err = dx - dy;
    for (int i = 0; i < 400; ++i) {
      put(x0, y0, w);
      if ((x0 == x1) && (y0 == y1)) {
        break;
      }
      const int e2 = 2 * err;
      if (e2 > -dy) {
        err -= dy;
        x0 += sx;
      }
      if (e2 < dx) {
        err += dx;
        y0 += sy;
      }
    }
    touchRam();
    busy();
  }

  void rectangle() noexcept {
    const int x0 = params_[0];
    const int y0 = params_[1];
    const int x1 = params_[2];
    const int y1 = params_[3];
    const uint32_t o = packColour(params_[4], params_[5], params_[6]);
    const uint32_t f = packColour(params_[7], params_[8], params_[9]);
    for (int y = y0; y <= y1; ++y) {
      for (int x = x0; x <= x1; ++x) {
        const bool edge = (x == x0) || (x == x1) || (y == y0) || (y == y1);
        if (edge) {
          put(x, y, o);
        } else if (fill_) {
          put(x, y, f);
        } else {
          // Outline only: the inside is untouched.
        }
      }
    }
    touchRam();
    busy();
  }

  void copy() noexcept {
    const int x0 = params_[0];
    const int y0 = params_[1];
    const int x1 = params_[2];
    const int y1 = params_[3];
    const int nx = params_[4];
    const int ny = params_[5];
    std::array<uint32_t, static_cast<std::size_t>(kCols) * kRows> snap = ram_;
    for (int y = y0; y <= y1; ++y) {
      for (int x = x0; x <= x1; ++x) {
        uint32_t w = snap[static_cast<unsigned>((y * kCols) + x)];
        if (reverse_copy_ && (w != kUndefined)) {
          w = invert(w);
        }
        put(nx + (x - x0), ny + (y - y0), w);
      }
    }
    touchRam();
    busy();
  }

  static uint32_t invert(uint32_t w) noexcept {
    const uint32_t c = 31U - ((w >> 11U) & 0x1FU);
    const uint32_t b = 63U - ((w >> 5U) & 0x3FU);
    const uint32_t a = 31U - (w & 0x1FU);
    return (c << 11U) | (b << 5U) | a;
  }

  static uint32_t gsDim(uint32_t gs) noexcept {
    return gs < 16U ? gs : 4U + ((gs - 16U) / 4U);
  }

  void dim() noexcept {
    const int xa = params_[0];
    const int ya = params_[1];
    const int xb = params_[2];
    const int yb = params_[3];
    for (int y = ya; y <= yb; ++y) {
      for (int x = xa; x <= xb; ++x) {
        const uint32_t w = Ram(x, y);
        if (w == kUndefined) {
          continue;
        }
        const uint32_t c = gsDim(((w >> 11U) & 0x1FU) * 2U) / 2U;
        const uint32_t b = gsDim((w >> 5U) & 0x3FU);
        const uint32_t a = gsDim((w & 0x1FU) * 2U) / 2U;
        put(x, y, (c << 11U) | (b << 5U) | a);
      }
    }
    busy();
  }

  void clearWindow() noexcept {
    const int xa = params_[0];
    const int ya = params_[1];
    const int xb = params_[2];
    const int yb = params_[3];
    for (int y = ya; y <= yb; ++y) {
      for (int x = xa; x <= xb; ++x) {
        put(x, y, 0U);
      }
    }
    touchRam();
    busy();
  }

  std::array<uint8_t, 256> regs_{};
  std::array<uint8_t, 32> gray_{};
  std::array<uint32_t, static_cast<std::size_t>(kCols) * kRows> ram_{};
  std::array<uint8_t, 40> params_{};
  std::array<uint8_t, 5> scroll_{};
  std::array<uint8_t, 4> dim_{};
  std::array<uint8_t, 3> pend_bytes_{};
  uint64_t now_us_{1000000};
  uint64_t res_fell_at_{0};
  uint64_t res_rose_at_{0};
  bool res_rose_{false};
  uint64_t vcc_off_at_{0};
  uint64_t vdd_off_at_{0};
  uint64_t busy_until_us_{0};
  uint64_t command_bytes_{0};
  uint64_t data_bytes_{0};
  uint32_t violations_{0};
  uint32_t resets_{0};
  uint32_t vdd_cuts_{0};
  uint32_t vcc_on_count_{0};
  uint32_t transfers_{0};
  uint32_t unknown_{0};
  int need_{0};
  int got_{0};
  int mux_{64};
  int col_start_{0}, col_end_{95}, row_start_{0}, row_end_{63}, col_{0}, row_{0};
  int have3_{0};
  uint8_t pending_{0};
  uint8_t pend_data_{0};
  uint8_t mode_{0xA4};
  uint8_t last_opcode_{0};
  uint8_t first_command_{0};
  bool first_seen_{false};
  Display display_{Display::Off};
  bool have_high_{false};
  bool vcc_{false};
  bool res_low_{false};
  bool cs_low_{false};
  bool dc_high_{false};
  bool locked_{false};
  bool scrolling_{false};
  bool fill_{false};
  bool reverse_copy_{false};
  bool master_config_ext_{false};
  bool remap_seen_{false};
  std::array<bool, 256> seen_{};
  std::array<uint32_t, 256> op_transfer_{};
  bool ram_defined_{false};
};

} // namespace isf15acp4_test

#endif // ISF15ACP4_MOCK_ISF15ACP4_HPP
