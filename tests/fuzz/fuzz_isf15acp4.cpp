/**
 * @file fuzz_isf15acp4.cpp
 * @brief Fuzz target: the driver over the display-controller model, with a fuzzer that chooses the
 * calls, their arguments, the switch level, the passing time and the faults.
 *
 * The bytes choose a configuration (often an invalid one) and then a sequence of driver calls with
 * arbitrary arguments. Between calls they let time pass, flip the switch, fail a transfer or a pin
 * write, and reset the controller behind the driver's back (a brown-out: the write-only bus gives
 * no way to see it).
 *
 * After every call the target checks the promises of docs/contract.md and aborts, naming the line,
 * when one breaks:
 *
 * - a call returns an `Error` the enum defines;
 * - `InvalidArgument` means nothing was sent: no transfer, no pin change;
 * - before `EnsureInitialized()` succeeds, no call but the lifecycle calls makes a transfer;
 * - the model records no violation: no supply before the controller is configured, no display on
 *   without supply, no byte during reset, no unknown opcode, no parameter out of range. The only
 *   allowed ones are a supply forced off after a failed display-off transfer, a display-on of a
 *   controller that browned out, and a drawing command into a busy engine when the model's engine
 *   is slower than the configured wait (the model is set to the configured wait here);
 * - a power-down with a working bus ends with the supply off and the display off.
 *
 * Built two ways (tests/fuzz/CMakeLists.txt): with libFuzzer (Clang,
 * `-DISF15ACP4_BUILD_FUZZERS=ON`), and as a plain executable that replays the corpus in
 * tests/fuzz/corpus/ as a CTest on any compiler.
 */
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>

#include "isf15acp4.hpp"
#include "mock_isf15acp4_bus.hpp"

namespace {

namespace is = ::isf15acp4;
using isf15acp4_test::FullPort;

/// The fuzzer's bytes, read front to back; zeros once they run out.
class Input {
public:
  Input(const uint8_t* data, std::size_t size) noexcept : data_(data), size_(size) {}

