// Sableui - C API implementation.
// Thin wrappers over the internal C++ core. Nothing throws across the C
// boundary: failures are reported through return codes and sabGetLastError.

#include "sableui.hpp"

#include <cstring>
#include <new>
#include <string>

namespace {

thread_local char g_lastError[256] = "no error";

void SetError(const char* msg) {
    std::strncpy(g_lastError, msg ? msg : "unknown error",
                 sizeof(g_lastError) - 1);
    g_lastError[sizeof(g_lastError) - 1] = '\0';
}

std::wstring FromW(const wchar_t* s) {
    return s ? std::wstring(s) : std::wstring();
}

sableui::Window* W(sabWindow window) {
    return reinterpret_cast<sableui::Window*>(window);
}

} // namespace

// --- Window lifecycle ----------------------------------------------------------

SABLEUI_API sabWindow sabCreateWindow(const wchar_t* title, int width, int height) {
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
    if (!win->Create(FromW(title), width, height, &err)) {
        SetError(err.empty() ? "sabCreateWindow: failed" : err.c_str());
        delete win;
        return nullptr;
    }
    SetError("no error");
    return reinterpret_cast<sabWindow>(win);
}

SABLEUI_API void sabDestroyWindow(sabWindow window) {
    delete W(window);
}

SABLEUI_API void sabRun(sabWindow window) {
    sableui::Window* w = W(window);
    if (!w) {
        SetError("sabRun: NULL window handle");
        return;
    }
    w->Run();
}

SABLEUI_API void sabSetTitle(sabWindow window, const wchar_t* title) {
    if (sableui::Window* w = W(window)) w->SetTitle(FromW(title));
}

SABLEUI_API void sabSetResizable(sabWindow window, int on) {
    if (sableui::Window* w = W(window)) w->SetResizable(on != 0);
}

SABLEUI_API void sabSetBorderless(sabWindow window, int on) {
    if (sableui::Window* w = W(window)) w->SetBorderless(on != 0);
}

SABLEUI_API void sabSetMinimumSize(sabWindow window, int width, int height) {
    if (sableui::Window* w = W(window)) w->SetMinimumSize(width, height);
}

SABLEUI_API void sabCloseWindow(sabWindow window) {
    if (sableui::Window* w = W(window)) w->CloseRequest();
}

SABLEUI_API void sabGetClientSize(sabWindow window, int* width, int* height) {
    sableui::Window* w = W(window);
    if (!w) {
        SetError("sabGetClientSize: NULL window handle");
        if (width) *width = 0;
        if (height) *height = 0;
        return;
    }
    w->ClientSize(width, height);
}

SABLEUI_API void sabInvalidate(sabWindow window) {
    if (sableui::Window* w = W(window)) w->Invalidate();
}

// --- Events ---------------------------------------------------------------------

SABLEUI_API void sabOnMouseDown(sabWindow window, sabMouseButtonCallback cb, void* userdata) {
    sableui::Window* w = W(window);
    if (!w) return;
    if (!cb) { w->OnMouseDown(nullptr); return; }
    w->OnMouseDown([cb, userdata](int button, int x, int y) {
        cb(userdata, button, x, y);
    });
}

SABLEUI_API void sabOnMouseUp(sabWindow window, sabMouseButtonCallback cb, void* userdata) {
    sableui::Window* w = W(window);
    if (!w) return;
    if (!cb) { w->OnMouseUp(nullptr); return; }
    w->OnMouseUp([cb, userdata](int button, int x, int y) {
        cb(userdata, button, x, y);
    });
}

SABLEUI_API void sabOnMouseMove(sabWindow window, sabMouseMoveCallback cb, void* userdata) {
    sableui::Window* w = W(window);
    if (!w) return;
    if (!cb) { w->OnMouseMove(nullptr); return; }
    w->OnMouseMove([cb, userdata](int x, int y) {
        cb(userdata, x, y);
    });
}

SABLEUI_API void sabOnMouseWheel(sabWindow window, sabWheelCallback cb, void* userdata) {
    sableui::Window* w = W(window);
    if (!w) return;
    if (!cb) { w->OnMouseWheel(nullptr); return; }
    w->OnMouseWheel([cb, userdata](int delta, int x, int y) {
        cb(userdata, delta, x, y);
    });
}

