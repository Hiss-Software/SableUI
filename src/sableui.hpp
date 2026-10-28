#pragma once

// Internal implementation core of Sableui. Not part of the public contract:
// consumers must only include <sableui/sableui.h>.
//
// All public coordinates and sizes are logical (DIP) units: 1 DIP = 1 px at
// 96 dpi. The renderer multiplies everything by the current monitor DPI, so
// the library is per-monitor DPI aware end to end.
//
// Link with: -ld2d1 -ldwrite -ldwmapi -lole32

#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX

#include <windows.h>
#include <windowsx.h>
#include <objbase.h>
#include <d2d1.h>
#include <dwrite.h>
#include <dwmapi.h>
#include <wrl/client.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <functional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#ifndef DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2
#define DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2 ((DPI_AWARENESS_CONTEXT)-4)
#endif

namespace sableui {

struct Color {
    float r, g, b, a;
};

enum class TextAlign { Left, Center };

// Central place for all theme colors. A future theming layer touches only
// this struct.
struct Palette {
    Color background   { 0.118f, 0.122f, 0.141f, 1.0f };
    Color accent       { 0.310f, 0.549f, 1.0f,  1.0f };
    Color accentHover  { 0.412f, 0.627f, 1.0f,  1.0f };
    Color accentPressed{ 0.220f, 0.420f, 0.820f, 1.0f };
    Color buttonText   { 1.0f,   1.0f,   1.0f,  1.0f };
    Color labelText    { 0.851f, 0.859f, 0.878f, 1.0f };
    Color border       { 0.450f, 0.470f, 0.520f, 1.0f };
};

enum class WidgetKind { Button, Label, Checkbox };

struct Widget {
    WidgetKind kind;
    int x, y, w, h;               // DIPs
    std::wstring text;
    bool checked = false;
    bool pressed = false;
    bool enabled  = true;
    bool visible  = true;
    std::function<void()> onClick;
    std::function<void(bool)> onChange;
};

// Abstract rendering surface. A future backend (Direct3D, software) can be
// added without touching widgets or the window class. All coordinates are
// DIPs; the backend is responsible for DPI scaling.
class Renderer {
public:
    virtual ~Renderer() = default;
    virtual bool Begin(HWND hwnd) = 0;
    virtual void End() = 0;
    virtual void SetScale(float scale) = 0;
    virtual void Clear(const Color& color) = 0;
    virtual void PushClipRect(float x, float y, float w, float h) = 0;
    virtual void PopClipRect() = 0;
    virtual void FillRoundedRect(float x, float y, float w, float h,
                                 float radius, const Color& color) = 0;
    virtual void DrawRoundedRect(float x, float y, float w, float h,
                                 float radius, const Color& color,
                                 float width) = 0;
    virtual void FillRect(float x, float y, float w, float h,
                          const Color& color) = 0;
    virtual void DrawRect(float x, float y, float w, float h,
                          const Color& color, float width) = 0;
    virtual void FillEllipse(float cx, float cy, float rx, float ry,
                             const Color& color) = 0;
    virtual void DrawEllipse(float cx, float cy, float rx, float ry,
                             const Color& color, float width) = 0;
    virtual void DrawLine(float x1, float y1, float x2, float y2,
                          const Color& color, float width) = 0;
    virtual void DrawTextIn(float x, float y, float w, float h,
                            const std::wstring& text, const Color& color,
                            TextAlign align, float font_size) = 0;
    virtual void OnResize(unsigned width, unsigned height) = 0;  // physical px
};

// Direct2D backend. DPI scaling is applied through the world transform, so
// stroke widths and font sizes scale together with geometry.
//
// Note: WRL ComPtr has no implicit conversion to the raw interface pointer,
// so every D2D call passes brush_.Get() explicitly.
class D2DRenderer final : public Renderer {
public:
    D2DRenderer() = default;
    ~D2DRenderer() override = default;

    D2DRenderer(const D2DRenderer&) = delete;
    D2DRenderer& operator=(const D2DRenderer&) = delete;

    bool Begin(HWND hwnd) override {
        if (!EnsureTarget(hwnd)) return false;
        target_->SetTransform(
            D2D1::Matrix3x2F::Scale(D2D1::SizeF(scale_, scale_)));
        target_->BeginDraw();
        return true;
    }

    void End() override {
        if (!target_) return;
        if (target_->EndDraw() == D2DERR_RECREATE_TARGET) {
            DiscardTarget();  // device lost: recreated on the next Begin()
        }
    }

    void SetScale(float scale) override {
        scale_ = scale > 0.01f ? scale : 0.01f;
    }

    void Clear(const Color& color) override {
        if (!target_) return;
        target_->Clear(ToD2D(color));
    }

