# Testing and CI

How the driver is checked: host tests against a model of the chip, a fuzzer that corrupts what the chip sends
back, compiled documentation, an install test, ESP32 and Cortex-M builds, static analysis and measured quality
gates, on every pull request.

## Host tests

The tests build the real driver against an independent model of the display controller as wired in the module: its
pins (RES#, VCC enable, CS#, D/C#), its command decoder, the display RAM in three colour depths, the drawing engine,
scrolling, the command lock, and a virtual clock that `DelayUs()` advances. The model records a violation whenever the
driver breaks a rule of the datasheet or of the safety design. The tests inject faults (a failed transfer at every
index, a pin that will not switch, a brown-out). They build with the firmware's flags, `-fno-exceptions -fno-rtti`, and
warnings as errors.

```bash
cmake -S . -B build -DISF15ACP4_BUILD_TESTS=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

| File | Covers |
| --- | --- |
| [`tests/test_isf15acp4.cpp`](../tests/test_isf15acp4.cpp) | Power order, display states, pixels in every depth and increment mode, the engine, scrolling, the lock, setters, variants |
| [`tests/test_button.cpp`](../tests/test_button.cpp) | The debouncer alone: bounce, press, release, long press, repeat, counter wrap-around, validation |
| [`tests/test_poll.cpp`](../tests/test_poll.cpp) | The switch through the driver: both polarities, events, idle dim and sleep, wake on a press |
| [`tests/test_errors.cpp`](../tests/test_errors.cpp) | Every chip-specific `Error` is returned by some call; `InvalidArgument` sends no transfer and no pin change |
| [`tests/test_bus_faults.cpp`](../tests/test_bus_faults.cpp) | A failed transfer at every index of a full run, pins that fail, the supply never on before configuration |
| [`tests/test_guards.cpp`](../tests/test_guards.cpp) | Behaviours that one-edit changes of the driver (mutants) had left unnoticed: stale supply flag, failure clean-up, idle-policy edges, the press counter |
| [`tests/test_model.cpp`](../tests/test_model.cpp) | The model's rules, that it flags each breach, the brown-out, and its command table against the driver's |
| [`tests/fuzz/`](../tests/fuzz/fuzz_isf15acp4.cpp) | The fuzz target; CTest replays its corpus (`isf15acp4_fuzz_isf15acp4_corpus`), see below |
| [`tests/test_version.cpp`](../tests/test_version.cpp) | The version getters follow the generated header (also against a 9.8.7 probe) |
| [`tests/test_kconfig.cpp`](../tests/test_kconfig.cpp) | `KconfigDefaults()` with and without `CONFIG_` values |
| [`tests/compile_fail/`](../tests/compile_fail/port_without_delay.cpp) | Code that must not compile, each for its `EXPECT:` diagnostic: a port without `DelayUs()`, a `void GpioSet()`, a `DelayUs(uint16_t)`, a `Write()` with a narrowed length or without `noexcept`, a `ReadSwitch()` or `NowMs()` of the wrong type, an invalid `CheckedConfig`, a class that is not a port |
| [`tests/mock_isf15acp4.hpp`](../tests/mock_isf15acp4.hpp) | The controller model; the bus, pins and faults are in `mock_isf15acp4_bus.hpp` |
| [`tests/check_version_copy.cmake`](../tests/check_version_copy.cmake) | `_config/Doxyfile` carries the CMake version |
| [`examples/host/`](../examples/host/README.md) | The host example, built with the test flags and run (`isf15acp4_host_example`) |
| [`tests/check_install.cmake`](../tests/check_install.cmake) | Installs into a scratch prefix and builds [`tests/install_consumer/`](../tests/install_consumer/CMakeLists.txt) with `find_package(isf15acp4)` |
| [`tests/check_doxygen.cmake`](../tests/check_doxygen.cmake) | Doxygen with `_config/Doxyfile` and warnings as errors, when `doxygen` is installed |

### The model stands alone

The bus is write only, so there is no readback to test against: the model is how the driver is verified.
[`tests/mock_isf15acp4.hpp`](../tests/mock_isf15acp4.hpp) is written from the SSD1331 datasheet, not from the driver: it
does not include any driver header, and its command table (opcodes and parameter counts, Table 10) is its own. A wrong
constant in the driver cannot hide behind the same wrong constant in the model (handbook KB-TST-030).
[`tests/test_model.cpp`](../tests/test_model.cpp) compares the two for all 256 opcodes: the driver's parameter table
accepts exactly the opcodes the model accepts.

What the model checks, each as a bit of `violations()`:

- the OLED supply enabled before the controller was configured (reset, remap, master configuration);
- the display turned on without the supply, or the supply cut while the display is on;
- a byte on the bus while RES# is low, a reset pulse under 3 µs, the supply enabled under 3 µs after RES# fell or while
  RES# is low;
- a byte clocked while CS# is high, an opcode that is not in Table 10, a parameter outside its range;
- a command sent while a drawing command is still running (the engine's busy time is a model knob, because the
  datasheet gives none);
- the logic supply cut while the OLED supply is on; the display turned on before the RAM was written.

It also changes state by itself: `BrownOut()` resets the chip with no RES# edge, which is how the tests show that the
driver cannot see it and that `Reinitialize()` repairs it.

## Fuzz tests

[`tests/fuzz/fuzz_isf15acp4.cpp`](../tests/fuzz/fuzz_isf15acp4.cpp) lets libFuzzer choose a configuration (often an
invalid one), then up to 300 driver calls with arbitrary arguments, and between them time passing, the switch level, a
transfer or a pin write that fails, and a brown-out of the controller. After every call it checks the driver's promises
and aborts, naming the line, when one breaks:

- a call returns an `Error` the enum defines;
- `InvalidArgument` means no transfer and no pin change;
- before `EnsureInitialized()` succeeds, only the lifecycle calls touch the bus or a pin;
- the model records no violation, except a supply forced off after an injected transfer failure and a display-on after a
  brown-out;
- a power-down with a working bus ends with the supply off and the display off.

```bash
# libFuzzer needs Clang (on macOS, Homebrew's LLVM)
cmake -S . -B build-fuzz -DCMAKE_CXX_COMPILER=clang++ -DISF15ACP4_BUILD_TESTS=ON -DISF15ACP4_BUILD_FUZZERS=ON
cmake --build build-fuzz --target fuzz_isf15acp4
./build-fuzz/tests/fuzz/fuzz_isf15acp4 -max_total_time=120 tests/fuzz/corpus/fuzz_isf15acp4
```

The same file also builds without libFuzzer as `fuzz_replay_fuzz_isf15acp4`, and CTest replays every input in
[`tests/fuzz/corpus/fuzz_isf15acp4/`](../tests/fuzz/corpus/fuzz_isf15acp4) on GCC and Clang
(`isf15acp4_fuzz_isf15acp4_corpus`): the seeds, and every input that ever broke a promise (name it `regress-*`), so a
fixed bug stays fixed. CI fuzzes for 120 s on every pull request, from the seeds plus a corpus it keeps between runs
(job `Host quality / fuzz`).

## Quality gates

The `Host quality` job measures the library code, `inc/` and `src/` only (tests and examples are left out), and
fails when a value is over the limits of this repository's assurance level, `high` (set in
[`.kokeb/handbook.yml`](../.kokeb/handbook.yml), defined by the handbook's ADR-0020):

| Gate | Tool and how it is measured | Limit at `high` |
| --- | --- | --- |
| Complexity | lizard 1.24.1, modified cyclomatic complexity (`-m`: a `switch` counts once) per function | CCN at most 15 |
| Function length | lizard, lines of code per function | at most 60 |
| Parameters | lizard, per function | at most 6 |
| Line coverage | GCC 14 `--coverage -O0`, every CTest, gcovr 8.7 `--merge-lines` (a template's lines count once across its instances), `--exclude-unreachable-branches --exclude-throw-branches` | at least 85 % |
| Branch coverage | same run | at least 75 % |
| MC/DC | GCC `-fcondition-coverage`, same run | measured, not gated at `high` (100 % at `safety`) |
| Sanitizers | Clang, `-fsanitize=address,undefined -fno-sanitize-recover=all`, every CTest | no report |
| Fuzzing | libFuzzer with ASan and UBSan, 120 s per target | no broken promise |

The reports are in the run's `host-quality-reports` artifact and its step summary. To run the gates locally:

```bash
# Coverage (GCC 14 or later), then the complexity limits
cmake -S . -B build-cov -DCMAKE_CXX_COMPILER=g++-14 -DISF15ACP4_BUILD_TESTS=ON \
  -DCMAKE_CXX_FLAGS="--coverage -O0 -fcondition-coverage"
cmake --build build-cov && ctest --test-dir build-cov
gcovr --root . --filter inc/ --filter src/ --merge-lines --exclude-unreachable-branches --txt build-cov
lizard -m -C 15 -L 60 -a 6 inc src

# Sanitizers (Clang)
cmake -S . -B build-san -DCMAKE_CXX_COMPILER=clang++ -DISF15ACP4_BUILD_TESTS=ON \
  -DCMAKE_CXX_FLAGS="-fsanitize=address,undefined -fno-sanitize-recover=all -O1 -g"
cmake --build build-san && ctest --test-dir build-san --output-on-failure
```

The fuzz gate runs locally as shown in [Fuzz tests](#fuzz-tests).

clang-tidy 20 ([`_config/.clang-tidy`](../_config/.clang-tidy): bugprone, CERT, clang-analyzer,
cppcoreguidelines, modernize, performance, portability and readability, each disabled check with its reason) and
cppcheck (strict) run in the `C++ lint` and `Repository checks` jobs and fail on any finding.

The `MISRA` job ([`ci-misra.yml`](../.github/workflows/ci-misra.yml)) runs the shared `ru-misra` workflow of
ci-tools at level `high`: it traces a host build and the Cortex-M build (`isf15acp4_optimised_build`) of every
translation unit, checks them against MISRA C++:2023 and the handbook's C++20 profile (the CodeQL coding-standards
and profile packs, plus the validated clang-tidy and cppcheck checks), and reports each finding against the
handbook's compliance matrix and deviation records. A finding is fixed in the code; a deliberate exception names
its deviation on the line before it (`// codeql::misra_deviation_next_line(MD-CPP-009)` above the `.ipp` include
of `inc/isf15acp4.hpp`). Until the repository is public the job runs with `open_source_prerelease: true`, the
declaration that the code is released under the licence in `LICENSE` and is to be published.

