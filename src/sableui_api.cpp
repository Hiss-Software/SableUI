// Implementation of the public C ABI over the internal core. Nothing here is
// allowed to throw across the boundary: every failure is reported through the
// return value and sabGetLastError.
#include <initguid.h>

#include <sableui/sableui.h>

#include "sableui.hpp"

#include <algorithm>
#include <cstring>
#include <exception>
#include <memory>
#include <new>
#include <string>

namespace {

// Per-thread error buffer. Never throws, never allocates on the hot path.
thread_local char g_lastError[256] = "no error";

void SetError(const char* msg) {
    std::strncpy(g_lastError, msg ? msg : "unknown error",
                 sizeof(g_lastError) - 1);
    g_lastError[sizeof(g_lastError) - 1] = '\0';
}

void ClearError() { SetError("no error"); }

// Copies a wide string into a caller-owned buffer. Returns the required
// length in characters including the NUL terminator. If the buffer is too
// small (or NULL), writes nothing and sets the error.
int CopyWide(const std::wstring& src, wchar_t* buffer, int buffer_chars,
             const char* where) {
    const int required = static_cast<int>(src.size()) + 1;
    if (!buffer || buffer_chars < required) {
        SetError(where);
        return required;
    }
    std::wmemcpy(buffer, src.c_str(), static_cast<size_t>(required));
    return required;
}

// Calls fn, converts any exception into a last-error message. Returns the
// value fn produced, or fallback if fn threw.
template <class Fn>
auto Guard(const char* where, decltype(std::declval<Fn>()()) fallback, Fn fn)
    -> decltype(fn()) {
    try {
        return fn();
    } catch (const std::exception& e) {
        SetError((std::string(where) + ": " + e.what()).c_str());
    } catch (...) {
        SetError((std::string(where) + ": unknown error").c_str());
    }
    return fallback;
}

sableui::Window* W(sabWindow window) {
    return reinterpret_cast<sableui::Window*>(window);
}

} // namespace

