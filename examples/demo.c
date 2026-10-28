// demo.c - SableUI C API demo, "fancy" edition.
// Build (MinGW):
//   gcc demo.c -Iinclude -L. -lsableui -o demo.exe

#include <sableui/sableui.h>
#include <math.h>
#include <stdio.h>
#include <wchar.h>

static sabWindow   g_win = NULL;
static sabWidgetId g_status = -1;
static sabWidgetId g_name   = -1;
static int         g_counter = 0;
static float       g_phase = 0.0f;       // animation phase for the footer dot
static int         g_dark = 1;           // 1 = dark theme, 0 = light
static int         g_hover = 0;          // pointer over the custom button
static int         g_custom_btn_x = 0, g_custom_btn_y = 0;
static int         g_custom_btn_w = 0, g_custom_btn_h = 0;
static int         g_mouse_x = -1, g_mouse_y = -1;

// --- Theme ------------------------------------------------------------------

typedef struct { float r, g, b; } RGB;

static RGB bg(void)        { return g_dark ? (RGB){0.118f,0.122f,0.141f} : (RGB){0.96f,0.96f,0.98f}; }
static RGB header_top(void){ return g_dark ? (RGB){0.16f,0.18f,0.26f}  : (RGB){0.85f,0.88f,0.97f}; }
static RGB header_bot(void){ return g_dark ? (RGB){0.10f,0.11f,0.16f}  : (RGB){0.92f,0.94f,0.99f}; }
static RGB accent(void)    { return (RGB){0.31f,0.55f,1.0f}; }
static RGB text_fg(void)   { return g_dark ? (RGB){0.93f,0.94f,0.96f} : (RGB){0.10f,0.11f,0.14f}; }
static RGB text_dim(void)  { return g_dark ? (RGB){0.55f,0.58f,0.66f} : (RGB){0.40f,0.42f,0.48f}; }

static float lerp(float a, float b, float t) { return a + (b - a) * t; }

// --- Callbacks --------------------------------------------------------------

static void on_greet(void* ud) {
    (void)ud;
    wchar_t name[128] = {0};
    if (g_name >= 0) sabGetWidgetText(g_win, g_name, name, 128);
    wchar_t status[256];
    if (name[0]) swprintf(status, 256, L"Hello, %ls!", name);
    else         wcscpy(status, L"Please enter a name.");
    if (g_status >= 0) sabSetLabelText(g_win, g_status, status);
}

static void on_name_changed(void* ud, const wchar_t* text) { (void)ud; (void)text; }

static void on_agree(void* ud, int checked) {
    (void)ud;
    wchar_t buf[128];
    swprintf(buf, 128, L"Agreed: %ls", checked ? L"yes" : L"no");
    if (g_status >= 0) sabSetLabelText(g_win, g_status, buf);
}

static void on_tick(void* ud) {
    (void)ud;
    ++g_counter;
    g_phase += 0.12f;
    if (g_phase > 6.28318f) g_phase -= 6.28318f;
    if (g_counter % 2 == 0 && g_status >= 0) {
        wchar_t buf[64];
        swprintf(buf, 64, L"Ticks: %d", g_counter);
        sabSetLabelText(g_win, g_status, buf);
    }
    sabInvalidate(g_win);  // repaint for the animated dot
}

static int on_close(void* ud) { (void)ud; return 1; }

static void on_mouse_move(void* ud, int x, int y) {
    (void)ud;
    g_mouse_x = x;
    g_mouse_y = y;
    const int inside =
        x >= g_custom_btn_x && x < g_custom_btn_x + g_custom_btn_w &&
        y >= g_custom_btn_y && y < g_custom_btn_y + g_custom_btn_h;
    if (inside != g_hover) {
        g_hover = inside;
        sabInvalidate(g_win);
    }
}

static void on_mouse_leave(void* ud) {
    (void)ud;
    if (g_hover) { g_hover = 0; sabInvalidate(g_win); }
}

// --- Painting ---------------------------------------------------------------

static void paint_header(int cw) {
    // Vertical gradient: 28 thin strips from header_top to header_bot.
    const int strips = 28;
    for (int i = 0; i < strips; ++i) {
        const float t = (float)i / (strips - 1);
        RGB a = header_top(), b = header_bot();
        sabSetFillColor(g_win, lerp(a.r,b.r,t), lerp(a.g,b.g,t), lerp(a.b,b.b,t), 1.0f);
        sabFillRect(g_win, 0.0f, i * 2.0f, (float)cw, 2.0f);
    }
    // Accent hairline under the header.
    RGB ac = accent();
    sabSetStrokeColor(g_win, ac.r, ac.g, ac.b, 1.0f);
    sabSetStrokeWidth(g_win, 2.0f);
    sabDrawLine(g_win, 0.0f, 55.0f, (float)cw, 55.0f);
    // Title.
    RGB tf = text_fg();
    sabSetFillColor(g_win, tf.r, tf.g, tf.b, 1.0f);
    sabDrawText(g_win, 16.0f, 8.0f, (float)cw - 32.0f, 40.0f, L"SableUI Demo", 20.0f);
    // Subtitle.
    RGB td = text_dim();
    sabSetFillColor(g_win, td.r, td.g, td.b, 1.0f);
    sabDrawText(g_win, 16.0f, 32.0f, (float)cw - 32.0f, 18.0f,
                L"fancy edition", 12.0f);
}

