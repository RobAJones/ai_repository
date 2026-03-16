# Contributing

Thank you for your interest in contributing to the DFR1154 AI Camera project.

## Who This Is For

This project has two audiences:

- **Developers and engineers** making firmware contributions
- **CIJE educators and students** using this as an educational platform — see the [Quick Start Guide](docs/quick_start_guide.pdf) if you are setting up for the first time

---

## Reporting Issues

Use the [Issues tab](../../issues) to report bugs or request features.

**For a bug report, please include:**
- Firmware version (shown in serial monitor on boot, e.g. `v1.2.16`)
- Arduino IDE version and ESP32 board package version
- A description of expected vs. actual behavior
- Serial monitor output if available
- Steps to reproduce

**For a feature request, please include:**
- What you want to accomplish
- Which firmware line it applies to (Standalone or Automation Agent)

---

## Making Changes

1. Fork the repository and create a branch from `main`
2. Name your branch descriptively: `fix/tts-dropout` or `feat/settings-page`
3. Follow the versioning convention already in use — patch fixes increment the third digit (`1.2.16` → `1.2.17`)
4. Update `CHANGELOG.md` with a clear entry under the correct version line
5. **Never commit `secrets.h`** — credentials must stay local
6. Submit a Pull Request with a summary of what changed and why

---

## Code Style

- This project uses Arduino C++ targeting the ESP32-S3 platform
- Keep large buffers (`static` or `ps_malloc()`) off the FreeRTOS `loopTask` stack — the stack limit is approximately 8KB
- Use `vTaskDelay(1)` instead of `esp_task_wdt_reset()` in `loop()` for WDT-safe yielding
- Maintain async state machine patterns in `loop()` — do not block `handleClient()` polling

---

## Questions

For questions about the CIJE educational program, contact Orly Nader, Director of Innovation at CIJE.  
For engineering questions, open an Issue or reach out through the repository.
