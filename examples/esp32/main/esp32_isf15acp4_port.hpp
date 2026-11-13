/**
 * @file esp32_isf15acp4_port.hpp
 * @brief ESP-IDF port for the ISF15ACP4 driver: spi_master (write only, no MISO) for the display
 * bus, GPIO for D/C, RES#, the OLED supply enable and the switch, esp_timer for time, esp_rom
 * delays and ESP_LOG for the log hook.
 *
 * Start from this file for any ESP-IDF target. The pins are in Esp32Isf15acp4Pins; the defaults
 * match the wiring table in examples/esp32/README.md (ESP32-C6 DevKitC-1). Not built or run on
 * hardware by the author of the driver: CI compiles it, a bench run is still open.
 */
#ifndef ISF15ACP4_ESP32_ISF15ACP4_PORT_HPP
#define ISF15ACP4_ESP32_ISF15ACP4_PORT_HPP

#include <cstddef>
#include <cstdint>

#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_log.h"
#include "esp_rom_sys.h"
#include "esp_timer.h"

#include "isf15acp4.hpp"

/// The port's default SPI clock: 4 MHz, below the controller's 6.6 MHz limit.
inline constexpr int kEsp32Isf15acp4ClockHz = 4'000'000;
/// The port's SPI mode.
inline constexpr int kEsp32Isf15acp4SpiMode = 0;
// The bus requirements of the driver (handbook KB-CPP-123), checked at compile time.
static_assert(kEsp32Isf15acp4ClockHz <= static_cast<int>(isf15acp4::kMaxClockHz),
              "the default clock is above the SSD1331's SCLK maximum");
static_assert((kEsp32Isf15acp4SpiMode == isf15acp4::kSpiMode) || (kEsp32Isf15acp4SpiMode == 3),
              "the SSD1331 samples in SPI mode 0 or 3");
static_assert(isf15acp4::kMsbFirst, "this port sends MSB first (no SPI_DEVICE_BIT_LSBFIRST)");
static_assert(isf15acp4::kWriteOnly, "the bus has no MISO: the port never reads");

/// Pins and bus settings of the ESP-IDF port.
struct Esp32Isf15acp4Pins {
  spi_host_device_t host = SPI2_HOST;
  int sclk = 6;
  int mosi = 7;
  int cs = 10;
  int dc = 3;                            ///< D/C#: low for a command, high for pixel data.
  int reset_n = 11;                      ///< RES#.
  int vcc_enable = 4;                    ///< Enable of the 15 V OLED supply: high = supply on.
  int button = 5;                        ///< The switch, to ground with a pull-up (closed = low).
  int clock_hz = kEsp32Isf15acp4ClockHz; ///< Up to `isf15acp4::kMaxClockHz`; checked at start.
  int spi_mode = kEsp32Isf15acp4SpiMode; ///< Mode 0 or 3.
};

/// The ISF15ACP4 port on ESP-IDF, with every optional hook.
class Esp32Isf15acp4Port : public isf15acp4::SpiInterface<Esp32Isf15acp4Port> {
public:
  explicit Esp32Isf15acp4Port(const Esp32Isf15acp4Pins& pins = {}) noexcept : pins_(pins) {}

  bool EnsureInitialized() noexcept {
    if (device_ != nullptr) {
      return true;
    }
    if ((pins_.clock_hz <= 0) || (pins_.clock_hz > static_cast<int>(isf15acp4::kMaxClockHz)) ||
        ((pins_.spi_mode != 0) && (pins_.spi_mode != 3))) {
      ESP_LOGE(kTag, "clock or SPI mode outside the controller's bus requirements");
      return false;
    }
    // Idle levels first (handbook KB-HW-018): the OLED supply off, RES# not asserted. Each level is
    // written before the output is enabled, so configuring a pin never switches the supply on.
    if (!output(pins_.vcc_enable, 0) || !output(pins_.reset_n, 1) || !output(pins_.dc, 0)) {
      return false;
    }
    gpio_config_t in{};
    in.pin_bit_mask = 1ULL << static_cast<unsigned>(pins_.button);
    in.mode = GPIO_MODE_INPUT;
    in.pull_up_en = GPIO_PULLUP_ENABLE;
    if (gpio_config(&in) != ESP_OK) {
      return false;
    }
    spi_bus_config_t bus{};
    bus.mosi_io_num = pins_.mosi;
    bus.miso_io_num = -1; // write only
    bus.sclk_io_num = pins_.sclk;
    bus.quadwp_io_num = -1;
    bus.quadhd_io_num = -1;
    // The driver sends up to kDataChunkBytes (96) per transfer. Without DMA, ESP-IDF's spi_master
    // limits a transfer to 64 bytes, so the bus uses DMA; with SPI_DMA_DISABLED the chunk would
    // have to be 64 or less.
    bus.max_transfer_sz = static_cast<int>(isf15acp4::detail::kDataChunkBytes);
    const esp_err_t bus_err = spi_bus_initialize(pins_.host, &bus, SPI_DMA_CH_AUTO);
    if ((bus_err != ESP_OK) &&
        (bus_err != ESP_ERR_INVALID_STATE)) { // INVALID_STATE: bus already up
      ESP_LOGE(kTag, "spi_bus_initialize: %s", esp_err_to_name(bus_err));
      return false;
    }
    spi_device_interface_config_t dev{};
    dev.clock_speed_hz = pins_.clock_hz;
    dev.mode = static_cast<uint8_t>(pins_.spi_mode);
    dev.spics_io_num = pins_.cs;
    dev.queue_size = 1;
    dev.flags = SPI_DEVICE_HALFDUPLEX | SPI_DEVICE_NO_DUMMY;
    if (spi_bus_add_device(pins_.host, &dev, &device_) != ESP_OK) {
      device_ = nullptr;
      return false;
    }
    return true;
  }

