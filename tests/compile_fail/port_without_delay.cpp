/**
 * @file port_without_delay.cpp
 * @brief Must NOT compile: the reset, supply and engine waits need the port's DelayUs().
 *
 * EXPECT: DelayUs
 */
#include <cstddef>
#include <cstdint>

#include "isf15acp4.hpp"

class NoDelayPort : public isf15acp4::SpiInterface<NoDelayPort> {
public:
  bool EnsureInitialized() noexcept {
    return true;
  }
  bool Write(isf15acp4::DataKind kind, const uint8_t* data, std::size_t len) noexcept {
    static_cast<void>(kind);
    static_cast<void>(data);
    return len > 0U;
  }
  bool GpioSet(isf15acp4::CtrlPin pin, isf15acp4::GpioSignal signal) noexcept {
    static_cast<void>(pin);
    static_cast<void>(signal);
    return true;
  }
};

int main() {
  NoDelayPort port;
  isf15acp4::ISF15ACP4<NoDelayPort> display(port);
  return static_cast<int>(display.EnsureInitialized().error);
}