### Rehearsing the MISRA scan and the ESP32 builds locally

A first push is green only when the two CI checks a workstation used to leave to CI have passed locally.
`tools/preflight.sh --container` runs them in Docker, in addition to everything else it runs, and its last lines are
`== N failure(s)` and the gates line; both must show 0 failures.

- **`misra`** runs the `ru-misra` workflow's own scripts (`tools/ci-container/misra.sh`, image
  `tools/ci-container/Dockerfile.misra`): the caller's inputs are read from `.github/workflows/ci-misra.yml`; the
  tools are the versions and SHA-256 hashes of `tools/misra/toolchain.json` of the pinned ci-tools commit
  (CodeQL CLI bundle, `codeql/misra-cpp-coding-standards` pack, Arm GNU toolchain, clang-tidy, cppcheck); the
  configure, traced build, analysis, deviation records and report are the CI's. It passes when the report passes:
  a finding of a mandatory or required guideline that no record deviates, a scan problem or a suppression error
  fails; advisory findings are warnings. Each blocking finding is listed as `file:line failure Rule message`.
  The SARIF, JSON and CSV reports and the full log are kept in `$PREFLIGHT_OUT/misra` (default
  `$TMPDIR/kokeb-preflight/<repository>-<time>`).
- **`esp32`** builds what `ru-build.yml` builds: the matrix of `examples/esp32/app_config.yml` (the submodule's
  `generate_matrix.py`), every app and build type with `build_app.sh`, on ESP-IDF `release/v5.5` for the target of
  the examples. An app listed for another ESP-IDF version or target is not built in the image; the last line names
  it.