SABLEUI_API void sabOnKeyDown(sabWindow window, sabKeyCallback cb, void* userdata) {
    sableui::Window* w = W(window);
    if (!w) return;
    if (!cb) { w->OnKeyDown(nullptr); return; }
    w->OnKeyDown([cb, userdata](int key, int mods) {
        cb(userdata, key, mods);
    });
}

SABLEUI_API void sabOnKeyUp(sabWindow window, sabKeyCallback cb, void* userdata) {
    sableui::Window* w = W(window);
    if (!w) return;
    if (!cb) { w->OnKeyUp(nullptr); return; }
    w->OnKeyUp([cb, userdata](int key, int mods) {
        cb(userdata, key, mods);
    });
}

SABLEUI_API void sabOnText(sabWindow window, sabTextCallback cb, void* userdata) {
    sableui::Window* w = W(window);
    if (!w) return;
    if (!cb) { w->OnText(nullptr); return; }
    w->OnText([cb, userdata](unsigned int codepoint) {
        cb(userdata, codepoint);
    });
}

SABLEUI_API void sabOnResize(sabWindow window, sabResizeCallback cb, void* userdata) {
    sableui::Window* w = W(window);
    if (!w) return;
    if (!cb) { w->OnResize(nullptr); return; }
    w->OnResize([cb, userdata](int width, int height) {
        cb(userdata, width, height);
    });
}

SABLEUI_API void sabOnClose(sabWindow window, sabCloseCallback cb, void* userdata) {
    sableui::Window* w = W(window);
    if (!w) return;
    if (!cb) { w->OnClose(nullptr); return; }
    w->OnClose([cb, userdata]() -> bool {
        return cb(userdata) != 0;
    });
}

SABLEUI_API void sabOnPaint(sabWindow window, sabSimpleCallback cb, void* userdata) {
    sableui::Window* w = W(window);
    if (!w) return;
    if (!cb) { w->OnPaint(nullptr); return; }
    w->OnPaint([cb, userdata]() {
        cb(userdata);
    });
}

// --- Timers ---------------------------------------------------------------------

SABLEUI_API sabTimerId sabAddTimer(sabWindow window, int interval_ms,
                                   sabSimpleCallback cb, void* userdata) {
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
    const int id = w->AddTimer(interval_ms, [cb, userdata]() {
        cb(userdata);
    }, &err);
    if (id < 0) SetError(err.empty() ? "sabAddTimer: failed" : err.c_str());
    return id;
}

SABLEUI_API void sabRemoveTimer(sabWindow window, sabTimerId id) {
    if (sableui::Window* w = W(window)) w->RemoveTimer(id);
}

// --- Drawing --------------------------------------------------------------------

SABLEUI_API void sabSetFillColor(sabWindow window, float r, float g, float b, float a) {
    if (sableui::Window* w = W(window)) w->SetFillColor(sableui::Color{r, g, b, a});
}

SABLEUI_API void sabSetStrokeColor(sabWindow window, float r, float g, float b, float a) {
    if (sableui::Window* w = W(window)) w->SetStrokeColor(sableui::Color{r, g, b, a});
}

SABLEUI_API void sabSetStrokeWidth(sabWindow window, float width) {
    if (sableui::Window* w = W(window)) w->SetStrokeWidth(width);
}

SABLEUI_API void sabClear(sabWindow window) {
    if (sableui::Window* w = W(window)) w->ClearWithFill();
}

SABLEUI_API void sabFillRect(sabWindow window, float x, float y, float w, float h) {
    if (sableui::Window* win = W(window)) win->FillRect(x, y, w, h);
}

SABLEUI_API void sabDrawRect(sabWindow window, float x, float y, float w, float h) {
    if (sableui::Window* win = W(window)) win->DrawRect(x, y, w, h);
}

SABLEUI_API void sabFillRoundedRect(sabWindow window, float x, float y, float w, float h,
                                    float radius) {
    if (sableui::Window* win = W(window)) win->FillRoundedRect(x, y, w, h, radius);
}

SABLEUI_API void sabFillEllipse(sabWindow window, float cx, float cy, float rx, float ry) {
    if (sableui::Window* win = W(window)) win->FillEllipse(cx, cy, rx, ry);
}

SABLEUI_API void sabDrawEllipse(sabWindow window, float cx, float cy, float rx, float ry) {
    if (sableui::Window* win = W(window)) win->DrawEllipse(cx, cy, rx, ry);
}

SABLEUI_API void sabDrawLine(sabWindow window, float x1, float y1, float x2, float y2) {
    if (sableui::Window* win = W(window)) win->DrawLine(x1, y1, x2, y2);
}

