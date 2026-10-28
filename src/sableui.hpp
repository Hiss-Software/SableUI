#pragma once

// Internal implementation core of Sableui. Not part of the public contract:
// consumers must only include <sableui/sableui.h>.

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
#include <d2d1.h>
#include <dwrite.h>

#include <algorithm>
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

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
    int x, y, w, h;
    std::wstring text;
    bool checked = false;
    bool pressed = false;
    std::function<void()> onClick;
    std::function<void(bool)> onChange;
};

// Abstract rendering surface. A future backend (Direct3D, software) can be
// added without touching widgets or the window class.
class Renderer {
public:
    virtual ~Renderer() = default;
    virtual bool Begin(HWND hwnd) = 0;
    virtual void End() = 0;
    virtual void Clear(const Color& color) = 0;
    virtual void FillRoundedRect(int x, int y, int w, int h, float radius,
                                 const Color& color) = 0;
    virtual void DrawRoundedRect(int x, int y, int w, int h, float radius,
                                 const Color& color, float width) = 0;
    virtual void FillRect(int x, int y, int w, int h, const Color& color) = 0;
    virtual void DrawRect(int x, int y, int w, int h, const Color& color,
                          float width) = 0;
    virtual void FillEllipse(float cx, float cy, float rx, float ry,
                             const Color& color) = 0;
    virtual void DrawEllipse(float cx, float cy, float rx, float ry,
                             const Color& color, float width) = 0;
    virtual void DrawLine(float x1, float y1, float x2, float y2,
                          const Color& color, float width) = 0;
    virtual void DrawTextIn(int x, int y, int w, int h, const std::wstring& text,
                            const Color& color, TextAlign align,
                            float font_size) = 0;
    virtual void OnResize(unsigned width, unsigned height) = 0;
};

// Direct2D backend. Recreates the render target automatically on
// D2DERR_RECREATE_TARGET (device loss).
class D2DRenderer final : public Renderer {
public:
    D2DRenderer() = default;
    ~D2DRenderer() override { DiscardTarget(); }

    D2DRenderer(const D2DRenderer&) = delete;
    D2DRenderer& operator=(const D2DRenderer&) = delete;

    bool Begin(HWND hwnd) override {
        if (!EnsureTarget(hwnd)) return false;
        target_->BeginDraw();
        return true;
    }

    void End() override {
        if (!target_) return;
        if (target_->EndDraw() == D2DERR_RECREATE_TARGET) {
            DiscardTarget();  // device lost: recreated on the next Begin()
        }
    }

    void Clear(const Color& color) override {
        if (!target_) return;
        target_->Clear(ToD2D(color));
    }

    void FillRoundedRect(int x, int y, int w, int h, float radius,
                         const Color& color) override {
        if (!target_ || !brush_) return;
        brush_->SetColor(ToD2D(color));
        target_->FillRoundedRectangle(
            D2D1::RoundedRect(ToRectF(x, y, w, h), radius, radius), brush_);
    }

    void DrawRoundedRect(int x, int y, int w, int h, float radius,
                         const Color& color, float width) override {
        if (!target_ || !brush_) return;
        brush_->SetColor(ToD2D(color));
        target_->DrawRoundedRectangle(
            D2D1::RoundedRect(ToRectF(x, y, w, h), radius, radius), brush_,
            width);
    }

    void FillRect(int x, int y, int w, int h, const Color& color) override {
        if (!target_ || !brush_) return;
        brush_->SetColor(ToD2D(color));
        target_->FillRectangle(ToRectF(x, y, w, h), brush_);
    }

    void DrawRect(int x, int y, int w, int h, const Color& color,
                  float width) override {
        if (!target_ || !brush_) return;
        brush_->SetColor(ToD2D(color));
        target_->DrawRectangle(ToRectF(x, y, w, h), brush_, width);
    }

    void FillEllipse(float cx, float cy, float rx, float ry,
                     const Color& color) override {
        if (!target_ || !brush_) return;
        brush_->SetColor(ToD2D(color));
        target_->FillEllipse(D2D1::Ellipse(D2D1::Point2F(cx, cy), rx, ry),
                             brush_);
    }

