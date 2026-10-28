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

typedef void (SABLEUI_CALL* sabClickCallback)(void* userdata);

typedef void (SABLEUI_CALL* sabChangeCallback)(void* userdata, int checked);

typedef void (SABLEUI_CALL* sabValueCallback)(void* userdata, float value);

typedef void (SABLEUI_CALL* sabTextChangeCallback)(void* userdata,
                                                   const wchar_t* text);

typedef void (SABLEUI_CALL* sabMouseButtonCallback)(void* userdata, int button,
                                                    int x, int y);

typedef void (SABLEUI_CALL* sabMouseMoveCallback)(void* userdata, int x, int y);

typedef void (SABLEUI_CALL* sabWheelCallback)(void* userdata, int delta,
                                              int x, int y);

typedef void (SABLEUI_CALL* sabKeyCallback)(void* userdata, int key, int mods);

typedef void (SABLEUI_CALL* sabTextCallback)(void* userdata, unsigned int codepoint);

typedef void (SABLEUI_CALL* sabResizeCallback)(void* userdata, int width, int height);

typedef int (SABLEUI_CALL* sabCloseCallback)(void* userdata);

typedef void (SABLEUI_CALL* sabSimpleCallback)(void* userdata);

// --- Window lifecycle ----------------------------------------------------------

SABLEUI_API sabWindow sabCreateWindow(const wchar_t* title, int width, int height);
SABLEUI_API void sabDestroyWindow(sabWindow window);
SABLEUI_API void sabRun(sabWindow window);
SABLEUI_API void sabSetTitle(sabWindow window, const wchar_t* title);
SABLEUI_API void sabSetResizable(sabWindow window, int on);
SABLEUI_API void sabSetBorderless(sabWindow window, int on);
SABLEUI_API void sabSetMinimumSize(sabWindow window, int width, int height);
SABLEUI_API void sabCloseWindow(sabWindow window);
SABLEUI_API void sabGetClientSize(sabWindow window, int* width, int* height);
SABLEUI_API void sabInvalidate(sabWindow window);

// --- Events ---------------------------------------------------------------------

SABLEUI_API void sabOnMouseDown(sabWindow window, sabMouseButtonCallback cb, void* userdata);
SABLEUI_API void sabOnMouseUp(sabWindow window, sabMouseButtonCallback cb, void* userdata);
SABLEUI_API void sabOnMouseMove(sabWindow window, sabMouseMoveCallback cb, void* userdata);
SABLEUI_API void sabOnMouseWheel(sabWindow window, sabWheelCallback cb, void* userdata);
SABLEUI_API void sabOnKeyDown(sabWindow window, sabKeyCallback cb, void* userdata);
SABLEUI_API void sabOnKeyUp(sabWindow window, sabKeyCallback cb, void* userdata);
SABLEUI_API void sabOnText(sabWindow window, sabTextCallback cb, void* userdata);
SABLEUI_API void sabOnResize(sabWindow window, sabResizeCallback cb, void* userdata);
SABLEUI_API void sabOnClose(sabWindow window, sabCloseCallback cb, void* userdata);
SABLEUI_API void sabOnPaint(sabWindow window, sabSimpleCallback cb, void* userdata);

// --- Timers ---------------------------------------------------------------------

SABLEUI_API sabTimerId sabAddTimer(sabWindow window, int interval_ms,
                                   sabSimpleCallback cb, void* userdata);
SABLEUI_API void sabRemoveTimer(sabWindow window, sabTimerId id);

// --- Drawing (paint callback only; colors are 0.0-1.0) ---------------------------

SABLEUI_API void sabSetFillColor(sabWindow window, float r, float g, float b, float a);
SABLEUI_API void sabSetStrokeColor(sabWindow window, float r, float g, float b, float a);
SABLEUI_API void sabSetStrokeWidth(sabWindow window, float width);
SABLEUI_API void sabClear(sabWindow window);
SABLEUI_API void sabFillRect(sabWindow window, float x, float y, float w, float h);
SABLEUI_API void sabDrawRect(sabWindow window, float x, float y, float w, float h);
SABLEUI_API void sabFillRoundedRect(sabWindow window, float x, float y, float w, float h,
                                    float radius);
