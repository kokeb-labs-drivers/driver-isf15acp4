/**
 * @file isf15acp4_button.hpp
 * @brief The switch debouncer: a time-injected state machine with press, release, long-press and
 * repeat events.
 * @copyright Copyright (c) 2026 The driver-isf15acp4 authors. SPDX-License-Identifier: MIT
 *
 * @details
 * The switch is an SPST normally-open contact (NKK sheet E4: "Leave actuator: 1 - 2 OFF, push
 * actuator: 1 - 2 ON"). The sheet gives no bounce time, so the debounce time is `ButtonConfig`'s
 * knob. `ButtonDebouncer` has no port, no clock and no I/O: the caller passes the sampled level and
 * a millisecond timestamp. All time arithmetic is unsigned subtraction, so a wrapping 32-bit
 * counter is handled.
 */
#ifndef ISF15ACP4_BUTTON_HPP
#define ISF15ACP4_BUTTON_HPP

#include <cstdint>

#include "isf15acp4_types.hpp"

namespace isf15acp4 {

/** @brief One result of `ButtonDebouncer::Update()`. */
struct ButtonUpdate {
  ButtonEvent event{ButtonEvent::None}; ///< The event, or `None`.
  uint32_t timestamp_ms{
      0}; ///< When the level first changed (`Pressed`, `Released`), else the poll time.
  uint32_t held_ms{0};  ///< `Released`, `LongPress` and `Repeat`: how long the switch was down.
  bool was_long{false}; ///< `Released`: the press had reached `long_press_ms`.
  bool woke_display{
      false}; ///< Set by the driver's `Poll()`: this press woke a dimmed or sleeping display.
};

/**
 * @brief Debounces a switch sampled at any rate and classifies the press.
 *
 * A level change is accepted once the new level has been seen continuously for `debounce_ms`
 * (a sample of the old level in between starts the wait again). `Pressed` and `Released` carry the
 * time of the first sample of the new level, so the debounce delay is not in the timestamp. Poll at
 * least twice per debounce time: a press shorter than the poll period is not seen. One event is
 * returned per `Update()`; a second condition that is due stays due and is returned by the next
 * call.
 */
class ButtonDebouncer {
public:
  /**
   * @brief A debouncer with the given configuration, switch open.
   * @param config The configuration; the caller has checked it with `Validate()`.
   */
  explicit constexpr ButtonDebouncer(const ButtonConfig& config = {}) noexcept : config_(config) {}

  /**
   * @brief Take a new configuration and forget the state (the next sample is adopted again).
   * @param config The configuration; the caller has checked it with `Validate()`.
   */
  constexpr void Configure(const ButtonConfig& config) noexcept {
    config_ = config;
    Reset();
  }

  /** @return The configuration in use. No state change. */
  [[nodiscard]] constexpr const ButtonConfig& GetConfig() const noexcept {
    return config_;
  }

  /** @brief Forget the state: the next `Update()` adopts its level without an event. */
  constexpr void Reset() noexcept {
    started_ = false;
    pressed_ = false;
    changing_ = false;
    long_sent_ = false;
    repeat_armed_ = false;
  }

  /**
   * @brief Feed one sample.
   * @param down true when the switch is closed (the caller applied the polarity).
   * @param now_ms A millisecond counter; may wrap.
   * @return The event of this sample. The first sample after construction or `Reset()` sets the
   *         level and returns `None`, also when the switch is already down.
   */
  constexpr ButtonUpdate Update(bool down, uint32_t now_ms) noexcept {
    ButtonUpdate u{};
    u.timestamp_ms = now_ms;
    if (!started_) {
      started_ = true;
      pressed_ = down;
      pressed_at_ms_ = now_ms;
      changing_ = false;
      return u;
    }
    if (down != pressed_) {
      if (!changing_) {
        changing_ = true;
        changed_at_ms_ = now_ms;
      }
      if ((now_ms - changed_at_ms_) >= config_.debounce_ms) {
        return accept(down, now_ms);
      }
      return u;
    }
    changing_ = false;
    if (pressed_) {
      return holdEvent(now_ms);
    }
    return u;
  }

  /** @return The debounced level: true while the switch is down. */
  [[nodiscard]] constexpr bool IsDown() const noexcept {
    return pressed_;
  }
  /** @return Presses counted since construction (wraps at 2^32). */
  [[nodiscard]] constexpr uint32_t PressCount() const noexcept {
    return press_count_;
  }

private:
  constexpr ButtonUpdate accept(bool down, uint32_t now_ms) noexcept {
    ButtonUpdate u{};
    u.timestamp_ms = changed_at_ms_;
    changing_ = false;
    pressed_ = down;
    if (down) {
      pressed_at_ms_ = changed_at_ms_;
      long_sent_ = false;
      repeat_armed_ = (config_.repeat_interval_ms != 0U);
      repeat_due_ms_ = config_.repeat_delay_ms;
      press_count_ = press_count_ + 1U;
      u.event = ButtonEvent::Pressed;
    } else {
      u.event = ButtonEvent::Released;
      u.held_ms = changed_at_ms_ - pressed_at_ms_;
      u.was_long = long_sent_;
      repeat_armed_ = false;
    }
    static_cast<void>(now_ms);
    return u;
  }

  constexpr ButtonUpdate holdEvent(uint32_t now_ms) noexcept {
    ButtonUpdate u{};
    u.timestamp_ms = now_ms;
    const uint32_t held = now_ms - pressed_at_ms_;
    u.held_ms = held;
    if (!long_sent_ && (held >= config_.long_press_ms)) {
      long_sent_ = true;
      u.event = ButtonEvent::LongPress;
    } else if (repeat_armed_ && (held >= repeat_due_ms_)) {
      // The next repeat is one interval after this poll: a slow poll never bursts.
      repeat_due_ms_ = held + config_.repeat_interval_ms;
      u.event = ButtonEvent::Repeat;
    } else {
      // Nothing is due.
    }
    return u;
  }

  ButtonConfig config_;
  uint32_t pressed_at_ms_{0};
  uint32_t changed_at_ms_{0};
  uint32_t repeat_due_ms_{0}; ///< Hold time at which the next `Repeat` is due.
  uint32_t press_count_{0};
  bool started_{false};
  bool pressed_{false};
  bool changing_{false};
  bool long_sent_{false};
  bool repeat_armed_{false};
};

namespace detail {
/// A short press is accepted after exactly the debounce time and reported at its first sample.
constexpr bool DebounceSelfCheck() noexcept {
  ButtonConfig cfg{};
  cfg.debounce_ms = 10;
  ButtonDebouncer d(cfg);
  static_cast<void>(d.Update(false, 0U));
  static_cast<void>(d.Update(true, 100U));
  const ButtonUpdate early = d.Update(true, 109U);
  const ButtonUpdate edge = d.Update(true, 110U);
  return (early.event == ButtonEvent::None) && (edge.event == ButtonEvent::Pressed) &&
         (edge.timestamp_ms == 100U);
}
static_assert(DebounceSelfCheck(),
              "debounce: accepted after debounce_ms, stamped at the first sample");
} // namespace detail

} // namespace isf15acp4

#endif // ISF15ACP4_BUTTON_HPP
