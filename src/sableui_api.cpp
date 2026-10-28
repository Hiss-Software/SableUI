// Implementation of the public C ABI over the internal core. Nothing here is
// allowed to throw across the boundary: every failure is reported through the
// return value and sabGetLastError.

#include <sableui/sableui.h>

#include "sableui.hpp"

#include <exception>
#include <memory>
#include <new>
#include <string>

namespace {

std::string& LastErrorStorage() {
    static std::string message = "no error";
    return message;
}

void SetError(const std::string& message) { LastErrorStorage() = message; }

} // namespace

extern "C" {

const char* sabGetLastError(void) {
    return LastErrorStorage().c_str();
}

// --- Window lifecycle -------------------------------------------------------

sabWindow sabCreateWindow(const wchar_t* title, int width, int height) {
    try {
        auto window = std::make_unique<sableui::Window>();
        std::string error;
        if (!window->Create(title ? title : L"", width, height, &error)) {
            SetError(error);
            return nullptr;
        }
        return reinterpret_cast<sabWindow>(window.release());
    } catch (const std::exception& e) {
        SetError(std::string("sabCreateWindow: ") + e.what());
    } catch (...) {
        SetError("sabCreateWindow: unknown error");
    }
    return nullptr;
}

void sabDestroyWindow(sabWindow window) {
    if (!window) return;
    delete reinterpret_cast<sableui::Window*>(window);
}

void sabRun(sabWindow window) {
    auto* win = reinterpret_cast<sableui::Window*>(window);
    if (win) win->Run();
}

void sabSetTitle(sabWindow window, const wchar_t* title) {
    auto* win = reinterpret_cast<sableui::Window*>(window);
    if (win) win->SetTitle(title ? title : L"");
}

void sabSetResizable(sabWindow window, int on) {
    auto* win = reinterpret_cast<sableui::Window*>(window);
    if (win) win->SetResizable(on != 0);
}

void sabSetBorderless(sabWindow window, int on) {
    auto* win = reinterpret_cast<sableui::Window*>(window);
    if (win) win->SetBorderless(on != 0);
}

void sabSetMinimumSize(sabWindow window, int width, int height) {
    auto* win = reinterpret_cast<sableui::Window*>(window);
    if (win) win->SetMinimumSize(width, height);
}

void sabCloseWindow(sabWindow window) {
    auto* win = reinterpret_cast<sableui::Window*>(window);
    if (win) win->CloseRequest();
}

void sabGetClientSize(sabWindow window, int* width, int* height) {
    auto* win = reinterpret_cast<sableui::Window*>(window);
    if (win) {
        win->ClientSize(width, height);
    } else {
        if (width) *width = 0;
        if (height) *height = 0;
    }
}

void sabInvalidate(sabWindow window) {
    auto* win = reinterpret_cast<sableui::Window*>(window);
    if (win) win->Invalidate();
}

// --- Events -----------------------------------------------------------------

void sabOnMouseDown(sabWindow window, sabMouseButtonCallback cb, void* userdata) {
    auto* win = reinterpret_cast<sableui::Window*>(window);
    if (!win) return;
    if (!cb) { win->OnMouseDown(nullptr); return; }
    win->OnMouseDown([cb, userdata](int b, int x, int y) { cb(userdata, b, x, y); });
}

void sabOnMouseUp(sabWindow window, sabMouseButtonCallback cb, void* userdata) {
    auto* win = reinterpret_cast<sableui::Window*>(window);
    if (!win) return;
    if (!cb) { win->OnMouseUp(nullptr); return; }
    win->OnMouseUp([cb, userdata](int b, int x, int y) { cb(userdata, b, x, y); });
}

void sabOnMouseMove(sabWindow window, sabMouseMoveCallback cb, void* userdata) {
    auto* win = reinterpret_cast<sableui::Window*>(window);
    if (!win) return;
    if (!cb) { win->OnMouseMove(nullptr); return; }
    win->OnMouseMove([cb, userdata](int x, int y) { cb(userdata, x, y); });
}

void sabOnMouseWheel(sabWindow window, sabWheelCallback cb, void* userdata) {
    auto* win = reinterpret_cast<sableui::Window*>(window);
    if (!win) return;
    if (!cb) { win->OnMouseWheel(nullptr); return; }
    win->OnMouseWheel([cb, userdata](int d, int x, int y) { cb(userdata, d, x, y); });
}

void sabOnKeyDown(sabWindow window, sabKeyCallback cb, void* userdata) {
    auto* win = reinterpret_cast<sableui::Window*>(window);
    if (!win) return;
    if (!cb) { win->OnKeyDown(nullptr); return; }
    win->OnKeyDown([cb, userdata](int k, int m) { cb(userdata, k, m); });
}

void sabOnKeyUp(sabWindow window, sabKeyCallback cb, void* userdata) {
    auto* win = reinterpret_cast<sableui::Window*>(window);
    if (!win) return;
    if (!cb) { win->OnKeyUp(nullptr); return; }
    win->OnKeyUp([cb, userdata](int k, int m) { cb(userdata, k, m); });
}

void sabOnText(sabWindow window, sabTextCallback cb, void* userdata) {
    auto* win = reinterpret_cast<sableui::Window*>(window);
    if (!win) return;
    if (!cb) { win->OnText(nullptr); return; }
    win->OnText([cb, userdata](unsigned int cp) { cb(userdata, cp); });
}

void sabOnResize(sabWindow window, sabResizeCallback cb, void* userdata) {
    auto* win = reinterpret_cast<sableui::Window*>(window);
    if (!win) return;
    if (!cb) { win->OnResize(nullptr); return; }
    win->OnResize([cb, userdata](int w, int h) { cb(userdata, w, h); });
}

void sabOnClose(sabWindow window, sabCloseCallback cb, void* userdata) {
    auto* win = reinterpret_cast<sableui::Window*>(window);
    if (!win) return;
    if (!cb) { win->OnClose(nullptr); return; }
    win->OnClose([cb, userdata]() -> bool { return cb(userdata) != 0; });
}

void sabOnPaint(sabWindow window, sabSimpleCallback cb, void* userdata) {
    auto* win = reinterpret_cast<sableui::Window*>(window);
    if (!win) return;
    if (!cb) { win->OnPaint(nullptr); return; }
    win->OnPaint([cb, userdata]() { cb(userdata); });
}

// --- Timers -----------------------------------------------------------------

sabTimerId sabAddTimer(sabWindow window, int interval_ms, sabSimpleCallback cb, void* userdata) {
    auto* win = reinterpret_cast<sableui::Window*>(window);
    if (!win || !cb) {
        SetError("sabAddTimer: invalid argument");
        return -1;
    }
    try {
        std::string error;
        int id = win->AddTimer(interval_ms, [cb, userdata]() { cb(userdata); }, &error);
        if (id < 0) SetError(error.empty() ? "sabAddTimer: failed" : error);
        return id;
    } catch (const std::exception& e) {
        SetError(std::string("sabAddTimer: ") + e.what());
    } catch (...) {
        SetError("sabAddTimer: unknown error");
    }
    return -1;
}

void sabRemoveTimer(sabWindow window, sabTimerId id) {
    auto* win = reinterpret_cast<sableui::Window*>(window);
    if (win) win->RemoveTimer(id);
}

// --- Widgets ----------------------------------------------------------------

sabWidgetId sabAddButton(sabWindow window, int x, int y, int w, int h,
                         const wchar_t* text, sabClickCallback onClick, void* userdata) {
    auto* win = reinterpret_cast<sableui::Window*>(window);
    if (!win || !text) {
        SetError("sabAddButton: invalid argument");
        return -1;
    }
    try {
        std::string error;
        int id = win->AddButton(x, y, w, h, text,
                                [onClick, userdata]() {
                                    if (onClick) onClick(userdata);
                                },
                                &error);
        if (id < 0) SetError(error.empty() ? "sabAddButton: failed" : error);
        return id;
    } catch (const std::exception& e) {
        SetError(std::string("sabAddButton: ") + e.what());
    } catch (...) {
        SetError("sabAddButton: unknown error");
    }
    return -1;
}

sabWidgetId sabAddLabel(sabWindow window, int x, int y, int w, int h, const wchar_t* text) {
    auto* win = reinterpret_cast<sableui::Window*>(window);
    if (!win || !text) {
        SetError("sabAddLabel: invalid argument");
        return -1;
    }
    try {
        std::string error;
        int id = win->AddLabel(x, y, w, h, text, &error);
        if (id < 0) SetError(error.empty() ? "sabAddLabel: failed" : error);
        return id;
    } catch (const std::exception& e) {
        SetError(std::string("sabAddLabel: ") + e.what());
    } catch (...) {
        SetError("sabAddLabel: unknown error");
    }
    return -1;
}

sabWidgetId sabAddCheckbox(sabWindow window, int x, int y, int w, int h,
                           const wchar_t* text, int checked,
                           sabChangeCallback onChange, void* userdata) {
    auto* win = reinterpret_cast<sableui::Window*>(window);
    if (!win || !text) {
        SetError("sabAddCheckbox: invalid argument");
        return -1;
    }
    try {
        std::string error;
        int id = win->AddCheckbox(x, y, w, h, text, checked != 0,
                                  [onChange, userdata](bool v) {
                                      if (onChange) onChange(userdata, v ? 1 : 0);
                                  },
                                  &error);
        if (id < 0) SetError(error.empty() ? "sabAddCheckbox: failed" : error);
        return id;
    } catch (const std::exception& e) {
        SetError(std::string("sabAddCheckbox: ") + e.what());
    } catch (...) {
        SetError("sabAddCheckbox: unknown error");
    }
    return -1;
}

sabWidgetId sabAddTextField(sabWindow window, int x, int y, int w, int h,
                            const wchar_t* initial, int masked,
                            sabTextChangeCallback onChange, void* userdata) {
    auto* win = reinterpret_cast<sableui::Window*>(window);
    if (!win) {
        SetError("sabAddTextField: invalid argument");
        return -1;
    }
    try {
        std::string error;
        std::wstring initial_text = initial ? initial : L"";
        int id = win->AddTextField(x, y, w, h, initial_text, masked != 0,
                                   [onChange, userdata](const std::wstring& t) {
                                       if (onChange) onChange(userdata, t.c_str());
                                   });
        if (id < 0) SetError("sabAddTextField: failed");
        return id;
    } catch (const std::exception& e) {
        SetError(std::string("sabAddTextField: ") + e.what());
    } catch (...) {
        SetError("sabAddTextField: unknown error");
    }
    return -1;
}

int sabSetLabelText(sabWindow window, sabWidgetId id, const wchar_t* text) {
    auto* win = reinterpret_cast<sableui::Window*>(window);
    if (!win || !text) {
        SetError("sabSetLabelText: invalid argument");
        return 0;
    }
    try {
        return win->SetLabelText(id, text) ? 1 : 0;
    } catch (const std::exception& e) {
        SetError(std::string("sabSetLabelText: ") + e.what());
    } catch (...) {
        SetError("sabSetLabelText: unknown error");
    }
    return 0;
}

int sabGetWidgetText(sabWindow window, sabWidgetId id,
                     wchar_t* buffer, int buffer_chars) {
    auto* win = reinterpret_cast<sableui::Window*>(window);
    if (!win) {
        SetError("sabGetWidgetText: invalid argument");
        return 0;
    }
    try {
        const std::wstring text = win->GetWidgetText(id);
        const int required = static_cast<int>(text.size()) + 1;
        if (!buffer || buffer_chars < required) {
            SetError("sabGetWidgetText: buffer too small");
            return required;
        }
        std::wmemcpy(buffer, text.c_str(), text.size());
        buffer[text.size()] = L'\0';
        SetError("");
        return required;
    } catch (const std::exception& e) {
        SetError(std::string("sabGetWidgetText: ") + e.what());
    } catch (...) {
        SetError("sabGetWidgetText: unknown error");
    }
    return 0;
}

int sabSetWidgetChecked(sabWindow window, sabWidgetId id, int on) {
    auto* win = reinterpret_cast<sableui::Window*>(window);
    if (!win) return 0;
    try {
        return win->SetWidgetChecked(id, on != 0) ? 1 : 0;
    } catch (const std::exception& e) {
        SetError(std::string("sabSetWidgetChecked: ") + e.what());
    } catch (...) {
        SetError("sabSetWidgetChecked: unknown error");
    }
    return 0;
}

int sabIsWidgetChecked(sabWindow window, sabWidgetId id) {
    auto* win = reinterpret_cast<sableui::Window*>(window);
    return (win && win->IsChecked(id)) ? 1 : 0;
}

int sabSetWidgetEnabled(sabWindow window, sabWidgetId id, int on) {
    auto* win = reinterpret_cast<sableui::Window*>(window);
    if (!win) return 0;
    try {
        return win->SetWidgetEnabled(id, on != 0) ? 1 : 0;
    } catch (const std::exception& e) {
        SetError(std::string("sabSetWidgetEnabled: ") + e.what());
    } catch (...) {
        SetError("sabSetWidgetEnabled: unknown error");
    }
    return 0;
}

int sabSetWidgetVisible(sabWindow window, sabWidgetId id, int on) {
    auto* win = reinterpret_cast<sableui::Window*>(window);
    if (!win) return 0;
    try {
        return win->SetWidgetVisible(id, on != 0) ? 1 : 0;
    } catch (const std::exception& e) {
        SetError(std::string("sabSetWidgetVisible: ") + e.what());
    } catch (...) {
        SetError("sabSetWidgetVisible: unknown error");
    }
    return 0;
}

int sabRemoveWidget(sabWindow window, sabWidgetId id) {
    auto* win = reinterpret_cast<sableui::Window*>(window);
    if (!win) return 0;
    try {
        return win->RemoveWidget(id) ? 1 : 0;
    } catch (const std::exception& e) {
        SetError(std::string("sabRemoveWidget: ") + e.what());
    } catch (...) {
        SetError("sabRemoveWidget: unknown error");
    }
    return 0;
}

int sabMoveWidgetToFront(sabWindow window, sabWidgetId id) {
    auto* win = reinterpret_cast<sableui::Window*>(window);
    if (!win) return 0;
    try {
        return win->MoveWidgetToFront(id) ? 1 : 0;
    } catch (const std::exception& e) {
        SetError(std::string("sabMoveWidgetToFront: ") + e.what());
    } catch (...) {
        SetError("sabMoveWidgetToFront: unknown error");
    }
    return 0;
}

// --- Scroll areas -----------------------------------------------------------

sabWidgetId sabAddScrollArea(sabWindow window, int x, int y, int w, int h) {
    auto* win = reinterpret_cast<sableui::Window*>(window);
    if (!win) {
        SetError("sabAddScrollArea: invalid argument");
        return -1;
    }
    try {
        return win->AddScrollArea(x, y, w, h);
    } catch (const std::exception& e) {
        SetError(std::string("sabAddScrollArea: ") + e.what());
    } catch (...) {
        SetError("sabAddScrollArea: unknown error");
    }
    return -1;
}

sabWidgetId sabAddButtonIn(sabWindow window, sabWidgetId area,
                           int x, int y, int w, int h,
                           const wchar_t* text,
                           sabClickCallback onClick, void* userdata) {
    auto* win = reinterpret_cast<sableui::Window*>(window);
    if (!win || !text) {
        SetError("sabAddButtonIn: invalid argument");
        return -1;
    }
    try {
        int id = win->AddButtonIn(area, x, y, w, h, text,
                                  [onClick, userdata]() {
                                      if (onClick) onClick(userdata);
                                  });
        if (id < 0) SetError("sabAddButtonIn: failed");
        return id;
    } catch (const std::exception& e) {
        SetError(std::string("sabAddButtonIn: ") + e.what());
    } catch (...) {
        SetError("sabAddButtonIn: unknown error");
    }
    return -1;
}

sabWidgetId sabAddLabelIn(sabWindow window, sabWidgetId area,
                          int x, int y, int w, int h, const wchar_t* text) {
    auto* win = reinterpret_cast<sableui::Window*>(window);
    if (!win || !text) {
        SetError("sabAddLabelIn: invalid argument");
        return -1;
    }
    try {
        int id = win->AddLabelIn(area, x, y, w, h, text);
        if (id < 0) SetError("sabAddLabelIn: failed");
        return id;
    } catch (const std::exception& e) {
        SetError(std::string("sabAddLabelIn: ") + e.what());
    } catch (...) {
        SetError("sabAddLabelIn: unknown error");
    }
    return -1;
}

sabWidgetId sabAddCheckboxIn(sabWindow window, sabWidgetId area,
                             int x, int y, int w, int h,
                             const wchar_t* text, int checked,
                             sabChangeCallback onChange, void* userdata) {
    auto* win = reinterpret_cast<sableui::Window*>(window);
    if (!win || !text) {
        SetError("sabAddCheckboxIn: invalid argument");
        return -1;
    }
    try {
        int id = win->AddCheckboxIn(area, x, y, w, h, text, checked != 0,
                                    [onChange, userdata](bool v) {
                                        if (onChange) onChange(userdata, v ? 1 : 0);
                                    });
        if (id < 0) SetError("sabAddCheckboxIn: failed");
        return id;
    } catch (const std::exception& e) {
        SetError(std::string("sabAddCheckboxIn: ") + e.what());
    } catch (...) {
        SetError("sabAddCheckboxIn: unknown error");
    }
    return -1;
}

sabWidgetId sabAddTextFieldIn(sabWindow window, sabWidgetId area,
                              int x, int y, int w, int h,
                              const wchar_t* initial, int masked,
                              sabTextChangeCallback onChange, void* userdata) {
    auto* win = reinterpret_cast<sableui::Window*>(window);
    if (!win) {
        SetError("sabAddTextFieldIn: invalid argument");
        return -1;
    }
    try {
        std::wstring initial_text = initial ? initial : L"";
        int id = win->AddTextFieldIn(area, x, y, w, h, initial_text, masked != 0,
                                     [onChange, userdata](const std::wstring& t) {
                                         if (onChange) onChange(userdata, t.c_str());
                                     });
        if (id < 0) SetError("sabAddTextFieldIn: failed");
        return id;
    } catch (const std::exception& e) {
        SetError(std::string("sabAddTextFieldIn: ") + e.what());
    } catch (...) {
        SetError("sabAddTextFieldIn: unknown error");
    }
    return -1;
}

int sabSetScrollOffset(sabWindow window, sabWidgetId area, int offset) {
    auto* win = reinterpret_cast<sableui::Window*>(window);
    if (!win) return 0;
    try {
        return win->SetScrollOffset(area, offset) ? 1 : 0;
    } catch (const std::exception& e) {
        SetError(std::string("sabSetScrollOffset: ") + e.what());
    } catch (...) {
        SetError("sabSetScrollOffset: unknown error");
    }
    return 0;
}

int sabGetScrollOffset(sabWindow window, sabWidgetId area) {
    auto* win = reinterpret_cast<sableui::Window*>(window);
    if (!win) return -1;
    try {
        return win->GetScrollOffset(area);
    } catch (const std::exception& e) {
        SetError(std::string("sabGetScrollOffset: ") + e.what());
    } catch (...) {
        SetError("sabGetScrollOffset: unknown error");
    }
    return -1;
}

// --- Layout anchors ---------------------------------------------------------

int sabSetWidgetAnchors(sabWindow window, sabWidgetId id, unsigned anchors) {
    auto* win = reinterpret_cast<sableui::Window*>(window);
    if (!win) {
        SetError("sabSetWidgetAnchors: invalid argument");
        return 0;
    }
    try {
        return win->SetWidgetAnchors(id, anchors) ? 1 : 0;
    } catch (const std::exception& e) {
        SetError(std::string("sabSetWidgetAnchors: ") + e.what());
    } catch (...) {
        SetError("sabSetWidgetAnchors: unknown error");
    }
    return 0;
}

// --- Drawing (paint callback only) ------------------------------------------

void sabSetFillColor(sabWindow window, float r, float g, float b, float a) {
    auto* win = reinterpret_cast<sableui::Window*>(window);
    if (win) win->SetFillColor({r, g, b, a});
}

void sabSetStrokeColor(sabWindow window, float r, float g, float b, float a) {
    auto* win = reinterpret_cast<sableui::Window*>(window);
    if (win) win->SetStrokeColor({r, g, b, a});
}

void sabSetStrokeWidth(sabWindow window, float width) {
    auto* win = reinterpret_cast<sableui::Window*>(window);
    if (win) win->SetStrokeWidth(width);
}

void sabClear(sabWindow window) {
    auto* win = reinterpret_cast<sableui::Window*>(window);
    if (win) win->ClearWithFill();
}

void sabFillRect(sabWindow window, float x, float y, float w, float h) {
    auto* win = reinterpret_cast<sableui::Window*>(window);
    if (win) win->FillRect(x, y, w, h);
}

void sabDrawRect(sabWindow window, float x, float y, float w, float h) {
    auto* win = reinterpret_cast<sableui::Window*>(window);
    if (win) win->DrawRect(x, y, w, h);
}

void sabFillRoundedRect(sabWindow window, float x, float y, float w, float h,
                        float radius) {
    auto* win = reinterpret_cast<sableui::Window*>(window);
    if (win) win->FillRoundedRect(x, y, w, h, radius);
}

void sabFillEllipse(sabWindow window, float cx, float cy, float rx, float ry) {
    auto* win = reinterpret_cast<sableui::Window*>(window);
    if (win) win->FillEllipse(cx, cy, rx, ry);
}

void sabDrawEllipse(sabWindow window, float cx, float cy, float rx, float ry) {
    auto* win = reinterpret_cast<sableui::Window*>(window);
    if (win) win->DrawEllipse(cx, cy, rx, ry);
}

void sabDrawLine(sabWindow window, float x1, float y1, float x2, float y2) {
    auto* win = reinterpret_cast<sableui::Window*>(window);
    if (win) win->DrawLine(x1, y1, x2, y2);
}

void sabDrawText(sabWindow window, float x, float y, float w, float h,
                 const wchar_t* text, float font_size) {
    auto* win = reinterpret_cast<sableui::Window*>(window);
    if (win && text) win->DrawTextAt(x, y, w, h, text, font_size);
}

} // extern "C"