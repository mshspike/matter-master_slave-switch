# ESP32 Matter switch

Battery-powered handheld ESP32-C6 switch that turns outlets on a TP-Link Tapo P304M Matter power strip on and off
(workshop master/slave pairing, on/off only, no power sensing). Read README.md for the full picture and the BOM.

## Layout
- `lib/core/`   hardware-independent switch logic (modes, outlet roles, toggle rules). No Arduino includes.
- `test/`       host unit tests for the core logic.
- `src/`        firmware: `main.cpp` (wake, inputs, display, Matter, sleep), `config.h` (pins, timing).
- `hardware/case/case.py`  parametric 3D case; writes `hardware/case/stl/`.
- `platformio.ini`  Arduino-ESP32 3.x via pioarduino (needed for the Matter library).

## Commands
- Logic tests: `g++ -std=c++17 -Ilib/core test/test_logic.cpp lib/core/switch_logic.cpp -o /tmp/t && /tmp/t`
- Firmware build/flash: `pio run`, `pio run -t upload`, `pio device monitor`
- Regenerate case STLs: `python hardware/case/case.py` (needs trimesh, manifold3d, numpy, networkx, scipy)

## Rules
- Keep logic in `lib/core` and add a test in `test/test_logic.cpp` for every behaviour change. Hardware code stays thin.
- Low battery drain is the top priority: deep sleep between presses, e-paper only redrawn on change, external 1 MΩ pull-ups.
- Only GPIO0..GPIO7 can wake the ESP32-C6 from deep sleep.
- Two mini SPDT slide switches: mode (`PIN_MODE`, GPIO0, wakes on the level opposite its current one) and power
  (in series with battery +, no GPIO; USB charging only works with it on). The mode switch needs no logic changes.
- Never commit `src/secrets.h` (Wi-Fi details) or any personal info, addresses or account links. The repo holds code, case model and README BOM only.
- The case dimensions are nominal datasheet values. Edit the PARAMETERS block in `case.py` after measuring real parts; keep the solids watertight and single-body.
- The build has no mains wiring. Do not suggest switching large induction motors directly through the strip.

## Status and next steps
- Core logic: done and tested. Firmware: written, not yet compiled or run on hardware. Case: modelled, not test-printed.
- Measure the slide switches you buy and update the `SLIDE_*` parameters in `case.py` before printing.
- Verify pins in `src/config.h` against the DFR1075 pinout; compile with PlatformIO and fix API mismatches in the Matter and display calls.
- Measure deep-sleep and wake-to-send time; tune `MATTER_SETTLE_MS` (events sent before the hub resubscribes are lost).
- Not implemented: direct binding and ESP32-as-controller routes.
