#pragma once

// Internal implementation core of Sableui. Not part of the public contract:
// consumers must only include <sableui/sableui.h>.
//
// All public coordinates and sizes are logical (DIP) units: 1 DIP = 1 px at
// 96 dpi. The renderer multiplies everything by the current monitor DPI, so
// the library is per-monitor DPI aware end to end.
//
// Link with: -ld2d1 -ldwrite -ldwmapi -lole32 -limm32 -lwindowscodecs

#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <windows.h>
#include <windowsx.h>
#include <objbase.h>
#include <d2d1.h>
#include <d2d1helper.h>
#include <dwrite.h>
#include <dwmapi.h>
#include <imm.h>
#include <wincodec.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <cwctype>
#include <functional>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#ifndef DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2
#define DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2 ((DPI_AWARENESS_CONTEXT)-4)
#endif

namespace sableui {

// Minimal ComPtr replacement for MinGW, which has no <wrl/client.h>.
template <class T>
class ComPtr {
public:
    ComPtr() = default;
    ~ComPtr() { Reset(); }

    ComPtr(const ComPtr&) = delete;
    ComPtr& operator=(const ComPtr&) = delete;

    ComPtr(ComPtr&& o) noexcept : p_(o.p_) { o.p_ = nullptr; }
    ComPtr& operator=(ComPtr&& o) noexcept {
        if (this != &o) { Reset(); p_ = o.p_; o.p_ = nullptr; }
        return *this;
    }

    T* Get() const { return p_; }
    T* operator->() const { return p_; }
    explicit operator bool() const { return p_ != nullptr; }

    void Attach(T* p) { Reset(); p_ = p; }

    T** ReleaseAndGetAddressOf() {
        Reset();
        return &p_;
    }

    void Reset() {
        if (p_) { p_->Release(); p_ = nullptr; }
    }

private:
    T* p_ = nullptr;
};

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

// Per-widget style overrides. Unset fields fall back to the Palette.
struct WidgetStyle {
    std::optional<Color> background;
    std::optional<Color> text;
    std::optional<Color> border;
    std::optional<float> radius;
};

enum class WidgetKind {
    Button, Label, Checkbox, TextField, ScrollArea, Slider, ProgressBar, Image
};

// Snapshot for text-field undo/redo. Full text plus caret position.
struct TextSnapshot {
    std::wstring text;
    int caret = 0;
};

struct Widget {
    WidgetKind kind;
    int x, y, w, h;               // DIPs
    std::wstring text;
    std::wstring fontFamily = L"Segoe UI";
    float fontSize = 15.0f;
    bool multiline = false;

    bool checked = false;
    bool pressed = false;
    bool enabled = true;
    bool visible = true;

    float minVal = 0.0f;
    float maxVal = 100.0f;
    float value  = 0.0f;

    WidgetStyle style;

    std::function<void()> onClick;
    std::function<void(bool)> onChange;
    std::function<void(float)> onValueChange;

    // Text field state.
    bool masked = false;
    int caret = 0;
    int selAnchor = 0;
    int selStart = 0;
    int selEnd = 0;
    std::function<void(const std::wstring&)> onTextChange;

    // Undo/redo history. Text field only.
    std::vector<TextSnapshot> undoStack;
    std::vector<TextSnapshot> redoStack;

    // Image state. imagePath is the source file; the renderer owns the
    // decoded bitmap and returns an opaque handle we cache here.
    std::wstring imagePath;
    void* imageHandle = nullptr;

    // Ownership: >= 0 when the widget lives inside a scroll area and its
    // coordinates are relative to that area.
    int owner = -1;

    // Vertical offset for scroll areas (and horizontal text offset for text
    // fields). scrollOffX is the horizontal offset for scroll areas.
    int scrollOff = 0;
    int scrollOffX = 0;

    // Layout anchors: keep margins to the client area across resizes.
    unsigned anchors = 0;
    int ml = 0, mt = 0, mr = 0, mb = 0;
};

// Abstract rendering surface. A future backend (Direct3D, software) can be
// added without touching widgets or the window class. All coordinates are
// DIPs; the backend is responsible for DPI scaling.
class Renderer {
public:
    virtual ~Renderer() = default;
    virtual bool Begin(HWND hwnd) = 0;
    virtual bool End() = 0;
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
                            TextAlign align, float font_size,
                            const std::wstring& font_family,
                            bool multiline) = 0;
    // Text metrics in DIPs. Both are required for text fields to work:
    // caret placement and click hit-testing rely on them.
    virtual float TextAdvance(const std::wstring& text, float font_size,
                              const std::wstring& font_family,
                              int index) = 0;
    virtual int TextIndexAtPos(const std::wstring& text, float font_size,
                               const std::wstring& font_family,
                               float x, float y) = 0;
    // Bitmap lifecycle. LoadBitmap decodes the file once and returns an
    // opaque handle; ReleaseBitmap drops a reference; DrawBitmap blits into
    // the given DIP rectangle with the given opacity.
    virtual void* LoadBitmap(const std::wstring& path) = 0;
    virtual void ReleaseBitmap(void* handle) = 0;
    virtual void DrawBitmap(void* handle, float x, float y, float w, float h,
                            float opacity) = 0;
    virtual void OnResize(unsigned width, unsigned height) = 0;
};

// Direct2D backend. DPI scaling is applied through the world transform, so
// stroke widths and font sizes scale together with geometry.
class D2DRenderer final : public Renderer {
public:
    D2DRenderer() = default;
    ~D2DRenderer() override {
        for (auto& kv : bitmapCache_) ReleaseBitmapEntry(kv.second);
        bitmapCache_.clear();
        d2dFromWic_.clear();
    }

    D2DRenderer(const D2DRenderer&) = delete;
    D2DRenderer& operator=(const D2DRenderer&) = delete;

    bool Begin(HWND hwnd) override {
        if (!EnsureTarget(hwnd)) return false;
        target_->SetTransform(
            D2D1::Matrix3x2F::Scale(D2D1::SizeF(scale_, scale_)));
        target_->BeginDraw();
        return true;
    }

    bool End() override {
        if (!target_) return true;
        const HRESULT hr = target_->EndDraw();
        if (hr == D2DERR_RECREATE_TARGET) {
            DiscardTarget();
            return false;
        }
        return SUCCEEDED(hr);
    }

    void SetScale(float scale) override {
        scale_ = scale > 0.01f ? scale : 0.01f;
    }