SABLEUI_API void sabDrawText(sabWindow window, float x, float y, float w, float h,
                             const wchar_t* text, float font_size) {
    sableui::Window* win = W(window);
    if (!win) {
        SetError("sabDrawText: NULL window handle");
        return;
    }
    win->DrawTextAt(x, y, w, h, FromW(text), font_size);
}

// --- Widgets --------------------------------------------------------------------

SABLEUI_API sabWidgetId sabAddButton(sabWindow window, int x, int y, int w, int h,
                                     const wchar_t* text,
                                     sabClickCallback onClick, void* userdata) {
    sableui::Window* win = W(window);
    if (!win) {
        SetError("sabAddButton: NULL window handle");
        return -1;
    }
    return win->AddButton(x, y, w, h, FromW(text), [onClick, userdata]() {
        if (onClick) onClick(userdata);
    });
}

SABLEUI_API sabWidgetId sabAddLabel(sabWindow window, int x, int y, int w, int h,
                                    const wchar_t* text) {
    sableui::Window* win = W(window);
    if (!win) {
        SetError("sabAddLabel: NULL window handle");
        return -1;
    }
    return win->AddLabel(x, y, w, h, FromW(text));
}

SABLEUI_API sabWidgetId sabAddCheckbox(sabWindow window, int x, int y, int w, int h,
                                       const wchar_t* text, int checked,
                                       sabChangeCallback onChange, void* userdata) {
    sableui::Window* win = W(window);
    if (!win) {
        SetError("sabAddCheckbox: NULL window handle");
        return -1;
    }
    return win->AddCheckbox(x, y, w, h, FromW(text), checked != 0,
                            [onChange, userdata](bool on) {
        if (onChange) onChange(userdata, on ? 1 : 0);
    });
}

SABLEUI_API sabWidgetId sabAddTextField(sabWindow window, int x, int y, int w, int h,
                                        const wchar_t* initial, int masked,
                                        sabTextChangeCallback onChange,
                                        void* userdata) {
    sableui::Window* win = W(window);
    if (!win) {
        SetError("sabAddTextField: NULL window handle");
        return -1;
    }
    return win->AddTextField(x, y, w, h, FromW(initial), masked != 0,
                             [onChange, userdata](const std::wstring& t) {
        if (onChange) onChange(userdata, t.c_str());
    });
}

SABLEUI_API int sabSetLabelText(sabWindow window, sabWidgetId id, const wchar_t* text) {
    sableui::Window* win = W(window);
    if (!win) {
        SetError("sabSetLabelText: NULL window handle");
        return 0;
    }
    return win->SetWidgetText(id, FromW(text)) ? 1 : 0;
}

SABLEUI_API int sabGetWidgetText(sabWindow window, sabWidgetId id,
                                 wchar_t* buffer, int buffer_chars) {
    sableui::Window* win = W(window);
    if (!win) {
        SetError("sabGetWidgetText: NULL window handle");
        return -1;
    }
    if (!buffer || buffer_chars <= 0) {
        SetError("sabGetWidgetText: invalid buffer");
        return -1;
    }
    const std::wstring text = win->GetWidgetText(id);
    const int required = static_cast<int>(text.size()) + 1;  // incl. NUL
    if (buffer_chars < required) {
        SetError("sabGetWidgetText: buffer too small");
        return required;
    }
    std::wmemcpy(buffer, text.c_str(), text.size() + 1);
    SetError("no error");
    return required;
}

SABLEUI_API int sabSetWidgetChecked(sabWindow window, sabWidgetId id, int on) {
    sableui::Window* win = W(window);
    if (!win) {
        SetError("sabSetWidgetChecked: NULL window handle");
        return 0;
    }
    return win->SetWidgetChecked(id, on != 0) ? 1 : 0;
}

SABLEUI_API int sabIsWidgetChecked(sabWindow window, sabWidgetId id) {
    sableui::Window* win = W(window);
    if (!win) return 0;
    return win->IsChecked(id) ? 1 : 0;
}

SABLEUI_API int sabSetWidgetEnabled(sabWindow window, sabWidgetId id, int on) {
    sableui::Window* win = W(window);
    if (!win) {
        SetError("sabSetWidgetEnabled: NULL window handle");
        return 0;
    }
    return win->SetWidgetEnabled(id, on != 0) ? 1 : 0;
}

