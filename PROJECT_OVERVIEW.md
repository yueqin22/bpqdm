# BPQDM Inverter Firmware Source

This repository contains the firmware source and CCS project files for a
frequency-converter platform based on the Texas Instruments TMS320F28035
(C28x). The project is organized around system startup, motor-control,
peripheral I/O, communications, and supporting utilities.

The project-authored source is released under GPL-3.0-only. Some device
support files originate from or are derived from TI development materials;
check the applicable vendor terms before redistributing those files.

## Hardware and software

- **Target MCU:** TI TMS320F28035 / C28x.
- **Project format:** Code Composer Studio project files using the COFF ABI.
- **Firmware areas:** motor-control modes and parameter handling, PWM and
  current/encoder interfaces, inverter protection, analog/digital I/O, and
  communication modules including CAN, Modbus, and Profibus-related code.
- **Project contents:** C and assembly sources, device and module headers,
  linker command files, CCS project configuration, and development/test tools.

## Build and checks

The host-side tests and build-evidence checks are under `tests/`. The full
`tests/run_all.py` suite consumes generated files under `build/` and
`docs/_difftest/`; these generated artifacts are intentionally excluded from
publication, so regenerate the required evidence using the workflows in
`docs/` before running the full suite from a clean checkout.

Building the target firmware requires a compatible TI C2000 compiler and CCS
environment. The TI runtime library is not included; obtain it through an
appropriately licensed TI toolchain. See the project configuration and build
instructions in `docs/`.

The board-specific CSM password assembly file is excluded from this public
source set. Provide an authorized local copy when building firmware that
requires it.

No project `LICENSE` file is included in this release. Add an appropriate
license before representing the code as formally licensed open source or
granting reuse and redistribution rights.

The host-side checks and recorded build comparisons are not a substitute for
testing on the target board. Motor operation, protection behavior,
communications timing, Flash programming, and power-on startup still require
hardware validation before deployment.

## Publication note

This overview is a fresh project description. The previous `README.md` and the
`.workbuddy-ai/` workspace notes are intentionally excluded from publication.