    void Clear(const Color& color) override {
        if (!target_) return;
        target_->Clear(ToD2D(color));
    }

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
                    TextAlign align, float font_size,
                    const std::wstring& font_family,
                    bool multiline) override {
        if (!target_ || !brush_ || text.empty()) return;
        IDWriteTextFormat* format = GetFormat(font_size, font_family, multiline);
        if (!format) return;
        format->SetTextAlignment(align == TextAlign::Center
                                     ? DWRITE_TEXT_ALIGNMENT_CENTER
                                     : DWRITE_TEXT_ALIGNMENT_LEADING);
        format->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
        brush_->SetColor(ToD2D(color));
        target_->DrawTextW(text.c_str(), static_cast<UINT32>(text.size()),
                           format, ToRectF(x, y, w, h), brush_.Get());
    }

    float TextAdvance(const std::wstring& text, float font_size,
                      const std::wstring& font_family,
                      int index) override {
        if (text.empty() || index <= 0) return 0.0f;
        IDWriteTextLayout* raw = CreateLayout(text, font_size, font_family, false);
        if (!raw) return 0.0f;
        ComPtr<IDWriteTextLayout> layout;
        layout.Attach(raw);
        const UINT32 pos = static_cast<UINT32>(
            std::min<int>(index, static_cast<int>(text.size())));
        FLOAT x = 0.0f, y = 0.0f;
        DWRITE_HIT_TEST_METRICS m{};
        if (FAILED(layout->HitTestTextPosition(pos, FALSE, &x, &y, &m)))
            return 0.0f;
        return x;
    }

    int TextIndexAtPos(const std::wstring& text, float font_size,
                       const std::wstring& font_family,
                       float x, float y) override {
        if (text.empty()) return 0;
        IDWriteTextLayout* raw = CreateLayout(text, font_size, font_family, false);
        if (!raw) return 0;
        ComPtr<IDWriteTextLayout> layout;
        layout.Attach(raw);
        BOOL trailing = FALSE, inside = FALSE;
        DWRITE_HIT_TEST_METRICS m{};
        if (FAILED(layout->HitTestPoint(x, y, &trailing, &inside, &m)))
            return 0;
        return std::clamp(static_cast<int>(m.textPosition) + (trailing ? 1 : 0),
                          0, static_cast<int>(text.size()));
    }

    // --- Bitmap support -----------------------------------------------------

    void* LoadBitmap(const std::wstring& path) override {
        if (path.empty()) return nullptr;

        auto it = bitmapCache_.find(path);
        if (it != bitmapCache_.end()) {
            ++it->second.refs;
            return it->second.handle;
        }

        if (!wic_) {
            if (FAILED(CoCreateInstance(
                    CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
                    IID_PPV_ARGS(wic_.ReleaseAndGetAddressOf())))) {
                return nullptr;
            }
        }

        ComPtr<IWICBitmapDecoder> decoder;
        if (FAILED(wic_->CreateDecoderFromFilename(
                path.c_str(), nullptr, GENERIC_READ,
                WICDecodeMetadataCacheOnDemand,
                decoder.ReleaseAndGetAddressOf()))) {
            return nullptr;
        }
        ComPtr<IWICBitmapFrameDecode> frame;
        if (FAILED(decoder->GetFrame(0, frame.ReleaseAndGetAddressOf())))
            return nullptr;
        ComPtr<IWICFormatConverter> conv;
        if (FAILED(wic_->CreateFormatConverter(conv.ReleaseAndGetAddressOf())))
            return nullptr;
        if (FAILED(conv->Initialize(frame.Get(), GUID_WICPixelFormat32bppPBGRA,
                                    WICBitmapDitherTypeNone, nullptr, 0.0,
                                    WICBitmapPaletteTypeMedianCut))) {
            return nullptr;
        }
        ComPtr<IWICBitmap> bmp;
        if (FAILED(wic_->CreateBitmapFromSource(
                conv.Get(), WICBitmapCacheOnDemand,
                bmp.ReleaseAndGetAddressOf()))) {
            return nullptr;
        }

        BitmapEntry entry;
        entry.bmp = std::move(bmp);
        entry.refs = 1;
        entry.handle = reinterpret_cast<void*>(entry.bmp.Get());
        void* handle = entry.handle;
        bitmapCache_.emplace(path, std::move(entry));
        return handle;
    }

    void ReleaseBitmap(void* handle) override {
        if (!handle) return;
        for (auto it = bitmapCache_.begin(); it != bitmapCache_.end(); ++it) {
            if (it->second.handle == handle) {
                if (--it->second.refs <= 0) {
                    d2dFromWic_.erase(handle);
                    ReleaseBitmapEntry(it->second);
                    bitmapCache_.erase(it);
                }
                return;
            }
        }
    }

    void DrawBitmap(void* handle, float x, float y, float w, float h,
                    float opacity) override {
        if (!handle || !target_) return;

        ID2D1Bitmap* d2dBmp = nullptr;
        auto it = d2dFromWic_.find(handle);
        if (it != d2dFromWic_.end()) {
            d2dBmp = it->second.Get();
        } else {
            IWICBitmap* wicBmp = nullptr;
            for (auto& kv : bitmapCache_) {
                if (kv.second.handle == handle) { wicBmp = kv.second.bmp.Get(); break; }
            }
            if (!wicBmp) return;
            ComPtr<ID2D1Bitmap> created;
            if (FAILED(target_->CreateBitmapFromWicBitmap(
                    wicBmp, nullptr, created.ReleaseAndGetAddressOf()))) {
                return;
            }
            auto ins = d2dFromWic_.emplace(handle, std::move(created));
            d2dBmp = ins.first->second.Get();
        }

        const D2D1_RECT_F dst = ToRectF(x, y, w, h);
        target_->DrawBitmap(d2dBmp, &dst, std::clamp(opacity, 0.0f, 1.0f),
                            D2D1_BITMAP_INTERPOLATION_MODE_LINEAR);
    }

    void OnResize(unsigned width, unsigned height) override {
        if (target_) {
            target_->Resize(
                D2D1::SizeU(std::max(1u, width), std::max(1u, height)));
        }
        d2dFromWic_.clear();
    }

