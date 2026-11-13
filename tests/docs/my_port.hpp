/**
 * @file my_port.hpp
 * @brief The SPI port shown on the Quickstart and Porting pages, compiled by the host build.
 *
 * Every platform call goes through the `platform_*` functions declared below; a real port replaces
 * them with its SDK (ESP-IDF, STM32 HAL, Zephyr, Linux spidev).
 */
#ifndef ISF15ACP4_MY_PORT_HPP
#define ISF15ACP4_MY_PORT_HPP

#include <cstddef>
#include <cstdint>

#include "isf15acp4_spi_interface.hpp"

// Your SDK's calls. Declared only: the docs build checks that the port compiles.
bool platform_spi_init() noexcept; // mode 0, at most 6.6 MHz, MOSI and SCLK only
bool platform_spi_write(const uint8_t* data, std::size_t len) noexcept; // CS low for all bytes
bool platform_dc(bool high) noexcept;                                   // the D/C# pin
bool platform_res(bool high) noexcept;                                  // the RES# pin
bool platform_vcc(bool on) noexcept;          // enable of the 15 V OLED supply
void platform_delay_us(uint32_t us) noexcept; // busy-wait or sleep

// --8<-- [start:port]
class MyPort : public isf15acp4::SpiInterface<MyPort> {
public:
  // Bring the bus and the pins up once, each at its idle level (supply off, RES# high, CS# high);
  // later calls return true at once.
  bool EnsureInitialized() noexcept {
    if (!ready_) {
      ready_ = platform_vcc(false) && platform_res(true) && platform_spi_init();
    }
    return ready_;
  }

  // One write-only transfer, CS low for all `len` bytes. There is no read.
  bool Write(isf15acp4::DataKind kind, const uint8_t* data, std::size_t len) noexcept {
    return platform_dc(kind == isf15acp4::DataKind::Data) && platform_spi_write(data, len);
  }

  // RES# is active low and the supply enable active high; the port maps the logical level.
  bool GpioSet(isf15acp4::CtrlPin pin, isf15acp4::GpioSignal signal) noexcept {
    const bool active = (signal == isf15acp4::GpioSignal::ACTIVE);
    if (pin == isf15acp4::CtrlPin::Reset) {
      return platform_res(!active);
    }
    return platform_vcc(active);
  }

  // Wait at least `us` microseconds: the reset and the supply waits depend on it.
  void DelayUs(uint32_t us) noexcept {
    platform_delay_us(us);
  }

private:
  bool ready_ = false;
};
// --8<-- [end:port]

#endif // ISF15ACP4_MY_PORT_HPP