The CodeQL CLI and the pinned Arm toolchain exist for Linux x86-64 only. On an arm64 host (Apple silicon) the MISRA
image is `linux/amd64` and runs emulated; the ESP32 image is native on both architectures. A host without Docker
cannot run either; `tools/preflight.sh` then names both as skipped.

Differences from CI to know: the ESP-IDF commit is pinned in `Dockerfile.esp32` (CI clones the tip of the release
branch each run) and the ESP32 image uses Ubuntu's Python 3.12; the MISRA image has Ubuntu's `cmake` and `ninja`
where the runner has newer ones. `MISRA_ARGS=--host-only` scans the host build only, which is faster and leaves
the Cortex-M preprocessor paths to CI.

#### Bumping the pins

`Dockerfile.misra`: `CI_TOOLS_REF` is the commit `ru-misra.yml@v1` resolves to (`git ls-remote
https://github.com/kokeb-labs/ci-tools v1`), `HANDBOOK_REF` the default `handbook_ref` of that workflow; the tool
versions follow from `toolchain.json` of that commit. `Dockerfile.esp32`: `IDF_COMMIT` is the commit CI prints as
`ESP-IDF release/v5.5 is <sha>`. Rebuild with `docker rmi kokeb-ci-misra:local kokeb-ci-esp32:local`, then run
`--container`.

