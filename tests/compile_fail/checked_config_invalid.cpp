/**
 * @file checked_config_invalid.cpp
 * @brief Must NOT compile: a CheckedConfig built from a configuration that Validate() rejects fails
 * the build, naming the error (handbook KB-CPP20-013).
 *
 * EXPECT: ConfigIsInvalid
 */
#include "isf15acp4_types.hpp"

int main() {
  // A master current step of 16 does not exist (87h takes 0 to 15): Validate() rejects it.
  constexpr isf15acp4::CheckedConfig kBoard{isf15acp4::Config{.master_current = 16}};
  return static_cast<int>(kBoard.Get().master_current);
}
