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

// --- Layout anchors --------------------------------------------------------------

// Each anchored edge keeps its current distance (margin) to the window client
// area when the window is resized. Anchoring opposite edges stretches the
// widget. Only top-level widgets can be anchored: sabSetWidgetAnchors fails
// for widgets inside scroll areas.
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

// Captures the widget's current position as the anchor reference. Call once
// after creating the widget. Returns 1 on success, 0 on failure (bad id or
// the widget lives inside a scroll area); check sabGetLastError.
SABLEUI_API int sabSetWidgetAnchors(sabWindow window, sabWidgetId id,
                                    unsigned anchors);

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