private:
    struct BitmapEntry {
        ComPtr<IWICBitmap> bmp;
        int refs = 0;
        void* handle = nullptr;
    };

    static void ReleaseBitmapEntry(BitmapEntry& e) {
        e.bmp.Reset();
        e.handle = nullptr;
    }

    static D2D1_COLOR_F ToD2D(const Color& c) {
        return D2D1_COLOR_F{c.r, c.g, c.b, c.a};
    }
    static D2D1_RECT_F ToRectF(float x, float y, float w, float h) {
        return D2D1::RectF(x, y, x + w, y + h);
    }

    IDWriteTextFormat* GetFormat(float px, const std::wstring& family,
                                 bool multiline) {
        uint32_t key = 0;
        static_assert(sizeof(px) == sizeof(key), "unexpected float size");
        std::memcpy(&key, &px, sizeof(key));
        key ^= static_cast<uint32_t>(std::hash<std::wstring>{}(family));
        if (multiline) key ^= 0x80000000u;

        auto it = formats_.find(key);
        if (it != formats_.end()) return it->second.Get();

        IDWriteTextFormat* f = nullptr;
        if (FAILED(write_->CreateTextFormat(
                family.c_str(), nullptr, DWRITE_FONT_WEIGHT_NORMAL,
                DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL,
                px, L"en-US", &f))) {
            return nullptr;
        }
        f->SetWordWrapping(multiline ? DWRITE_WORD_WRAPPING_WRAP
                                     : DWRITE_WORD_WRAPPING_NO_WRAP);
        formats_[key].Attach(f);
        return f;
    }

    IDWriteTextLayout* CreateLayout(const std::wstring& text, float px,
                                    const std::wstring& family,
                                    bool multiline) {
        IDWriteTextFormat* fmt = GetFormat(px, family, multiline);
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
            if (FAILED(D2D1CreateFactory(
                    D2D1_FACTORY_TYPE_SINGLE_THREADED,
                    IID_ID2D1Factory, nullptr,
                    reinterpret_cast<void**>(
                        factory_.ReleaseAndGetAddressOf())))) {
                return false;
            }
        }
        if (!write_) {
            if (FAILED(DWriteCreateFactory(
                    DWRITE_FACTORY_TYPE_SHARED,
                    IID_IDWriteFactory,
                    reinterpret_cast<IUnknown**>(
                        write_.ReleaseAndGetAddressOf())))) {
                return false;
            }
        }

        if (FAILED(factory_->CreateHwndRenderTarget(
                D2D1::RenderTargetProperties(),
                D2D1::HwndRenderTargetProperties(hwnd, D2D1::SizeU(w, h)),
                target_.ReleaseAndGetAddressOf()))) {
            return false;
        }

        if (FAILED(target_->CreateSolidColorBrush(
                D2D1_COLOR_F{0, 0, 0, 1},
                brush_.ReleaseAndGetAddressOf()))) {
            target_.Reset();
            return false;
        }
        return true;
    }

    void DiscardTarget() {
        formats_.clear();
        d2dFromWic_.clear();
        brush_.Reset();
        target_.Reset();
    }

    ComPtr<ID2D1Factory> factory_;
    ComPtr<ID2D1HwndRenderTarget> target_;
    ComPtr<ID2D1SolidColorBrush> brush_;
    ComPtr<IDWriteFactory> write_;
    ComPtr<IWICImagingFactory> wic_;
    std::unordered_map<uint32_t, ComPtr<IDWriteTextFormat>> formats_;
    std::unordered_map<std::wstring, BitmapEntry> bitmapCache_;
    std::unordered_map<void*, ComPtr<ID2D1Bitmap>> d2dFromWic_;
    float scale_ = 1.0f;
};

