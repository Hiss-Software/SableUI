# Sableui

Sableui is a minimalist immediate-style GUI library for Windows built on
Direct2D, with a clean C ABI.

- **Small public surface**: one header (`include/sableui/sableui.h`), opaque
  handles, error codes instead of exceptions.
- **Per-monitor DPI aware end to end**: all public coordinates are logical
  (DIP) units; windows rescale live when moved between monitors with
  different densities.
- **Hardware-accelerated rendering** through Direct2D with automatic recovery
  from device loss (`D2DERR_RECREATE_TARGET`).
- **Keyboard accessible**: Tab / Shift+Tab cycles focus, Enter and Space
  activate widgets, focus ring is drawn, double-click and wheel events are
  exposed.
- **Custom drawing**: paint-time hook with fills, strokes, ellipses, lines,
  text and clip rects — on top of or instead of the built-in widgets.
- **Modular core**: widgets talk to an abstract `Renderer`, so additional
  backends (Direct3D, software) can be added without touching widget code.
- **ABI-safe error model**: nothing crosses the C boundary with exceptions;
  failures come back as return codes plus `sabGetLastError()`.

## Repository layout

```
include/sableui/    public C API (the only consumer-facing contract)
src/                internal core: renderer, widgets, window, message loop
examples/           runnable examples
```

## Building

Requirements: CMake >= 3.20, Ninja, a toolchain (MinGW-w64 or MSVC).

```sh
cmake -S . -B build -G Ninja
cmake --build build
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
