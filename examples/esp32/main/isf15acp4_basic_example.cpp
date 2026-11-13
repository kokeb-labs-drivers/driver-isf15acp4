/**
 * @file isf15acp4_basic_example.cpp
 * @brief Start the ISF15ACP4 from the menuconfig values, draw a test picture and follow the button:
 * a press shows a counter, a long press inverts the display, the idle timers dim and sleep it.
 *
 * Wiring: examples/esp32/README.md. Every Result is checked and logged by name.
 */
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp32_isf15acp4_port.hpp"
#include "isf15acp4.hpp"
#include "isf15acp4_kconfig.hpp"

namespace {
constexpr const char* kTag = "isf15acp4_example";

/// Draw the test picture: a frame, the diagonals, and a block whose width shows the press count.
void Draw(isf15acp4::ISF15ACP4<Esp32Isf15acp4Port>& panel, uint32_t presses) {
  const isf15acp4::Geometry g = panel.GetGeometry();
  const uint8_t right = static_cast<uint8_t>(g.width - 1U);
  const uint8_t bottom = static_cast<uint8_t>(g.height - 1U);
  const uint8_t bar = static_cast<uint8_t>(((presses % 8U) * 8U) + 4U);
  if (!panel.Clear(isf15acp4::kBlack) ||
      !panel.DrawRectangle(0, 0, right, bottom, isf15acp4::kWhite) ||
      !panel.DrawLine(0, 0, right, bottom, isf15acp4::kRed) ||
      !panel.DrawLine(0, bottom, right, 0, isf15acp4::kBlue) ||
      !panel.FillRectangle(4, 4, static_cast<uint8_t>(4U + (bar % (g.width - 8U))), 12,
                           isf15acp4::kGreen, isf15acp4::kGreen)) {
    ESP_LOGW(kTag, "drawing failed");
  }
}
} // namespace

extern "C" void app_main(void) {
  static Esp32Isf15acp4Port port; // static: the driver keeps a reference
  static isf15acp4::ISF15ACP4<Esp32Isf15acp4Port> panel(port,
                                                        isf15acp4::KconfigDefaults()); // no I/O

  ESP_LOGI(kTag, "ISF15ACP4 driver %s", isf15acp4::GetDriverVersion());
  if (const auto r = panel.EnsureInitialized();
      !r) { // reset, configure, clear; the supply stays off
    ESP_LOGE(kTag, "EnsureInitialized failed: %s", isf15acp4::ToString(r.error));
    return;
  }
  if (const auto r = panel.DisplayOn(); !r) { // only now is the OLED supply enabled
    ESP_LOGE(kTag, "DisplayOn failed: %s", isf15acp4::ToString(r.error));
    static_cast<void>(panel.Deinitialize());
    return;
  }
  Draw(panel, 0);

  bool inverted = false;
  while (true) {
    const auto u = panel.Poll(); // read the switch, debounce, apply the idle timers
    if (!u) {
      ESP_LOGW(kTag, "poll failed: %s", isf15acp4::ToString(u.error));
      if ((u.error == isf15acp4::Error::Bus) && panel.IsShadowInDoubt()) {
        // A failed transfer leaves the chip's state unknown: start again from the power-down. A
        // failed switch read is not a controller fault and does not power-cycle the panel.
        static_cast<void>(panel.Reinitialize());
      }
    } else if ((u.value.event == isf15acp4::ButtonEvent::Pressed) && !u.value.woke_display) {
      Draw(panel, panel.GetButtonPressCount());
    } else if (u.value.event == isf15acp4::ButtonEvent::LongPress) {
      inverted = !inverted;
      static_cast<void>(panel.SetDisplayMode(inverted ? isf15acp4::DisplayMode::Inverse
                                                      : isf15acp4::DisplayMode::Normal));
    } else {
      // No event, or a press that only woke the display.
    }
    vTaskDelay(pdMS_TO_TICKS(5)); // at least two polls per debounce time
  }
}