// Win32 window with its own widget tree and message loop. Owns a D2DRenderer,
// translates Win32 messages into widget events and paints the whole tree on
// every WM_PAINT.
class Window {
public:
    Window() = default;
    ~Window() {
        for (Widget& w : widgets_) {
            if (w.imageHandle) {
                renderer_.ReleaseBitmap(w.imageHandle);
                w.imageHandle = nullptr;
            }
        }
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

    // --- Widget adders (top-level) ------------------------------------------

    int AddButton(int x, int y, int w, int h, const std::wstring& text,
                  std::function<void()> onClick) {
        Widget wf{}; wf.kind = WidgetKind::Button;
        wf.x = x; wf.y = y; wf.w = w; wf.h = h; wf.text = text;
        wf.onClick = std::move(onClick);
        widgets_.push_back(std::move(wf));
        Invalidate();
        return static_cast<int>(widgets_.size()) - 1;
    }

    int AddLabel(int x, int y, int w, int h, const std::wstring& text) {
        Widget wf{}; wf.kind = WidgetKind::Label;
        wf.x = x; wf.y = y; wf.w = w; wf.h = h; wf.text = text;
        widgets_.push_back(std::move(wf));
        Invalidate();
        return static_cast<int>(widgets_.size()) - 1;
    }

    int AddCheckbox(int x, int y, int w, int h, const std::wstring& text,
                    bool checked, std::function<void(bool)> onChange) {
        Widget wf{}; wf.kind = WidgetKind::Checkbox;
        wf.x = x; wf.y = y; wf.w = w; wf.h = h; wf.text = text;
        wf.checked = checked; wf.onChange = std::move(onChange);
        widgets_.push_back(std::move(wf));
        Invalidate();
        return static_cast<int>(widgets_.size()) - 1;
    }

    int AddTextField(int x, int y, int w, int h, const std::wstring& text,
                     bool masked, bool multiline,
                     std::function<void(const std::wstring&)> onTextChange) {
        Widget wf{}; wf.kind = WidgetKind::TextField;
        wf.x = x; wf.y = y; wf.w = w; wf.h = h; wf.text = text;
        wf.masked = masked; wf.multiline = multiline;
        wf.onTextChange = std::move(onTextChange);
        widgets_.push_back(std::move(wf));
        Invalidate();
        return static_cast<int>(widgets_.size()) - 1;
    }

    int AddTextArea(int x, int y, int w, int h, const std::wstring& text,
                    std::function<void(const std::wstring&)> onTextChange) {
        return AddTextField(x, y, w, h, text, false, true,
                            std::move(onTextChange));
    }

    int AddSlider(int x, int y, int w, int h, float minV, float maxV,
                  float val, std::function<void(float)> onValueChange) {
        Widget wf{}; wf.kind = WidgetKind::Slider;
        wf.x = x; wf.y = y; wf.w = w; wf.h = h;
        wf.minVal = minV; wf.maxVal = maxV; wf.value = val;
        wf.onValueChange = std::move(onValueChange);
        widgets_.push_back(std::move(wf));
        Invalidate();
        return static_cast<int>(widgets_.size()) - 1;
    }

    int AddProgressBar(int x, int y, int w, int h, float val) {
        Widget wf{}; wf.kind = WidgetKind::ProgressBar;
        wf.x = x; wf.y = y; wf.w = w; wf.h = h; wf.value = val;
        widgets_.push_back(std::move(wf));
        Invalidate();
        return static_cast<int>(widgets_.size()) - 1;
    }

    int AddScrollArea(int x, int y, int w, int h) {
        Widget wf{}; wf.kind = WidgetKind::ScrollArea;
        wf.x = x; wf.y = y; wf.w = w; wf.h = h;
        widgets_.push_back(std::move(wf));
        Invalidate();
        return static_cast<int>(widgets_.size()) - 1;
    }

    int AddImage(int x, int y, int w, int h, const std::wstring& path) {
        Widget wf{}; wf.kind = WidgetKind::Image;
        wf.x = x; wf.y = y; wf.w = w; wf.h = h;
        wf.imagePath = path;
        wf.imageHandle = renderer_.LoadBitmap(path);
        widgets_.push_back(std::move(wf));
        Invalidate();
        return static_cast<int>(widgets_.size()) - 1;
    }

    // --- Widget adders (inside a scroll area) --------------------------------

    int AddButtonIn(int area, int x, int y, int bw, int bh,
                    const std::wstring& text, std::function<void()> onClick) {
        if (!ValidArea(area)) return -1;
        Widget w{}; w.kind = WidgetKind::Button;
        w.x = x; w.y = y; w.w = bw; w.h = bh; w.text = text;
        w.onClick = std::move(onClick); w.owner = area;
        widgets_.push_back(std::move(w));
        Invalidate();
        return static_cast<int>(widgets_.size()) - 1;
    }

    int AddLabelIn(int area, int x, int y, int bw, int bh,
                   const std::wstring& text) {
        if (!ValidArea(area)) return -1;
        Widget w{}; w.kind = WidgetKind::Label;
        w.x = x; w.y = y; w.w = bw; w.h = bh; w.text = text;
        w.owner = area;
        widgets_.push_back(std::move(w));
        Invalidate();
        return static_cast<int>(widgets_.size()) - 1;
    }

    int AddCheckboxIn(int area, int x, int y, int bw, int bh,
                      const std::wstring& text, bool checked,
                      std::function<void(bool)> onChange) {
        if (!ValidArea(area)) return -1;
        Widget w{}; w.kind = WidgetKind::Checkbox;
        w.x = x; w.y = y; w.w = bw; w.h = bh; w.text = text;
        w.checked = checked; w.onChange = std::move(onChange); w.owner = area;
        widgets_.push_back(std::move(w));
        Invalidate();
        return static_cast<int>(widgets_.size()) - 1;
    }

    int AddTextFieldIn(int area, int x, int y, int bw, int bh,
                       const std::wstring& text, bool masked, bool multiline,
                       std::function<void(const std::wstring&)> onTextChange) {
        if (!ValidArea(area)) return -1;
        Widget w{}; w.kind = WidgetKind::TextField;
        w.x = x; w.y = y; w.w = bw; w.h = bh; w.text = text;
        w.masked = masked; w.multiline = multiline;
        w.onTextChange = std::move(onTextChange); w.owner = area;
        widgets_.push_back(std::move(w));
        Invalidate();
        return static_cast<int>(widgets_.size()) - 1;
    }

    int AddTextAreaIn(int area, int x, int y, int bw, int bh,
                      const std::wstring& text,
                      std::function<void(const std::wstring&)> onTextChange) {
        return AddTextFieldIn(area, x, y, bw, bh, text, false, true,
                              std::move(onTextChange));
    }

    int AddImageIn(int area, int x, int y, int bw, int bh,
                   const std::wstring& path) {
        if (!ValidArea(area)) return -1;
        Widget w{}; w.kind = WidgetKind::Image;
        w.x = x; w.y = y; w.w = bw; w.h = bh; w.imagePath = path;
        w.imageHandle = renderer_.LoadBitmap(path);
        w.owner = area;
        widgets_.push_back(std::move(w));
        Invalidate();
        return static_cast<int>(widgets_.size()) - 1;
    }

    // --- Setters ------------------------------------------------------------

    bool SetWidgetText(int id, const std::wstring& text) {
        if (!Valid(id)) return false;
        widgets_[id].text = text;
        Invalidate();
        return true;
    }

    bool SetLabelText(int id, const std::wstring& text) {
        return SetWidgetText(id, text);
    }

    bool SetWidgetFont(int id, const std::wstring& family, float size) {
        if (!Valid(id)) return false;
        widgets_[id].fontFamily = family;
        widgets_[id].fontSize = size;
        Invalidate();
        return true;
    }

    // Reads back the current font of a widget. Both out-params are optional.
    bool GetWidgetFont(int id, std::wstring* family, float* size) const {
        if (!Valid(id)) return false;
        if (family) *family = widgets_[id].fontFamily;
        if (size)   *size   = widgets_[id].fontSize;
        return true;
    }

    bool SetWidgetStyle(int id, const WidgetStyle& style) {
        if (!Valid(id)) return false;
        widgets_[id].style = style;
        Invalidate();
        return true;
    }

    bool SetWidgetBackground(int id, const Color& c) {
        if (!Valid(id)) return false;
        widgets_[id].style.background = c;
        Invalidate();
        return true;
    }

    // Reverts the per-widget background override to the palette default.
    bool ClearWidgetBackground(int id) {
        if (!Valid(id)) return false;
        widgets_[id].style.background.reset();
        Invalidate();
        return true;
    }

    bool SetWidgetRadius(int id, float r) {
        if (!Valid(id)) return false;
        widgets_[id].style.radius = r;
        Invalidate();
        return true;
    }

    // Reverts the per-widget radius override to the palette default.
    bool ClearWidgetRadius(int id) {
        if (!Valid(id)) return false;
        widgets_[id].style.radius.reset();
        Invalidate();
        return true;
    }

    bool SetWidgetValue(int id, float value) {
        if (!Valid(id)) return false;
        widgets_[id].value = value;
        Invalidate();
        return true;
    }

    bool Undo() {
        if (focused_ < 0 || widgets_[focused_].kind != WidgetKind::TextField)
            return false;
        DoUndo(widgets_[focused_]);
        return true;
    }

    bool Redo() {
        if (focused_ < 0 || widgets_[focused_].kind != WidgetKind::TextField)
            return false;
        DoRedo(widgets_[focused_]);
        return true;
    }

    bool SetWidgetChecked(int id, bool on) {
        if (!Valid(id) || widgets_[id].kind != WidgetKind::Checkbox) return false;
        widgets_[id].checked = on;
        Invalidate();
        return true;
    }

    bool SetWidgetEnabled(int id, bool on) {
        if (!Valid(id)) return false;
        widgets_[id].enabled = on;
        Invalidate();
        return true;
    }

    bool SetWidgetVisible(int id, bool on) {
        if (!Valid(id)) return false;
        widgets_[id].visible = on;
        Invalidate();
        return true;
    }

    bool SetWidgetImage(int id, const std::wstring& path) {
        if (!Valid(id) || widgets_[id].kind != WidgetKind::Image) return false;
        Widget& w = widgets_[id];
        if (w.imageHandle) {
            renderer_.ReleaseBitmap(w.imageHandle);
            w.imageHandle = nullptr;
        }
        w.imagePath = path;
        w.imageHandle = renderer_.LoadBitmap(path);
        Invalidate();
        return true;
    }

    bool SetWidgetAnchors(int id, unsigned anchors) {
        if (!Valid(id)) return false;
        int cw = 0, ch = 0; ClientSize(&cw, &ch);
        Widget& w = widgets_[id];
        w.anchors = anchors;
        w.ml = w.x; w.mt = w.y;
        w.mr = cw - (w.x + w.w); w.mb = ch - (w.y + w.h);
        Invalidate();
        return true;
    }

    bool SetScrollOffset(int area, int offset) {
        if (!ValidArea(area)) return false;
        widgets_[area].scrollOff = offset;
        ClampScroll(area);
        Invalidate();
        return true;
    }

    bool SetScrollOffsetX(int area, int offset) {
        if (!ValidArea(area)) return false;
        widgets_[area].scrollOffX = offset;
        ClampScroll(area);
        Invalidate();
        return true;
    }

    // --- Getters ------------------------------------------------------------

    std::wstring GetWidgetText(int id) const {
        if (!Valid(id)) return L"";
        return widgets_[id].text;
    }

    int GetScrollOffset(int area) const {
        if (!ValidArea(area)) return 0;
        return widgets_[area].scrollOff;
    }

    int GetScrollOffsetX(int area) const {
        if (!ValidArea(area)) return 0;
        return widgets_[area].scrollOffX;
    }

    bool IsChecked(int id) const {
        return Valid(id) && widgets_[id].checked;
    }

    bool RemoveWidget(int id) {
        if (!Valid(id)) return false;
        if (widgets_[id].imageHandle)
            renderer_.ReleaseBitmap(widgets_[id].imageHandle);
        widgets_.erase(widgets_.begin() + id);
        for (Widget& w : widgets_) {
            if (w.owner > id) --w.owner;
            else if (w.owner == id) w.owner = -1;
        }
        if (focused_ > id) --focused_;
        else if (focused_ == id) focused_ = -1;
        if (pressed_ > id) --pressed_;
        else if (pressed_ == id) pressed_ = -1;
        if (hover_ > id) --hover_;
        else if (hover_ == id) hover_ = -1;
        Invalidate();
        return true;
    }

    bool MoveWidgetToFront(int id) {
        if (!Valid(id)) return false;
        Widget w = widgets_[id];
        widgets_.erase(widgets_.begin() + id);
        widgets_.push_back(std::move(w));
        Invalidate();
        return true;
    }

    // --- Window state -------------------------------------------------------

    void SetTitle(const std::wstring& title) {
        if (hwnd_) SetWindowTextW(hwnd_, title.c_str());
    }

    void SetResizable(bool on) {
        if (!hwnd_) return;
        LONG_PTR style = GetWindowLongPtrW(hwnd_, GWL_STYLE);
        style &= ~(WS_THICKFRAME | WS_MAXIMIZEBOX | WS_MINIMIZEBOX);
        if (on) style |= WS_THICKFRAME | WS_MAXIMIZEBOX | WS_MINIMIZEBOX;
        SetWindowLongPtrW(hwnd_, GWL_STYLE, style);
        SetWindowPos(hwnd_, nullptr, 0, 0, 0, 0,
                     SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
    }

    void SetBorderless(bool on) {
        if (!hwnd_) return;
        borderless_ = on;
        LONG_PTR style = GetWindowLongPtrW(hwnd_, GWL_STYLE);
        style &= ~(WS_CAPTION | WS_THICKFRAME | WS_MAXIMIZEBOX | WS_MINIMIZEBOX);
        if (!on) style |= WS_CAPTION;
        SetWindowLongPtrW(hwnd_, GWL_STYLE, style);
        SetWindowPos(hwnd_, nullptr, 0, 0, 0, 0,
                     SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
    }

    void SetMinimumSize(int w, int h) {
        min_w_ = w; min_h_ = h;
    }

    void CloseRequest() {
        if (hwnd_) PostMessageW(hwnd_, WM_CLOSE, 0, 0);
    }

    void ClientSize(int* w, int* h) const {
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

    // --- Event hooks --------------------------------------------------------

    void OnMouseDown(std::function<void(int,int,int)> f) { on_mouse_down_ = std::move(f); }
    void OnMouseUp(std::function<void(int,int,int)> f)   { on_mouse_up_ = std::move(f); }
    void OnMouseMove(std::function<void(int,int)> f)     { on_mouse_move_ = std::move(f); }
    void OnMouseWheel(std::function<void(int,int,int)> f){ on_mouse_wheel_ = std::move(f); }
    void OnKeyDown(std::function<void(int,int)> f)       { on_key_down_ = std::move(f); }
    void OnKeyUp(std::function<void(int,int)> f)         { on_key_up_ = std::move(f); }
    void OnText(std::function<void(unsigned int)> f)     { on_text_ = std::move(f); }
    void OnResize(std::function<void(int,int)> f)        { on_resize_ = std::move(f); }
    void OnClose(std::function<bool()> f)                { on_close_ = std::move(f); }
    void OnPaint(std::function<void()> f)                { on_paint_ = std::move(f); }

    // --- Timers -------------------------------------------------------------

    int AddTimer(int interval_ms, std::function<void()> fn, std::string* error) {
        if (error) error->clear();
        if (!hwnd_) { if (error) *error = "AddTimer: no window"; return -1; }
        if (interval_ms <= 0 || !fn) { if (error) *error = "AddTimer: bad args"; return -1; }
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

    // --- Paint-time drawing -------------------------------------------------

    void SetFillColor(const Color& c)   { fill_ = c; }
    void SetStrokeColor(const Color& c) { stroke_ = c; }
    void SetTextColor(const Color& c)   { text_ = c; }
    void SetStrokeWidth(float w)        { stroke_width_ = w > 0 ? w : 1.0f; }

    void PushClipRect(float x, float y, float w, float h) {
        if (painting_) renderer_.PushClipRect(x, y, w, h);
    }
    void PopClipRect() { if (painting_) renderer_.PopClipRect(); }
    void ClearWithFill() { if (painting_) renderer_.Clear(fill_); }
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
        if (painting_)
            renderer_.DrawTextIn(x, y, w, h, text, text_, TextAlign::Left,
                                 font_size, L"Segoe UI", false);
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

    void Destroy() { if (hwnd_) DestroyWindow(hwnd_); }
    float DpiScale() const { return Scale(); }

private:
    // --- Utilities ---------------------------------------------------------

    template <class F, class... A>
    static void SafeCall(const F& f, A... args) {
        if (!f) return;
        try {
            f(args...);
        } catch (...) {
        }
    }

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

    bool Valid(int id) const {
        return id >= 0 && id < static_cast<int>(widgets_.size());
    }

    bool ValidArea(int id) const {
        return Valid(id) && widgets_[id].kind == WidgetKind::ScrollArea;
    }

    // --- DPI ---------------------------------------------------------------

    void UpdateDpi() {
        if (!hwnd_) return;
        const UINT dpi = GetDpiForWindowSafe();
        dpi_ = dpi > 0 ? static_cast<float>(dpi) : 96.0f;
        renderer_.SetScale(Scale());
    }

    UINT GetDpiForWindowSafe() const {
        using Fn = UINT(WINAPI*)(HWND);
        if (auto fn = LoadProcAddress<Fn>(L"user32.dll", "GetDpiForWindow"))
            return fn(hwnd_);
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
        if (FAILED(DwmSetWindowAttribute(hwnd_, 20, &dark, sizeof(dark))))
            DwmSetWindowAttribute(hwnd_, 19, &dark, sizeof(dark));
    }

    // --- Window class ------------------------------------------------------

    static bool RegisterClassOnce() {
        static bool registered = false;
        if (registered) return true;
        registered = true;

        WNDCLASSEXW wc{};
        wc.cbSize = sizeof(wc);
        wc.lpfnWndProc = &Window::WndProcThunk;
        wc.hInstance = GetModuleHandleW(nullptr);
        wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        wc.style = CS_DBLCLKS | CS_HREDRAW | CS_VREDRAW;
        wc.lpszClassName = kClassName;
        return RegisterClassExW(&wc) != 0;
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
        if (GetKeyState(VK_SHIFT) & 0x8000) mods |= kModShift;
        if (GetKeyState(VK_MENU) & 0x8000) mods |= kModAlt;
        return mods;
    }

    // --- Rendering ---------------------------------------------------------

    Color BackgroundFor(const Widget& w) const {
        if (w.owner >= 0 && Valid(w.owner) &&
            widgets_[w.owner].kind == WidgetKind::ScrollArea) {
            return palette_.fieldBg;
        }
        return palette_.background;
    }

    static Color DimColor(const Color& c, const Color& bg) {
        constexpr float k = 0.45f;
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
            Widget& w = widgets_[i];
            if (!w.visible) continue;
            if (w.owner >= 0) continue;
            if (w.kind == WidgetKind::ScrollArea) {
                DrawScrollArea(w, static_cast<int>(i));
            } else {
                DrawWidget(w, static_cast<int>(i), 0, 0);
            }
        }

        if (!renderer_.End()) Invalidate();
    }

    void DrawScrollArea(Widget& sa, int id) {
        const float fx = float(sa.x), fy = float(sa.y);
        renderer_.FillRoundedRect(fx, fy, float(sa.w), float(sa.h), 6.0f,
                                  palette_.fieldBg);
        renderer_.PushClipRect(fx + 1.0f, fy + 1.0f, float(sa.w) - 2.0f,
                               float(sa.h) - 2.0f);
        for (std::size_t i = 0; i < widgets_.size(); ++i) {
            Widget& c = widgets_[i];
            if (c.owner != id || !c.visible) continue;
            DrawWidget(c, static_cast<int>(i), sa.x - sa.scrollOffX,
                       sa.y - sa.scrollOff);
        }
        renderer_.PopClipRect();
    }

    void DrawWidget(Widget& w, int id, int ox, int oy) {
        const float fx = float(w.x + ox);
        const float fy = float(w.y + oy);
        const Color bg = BackgroundFor(w);
        const float radius = w.style.radius.value_or(8.0f);

        if (w.kind == WidgetKind::Button) {
            Color fill = w.style.background.value_or(
                w.enabled ? (w.pressed && id == hover_
                                ? palette_.accentPressed
                                : id == hover_ ? palette_.accentHover
                                               : palette_.accent)
                          : DimColor(palette_.accent, bg));
            renderer_.FillRoundedRect(fx, fy, float(w.w), float(w.h), radius, fill);
            renderer_.DrawTextIn(fx, fy, float(w.w), float(w.h), w.text,
                                 w.style.text.value_or(
                                     w.enabled ? palette_.buttonText
                                               : DimColor(palette_.buttonText, bg)),
                                 TextAlign::Center, w.fontSize, w.fontFamily,
                                 false);
        } else if (w.kind == WidgetKind::Label) {
            renderer_.DrawTextIn(fx, fy, float(w.w), float(w.h), w.text,
                                 w.style.text.value_or(
                                     w.enabled ? palette_.labelText
                                               : DimColor(palette_.labelText, bg)),
                                 TextAlign::Left, w.fontSize, w.fontFamily,
                                 w.multiline);
        } else if (w.kind == WidgetKind::Checkbox) {
            constexpr float box = 16.0f;
            const float cy = fy + (float(w.h) - box) * 0.5f;
            renderer_.FillRoundedRect(fx, cy, box, box, 3.0f,
                                      w.checked ? palette_.accent
                                                : Color{0.14f, 0.15f, 0.17f, 1.0f});
            if (w.checked) {
                renderer_.DrawLine(fx + 4.0f, cy + 8.0f, fx + 7.0f,
                                   cy + 11.0f, palette_.buttonText, 2.0f);
                renderer_.DrawLine(fx + 7.0f, cy + 11.0f, fx + 12.0f,
                                   cy + 5.0f, palette_.buttonText, 2.0f);
            }
            renderer_.DrawTextIn(fx + box + 8.0f, fy, float(w.w) - box - 8.0f,
                                 float(w.h), w.text, palette_.labelText,
                                 TextAlign::Left, w.fontSize, w.fontFamily, false);
        } else if (w.kind == WidgetKind::TextField) {
            constexpr float pad = 8.0f;
            renderer_.FillRoundedRect(fx, fy, float(w.w), float(w.h), 6.0f,
                                      palette_.fieldBg);
            renderer_.DrawRoundedRect(fx, fy, float(w.w), float(w.h), 6.0f,
                                      focused_ == id ? palette_.accent
                                                     : palette_.fieldBorder,
                                      1.0f);
            const std::wstring disp = DisplayText(w);
            const float caretX = renderer_.TextAdvance(disp, w.fontSize,
                                                      w.fontFamily, w.caret);
            renderer_.PushClipRect(fx + 1.0f, fy + 1.0f, float(w.w) - 2.0f,
                                   float(w.h) - 2.0f);
            renderer_.DrawTextIn(fx + pad, fy, float(w.w) - 2.0f * pad,
                                 float(w.h), disp, palette_.labelText,
                                 TextAlign::Left, w.fontSize, w.fontFamily,
                                 w.multiline);
            if (focused_ == id && caret_on_) {
                const float cx = fx + pad + caretX;
                renderer_.DrawLine(cx, fy + 4.0f, cx, fy + float(w.h) - 4.0f,
                                   palette_.labelText, 1.5f);
            }
            renderer_.PopClipRect();
        } else if (w.kind == WidgetKind::Slider) {
            const float cy = fy + float(w.h) * 0.5f;
            const float frac = (w.value - w.minVal) / (w.maxVal - w.minVal);
            renderer_.FillRoundedRect(fx, cy - 2.0f, float(w.w), 4.0f, 2.0f,
                                      palette_.fieldBg);
            renderer_.FillRoundedRect(fx, cy - 2.0f, float(w.w) * frac, 4.0f,
                                      2.0f, palette_.accent);
            const float knobX = fx + float(w.w) * frac;
            renderer_.FillEllipse(knobX, cy, 8.0f, 8.0f, palette_.accentHover);
        } else if (w.kind == WidgetKind::ProgressBar) {
            const float frac = (w.value - w.minVal) / (w.maxVal - w.minVal);
            renderer_.FillRoundedRect(fx, fy, float(w.w), float(w.h), 4.0f,
                                      palette_.fieldBg);
            renderer_.FillRoundedRect(fx, fy, float(w.w) * frac, float(w.h),
                                      4.0f, palette_.accent);
        } else if (w.kind == WidgetKind::Image) {
            if (w.imageHandle) {
                renderer_.DrawBitmap(w.imageHandle, fx, fy, float(w.w),
                                     float(w.h), 1.0f);
            } else if (!w.imagePath.empty()) {
                w.imageHandle = renderer_.LoadBitmap(w.imagePath);
                if (w.imageHandle) {
                    renderer_.DrawBitmap(w.imageHandle, fx, fy, float(w.w),
                                         float(w.h), 1.0f);
                }
            }
        }
    }

    std::wstring DisplayText(const Widget& tf) const {
        if (!tf.masked) return tf.text;
        return std::wstring(tf.text.size(), L'*');
    }

    // --- Text input helpers ------------------------------------------------

    int TextFieldCaretAt(int id, int xDip, int yDip) {
        const Widget& tf = widgets_[id];
        const std::wstring disp = DisplayText(tf);
        return renderer_.TextIndexAtPos(disp, tf.fontSize, tf.fontFamily,
                                        float(xDip - tf.x) - 8.0f,
                                        float(yDip - tf.y));
    }

    static void SetCaret(Widget& tf, int pos, bool extend) {
        tf.caret = std::clamp(pos, 0, static_cast<int>(tf.text.size()));
        if (extend) {
            tf.selStart = std::min(tf.selAnchor, tf.caret);
            tf.selEnd = std::max(tf.selAnchor, tf.caret);
        } else {
            tf.selAnchor = tf.caret;
            tf.selStart = tf.caret;
            tf.selEnd = tf.caret;
        }
    }

    void AfterEdit(Widget& tf) {
        ClampIndices(tf);
        caret_on_ = true;
        SafeCall(tf.onTextChange, tf.text);
        Invalidate();
    }

    static void ClampIndices(Widget& tf) {
        const int n = static_cast<int>(tf.text.size());
        tf.caret = std::clamp(tf.caret, 0, n);
        tf.selAnchor = std::clamp(tf.selAnchor, 0, n);
        tf.selStart = std::clamp(tf.selStart, 0, n);
        tf.selEnd = std::clamp(tf.selEnd, 0, n);
    }

    // --- Undo / redo -------------------------------------------------------

    static constexpr std::size_t kMaxUndo = 200;

    static void PushUndo(Widget& tf) {
        TextSnapshot s;
        s.text = tf.text;
        s.caret = tf.caret;
        tf.undoStack.push_back(std::move(s));
        if (tf.undoStack.size() > kMaxUndo)
            tf.undoStack.erase(tf.undoStack.begin());
        tf.redoStack.clear();
    }

    void DoUndo(Widget& tf) {
        if (tf.undoStack.empty()) return;
        TextSnapshot cur;
        cur.text = tf.text;
        cur.caret = tf.caret;
        tf.redoStack.push_back(std::move(cur));

        TextSnapshot s = std::move(tf.undoStack.back());
        tf.undoStack.pop_back();
        tf.text = std::move(s.text);
        SetCaret(tf, s.caret, false);
        AfterEdit(tf);
    }

    void DoRedo(Widget& tf) {
        if (tf.redoStack.empty()) return;
        TextSnapshot cur;
        cur.text = tf.text;
        cur.caret = tf.caret;
        tf.undoStack.push_back(std::move(cur));

        TextSnapshot s = std::move(tf.redoStack.back());
        tf.redoStack.pop_back();
        tf.text = std::move(s.text);
        SetCaret(tf, s.caret, false);
        AfterEdit(tf);
    }

    // --- IME ---------------------------------------------------------------

    void BeginImeComposition(Widget& tf) {
        if (!hwnd_) return;
        HIMC imc = ImmGetContext(hwnd_);
        if (!imc) return;
        COMPOSITIONFORM cf{};
        cf.dwStyle = CFS_POINT;
        POINT pt{ tf.x + 8, tf.y + tf.h };
        ClientToScreen(hwnd_, &pt);
        cf.ptCurrentPos = pt;
        ImmSetCompositionWindow(imc, &cf);
        ImmReleaseContext(hwnd_, imc);
    }

    void UpdateImeComposition(Widget& tf, LPARAM lp) {
        if (!hwnd_) return;
        HIMC imc = ImmGetContext(hwnd_);
        if (!imc) return;
        if (lp & GCS_RESULTSTR) {
            LONG bytes = ImmGetCompositionStringW(imc, GCS_RESULTSTR, nullptr, 0);
            if (bytes > 0) {
                std::wstring ins(static_cast<size_t>(bytes) / sizeof(wchar_t),
                                 L'\0');
                ImmGetCompositionStringW(imc, GCS_RESULTSTR, ins.data(), bytes);
                PushUndo(tf);
                tf.text.insert(tf.caret, ins);
                SetCaret(tf, tf.caret + static_cast<int>(ins.size()), false);
                AfterEdit(tf);
            }
        }
        ImmReleaseContext(hwnd_, imc);
    }

    // --- Hit test ----------------------------------------------------------

    int HitTest(int x, int y) const {
        for (int i = static_cast<int>(widgets_.size()) - 1; i >= 0; --i) {
            const Widget& w = widgets_[i];
            if (!w.visible || !w.enabled) continue;
            if (w.kind == WidgetKind::Label) continue;
            if (w.kind == WidgetKind::ScrollArea) continue;
            int wx = w.x, wy = w.y;
            if (w.owner >= 0) {
                const Widget& o = widgets_[w.owner];
                if (x < o.x || x >= o.x + o.w) continue;
                if (y < o.y || y >= o.y + o.h) continue;
                wx = o.x + w.x - o.scrollOffX;
                wy = o.y + w.y - o.scrollOff;
            }
            if (x >= wx && x < wx + w.w && y >= wy && y < wy + w.h) return i;
        }
        for (int i = static_cast<int>(widgets_.size()) - 1; i >= 0; --i) {
            const Widget& w = widgets_[i];
            if (w.kind != WidgetKind::ScrollArea) continue;
            if (!w.visible || !w.enabled) continue;
            if (x >= w.x && x < w.x + w.w && y >= w.y && y < w.y + w.h) return i;
        }
        return -1;
    }

    void ClampScroll(int area) {
        if (!ValidArea(area)) return;
        Widget& sa = widgets_[area];
        int contentW = 0, contentH = 0;
        for (const Widget& c : widgets_) {
            if (c.owner != area) continue;
            contentW = std::max(contentW, c.x + c.w);
            contentH = std::max(contentH, c.y + c.h);
        }
        const int maxX = std::max(0, contentW - sa.w);
        const int maxY = std::max(0, contentH - sa.h);
        sa.scrollOffX = std::clamp(sa.scrollOffX, 0, maxX);
        sa.scrollOff  = std::clamp(sa.scrollOff, 0, maxY);
    }

    // --- Anchors -----------------------------------------------------------

    void ApplyAnchors() {
        int cw = 0, ch = 0;
        ClientSize(&cw, &ch);
        for (Widget& w : widgets_) {
            if (w.owner >= 0 || w.anchors == 0) continue;
            const unsigned a = w.anchors;
            if ((a & kAnchorLeft) && (a & kAnchorRight)) {
                w.x = w.ml;
                w.w = std::max(1, cw - w.ml - w.mr);
            } else if (a & kAnchorRight) {
                w.x = cw - w.mr - w.w;
            }
            if ((a & kAnchorTop) && (a & kAnchorBottom)) {
                w.y = w.mt;
                w.h = std::max(1, ch - w.mt - w.mb);
            } else if (a & kAnchorBottom) {
                w.y = ch - w.mb - w.h;
            }
        }
    }

    // --- Message loop ------------------------------------------------------

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
                    SetCaret(hw, TextFieldCaretAt(hit, x, y),
                             (GetKeyState(VK_SHIFT) & 0x8000) != 0);
                    BeginImeComposition(hw);
                } else if (hw.kind == WidgetKind::Button) {
                    pressed_ = hit;
                    hw.pressed = true;
                    focused_ = hit;
                } else if (hw.kind == WidgetKind::Checkbox) {
                    hw.checked = !hw.checked;
                    SafeCall(hw.onChange, hw.checked);
                    Invalidate();
                }
            }
            SafeCall(on_mouse_down_, 0, x, y);
            return 0;
        }

        case WM_LBUTTONUP: {
            const int x = PxToDip(GET_X_LPARAM(lp));
            const int y = PxToDip(GET_Y_LPARAM(lp));
            const int hit = HitTest(x, y);
            if (pressed_ >= 0) {
                Widget& w = widgets_[pressed_];
                w.pressed = false;
                if (hit == pressed_ && w.kind == WidgetKind::Button)
                    SafeCall(w.onClick);
                pressed_ = -1;
            }
            ReleaseCapture();
            SafeCall(on_mouse_up_, 0, x, y);
            Invalidate();
            return 0;
        }

        case WM_MOUSEMOVE: {
            const int x = PxToDip(GET_X_LPARAM(lp));
            const int y = PxToDip(GET_Y_LPARAM(lp));
            SafeCall(on_mouse_move_, x, y);
            return 0;
        }

        case WM_MOUSEWHEEL: {
            POINT pt{GET_X_LPARAM(lp), GET_Y_LPARAM(lp)};
            ScreenToClient(hwnd, &pt);
            const int x = PxToDip(pt.x), y = PxToDip(pt.y);
            const int hit = HitTest(x, y);
            if (hit >= 0) {
                const int area = widgets_[hit].owner >= 0
                                     ? widgets_[hit].owner
                                     : (widgets_[hit].kind == WidgetKind::ScrollArea
                                            ? hit : -1);
                if (area >= 0) {
                    Widget& sa = widgets_[area];
                    sa.scrollOff -= (GET_WHEEL_DELTA_WPARAM(wp) / WHEEL_DELTA) * 48;
                    ClampScroll(area);
                    Invalidate();
                }
            }
            SafeCall(on_mouse_wheel_, GET_WHEEL_DELTA_WPARAM(wp), x, y);
            return 0;
        }

        case WM_MOUSEHWHEEL: {
            POINT pt{GET_X_LPARAM(lp), GET_Y_LPARAM(lp)};
            ScreenToClient(hwnd, &pt);
            const int x = PxToDip(pt.x), y = PxToDip(pt.y);
            const int hit = HitTest(x, y);
            if (hit >= 0) {
                const int area = widgets_[hit].owner >= 0
                                     ? widgets_[hit].owner
                                     : (widgets_[hit].kind == WidgetKind::ScrollArea
                                            ? hit : -1);
                if (area >= 0) {
                    Widget& sa = widgets_[area];
                    sa.scrollOffX += (GET_WHEEL_DELTA_WPARAM(wp) / WHEEL_DELTA) * 48;
                    ClampScroll(area);
                    Invalidate();
                }
            }
            return 0;
        }

        case WM_KEYDOWN:
            if (focused_ >= 0 &&
                widgets_[focused_].kind == WidgetKind::TextField) {
                Widget& tf = widgets_[focused_];
                const bool ctrl  = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
                const bool shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;

                if (ctrl && (wp == 'Z' || wp == 'z')) {
                    if (shift) DoRedo(tf); else DoUndo(tf);
                    return 0;
                }
                if (ctrl && (wp == 'Y' || wp == 'y')) {
                    DoRedo(tf);
                    return 0;
                }

                switch (wp) {
                case VK_LEFT:
                    SetCaret(tf, tf.caret - 1, shift);
                    Invalidate();
                    return 0;
                case VK_RIGHT:
                    SetCaret(tf, tf.caret + 1, shift);
                    Invalidate();
                    return 0;
                case VK_HOME:
                    SetCaret(tf, 0, shift);
                    Invalidate();
                    return 0;
                case VK_END:
                    SetCaret(tf, static_cast<int>(tf.text.size()), shift);
                    Invalidate();
                    return 0;
                case VK_BACK:
                    if (tf.caret > 0) {
                        PushUndo(tf);
                        tf.text.erase(tf.caret - 1, 1);
                        SetCaret(tf, tf.caret - 1, false);
                        AfterEdit(tf);
                    }
                    return 0;
                case VK_DELETE:
                    if (tf.caret < static_cast<int>(tf.text.size())) {
                        PushUndo(tf);
                        tf.text.erase(tf.caret, 1);
                        AfterEdit(tf);
                    }
                    return 0;
                case VK_RETURN:
                    if (tf.multiline) {
                        PushUndo(tf);
                        tf.text.insert(tf.caret, 1, L'\n');
                        SetCaret(tf, tf.caret + 1, false);
                        AfterEdit(tf);
                        return 0;
                    }
                    return 0;
                }
            }
            SafeCall(on_key_down_, static_cast<int>(wp), ModsNow());
            return 0;

        case WM_CHAR:
            if (focused_ >= 0 && widgets_[focused_].kind == WidgetKind::TextField) {
                wchar_t ch = static_cast<wchar_t>(wp);
                if (ch >= 0x20 || ch == L'\n' || ch == L'\r') {
                    Widget& tf = widgets_[focused_];
                    if (ch == L'\r') ch = L'\n';
                    if (ch == L'\n' && !tf.multiline) return 0;
                    PushUndo(tf);
                    tf.text.insert(tf.caret, 1, ch);
                    SetCaret(tf, tf.caret + 1, false);
                    AfterEdit(tf);
                }
            }
            SafeCall(on_text_, static_cast<unsigned int>(wp));
            return 0;

        case WM_IME_STARTCOMPOSITION:
            if (focused_ >= 0 && widgets_[focused_].kind == WidgetKind::TextField)
                BeginImeComposition(widgets_[focused_]);
            return 0;

        case WM_IME_COMPOSITION:
            if (focused_ >= 0 && widgets_[focused_].kind == WidgetKind::TextField)
                UpdateImeComposition(widgets_[focused_], lp);
            return 0;

        case WM_IME_ENDCOMPOSITION:
            return 0;

        case WM_TIMER:
            {
                auto it = timers_.find(static_cast<int>(wp));
                if (it != timers_.end()) SafeCall(it->second);
            }
            return 0;

        case WM_SIZE:
            renderer_.OnResize(LOWORD(lp), HIWORD(lp));
            if (wp != SIZE_MINIMIZED) ApplyAnchors();
            for (std::size_t i = 0; i < widgets_.size(); ++i)
                if (widgets_[i].kind == WidgetKind::ScrollArea)
                    ClampScroll(static_cast<int>(i));
            SafeCall(on_resize_, PxToDip(LOWORD(lp)), PxToDip(HIWORD(lp)));
            Invalidate();
            return 0;

        case WM_DPICHANGED: {
            const float new_dpi = static_cast<float>(HIWORD(wp));
            if (new_dpi > 0.0f) {
                dpi_ = new_dpi;
                renderer_.SetScale(Scale());
            }
            Invalidate();
            return 0;
        }

        case WM_CLOSE: {
            bool allow = true;
            if (on_close_) {
                try { allow = on_close_(); } catch (...) { allow = true; }
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

    // --- Fields ------------------------------------------------------------

    static constexpr wchar_t kClassName[] = L"SableUIWindow";
    static constexpr int kModCtrl = 1, kModShift = 2, kModAlt = 4;
    static constexpr unsigned kAnchorLeft = 1, kAnchorTop = 2,
                              kAnchorRight = 4, kAnchorBottom = 8;

    HWND hwnd_ = nullptr;
    D2DRenderer renderer_;
    Palette palette_;
    std::vector<Widget> widgets_;
    std::unordered_map<int, std::function<void()>> timers_;
    int next_timer_id_ = 1;

    std::function<void()> on_paint_;
    std::function<bool()> on_close_;
    std::function<void(int, int, int)> on_mouse_down_, on_mouse_up_;
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
    bool borderless_ = false;
    bool com_initialized_ = false;
    int min_w_ = 0, min_h_ = 0;
    float dpi_ = 96.0f;
    int hover_ = -1;
    int pressed_ = -1;
    int focused_ = -1;
    bool caret_on_ = false;
};

} // namespace sableui