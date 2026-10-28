// demo_calc.c - SableUI calculator demo.
// Build (MinGW):
//   gcc demo_calc.c -Iinclude -L. -lsableui -o calc.exe

#include <sableui/sableui.h>
#include <stdio.h>
#include <wchar.h>
#include <stdlib.h>

static sabWindow g_win = NULL;
static sabWidgetId g_display = -1;

static wchar_t g_expr[128] = L"0";
static int g_just_evaluated = 0;

static void update_display(void) {
    if (g_display >= 0) {
        sabSetLabelText(g_win, g_display, g_expr);
    }
}

static void on_clear(void* ud) {
    (void)ud;
    wcscpy(g_expr, L"0");
    g_just_evaluated = 0;
    update_display();
}

static void on_digit(void* ud) {
    const wchar_t* digit = (const wchar_t*)ud;
    
    if (g_just_evaluated) {
        wcscpy(g_expr, L"0");
        g_just_evaluated = 0;
    }

    if (wcscmp(g_expr, L"0") == 0 && wcscmp(digit, L"0") != 0) {
        wcscpy(g_expr, digit);
    } else if (wcslen(g_expr) < 20) {
        wcscat(g_expr, digit);
    }
    update_display();
}

static void on_op(void* ud) {
    const wchar_t* op = (const wchar_t*)ud;
    if (g_just_evaluated) g_just_evaluated = 0;
    
    // Simple guard against appending operators to an empty/zero state
    if (wcscmp(g_expr, L"0") == 0 && wcscmp(op, L"-") != 0) {
        wcscpy(g_expr, op);
    } else if (wcslen(g_expr) < 20) {
        // If last char is an operator, replace it
        wchar_t last = g_expr[wcslen(g_expr) - 1];
        if (last == L'+' || last == L'-' || last == L'*' || last == L'/') {
            g_expr[wcslen(g_expr) - 1] = op[0];
        } else {
            wcscat(g_expr, op);
        }
    }
    update_display();
}

static void on_equals(void* ud) {
    (void)ud;
    // Very basic evaluation: parse two numbers separated by one operator.
    wchar_t* p = g_expr;
    double a = wcstod(p, &p);
    if (*p == L'\0') { g_just_evaluated = 1; return; }

    wchar_t op = *p++;
    double b = wcstod(p, &p);
    double res = 0;

    if (op == L'+') res = a + b;
    else if (op == L'-') res = a - b;
    else if (op == L'*') res = a * b;
    else if (op == L'/') res = (b != 0) ? a / b : 0;

    swprintf(g_expr, 128, L"%g", res);
    g_just_evaluated = 1;
    update_display();
}

int main(void) {
    g_win = sabCreateWindow(L"SableUI Calculator", 340, 480);
    if (!g_win) {
        printf("sabCreateWindow failed: %s\n", sabGetLastError());
        return 1;
    }

    sabSetMinimumSize(g_win, 300, 400);

    // Display label
    g_display = sabAddLabel(g_win, 16, 16, 308, 60, g_expr);

    // Grid of buttons (4 columns x 5 rows)
    const int start_x = 16;
    const int start_y = 90;
    const int btn_w = 68;
    const int btn_h = 60;
    const int gap = 10;

    // Row 1: C, +/-, %, /
    sabAddButton(g_win, start_x, start_y, btn_w, btn_h, L"C", on_clear, NULL);
    sabAddButton(g_win, start_x + (btn_w + gap), start_y, btn_w, btn_h, L"/", on_op, L"/");

    // Row 2: 7, 8, 9, *
    sabAddButton(g_win, start_x, start_y + (btn_h + gap), btn_w, btn_h, L"7", on_digit, L"7");
    sabAddButton(g_win, start_x + (btn_w + gap), start_y + (btn_h + gap), btn_w, btn_h, L"8", on_digit, L"8");
    sabAddButton(g_win, start_x + 2 * (btn_w + gap), start_y + (btn_h + gap), btn_w, btn_h, L"9", on_digit, L"9");
    sabAddButton(g_win, start_x + 3 * (btn_w + gap), start_y + (btn_h + gap), btn_w, btn_h, L"*", on_op, L"*");

    // Row 3: 4, 5, 6, -
    sabAddButton(g_win, start_x, start_y + 2 * (btn_h + gap), btn_w, btn_h, L"4", on_digit, L"4");
    sabAddButton(g_win, start_x + (btn_w + gap), start_y + 2 * (btn_h + gap), btn_w, btn_h, L"5", on_digit, L"5");
    sabAddButton(g_win, start_x + 2 * (btn_w + gap), start_y + 2 * (btn_h + gap), btn_w, btn_h, L"6", on_digit, L"6");
    sabAddButton(g_win, start_x + 3 * (btn_w + gap), start_y + 2 * (btn_h + gap), btn_w, btn_h, L"-", on_op, L"-");

    // Row 4: 1, 2, 3, +
    sabAddButton(g_win, start_x, start_y + 3 * (btn_h + gap), btn_w, btn_h, L"1", on_digit, L"1");
    sabAddButton(g_win, start_x + (btn_w + gap), start_y + 3 * (btn_h + gap), btn_w, btn_h, L"2", on_digit, L"2");
    sabAddButton(g_win, start_x + 2 * (btn_w + gap), start_y + 3 * (btn_h + gap), btn_w, btn_h, L"3", on_digit, L"3");
    sabAddButton(g_win, start_x + 3 * (btn_w + gap), start_y + 3 * (btn_h + gap), btn_w, btn_h, L"+", on_op, L"+");

    // Row 5: 0, ., =
    sabAddButton(g_win, start_x, start_y + 4 * (btn_h + gap), btn_w * 2 + gap, btn_h, L"0", on_digit, L"0");
    sabAddButton(g_win, start_x + 2 * (btn_w + gap), start_y + 4 * (btn_h + gap), btn_w, btn_h, L".", on_digit, L".");
    sabAddButton(g_win, start_x + 3 * (btn_w + gap), start_y + 4 * (btn_h + gap), btn_w, btn_h, L"=", on_equals, NULL);

    sabRun(g_win);
    sabDestroyWindow(g_win);
    return 0;
}