/**
 * @file isf15acp4.ipp
 * @brief ISF15ACP4 driver implementation, included by `isf15acp4.hpp`.
 * @copyright Copyright (c) 2026 The driver-isf15acp4 authors. SPDX-License-Identifier: MIT
 *
 * Section numbers refer to the SSD1331 datasheet Rev 1.2; "NKK" is the ISF15ACP4 sheet.
 */
#ifndef ISF15ACP4_IPP
#define ISF15ACP4_IPP

namespace isf15acp4 {

// -------------------------------------------------------------------------------------------------
// Lifecycle and power (SSD1331 section 7.9; NKK sheet E7, state transitions)
// -------------------------------------------------------------------------------------------------

template <SpiPort Port>
Result<void> ISF15ACP4<Port>::EnsureInitialized() noexcept {
  if (initialized_) {
    return Result<void>::Success();
  }
  const Config config = config_;
  if (Validate(config) != Error::Ok) {
    return fail(Error::InvalidArgument, "isf15acp4: invalid Config");
  }
  if (auto r = powerUp(config); !r) {
    return r;
  }
  initialized_ = true;
  shadow_doubt_ = false;
  detail::Log(bus_.GetPort(), LogLevel::Info, "isf15acp4: initialised, supply off");
  return Result<void>::Success();
}

template <SpiPort Port>
Result<void> ISF15ACP4<Port>::powerUp(const Config& config) noexcept {
  if (!bus_.GetPort().EnsureInitialized()) {
    return fail(Error::Bus, "isf15acp4: the port could not bring the bus up");
  }
  // The supply is off before the controller is touched, whatever an earlier session left on.
  if (auto r = pin(CtrlPin::VccEnable, GpioSignal::INACTIVE); !r) {
    return r;
  }
  vcc_on_ = false;
  output_ = Output::Off;
  // RES# low for at least 3 us (Table 18), then high; reset completes within 2 us.
  if (auto r = pin(CtrlPin::Reset, GpioSignal::ACTIVE); !r) {
    return r;
  }
  detail::DelayUs(bus_.GetPort(), reg::kResetPulseMinUs);
  if (auto r = pin(CtrlPin::Reset, GpioSignal::INACTIVE); !r) {
    return r;
  }
  detail::DelayUs(bus_.GetPort(), reg::kResetCompleteMaxUs);
  locked_ = false;
  scrolling_ = false;
  scroll_set_ = false;
  ram_rewrite_ = false;
  idle_lowered_ = false;
  if (auto r = send1(reg::kDisplayOff); !r) {
    return r;
  }
  if (auto r = sendConfig(config); !r) {
    return r;
  }
  if (config.clear_ram) {
    // GDDRAM content is undefined after a reset: clear it before the supply can light it up.
    if (auto r = sendWindow(uint8_t{0}, uint8_t{0}, static_cast<uint8_t>(reg::kRamColumns - 1U),
                            static_cast<uint8_t>(reg::kRamRows - 1U));
        !r) {
      return r;
    }
    std::fill(data_.begin(), data_.end(), uint8_t{0});
    const std::size_t total = static_cast<std::size_t>(reg::kRamColumns) * reg::kRamRows *
                              detail::BytesPerPixel(config.remap.depth);
    const std::size_t chunks = total / data_.size();
    for (std::size_t i = 0U; i < chunks; ++i) {
      if (auto r = sendData(data_.size()); !r) {
        return r;
      }
    }
  }
  config_ = config;
  multiplex_ = GeometryOf(config.variant).multiplex;
  configureButton(config.button);
  return Result<void>::Success();
}

template <SpiPort Port>
Result<void> ISF15ACP4<Port>::sendConfig(const Config& config) noexcept {
  using detail::operator""_u8;
  const Geometry geometry = GeometryOf(config.variant);
  const Remap& rm = config.remap;
  auto remap = static_cast<uint32_t>(reg::RemapDepth::Insert(0_u8, detail::ToUnderlying(rm.depth)));
  remap |= rm.vertical_increment ? reg::kRemapVertical : 0U;
  remap |= rm.column_reverse ? reg::kRemapColumnReverse : 0U;
  remap |= rm.reverse_colour_order ? reg::kRemapColourReverse : 0U;
  remap |= rm.com_left_right_swap ? reg::kRemapComSwap : 0U;
  remap |= rm.com_scan_reverse ? reg::kRemapComScanReverse : 0U;
  remap |= rm.com_split_odd_even ? reg::kRemapComSplit : 0U;
  const uint8_t phases =
      reg::PhaseTwo::Insert(reg::PhaseOne::Insert(0_u8, config.phase1), config.phase2);
  const uint8_t power_save = config.power_save ? reg::kPowerSaveEnable : reg::kPowerSaveDisable;
  const auto mode_opcode = static_cast<uint8_t>(static_cast<uint32_t>(reg::kModeNormal) +
                                                detail::ToUnderlying(config.mode));
  // One opcode and one parameter each; the order follows the datasheet's command table.
  const std::array<std::array<uint8_t, 2>, 11> pairs{{
      {reg::kMasterConfig, reg::kMasterConfigExternalVcc}, // must be 0b after RESET (Table 10)
      {reg::kRemap, static_cast<uint8_t>(remap)},
      {reg::kStartLine, config.start_line},
      {reg::kDisplayOffset, config.display_offset},
      {reg::kMultiplex, static_cast<uint8_t>(geometry.multiplex - 1U)},
      {reg::kSetContrastA, config.contrast_a},
      {reg::kSetContrastB, config.contrast_b},
      {reg::kSetContrastC, config.contrast_c},
      {reg::kMasterCurrent, config.master_current},
      {reg::kPowerSave, power_save},
      {reg::kPhasePeriods, phases},
  }};
  for (const auto& pair : pairs) {
    if (auto r = send2(pair[0], pair[1]); !r) {
      return r;
    }
  }
  if (auto r = send1(mode_opcode); !r) {
    return r;
  }
  if (auto r = sendTiming(config); !r) {
    return r;
  }
  // The dim limit moves only once the whole configuration is in the controller.
  contrast_cap_ = {config.contrast_a, config.contrast_b, config.contrast_c};
  return Result<void>::Success();
}

template <SpiPort Port>
uint8_t ISF15ACP4<Port>::prechargeByte(uint8_t level, const Config& config) noexcept {
  using detail::operator""_u8;
  // Bit 0 is reserved in the SSD1331 table; NKK's Table 3 sends 1Dh, Tables 1 and 2 send 12h.
  const uint8_t bit0 = config.precharge_bit0 ? reg::kPrechargeBit0 : 0_u8;
  return static_cast<uint8_t>(reg::Level5::Insert(0_u8, level) | bit0);
}

template <SpiPort Port>
Result<void> ISF15ACP4<Port>::sendTiming(const Config& config) noexcept {
  using detail::operator""_u8;
  const uint8_t clock = reg::ClockFrequency::Insert(
      reg::ClockDivide::Insert(0_u8, config.clock_divide), config.clock_frequency);
  // The three second pre-charge speeds go together: six bytes in one session (NKK notes, Table 3,
  // note 2; SSD1331 Table 10). The notes' Tables 1 and 2 do not send them.
  if (config.send_precharge_speeds) {
    const std::array<uint8_t, 6> speeds{reg::kPrechargeSpeedA, config.precharge_speed_a,
                                        reg::kPrechargeSpeedB, config.precharge_speed_b,
                                        reg::kPrechargeSpeedC, config.precharge_speed_c};
    static_cast<void>(std::copy(speeds.begin(), speeds.end(), command_.begin()));
    if (auto r = send(speeds.size()); !r) {
      return r;
    }
  }
  const std::array<uint8_t, 6> dim{reg::kDimMode,         0_u8,
                                   config.dim.contrast_a, config.dim.contrast_b,
                                   config.dim.contrast_c, config.dim.precharge};
  static_cast<void>(std::copy(dim.begin(), dim.end(), command_.begin()));
  if (auto r = send(dim.size()); !r) {
    return r;
  }
  if (auto r = send2(reg::kClockDivider, clock); !r) {
    return r;
  }
  if (config.linear_gray_scale) {
    if (auto r = send1(reg::kLinearGrayScale); !r) {
      return r;
    }
  }
  if (auto r = send2(reg::kPrechargeLevel, prechargeByte(config.precharge_level, config)); !r) {
    return r;
  }
  return send2(reg::kVcomh, reg::Level5::Insert(0_u8, config.vcomh_level));
}

template <SpiPort Port>
Result<void> ISF15ACP4<Port>::Deinitialize() noexcept {
  // Every step runs, whatever the state and whatever fails (never refused).
  const Result<void> r = safeOff(true);
  initialized_ = false;
  scrolling_ = false;
  scroll_set_ = false;
  idle_lowered_ = false;
  return r;
}

template <SpiPort Port>
Result<void> ISF15ACP4<Port>::safeOff(bool wait_discharge) noexcept {
  using detail::operator""_u32;
  Error first = Error::Ok;
  const bool was_on = vcc_on_;
  if (!bus_.GetPort().EnsureInitialized()) {
    first = Error::Bus;
  }
  // Unlock (a locked controller ignores AEh), then display off, in one transfer (Table 10, FDh).
  command_[0] = reg::kCommandLock;
  command_[1] = reg::kCommandUnlock;
  command_[2] = reg::kDisplayOff;
  const Error sent = bus_.Command(std::span<const uint8_t>{command_}.first(std::size_t{3}));
  if (sent == Error::Ok) {
    locked_ = false;
    last_opcode_ = reg::kCommandLock;
    last_len_ = 3U;
  } else {
    shadow_doubt_ = true;
    if (first == Error::Ok) {
      first = sent;
    }
  }
  // The supply enable goes off even when the transfer failed: the supply is what can do harm.
  const bool pin_ok = bus_.GetPort().GpioSet(CtrlPin::VccEnable, GpioSignal::INACTIVE);
  vcc_on_ = !pin_ok; // a pin that could not be driven leaves the supply possibly on
  output_ = Output::Off;
  if (!pin_ok && (first == Error::Ok)) {
    first = Error::Bus;
  }
  if (wait_discharge && (was_on || !pin_ok || (sent != Error::Ok))) {
    // Clamped: the stop path runs before the start too, on a configuration not yet checked.
    detail::DelayUs(bus_.GetPort(), std::min(config_.vcc_off_ms, kWaitMaxMs) * 1000_u32);
  }
  if (first != Error::Ok) {
    detail::Log(bus_.GetPort(), LogLevel::Error, "isf15acp4: power-down step failed");
    return Result<void>::Failure(first);
  }
  detail::Log(bus_.GetPort(), LogLevel::Info, "isf15acp4: display and supply off");
  return Result<void>::Success();
}

template <SpiPort Port>
Result<void> ISF15ACP4<Port>::Reinitialize() noexcept {
  if (Validate(config_) != Error::Ok) {
    // The setters keep a running driver's configuration valid, so this is a configuration given to
    // the constructor, before any start. Should a running driver ever get here, it still powers
    // down rather than leave the panel lit in an unknown state.
    if (initialized_) {
      static_cast<void>(Deinitialize());
    }
    return fail(Error::InvalidArgument, "isf15acp4: invalid Config");
  }
  const Output before = output_;
  Result<void> first = Deinitialize();
  Result<void> r = EnsureInitialized();
  if (first && !r) {
    first = r;
  }
  if (r && (before != Output::Off)) {
    r = (before == Output::Dim) ? DisplayDim() : DisplayOn();
    if (first && !r) {
      first = r;
    }
  }
  return first;
}

template <SpiPort Port>
Result<void> ISF15ACP4<Port>::turnOn(bool dim) noexcept {
  using detail::operator""_u32;
  if (!initialized_) {
    return Result<void>::Failure(Error::NotInitialized);
  }
  if (locked_) {
    return Result<void>::Failure(Error::Locked);
  }
  const Output target = dim ? Output::Dim : Output::Normal;
  if (output_ == target) {
    return Result<void>::Success();
  }
  const bool from_off = (output_ == Output::Off);
  if (!vcc_on_ || from_off) {
    // The controller has been configured and cleared by EnsureInitialized(): the supply may come
    // on. From off the pin is driven again even when `vcc_on_` says on: a failed power-down leaves
    // that flag set for a supply that was never switched on, and the display must not light without
    // it.
    if (auto r = pin(CtrlPin::VccEnable, GpioSignal::ACTIVE); !r) {
      static_cast<void>(safeOff(false));
      return r;
    }
    vcc_on_ = true;
    detail::Log(bus_.GetPort(), LogLevel::Info, "isf15acp4: supply on");
    detail::DelayUs(bus_.GetPort(), std::min(config_.vcc_settle_ms, kWaitMaxMs) * 1000_u32);
  }
  if (auto r = send1(dim ? reg::kDisplayDim : reg::kDisplayOn); !r) {
    static_cast<void>(safeOff(false));
    return r;
  }
  output_ = target;
  if (from_off) {
    detail::DelayUs(bus_.GetPort(), reg::kDisplayOnDelayMs * 1000_u32); // tAF, section 7.9
  }
  return Result<void>::Success();
}

template <SpiPort Port>
Result<void> ISF15ACP4<Port>::DisplayOn() noexcept {
  idle_lowered_ = false;
  touch();
  return turnOn(false);
}

template <SpiPort Port>
Result<void> ISF15ACP4<Port>::DisplayDim() noexcept {
  idle_lowered_ = false;
  touch();
  return turnOn(true);
}

template <SpiPort Port>
Result<void> ISF15ACP4<Port>::DisplayOff() noexcept {
  // Never refused, also before the start: a failed power-down leaves the supply possibly on, and
  // this is the call an application makes to switch it off.
  idle_lowered_ = false;
  return safeOff(false);
}

// -------------------------------------------------------------------------------------------------
// Configuration
// -------------------------------------------------------------------------------------------------

template <SpiPort Port>
Result<void> ISF15ACP4<Port>::ApplyConfig(const Config& config) noexcept {
  if (Validate(config) != Error::Ok) {
    return Result<void>::Failure(Error::InvalidArgument);
  }
  // A vertical scroll runs with 64 MUX only: a variant with fewer rows is refused while one is set.
  if (scroll_set_ && (scroll_.vertical != 0U) &&
      (GeometryOf(config.variant).multiplex != reg::kMuxMax)) {
    return Result<void>::Failure(Error::InvalidArgument);
  }
  if (!initialized_) {
    config_ = config; // applied by the first EnsureInitialized()
    multiplex_ = GeometryOf(config.variant).multiplex;
    configureButton(config.button);
    return Result<void>::Success();
  }
  if (auto r = ready(false); !r) {
    return r;
  }
  if (scrolling_) {
    // The driver's own choice: a whole configuration is not resent under a running scroll. (NKK
    // Table 4, note 2 is about the 27h parameters only; the single setters still run.)
    return Result<void>::Failure(Error::ScrollActive);
  }
  if (auto r = sendConfig(config); !r) {
    return r;
  }
  config_ = config;
  multiplex_ = GeometryOf(config.variant).multiplex;
  configureButton(config.button);
  return Result<void>::Success();
}

template <SpiPort Port>
Result<void> ISF15ACP4<Port>::SetContrast(uint8_t a, uint8_t b, uint8_t c) noexcept {
  if (auto r = ready(false); !r) {
    return r;
  }
  // The dim contrast may not exceed the contrast (NKK notes, Display On/Off): keep it true.
  if ((a < config_.dim.contrast_a) || (b < config_.dim.contrast_b) ||
      (c < config_.dim.contrast_c)) {
    return Result<void>::Failure(Error::InvalidArgument);
  }
  if (auto r = send2(reg::kSetContrastA, a); !r) {
    return r;
  }
  if (auto r = send2(reg::kSetContrastB, b); !r) {
    return r;
  }
  if (auto r = send2(reg::kSetContrastC, c); !r) {
    return r;
  }
  config_.contrast_a = a;
  config_.contrast_b = b;
  config_.contrast_c = c;
  return Result<void>::Success();
}

template <SpiPort Port>
Result<void> ISF15ACP4<Port>::SetMasterCurrent(uint8_t step) noexcept {
  if (auto r = ready(false); !r) {
    return r;
  }
  if (step > reg::kMasterCurrentMax) {
    return Result<void>::Failure(Error::InvalidArgument);
  }
  if (auto r = send2(reg::kMasterCurrent, step); !r) {
    return r;
  }
  config_.master_current = step;
  return Result<void>::Success();
}

template <SpiPort Port>
Result<void> ISF15ACP4<Port>::SetPrechargeSpeed(uint8_t a, uint8_t b, uint8_t c) noexcept {
  if (auto r = ready(false); !r) {
    return r;
  }
  const std::array<uint8_t, 6> bytes{reg::kPrechargeSpeedA, a, reg::kPrechargeSpeedB, b,
                                     reg::kPrechargeSpeedC, c};
  static_cast<void>(std::copy(bytes.begin(), bytes.end(), command_.begin()));
  if (auto r = send(bytes.size()); !r) {
    return r;
  }
  config_.send_precharge_speeds = true; // sent again by a restart
  config_.precharge_speed_a = a;
  config_.precharge_speed_b = b;
  config_.precharge_speed_c = c;
  return Result<void>::Success();
}

template <SpiPort Port>
Result<void> ISF15ACP4<Port>::SetRemap(const Remap& remap) noexcept {
  using detail::operator""_u8;
  if (auto r = ready(false); !r) {
    return r;
  }
  if (!detail::IsEnumerator(remap.depth)) {
    return Result<void>::Failure(Error::InvalidArgument);
  }
  Config next = config_;
  next.remap = remap;
  auto value =
      static_cast<uint32_t>(reg::RemapDepth::Insert(0_u8, detail::ToUnderlying(remap.depth)));
  value |= remap.vertical_increment ? reg::kRemapVertical : 0U;
  value |= remap.column_reverse ? reg::kRemapColumnReverse : 0U;
  value |= remap.reverse_colour_order ? reg::kRemapColourReverse : 0U;
  value |= remap.com_left_right_swap ? reg::kRemapComSwap : 0U;
  value |= remap.com_scan_reverse ? reg::kRemapComScanReverse : 0U;
  value |= remap.com_split_odd_even ? reg::kRemapComSplit : 0U;
  if (auto r = send2(reg::kRemap, static_cast<uint8_t>(value)); !r) {
    return r;
  }
  config_ = next;
  return Result<void>::Success();
}

template <SpiPort Port>
Result<void> ISF15ACP4<Port>::SetStartLine(uint8_t line) noexcept {
  if (auto r = ready(false); !r) {
    return r;
  }
  if (line >= reg::kRamRows) {
    return Result<void>::Failure(Error::InvalidArgument);
  }
  if (auto r = send2(reg::kStartLine, line); !r) {
    return r;
  }
  config_.start_line = line;
  return Result<void>::Success();
}

template <SpiPort Port>
Result<void> ISF15ACP4<Port>::SetDisplayOffset(uint8_t offset) noexcept {
  if (auto r = ready(false); !r) {
    return r;
  }
  if (offset >= reg::kRamRows) {
    return Result<void>::Failure(Error::InvalidArgument);
  }
  if (auto r = send2(reg::kDisplayOffset, offset); !r) {
    return r;
  }
  config_.display_offset = offset;
  return Result<void>::Success();
}

template <SpiPort Port>
Result<void> ISF15ACP4<Port>::SetDisplayMode(DisplayMode mode) noexcept {
  if (auto r = ready(false); !r) {
    return r;
  }
  if (!detail::IsEnumerator(mode)) {
    return Result<void>::Failure(Error::InvalidArgument);
  }
  const auto opcode =
      static_cast<uint8_t>(static_cast<uint32_t>(reg::kModeNormal) + detail::ToUnderlying(mode));
  if (auto r = send1(opcode); !r) {
    return r;
  }
  config_.mode = mode;
  return Result<void>::Success();
}

template <SpiPort Port>
Result<void> ISF15ACP4<Port>::SetMultiplexRatio(uint8_t rows) noexcept {
  if (auto r = ready(false); !r) {
    return r;
  }
  if ((rows < reg::kMuxMin) || (rows > reg::kMuxMax)) {
    return Result<void>::Failure(Error::InvalidArgument);
  }
  if (scroll_set_ && (scroll_.vertical != 0U) && (rows != reg::kMuxMax)) {
    return Result<void>::Failure(Error::InvalidArgument); // vertical scrolling needs 64 MUX
  }
  if (auto r = send2(reg::kMultiplex, static_cast<uint8_t>(rows - 1U)); !r) {
    return r;
  }
  multiplex_ = rows;
  return Result<void>::Success();
}

template <SpiPort Port>
Result<void> ISF15ACP4<Port>::SetDimSettings(const DimSettings& dim) noexcept {
  using detail::operator""_u8;
  if (auto r = ready(false); !r) {
    return r;
  }
  // ACh may not show more than the start-up contrast of each colour (NKK notes, Display On/Off).
  // It is also kept at or below the contrast in force, so the stored configuration stays valid.
  const bool above = (dim.contrast_a > std::min(contrast_cap_[0], config_.contrast_a)) ||
                     (dim.contrast_b > std::min(contrast_cap_[1], config_.contrast_b)) ||
                     (dim.contrast_c > std::min(contrast_cap_[2], config_.contrast_c));
  if ((dim.precharge > reg::kPrechargeSteps) || above) {
    return Result<void>::Failure(Error::InvalidArgument);
  }
  const std::array<uint8_t, 6> bytes{reg::kDimMode,  0_u8,           dim.contrast_a,
                                     dim.contrast_b, dim.contrast_c, dim.precharge};
  static_cast<void>(std::copy(bytes.begin(), bytes.end(), command_.begin()));
  if (auto r = send(bytes.size()); !r) {
    return r;
  }
  config_.dim = dim;
  return Result<void>::Success();
}

template <SpiPort Port>
Result<void> ISF15ACP4<Port>::SetPowerSave(bool enable) noexcept {
  if (auto r = ready(false); !r) {
    return r;
  }
  if (auto r = send2(reg::kPowerSave, enable ? reg::kPowerSaveEnable : reg::kPowerSaveDisable);
      !r) {
    return r;
  }
  config_.power_save = enable;
  return Result<void>::Success();
}

template <SpiPort Port>
Result<void> ISF15ACP4<Port>::SetPhasePeriods(uint8_t phase1, uint8_t phase2) noexcept {
  using detail::operator""_u8;
  if (auto r = ready(false); !r) {
    return r;
  }
  if ((phase1 == 0U) || (phase1 > reg::kPhaseMax) || (phase2 == 0U) || (phase2 > reg::kPhaseMax)) {
    return Result<void>::Failure(Error::InvalidArgument);
  }
  if (auto r = send2(reg::kPhasePeriods,
                     reg::PhaseTwo::Insert(reg::PhaseOne::Insert(0_u8, phase1), phase2));
      !r) {
    return r;
  }
  config_.phase1 = phase1;
  config_.phase2 = phase2;
  return Result<void>::Success();
}

template <SpiPort Port>
Result<void> ISF15ACP4<Port>::SetClock(uint8_t divide, uint8_t frequency) noexcept {
  using detail::operator""_u8;
  if (auto r = ready(false); !r) {
    return r;
  }
  if ((divide > 15U) || (frequency > 15U)) {
    return Result<void>::Failure(Error::InvalidArgument);
  }
  if (auto r = send2(reg::kClockDivider, reg::ClockFrequency::Insert(
                                             reg::ClockDivide::Insert(0_u8, divide), frequency));
      !r) {
    return r;
  }
  config_.clock_divide = divide;
  config_.clock_frequency = frequency;
  return Result<void>::Success();
}

template <SpiPort Port>
Result<void> ISF15ACP4<Port>::SetGrayScaleTable(
    std::span<const uint8_t, reg::kGrayScaleEntries> widths) noexcept {
  if (auto r = ready(false); !r) {
    return r;
  }
  // Pulse widths rise strictly and stay within 125 DCLK (Table 10, B8h notes 4 and 5).
  uint32_t previous = 0U;
  for (const uint8_t w : widths) {
    if ((w == 0U) || (w > reg::kGrayScaleMax) || (static_cast<uint32_t>(w) <= previous)) {
      return Result<void>::Failure(Error::InvalidArgument);
    }
    previous = w;
  }
  command_[0] = reg::kGrayScaleTable;
  static_cast<void>(std::copy(widths.begin(), widths.end(), command_.begin() + 1));
  if (auto r = send(1U + reg::kGrayScaleEntries); !r) {
    return r;
  }
  // The table is not stored: a restart loads the start-up gray scale again (B9h when
  // `linear_gray_scale` is set, NKK step 4.15).
  return Result<void>::Success();
}

template <SpiPort Port>
Result<void> ISF15ACP4<Port>::UseLinearGrayScale() noexcept {
  if (auto r = ready(false); !r) {
    return r;
  }
  if (auto r = send1(reg::kLinearGrayScale); !r) {
    return r;
  }
  config_.linear_gray_scale = true;
  return Result<void>::Success();
}

template <SpiPort Port>
Result<void> ISF15ACP4<Port>::SetPrechargeLevel(uint8_t level) noexcept {
  using detail::operator""_u8;
  if (auto r = ready(false); !r) {
    return r;
  }
  if (level > reg::kPrechargeSteps) {
    return Result<void>::Failure(Error::InvalidArgument);
  }
  if (auto r = send2(reg::kPrechargeLevel, prechargeByte(level, config_)); !r) {
    return r;
  }
  config_.precharge_level = level;
  return Result<void>::Success();
}

template <SpiPort Port>
Result<void> ISF15ACP4<Port>::SetVcomh(uint8_t level) noexcept {
  using detail::operator""_u8;
  if (auto r = ready(false); !r) {
    return r;
  }
  if (level > reg::kPrechargeSteps) {
    return Result<void>::Failure(Error::InvalidArgument);
  }
  if (auto r = send2(reg::kVcomh, reg::Level5::Insert(0_u8, level)); !r) {
    return r;
  }
  config_.vcomh_level = level;
  return Result<void>::Success();
}

template <SpiPort Port>
Result<void> ISF15ACP4<Port>::SetCommandLock(bool lock) noexcept {
  if (!initialized_) {
    return Result<void>::Failure(Error::NotInitialized);
  }
  if (auto r = send2(reg::kCommandLock, lock ? reg::kCommandLockOn : reg::kCommandUnlock); !r) {
    return r;
  }
  locked_ = lock;
  return Result<void>::Success();
}

template <SpiPort Port>
Result<void> ISF15ACP4<Port>::Nop() noexcept {
  if (auto r = ready(false); !r) {
    return r;
  }
  return send1(reg::kNopE3);
}

// -------------------------------------------------------------------------------------------------
// Pixels (SSD1331 sections 7.5, 8.1, 9.1.1, 9.1.2)
// -------------------------------------------------------------------------------------------------

template <SpiPort Port>
Result<void> ISF15ACP4<Port>::checkWindow(uint8_t x, uint8_t y, uint8_t width,
                                          uint8_t height) const noexcept {
  const Geometry g = GeometryOf(config_.variant);
  if ((width == 0U) || (height == 0U) || ((static_cast<uint32_t>(x) + width) > g.width) ||
      ((static_cast<uint32_t>(y) + height) > g.height)) {
    return Result<void>::Failure(Error::InvalidArgument);
  }
  return Result<void>::Success();
}

template <SpiPort Port>
Result<void> ISF15ACP4<Port>::sendWindow(uint8_t col0, uint8_t row0, uint8_t col1,
                                         uint8_t row1) noexcept {
  const std::array<uint8_t, 6> bytes{reg::kSetColumnAddress, col0, col1,
                                     reg::kSetRowAddress,    row0, row1};
  static_cast<void>(std::copy(bytes.begin(), bytes.end(), command_.begin()));
  return send(bytes.size());
}

template <SpiPort Port>
Result<void> ISF15ACP4<Port>::streamPixels(uint8_t width, uint8_t height,
                                           std::span<const Color> colors, Color fill) noexcept {
  const uint32_t w = width;
  const uint32_t h = height;
  const uint32_t total = w * h;
  const bool vertical = config_.remap.vertical_increment;
  const bool blue_first = detail::BlueFirst(config_);
  const ColorDepth depth = config_.remap.depth;
  const bool use_fill = colors.empty();
  std::size_t used = 0U;
  for (uint32_t n = 0U; n < total; ++n) {
    // Vertical increment makes the controller step down a column first (section 9.1.6).
    const uint32_t index = vertical ? (((n % h) * w) + (n / h)) : n;
    const Color color = use_fill ? fill : colors[index];
    const detail::PixelBytes p = detail::EncodePixel(depth, color, blue_first);
    const std::size_t count = p.count;
    if ((used + count) > data_.size()) {
      if (auto r = sendData(used); !r) {
        return r;
      }
      used = 0U;
    }
    for (std::size_t k = 0U; k < count; ++k) {
      data_[used + k] = p.bytes[k];
    }
    used += count;
  }
  if (used != 0U) {
    return sendData(used);
  }
  return Result<void>::Success();
}

template <SpiPort Port>
Result<void> ISF15ACP4<Port>::streamBytes(std::span<const uint8_t> bytes) noexcept {
  std::span<const uint8_t> rest = bytes;
  while (!rest.empty()) {
    const std::size_t n = std::min(rest.size(), data_.size());
    static_cast<void>(
        std::copy(rest.begin(), rest.begin() + static_cast<std::ptrdiff_t>(n), data_.begin()));
    if (auto r = sendData(n); !r) {
      return r;
    }
    rest = rest.subspan(n);
  }
  return Result<void>::Success();
}

template <SpiPort Port>
Result<void> ISF15ACP4<Port>::openWindow(uint8_t x, uint8_t y, uint8_t width,
                                         uint8_t height) noexcept {
  if (auto r = ready(true); !r) {
    return r;
  }
  if (auto r = checkWindow(x, y, width, height); !r) {
    return r;
  }
  const uint32_t origin = GeometryOf(config_.variant).column_origin;
  return sendWindow(static_cast<uint8_t>(origin + x), y,
                    static_cast<uint8_t>(origin + x + width - 1U),
                    static_cast<uint8_t>(y + height - 1U));
}

template <SpiPort Port>
void ISF15ACP4<Port>::windowDone(uint8_t width, uint8_t height) noexcept {
  const Geometry g = GeometryOf(config_.variant);
  if ((width == g.width) && (height == g.height)) {
    ram_rewrite_ = false;
  }
}

template <SpiPort Port>
Result<void> ISF15ACP4<Port>::WriteWindow(uint8_t x, uint8_t y, uint8_t width, uint8_t height,
                                          std::span<const Color> pixels) noexcept {
  if (auto r = ready(true); !r) {
    return r;
  }
  if (auto r = checkWindow(x, y, width, height); !r) {
    return r;
  }
  if (pixels.size() != (static_cast<std::size_t>(width) * height)) {
    return Result<void>::Failure(Error::InvalidArgument);
  }
  if (auto r = openWindow(x, y, width, height); !r) {
    return r;
  }
  if (auto r = streamPixels(width, height, pixels, kBlack); !r) {
    return r;
  }
  windowDone(width, height);
  return Result<void>::Success();
}

template <SpiPort Port>
Result<void> ISF15ACP4<Port>::WriteWindowRaw(uint8_t x, uint8_t y, uint8_t width, uint8_t height,
                                             std::span<const uint8_t> bytes) noexcept {
  if (auto r = ready(true); !r) {
    return r;
  }
  if (auto r = checkWindow(x, y, width, height); !r) {
    return r;
  }
  const std::size_t expected =
      static_cast<std::size_t>(width) * height * detail::BytesPerPixel(config_.remap.depth);
  if (bytes.size() != expected) {
    return Result<void>::Failure(Error::InvalidArgument);
  }
  if (auto r = openWindow(x, y, width, height); !r) {
    return r;
  }
  if (auto r = streamBytes(bytes); !r) {
    return r;
  }
  windowDone(width, height);
  return Result<void>::Success();
}

template <SpiPort Port>
Result<void> ISF15ACP4<Port>::Present(std::span<const Color> frame) noexcept {
  const Geometry g = GeometryOf(config_.variant);
  return WriteWindow(uint8_t{0}, uint8_t{0}, g.width, g.height, frame);
}

template <SpiPort Port>
Result<void> ISF15ACP4<Port>::FillWindow(uint8_t x, uint8_t y, uint8_t width, uint8_t height,
                                         Color color) noexcept {
  if (auto r = openWindow(x, y, width, height); !r) {
    return r;
  }
  if (auto r = streamPixels(width, height, std::span<const Color>{}, color); !r) {
    return r;
  }
  windowDone(width, height);
  return Result<void>::Success();
}

template <SpiPort Port>
Result<void> ISF15ACP4<Port>::SetPixel(uint8_t x, uint8_t y, Color color) noexcept {
  return FillWindow(x, y, uint8_t{1}, uint8_t{1}, color);
}

template <SpiPort Port>
Result<void> ISF15ACP4<Port>::Clear(Color color) noexcept {
  const Geometry g = GeometryOf(config_.variant);
  return FillWindow(uint8_t{0}, uint8_t{0}, g.width, g.height, color);
}

// -------------------------------------------------------------------------------------------------
// Drawing engine (section 9.2)
// -------------------------------------------------------------------------------------------------

template <SpiPort Port>
Result<void> ISF15ACP4<Port>::engineCheck(uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1,
                                          bool ordered) const noexcept {
  if (auto r = ready(true); !r) {
    return r;
  }
  const Geometry g = GeometryOf(config_.variant);
  if ((x0 >= g.width) || (x1 >= g.width) || (y0 >= g.height) || (y1 >= g.height)) {
    return Result<void>::Failure(Error::InvalidArgument);
  }
  if (ordered && ((x1 < x0) || (y1 < y0))) {
    return Result<void>::Failure(Error::InvalidArgument);
  }
  return Result<void>::Success();
}

template <SpiPort Port>
Result<void> ISF15ACP4<Port>::engineEnd() noexcept {
  if (config_.engine_settle_us != 0U) {
    detail::DelayUs(bus_.GetPort(), std::min(config_.engine_settle_us, kEngineSettleMaxUs));
  }
  return Result<void>::Success();
}

template <SpiPort Port>
std::array<uint8_t, 3> ISF15ACP4<Port>::triple(Color color) const noexcept {
  // Colours C, B, A of the engine commands: C and A in bits 5..1, B in bits 5..0 (Table 10).
  const Channels ch = Split(color);
  const bool blue_first = detail::BlueFirst(config_);
  const uint32_t first = blue_first ? ch.blue : ch.red;
  const uint32_t third = blue_first ? ch.red : ch.blue;
  return {static_cast<uint8_t>(first << 1U), ch.green, static_cast<uint8_t>(third << 1U)};
}

template <SpiPort Port>
Result<void> ISF15ACP4<Port>::DrawLine(uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1,
                                       Color color) noexcept {
  if (auto r = engineCheck(x0, y0, x1, y1, false); !r) {
    return r;
  }
  const uint32_t origin = GeometryOf(config_.variant).column_origin;
  const std::array<uint8_t, 3> c = triple(color);
  const std::array<uint8_t, 8> bytes{reg::kDrawLine,
                                     static_cast<uint8_t>(origin + x0),
                                     y0,
                                     static_cast<uint8_t>(origin + x1),
                                     y1,
                                     c[0],
                                     c[1],
                                     c[2]};
  static_cast<void>(std::copy(bytes.begin(), bytes.end(), command_.begin()));
  if (auto r = send(bytes.size()); !r) {
    return r;
  }
  return engineEnd();
}

template <SpiPort Port>
Result<void> ISF15ACP4<Port>::setFillBit(bool filled) noexcept {
  // The fill and reverse-copy bits are shared chip state: set them for this command
  // (section 9.2.6).
  return send2(reg::kFillEnable, filled ? static_cast<uint8_t>(reg::kFillRectangle) : uint8_t{0});
}

template <SpiPort Port>
Result<void> ISF15ACP4<Port>::sendRectangle(uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1,
                                            Color outline, Color fill) noexcept {
  const uint32_t origin = GeometryOf(config_.variant).column_origin;
  const std::array<uint8_t, 3> o = triple(outline);
  const std::array<uint8_t, 3> f = triple(fill);
  const std::array<uint8_t, 11> bytes{reg::kDrawRectangle,
                                      static_cast<uint8_t>(origin + x0),
                                      y0,
                                      static_cast<uint8_t>(origin + x1),
                                      y1,
                                      o[0],
                                      o[1],
                                      o[2],
                                      f[0],
                                      f[1],
                                      f[2]};
  static_cast<void>(std::copy(bytes.begin(), bytes.end(), command_.begin()));
  if (auto r = send(bytes.size()); !r) {
    return r;
  }
  return engineEnd();
}

template <SpiPort Port>
Result<void> ISF15ACP4<Port>::DrawRectangle(uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1,
                                            Color outline) noexcept {
  if (auto r = engineCheck(x0, y0, x1, y1, true); !r) {
    return r;
  }
  if (auto r = setFillBit(false); !r) {
    return r;
  }
  return sendRectangle(x0, y0, x1, y1, outline, kBlack);
}

template <SpiPort Port>
Result<void> ISF15ACP4<Port>::FillRectangle(uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1,
                                            Color outline, Color fill) noexcept {
  if (auto r = engineCheck(x0, y0, x1, y1, true); !r) {
    return r;
  }
  if (auto r = setFillBit(true); !r) {
    return r;
  }
  return sendRectangle(x0, y0, x1, y1, outline, fill);
}

template <SpiPort Port>
Result<void> ISF15ACP4<Port>::CopyWindow(uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1, Point dest,
                                         bool reverse) noexcept {
  if (auto r = engineCheck(x0, y0, x1, y1, true); !r) {
    return r;
  }
  const Geometry g = GeometryOf(config_.variant);
  const uint32_t w = static_cast<uint32_t>(x1 - x0) + 1U;
  const uint32_t h = static_cast<uint32_t>(y1 - y0) + 1U;
  if (((static_cast<uint32_t>(dest.x) + w) > g.width) ||
      ((static_cast<uint32_t>(dest.y) + h) > g.height)) {
    return Result<void>::Failure(Error::InvalidArgument);
  }
  if (auto r =
          send2(reg::kFillEnable, reverse ? static_cast<uint8_t>(reg::kReverseCopy) : uint8_t{0});
      !r) {
    return r;
  }
  const uint32_t origin = g.column_origin;
  const std::array<uint8_t, 7> bytes{
      reg::kCopy, static_cast<uint8_t>(origin + x0),     y0,    static_cast<uint8_t>(origin + x1),
      y1,         static_cast<uint8_t>(origin + dest.x), dest.y};
  static_cast<void>(std::copy(bytes.begin(), bytes.end(), command_.begin()));
  if (auto r = send(bytes.size()); !r) {
    return r;
  }
  return engineEnd();
}

template <SpiPort Port>
Result<void> ISF15ACP4<Port>::windowCommand(uint8_t opcode, uint8_t x0, uint8_t y0, uint8_t x1,
                                            uint8_t y1) noexcept {
  if (auto r = engineCheck(x0, y0, x1, y1, true); !r) {
    return r;
  }
  const uint32_t origin = GeometryOf(config_.variant).column_origin;
  const std::array<uint8_t, 5> bytes{opcode, static_cast<uint8_t>(origin + x0), y0,
                                     static_cast<uint8_t>(origin + x1), y1};
  static_cast<void>(std::copy(bytes.begin(), bytes.end(), command_.begin()));
  if (auto r = send(bytes.size()); !r) {
    return r;
  }
  return engineEnd();
}

template <SpiPort Port>
Result<void> ISF15ACP4<Port>::DimWindow(uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1) noexcept {
  return windowCommand(reg::kDimWindow, x0, y0, x1, y1);
}

template <SpiPort Port>
Result<void> ISF15ACP4<Port>::ClearWindow(uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1) noexcept {
  return windowCommand(reg::kClearWindow, x0, y0, x1, y1);
}

// -------------------------------------------------------------------------------------------------
// Scrolling (sections 9.2.7 to 9.2.9)
// -------------------------------------------------------------------------------------------------

template <SpiPort Port>
Result<void> ISF15ACP4<Port>::SetupScroll(const ScrollSetup& setup) noexcept {
  if (auto r = ready(false); !r) {
    return r;
  }
  if (scrolling_) {
    return Result<void>::Failure(Error::ScrollActive);
  }
  const bool both_zero = (setup.columns == 0U) && (setup.vertical == 0U);
  if (!detail::IsEnumerator(setup.interval) || both_zero ||
      (setup.columns > reg::kScrollColumnsMax) || (setup.vertical > reg::kScrollRowsMax) ||
      (setup.start_row >= reg::kRamRows) || (setup.rows == 0U) ||
      ((static_cast<uint32_t>(setup.start_row) + setup.rows) > reg::kRamRows)) {
    return Result<void>::Failure(Error::InvalidArgument);
  }
  // Vertical scrolling runs with 64 MUX only (Table 10, note 1).
  if ((setup.vertical != 0U) && (multiplex_ != reg::kMuxMax)) {
    return Result<void>::Failure(Error::InvalidArgument);
  }
  const std::array<uint8_t, 6> bytes{
      reg::kScrollSetup, setup.columns,
      setup.start_row,   setup.rows,
      setup.vertical,    static_cast<uint8_t>(detail::ToUnderlying(setup.interval))};
  static_cast<void>(std::copy(bytes.begin(), bytes.end(), command_.begin()));
  if (auto r = send(bytes.size()); !r) {
    return r;
  }
  scroll_ = setup;
  scroll_set_ = true;
  return Result<void>::Success();
}

template <SpiPort Port>
Result<void> ISF15ACP4<Port>::ActivateScroll() noexcept {
  if (auto r = ready(false); !r) {
    return r;
  }
  if (!scroll_set_) {
    return Result<void>::Failure(Error::ScrollNotSet);
  }
  // A vertical setup is only kept while the ratio is 64 (SetupScroll, SetMultiplexRatio,
  // ApplyConfig).
  if (auto r = send1(reg::kScrollOn); !r) {
    return r;
  }
  scrolling_ = true;
  return Result<void>::Success();
}

template <SpiPort Port>
Result<void> ISF15ACP4<Port>::DeactivateScroll() noexcept {
  if (auto r = ready(false); !r) {
    return r;
  }
  if (auto r = send1(reg::kScrollOff); !r) {
    return r;
  }
  scrolling_ = false;
  ram_rewrite_ = true; // the RAM content must be rewritten (section 9.2.8)
  return Result<void>::Success();
}

// -------------------------------------------------------------------------------------------------
// Switch
// -------------------------------------------------------------------------------------------------

template <SpiPort Port>
void ISF15ACP4<Port>::touch() noexcept {
  if constexpr (detail::HasNowMs<Port>) {
    last_activity_ms_ = bus_.GetPort().NowMs();
    activity_valid_ = true;
  }
}

template <SpiPort Port>
Result<void> ISF15ACP4<Port>::NotifyActivity() noexcept {
  if constexpr (detail::HasNowMs<Port>) {
    touch();
    return Result<void>::Success();
  } else {
    return Result<void>::Failure(Error::Unsupported);
  }
}

template <SpiPort Port>
Result<bool> ISF15ACP4<Port>::ReadSwitchRaw() noexcept {
  if constexpr (detail::HasReadSwitch<Port>) {
    if (!HasSwitch(config_.variant)) {
      return Result<bool>::Failure(Error::Unsupported); // the display-only module has no switch
    }
    bool high = false;
    if (!bus_.GetPort().ReadSwitch(high)) {
      return Result<bool>::Failure(Error::Bus);
    }
    return Result<bool>::Success(config_.button.active_low ? !high : high);
  } else {
    return Result<bool>::Failure(Error::Unsupported);
  }
}

template <SpiPort Port>
Result<void> ISF15ACP4<Port>::SetButtonConfig(const ButtonConfig& config) noexcept {
  if (Validate(config) != Error::Ok) {
    return Result<void>::Failure(Error::InvalidArgument);
  }
  config_.button = config;
  button_.Configure(config);
  return Result<void>::Success();
}

template <SpiPort Port>
Result<void> ISF15ACP4<Port>::applyIdle(uint32_t idle) noexcept {
  if ((config_.idle_sleep_ms != 0U) && (idle >= config_.idle_sleep_ms) &&
      (output_ != Output::Off)) {
    if (auto r = safeOff(false); !r) {
      return r;
    }
    idle_lowered_ = true;
  } else if ((config_.idle_dim_ms != 0U) && (idle >= config_.idle_dim_ms) &&
             (output_ == Output::Normal)) {
    if (auto r = turnOn(true); !r) {
      return r;
    }
    idle_lowered_ = true;
  } else {
    // No idle timer has expired.
  }
  return Result<void>::Success();
}

template <SpiPort Port>
void ISF15ACP4<Port>::configureButton(const ButtonConfig& b) noexcept {
  // Only a changed switch configuration resets the debouncer: a press in progress survives an
  // ApplyConfig() or Reinitialize() that leaves it alone.
  if (!SameButtonConfig(b, button_.GetConfig())) {
    button_.Configure(b);
  }
}

template <SpiPort Port>
bool ISF15ACP4<Port>::wakeWanted(const ButtonUpdate& u) const noexcept {
  // A press wakes only a display the idle timers lowered, and never a locked controller.
  return (u.event == ButtonEvent::Pressed) && idle_lowered_ && initialized_ && !locked_;
}

template <SpiPort Port>
Result<ButtonUpdate> ISF15ACP4<Port>::Poll() noexcept {
  if constexpr (detail::HasReadSwitch<Port> && detail::HasNowMs<Port>) {
    const Result<bool> level = ReadSwitchRaw();
    if (!level) {
      return Result<ButtonUpdate>::Failure(level.error);
    }
    const uint32_t t = bus_.GetPort().NowMs();
    if (!activity_valid_) {
      last_activity_ms_ = t;
      activity_valid_ = true;
    }
    ButtonUpdate u = button_.Update(level.value, t);
    if ((u.event != ButtonEvent::None) || button_.IsDown()) {
      last_activity_ms_ = t;
    }
    if (wakeWanted(u)) {
      // A press wakes a display that the idle timers lowered.
      if (auto r = DisplayOn(); !r) {
        return Result<ButtonUpdate>::Failure(r.error);
      }
      u.woke_display = true;
      return Result<ButtonUpdate>::Success(u);
    }
    if (initialized_ && !locked_ && (u.event == ButtonEvent::None)) {
      if (auto r = applyIdle(t - last_activity_ms_); !r) {
        return Result<ButtonUpdate>::Failure(r.error);
      }
    }
    return Result<ButtonUpdate>::Success(u);
  } else {
    return Result<ButtonUpdate>::Failure(Error::Unsupported);
  }
}

// -------------------------------------------------------------------------------------------------
// Raw access and transfers
// -------------------------------------------------------------------------------------------------

template <SpiPort Port>
Result<void> ISF15ACP4<Port>::TraceCommand(uint8_t opcode,
                                           std::span<const uint8_t> parameters) noexcept {
  if (!initialized_) {
    return Result<void>::Failure(Error::NotInitialized);
  }
  if (!rawCommandValid(opcode, parameters)) {
    return Result<void>::Failure(Error::InvalidArgument);
  }
  if (locked_ && (opcode != reg::kCommandLock)) {
    return Result<void>::Failure(Error::Locked);
  }
  if (((opcode == reg::kDisplayOn) || (opcode == reg::kDisplayDim)) && !vcc_on_) {
    return Result<void>::Failure(Error::SupplyOff);
  }
  command_[0] = opcode;
  static_cast<void>(std::copy(parameters.begin(), parameters.end(), command_.begin() + 1));
  shadow_doubt_ = true; // the driver cannot tell what a raw command changed
  if (auto r = send(1U + parameters.size()); !r) {
    return r;
  }
  if (opcode == reg::kCommandLock) {
    locked_ = (parameters[0] == reg::kCommandLockOn);
  }
  trackRawScroll(opcode);
  return Result<void>::Success();
}

template <SpiPort Port>
bool ISF15ACP4<Port>::rawCommandValid(uint8_t opcode,
                                      std::span<const uint8_t> parameters) noexcept {
  const int32_t expected = detail::ParameterCount(opcode);
  if ((expected < 0) || (parameters.size() != static_cast<std::size_t>(expected))) {
    return false;
  }
  // FDh takes only its two defined parameters: the controller locks on bit 2, which the shadow
  // could not follow for any other value (Table 10).
  if ((opcode == reg::kCommandLock) && (parameters[0] != reg::kCommandLockOn) &&
      (parameters[0] != reg::kCommandUnlock)) {
    return false;
  }
  // ADh: A[0] must be 0 (Table 10); 8Eh is the only value the module uses (NKK step 4.11).
  return (opcode != reg::kMasterConfig) || (parameters[0] == reg::kMasterConfigExternalVcc);
}

template <SpiPort Port>
void ISF15ACP4<Port>::trackRawScroll(uint8_t opcode) noexcept {
  // A raw 2Fh or 2Eh keeps IsScrolling() in step, so the RAM guards still apply.
  if (opcode == reg::kScrollOn) {
    scrolling_ = true;
  } else if (opcode == reg::kScrollOff) {
    scrolling_ = false;
    ram_rewrite_ = true; // the RAM content must be rewritten (section 9.2.8)
  } else {
    // other opcodes do not change the scroll state
  }
}

template <SpiPort Port>
Result<void> ISF15ACP4<Port>::ready(bool ram) const noexcept {
  if (!initialized_) {
    return Result<void>::Failure(Error::NotInitialized);
  }
  if (locked_) {
    return Result<void>::Failure(Error::Locked);
  }
  if (ram && scrolling_ && !config_.writes_while_scrolling) {
    return Result<void>::Failure(Error::ScrollActive);
  }
  return Result<void>::Success();
}

template <SpiPort Port>
Result<void> ISF15ACP4<Port>::send(std::size_t length) noexcept {
  const Error e = bus_.Command(std::span<const uint8_t>{command_}.first(length));
  if (e != Error::Ok) {
    shadow_doubt_ = true; // the transfer may have been partly clocked out
    return fail(e, "isf15acp4: command transfer failed");
  }
  last_opcode_ = command_[0];
  last_len_ = length;
  return Result<void>::Success();
}

template <SpiPort Port>
Result<void> ISF15ACP4<Port>::send1(uint8_t opcode) noexcept {
  command_[0] = opcode;
  return send(std::size_t{1});
}

template <SpiPort Port>
Result<void> ISF15ACP4<Port>::send2(uint8_t opcode, uint8_t a) noexcept {
  command_[0] = opcode;
  command_[1] = a;
  return send(std::size_t{2});
}

template <SpiPort Port>
Result<void> ISF15ACP4<Port>::sendData(std::size_t length) noexcept {
  const Error e = bus_.Data(std::span<const uint8_t>{data_}.first(length));
  if (e != Error::Ok) {
    shadow_doubt_ = true;
    return fail(e, "isf15acp4: data transfer failed");
  }
  last_len_ = length;
  return Result<void>::Success();
}

template <SpiPort Port>
Result<void> ISF15ACP4<Port>::pin(CtrlPin which, GpioSignal level) noexcept {
  if (!bus_.GetPort().GpioSet(which, level)) {
    return fail(Error::Bus, "isf15acp4: a control pin could not be driven");
  }
  return Result<void>::Success();
}

template <SpiPort Port>
Result<void> ISF15ACP4<Port>::fail(Error error, const char* message) noexcept {
  detail::Log(bus_.GetPort(), LogLevel::Error, message);
  return Result<void>::Failure(error);
}

} // namespace isf15acp4

#endif // ISF15ACP4_IPP
