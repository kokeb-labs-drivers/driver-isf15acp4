/**
 * @file not_a_port.cpp
 * @brief Must NOT compile: the driver is constrained by the port concept.
 *
 * EXPECT: Port
 */
#include "isf15acp4.hpp"

struct NotAPort {
  bool EnsureInitialized() noexcept {
    return true;
  }
};

int main() {
  NotAPort port;
  isf15acp4::ISF15ACP4<NotAPort> display(port);
  return static_cast<int>(display.EnsureInitialized().error);
}