static void paint_footer(int cw, int ch) {
    // Animated accent dot travelling along a sine wave.
    const float cx = (float)cw - 40.0f;
    const float cy = (float)ch - 22.0f;
    const float dx = cosf(g_phase) * 18.0f;
    const float dy = sinf(g_phase * 1.7f) * 4.0f;
    RGB ac = accent();
    // Glow.
    sabSetFillColor(g_win, ac.r, ac.g, ac.b, 0.15f);
    sabFillEllipse(g_win, cx + dx, cy + dy, 9.0f, 9.0f);
    // Core.
    sabSetFillColor(g_win, ac.r, ac.g, ac.b, 1.0f);
    sabFillEllipse(g_win, cx + dx, cy + dy, 4.0f, 4.0f);

    // Static dot row on the left.
    sabSetFillColor(g_win, ac.r, ac.g, ac.b, 0.30f);
    for (int i = 0; i < 10; ++i) {
        sabFillEllipse(g_win, 20.0f + i * 14.0f, (float)ch - 22.0f, 2.5f, 2.5f);
    }
}

static void paint_custom_button(void) {
    // Painted entirely by hand: rounded rect + label, with hover/pressed state.
    RGB ac = accent();
    const float r = g_hover ? 0.45f : 0.31f;
    const float g = g_hover ? 0.65f : 0.55f;
    const float b = g_hover ? 1.00f : 1.00f;
    sabSetFillColor(g_win, r, g, b, 1.0f);
    sabFillRoundedRect(g_win,
                       (float)g_custom_btn_x, (float)g_custom_btn_y,
                       (float)g_custom_btn_w, (float)g_custom_btn_h,
                       8.0f);
    // Outline glow on hover.
    if (g_hover) {
        sabSetStrokeColor(g_win, 1.0f, 1.0f, 1.0f, 0.35f);
        sabSetStrokeWidth(g_win, 1.5f);
        sabDrawRect(g_win, (float)g_custom_btn_x + 1.0f, (float)g_custom_btn_y + 1.0f,
                    (float)g_custom_btn_w - 2.0f, (float)g_custom_btn_h - 2.0f);
    }
    sabSetFillColor(g_win, 1.0f, 1.0f, 1.0f, 1.0f);
    sabDrawText(g_win,
                (float)g_custom_btn_x, (float)g_custom_btn_y,
                (float)g_custom_btn_w, (float)g_custom_btn_h,
                L"Custom button", 15.0f);
}

static void on_paint(void* ud) {
    (void)ud;
    int cw = 0, ch = 0;
    sabGetClientSize(g_win, &cw, &ch);

    // Backdrop.
    RGB b = bg();
    sabSetFillColor(g_win, b.r, b.g, b.b, 1.0f);
    sabClear(g_win);

    paint_header(cw);
    paint_footer(cw, ch);
    paint_custom_button();
}

// --- Main -------------------------------------------------------------------

int main(void) {
    g_win = sabCreateWindow(L"SableUI demo", 560, 600);
    if (!g_win) {
        printf("sabCreateWindow failed: %s\n", sabGetLastError());
        return 1;
    }

    sabSetMinimumSize(g_win, 420, 520);
    sabOnPaint(g_win, on_paint, NULL);
    sabOnClose(g_win, on_close, NULL);
    sabOnMouseMove(g_win, on_mouse_move, NULL);

    // Header occupies 0..56; widgets start below.
    sabAddLabel(g_win, 16, 72, 528, 24,
                L"Enter a name and click \"Greet\".");

    g_name = sabAddTextField(g_win, 16, 104, 340, 32, L"", 0,
                             on_name_changed, NULL);

    sabAddButton(g_win, 368, 104, 176, 32, L"Greet",
                 on_greet, NULL);

    sabAddCheckbox(g_win, 16, 152, 300, 24, L"I agree",
                   0, on_agree, NULL);

    g_status = sabAddLabel(g_win, 16, 184, 528, 24, L"Ready.");

    // Custom-painted button: position is tracked manually for hover.
    g_custom_btn_x = 16;
    g_custom_btn_y = 220;
    g_custom_btn_w = 220;
    g_custom_btn_h = 36;

    // Scroll area with a list of labels and a button inside.
    sabAddLabel(g_win, 16, 272, 528, 24, L"List (scrollable):");
    sabWidgetId area = sabAddScrollArea(g_win, 16, 300, 528, 220);
    if (area < 0) {
        printf("sabAddScrollArea failed: %s\n", sabGetLastError());
        sabDestroyWindow(g_win);
        return 1;
    }
    for (int i = 0; i < 24; ++i) {
        wchar_t line[64];
        swprintf(line, 64, L"Item #%d", i + 1);
        sabAddLabelIn(g_win, area, 12, i * 28, 480, 24, line);
    }
    sabAddButtonIn(g_win, area, 12, 24 * 28, 220, 32,
                   L"Button in scroll", on_greet, NULL);

    sabAddTimer(g_win, 1000, on_tick, NULL);
    sabAddTimer(g_win, 33, on_tick, NULL);   // ~30 fps for the dot animation

    sabRun(g_win);
    sabDestroyWindow(g_win);
    return 0;
}