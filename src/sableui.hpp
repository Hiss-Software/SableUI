#pragma once

// Internal implementation core of Sableui. Not part of the public contract:
// consumers must only include <sableui/sableui.h>.
//
// All public coordinates and sizes are logical (DIP) units: 1 DIP = 1 px at
// 96 dpi. The renderer multiplies everything by the current monitor DPI, so
// the library is per-monitor DPI aware end to end.
//
// Known limitations:
//   - no IME composition (Chinese/Japanese/Korean input); WM_CHAR covers
//     Latin and Cyrillic keyboard layouts;
//   - no undo/redo in text fields;
//   - removing a scroll area detaches its children as top-level widgets with
//     stale (relative) coordinates.
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
#include <cwctype>
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
    Color fieldBg      { 0.161f, 0.165f, 0.190f, 1.0f };
    Color fieldBorder  { 0.300f, 0.310f, 0.360f, 1.0f };
    Color selection    { 0.310f, 0.549f, 1.0f,  0.35f };
    Color scrollbar    { 0.430f, 0.450f, 0.510f, 0.9f };
};

enum class WidgetKind { Button, Label, Checkbox, TextField, ScrollArea };

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
    // Text field state. All indices are code-unit offsets into `text`.
    bool masked = false;
    int caret = 0;
    int selAnchor = 0;
    int selStart = 0;
    int selEnd = 0;
    std::function<void(const std::wstring&)> onTextChange;
    // Ownership: >= 0 when the widget lives inside a scroll area and its
    // coordinates are relative to that area.
    int owner = -1;
    // Scroll offset in DIPs: horizontal for text fields, vertical for
    // scroll areas.
    int scrollOff = 0;
    // Layout anchors (bits from sableui.h SAB_ANCHOR_*): when non-zero, the
    // widget keeps its margins to the window client area across resizes.
    // Top-level widgets only (owner < 0).
    unsigned anchors = 0;
    int ml = 0, mt = 0, mr = 0, mb = 0;  // captured margins, DIPs
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
    // Text metrics in DIPs. Both are required for text fields to work:
    // caret placement and click hit-testing rely on them.
    virtual float TextAdvance(const std::wstring& text, float font_size,
                              int index) = 0;
    virtual int TextIndexAtX(const std::wstring& text, float font_size,
                             float x) = 0;
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

    // Caret/selection metrics for text fields. Layouts are built unscaled,
    // so the returned DIP coordinates need no DPI correction.
    float TextAdvance(const std::wstring& text, float font_size,
                      int index) override {
        if (text.empty() || index <= 0) return 0.0f;
        IDWriteTextLayout* raw = CreateLayout(text, font_size);
        if (!raw) return 0.0f;
        Microsoft::WRL::ComPtr<IDWriteTextLayout> layout;
        layout.Attach(raw);
        const UINT32 pos = static_cast<UINT32>(
            std::min<int>(index, static_cast<int>(text.size())));
        FLOAT x = 0.0f, y = 0.0f;
        DWRITE_HIT_TEST_METRICS m{};
        if (FAILED(layout->HitTestTextPosition(pos, FALSE, &x, &y, &m)))
            return 0.0f;
        return x;
    }

    int TextIndexAtX(const std::wstring& text, float font_size,
                     float x) override {
        if (text.empty()) return 0;
        IDWriteTextLayout* raw = CreateLayout(text, font_size);
        if (!raw) return 0;
        Microsoft::WRL::ComPtr<IDWriteTextLayout> layout;
        layout.Attach(raw);
        BOOL trailing = FALSE, inside = FALSE;
        DWRITE_HIT_TEST_METRICS m{};
        if (FAILED(layout->HitTestPoint(x, 1.0f, &trailing, &inside, &m)))
            return 0;
        return std::clamp(static_cast<int>(m.textPosition) + (trailing ? 1 : 0),
                          0, static_cast<int>(text.size()));
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

    IDWriteTextLayout* CreateLayout(const std::wstring& text, float px) {
        IDWriteTextFormat* fmt = GetFormat(px);
        if (!fmt || !write_) return nullptr;
        IDWriteTextLayout* layout = nullptr;
        if (FAILED(write_->CreateTextLayout(
                text.c_str(), static_cast<UINT32>(text.size()), fmt,
                1.0e6f, 1.0e6f, &layout))) {
            return nullptr;
        }
        return layout;
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
                                  std::move(onClick), nullptr,
                                  false, 0, 0, 0, 0, nullptr, -1, 0});
        Invalidate();
        return static_cast<int>(widgets_.size()) - 1;
    }

    int AddLabel(int x, int y, int w, int h, const std::wstring& text) {
        widgets_.push_back(Widget{WidgetKind::Label, x, y, w, h, text,
                                  false, false, true, true,
                                  nullptr, nullptr,
                                  false, 0, 0, 0, 0, nullptr, -1, 0});
        Invalidate();
        return static_cast<int>(widgets_.size()) - 1;
    }

    int AddCheckbox(int x, int y, int w, int h, const std::wstring& text,
                    bool checked, std::function<void(bool)> onChange) {
        widgets_.push_back(Widget{WidgetKind::Checkbox, x, y, w, h, text,
                                  checked, false, true, true,
                                  nullptr, std::move(onChange),
                                  false, 0, 0, 0, 0, nullptr, -1, 0});
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

    // Adds a single-line text field. Coordinates in DIPs. When masked, the
    // field renders '*' instead of characters (passwords).
    int AddTextField(int x, int y, int w, int h, const std::wstring& text,
                     bool masked,
                     std::function<void(const std::wstring&)> onTextChange) {
        Widget wf{};
        wf.kind = WidgetKind::TextField;
        wf.x = x; wf.y = y; wf.w = w; wf.h = h;
        wf.text = text;
        wf.masked = masked;
        wf.onTextChange = std::move(onTextChange);
        widgets_.push_back(std::move(wf));
        Invalidate();
        return static_cast<int>(widgets_.size()) - 1;
    }

    // Adds a vertical scroll area. Widgets added with the Add*In family live
    // inside it and use coordinates relative to the area.
    int AddScrollArea(int x, int y, int w, int h) {
        Widget sa{};
        sa.kind = WidgetKind::ScrollArea;
        sa.x = x; sa.y = y; sa.w = w; sa.h = h;
        widgets_.push_back(std::move(sa));
        Invalidate();
        return static_cast<int>(widgets_.size()) - 1;
    }

    // --- Widgets inside a scroll area ---------------------------------------
    // Coordinates are relative to the area's top-left corner and may exceed
    // the area's height; the area scrolls them.

    int AddButtonIn(int area, int x, int y, int bw, int bh,
                    const std::wstring& text, std::function<void()> onClick) {
        if (!Valid(area) ||
            widgets_[area].kind != WidgetKind::ScrollArea) return -1;
        Widget w{};
        w.kind = WidgetKind::Button;
        w.x = x; w.y = y; w.w = bw; w.h = bh;
        w.text = text;
        w.onClick = std::move(onClick);
        w.owner = area;
        widgets_.push_back(std::move(w));
        Invalidate();
        return static_cast<int>(widgets_.size()) - 1;
    }

    int AddLabelIn(int area, int x, int y, int bw, int bh,
                   const std::wstring& text) {
        if (!Valid(area) ||
            widgets_[area].kind != WidgetKind::ScrollArea) return -1;
        Widget w{};
        w.kind = WidgetKind::Label;
        w.x = x; w.y = y; w.w = bw; w.h = bh;
        w.text = text;
        w.owner = area;
        widgets_.push_back(std::move(w));
        Invalidate();
        return static_cast<int>(widgets_.size()) - 1;
    }

    int AddCheckboxIn(int area, int x, int y, int bw, int bh,
                      const std::wstring& text, bool checked,
                      std::function<void(bool)> onChange) {
        if (!Valid(area) ||
            widgets_[area].kind != WidgetKind::ScrollArea) return -1;
        Widget w{};
        w.kind = WidgetKind::Checkbox;
        w.x = x; w.y = y; w.w = bw; w.h = bh;
        w.text = text;
        w.checked = checked;
        w.onChange = std::move(onChange);
        w.owner = area;
        widgets_.push_back(std::move(w));
        Invalidate();
        return static_cast<int>(widgets_.size()) - 1;
    }

    int AddTextFieldIn(int area, int x, int y, int bw, int bh,
                       const std::wstring& text, bool masked,
                       std::function<void(const std::wstring&)> onTextChange) {
        if (!Valid(area) ||
            widgets_[area].kind != WidgetKind::ScrollArea) return -1;
        Widget w{};
        w.kind = WidgetKind::TextField;
        w.x = x; w.y = y; w.w = bw; w.h = bh;
        w.text = text;
        w.masked = masked;
        w.onTextChange = std::move(onTextChange);
        w.owner = area;
        widgets_.push_back(std::move(w));
        Invalidate();
        return static_cast<int>(widgets_.size()) - 1;
    }

    // Vertical scroll position of a scroll area, in DIPs. Returns false for
    // non-scroll-area ids.
    bool SetScrollOffset(int areaId, int offset) {
        if (!Valid(areaId) ||
            widgets_[areaId].kind != WidgetKind::ScrollArea) return false;
        widgets_[areaId].scrollOff = offset;
        ClampScrollOffset(widgets_[areaId], areaId);
        Invalidate();
        return true;
    }

    int GetScrollOffset(int areaId) const {
        if (!Valid(areaId) ||
            widgets_[areaId].kind != WidgetKind::ScrollArea) return -1;
        return widgets_[areaId].scrollOff;
    }

    // --- Layout anchors --------------------------------------------------------

    // Captures the widget's current rect as the anchor reference. Top-level
    // widgets only: children of scroll areas live in content coordinates
    // and scroll with the content, so anchoring them is meaningless.
    bool SetWidgetAnchors(int id, unsigned anchors) {
        if (!Valid(id)) return false;
        Widget& w = widgets_[id];
        if (w.owner >= 0) return false;
        w.anchors = anchors;
        int cw = 0, ch = 0;
        ClientSize(&cw, &ch);
        w.ml = w.x;
        w.mt = w.y;
        w.mr = cw - (w.x + w.w);
        w.mb = ch - (w.y + w.h);
        Invalidate();
        return true;
    }

    std::wstring GetWidgetText(int id) const {
        if (!Valid(id)) return L"";
        return widgets_[id].text;
    }

    bool SetLabelText(int id, const std::wstring& text) {
        return SetWidgetText(id, text);
    }

    bool SetWidgetText(int id, const std::wstring& text) {
        if (!Valid(id)) return false;
        widgets_[id].text = text;
        if (widgets_[id].kind == WidgetKind::TextField)
            ClampTextField(widgets_[id]);
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
        if (!on && focused_ == id) {
            focused_ = -1;
            EnsureCaretTimer();
        }
        if (!on && pressed_ == id) pressed_ = -1;
        Invalidate();
        return true;
    }

    bool SetWidgetVisible(int id, bool on) {
        if (!Valid(id)) return false;
        if (widgets_[id].visible == on) return true;
        widgets_[id].visible = on;
        if (!on && focused_ == id) {
            focused_ = -1;
            EnsureCaretTimer();
        }
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
        if (focused_ == id) {
            focused_ = -1;
            EnsureCaretTimer();
        } else if (focused_ > id) {
            --focused_;
        }
        if (text_hit_ == id) text_hit_ = -1;
        else if (text_hit_ > id) --text_hit_;
        if (scroll_drag_ == id) scroll_drag_ = -1;
        else if (scroll_drag_ > id) --scroll_drag_;
        // Children of a removed scroll area become top-level widgets; their
        // coordinates stay relative and may need manual adjustment.
        auto fixOwner = [id](int& owner) {
            if (owner == id) owner = -1;
            else if (owner > id) --owner;
        };
        for (auto& w : widgets_) fixOwner(w.owner);
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
        if (text_hit_ == id) text_hit_ = last;
        else if (text_hit_ > id) --text_hit_;
        if (scroll_drag_ == id) scroll_drag_ = last;
        else if (scroll_drag_ > id) --scroll_drag_;
        for (auto& w : widgets_) fix(w.owner);
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
    // Returning true (or installing no handler) closes the window; returning
    // false keeps it open (e.g. for a "save changes?" dialog).
    void OnClose(std::function<bool()> f)                  { on_close_ = std::move(f); }
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
                Widget& hw = widgets_[hit];
                if (hw.kind == WidgetKind::TextField) {
                    focused_ = hit;
                    text_hit_ = hit;
                    const int caret = TextFieldCaretAt(hw, x);
                    SetCaret(hw, caret,
                             (GetKeyState(VK_SHIFT) & 0x8000) != 0);
                } else if (hw.kind == WidgetKind::ScrollArea) {
                    focused_ = hit;
                    if (InScrollbarColumn(hw, x)) {
                        scroll_drag_ = hit;
                        const int contentH = ScrollContentHeight(hit);
                        scroll_drag_grab_ = y - ThumbTop(hw, contentH);
                        ApplyScrollDrag(hw, y);
                    }
                } else {
                    pressed_ = hit;
                    hw.pressed = true;
                    if (IsFocusable(hit)) focused_ = hit;
                }
            } else {
                focused_ = -1;
            }
            EnsureCaretTimer();
            SafeCall(on_mouse_down_, 0, x, y);
            UpdateHover(x, y);
            Invalidate();
            return 0;
        }

        case WM_LBUTTONDBLCLK: {
            const int x = PxToDip(GET_X_LPARAM(lp));
            const int y = PxToDip(GET_Y_LPARAM(lp));
            const int hit = HitTest(x, y);
            if (hit >= 0 && widgets_[hit].kind == WidgetKind::TextField) {
                focused_ = hit;
                text_hit_ = hit;
                EnsureCaretTimer();
                SelectWord(widgets_[hit], TextFieldCaretAt(widgets_[hit], x));
                Invalidate();
                return 0;
            }
            SafeCall(on_double_click_, 0, x, y);
            [[fallthrough]];
        }

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
            text_hit_ = -1;
            scroll_drag_ = -1;
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
            text_hit_ = -1;
            scroll_drag_ = -1;
            return 0;

        case WM_MOUSEMOVE: {
            if (!tracking_) {
                TRACKMOUSEEVENT tme{sizeof(tme), TME_LEAVE, hwnd, 0};
                TrackMouseEvent(&tme);
                tracking_ = true;
            }
            const int x = PxToDip(GET_X_LPARAM(lp));
            const int y = PxToDip(GET_Y_LPARAM(lp));
            if (text_hit_ >= 0) {
                Widget& tf = widgets_[text_hit_];
                const int caret = TextFieldCaretAt(tf, x);
                if (caret != tf.caret) {
                    SetCaret(tf, caret, true);
                    Invalidate();
                }
            } else if (scroll_drag_ >= 0) {
                ApplyScrollDrag(widgets_[scroll_drag_], y);
            }
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
            const int x = PxToDip(pt.x);
            const int y = PxToDip(pt.y);
            if (msg == WM_MOUSEWHEEL) {
                const int hit = HitTest(x, y);
                if (hit >= 0) {
                    const int areaId = widgets_[hit].owner >= 0
                        ? widgets_[hit].owner
                        : (widgets_[hit].kind == WidgetKind::ScrollArea
                               ? hit : -1);
                    if (areaId >= 0) {
                        Widget& sa = widgets_[areaId];
                        const int maxOff =
                            std::max(0, ScrollContentHeight(areaId) - sa.h);
                        sa.scrollOff -=
                            (GET_WHEEL_DELTA_WPARAM(wp) / WHEEL_DELTA) *
                            kScrollLineDip;
                        sa.scrollOff = std::clamp(sa.scrollOff, 0, maxOff);
                        Invalidate();
                    }
                }
            }
            SafeCall(on_mouse_wheel_, GET_WHEEL_DELTA_WPARAM(wp), x, y);
            return 0;
        }

        case WM_KEYDOWN:
            // Text fields consume their keys first; anything they do not
            // handle falls through to focus navigation and user hooks.
            if (focused_ >= 0 &&
                widgets_[focused_].kind == WidgetKind::TextField) {
                if (HandleTextFieldKey(widgets_[focused_],
                                       static_cast<int>(wp))) {
                    return 0;
                }
            }
            if (wp == VK_TAB && CycleFocus(
                    (GetKeyState(VK_SHIFT) & 0x8000) ? -1 : 1)) {
                EnsureCaretTimer();
                ScrollIntoView(focused_);
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
            if (focused_ >= 0 &&
                widgets_[focused_].kind == WidgetKind::TextField) {
                TextFieldChar(widgets_[focused_], static_cast<wchar_t>(wp));
            }
            SafeCall(on_text_, static_cast<unsigned int>(wp));
            return 0;

        case WM_UNICHAR:
            if (wp == UNICODE_NOCHAR) return 1;  // capability query
            if (focused_ >= 0 &&
                widgets_[focused_].kind == WidgetKind::TextField) {
                TextFieldChar(widgets_[focused_], static_cast<wchar_t>(wp));
            }
            SafeCall(on_text_, static_cast<unsigned int>(wp));
            return 0;

        case WM_TIMER:
            if (wp == static_cast<UINT_PTR>(kCaretTimerId)) {
                caret_on_ = !caret_on_;
                Invalidate();
                return 0;
            }
            {
                auto it = timers_.find(static_cast<int>(wp));
                if (it != timers_.end()) SafeCall(it->second);
            }
            return 0;

        case WM_SIZE:
            // Physical pixels to the renderer, DIPs to the callback.
            renderer_.OnResize(LOWORD(lp), HIWORD(lp));
            // Skip anchoring while minimized: a zero-sized client area would
            // collapse anchored widgets and freeze their margins at zero.
            if (wp != SIZE_MINIMIZED) ApplyAnchors();
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

        case WM_CLOSE: {
            // The on_close hook may veto the close (return false to keep the
            // window alive, e.g. for a "save changes?" dialog).
            bool allow = true;
            if (on_close_) {
                try {
                    allow = on_close_();
                } catch (...) {
                    allow = true;
                }
            }
            if (allow) DestroyWindow(hwnd);
            return 0;
        }

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

    // Re-applies anchors after the client area changed. Margins were
    // captured by SetWidgetAnchors; non-anchored edges keep their current
    // coordinate or size.
    void ApplyAnchors() {
        int cw = 0, ch = 0;
        ClientSize(&cw, &ch);
        for (std::size_t i = 0; i < widgets_.size(); ++i) {
            Widget& w = widgets_[i];
            if (w.owner >= 0 || w.anchors == 0) continue;
            const unsigned a = w.anchors;
            if ((a & kAnchorLeft) && (a & kAnchorRight)) {
                w.x = w.ml;
                w.w = std::max(1, cw - w.ml - w.mr);
            } else if (a & kAnchorRight) {
                w.x = cw - w.mr - w.w;  // keep size, pin right edge
            }
            if ((a & kAnchorTop) && (a & kAnchorBottom)) {
                w.y = w.mt;
                w.h = std::max(1, ch - w.mt - w.mb);
            } else if (a & kAnchorBottom) {
                w.y = ch - w.mb - w.h;
            }
            if (w.kind == WidgetKind::ScrollArea) {
                ClampScrollOffset(w, static_cast<int>(i));
            }
        }
    }

    void Render() {
        if (!hwnd_ || !renderer_.Begin(hwnd_)) return;

        renderer_.Clear(palette_.background);

        painting_ = true;
        if (on_paint_) SafeCall(on_paint_);
        painting_ = false;

        for (std::size_t i = 0; i < widgets_.size(); ++i) {
            Widget& w = widgets_[i];
            if (!w.visible) continue;
            if (w.owner >= 0) continue;  // drawn by the owning scroll area
            if (w.kind == WidgetKind::ScrollArea) {
                DrawScrollArea(w, static_cast<int>(i));
            } else {
                DrawWidget(w, static_cast<int>(i), 0, 0);
            }
        }

        // Keyboard focus ring.
        if (focused_ >= 0 && IsFocusable(focused_)) {
            const Widget& w = widgets_[focused_];
            int ox = 0, oy = 0;
            if (w.owner >= 0) {
                const Widget& o = widgets_[w.owner];
                ox = o.x;
                oy = o.y - o.scrollOff;
            }
            renderer_.DrawRoundedRect(float(w.x + ox) - 2.0f,
                                      float(w.y + oy) - 2.0f,
                                      float(w.w) + 4.0f, float(w.h) + 4.0f,
                                      9.0f, palette_.accentHover, 2.0f);
        }

        renderer_.End();
    }

    // Draws one scroll area: background, its children (clipped and offset by
    // the scroll position) and the scrollbar.
    void DrawScrollArea(Widget& sa, int id) {
        const float fx = float(sa.x);
        const float fy = float(sa.y);
        renderer_.FillRoundedRect(fx, fy, float(sa.w), float(sa.h), 6.0f,
                                  palette_.fieldBg);
        renderer_.PushClipRect(fx + 1.0f, fy + 1.0f, float(sa.w) - 2.0f,
                               float(sa.h) - 2.0f);
        for (std::size_t i = 0; i < widgets_.size(); ++i) {
            Widget& c = widgets_[i];
            if (c.owner != id || !c.visible) continue;
            DrawWidget(c, static_cast<int>(i), sa.x, sa.y - sa.scrollOff);
        }
        renderer_.PopClipRect();

        const int contentH = ScrollContentHeight(id);
        if (contentH > sa.h) {
            const int th = ThumbHeight(sa, contentH);
            const int tt = ThumbTop(sa, contentH);
            renderer_.FillRoundedRect(fx + float(sa.w) - 9.0f, float(tt),
                                      6.0f, float(th), 3.0f,
                                      palette_.scrollbar);
        }
    }

    // Draws a single widget. ox/oy translate the widget (scroll area offset).
    void DrawWidget(Widget& w, int id, int ox, int oy) {
        const float fx = float(w.x + ox);
        const float fy = float(w.y + oy);

        if (w.kind == WidgetKind::Button) {
            // A pressed button looks pressed only while the cursor is over
            // it; this fixes the desync where dragging off and back kept the
            // visual "pressed" state out of step with reality.
            const bool pressed_look =
                w.pressed && id == hover_;
            Color fill = w.enabled
                ? (pressed_look ? palette_.accentPressed
                   : id == hover_ ? palette_.accentHover
                                  : palette_.accent)
                : DimColor(palette_.accent, palette_.background);
            renderer_.FillRoundedRect(fx, fy, float(w.w), float(w.h), 8.0f, fill);
            renderer_.DrawTextIn(fx, fy, float(w.w), float(w.h), w.text,
                                 w.enabled ? palette_.buttonText
                                           : DimColor(palette_.buttonText,
                                                      palette_.background),
                                 TextAlign::Center, 15.0f);
        } else if (w.kind == WidgetKind::Checkbox) {
            constexpr float box = 16.0f;
            const float cy = fy + (float(w.h) - box) * 0.5f;
            const Color off = Color{0.14f, 0.15f, 0.17f, 1.0f};
            renderer_.FillRoundedRect(fx, cy, box, box, 3.0f,
                                      w.checked
                                          ? (w.enabled
                                                 ? palette_.accent
                                                 : DimColor(palette_.accent,
                                                            palette_.background))
                                          : off);
            renderer_.DrawRoundedRect(fx, cy, box, box, 3.0f,
                                      palette_.border, 1.0f);
            if (w.checked) {
                const Color check = w.enabled
                    ? palette_.buttonText
                    : DimColor(palette_.buttonText, palette_.background);
                renderer_.DrawLine(fx + 4.0f, cy + 8.0f, fx + 7.0f,
                                   cy + 11.0f, check, 2.0f);
                renderer_.DrawLine(fx + 7.0f, cy + 11.0f, fx + 12.0f,
                                   cy + 5.0f, check, 2.0f);
            }
            renderer_.DrawTextIn(fx + box + 8.0f, fy, float(w.w) - box - 8.0f,
                                 float(w.h), w.text,
                                 w.enabled ? palette_.labelText
                                           : DimColor(palette_.labelText,
                                                      palette_.background),
                                 TextAlign::Left, 15.0f);
        } else if (w.kind == WidgetKind::TextField) {
            constexpr float pad = kFieldPadDip;
            constexpr float font = kFieldFontDip;
            const bool is_focused = (focused_ == id);
            renderer_.FillRoundedRect(fx, fy, float(w.w), float(w.h), 6.0f,
                                      palette_.fieldBg);
            renderer_.DrawRoundedRect(fx, fy, float(w.w), float(w.h), 6.0f,
                                      is_focused ? palette_.accent
                                                 : palette_.fieldBorder,
                                      1.0f);
            if (!w.enabled) {
                renderer_.DrawTextIn(fx + pad, fy, float(w.w) - 2.0f * pad,
                                     float(w.h), DisplayText(w),
                                     DimColor(palette_.labelText,
                                              palette_.background),
                                     TextAlign::Left, font);
                return;
            }
            const std::wstring disp = DisplayText(w);
            const float innerW = float(w.w) - 2.0f * pad;
            const float caretX = renderer_.TextAdvance(disp, font, w.caret);
            // Horizontal auto-scroll: keep the caret inside the field.
            if (caretX - float(w.scrollOff) > innerW) {
                w.scrollOff = static_cast<int>(std::lround(caretX - innerW));
            }
            if (caretX - float(w.scrollOff) < 0.0f) {
                w.scrollOff = static_cast<int>(std::lround(caretX));
            }
            w.scrollOff = std::max(0, w.scrollOff);
            const float scroll = float(w.scrollOff);
            if (w.selEnd > w.selStart) {
                const float x1 =
                    renderer_.TextAdvance(disp, font, w.selStart) - scroll;
                const float x2 =
                    renderer_.TextAdvance(disp, font, w.selEnd) - scroll;
                renderer_.FillRect(fx + pad + x1, fy + 2.0f, x2 - x1,
                                   float(w.h) - 4.0f, palette_.selection);
            }
            renderer_.PushClipRect(fx + 1.0f, fy + 1.0f, float(w.w) - 2.0f,
                                   float(w.h) - 2.0f);
            renderer_.DrawTextIn(fx + pad - scroll, fy, innerW, float(w.h),
                                 disp, palette_.labelText, TextAlign::Left,
                                 font);
            if (is_focused && caret_on_) {
                const float cx = fx + pad + caretX - scroll;
                renderer_.DrawLine(cx, fy + 4.0f, cx, fy + float(w.h) - 4.0f,
                                   palette_.labelText, 1.5f);
            }
            renderer_.PopClipRect();
        } else {  // Label
            renderer_.DrawTextIn(fx, fy, float(w.w), float(w.h), w.text,
                                 palette_.labelText, TextAlign::Left, 15.0f);
        }
    }

    int HitTest(int x, int y) const {
        // Topmost widget wins. Owned widgets are tested in the same pass as
        // top-level ones (their coordinates are mapped through the owning
        // scroll area); scroll areas themselves are hit only if no widget
        // under the cursor claims the point.
        for (int i = static_cast<int>(widgets_.size()) - 1; i >= 0; --i) {
            const Widget& w = widgets_[i];
            if (!w.visible || !w.enabled) continue;
            if (w.kind == WidgetKind::Label) continue;
            if (w.kind == WidgetKind::ScrollArea) continue;
            int wx = w.x;
            int wy = w.y;
            if (w.owner >= 0) {
                const Widget& o = widgets_[w.owner];
                if (!o.visible || !o.enabled) continue;
                if (x < o.x || x >= o.x + o.w) continue;
                if (y < o.y || y >= o.y + o.h) continue;
                wy = w.y - o.scrollOff;
            }
            if (x >= wx && x < wx + w.w && y >= wy && y < wy + w.h) return i;
        }
        for (int i = static_cast<int>(widgets_.size()) - 1; i >= 0; --i) {
            const Widget& w = widgets_[i];
            if (w.kind != WidgetKind::ScrollArea) continue;
            if (!w.visible || !w.enabled) continue;
            if (x >= w.x && x < w.x + w.w && y >= w.y && y < w.y + w.h)
                return i;
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
            HCURSOR cur = LoadCursorW(nullptr, IDC_ARROW);
            if (hover_ >= 0) {
                cur = widgets_[hover_].kind == WidgetKind::TextField
                          ? LoadCursorW(nullptr, IDC_IBEAM)
                          : LoadCursorW(nullptr, IDC_HAND);
            }
            SetCursor(cur);
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
        } else {
            pos = (pos + direction + static_cast<int>(order.size())) %
                  static_cast<int>(order.size());
            focused_ = order[pos];
        }
        ScrollIntoView(focused_);
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

    // --- Text field internals ---------------------------------------------------

    // What is actually rendered: '*' for masked fields, the text otherwise.
    std::wstring DisplayText(const Widget& tf) const {
        if (!tf.masked) return tf.text;
        return std::wstring(tf.text.size(), L'*');
    }

    // Caret index under a window-space x coordinate (DIPs).
    int TextFieldCaretAt(const Widget& tf, int xDip) {
        const std::wstring disp = DisplayText(tf);
        const float lx = float(xDip) - (float(tf.x) + kFieldPadDip) +
                         float(tf.scrollOff);
        return renderer_.TextIndexAtX(disp, kFieldFontDip, lx);
    }

    // Moves the caret and updates the selection. extend=true keeps the
    // selection anchor (Shift held or drag-select).
    static void SetCaret(Widget& tf, int pos, bool extend) {
        tf.caret = pos;
        if (extend) {
            tf.selStart = std::min(tf.selAnchor, pos);
            tf.selEnd = std::max(tf.selAnchor, pos);
        } else {
            tf.selAnchor = pos;
            tf.selStart = pos;
            tf.selEnd = pos;
        }
    }

    // Deletes the selected range, if any; leaves the caret at its start.
    void DeleteSelection(Widget& tf) {
        if (tf.selEnd <= tf.selStart) return;
        tf.text.erase(tf.selStart, tf.selEnd - tf.selStart);
        tf.caret = tf.selStart;
        tf.selAnchor = tf.caret;
        tf.selEnd = tf.selStart;
    }

    // Clamps all caret/selection indices after any text mutation.
    void ClampTextField(Widget& tf) {
        const int len = static_cast<int>(tf.text.size());
        tf.caret = std::clamp(tf.caret, 0, len);
        tf.selAnchor = std::clamp(tf.selAnchor, 0, len);
        tf.selStart = std::clamp(tf.selStart, 0, len);
        tf.selEnd = std::clamp(tf.selEnd, 0, len);
        if (tf.selStart > tf.selEnd) std::swap(tf.selStart, tf.selEnd);
    }

    // Caret-only change: no text was touched, so no onChange fires.
    void CommitTextField(Widget& tf) {
        ClampTextField(tf);
        Invalidate();
    }

    // Text changed: clamp, restart the blink phase, notify, repaint.
    void AfterTextFieldEdit(Widget& tf) {
        ClampTextField(tf);
        caret_on_ = true;
        SafeCall(tf.onTextChange, tf.text);
        Invalidate();
    }

    void TextFieldChar(Widget& tf, wchar_t ch) {
        if (ch == L'\b') {  // backspace
            if (tf.selEnd > tf.selStart) {
                DeleteSelection(tf);
            } else if (tf.caret > 0) {
                tf.text.erase(tf.caret - 1, 1);
                SetCaret(tf, tf.caret - 1, false);
            } else {
                return;
            }
            AfterTextFieldEdit(tf);
            return;
        }
        if (ch < 0x20 || ch == 0x7F) return;  // control characters are ignored
        DeleteSelection(tf);
        tf.text.insert(tf.caret, 1, ch);
        SetCaret(tf, tf.caret + 1, false);
        AfterTextFieldEdit(tf);
    }

    // Word-boundary caret step for Ctrl+Left/Ctrl+Right.
    static int WordStep(const std::wstring& s, int pos, int dir) {
        const int n = static_cast<int>(s.size());
        int i = std::clamp(pos, 0, n);
        auto isSpace = [](wchar_t ch) { return iswspace(ch) != 0; };
        if (dir < 0) {
            while (i > 0 && isSpace(s[i - 1])) --i;
            while (i > 0 && !isSpace(s[i - 1])) --i;
        } else {
            while (i < n && isSpace(s[i])) ++i;
            while (i < n && !isSpace(s[i])) ++i;
        }
        return i;
    }

    void SelectWord(Widget& tf, int pos) {
        const std::wstring& s = tf.text;
        const int n = static_cast<int>(s.size());
        int a = std::clamp(pos, 0, n);
        int b = a;
        auto isWordChar = [](wchar_t ch) {
            return iswalnum(ch) != 0 || ch == L'_';
        };
        while (a > 0 && isWordChar(s[a - 1])) --a;
        while (b < n && isWordChar(s[b])) ++b;
        tf.selAnchor = a;
        SetCaret(tf, b, true);
        CommitTextField(tf);
    }

    // Handles key events targeted at the focused text field. Returns true
    // when the key is consumed.
    bool HandleTextFieldKey(Widget& tf, int vk) {
        const bool ctrl = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
        const bool shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
        const int len = static_cast<int>(tf.text.size());
        switch (vk) {
        case VK_LEFT: {
            int c = tf.caret;
            if (ctrl) c = WordStep(tf.text, c, -1);
            else if (c > 0) --c;
            SetCaret(tf, c, shift);
            CommitTextField(tf);
            return true;
        }
        case VK_RIGHT: {
            int c = tf.caret;
            if (ctrl) c = WordStep(tf.text, c, +1);
            else if (c < len) ++c;
            SetCaret(tf, c, shift);
            CommitTextField(tf);
            return true;
        }
        case VK_HOME:
            SetCaret(tf, 0, shift);
            CommitTextField(tf);
            return true;
        case VK_END:
            SetCaret(tf, len, shift);
            CommitTextField(tf);
            return true;
        case VK_DELETE: {
            const int before = static_cast<int>(tf.text.size());
            if (tf.selEnd > tf.selStart) {
                DeleteSelection(tf);
            } else if (tf.caret < len) {
                tf.text.erase(tf.caret, 1);
            }
            if (static_cast<int>(tf.text.size()) != before)
                AfterTextFieldEdit(tf);
            return true;
        }
        case VK_UP:
        case VK_DOWN:
            return true;      // single-line field: consumed, no movement
        case VK_SPACE:
        case VK_RETURN:
            return true;      // space arrives via WM_CHAR; Enter is ignored
        case L'A':
            if (ctrl) {
                tf.selAnchor = 0;
                SetCaret(tf, len, true);
                CommitTextField(tf);
                return true;
            }
            return false;
        case L'C':
            if (ctrl) { CopyToClipboard(tf, false); return true; }
            return false;
        case L'X':
            if (ctrl) { CopyToClipboard(tf, true); return true; }
            return false;
        case L'V':
            if (ctrl) { PasteFromClipboard(tf); return true; }
            return false;
        default:
            return false;     // navigation keys (Tab etc.) pass through
        }
    }

    bool CopyToClipboard(Widget& tf, bool cut) {
        if (tf.selEnd <= tf.selStart) return false;
        const std::wstring sel =
            tf.text.substr(tf.selStart, tf.selEnd - tf.selStart);
        if (!OpenClipboard(hwnd_)) return false;
        EmptyClipboard();
        const SIZE_T bytes = (sel.size() + 1) * sizeof(wchar_t);
        HGLOBAL g = GlobalAlloc(GMEM_MOVEABLE, bytes);
        if (g) {
            if (void* p = GlobalLock(g)) {
                std::memcpy(p, sel.c_str(), bytes);
                GlobalUnlock(g);
                if (!SetClipboardData(CF_UNICODETEXT, g)) GlobalFree(g);
            } else {
                GlobalFree(g);
            }
        }
        CloseClipboard();
        if (g && cut) {
            DeleteSelection(tf);
            AfterTextFieldEdit(tf);
        }
        return g != nullptr;
    }

    void PasteFromClipboard(Widget& tf) {
        if (!OpenClipboard(hwnd_)) return;
        HANDLE h = GetClipboardData(CF_UNICODETEXT);
        if (h) {
            if (const wchar_t* p =
                    static_cast<const wchar_t*>(GlobalLock(h))) {
                std::wstring ins(p);
                GlobalUnlock(h);
                DeleteSelection(tf);
                tf.text.insert(tf.caret, ins);
                SetCaret(tf, tf.caret + static_cast<int>(ins.size()), false);
                AfterTextFieldEdit(tf);
            }
        }
        CloseClipboard();
    }

    // Starts/stops the caret blink timer so it only runs while a text field
    // holds focus. kCaretTimerId is reserved; user timer ids start at 1.
    void EnsureCaretTimer() {
        const bool need = focused_ >= 0 &&
                          widgets_[focused_].kind == WidgetKind::TextField;
        if (need) {
            if (hwnd_) SetTimer(hwnd_, kCaretTimerId, 500, nullptr);
            caret_on_ = true;
        } else {
            if (hwnd_) KillTimer(hwnd_, kCaretTimerId);
            caret_on_ = false;
        }
    }

    // --- Scroll area internals ---------------------------------------------------

    // Bottom edge of the tallest child, in the area's local coordinates.
    int ScrollContentHeight(int areaId) const {
        int bottom = 0;
        for (const Widget& w : widgets_) {
            if (w.owner == areaId) bottom = std::max(bottom, w.y + w.h);
        }
        return bottom;
    }

    void ClampScrollOffset(Widget& sa, int areaId) {
        const int maxOff =
            std::max(0, ScrollContentHeight(areaId) - sa.h);
        sa.scrollOff = std::clamp(sa.scrollOff, 0, maxOff);
    }

    static bool InScrollbarColumn(const Widget& sa, int xDip) {
        return sa.w >= 16 && xDip >= sa.x + sa.w - 10;
    }

    static int ThumbHeight(const Widget& sa, int contentH) {
        if (contentH <= sa.h) return 0;
        const int th = static_cast<int>(
            std::lround(float(sa.h) * float(sa.h) / float(contentH)));
        return std::clamp(th, 20, sa.h);
    }

    static int ThumbTop(const Widget& sa, int contentH) {
        const int th = ThumbHeight(sa, contentH);
        if (th <= 0) return sa.y;
        const int maxOff = std::max(0, contentH - sa.h);
        const float frac =
            maxOff > 0 ? float(sa.scrollOff) / float(maxOff) : 0.0f;
        const int trackH = sa.h - 2 * kScrollPadDip;
        return sa.y + kScrollPadDip +
               static_cast<int>(std::lround(frac * float(trackH - th)));
    }

    // Updates the scroll offset while the scrollbar thumb is being dragged.
    void ApplyScrollDrag(Widget& sa, int yDip) {
        const int id = scroll_drag_;
        if (id < 0) return;
        const int contentH = ScrollContentHeight(id);
        const int maxOff = std::max(0, contentH - sa.h);
        const int trackH = sa.h - 2 * kScrollPadDip;
        const int th = ThumbHeight(sa, contentH);
        if (maxOff == 0 || trackH <= th) {
            sa.scrollOff = 0;
            Invalidate();
            return;
        }
        const int rel = yDip - scroll_drag_grab_ - sa.y - kScrollPadDip;
        const float frac =
            std::clamp(float(rel) / float(trackH - th), 0.0f, 1.0f);
        sa.scrollOff =
            static_cast<int>(std::lround(frac * float(maxOff)));
        Invalidate();
    }

    // Makes the focused widget fully visible inside its scroll area.
    void ScrollIntoView(int id) {
        if (id < 0 || !Valid(id)) return;
        const Widget& w = widgets_[id];
        if (w.owner < 0) return;
        Widget& sa = widgets_[w.owner];
        const int top = w.y;
        const int bottom = w.y + w.h;
        if (top - sa.scrollOff < 0) {
            sa.scrollOff = top;
        } else if (bottom - sa.scrollOff > sa.h) {
            sa.scrollOff = bottom - sa.h;
        }
        ClampScrollOffset(sa, w.owner);
        Invalidate();
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
    static constexpr int kCaptionDip = 24;    // borderless drag strip height
    static constexpr float kFieldPadDip = 8.0f;   // text field inner padding
    static constexpr float kFieldFontDip = 15.0f; // text field font size
    static constexpr int kScrollLineDip = 48;     // wheel step for scroll areas
    static constexpr int kScrollPadDip = 4;
    static constexpr int kCaretTimerId = 0;       // reserved blink timer id
    // Layout anchor bits (kept in sync with SAB_ANCHOR_* in sableui.h).
    static constexpr unsigned kAnchorLeft = 1, kAnchorTop = 2,
                              kAnchorRight = 4, kAnchorBottom = 8;
    static constexpr wchar_t kClassName[] = L"SableUIWindow";

    HWND hwnd_ = nullptr;
    D2DRenderer renderer_;
    Palette palette_;
    std::vector<Widget> widgets_;
    std::unordered_map<int, std::function<void()>> timers_;
    int next_timer_id_ = 1;

    std::function<void()> on_paint_;
    std::function<bool()> on_close_;
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
    int text_hit_ = -1;      // text field being drag-selected
    int scroll_drag_ = -1;   // scroll area whose thumb is being dragged
    int scroll_drag_grab_ = 0;  // y offset of the grab point inside the thumb
    bool caret_on_ = false;
};

} // namespace sableui

// Sableui - public C API.
// This header is the only contract exposed to consumers: C, C++ or any
// language with a C foreign function interface.

#ifndef SABLEUI_H
#define SABLEUI_H

#include <wchar.h>

#if defined(_WIN32) && defined(SABLEUI_BUILDING)
    #define SABLEUI_API __declspec(dllexport)
#elif defined(_WIN32)
    #define SABLEUI_API __declspec(dllimport)
#else
    #define SABLEUI_API
#endif

#if defined(_WIN32)
    #define SABLEUI_CALL __cdecl
#else
    #define SABLEUI_CALL
#endif

#ifdef __cplusplus
extern "C" {
#endif

// Opaque window handle. Created by sabCreateWindow, owned by the caller.
typedef struct sabWindowTag* sabWindow;

// Widget identifier. Non-negative on success, negative on failure.
typedef int sabWidgetId;

// Timer identifier. Same convention as sabWidgetId.
typedef int sabTimerId;

// --- Callback types ----------------------------------------------------------

// Click callback. Called from the UI thread while sabRun is active.
typedef void (SABLEUI_CALL* sabClickCallback)(void* userdata);

// Checkbox state change callback. checked: 1 = on, 0 = off.
typedef void (SABLEUI_CALL* sabChangeCallback)(void* userdata, int checked);

// Text field edit callback. text is the new field content (NUL-terminated);
// it stays valid only for the duration of the call.
typedef void (SABLEUI_CALL* sabTextChangeCallback)(void* userdata,
                                                   const wchar_t* text);

// Mouse button callback. button: 0 = left, 1 = right, 2 = middle.
typedef void (SABLEUI_CALL* sabMouseButtonCallback)(void* userdata, int button,
                                                    int x, int y);

// Mouse move callback (client coordinates).
typedef void (SABLEUI_CALL* sabMouseMoveCallback)(void* userdata, int x, int y);

// Mouse wheel callback. delta is a signed wheel increment (typically +-120).
typedef void (SABLEUI_CALL* sabWheelCallback)(void* userdata, int delta,
                                              int x, int y);

// Keyboard callback. key is a Win32 virtual-key code; mods is a bitmask:
// 1 = Ctrl, 2 = Shift, 4 = Alt.
typedef void (SABLEUI_CALL* sabKeyCallback)(void* userdata, int key, int mods);

// Text callback. codepoint is a character code from WM_CHAR.
typedef void (SABLEUI_CALL* sabTextCallback)(void* userdata, unsigned int codepoint);

// Resize callback. Reports the new client (inner) size in pixels.
typedef void (SABLEUI_CALL* sabResizeCallback)(void* userdata, int width, int height);

// Close request callback. Return non-zero to close the window, zero to veto
// the close (e.g. to show a "save changes?" dialog first).
typedef int (SABLEUI_CALL* sabCloseCallback)(void* userdata);

// Callback without arguments (paint, timers).
typedef void (SABLEUI_CALL* sabSimpleCallback)(void* userdata);

// --- Window lifecycle ----------------------------------------------------------

// Creates a top-level window. Returns NULL on failure; call sabGetLastError.
SABLEUI_API sabWindow sabCreateWindow(const wchar_t* title, int width, int height);

// Destroys the window and releases all associated resources.
SABLEUI_API void sabDestroyWindow(sabWindow window);

// Runs the message loop until the window is closed. Blocks the calling thread.
SABLEUI_API void sabRun(sabWindow window);

// Replaces the window title. Does nothing if window is NULL.
SABLEUI_API void sabSetTitle(sabWindow window, const wchar_t* title);

// Toggles the resize grip and maximize box. Default: enabled.
SABLEUI_API void sabSetResizable(sabWindow window, int on);

// Removes the title bar and resize frame. Default: disabled.
SABLEUI_API void sabSetBorderless(sabWindow window, int on);

// Constrains interactive resizing. Zero disables the constraint on that axis.
SABLEUI_API void sabSetMinimumSize(sabWindow window, int width, int height);

// Closes the window asynchronously (safe to call from any callback). The
// close-request hook can still veto it.
SABLEUI_API void sabCloseWindow(sabWindow window);

// Writes the current client (inner) size into width/height if non-NULL.
SABLEUI_API void sabGetClientSize(sabWindow window, int* width, int* height);

// Forces a repaint of the whole window.
SABLEUI_API void sabInvalidate(sabWindow window);

// --- Events ---------------------------------------------------------------------

// All event hooks accept NULL to disable the previous handler for that event.
// Handlers run on the UI thread while sabRun is active.

// Called when any mouse button is pressed. button: 0 = left, 1 = right, 2 = middle.
SABLEUI_API void sabOnMouseDown(sabWindow window, sabMouseButtonCallback cb, void* userdata);

// Called when any mouse button is released.
SABLEUI_API void sabOnMouseUp(sabWindow window, sabMouseButtonCallback cb, void* userdata);

// Called every time the cursor moves over the window.
SABLEUI_API void sabOnMouseMove(sabWindow window, sabMouseMoveCallback cb, void* userdata);

// Called on wheel scrolling.
SABLEUI_API void sabOnMouseWheel(sabWindow window, sabWheelCallback cb, void* userdata);

// Called when a key is pressed/released. Also fires for system keys (Alt).
SABLEUI_API void sabOnKeyDown(sabWindow window, sabKeyCallback cb, void* userdata);
SABLEUI_API void sabOnKeyUp(sabWindow window, sabKeyCallback cb, void* userdata);

// Called for each character typed (WM_CHAR level). Characters targeting a
// focused text field are also consumed by the field itself.
SABLEUI_API void sabOnText(sabWindow window, sabTextCallback cb, void* userdata);

// Called after the window size changed.
SABLEUI_API void sabOnResize(sabWindow window, sabResizeCallback cb, void* userdata);

// Called when the user requested closing (title bar X, Alt+F4). Return
// non-zero to allow the close, zero to veto it and keep the window open.
SABLEUI_API void sabOnClose(sabWindow window, sabCloseCallback cb, void* userdata);

// Custom painting. The callback runs after the background is cleared and
// before widgets are drawn. All sab* drawing functions below are valid ONLY
// inside this callback.
SABLEUI_API void sabOnPaint(sabWindow window, sabSimpleCallback cb, void* userdata);

// --- Timers ---------------------------------------------------------------------

// Starts a periodic timer firing every interval_ms. Returns a timer id, or
// -1 on failure. The callback runs on the UI thread while sabRun is active.
SABLEUI_API sabTimerId sabAddTimer(sabWindow window, int interval_ms,
                                   sabSimpleCallback cb, void* userdata);

// Stops and releases a previously created timer.
SABLEUI_API void sabRemoveTimer(sabWindow window, sabTimerId id);

// --- Drawing (paint callback only; colors are 0.0-1.0) ---------------------------

SABLEUI_API void sabSetFillColor(sabWindow window, float r, float g, float b, float a);
SABLEUI_API void sabSetStrokeColor(sabWindow window, float r, float g, float b, float a);
SABLEUI_API void sabSetStrokeWidth(sabWindow window, float width);

// Fills the whole client area with the current fill color.
SABLEUI_API void sabClear(sabWindow window);

SABLEUI_API void sabFillRect(sabWindow window, float x, float y, float w, float h);
SABLEUI_API void sabDrawRect(sabWindow window, float x, float y, float w, float h);
SABLEUI_API void sabFillRoundedRect(sabWindow window, float x, float y, float w, float h,
                                    float radius);
SABLEUI_API void sabFillEllipse(sabWindow window, float cx, float cy, float rx, float ry);
SABLEUI_API void sabDrawEllipse(sabWindow window, float cx, float cy, float rx, float ry);
SABLEUI_API void sabDrawLine(sabWindow window, float x1, float y1, float x2, float y2);

// Draws text in the given rect with the current fill color. font_size is in
// pixels (DIPs at 96 DPI).
SABLEUI_API void sabDrawText(sabWindow window, float x, float y, float w, float h,
                             const wchar_t* text, float font_size);

// --- Widgets ---------------------------------------------------------------------

// Adds a button. onClick may be NULL for a purely visual button.
// Returns the widget id, or -1 on failure.
SABLEUI_API sabWidgetId sabAddButton(sabWindow window, int x, int y, int w, int h,
                                     const wchar_t* text,
                                     sabClickCallback onClick, void* userdata);

// Adds a static text label. Returns the widget id, or -1 on failure.
SABLEUI_API sabWidgetId sabAddLabel(sabWindow window, int x, int y, int w, int h,
                                    const wchar_t* text);

// Adds a checkbox. onChange may be NULL; it fires on user clicks and on
// sabSetWidgetChecked. Returns the widget id, or -1 on failure.
SABLEUI_API sabWidgetId sabAddCheckbox(sabWindow window, int x, int y, int w, int h,
                                       const wchar_t* text, int checked,
                                       sabChangeCallback onChange, void* userdata);

// Adds a single-line text field. masked: 1 renders '*' instead of characters
// (passwords). onChange may be NULL; it fires after every text edit.
// Returns the widget id, or -1 on failure.
SABLEUI_API sabWidgetId sabAddTextField(sabWindow window, int x, int y, int w, int h,
                                        const wchar_t* initial, int masked,
                                        sabTextChangeCallback onChange,
                                        void* userdata);

// Replaces the text of an existing button, label or text field. For text
// fields the caret and selection are clamped to the new length.
// Returns 1 on success, 0 on failure.
SABLEUI_API int sabSetLabelText(sabWindow window, sabWidgetId id, const wchar_t* text);

// Copies the widget text (buttons, labels, text fields) into buffer.
// Returns the text length in characters on success. If the buffer is too
// small, nothing is written, the required length is returned and
// sabGetLastError is set.
SABLEUI_API int sabGetWidgetText(sabWindow window, sabWidgetId id,
                                 wchar_t* buffer, int buffer_chars);

// Sets checkbox state programmatically. Fires onChange if the state changed.
// Returns 1 on success, 0 on failure.
SABLEUI_API int sabSetWidgetChecked(sabWindow window, sabWidgetId id, int on);

// Returns 1 if the widget is a checked checkbox, 0 otherwise.
SABLEUI_API int sabIsWidgetChecked(sabWindow window, sabWidgetId id);

// --- Scroll areas ----------------------------------------------------------------

// Adds a vertical scroll area. Widgets added with the sabAdd*In functions
// below live inside it and use coordinates relative to the area's top-left
// corner; content taller than the area scrolls. Returns the widget id, or
// -1 on failure.
SABLEUI_API sabWidgetId sabAddScrollArea(sabWindow window, int x, int y, int w, int h);

SABLEUI_API sabWidgetId sabAddButtonIn(sabWindow window, sabWidgetId area,
                                       int x, int y, int w, int h,
                                       const wchar_t* text,
                                       sabClickCallback onClick, void* userdata);
SABLEUI_API sabWidgetId sabAddLabelIn(sabWindow window, sabWidgetId area,
                                      int x, int y, int w, int h,
                                      const wchar_t* text);
SABLEUI_API sabWidgetId sabAddCheckboxIn(sabWindow window, sabWidgetId area,
                                         int x, int y, int w, int h,
                                         const wchar_t* text, int checked,
                                         sabChangeCallback onChange, void* userdata);
SABLEUI_API sabWidgetId sabAddTextFieldIn(sabWindow window, sabWidgetId area,
                                          int x, int y, int w, int h,
                                          const wchar_t* initial, int masked,
                                          sabTextChangeCallback onChange,
                                          void* userdata);

// Programmatic scroll control. offset is in DIPs from the top of the
// content; it is clamped to the valid range. sabGetScrollOffset returns the
// current offset, or -1 on failure.
SABLEUI_API int sabSetScrollOffset(sabWindow window, sabWidgetId area, int offset);
SABLEUI_API int sabGetScrollOffset(sabWindow window, sabWidgetId area);

// --- Errors ---------------------------------------------------------------------

// Returns a description of the last error. The pointer stays valid until the
// next Sableui call. Never returns NULL.
SABLEUI_API const char* sabGetLastError(void);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // SABLEUI_H