    void DrawEllipse(float cx, float cy, float rx, float ry,
                     const Color& color, float width) override {
        if (!target_ || !brush_) return;
        brush_->SetColor(ToD2D(color));
        target_->DrawEllipse(D2D1::Ellipse(D2D1::Point2F(cx, cy), rx, ry),
                             brush_, width);
    }

    void DrawLine(float x1, float y1, float x2, float y2,
                  const Color& color, float width) override {
        if (!target_ || !brush_) return;
        brush_->SetColor(ToD2D(color));
        target_->DrawLine(D2D1::Point2F(x1, y1), D2D1::Point2F(x2, y2), brush_,
                          width);
    }

    void DrawTextIn(int x, int y, int w, int h, const std::wstring& text,
                    const Color& color, TextAlign align,
                    float font_size) override {
        if (!target_ || !brush_ || text.empty()) return;
        IDWriteTextFormat* format = GetFormat(font_size);
        if (!format) return;
        format->SetTextAlignment(align == TextAlign::Center
                                     ? DWRITE_TEXT_ALIGNMENT_CENTER
                                     : DWRITE_TEXT_ALIGNMENT_LEADING);
        format->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
        brush_->SetColor(ToD2D(color));
        target_->DrawTextW(text.c_str(), static_cast<UINT32>(text.size()),
                           format, ToRectF(x, y, w, h), brush_);
    }

    void OnResize(unsigned width, unsigned height) override {
        if (target_) {
            target_->Resize(D2D1::SizeU(std::max(1u, width), std::max(1u, height)));
        }
    }

    static bool IsDeviceLoss(HRESULT hr) { return hr == D2DERR_RECREATE_TARGET; }

private:
    static D2D1_COLOR_F ToD2D(const Color& c) {
        return D2D1_COLOR_F{c.r, c.g, c.b, c.a};
    }
    static D2D1_RECT_F ToRectF(int x, int y, int w, int h) {
        return D2D1::RectF(float(x), float(y), float(x + w), float(y + h));
    }

    IDWriteTextFormat* GetFormat(float px) {
        const int key = static_cast<int>(px * 4.0f);
        auto it = formats_.find(key);
        if (it != formats_.end()) return it->second;
        IDWriteTextFormat* f = nullptr;
        if (FAILED(write_->CreateTextFormat(
                L"Segoe UI", nullptr, DWRITE_FONT_WEIGHT_NORMAL,
                DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL,
                px, L"en-US", &f))) {
            return nullptr;
        }
        f->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
        formats_[key] = f;
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
                                         reinterpret_cast<void**>(&factory_)))) {
                return false;
            }
        }
        if (!write_) {
            if (FAILED(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED,
                                           __uuidof(IDWriteFactory),
                                           reinterpret_cast<IUnknown**>(&write_)))) {
                return false;
            }
        }

        if (FAILED(factory_->CreateHwndRenderTarget(
                D2D1::RenderTargetProperties(),
                D2D1::HwndRenderTargetProperties(hwnd, D2D1::SizeU(w, h)),
                &target_))) {
            return false;
        }

        if (FAILED(target_->CreateSolidColorBrush(D2D1_COLOR_F{0, 0, 0, 1}, &brush_))) {
            target_->Release();
            target_ = nullptr;
            return false;
        }
        return true;
    }

    void DiscardTarget() {
        for (auto& kv : formats_) if (kv.second) kv.second->Release();
        formats_.clear();
        if (write_)   { write_->Release();   write_   = nullptr; }
        if (brush_)   { brush_->Release();   brush_   = nullptr; }
        if (target_)  { target_->Release();  target_  = nullptr; }
        if (factory_) { factory_->Release(); factory_ = nullptr; }
    }

    ID2D1Factory* factory_ = nullptr;
    ID2D1HwndRenderTarget* target_ = nullptr;
    ID2D1SolidColorBrush* brush_ = nullptr;
    IDWriteFactory* write_ = nullptr;
    std::unordered_map<int, IDWriteTextFormat*> formats_;
};

// Win32 window with its own widget tree and message loop.
class Window {
public:
    Window() = default;
    ~Window() {
        if (hwnd_) DestroyWindow(hwnd_);
    }

    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;

