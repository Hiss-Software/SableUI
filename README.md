# SableUI

<img src="data:image/svg+xml;base64,PHN2ZyB4bWxucz0iaHR0cDovL3d3dy53My5vcmcvMjAwMC9zdmciIHZpZXdCb3g9IjAgMCAyNTYgMjU2IiB3aWR0aD0iMjU2IiBoZWlnaHQ9IjI1NiI+CiAgPGNpcmNsZSBjeD0iMTI4IiBjeT0iMTI4IiByPSIxMjQiIGZpbGw9IiMwMDAwMDAiLz4KICA8dGV4dCB4PSIxMjgiIHk9IjEzNSIgZm9udC1mYW1pbHk9IidPbGQgRW5nbGlzaCBUZXh0IE1UJywgc2VyaWYiIGZvbnQtc2l6ZT0iMTgwIiBmaWxsPSIjRkZGRkZGIiB0ZXh0LWFuY2hvcj0ibWlkZGxlIiBkb21pbmFudC1iYXNlbGluZT0ibWlkZGxlIj5IPC90ZXh0Pgo8L3N2Zz4=" width="128" />

SableUI is a small, dependency-free GUI library for Windows, built on
Direct2D and DirectWrite and exposed through a stable C ABI. It ships a
familiar widget set (buttons, labels, checkboxes, text fields, text areas,
sliders, progress bars, images, scroll areas) and stays out of the way when
you want to paint yourself.

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
- **Images**: PNG, JPEG, BMP and GIF are decoded through WIC and cached
  by path.
- **Undo/redo**: text fields keep a bounded undo history (200 snapshots),
  driven by Ctrl+Z / Ctrl+Shift+Z / Ctrl+Y.
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
- Direct2D, DirectWrite, DWM, IMM, WIC (shipped with Windows)

Link with `d2d1`, `dwrite`, `dwmapi`, `ole32`, `user32`, `gdi32`, `imm32`,
`windowscodecs`.

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
coordinates relative to its top-left corner. Content taller or wider than
the area scrolls with the wheel (`WM_MOUSEWHEEL` vertically,
`WM_MOUSEHWHEEL` horizontally) or by dragging the thumb.

## Text areas

```c
sabWidgetId body = sabAddTextArea(w, 16, 16, 448, 200, L"", NULL, NULL);
```

`sabAddTextArea` is a multiline variant of `sabAddTextField`. Enter inserts
a newline and word wrapping is enabled.

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

## Images

```c
sabWidgetId img = sabAddImage(w, 16, 16, 256, 256, L"logo.png");
sabSetWidgetImage(w, img, L"other.png");  // swap later
```

`sabAddImage` accepts PNG, JPEG, BMP and GIF through WIC. The decoded
bitmap is cached by path; changing the path decodes a new one.

## Undo and redo

Text fields keep a bounded undo history (200 snapshots). The user can
press Ctrl+Z / Ctrl+Shift+Z / Ctrl+Y while the field has focus.

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

## License

MIT — see [LICENSE](LICENSE).