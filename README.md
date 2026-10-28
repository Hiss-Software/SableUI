# Sableui

Sableui is a minimalist immediate-style GUI library for Windows built on
Direct2D, with a clean C ABI and first-class Python bindings.

- **Small public surface**: one header (`include/sableui/sableui.h`), seven
  functions, opaque handles.
- **Hardware-accelerated rendering** through Direct2D with automatic recovery
  from device loss (`D2DERR_RECREATE_TARGET`).
- **Modular core**: widgets talk to an abstract `Renderer`, so additional
  backends (Direct3D, software) can be added without touching widget code.
- **ABI-safe error model**: nothing crosses the C boundary with exceptions;
  failures come back as return codes plus `sabGetLastError()`.
- **Python-native feel**: `from sableui import Window`, context manager
  support, typed stubs for IDE autocompletion, correct GIL handling.

## Repository layout

```
include/sableui/    public C API (the only consumer-facing contract)
src/                internal core: renderer, widgets, window, message loop
bindings/           pybind11 bindings
python/sableui/     importable package (+ type stubs); build artifacts land here
examples/           runnable examples
```

## Building

Requirements: CMake >= 3.20, Ninja, a toolchain (MinGW-w64 or MSVC), Python 3
with development headers, pybind11.

```sh
cmake -S . -B build -G Ninja
cmake --build build
python examples/password_generator.py
```

Install pybind11 on MSYS2 with:

```sh
pacman -S mingw-w64-x86_64-pybind11
```

## Python usage

```python
from sableui import Window

with Window("Hello", 480, 320) as win:
    label = win.add_label(20, 140, 440, 40, "Ready")

    def say_hi():
        win.set_label_text(label, "Hello from Sableui!")

    win.add_button(150, 60, 180, 44, "Say hi", say_hi)
    win.run()
```

## C usage

```c
sabWindow w = sabCreateWindow(L"Hello", 480, 320);
if (!w) { fprintf(stderr, "%s\n", sabGetLastError()); return 1; }
sabAddButton(w, 150, 60, 180, 44, L"Say hi", on_click, NULL);
sabRun(w);
sabDestroyWindow(w);
```

## License

MIT - see [LICENSE](LICENSE).
