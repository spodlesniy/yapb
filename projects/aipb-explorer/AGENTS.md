# AiPB Explorer rules

These rules apply to `projects/aipb-explorer/` alongside repository `AGENTS.md` and `projects/STYLE.md`.

- CMake 3.21+, Ninja and C++20 are required.
- Use standard C++20 library facilities with the shared project style.
- Keep source-code comments in English.
- The explorer core must not link or include YaPB, GoldSrc SDK, crlib, ONNX or the Map Tool.
- Each accepted `step` consumes exactly one fixed-duration tick.
- Invalid steps leave state and tick unchanged.
- Test every contract, using CTest with GCC, Clang and MSVC.
- Each named development step is one atomic commit in `spodlesniy/yapb`; upstream remains read-only.