    bool Create(const std::wstring& title, int width, int height,
                std::string* error) {
        auto fail = [error](const char* what) {
            if (error) *error = what;
            return false;
        };

        if (!RegisterClassOnce()) return fail("RegisterClassExW failed");

        hwnd_ = CreateWindowExW(
            0, kClassName, title.c_str(), WS_OVERLAPPEDWINDOW,
            CW_USEDEFAULT, CW_USEDEFAULT, width, height,
            nullptr, nullptr, GetModuleHandleW(nullptr), this);
        if (!hwnd_) return fail("CreateWindowExW failed");

        ShowWindow(hwnd_, SW_SHOW);
        if (error) error->clear();
        return true;
    }

    int AddButton(int x, int y, int w, int h, const std::wstring& text,
                  std::function<void()> onClick, std::string* error) {
        if (error) error->clear();
        widgets_.push_back(Widget{WidgetKind::Button, x, y, w, h, text,
                                  false, false, std::move(onClick), nullptr});
        Invalidate();
        return static_cast<int>(widgets_.size()) - 1;
    }

    int AddLabel(int x, int y, int w, int h, const std::wstring& text,
                 std::string* error) {
        if (error) error->clear();
        widgets_.push_back(Widget{WidgetKind::Label, x, y, w, h, text,
                                  false, false, nullptr, nullptr});
        Invalidate();
        return static_cast<int>(widgets_.size()) - 1;
    }

    int AddCheckbox(int x, int y, int w, int h, const std::wstring& text,
                    bool checked, std::function<void(bool)> onChange,
                    std::string* error) {
        if (error) error->clear();
        widgets_.push_back(Widget{WidgetKind::Checkbox, x, y, w, h, text,
                                  checked, false, nullptr, std::move(onChange)});
        Invalidate();
        return static_cast<int>(widgets_.size()) - 1;
    }

    bool SetLabelText(int id, const std::wstring& text) {
        if (id < 0 || id >= static_cast<int>(widgets_.size())) return false;
        if (widgets_[id].kind == WidgetKind::Checkbox) return false;
        widgets_[id].text = text;
        Invalidate();
        return true;
    }

    bool SetWidgetChecked(int id, bool on) {
        if (id < 0 || id >= static_cast<int>(widgets_.size())) return false;
        Widget& w = widgets_[id];
        if (w.kind != WidgetKind::Checkbox) return false;
        if (w.checked == on) return true;
        w.checked = on;
        SafeCall(w.onChange, on);
        Invalidate();
        return true;
    }

    bool IsChecked(int id) const {
        if (id < 0 || id >= static_cast<int>(widgets_.size())) return false;
        return widgets_[id].kind == WidgetKind::Checkbox && widgets_[id].checked;
    }

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

    void SetMinimumSize(int w, int h) {
        min_w_ = w;
        min_h_ = h;
    }

    void CloseRequest() {
        if (hwnd_) PostMessageW(hwnd_, WM_CLOSE, 0, 0);
    }

    void ClientSize(int* w, int* h) const {
        RECT rc{};
        if (hwnd_ && GetClientRect(hwnd_, &rc)) {
            if (w) *w = rc.right - rc.left;
            if (h) *h = rc.bottom - rc.top;
        } else {
            if (w) *w = 0;
            if (h) *h = 0;
        }
    }

    void Invalidate() {
        if (hwnd_) InvalidateRect(hwnd_, nullptr, FALSE);
    }

    // --- Event hooks ----------------------------------------------------------

    void OnMouseDown(std::function<void(int, int, int)> f) { on_mouse_down_ = std::move(f); }
    void OnMouseUp(std::function<void(int, int, int)> f)   { on_mouse_up_ = std::move(f); }
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

    void SetFillColor(const Color& c)   { fill_ = c; }
    void SetStrokeColor(const Color& c) { stroke_ = c; }
    void SetStrokeWidth(float w)        { stroke_width_ = w > 0 ? w : 1.0f; }