extern "C" {

const char* sabGetLastError(void) {
    return g_lastError;
}

// --- Window lifecycle -------------------------------------------------------

sabWindow sabCreateWindow(const wchar_t* title, int width, int height) {
    if (width <= 0 || height <= 0) {
        SetError("sabCreateWindow: invalid size");
        return nullptr;
    }
    auto* win = new (std::nothrow) sableui::Window();
    if (!win) {
        SetError("sabCreateWindow: out of memory");
        return nullptr;
    }
    std::string err;
    if (!win->Create(title ? title : L"", width, height, &err)) {
        SetError(err.empty() ? "sabCreateWindow: failed" : err.c_str());
        delete win;
        return nullptr;
    }
    ClearError();
    return reinterpret_cast<sabWindow>(win);
}

void sabDestroyWindow(sabWindow window) {
    delete W(window);
}

void sabRun(sabWindow window) {
    sableui::Window* w = W(window);
    if (!w) {
        SetError("sabRun: NULL window handle");
        return;
    }
    w->Run();
}

void sabSetTitle(sabWindow window, const wchar_t* title) {
    if (sableui::Window* w = W(window)) w->SetTitle(title ? title : L"");
}

void sabSetResizable(sabWindow window, int on) {
    if (sableui::Window* w = W(window)) w->SetResizable(on != 0);
}

void sabSetBorderless(sabWindow window, int on) {
    if (sableui::Window* w = W(window)) w->SetBorderless(on != 0);
}

void sabSetMinimumSize(sabWindow window, int width, int height) {
    if (sableui::Window* w = W(window)) w->SetMinimumSize(width, height);
}

void sabCloseWindow(sabWindow window) {
    if (sableui::Window* w = W(window)) w->CloseRequest();
}

void sabGetClientSize(sabWindow window, int* width, int* height) {
    sableui::Window* w = W(window);
    if (!w) {
        SetError("sabGetClientSize: NULL window handle");
        if (width) *width = 0;
        if (height) *height = 0;
        return;
    }
    w->ClientSize(width, height);
}

void sabInvalidate(sabWindow window) {
    if (sableui::Window* w = W(window)) w->Invalidate();
}

// --- Events -----------------------------------------------------------------

void sabOnMouseDown(sabWindow window, sabMouseButtonCallback cb, void* userdata) {
    sableui::Window* w = W(window);
    if (!w) return;
    if (!cb) { w->OnMouseDown(nullptr); return; }
    w->OnMouseDown([cb, userdata](int b, int x, int y) { cb(userdata, b, x, y); });
}

void sabOnMouseUp(sabWindow window, sabMouseButtonCallback cb, void* userdata) {
    sableui::Window* w = W(window);
    if (!w) return;
    if (!cb) { w->OnMouseUp(nullptr); return; }
    w->OnMouseUp([cb, userdata](int b, int x, int y) { cb(userdata, b, x, y); });
}

void sabOnMouseMove(sabWindow window, sabMouseMoveCallback cb, void* userdata) {
    sableui::Window* w = W(window);
    if (!w) return;
    if (!cb) { w->OnMouseMove(nullptr); return; }
    w->OnMouseMove([cb, userdata](int x, int y) { cb(userdata, x, y); });
}

void sabOnMouseWheel(sabWindow window, sabWheelCallback cb, void* userdata) {
    sableui::Window* w = W(window);
    if (!w) return;
    if (!cb) { w->OnMouseWheel(nullptr); return; }
    w->OnMouseWheel([cb, userdata](int d, int x, int y) { cb(userdata, d, x, y); });
}

void sabOnKeyDown(sabWindow window, sabKeyCallback cb, void* userdata) {
    sableui::Window* w = W(window);
    if (!w) return;
    if (!cb) { w->OnKeyDown(nullptr); return; }
    w->OnKeyDown([cb, userdata](int k, int m) { cb(userdata, k, m); });
}

void sabOnKeyUp(sabWindow window, sabKeyCallback cb, void* userdata) {
    sableui::Window* w = W(window);
    if (!w) return;
    if (!cb) { w->OnKeyUp(nullptr); return; }
    w->OnKeyUp([cb, userdata](int k, int m) { cb(userdata, k, m); });
}

void sabOnText(sabWindow window, sabTextCallback cb, void* userdata) {
    sableui::Window* w = W(window);
    if (!w) return;
    if (!cb) { w->OnText(nullptr); return; }
    w->OnText([cb, userdata](unsigned int cp) { cb(userdata, cp); });
}

void sabOnResize(sabWindow window, sabResizeCallback cb, void* userdata) {
    sableui::Window* w = W(window);
    if (!w) return;
    if (!cb) { w->OnResize(nullptr); return; }
    w->OnResize([cb, userdata](int ww, int hh) { cb(userdata, ww, hh); });
}

void sabOnClose(sabWindow window, sabCloseCallback cb, void* userdata) {
    sableui::Window* w = W(window);
    if (!w) return;
    if (!cb) { w->OnClose(nullptr); return; }
    w->OnClose([cb, userdata]() -> bool { return cb(userdata) != 0; });
}

void sabOnPaint(sabWindow window, sabSimpleCallback cb, void* userdata) {
    sableui::Window* w = W(window);
    if (!w) return;
    if (!cb) { w->OnPaint(nullptr); return; }
    w->OnPaint([cb, userdata]() { cb(userdata); });
}

// --- Timers -----------------------------------------------------------------

sabTimerId sabAddTimer(sabWindow window, int interval_ms, sabSimpleCallback cb, void* userdata) {
    sableui::Window* w = W(window);
    if (!w) {
        SetError("sabAddTimer: NULL window handle");
        return -1;
    }
    if (!cb) {
        SetError("sabAddTimer: callback is NULL");
        return -1;
    }
    std::string err;
    const int id = w->AddTimer(interval_ms,
                               [cb, userdata]() { cb(userdata); },
                               &err);
    if (id < 0) SetError(err.empty() ? "sabAddTimer: failed" : err.c_str());
    else ClearError();
    return id;
}

void sabRemoveTimer(sabWindow window, sabTimerId id) {
    if (sableui::Window* w = W(window)) w->RemoveTimer(id);
}

// --- Widgets ----------------------------------------------------------------

sabWidgetId sabAddButton(sabWindow window, int x, int y, int w, int h,
                         const wchar_t* text, sabClickCallback onClick, void* userdata) {
    sableui::Window* win = W(window);
    if (!win) {
        SetError("sabAddButton: NULL window handle");
        return -1;
    }
    const sabWidgetId id = win->AddButton(
        x, y, w, h, text ? text : L"",
        [onClick, userdata]() { if (onClick) onClick(userdata); });
    if (id < 0) SetError("sabAddButton: failed");
    else ClearError();
    return id;
}

sabWidgetId sabAddLabel(sabWindow window, int x, int y, int w, int h, const wchar_t* text) {
    sableui::Window* win = W(window);
    if (!win) {
        SetError("sabAddLabel: NULL window handle");
        return -1;
    }
    const sabWidgetId id = win->AddLabel(x, y, w, h, text ? text : L"");
    if (id < 0) SetError("sabAddLabel: failed");
    else ClearError();
    return id;
}

sabWidgetId sabAddCheckbox(sabWindow window, int x, int y, int w, int h,
                           const wchar_t* text, int checked,
                           sabChangeCallback onChange, void* userdata) {
    sableui::Window* win = W(window);
    if (!win) {
        SetError("sabAddCheckbox: NULL window handle");
        return -1;
    }
    const sabWidgetId id = win->AddCheckbox(
        x, y, w, h, text ? text : L"", checked != 0,
        [onChange, userdata](bool v) { if (onChange) onChange(userdata, v ? 1 : 0); });
    if (id < 0) SetError("sabAddCheckbox: failed");
    else ClearError();
    return id;
}

sabWidgetId sabAddTextField(sabWindow window, int x, int y, int w, int h,
                            const wchar_t* initial, int masked,
                            sabTextChangeCallback onChange, void* userdata) {
    sableui::Window* win = W(window);
    if (!win) {
        SetError("sabAddTextField: NULL window handle");
        return -1;
    }
    const sabWidgetId id = win->AddTextField(
        x, y, w, h, initial ? initial : L"", masked != 0,
        [onChange, userdata](const std::wstring& t) {
            if (onChange) onChange(userdata, t.c_str());
        });
    if (id < 0) SetError("sabAddTextField: failed");
    else ClearError();
    return id;
}

int sabSetLabelText(sabWindow window, sabWidgetId id, const wchar_t* text) {
    sableui::Window* win = W(window);
    if (!win) {
        SetError("sabSetLabelText: NULL window handle");
        return 0;
    }
    const int ok = win->SetWidgetText(id, text ? text : L"") ? 1 : 0;
    if (!ok) SetError("sabSetLabelText: failed");
    else ClearError();
    return ok;
}

int sabGetWidgetText(sabWindow window, sabWidgetId id,
                     wchar_t* buffer, int buffer_chars) {
    sableui::Window* win = W(window);
    if (!win) {
        SetError("sabGetWidgetText: NULL window handle");
        return -1;
    }
    const std::wstring text = win->GetWidgetText(id);
    const int required = CopyWide(text, buffer, buffer_chars,
                                  "sabGetWidgetText: buffer too small");
    if (buffer && buffer_chars >= required) ClearError();
    return required;
}

int sabSetWidgetChecked(sabWindow window, sabWidgetId id, int on) {
    sableui::Window* win = W(window);
    if (!win) {
        SetError("sabSetWidgetChecked: NULL window handle");
        return 0;
    }
    const int ok = win->SetWidgetChecked(id, on != 0) ? 1 : 0;
    if (!ok) SetError("sabSetWidgetChecked: failed");
    else ClearError();
    return ok;
}

int sabIsWidgetChecked(sabWindow window, sabWidgetId id) {
    sableui::Window* win = W(window);
    return (win && win->IsChecked(id)) ? 1 : 0;
}

int sabSetWidgetEnabled(sabWindow window, sabWidgetId id, int on) {
    sableui::Window* win = W(window);
    if (!win) {
        SetError("sabSetWidgetEnabled: NULL window handle");
        return 0;
    }
    const int ok = win->SetWidgetEnabled(id, on != 0) ? 1 : 0;
    if (!ok) SetError("sabSetWidgetEnabled: failed");
    else ClearError();
    return ok;
}

int sabSetWidgetVisible(sabWindow window, sabWidgetId id, int on) {
    sableui::Window* win = W(window);
    if (!win) {
        SetError("sabSetWidgetVisible: NULL window handle");
        return 0;
    }
    const int ok = win->SetWidgetVisible(id, on != 0) ? 1 : 0;
    if (!ok) SetError("sabSetWidgetVisible: failed");
    else ClearError();
    return ok;
}

int sabRemoveWidget(sabWindow window, sabWidgetId id) {
    sableui::Window* win = W(window);
    if (!win) {
        SetError("sabRemoveWidget: NULL window handle");
        return 0;
    }
    const int ok = win->RemoveWidget(id) ? 1 : 0;
    if (!ok) SetError("sabRemoveWidget: failed");
    else ClearError();
    return ok;
}

int sabMoveWidgetToFront(sabWindow window, sabWidgetId id) {
    sableui::Window* win = W(window);
    if (!win) {
        SetError("sabMoveWidgetToFront: NULL window handle");
        return 0;
    }
    const int ok = win->MoveWidgetToFront(id) ? 1 : 0;
    if (!ok) SetError("sabMoveWidgetToFront: failed");
    else ClearError();
    return ok;
}

// --- Layout anchors ---------------------------------------------------------

int sabSetWidgetAnchors(sabWindow window, sabWidgetId id, unsigned anchors) {
    sableui::Window* win = W(window);
    if (!win) {
        SetError("sabSetWidgetAnchors: NULL window handle");
        return 0;
    }
    const int ok = win->SetWidgetAnchors(id, anchors) ? 1 : 0;
    if (!ok) SetError("sabSetWidgetAnchors: failed");
    else ClearError();
    return ok;
}

// --- Scroll areas -----------------------------------------------------------

sabWidgetId sabAddScrollArea(sabWindow window, int x, int y, int w, int h) {
    sableui::Window* win = W(window);
    if (!win) {
        SetError("sabAddScrollArea: NULL window handle");
        return -1;
    }
    const sabWidgetId id = win->AddScrollArea(x, y, w, h);
    if (id < 0) SetError("sabAddScrollArea: failed");
    else ClearError();
    return id;
}

sabWidgetId sabAddButtonIn(sabWindow window, sabWidgetId area,
                           int x, int y, int w, int h, const wchar_t* text,
                           sabClickCallback onClick, void* userdata) {
    sableui::Window* win = W(window);
    if (!win) {
        SetError("sabAddButtonIn: NULL window handle");
        return -1;
    }
    const sabWidgetId id = win->AddButtonIn(
        area, x, y, w, h, text ? text : L"",
        [onClick, userdata]() { if (onClick) onClick(userdata); });
    if (id < 0) SetError("sabAddButtonIn: failed");
    else ClearError();
    return id;
}

sabWidgetId sabAddLabelIn(sabWindow window, sabWidgetId area,
                          int x, int y, int w, int h, const wchar_t* text) {
    sableui::Window* win = W(window);
    if (!win) {
        SetError("sabAddLabelIn: NULL window handle");
        return -1;
    }
    const sabWidgetId id = win->AddLabelIn(area, x, y, w, h, text ? text : L"");
    if (id < 0) SetError("sabAddLabelIn: failed");
    else ClearError();
    return id;
}

sabWidgetId sabAddCheckboxIn(sabWindow window, sabWidgetId area,
                             int x, int y, int w, int h,
                             const wchar_t* text, int checked,
                             sabChangeCallback onChange, void* userdata) {
    sableui::Window* win = W(window);
    if (!win) {
        SetError("sabAddCheckboxIn: NULL window handle");
        return -1;
    }
    const sabWidgetId id = win->AddCheckboxIn(
        area, x, y, w, h, text ? text : L"", checked != 0,
        [onChange, userdata](bool v) { if (onChange) onChange(userdata, v ? 1 : 0); });
    if (id < 0) SetError("sabAddCheckboxIn: failed");
    else ClearError();
    return id;
}

sabWidgetId sabAddTextFieldIn(sabWindow window, sabWidgetId area,
                              int x, int y, int w, int h,
                              const wchar_t* initial, int masked,
                              sabTextChangeCallback onChange, void* userdata) {
    sableui::Window* win = W(window);
    if (!win) {
        SetError("sabAddTextFieldIn: NULL window handle");
        return -1;
    }
    const sabWidgetId id = win->AddTextFieldIn(
        area, x, y, w, h, initial ? initial : L"", masked != 0,
        [onChange, userdata](const std::wstring& t) {
            if (onChange) onChange(userdata, t.c_str());
        });
    if (id < 0) SetError("sabAddTextFieldIn: failed");
    else ClearError();
    return id;
}

int sabSetScrollOffset(sabWindow window, sabWidgetId area, int offset) {
    sableui::Window* win = W(window);
    if (!win) {
        SetError("sabSetScrollOffset: NULL window handle");
        return 0;
    }
    const int ok = win->SetScrollOffset(area, offset) ? 1 : 0;
    if (!ok) SetError("sabSetScrollOffset: failed");
    else ClearError();
    return ok;
}

int sabGetScrollOffset(sabWindow window, sabWidgetId area) {
    sableui::Window* win = W(window);
    if (!win) {
        SetError("sabGetScrollOffset: NULL window handle");
        return -1;
    }
    return win->GetScrollOffset(area);
}

// --- Drawing (paint callback only) ------------------------------------------

void sabSetFillColor(sabWindow window, float r, float g, float b, float a) {
    if (sableui::Window* win = W(window)) win->SetFillColor({r, g, b, a});
}

void sabSetStrokeColor(sabWindow window, float r, float g, float b, float a) {
    if (sableui::Window* win = W(window)) win->SetStrokeColor({r, g, b, a});
}

void sabSetStrokeWidth(sabWindow window, float width) {
    if (sableui::Window* win = W(window)) win->SetStrokeWidth(width);
}

void sabClear(sabWindow window) {
    if (sableui::Window* win = W(window)) win->ClearWithFill();
}

void sabFillRect(sabWindow window, float x, float y, float w, float h) {
    if (sableui::Window* win = W(window)) win->FillRect(x, y, w, h);
}

void sabDrawRect(sabWindow window, float x, float y, float w, float h) {
    if (sableui::Window* win = W(window)) win->DrawRect(x, y, w, h);
}

void sabFillRoundedRect(sabWindow window, float x, float y, float w, float h,
                        float radius) {
    if (sableui::Window* win = W(window)) win->FillRoundedRect(x, y, w, h, radius);
}

void sabFillEllipse(sabWindow window, float cx, float cy, float rx, float ry) {
    if (sableui::Window* win = W(window)) win->FillEllipse(cx, cy, rx, ry);
}

void sabDrawEllipse(sabWindow window, float cx, float cy, float rx, float ry) {
    if (sableui::Window* win = W(window)) win->DrawEllipse(cx, cy, rx, ry);
}

void sabDrawLine(sabWindow window, float x1, float y1, float x2, float y2) {
    if (sableui::Window* win = W(window)) win->DrawLine(x1, y1, x2, y2);
}

void sabDrawText(sabWindow window, float x, float y, float w, float h,
                 const wchar_t* text, float font_size) {
    sableui::Window* win = W(window);
    if (!win) {
        SetError("sabDrawText: NULL window handle");
        return;
    }
    win->DrawTextAt(x, y, w, h, text ? text : L"", font_size);
}

} // extern "C"