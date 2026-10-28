# SableUI

SableUI is a small, dependency-free GUI library for Windows, built on
Direct2D and DirectWrite and exposed through a stable C ABI. It ships a
familiar widget set (buttons, labels, checkboxes, text fields, sliders,
progress bars, vertical scroll areas) and stays out of the way when you
want to paint yourself.

- **Small public surface**: one header (`include/sableui/sableui.h`), opaque
  handles, plain return codes instead of exceptions.
- **Per-monitor DPI aware end to end**: all public coordinates are logical
  (DIP) units (1 DIP = 1 px at 96 dpi); windows rescale live when moved
  between monitors with different densities (`WM_DPICHANGED`).
- **Hardware-accelerated rendering** through Direct2D, with automatic
  recovery from device loss (`D2DERR_RECREATE_TARGET`).
- **Keyboard accessible**: Tab / Shift+Tab cycles focus, Enter and Space
  activate widgets, a focus ring is drawn, double-click and wheel events
  are exposed. Text fields support caret placement, selection,
  Ctrl+A/C/X/V and word-wise navigation.
- **IME composition**: Chinese, Japanese and Korean input is supported
  through `WM_IME_*` and `imm32`; the composition window follows the
  focused text field.
- **Custom drawing**: a paint-time hook with fills, strokes, ellipses,
  lines, text and clip rects — on top of or instead of the built-in
  widgets.
- **Layout anchors**: pin a widget's edges to the client area so it keeps
  its margins (or stretches) across resizes.
- **Per-widget styling**: font family and size, background colour and
  corner radius can be overridden on any widget.
- **Modular core**: widgets talk to an abstract `Renderer`, so additional
  backends (Direct3D, software) can be added without touching widget code.
- **ABI-safe error model**: nothing crosses the C boundary with exceptions;
  failures come back as return codes plus `sabGetLastError()`.

## Repository layout

```
include/sableui/    public C API (the only consumer-facing contract)
src/                internal core: renderer, widgets, window, message loop
cmake/              version derivation helpers
examples/           runnable C examples
```

## Requirements

> **Note:** MSVC is not officially supported at the moment. The
> `CMakeLists.txt` contains an MSVC branch, but `IID_IDWriteFactory`
> resolution under MSVC (`dwrite.h` declares it as `EXTERN_C const IID`,
> which `<initguid.h>` does not generate) is still unresolved. Use
> **MinGW-w64** for a working build.

- Windows 10 or later
- CMake >= 3.20
- Ninja
- A C++20 compiler: MinGW-w64 (GCC 12+) or MSVC 2022 (19.30+)
- Direct2D, DirectWrite, DWM, IMM (shipped with Windows)

Link with `d2d1`, `dwrite`, `dwmapi`, `ole32`, `user32`, `gdi32`, `imm32`.

## Building

### MinGW-w64

```sh
cmake -S . -B build -G Ninja
cmake --build build
```

### MSVC 2022

> **Note:** MSVC is not officially supported at the moment. See the note
> in Requirements.

From a Developer Command Prompt (so that `cl.exe` is on `PATH`):

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

Or with the Visual Studio generator:

```powershell
cmake -S . -B build-msvc -G "Visual Studio 17 2022" -A x64
cmake --build build-msvc --config Release
```

The result is `libsableui.dll` and `libsableui.dll.a` (MinGW), or
`sableui.dll` and `sableui.lib` (MSVC). The library version is derived from
the latest git tag (`cmake/gen_version.cmake`).

## C usage

```c
#include <sableui/sableui.h>
#include <stdio.h>

static void on_click(void* ud) {
    (void)ud;
    puts("clicked");
}

static void on_slide(void* ud, float v) {
    (void)ud;
    printf("slider: %.1f\n", v);
}

int main(void) {
    sabWindow w = sabCreateWindow(L"Hello", 480, 320);
    if (!w) {
        fprintf(stderr, "%s\n", sabGetLastError());
        return 1;
    }
    sabAddLabel(w, 24, 24, 432, 24, L"Hello, SableUI!");
    sabAddButton(w, 150, 60, 180, 44, L"Say hi", on_click, NULL);
    sabAddSlider(w, 24, 120, 432, 32, 0.0f, 100.0f, 50.0f, on_slide, NULL);
    sabAddProgressBar(w, 24, 168, 432, 16, 50.0f);
    sabRun(w);
    sabDestroyWindow(w);
    return 0;
}
```

Compile with MinGW:

```sh
gcc main.c -Iinclude -L. -lsableui -o app.exe
```

Compile with MSVC:

```bat
cl main.c /I include sableui.lib
```

Keep `libsableui.dll` (or `sableui.dll`) next to `app.exe` (or on `PATH`).

## Text fields and scroll areas

```c
sabWidgetId name = sabAddTextField(w, 16, 16, 300, 32, L"", 0, NULL, NULL);

sabWidgetId area = sabAddScrollArea(w, 16, 64, 448, 200);
for (int i = 0; i < 20; ++i)
    sabAddLabelIn(w, area, 12, i * 28, 400, 24, L"row");
```

Widgets added with the `sabAdd*In` family live inside a scroll area and use
coordinates relative to its top-left corner. Content taller than the area
scrolls with the wheel or by dragging the thumb.

## Sliders and progress bars

```c
static void on_value(void* ud, float v) {
    (void)ud; (void)v;
}

sabWidgetId slider = sabAddSlider(w, 16, 16, 300, 32,
                                  0.0f, 100.0f, 42.0f, on_value, NULL);
sabWidgetId bar    = sabAddProgressBar(w, 16, 64, 300, 16, 42.0f);

sabSetWidgetValue(w, slider, 75.0f);
sabSetWidgetValue(w, bar,    75.0f);
```

`sabSetWidgetValue` clamps to the widget's range. `sabAddProgressBar` uses
`[0, 100]` by default.

## Styling a widget

```c
sabSetWidgetFont(w, id, L"Consolas", 16.0f);
sabSetWidgetBackground(w, id, 0.20f, 0.25f, 0.35f, 1.0f);
sabSetWidgetRadius(w, id, 12.0f);
```

`family == NULL` keeps the current font, `size <= 0` keeps the current
size, a negative radius reverts to the palette default, and a fully
transparent background reverts to the palette colour.

## Layout anchors

```c
sabWidgetId status = sabAddLabel(w, 16, 280, 448, 24, L"ready");
sabSetWidgetAnchors(w, status, SAB_LAYOUT_FILL_H);  // stretch horizontally
```

`SAB_ANCHOR_LEFT/TOP/RIGHT/BOTTOM` are combinable; anchoring opposite edges
stretches the widget. Anchors are captured once and re-applied on
`WM_SIZE`. Only top-level widgets can be anchored.

## Threading model

SableUI is single-threaded and expects to own its window and message loop.
All API calls must be made from the thread that created the window. Event
callbacks and timers run on that thread inside `sabRun()`.

## Limitations

- No undo/redo in text fields.
- Text fields are single-line by default (multi-line is supported at the
  internal layer but not yet exposed through the C API).
- Scroll areas are vertical only.

## License

MIT — see [LICENSE](LICENSE).