SABLEUI_API void sabFillEllipse(sabWindow window, float cx, float cy, float rx, float ry);
SABLEUI_API void sabDrawEllipse(sabWindow window, float cx, float cy, float rx, float ry);
SABLEUI_API void sabDrawLine(sabWindow window, float x1, float y1, float x2, float y2);
SABLEUI_API void sabDrawText(sabWindow window, float x, float y, float w, float h,
                             const wchar_t* text, float font_size);

// --- Widgets ---------------------------------------------------------------------

SABLEUI_API sabWidgetId sabAddButton(sabWindow window, int x, int y, int w, int h,
                                     const wchar_t* text,
                                     sabClickCallback onClick, void* userdata);

SABLEUI_API sabWidgetId sabAddLabel(sabWindow window, int x, int y, int w, int h,
                                    const wchar_t* text);

SABLEUI_API sabWidgetId sabAddCheckbox(sabWindow window, int x, int y, int w, int h,
                                       const wchar_t* text, int checked,
                                       sabChangeCallback onChange, void* userdata);

// Single-line text field. masked: 1 renders '*' (passwords).
SABLEUI_API sabWidgetId sabAddTextField(sabWindow window, int x, int y, int w, int h,
                                        const wchar_t* initial, int masked,
                                        sabTextChangeCallback onChange,
                                        void* userdata);

// Text field with explicit multiline flag. Same widget as sabAddTextField,
// but callers can request a multiline field directly.
SABLEUI_API sabWidgetId sabAddTextFieldEx(sabWindow window, int x, int y, int w, int h,
                                          const wchar_t* initial, int masked,
                                          int multiline,
                                          sabTextChangeCallback onChange,
                                          void* userdata);

// Multiline convenience wrapper (same as sabAddTextFieldEx with multiline=1
// and masked=0).
SABLEUI_API sabWidgetId sabAddTextArea(sabWindow window, int x, int y, int w, int h,
                                       const wchar_t* initial,
                                       sabTextChangeCallback onChange,
                                       void* userdata);

SABLEUI_API sabWidgetId sabAddSlider(sabWindow window, int x, int y, int w, int h,
                                     float minVal, float maxVal, float value,
                                     sabValueCallback onValueChange, void* userdata);

SABLEUI_API sabWidgetId sabAddProgressBar(sabWindow window, int x, int y, int w, int h,
                                          float value);

// Image widget. path points to a file readable by WIC (PNG, JPEG, BMP, GIF,
// ICO, TIFF, ...). The image is scaled to fit the widget rect. Returns the
// widget id, or -1 on failure.
SABLEUI_API sabWidgetId sabAddImage(sabWindow window, int x, int y, int w, int h,
                                    const wchar_t* path);

// Sets the text of a button, label or text field. Fires the field's
// onChange for text fields. Returns 1 on success, 0 on failure.
SABLEUI_API int sabSetWidgetText(sabWindow window, sabWidgetId id, const wchar_t* text);

// Legacy alias for sabSetWidgetText (kept for source compatibility).
SABLEUI_API int sabSetLabelText(sabWindow window, sabWidgetId id, const wchar_t* text);

SABLEUI_API int sabGetWidgetText(sabWindow window, sabWidgetId id,
                                 wchar_t* buffer, int buffer_chars);

SABLEUI_API int sabSetWidgetChecked(sabWindow window, sabWidgetId id, int on);
SABLEUI_API int sabIsWidgetChecked(sabWindow window, sabWidgetId id);
SABLEUI_API int sabSetWidgetValue(sabWindow window, sabWidgetId id, float value);
// Undo/redo for the focused text field. Returns 1 on success, 0 if there
// is nothing to undo or no text field has focus.
SABLEUI_API int sabUndo(sabWindow window);
SABLEUI_API int sabRedo(sabWindow window);

// --- Widget styling --------------------------------------------------------------

SABLEUI_API int sabSetWidgetFont(sabWindow window, sabWidgetId id,
                                 const wchar_t* family, float size);

// Per-widget background colour. Pass 0 for all four channels to revert to
// the palette. Returns 1 on success, 0 on failure.
SABLEUI_API int sabSetWidgetBackground(sabWindow window, sabWidgetId id,
                                       float r, float g, float b, float a);