  [[nodiscard]] bool Empty() const noexcept {
    return pos_ >= size_;
  }
  uint8_t Byte() noexcept {
    return pos_ < size_ ? data_[pos_++] : uint8_t{0};
  }
  uint16_t Word() noexcept {
    const auto high = Byte();
    return static_cast<uint16_t>((static_cast<uint32_t>(high) << 8U) | Byte());
  }

private:
  const uint8_t* data_;
  std::size_t size_;
  std::size_t pos_ = 0;
};

/// A broken promise: name the line and abort, so libFuzzer saves the input as a crash.
void Check(bool promise, int line = __builtin_LINE()) noexcept {
  if (!promise) {
    static_cast<void>(std::fprintf(stderr, "fuzz_isf15acp4: promise broken at line %d\n", line));
    std::abort();
  }
}

bool KnownError(is::Error e) noexcept {
  const int v = static_cast<int>(e);
  return ((v >= 0) && (v <= 9)) || ((v >= 16) && (v <= 19));
}

is::Config MakeConfig(Input& in) noexcept {
  is::Config c = is::PresetFor(static_cast<is::Variant>(in.Byte() % 4U)); // 3 is not an enumerator
  const uint8_t flags = in.Byte();
  c.remap.vertical_increment = ((flags & 1U) != 0U);
  c.remap.reverse_colour_order = ((flags & 2U) != 0U);
  c.remap.depth = static_cast<is::ColorDepth>((flags >> 2U) % 4U);
  c.red_on_segment_a = ((flags & 0x10U) == 0U);
  c.button.active_low = ((flags & 0x20U) == 0U);
  const uint8_t damage = in.Byte();
  if ((damage & 0x80U) != 0U) { // a configuration that may be invalid
    c.master_current = static_cast<uint8_t>(in.Byte() % 20U);
    c.phase1 = static_cast<uint8_t>(in.Byte() % 18U);
    c.vcc_settle_ms = static_cast<uint32_t>(in.Byte()) * 8U;
    c.engine_settle_us = static_cast<uint32_t>(in.Byte()) * 40U;
    c.button.debounce_ms = in.Byte() % 60U;
    c.button.long_press_ms = in.Byte() % 300U;
  } else {
    c.engine_settle_us = static_cast<uint32_t>(in.Byte()) * 40U;
    c.button.debounce_ms = 1U + (in.Byte() % 40U);
    c.button.long_press_ms = c.button.debounce_ms + 1U + (in.Byte() % 300U);
    c.idle_dim_ms = static_cast<uint32_t>(in.Byte() % 8U) * 500U;
    c.idle_sleep_ms = (c.idle_dim_ms == 0U)
                          ? 0U
                          : (c.idle_dim_ms + (static_cast<uint32_t>(in.Byte() % 8U) * 500U) + 1U);
    c.button.repeat_interval_ms = (in.Byte() % 2U) * 40U;
  }
  return c;
}

is::Color ColorOf(Input& in) noexcept {
  return is::Color{in.Word()};
}

/// What the fuzzer is allowed to see go wrong, per `Run()`.
struct Run {
  FullPort port;
  is::ISF15ACP4<FullPort> drv;
  bool browned{false};
  uint32_t browned_allowed{
      0}; ///< Violations a brown-out excuses; the model's bits stay set afterwards.
  explicit Run(const is::Config& c) : drv(port, c) {}
};

const std::array<is::Color, 6144> kFrame = [] {
  std::array<is::Color, 6144> frame{};
  for (std::size_t i = 0; i < frame.size(); ++i) {
    frame[i] = is::Color{static_cast<uint16_t>(i * 0x9E37U)};
  }
  return frame;
}();

} // namespace

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, std::size_t size) {
  Input in(data, size);
  const is::Config config = MakeConfig(in);
  Run run(config);
  auto& rig = run.port.rig;
  rig.chip.engine_busy_us = config.engine_settle_us;
  rig.chip.segment_a_is_red = config.red_on_segment_a;
  int calls = 0;
  while (!in.Empty() && (calls < 300)) {
    ++calls;
    const uint8_t op = in.Byte();
    const uint8_t fault = in.Byte();
    // Faults: fail a transfer a few calls ahead, or a pin, or none.
    rig.fail_write_at = ((fault & 0x03U) == 1U) ? (rig.writes + static_cast<int>(fault >> 2U)) : -1;
    rig.fail_vcc_on = ((fault & 0x03U) == 2U) && ((fault & 0x40U) != 0U);
    rig.fail_vcc_off = ((fault & 0x03U) == 2U) && ((fault & 0x80U) != 0U);
    rig.fail_reset_pin = ((fault & 0x03U) == 3U) && ((fault & 0x20U) != 0U);
    rig.fail_switch = ((fault & 0x03U) == 3U) && ((fault & 0x10U) != 0U);
    rig.switch_high = ((fault & 0x08U) != 0U);
    rig.chip.Elapse(static_cast<uint32_t>(in.Byte()) * 1000U);

    const int writes = rig.writes;
    const int gpio = rig.gpio_calls;
    const bool started = run.drv.IsInitialized();
    bool lifecycle = false;
    is::Error e = is::Error::Ok;
    const uint8_t x = in.Byte();
    const uint8_t y = in.Byte();
    const uint8_t w = in.Byte();
    const uint8_t h = in.Byte();
    switch (op % 34U) {
    case 0:
      e = run.drv.EnsureInitialized().error;
      lifecycle = true;
      break;
    case 1:
      e = run.drv.DisplayOn().error;
      break;
    case 2:
      e = run.drv.DisplayDim().error;
      break;
    case 3:
      e = run.drv.DisplayOff().error;
      break;
    case 4:
      e = run.drv.Deinitialize().error;
      lifecycle = true;
      break;
    case 5:
      e = run.drv.Reinitialize().error;
      lifecycle = true;
      break;
    case 6:
      e = run.drv.Clear(ColorOf(in)).error;
      break;
    case 7:
      e = run.drv.SetPixel(x, y, ColorOf(in)).error;
      break;
    case 8:
      e = run.drv
              .FillWindow(x, y, static_cast<uint8_t>(w % 40U), static_cast<uint8_t>(h % 40U),
                          ColorOf(in))
              .error;
      break;
    case 9: {
      const std::size_t n = (static_cast<std::size_t>(w % 12U)) * (h % 12U);
      e = run.drv
              .WriteWindow(x, y, static_cast<uint8_t>(w % 12U), static_cast<uint8_t>(h % 12U),
                           std::span<const is::Color>(kFrame.data(), n + (in.Byte() % 2U)))
              .error;
      break;
    }
    case 10:
      e = run.drv.DrawLine(x, y, w, h, ColorOf(in)).error;
      break;
    case 11:
      e = run.drv.DrawRectangle(x, y, w, h, ColorOf(in)).error;
      break;
    case 12:
      e = run.drv.FillRectangle(x, y, w, h, ColorOf(in), ColorOf(in)).error;
      break;
    case 13: {
      const uint8_t dx = in.Byte();
      const uint8_t dy = in.Byte();
      const bool reverse = (in.Byte() % 2U) != 0U;
      e = run.drv.CopyWindow(x, y, w, h, is::Point{dx, dy}, reverse).error;
      break;
    }
    case 14:
      e = run.drv.DimWindow(x, y, w, h).error;
      break;
    case 15:
      e = run.drv.ClearWindow(x, y, w, h).error;
      break;
    case 16: {
      is::ScrollSetup s{};
      s.columns = static_cast<uint8_t>(x % 100U);
      s.start_row = static_cast<uint8_t>(y % 70U);
      s.rows = static_cast<uint8_t>(w % 70U);
      s.vertical = static_cast<uint8_t>(h % 66U);
      s.interval = static_cast<is::ScrollInterval>(in.Byte() % 5U);
      e = run.drv.SetupScroll(s).error;
      break;
    }
    case 17:
      e = run.drv.ActivateScroll().error;
      break;
    case 18:
      e = run.drv.DeactivateScroll().error;
      break;
    case 19:
      e = run.drv.SetContrast(x, y, w).error;
      break;
    case 20:
      e = run.drv.SetMasterCurrent(static_cast<uint8_t>(x % 20U)).error;
      break;
    case 21:
      e = run.drv.SetDisplayMode(static_cast<is::DisplayMode>(x % 5U)).error;
      break;
    case 22:
      e = run.drv.SetMultiplexRatio(static_cast<uint8_t>(x % 70U)).error;
      break;
    case 23:
      e = run.drv.SetCommandLock((x % 2U) != 0U).error;
      break;
    case 24: {
      std::array<uint8_t, 12> p{};
      for (auto& b : p) {
        b = in.Byte();
      }
      e = run.drv
              .TraceCommand(x,
                            std::span<const uint8_t>(p.data(), static_cast<std::size_t>(w % 12U)))
              .error;
      break;
    }
    case 25: {
      const auto r = run.drv.Poll();
      e = r.error;
      Check(r.ok() || (r.value.event == is::ButtonEvent::None));
      break;
    }
    case 26: {
      is::ButtonConfig b{};
      b.debounce_ms = x % 50U;
      b.long_press_ms = static_cast<uint32_t>(y) * 4U;
      b.repeat_interval_ms = (static_cast<uint32_t>(w) % 3U) * 30U;
      e = run.drv.SetButtonConfig(b).error;
      break;
    }
    case 27:
      e = run.drv.ApplyConfig(MakeConfig(in)).error;
      if (e == is::Error::Ok) {
        // The model's busy time is the one the configuration assumes (the engine settle knob), so
        // it follows a new configuration as the driver does.
        rig.chip.engine_busy_us = run.drv.GetConfig().engine_settle_us;
        rig.chip.segment_a_is_red = run.drv.GetConfig().red_on_segment_a;
      }
      break;
    case 28: {
      is::Remap r{};
      r.vertical_increment = ((x & 1U) != 0U);
      r.reverse_colour_order = ((x & 2U) != 0U);
      r.depth = static_cast<is::ColorDepth>((x >> 2U) % 4U);
      e = run.drv.SetRemap(r).error;
      break;
    }
    case 29:
      e = run.drv.SetPhasePeriods(static_cast<uint8_t>(x % 18U), static_cast<uint8_t>(y % 18U))
              .error;
      break;
    case 30:
      e = run.drv.NotifyActivity().error;
      break;
    case 31:
      rig.chip.BrownOut(); // the controller resets by itself; the bus cannot tell
      run.browned = true;
      break;
    case 32:
      e = run.drv.SetPrechargeLevel(static_cast<uint8_t>(x % 34U)).error;
      break;
    default:
      if ((x % 2U) == 0U) {
        e = run.drv.SetupScroll(is::ScrollSetup{}).error;
      } else {
        e = run.drv
                .SetDimSettings(is::DimSettings{.contrast_a = y,
                                                .contrast_b = w,
                                                .contrast_c = h,
                                                .precharge = static_cast<uint8_t>(x % 34U)})
                .error;
      }
      break;
    }
    Check(KnownError(e));
    if (e == is::Error::InvalidArgument) {
      if (rig.writes != writes) {
        static_cast<void>(
            std::fprintf(stderr, "fuzz_isf15acp4: InvalidArgument after a transfer, call %u\n",
                         static_cast<unsigned>(op % 34U)));
      }
      Check(rig.writes == writes);
      Check(rig.gpio_calls == gpio);
    }
    // DisplayOff() (call 3) is the one call that is performed before the start: it is the
    // power-down.
    if (!started && !lifecycle && ((op % 34U) != 3U)) {
      Check(rig.writes == writes);
      Check(rig.gpio_calls == gpio);
    }
    if (((op % 34U) == 0U) && (e == is::Error::Ok)) {
      run.browned = false;
    }
    if (((op % 34U) == 5U) && (e == is::Error::Ok)) {
      run.browned = false;
    }
    // The model's rules.
    uint32_t allowed = 0U;
    if (rig.failed_writes > 0) {
      allowed |= isf15acp4_test::kVccOffWhileOn;
    }
    if (run.browned) {
      // A reset the bus cannot see leaves the controller unconfigured and its RAM undefined, so the
      // next turn-on legitimately breaks both rules; Reinitialize() is the documented recovery, and
      // the model's violation bits are sticky.
      run.browned_allowed = isf15acp4_test::kOnWithUndefinedRam | isf15acp4_test::kVccBeforeConfig;
    }
    allowed |= run.browned_allowed;
    Check((rig.chip.Violations() & ~allowed) == 0U);
    // The setters keep the stored configuration valid, so a restart can always run.
    if (run.drv.IsInitialized()) {
      Check(is::Validate(run.drv.GetConfig()) == is::Error::Ok);
    }
    // The supply is never on for a driver that was never started.
    if (!started && !run.drv.IsInitialized() && !lifecycle) {
      Check(!rig.chip.Vcc() || run.drv.IsSupplyEnabled() ||
            (rig.fail_vcc_off || (rig.failed_writes > 0)));
    }
  }
  // The power-down with everything working again ends with the supply off.
  rig.fail_write_at = -1;
  rig.fail_writes_from = -1;
  rig.fail_vcc_on = false;
  rig.fail_vcc_off = false;
  rig.fail_reset_pin = false;
  Check(run.drv.Deinitialize().ok());
  Check(!rig.chip.Vcc());
  Check(rig.chip.DisplayState() == isf15acp4_test::Display::Off);
  return 0;
}