SABLEUI_API int sabSetWidgetVisible(sabWindow window, sabWidgetId id, int on) {
    sableui::Window* win = W(window);
    if (!win) {
        SetError("sabSetWidgetVisible: NULL window handle");
        return 0;
    }
    return win->SetWidgetVisible(id, on != 0) ? 1 : 0;
}

SABLEUI_API int sabRemoveWidget(sabWindow window, sabWidgetId id) {
    sableui::Window* win = W(window);
    if (!win) {
        SetError("sabRemoveWidget: NULL window handle");
        return 0;
    }
    return win->RemoveWidget(id) ? 1 : 0;
}

SABLEUI_API int sabMoveWidgetToFront(sabWindow window, sabWidgetId id) {
    sableui::Window* win = W(window);
    if (!win) {
        SetError("sabMoveWidgetToFront: NULL window handle");
        return 0;
    }
    return win->MoveWidgetToFront(id) ? 1 : 0;
}

// --- Layout anchors --------------------------------------------------------------

SABLEUI_API int sabSetWidgetAnchors(sabWindow window, sabWidgetId id, unsigned anchors) {
    sableui::Window* win = W(window);
    if (!win) {
        SetError("sabSetWidgetAnchors: NULL window handle");
        return 0;
    }
    return win->SetWidgetAnchors(id, anchors) ? 1 : 0;
}

// --- Scroll areas ---------------------------------------------------------------

SABLEUI_API sabWidgetId sabAddScrollArea(sabWindow window, int x, int y, int w, int h) {
    sableui::Window* win = W(window);
    if (!win) {
        SetError("sabAddScrollArea: NULL window handle");
        return -1;
    }
    return win->AddScrollArea(x, y, w, h);
}

SABLEUI_API sabWidgetId sabAddButtonIn(sabWindow window, sabWidgetId area,
                                       int x, int y, int w, int h,
                                       const wchar_t* text,
                                       sabClickCallback onClick, void* userdata) {
    sableui::Window* win = W(window);
    if (!win) {
        SetError("sabAddButtonIn: NULL window handle");
        return -1;
    }
    return win->AddButtonIn(area, x, y, w, h, FromW(text), [onClick, userdata]() {
        if (onClick) onClick(userdata);
    });
}

SABLEUI_API sabWidgetId sabAddLabelIn(sabWindow window, sabWidgetId area,
                                      int x, int y, int w, int h,
                                      const wchar_t* text) {
    sableui::Window* win = W(window);
    if (!win) {
        SetError("sabAddLabelIn: NULL window handle");
        return -1;
    }
    return win->AddLabelIn(area, x, y, w, h, FromW(text));
}

SABLEUI_API sabWidgetId sabAddCheckboxIn(sabWindow window, sabWidgetId area,
                                         int x, int y, int w, int h,
                                         const wchar_t* text, int checked,
                                         sabChangeCallback onChange, void* userdata) {
    sableui::Window* win = W(window);
    if (!win) {
        SetError("sabAddCheckboxIn: NULL window handle");
        return -1;
    }
    return win->AddCheckboxIn(area, x, y, w, h, FromW(text), checked != 0,
                              [onChange, userdata](bool on) {
        if (onChange) onChange(userdata, on ? 1 : 0);
    });
}

SABLEUI_API sabWidgetId sabAddTextFieldIn(sabWindow window, sabWidgetId area,
                                          int x, int y, int w, int h,
                                          const wchar_t* initial, int masked,
                                          sabTextChangeCallback onChange,
                                          void* userdata) {
    sableui::Window* win = W(window);
    if (!win) {
        SetError("sabAddTextFieldIn: NULL window handle");
        return -1;
    }
    return win->AddTextFieldIn(area, x, y, w, h, FromW(initial), masked != 0,
                               [onChange, userdata](const std::wstring& t) {
        if (onChange) onChange(userdata, t.c_str());
    });
}

SABLEUI_API int sabSetScrollOffset(sabWindow window, sabWidgetId area, int offset) {
    sableui::Window* win = W(window);
    if (!win) {
        SetError("sabSetScrollOffset: NULL window handle");
        return 0;
    }
    return win->SetScrollOffset(area, offset) ? 1 : 0;
}

SABLEUI_API int sabGetScrollOffset(sabWindow window, sabWidgetId area) {
    sableui::Window* win = W(window);
    if (!win) return -1;
    return win->GetScrollOffset(area);
}

// --- Errors ---------------------------------------------------------------------

SABLEUI_API const char* sabGetLastError(void) {
    return g_lastError;
}