  /// One write-only transfer: D/C# set for the kind, CS# low for all bytes, CS# high at the end.
  bool Write(isf15acp4::DataKind kind, const uint8_t* data, std::size_t len) noexcept {
    if ((device_ == nullptr) || (data == nullptr) || (len == 0U)) {
      return false;
    }
    if (gpio_set_level(static_cast<gpio_num_t>(pins_.dc),
                       kind == isf15acp4::DataKind::Data ? 1U : 0U) != ESP_OK) {
      return false;
    }
    spi_transaction_t t{};
    t.length = len * 8U;
    t.tx_buffer = data;
    return spi_device_polling_transmit(device_, &t) == ESP_OK;
  }

  bool GpioSet(isf15acp4::CtrlPin pin, isf15acp4::GpioSignal signal) noexcept {
    const bool active = (signal == isf15acp4::GpioSignal::ACTIVE);
    if (pin == isf15acp4::CtrlPin::Reset) {
      // RES# is active low: ACTIVE (in reset) is level 0.
      return gpio_set_level(static_cast<gpio_num_t>(pins_.reset_n), active ? 0U : 1U) == ESP_OK;
    }
    // The supply enable is active high: ACTIVE is the supply on.
    return gpio_set_level(static_cast<gpio_num_t>(pins_.vcc_enable), active ? 1U : 0U) == ESP_OK;
  }

  void DelayUs(uint32_t us) noexcept {
    esp_rom_delay_us(us);
  }

  /// The switch pin's physical level.
  bool ReadSwitch(bool& high) noexcept {
    high = gpio_get_level(static_cast<gpio_num_t>(pins_.button)) != 0;
    return true;
  }

  /// A free-running millisecond counter (wraps at 2^32, which the debouncer handles).
  uint32_t NowMs() noexcept {
    return static_cast<uint32_t>(static_cast<uint64_t>(esp_timer_get_time()) / 1000ULL);
  }

  void Log(isf15acp4::LogLevel level, const char* message) noexcept {
    const esp_log_level_t esp_level = level == isf15acp4::LogLevel::Error     ? ESP_LOG_ERROR
                                      : level == isf15acp4::LogLevel::Warning ? ESP_LOG_WARN
                                      : level == isf15acp4::LogLevel::Info    ? ESP_LOG_INFO
                                                                              : ESP_LOG_DEBUG;
    esp_log_write(esp_level, kTag, "%s\n", message); // plain text, never a format
  }

private:
  static bool output(int pin, uint32_t idle_level) noexcept {
    gpio_config_t io{};
    io.pin_bit_mask = 1ULL << static_cast<unsigned>(pin);
    io.mode = GPIO_MODE_OUTPUT;
    return (gpio_set_level(static_cast<gpio_num_t>(pin), idle_level) == ESP_OK) &&
           (gpio_config(&io) == ESP_OK) &&
           (gpio_set_level(static_cast<gpio_num_t>(pin), idle_level) == ESP_OK);
  }

  static constexpr const char* kTag = "isf15acp4";
  Esp32Isf15acp4Pins pins_;
  spi_device_handle_t device_ = nullptr;
};

#endif // ISF15ACP4_ESP32_ISF15ACP4_PORT_HPP
