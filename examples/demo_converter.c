// demo_converter.c - SableUI currency converter demo.
// Build (MinGW):
//   gcc demo_converter.c -Iinclude -L. -lsableui -o converter.exe

#include <sableui/sableui.h>
#include <stdio.h>
#include <wchar.h>
#include <stdlib.h>

static sabWindow g_win = NULL;
static sabWidgetId g_amount_input = -1;
static sabWidgetId g_result_label = -1;
static sabWidgetId g_scroll_area = -1;

static wchar_t g_amount[64] = L"100";

// Простые курсы к USD (условно)
typedef struct {
    const wchar_t* name;
    double rate;
} Currency;

static Currency g_currencies[] = {
    { L"USD - US Dollar", 1.0 },
    { L"EUR - Euro", 1.08 },
    { L"GBP - British Pound", 1.27 },
    { L"JPY - Japanese Yen", 0.0067 },
    { L"CNY - Chinese Yuan", 0.14 },
    { L"RUB - Russian Ruble", 0.011 },
    { L"CAD - Canadian Dollar", 0.74 },
    { L"AUD - Australian Dollar", 0.66 },
};
static const int g_currency_count = sizeof(g_currencies) / sizeof(g_currencies[0]);

static void update_result(void) {
    if (g_amount_input < 0 || g_result_label < 0) return;
    
    wchar_t buf[64] = {0};
    sabGetWidgetText(g_win, g_amount_input, buf, 64);
    
    wchar_t* end = NULL;
    double amount = wcstod(buf, &end);
    if (end == buf || *end != L'\0') {
        sabSetLabelText(g_win, g_result_label, L"Invalid amount.");
        return;
    }

    wchar_t result[256];
    swprintf(result, 256, L"%.2f USD = %.2f EUR", amount, amount / g_currencies[1].rate);
    sabSetLabelText(g_win, g_result_label, result);
}

static void on_amount_changed(void* ud, const wchar_t* text) {
    (void)ud;
    (void)text;
    update_result();
}

static void on_currency_click(void* ud) {
    const Currency* c = (const Currency*)ud;
    if (g_amount_input < 0 || g_result_label < 0) return;

    wchar_t buf[64] = {0};
    sabGetWidgetText(g_win, g_amount_input, buf, 64);
    double amount = wcstod(buf, NULL);

    wchar_t result[256];
    swprintf(result, 256, L"%.2f %ls = %.2f USD", amount, c->name, amount * c->rate);
    sabSetLabelText(g_win, g_result_label, result);
}

static void on_scroll_top(void* ud) {
    (void)ud;
    if (g_scroll_area >= 0) sabSetScrollOffset(g_win, g_scroll_area, 0);
}

static void on_scroll_bottom(void* ud) {
    (void)ud;
    if (g_scroll_area >= 0) sabSetScrollOffset(g_win, g_scroll_area, 500);
}

int main(void) {
    g_win = sabCreateWindow(L"SableUI Currency Converter", 480, 560);
    if (!g_win) {
        printf("sabCreateWindow failed: %s\n", sabGetLastError());
        return 1;
    }

    sabSetMinimumSize(g_win, 400, 480);

    sabAddLabel(g_win, 16, 16, 448, 24, L"Amount:");
    g_amount_input = sabAddTextField(g_win, 16, 40, 200, 32, g_amount, 0, on_amount_changed, NULL);

    sabAddLabel(g_win, 16, 80, 448, 24, L"Result:");
    g_result_label = sabAddLabel(g_win, 16, 104, 448, 24, L"Enter amount and pick a currency.");

    sabAddLabel(g_win, 16, 144, 448, 24, L"Currencies (click to convert):");
    g_scroll_area = sabAddScrollArea(g_win, 16, 168, 448, 280);
    
    for (int i = 0; i < g_currency_count; ++i) {
        sabAddButtonIn(g_win, g_scroll_area, 8, i * 40, 420, 32,
                       g_currencies[i].name, on_currency_click, (void*)&g_currencies[i]);
    }

    // Управление скроллом
    sabAddButton(g_win, 16, 460, 100, 32, L"Scroll Top", on_scroll_top, NULL);
    sabAddButton(g_win, 124, 460, 100, 32, L"Scroll Bottom", on_scroll_bottom, NULL);

    sabRun(g_win);
    sabDestroyWindow(g_win);
    return 0;
}