    // PushAxisAlignedClip ignores the world transform, so the rect must be
    // scaled here explicitly.
    void PushClipRect(float x, float y, float w, float h) override {
        if (!target_) return;
        const D2D1_RECT_F r = D2D1::RectF(x * scale_, y * scale_,
                                          (x + w) * scale_, (y + h) * scale_);
        target_->PushAxisAlignedClip(r, D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
    }

    void PopClipRect() override {
        if (!target_) return;
        target_->PopAxisAlignedClip();
    }

    void FillRoundedRect(float x, float y, float w, float h, float radius,
                         const Color& color) override {
        if (!target_ || !brush_) return;
        brush_->SetColor(ToD2D(color));
        target_->FillRoundedRectangle(
            D2D1::RoundedRect(ToRectF(x, y, w, h), radius, radius),
            brush_.Get());
    }

    void DrawRoundedRect(float x, float y, float w, float h, float radius,
                         const Color& color, float width) override {
        if (!target_ || !brush_) return;
        brush_->SetColor(ToD2D(color));
        target_->DrawRoundedRectangle(
            D2D1::RoundedRect(ToRectF(x, y, w, h), radius, radius),
            brush_.Get(), width);
    }

    void FillRect(float x, float y, float w, float h,
                  const Color& color) override {
        if (!target_ || !brush_) return;
        brush_->SetColor(ToD2D(color));
        target_->FillRectangle(ToRectF(x, y, w, h), brush_.Get());
    }

    void DrawRect(float x, float y, float w, float h, const Color& color,
                  float width) override {
        if (!target_ || !brush_) return;
        brush_->SetColor(ToD2D(color));
        target_->DrawRectangle(ToRectF(x, y, w, h), brush_.Get(), width);
    }

    void FillEllipse(float cx, float cy, float rx, float ry,
                     const Color& color) override {
        if (!target_ || !brush_) return;
        brush_->SetColor(ToD2D(color));
        target_->FillEllipse(
            D2D1::Ellipse(D2D1::Point2F(cx, cy), rx, ry), brush_.Get());
    }

    void DrawEllipse(float cx, float cy, float rx, float ry,
                     const Color& color, float width) override {
        if (!target_ || !brush_) return;
        brush_->SetColor(ToD2D(color));
        target_->DrawEllipse(
            D2D1::Ellipse(D2D1::Point2F(cx, cy), rx, ry), brush_.Get(), width);
    }

    void DrawLine(float x1, float y1, float x2, float y2,
                  const Color& color, float width) override {
        if (!target_ || !brush_) return;
        brush_->SetColor(ToD2D(color));
        target_->DrawLine(D2D1::Point2F(x1, y1), D2D1::Point2F(x2, y2),
                          brush_.Get(), width);
    }

    void DrawTextIn(float x, float y, float w, float h,
                    const std::wstring& text, const Color& color,
                    TextAlign align, float font_size) override {
        if (!target_ || !brush_ || text.empty()) return;
        IDWriteTextFormat* format = GetFormat(font_size);
        if (!format) return;
        format->SetTextAlignment(align == TextAlign::Center
                                     ? DWRITE_TEXT_ALIGNMENT_CENTER
                                     : DWRITE_TEXT_ALIGNMENT_LEADING);
        format->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
        brush_->SetColor(ToD2D(color));
        target_->DrawTextW(text.c_str(), static_cast<UINT32>(text.size()),
                           format, ToRectF(x, y, w, h), brush_.Get());
    }

    void OnResize(unsigned width, unsigned height) override {
        if (target_) {
            target_->Resize(
                D2D1::SizeU(std::max(1u, width), std::max(1u, height)));
        }
    }

private:
    static D2D1_COLOR_F ToD2D(const Color& c) {
        return D2D1_COLOR_F{c.r, c.g, c.b, c.a};
    }
    static D2D1_RECT_F ToRectF(float x, float y, float w, float h) {
        return D2D1::RectF(x, y, x + w, y + h);
    }

    // Cache key: raw bits of the float size. Avoids collisions of the old
    // quantization scheme (14.2f and 14.3f used to share a slot).
    IDWriteTextFormat* GetFormat(float px) {
        uint32_t key = 0;
        static_assert(sizeof(px) == sizeof(key), "unexpected float size");
        std::memcpy(&key, &px, sizeof(key));
        auto it = formats_.find(key);
        if (it != formats_.end()) return it->second.Get();
        IDWriteTextFormat* f = nullptr;
        if (FAILED(write_->CreateTextFormat(
                L"Segoe UI", nullptr, DWRITE_FONT_WEIGHT_NORMAL,
                DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL,
                px, L"en-US", &f))) {
            return nullptr;
        }
        f->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
        formats_[key].Attach(f);
        return f;
    }

    bool EnsureTarget(HWND hwnd) {
        if (target_) return true;

        RECT rc;
        if (!GetClientRect(hwnd, &rc)) return false;
        const unsigned w = static_cast<unsigned>(std::max(1L, rc.right - rc.left));
        const unsigned h = static_cast<unsigned>(std::max(1L, rc.bottom - rc.top));

        if (!factory_) {
            if (FAILED(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED,
                                         __uuidof(ID2D1Factory),
                                         reinterpret_cast<void**>(
                                             factory_.ReleaseAndGetAddressOf())))) {
                return false;
            }
        }
        if (!write_) {
            if (FAILED(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED,
                                           __uuidof(IDWriteFactory),
                                           reinterpret_cast<IUnknown**>(
                                               write_.ReleaseAndGetAddressOf())))) {
                return false;
            }
        }

        if (FAILED(factory_->CreateHwndRenderTarget(
                D2D1::RenderTargetProperties(),
                D2D1::HwndRenderTargetProperties(hwnd, D2D1::SizeU(w, h)),
                &target_))) {
            return false;
        }

        if (FAILED(target_->CreateSolidColorBrush(D2D1_COLOR_F{0, 0, 0, 1},
                                                  &brush_))) {
            target_ = nullptr;
            return false;
        }
        return true;
    }

    void DiscardTarget() {
        formats_.clear();
        write_   = nullptr;
        brush_   = nullptr;
        target_  = nullptr;
        factory_ = nullptr;
    }