    void ClearWithFill()  { if (painting_) renderer_.Clear(fill_); }
    void FillRect(float x, float y, float w, float h) {
        if (painting_) renderer_.FillRect(int(x), int(y), int(w), int(h), fill_);
    }
    void DrawRect(float x, float y, float w, float h) {
        if (painting_) renderer_.DrawRect(int(x), int(y), int(w), int(h), stroke_, stroke_width_);
    }
    void FillRoundedRect(float x, float y, float w, float h, float r) {
        if (painting_) renderer_.FillRoundedRect(int(x), int(y), int(w), int(h), r, fill_);
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
        if (painting_) renderer_.DrawTextIn(int(x), int(y), int(w), int(h),
                                            text, fill_, TextAlign::Left, font_size);
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

    void ApplyStyles() {
        if (!hwnd_) return;
        LONG_PTR style = GetWindowLongPtrW(hwnd_, GWL_STYLE);
        style &= ~(WS_CAPTION | WS_THICKFRAME | WS_MAXIMIZEBOX);
        if (!borderless_) style |= WS_CAPTION;
        if (resizable_) style |= WS_THICKFRAME | WS_MAXIMIZEBOX;
        SetWindowLongPtrW(hwnd_, GWL_STYLE, style);
        SetWindowPos(hwnd_, nullptr, 0, 0, 0, 0,
                     SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
    }

    static LRESULT CALLBACK WndProcThunk(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
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
            const int x = GET_X_LPARAM(lp), y = GET_Y_LPARAM(lp);
            SetCapture(hwnd);
            const int hit = HitTest(x, y);
            if (hit >= 0) {
                pressed_ = hit;
                widgets_[hit].pressed = true;
            }
            SafeCall(on_mouse_down_, 0, x, y);
            UpdateHover(x, y);
            Invalidate();
            return 0;
        }

        case WM_RBUTTONDOWN:
        case WM_MBUTTONDOWN:
            SafeCall(on_mouse_down_,
                     msg == WM_RBUTTONDOWN ? 1 : 2,
                     GET_X_LPARAM(lp), GET_Y_LPARAM(lp));
            return 0;

        // Click is resolved BEFORE ReleaseCapture: ReleaseCapture sends a
        // synchronous WM_CAPTURECHANGED, and processing it first used to
        // reset pressed_ before the click check could run.
        case WM_LBUTTONUP: {
            const int x = GET_X_LPARAM(lp), y = GET_Y_LPARAM(lp);
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
                     GET_X_LPARAM(lp), GET_Y_LPARAM(lp));
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
            const int x = GET_X_LPARAM(lp), y = GET_Y_LPARAM(lp);
            UpdateHover(x, y);
            SafeCall(on_mouse_move_, x, y);
            return 0;
        }

        case WM_MOUSELEAVE:
            tracking_ = false;
            UpdateHover(-1, -1);
            return 0;

        case WM_MOUSEWHEEL: {
            POINT pt{GET_X_LPARAM(lp), GET_Y_LPARAM(lp)};
            ScreenToClient(hwnd, &pt);
            SafeCall(on_mouse_wheel_, GET_WHEEL_DELTA_WPARAM(wp), pt.x, pt.y);
            return 0;
        }

        case WM_KEYDOWN:
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

        case WM_TIMER: {
            auto it = timers_.find(static_cast<int>(wp));
            if (it != timers_.end()) SafeCall(it->second);
            return 0;
        }

        case WM_SIZE:
            renderer_.OnResize(LOWORD(lp), HIWORD(lp));
            SafeCall(on_resize_, static_cast<int>(LOWORD(lp)),
                     static_cast<int>(HIWORD(lp)));
            Invalidate();
            return 0;

        case WM_GETMINMAXINFO: {
            auto* mmi = reinterpret_cast<MINMAXINFO*>(lp);
            if (min_w_ > 0) mmi->ptMinTrackSize.x = min_w_;
            if (min_h_ > 0) mmi->ptMinTrackSize.y = min_h_;
            return 0;
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

    void Render() {
        if (!hwnd_ || !renderer_.Begin(hwnd_)) return;

        renderer_.Clear(palette_.background);

        painting_ = true;
        if (on_paint_) SafeCall(on_paint_);
        painting_ = false;

        for (std::size_t i = 0; i < widgets_.size(); ++i) {
            const Widget& w = widgets_[i];
            if (w.kind == WidgetKind::Button) {
                const Color& fill = w.pressed ? palette_.accentPressed
                                  : static_cast<int>(i) == hover_ ? palette_.accentHover
                                  : palette_.accent;
                renderer_.FillRoundedRect(w.x, w.y, w.w, w.h, 8.0f, fill);
                renderer_.DrawTextIn(w.x, w.y, w.w, w.h, w.text,
                                     palette_.buttonText, TextAlign::Center, 15.0f);
            } else if (w.kind == WidgetKind::Checkbox) {
                constexpr float box = 16.0f;
                const float cy = (w.y + w.h - box) * 0.5f;
                renderer_.FillRoundedRect(w.x, int(cy), int(box), int(box), 3.0f,
                                          w.checked ? palette_.accent
                                                    : Color{0.14f, 0.15f, 0.17f, 1.0f});
                renderer_.DrawRoundedRect(w.x, int(cy), int(box), int(box), 3.0f,
                                          palette_.border, 1.0f);
                if (w.checked) {
                    renderer_.DrawLine(w.x + 4.0f, cy + 8.0f, w.x + 7.0f, cy + 11.0f,
                                       palette_.buttonText, 2.0f);
                    renderer_.DrawLine(w.x + 7.0f, cy + 11.0f, w.x + 12.0f, cy + 5.0f,
                                       palette_.buttonText, 2.0f);
                }
                renderer_.DrawTextIn(w.x + int(box) + 8, w.y, w.w - int(box) - 8, w.h,
                                     w.text, palette_.labelText, TextAlign::Left, 15.0f);
            } else {
                renderer_.DrawTextIn(w.x, w.y, w.w, w.h, w.text,
                                     palette_.labelText, TextAlign::Left, 15.0f);
            }
        }
        renderer_.End();
    }

    int HitTest(int x, int y) const {
        for (int i = static_cast<int>(widgets_.size()) - 1; i >= 0; --i) {
            const Widget& w = widgets_[i];
            if (w.kind == WidgetKind::Label) continue;
            if (x >= w.x && x < w.x + w.w && y >= w.y && y < w.y + w.h) return i;
        }
        return -1;
    }

    void UpdateHover(int x, int y) {
        const int hover = HitTest(x, y);
        if (hover == hover_) return;
        if (hover_ >= 0 && hover_ < static_cast<int>(widgets_.size()))
            widgets_[hover_].pressed = false;
        hover_ = hover;
        if (hwnd_) {
            SetCursor(LoadCursorW(nullptr, hover_ >= 0 ? IDC_HAND : IDC_ARROW));
            Invalidate();
        }
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
        wc.lpszClassName = kClassName;
        return RegisterClassExW(&wc) != 0;
    }

    static constexpr int kModCtrl = 1;
    static constexpr int kModShift = 2;
    static constexpr int kModAlt = 4;
    static constexpr wchar_t kClassName[] = L"SableUIWindow";

    HWND hwnd_ = nullptr;
    D2DRenderer renderer_;
    Palette palette_;
    std::vector<Widget> widgets_;
    std::unordered_map<int, std::function<void()>> timers_;
    int next_timer_id_ = 1;

    std::function<void()> on_paint_, on_close_;
    std::function<void(int, int, int)> on_mouse_down_, on_mouse_up_;
    std::function<void(int, int)> on_mouse_move_;
    std::function<void(int, int, int)> on_mouse_wheel_;
    std::function<void(int, int)> on_key_down_, on_key_up_;
    std::function<void(unsigned int)> on_text_;
    std::function<void(int, int)> on_resize_;

    Color fill_{0, 0, 0, 1};
    Color stroke_{0, 0, 0, 1};
    float stroke_width_ = 1.0f;
    bool painting_ = false;
    bool running_ = false;
    bool resizable_ = true;
    bool borderless_ = false;
    int min_w_ = 0, min_h_ = 0;
    int hover_ = -1;
    int pressed_ = -1;
    bool tracking_ = false;
};

} // namespace sableui