// Per-widget corner radius. Negative radius reverts to the palette default.
SABLEUI_API int sabSetWidgetRadius(sabWindow window, sabWidgetId id, float radius);

// Replaces the image of an existing image widget. path may be NULL to clear.
// Returns 1 on success, 0 on failure.
SABLEUI_API int sabSetWidgetImage(sabWindow window, sabWidgetId id,
                                  const wchar_t* path);

// Replaces the image path of an image widget and reloads it.
SABLEUI_API int sabSetWidgetImage(sabWindow window, sabWidgetId id, const wchar_t* path);

// --- Widget state and lifetime ---------------------------------------------------

SABLEUI_API int sabSetWidgetEnabled(sabWindow window, sabWidgetId id, int on);
SABLEUI_API int sabSetWidgetVisible(sabWindow window, sabWidgetId id, int on);
SABLEUI_API int sabRemoveWidget(sabWindow window, sabWidgetId id);
SABLEUI_API int sabMoveWidgetToFront(sabWindow window, sabWidgetId id);

// --- Layout anchors --------------------------------------------------------------

enum {
    SAB_ANCHOR_NONE   = 0x0,
    SAB_ANCHOR_LEFT   = 0x1,
    SAB_ANCHOR_TOP    = 0x2,
    SAB_ANCHOR_RIGHT  = 0x4,
    SAB_ANCHOR_BOTTOM = 0x8
};

#define SAB_LAYOUT_FILL_H       (SAB_ANCHOR_LEFT | SAB_ANCHOR_RIGHT)
#define SAB_LAYOUT_FILL_V       (SAB_ANCHOR_TOP | SAB_ANCHOR_BOTTOM)
#define SAB_LAYOUT_FILL         (SAB_LAYOUT_FILL_H | SAB_LAYOUT_FILL_V)
#define SAB_LAYOUT_TOP_LEFT     (SAB_ANCHOR_LEFT | SAB_ANCHOR_TOP)
#define SAB_LAYOUT_TOP_RIGHT    (SAB_ANCHOR_RIGHT | SAB_ANCHOR_TOP)
#define SAB_LAYOUT_BOTTOM_LEFT  (SAB_ANCHOR_LEFT | SAB_ANCHOR_BOTTOM)
#define SAB_LAYOUT_BOTTOM_RIGHT (SAB_ANCHOR_RIGHT | SAB_ANCHOR_BOTTOM)

SABLEUI_API int sabSetWidgetAnchors(sabWindow window, sabWidgetId id,
                                    unsigned anchors);

// --- Scroll areas ----------------------------------------------------------------

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

// Multiline text field inside a scroll area, with explicit flag.
SABLEUI_API sabWidgetId sabAddTextFieldInEx(sabWindow window, sabWidgetId area,
                                            int x, int y, int w, int h,
                                            const wchar_t* initial, int masked,
                                            int multiline,
                                            sabTextChangeCallback onChange,
                                            void* userdata);

// Multiline convenience wrapper inside a scroll area.
SABLEUI_API sabWidgetId sabAddTextAreaIn(sabWindow window, sabWidgetId area,
                                         int x, int y, int w, int h,
                                         const wchar_t* initial,
                                         sabTextChangeCallback onChange,
                                         void* userdata);

// Image inside a scroll area.
SABLEUI_API sabWidgetId sabAddImageIn(sabWindow window, sabWidgetId area,
                                      int x, int y, int w, int h,
                                      const wchar_t* path);

// Vertical offset. Returns 1 on success, 0 on failure.
SABLEUI_API int sabSetScrollOffset(sabWindow window, sabWidgetId area, int offset);
SABLEUI_API int sabGetScrollOffset(sabWindow window, sabWidgetId area);

// Horizontal offset. Returns 1 on success, 0 on failure.
SABLEUI_API int sabSetScrollOffsetX(sabWindow window, sabWidgetId area, int offset);
// Returns the current horizontal scroll offset of a scroll area, or -1.
SABLEUI_API int sabGetScrollOffsetX(sabWindow window, sabWidgetId area);

// --- Errors ---------------------------------------------------------------------

SABLEUI_API const char* sabGetLastError(void);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // SABLEUI_H