    Microsoft::WRL::ComPtr<ID2D1Factory> factory_;
    Microsoft::WRL::ComPtr<ID2D1HwndRenderTarget> target_;
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> brush_;
    Microsoft::WRL::ComPtr<IDWriteFactory> write_;
    std::unordered_map<uint32_t, Microsoft::WRL::ComPtr<IDWriteTextFormat>>
        formats_;
    float scale_ = 1.0f;
};

// Win32 window with its own widget tree and message loop.
class Window {
public:
    Window() = default;
    ~Window() {
        if (hwnd_) DestroyWindow(hwnd_);
        if (com_initialized_) CoUninitialize();
    }

    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;

    bool Create(const std::wstring& title, int width, int height,
                std::string* error) {
        auto fail = [error](const char* what) {
            if (error) *error = what;
            return false;
        };

        EnablePerMonitorDpi();

        const HRESULT com_hr = CoInitializeEx(
            nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
        com_initialized_ = SUCCEEDED(com_hr);
        if (FAILED(com_hr)) return fail("CoInitializeEx failed");

        if (!RegisterClassOnce()) return fail("RegisterClassExW failed");

        hwnd_ = CreateWindowExW(
            0, kClassName, title.c_str(), WS_OVERLAPPEDWINDOW,
            CW_USEDEFAULT, CW_USEDEFAULT, width, height,
            nullptr, nullptr, GetModuleHandleW(nullptr), this);
        if (!hwnd_) return fail("CreateWindowExW failed");

        UpdateDpi();
        ApplyDarkMode();
        ShowWindow(hwnd_, SW_SHOW);
        if (error) error->clear();
        return true;
    }

    // Coordinates in DIPs.
    int AddButton(int x, int y, int w, int h, const std::wstring& text,
                  std::function<void()> onClick) {
        widgets_.push_back(Widget{WidgetKind::Button, x, y, w, h, text,
                                  false, false, true, true,
                                  std::move(onClick), nullptr});
        Invalidate();
        return static_cast<int>(widgets_.size()) - 1;
    }

    int AddLabel(int x, int y, int w, int h, const std::wstring& text) {
        widgets_.push_back(Widget{WidgetKind::Label, x, y, w, h, text,
                                  false, false, true, true, nullptr, nullptr});
        Invalidate();
        return static_cast<int>(widgets_.size()) - 1;
    }

    int AddCheckbox(int x, int y, int w, int h, const std::wstring& text,
                    bool checked, std::function<void(bool)> onChange) {
        widgets_.push_back(Widget{WidgetKind::Checkbox, x, y, w, h, text,
                                  checked, false, true, true,
                                  nullptr, std::move(onChange)});
        Invalidate();
        return static_cast<int>(widgets_.size()) - 1;
    }

    // Legacy overloads kept for the C API layer: the error parameter is
    // accepted for source compatibility and always cleared (these calls
    // cannot fail).
    int AddButton(int x, int y, int w, int h, const std::wstring& text,
                  std::function<void()> onClick, std::string* error) {
        if (error) error->clear();
        return AddButton(x, y, w, h, text, std::move(onClick));
    }

    int AddLabel(int x, int y, int w, int h, const std::wstring& text,
                 std::string* error) {
        if (error) error->clear();
        return AddLabel(x, y, w, h, text);
    }

    int AddCheckbox(int x, int y, int w, int h, const std::wstring& text,
                    bool checked, std::function<void(bool)> onChange,
                    std::string* error) {
        if (error) error->clear();
        return AddCheckbox(x, y, w, h, text, checked, std::move(onChange));
    }

    bool SetLabelText(int id, const std::wstring& text) {
        return SetWidgetText(id, text);
    }

    bool SetWidgetText(int id, const std::wstring& text) {
        if (!Valid(id)) return false;
        widgets_[id].text = text;
        Invalidate();
        return true;
    }

    bool SetWidgetChecked(int id, bool on) {
        if (!Valid(id)) return false;
        Widget& w = widgets_[id];
        if (w.kind != WidgetKind::Checkbox) return false;
        if (w.checked == on) return true;
        w.checked = on;
        SafeCall(w.onChange, on);
        Invalidate();
        return true;
    }

    bool IsChecked(int id) const {
        if (!Valid(id)) return false;
        return widgets_[id].kind == WidgetKind::Checkbox && widgets_[id].checked;
    }

    bool SetWidgetEnabled(int id, bool on) {
        if (!Valid(id)) return false;
        if (widgets_[id].enabled == on) return true;
        widgets_[id].enabled = on;
        if (!on && focused_ == id) focused_ = -1;
        if (!on && pressed_ == id) pressed_ = -1;
        Invalidate();
        return true;
    }

    bool SetWidgetVisible(int id, bool on) {
        if (!Valid(id)) return false;
        if (widgets_[id].visible == on) return true;
        widgets_[id].visible = on;
        if (!on && focused_ == id) focused_ = -1;
        if (!on && pressed_ == id) pressed_ = -1;
        if (!on && hover_ == id) hover_ = -1;
        Invalidate();
        return true;
    }

    bool RemoveWidget(int id) {
        if (!Valid(id)) return false;
        if (pressed_ == id) {
            if (hwnd_) ReleaseCapture();
            pressed_ = -1;
        } else if (pressed_ > id) {
            --pressed_;
        }
        if (hover_ == id) hover_ = -1;
        else if (hover_ > id) --hover_;
        if (focused_ == id) focused_ = -1;
        else if (focused_ > id) --focused_;
        widgets_.erase(widgets_.begin() + id);
        Invalidate();
        return true;
    }

    // Raises a widget above all others; it wins hit tests over overlaps.
    bool MoveWidgetToFront(int id) {
        if (!Valid(id)) return false;
        Widget w = widgets_[id];
        widgets_.erase(widgets_.begin() + id);
        widgets_.push_back(std::move(w));
        const int last = static_cast<int>(widgets_.size()) - 1;
        auto fix = [id, last](int& idx) {
            if (idx == id) idx = last;
            else if (idx > id) --idx;
        };
        fix(hover_);
        fix(pressed_);
        fix(focused_);
        Invalidate();
        return true;
    }

    bool IsEnabled(int id) const { return Valid(id) && widgets_[id].enabled; }
    bool IsVisible(int id) const { return Valid(id) && widgets_[id].visible; }

    // --- Window state -------------------------------------------------------

    void SetTitle(const std::wstring& title) {
        if (hwnd_) SetWindowTextW(hwnd_, title.c_str());
    }

    void SetResizable(bool on) {
        if (resizable_ == on) return;
        resizable_ = on;
        ApplyStyles();
    }

    void SetBorderless(bool on) {
        if (borderless_ == on) return;
        borderless_ = on;
        ApplyStyles();
    }

    // In DIPs.
    void SetMinimumSize(int w, int h) {
        min_w_ = w;
        min_h_ = h;
    }

    void CloseRequest() {
        if (hwnd_) PostMessageW(hwnd_, WM_CLOSE, 0, 0);
    }

    void ClientSize(int* w, int* h) const {  // DIPs
        RECT rc{};
        if (hwnd_ && GetClientRect(hwnd_, &rc)) {
            if (w) *w = PxToDip(rc.right - rc.left);
            if (h) *h = PxToDip(rc.bottom - rc.top);
        } else {
            if (w) *w = 0;
            if (h) *h = 0;
        }
    }

    void Invalidate() {
        if (hwnd_) InvalidateRect(hwnd_, nullptr, FALSE);
    }

    // --- Event hooks ----------------------------------------------------------
    // Mouse and resize coordinates arrive in DIPs.

    void OnMouseDown(std::function<void(int, int, int)> f) { on_mouse_down_ = std::move(f); }
    void OnMouseUp(std::function<void(int, int, int)> f)   { on_mouse_up_ = std::move(f); }
    void OnMouseDoubleClick(std::function<void(int, int, int)> f) { on_double_click_ = std::move(f); }
    void OnMouseMove(std::function<void(int, int)> f)      { on_mouse_move_ = std::move(f); }
    void OnMouseWheel(std::function<void(int, int, int)> f){ on_mouse_wheel_ = std::move(f); }
    void OnKeyDown(std::function<void(int, int)> f)        { on_key_down_ = std::move(f); }
    void OnKeyUp(std::function<void(int, int)> f)          { on_key_up_ = std::move(f); }
    void OnText(std::function<void(unsigned int)> f)       { on_text_ = std::move(f); }
    void OnResize(std::function<void(int, int)> f)         { on_resize_ = std::move(f); }
    void OnClose(std::function<void()> f)                  { on_close_ = std::move(f); }
    void OnPaint(std::function<void()> f)                  { on_paint_ = std::move(f); }

    // --- Timers ---------------------------------------------------------------

    int AddTimer(int interval_ms, std::function<void()> fn, std::string* error) {
        if (error) error->clear();
        if (!hwnd_) {
            if (error) *error = "AddTimer: window is not created yet";
            return -1;
        }
        if (interval_ms <= 0 || !fn) {
            if (error) *error = "AddTimer: invalid arguments";
            return -1;
        }
        const int id = next_timer_id_++;
        if (!SetTimer(hwnd_, static_cast<UINT_PTR>(id),
                      static_cast<UINT>(interval_ms), nullptr)) {
            if (error) *error = "SetTimer failed";
            return -1;
        }
        timers_[id] = std::move(fn);
        return id;
    }

    void RemoveTimer(int id) {
        if (hwnd_) KillTimer(hwnd_, static_cast<UINT_PTR>(id));
        timers_.erase(id);
    }

    // --- Paint-time drawing (valid only inside the on_paint callback) ---------
    // All coordinates are DIPs.

    void SetFillColor(const Color& c)   { fill_ = c; }
    void SetStrokeColor(const Color& c) { stroke_ = c; }
    void SetTextColor(const Color& c)   { text_ = c; }
    void SetStrokeWidth(float w)        { stroke_width_ = w > 0 ? w : 1.0f; }

    void PushClipRect(float x, float y, float w, float h) {
        if (painting_) renderer_.PushClipRect(x, y, w, h);
    }
    void PopClipRect() {
        if (painting_) renderer_.PopClipRect();
    }
    void ClearWithFill()  { if (painting_) renderer_.Clear(fill_); }
    void FillRect(float x, float y, float w, float h) {
        if (painting_) renderer_.FillRect(x, y, w, h, fill_);
    }
    void DrawRect(float x, float y, float w, float h) {
        if (painting_) renderer_.DrawRect(x, y, w, h, stroke_, stroke_width_);
    }
    void FillRoundedRect(float x, float y, float w, float h, float r) {
        if (painting_) renderer_.FillRoundedRect(x, y, w, h, r, fill_);
    }
    void FillEllipse(float cx, float cy, float rx, float ry) {
        if (painting_) renderer_.FillEllipse(cx, cy, rx, ry, fill_);
    }
    void DrawEllipse(float cx, float cy, float rx, float ry) {
        if (painting_) renderer_.DrawEllipse(cx, cy, rx, ry, stroke_, stroke_width_);
    }
    void DrawLine(float x1, float y1, float x2, float y2) {
        if (painting_) renderer_.DrawLine(x1, y1, x2, y2, stroke_, stroke_width_);
    }
    void DrawTextAt(float x, float y, float w, float h,
                    const std::wstring& text, float font_size) {
        if (painting_) renderer_.DrawTextIn(x, y, w, h, text, text_,
                                            TextAlign::Left, font_size);
    }

    void Run() {
        running_ = true;
        MSG msg{};
        while (running_ && GetMessageW(&msg, nullptr, 0, 0) > 0) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
        running_ = false;
    }

    void Destroy() {
        if (hwnd_) DestroyWindow(hwnd_);
    }

    float DpiScale() const { return Scale(); }

private:
    template <class F, class... A>
    static void SafeCall(const F& f, A... args) {
        if (!f) return;
        try {
            f(args...);
        } catch (...) {
            // Callbacks must never unwind through the message loop.
        }
    }

    // GetProcAddress returns FARPROC; routing the cast through void* keeps
    // -Wcast-function-type quiet.
    template <typename Fn>
    static Fn LoadProcAddress(const wchar_t* module, const char* name) {
        if (HMODULE h = GetModuleHandleW(module)) {
            return reinterpret_cast<Fn>(
                reinterpret_cast<void*>(GetProcAddress(h, name)));
        }
        return nullptr;
    }

    float Scale() const { return dpi_ / 96.0f; }
    int PxToDip(int px) const {
        return static_cast<int>(std::lround(static_cast<float>(px) / Scale()));
    }
    int DipToPx(int dip) const {
        return static_cast<int>(std::lround(static_cast<float>(dip) * Scale()));
    }

    void UpdateDpi() {
        if (!hwnd_) return;
        const UINT dpi = GetDpiForWindowSafe();
        dpi_ = dpi > 0 ? static_cast<float>(dpi) : 96.0f;
        renderer_.SetScale(Scale());
    }

    UINT GetDpiForWindowSafe() const {
        using Fn = UINT(WINAPI*)(HWND);
        if (auto fn = LoadProcAddress<Fn>(L"user32.dll", "GetDpiForWindow")) {
            return fn(hwnd_);
        }
        HDC dc = GetDC(hwnd_);
        const UINT dpi = static_cast<UINT>(GetDeviceCaps(dc, LOGPIXELSX));
        ReleaseDC(hwnd_, dc);
        return dpi;
    }

    static void EnablePerMonitorDpi() {
        using Fn = DPI_AWARENESS_CONTEXT(WINAPI*)(DPI_AWARENESS_CONTEXT);
        if (auto fn = LoadProcAddress<Fn>(
                L"user32.dll", "SetProcessDpiAwarenessContext")) {
            fn(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
        }
    }

    void ApplyDarkMode() {
        if (!hwnd_) return;
        const BOOL dark = TRUE;
        // Attribute 20 exists on Win10 1903+; 19 on earlier builds.
        if (FAILED(DwmSetWindowAttribute(hwnd_, 20, &dark, sizeof(dark)))) {
            DwmSetWindowAttribute(hwnd_, 19, &dark, sizeof(dark));
        }
    }

    void ApplyStyles() {
        if (!hwnd_) return;
        LONG_PTR style = GetWindowLongPtrW(hwnd_, GWL_STYLE);
        style &= ~(WS_CAPTION | WS_THICKFRAME | WS_MAXIMIZEBOX | WS_MINIMIZEBOX);
        if (!borderless_) style |= WS_CAPTION;
        if (resizable_) style |= WS_THICKFRAME | WS_MAXIMIZEBOX | WS_MINIMIZEBOX;
        SetWindowLongPtrW(hwnd_, GWL_STYLE, style);

        // DWM shadow for borderless windows (needs WS_THICKFRAME kept above).
        const MARGINS margins =
            borderless_ ? MARGINS{-1, -1, -1, -1} : MARGINS{0, 0, 0, 0};
        DwmExtendFrameIntoClientArea(hwnd_, &margins);

        SetWindowPos(hwnd_, nullptr, 0, 0, 0, 0,
                     SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
    }

    static LRESULT CALLBACK WndProcThunk(HWND hwnd, UINT msg, WPARAM wp,
                                         LPARAM lp) {
        if (msg == WM_NCCREATE) {
            SetWindowLongPtrW(hwnd, GWLP_USERDATA,
                              reinterpret_cast<LONG_PTR>(
                                  reinterpret_cast<CREATESTRUCTW*>(lp)->lpCreateParams));
        }
        if (auto* self = reinterpret_cast<Window*>(
                GetWindowLongPtrW(hwnd, GWLP_USERDATA))) {
            return self->HandleMessage(hwnd, msg, wp, lp);
        }
        return DefWindowProcW(hwnd, msg, wp, lp);
    }

    static int ModsNow() {
        int mods = 0;
        if (GetKeyState(VK_CONTROL) & 0x8000) mods |= kModCtrl;
        if (GetKeyState(VK_SHIFT)   & 0x8000) mods |= kModShift;
        if (GetKeyState(VK_MENU)    & 0x8000) mods |= kModAlt;
        return mods;
    }

    LRESULT HandleMessage(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
        switch (msg) {
        case WM_PAINT:
            ValidateRect(hwnd, nullptr);
            Render();
            return 0;

        case WM_ERASEBKGND:
            return 1;

        case WM_LBUTTONDOWN: {
            const int x = PxToDip(GET_X_LPARAM(lp));
            const int y = PxToDip(GET_Y_LPARAM(lp));
            SetCapture(hwnd);
            const int hit = HitTest(x, y);
            if (hit >= 0) {
                pressed_ = hit;
                widgets_[hit].pressed = true;
                if (IsFocusable(hit)) focused_ = hit;
            } else {
                focused_ = -1;
            }
            SafeCall(on_mouse_down_, 0, x, y);
            UpdateHover(x, y);
            Invalidate();
            return 0;
        }

        case WM_LBUTTONDBLCLK:
            SafeCall(on_double_click_, 0, PxToDip(GET_X_LPARAM(lp)),
                     PxToDip(GET_Y_LPARAM(lp)));
            [[fallthrough]];

        case WM_RBUTTONDOWN:
        case WM_MBUTTONDOWN:
            if (msg != WM_LBUTTONDBLCLK) {
                SafeCall(on_mouse_down_,
                         msg == WM_RBUTTONDOWN ? 1 : 2,
                         PxToDip(GET_X_LPARAM(lp)),
                         PxToDip(GET_Y_LPARAM(lp)));
            }
            return 0;

        // Click is resolved BEFORE ReleaseCapture: ReleaseCapture sends a
        // synchronous WM_CAPTURECHANGED, and processing it first used to
        // reset pressed_ before the click check could run.
        case WM_LBUTTONUP: {
            const int x = PxToDip(GET_X_LPARAM(lp));
            const int y = PxToDip(GET_Y_LPARAM(lp));
            const int hit = HitTest(x, y);
            if (pressed_ >= 0) {
                Widget& w = widgets_[pressed_];
                const bool released_on_same = (hit == pressed_);
                w.pressed = false;
                pressed_ = -1;
                if (released_on_same) {
                    if (w.kind == WidgetKind::Button) SafeCall(w.onClick);
                    if (w.kind == WidgetKind::Checkbox) {
                        w.checked = !w.checked;
                        SafeCall(w.onChange, w.checked);
                    }
                }
            }
            ReleaseCapture();
            SafeCall(on_mouse_up_, 0, x, y);
            Invalidate();
            return 0;
        }

        case WM_RBUTTONUP:
        case WM_MBUTTONUP:
            SafeCall(on_mouse_up_,
                     msg == WM_RBUTTONUP ? 1 : 2,
                     PxToDip(GET_X_LPARAM(lp)),
                     PxToDip(GET_Y_LPARAM(lp)));
            return 0;

        case WM_CAPTURECHANGED:
            if (pressed_ >= 0) widgets_[pressed_].pressed = false;
            pressed_ = -1;
            return 0;

        case WM_MOUSEMOVE: {
            if (!tracking_) {
                TRACKMOUSEEVENT tme{sizeof(tme), TME_LEAVE, hwnd, 0};
                TrackMouseEvent(&tme);
                tracking_ = true;
            }
            const int x = PxToDip(GET_X_LPARAM(lp));
            const int y = PxToDip(GET_Y_LPARAM(lp));
            UpdateHover(x, y);
            SafeCall(on_mouse_move_, x, y);
            return 0;
        }

        case WM_MOUSELEAVE:
            tracking_ = false;
            UpdateHover(-1, -1);
            return 0;

        case WM_MOUSEWHEEL:
        case WM_MOUSEHWHEEL: {
            POINT pt{GET_X_LPARAM(lp), GET_Y_LPARAM(lp)};
            ScreenToClient(hwnd, &pt);
            SafeCall(on_mouse_wheel_, GET_WHEEL_DELTA_WPARAM(wp),
                     PxToDip(pt.x), PxToDip(pt.y));
            return 0;
        }

        case WM_KEYDOWN:
            if (wp == VK_TAB && CycleFocus(
                    (GetKeyState(VK_SHIFT) & 0x8000) ? -1 : 1)) {
                Invalidate();
                return 0;  // Tab is consumed by focus navigation
            }
            if ((wp == VK_SPACE || wp == VK_RETURN) && focused_ >= 0) {
                Activate(focused_);
                return 0;
            }
            SafeCall(on_key_down_, static_cast<int>(wp), ModsNow());
            return 0;

        case WM_SYSKEYDOWN:
            SafeCall(on_key_down_, static_cast<int>(wp), ModsNow());
            return DefWindowProcW(hwnd, msg, wp, lp);  // keep Alt behavior intact

        case WM_KEYUP:
        case WM_SYSKEYUP:
            SafeCall(on_key_up_, static_cast<int>(wp), ModsNow());
            return msg == WM_KEYUP ? 0 : DefWindowProcW(hwnd, msg, wp, lp);

        case WM_CHAR:
            SafeCall(on_text_, static_cast<unsigned int>(wp));
            return 0;

        case WM_UNICHAR:
            if (wp == UNICODE_NOCHAR) return 1;  // capability query
            SafeCall(on_text_, static_cast<unsigned int>(wp));
            return 0;

        case WM_TIMER: {
            auto it = timers_.find(static_cast<int>(wp));
            if (it != timers_.end()) SafeCall(it->second);
            return 0;
        }

        case WM_SIZE:
            // Physical pixels to the renderer, DIPs to the callback.
            renderer_.OnResize(LOWORD(lp), HIWORD(lp));
            SafeCall(on_resize_, PxToDip(LOWORD(lp)), PxToDip(HIWORD(lp)));
            Invalidate();
            return 0;

        case WM_DPICHANGED: {
            const float new_dpi = static_cast<float>(HIWORD(wp));
            if (new_dpi > 0.0f) {
                dpi_ = new_dpi;
                renderer_.SetScale(Scale());
            }
            const RECT* suggested = reinterpret_cast<const RECT*>(lp);
            if (suggested) {
                SetWindowPos(hwnd, nullptr, suggested->left, suggested->top,
                             suggested->right - suggested->left,
                             suggested->bottom - suggested->top,
                             SWP_NOZORDER | SWP_NOACTIVATE);
            }
            Invalidate();
            return 0;
        }

        case WM_GETMINMAXINFO: {
            // Fill in the system defaults first, then override only the
            // minimum track size. Returning early used to wipe ptMaxSize and
            // maximized-window bounds.
            DefWindowProcW(hwnd, msg, wp, lp);
            auto* mmi = reinterpret_cast<MINMAXINFO*>(lp);
            if (min_w_ > 0) mmi->ptMinTrackSize.x = DipToPx(min_w_);
            if (min_h_ > 0) mmi->ptMinTrackSize.y = DipToPx(min_h_);
            return 0;
        }

        case WM_NCCALCSIZE:
            // Borderless: the whole window becomes the client area. Keeping
            // WS_THICKFRAME lets DefWindowProc still resolve resize borders.
            if (borderless_ && wp) return 0;
            return DefWindowProcW(hwnd, msg, wp, lp);

        case WM_NCHITTEST: {
            if (!borderless_) return DefWindowProcW(hwnd, msg, wp, lp);

            POINT pt{GET_X_LPARAM(lp), GET_Y_LPARAM(lp)};
            ScreenToClient(hwnd, &pt);

            if (resizable_) {
                const int bw = GetSystemMetrics(SM_CXSIZEFRAME) +
                               GetSystemMetrics(SM_CXPADDEDBORDER);
                RECT rc;
                if (GetClientRect(hwnd, &rc)) {
                    const bool left   = pt.x < bw;
                    const bool right  = pt.x >= rc.right - bw;
                    const bool top    = pt.y < bw;
                    const bool bottom = pt.y >= rc.bottom - bw;
                    if (left && top)         return HTTOPLEFT;
                    if (right && top)        return HTTOPRIGHT;
                    if (left && bottom)      return HTBOTTOMLEFT;
                    if (right && bottom)     return HTBOTTOMRIGHT;
                    if (left)                return HTLEFT;
                    if (right)               return HTRIGHT;
                    if (top)                 return HTTOP;
                    if (bottom)              return HTBOTTOM;
                }
            }

            // Drag zone: top strip, but only where no widget eats the click.
            if (pt.y < DipToPx(kCaptionDip) &&
                HitTest(PxToDip(pt.x), PxToDip(pt.y)) < 0) {
                return HTCAPTION;
            }
            return HTCLIENT;
        }

        case WM_CLOSE:
            SafeCall(on_close_);
            DestroyWindow(hwnd);
            return 0;

        case WM_DESTROY:
            hwnd_ = nullptr;
            running_ = false;
            PostQuitMessage(0);
            return 0;

        default:
            return DefWindowProcW(hwnd, msg, wp, lp);
        }
    }

    static Color DimColor(const Color& c, const Color& bg) {
        constexpr float k = 0.45f;  // blend the widget color toward the background
        return Color{c.r * k + bg.r * (1.0f - k),
                     c.g * k + bg.g * (1.0f - k),
                     c.b * k + bg.b * (1.0f - k),
                     c.a};
    }

    void Render() {
        if (!hwnd_ || !renderer_.Begin(hwnd_)) return;

        renderer_.Clear(palette_.background);

        painting_ = true;
        if (on_paint_) SafeCall(on_paint_);
        painting_ = false;

        for (std::size_t i = 0; i < widgets_.size(); ++i) {
            const Widget& w = widgets_[i];
            if (!w.visible) continue;

            // A pressed button looks pressed only while the cursor is over it;
            // this fixes the desync where dragging off and back kept the
            // visual "pressed" state out of step with reality.
            const bool pressed_look =
                w.pressed && static_cast<int>(i) == hover_;

            if (w.kind == WidgetKind::Button) {
                Color fill = w.enabled
                    ? (pressed_look ? palette_.accentPressed
                       : static_cast<int>(i) == hover_ ? palette_.accentHover
                                                       : palette_.accent)
                    : DimColor(palette_.accent, palette_.background);
                renderer_.FillRoundedRect(float(w.x), float(w.y), float(w.w),
                                          float(w.h), 8.0f, fill);
                renderer_.DrawTextIn(float(w.x), float(w.y), float(w.w),
                                     float(w.h), w.text,
                                     w.enabled ? palette_.buttonText
                                               : DimColor(palette_.buttonText,
                                                          palette_.background),
                                     TextAlign::Center, 15.0f);
            } else if (w.kind == WidgetKind::Checkbox) {
                constexpr float box = 16.0f;
                const float cy = (w.y + w.h - box) * 0.5f;
                const Color off = Color{0.14f, 0.15f, 0.17f, 1.0f};
                renderer_.FillRoundedRect(
                    float(w.x), cy, box, box, 3.0f,
                    w.checked ? (w.enabled ? palette_.accent
                                  : DimColor(palette_.accent,
                                             palette_.background))
                              : off);
                renderer_.DrawRoundedRect(float(w.x), cy, box, box, 3.0f,
                                          palette_.border, 1.0f);
                if (w.checked) {
                    const Color check = w.enabled
                        ? palette_.buttonText
                        : DimColor(palette_.buttonText, palette_.background);
                    renderer_.DrawLine(w.x + 4.0f, cy + 8.0f, w.x + 7.0f,
                                       cy + 11.0f, check, 2.0f);
                    renderer_.DrawLine(w.x + 7.0f, cy + 11.0f, w.x + 12.0f,
                                       cy + 5.0f, check, 2.0f);
                }
                renderer_.DrawTextIn(float(w.x + int(box) + 8), float(w.y),
                                     float(w.w - int(box) - 8), float(w.h),
                                     w.text,
                                     w.enabled ? palette_.labelText
                                               : DimColor(palette_.labelText,
                                                          palette_.background),
                                     TextAlign::Left, 15.0f);
            } else {
                renderer_.DrawTextIn(float(w.x), float(w.y), float(w.w),
                                     float(w.h), w.text, palette_.labelText,
                                     TextAlign::Left, 15.0f);
            }
        }

        // Keyboard focus ring.
        if (focused_ >= 0 && IsFocusable(focused_)) {
            const Widget& w = widgets_[focused_];
            renderer_.DrawRoundedRect(float(w.x) - 2.0f, float(w.y) - 2.0f,
                                      float(w.w) + 4.0f, float(w.h) + 4.0f,
                                      9.0f, palette_.accentHover, 2.0f);
        }

        renderer_.End();
    }

    int HitTest(int x, int y) const {
        for (int i = static_cast<int>(widgets_.size()) - 1; i >= 0; --i) {
            const Widget& w = widgets_[i];
            if (w.kind == WidgetKind::Label) continue;
            if (!w.visible || !w.enabled) continue;
            if (x >= w.x && x < w.x + w.w && y >= w.y && y < w.y + w.h) return i;
        }
        return -1;
    }

    // Hover never mutates `pressed`: the pressed state belongs to the mouse
    // capture, not to the hover. A pressed button renders unpressed while the
    // cursor is elsewhere (see Render), so no visual desync is possible.
    void UpdateHover(int x, int y) {
        const int hover = HitTest(x, y);
        if (hover == hover_) return;
        hover_ = hover;
        if (hwnd_) {
            SetCursor(LoadCursorW(nullptr, hover_ >= 0 ? IDC_HAND : IDC_ARROW));
            Invalidate();
        }
    }

    bool IsFocusable(int id) const {
        if (!Valid(id)) return false;
        const Widget& w = widgets_[id];
        return w.kind != WidgetKind::Label && w.enabled && w.visible;
    }

    bool CycleFocus(int direction) {
        std::vector<int> order;
        for (int i = 0; i < static_cast<int>(widgets_.size()); ++i) {
            if (IsFocusable(i)) order.push_back(i);
        }
        if (order.empty()) return false;
        int pos = 0;
        bool found = false;
        for (int i = 0; i < static_cast<int>(order.size()); ++i) {
            if (order[i] == focused_) {
                pos = i;
                found = true;
                break;
            }
        }
        if (!found) {
            focused_ = direction >= 0 ? order.front() : order.back();
            return true;
        }
        pos = (pos + direction + static_cast<int>(order.size())) %
              static_cast<int>(order.size());
        focused_ = order[pos];
        return true;
    }

    void Activate(int id) {
        if (!IsFocusable(id)) return;
        Widget& w = widgets_[id];
        if (w.kind == WidgetKind::Button) SafeCall(w.onClick);
        if (w.kind == WidgetKind::Checkbox) {
            w.checked = !w.checked;
            SafeCall(w.onChange, w.checked);
        }
        Invalidate();
    }

    bool Valid(int id) const {
        return id >= 0 && id < static_cast<int>(widgets_.size());
    }

    static bool RegisterClassOnce() {
        static bool registered = false;
        if (registered) return true;
        registered = true;

        WNDCLASSEXW wc{};
        wc.cbSize = sizeof(wc);
        wc.lpfnWndProc = &Window::WndProcThunk;
        wc.hInstance = GetModuleHandleW(nullptr);
        wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        // CS_DBLCLKS is required for WM_LBUTTONDBLCLK to arrive at all.
        wc.style = CS_DBLCLKS | CS_HREDRAW | CS_VREDRAW;
        wc.lpszClassName = kClassName;
        return RegisterClassExW(&wc) != 0;
    }

    static constexpr int kModCtrl = 1;
    static constexpr int kModShift = 2;
    static constexpr int kModAlt = 4;
    static constexpr int kCaptionDip = 24;  // borderless drag strip height
    static constexpr wchar_t kClassName[] = L"SableUIWindow";

    HWND hwnd_ = nullptr;
    D2DRenderer renderer_;
    Palette palette_;
    std::vector<Widget> widgets_;
    std::unordered_map<int, std::function<void()>> timers_;
    int next_timer_id_ = 1;

    std::function<void()> on_paint_, on_close_;
    std::function<void(int, int, int)> on_mouse_down_, on_mouse_up_,
        on_double_click_;
    std::function<void(int, int)> on_mouse_move_;
    std::function<void(int, int, int)> on_mouse_wheel_;
    std::function<void(int, int)> on_key_down_, on_key_up_;
    std::function<void(unsigned int)> on_text_;
    std::function<void(int, int)> on_resize_;

    Color fill_{0, 0, 0, 1};
    Color stroke_{0, 0, 0, 1};
    Color text_{1, 1, 1, 1};
    float stroke_width_ = 1.0f;
    bool painting_ = false;
    bool running_ = false;
    bool resizable_ = true;
    bool borderless_ = false;
    bool com_initialized_ = false;
    int min_w_ = 0, min_h_ = 0;  // DIPs
    float dpi_ = 96.0f;
    int hover_ = -1;
    int pressed_ = -1;
    int focused_ = -1;
    bool tracking_ = false;
};

} // namespace sableui