#### CodeQL licence

The CodeQL CLI is not free software. The MISRA scan uses it on CI only; nothing in this repository contains or
redistributes it. See the CodeQL terms before using it elsewhere.

## Documentation snippets

Every C++ block on this site is included from [`tests/docs/`](../tests/docs/my_port.hpp), which the host build
compiles (target `isf15acp4_doc_snippets`) with the driver instantiated over the documented port. A change to
the API that breaks a page breaks the build first. The README's example is a generated copy of the same file;
`kokeb-docs check` fails when it drifts.

## CI workflows

Everything runs from [`ci.yml`](../.github/workflows/ci.yml), which calls one workflow file per check and ends in
the `CI gate` job, the single required check (`CI gate / gate`).

| Job | What it checks |
| --- | --- |
| Host tests | GCC, then Clang (one job), build and run every test above, then build and run `examples/host` on its own |
| Host quality | The gates above: complexity, coverage, sanitizers, fuzzing |
| Cross-compile | `arm-none-eabi-g++` for Cortex-M0+, M4F and M7F with the strict flags, warnings as errors (nothing linked) |
| C++ lint | clang-format and clang-tidy over the sources |
| MISRA | MISRA C++:2023 and the C++20 profile on traced host and Cortex-M builds, at level `high` (see [Quality gates](#quality-gates)) |
| Docs | Strict site build (`_config/mkdocs.yml`), figure and generated-region drift, Doxygen (`_config/Doxyfile`), offline link check, Lighthouse |
| Repository checks | One job: DCO on pull requests, the org standard, Markdown and YAML lint, the [Kokeb Labs handbook](https://github.com/kokeb-labs/handbook) rules for `.kokeb/handbook.yml` (by rule ID), template drift (warns only) and cppcheck over the driver sources; a second job runs the handbook's source checks (include guards, bus requirements, raw access) |
| CI gate | Passes only if every job above passed: the one required check |

The **ESP32 examples** job builds every app in `examples/esp32/app_config.yml`, for each listed ESP-IDF version and
build type, with the repository root as the driver's component, and lints the example sources. The gate needs it
too.

The workflows call the shared, versioned workflows of
[kokeb-labs/ci-tools](https://github.com/kokeb-labs/ci-tools) at `@v1`.

## On hardware

No bench run has been done: see [Limitations](limitations.md#not-verified-on-hardware) for the steps. The ESP-IDF example
([Examples](examples.md)) draws a picture and follows the button; record the board and the result in the pull request's
bench-status section.
