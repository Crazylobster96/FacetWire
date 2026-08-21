/* SPDX-License-Identifier: MPL-2.0 */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commctrl.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>

#include "facetwire_ui_spike.h"

#define CONTROL_OPACITY 1001
#define CONTROL_DISTANCE 1002
#define CONTROL_FALLBACK 1003
#define FWDL_HEADER_SIZE 12u
#define FWDL_COMMAND_SIZE 40u

static fwui_context *g_context = NULL;
static HWND g_opacity_slider = NULL;
static HWND g_distance_slider = NULL;
static HWND g_fallback_checkbox = NULL;

static uint32_t read_u32_le(const uint8_t *src) {
    return (uint32_t)src[0] |
        ((uint32_t)src[1] << 8u) |
        ((uint32_t)src[2] << 16u) |
        ((uint32_t)src[3] << 24u);
}

static float read_f32_le(const uint8_t *src) {
    const uint32_t bits = read_u32_le(src);
    float value = 0.0f;
    memcpy(&value, &bits, sizeof(value));
    return value;
}

static void fill_rect(HDC dc, const RECT *rect, COLORREF color) {
    HBRUSH brush = CreateSolidBrush(color);
    FillRect(dc, rect, brush);
    DeleteObject(brush);
}

static void alpha_fill_rect(
    HDC destination,
    const RECT *rect,
    COLORREF color,
    BYTE opacity) {
    HDC source = CreateCompatibleDC(destination);
    HBITMAP bitmap = CreateCompatibleBitmap(destination, 1, 1);
    HGDIOBJ previous_bitmap = SelectObject(source, bitmap);
    RECT pixel = {0, 0, 1, 1};
    BLENDFUNCTION blend = {AC_SRC_OVER, 0, opacity, 0};
    fill_rect(source, &pixel, color);
    AlphaBlend(
        destination,
        rect->left,
        rect->top,
        rect->right - rect->left,
        rect->bottom - rect->top,
        source,
        0,
        0,
        1,
        1,
        blend);
    SelectObject(source, previous_bitmap);
    DeleteObject(bitmap);
    DeleteDC(source);
}

static void draw_text_line(HDC dc, int x, int y, COLORREF color, const wchar_t *text) {
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, color);
    TextOutW(dc, x, y, text, (int)wcslen(text));
}

static void draw_room_grid(HDC dc, const RECT *bounds) {
    HPEN pen = CreatePen(PS_SOLID, 1, RGB(45, 58, 78));
    HGDIOBJ previous = SelectObject(dc, pen);
    int x = 0;
    int y = 0;
    for (x = bounds->left; x < bounds->right; x += 48) {
        MoveToEx(dc, x, bounds->top, NULL);
        LineTo(dc, x, bounds->bottom);
    }
    for (y = bounds->top; y < bounds->bottom; y += 48) {
        MoveToEx(dc, bounds->left, y, NULL);
        LineTo(dc, bounds->right, y);
    }
    SelectObject(dc, previous);
    DeleteObject(pen);
}

static COLORREF command_color(const uint8_t *command) {
    const int red = (int)(read_f32_le(command + 24u) * 255.0f);
    const int green = (int)(read_f32_le(command + 28u) * 255.0f);
    const int blue = (int)(read_f32_le(command + 32u) * 255.0f);
    return RGB(red, green, blue);
}

static BOOL draw_display_list(
    HDC dc,
    const RECT *surface,
    float opacity,
    wchar_t *diagnostic,
    size_t diagnostic_count) {
    fwui_buffer display = {0};
    fwui_buffer semantics = {0};
    fwui_status status = fwui_render_placeholder(
        g_context, 640.0f, 360.0f, opacity, &display, &semantics);
    uint32_t command_count = 0u;
    uint32_t index = 0u;
    BOOL valid = FALSE;

    if (status != FWUI_STATUS_OK || display.length < FWDL_HEADER_SIZE ||
        memcmp(display.data, "FWDL", 4u) != 0) {
        swprintf_s(diagnostic, diagnostic_count, L"C bridge error: %d", (int)status);
        goto cleanup;
    }
    command_count = read_u32_le(display.data + 8u);
    if (display.length != FWDL_HEADER_SIZE +
        ((uint64_t)command_count * FWDL_COMMAND_SIZE)) {
        swprintf_s(diagnostic, diagnostic_count, L"Invalid DisplayList length");
        goto cleanup;
    }

    for (index = 0u; index < command_count; ++index) {
        const uint8_t *command = display.data + FWDL_HEADER_SIZE +
            ((uint64_t)index * FWDL_COMMAND_SIZE);
        const float x = read_f32_le(command + 4u);
        const float y = read_f32_le(command + 8u);
        const float width = read_f32_le(command + 12u);
        const float height = read_f32_le(command + 16u);
        const float alpha = read_f32_le(command + 36u);
        const float scale_x = (float)(surface->right - surface->left) / 640.0f;
        const float scale_y = (float)(surface->bottom - surface->top) / 360.0f;
        RECT target = {
            surface->left + (LONG)(x * scale_x),
            surface->top + (LONG)(y * scale_y),
            surface->left + (LONG)((x + width) * scale_x),
            surface->top + (LONG)((y + height) * scale_y)
        };
        const BYTE byte_alpha = (BYTE)(alpha * 255.0f);
        if (command[0] == 3u) {
            HPEN pen = CreatePen(PS_SOLID, 2, command_color(command));
            HGDIOBJ previous_pen = SelectObject(dc, pen);
            HGDIOBJ previous_brush = SelectObject(dc, GetStockObject(NULL_BRUSH));
            RoundRect(dc, target.left, target.top, target.right, target.bottom, 18, 18);
            SelectObject(dc, previous_brush);
            SelectObject(dc, previous_pen);
            DeleteObject(pen);
        } else {
            alpha_fill_rect(dc, &target, command_color(command), byte_alpha);
        }
    }

    swprintf_s(
        diagnostic,
        diagnostic_count,
        L"ABI v1 | FWDL v1 | %u commands | %llu semantics bytes",
        command_count,
        (unsigned long long)semantics.length);
    valid = TRUE;

cleanup:
    fwui_buffer_release(&display);
    fwui_buffer_release(&semantics);
    return valid;
}

static void paint_window(HWND window) {
    PAINTSTRUCT paint = {0};
    HDC dc = BeginPaint(window, &paint);
    RECT client = {0};
    RECT preview = {24, 82, 930, 610};
    const int opacity_percent = (int)SendMessageW(g_opacity_slider, TBM_GETPOS, 0, 0);
    const int distance_percent = (int)SendMessageW(g_distance_slider, TBM_GETPOS, 0, 0);
    const BOOL fallback = SendMessageW(g_fallback_checkbox, BM_GETCHECK, 0, 0) == BST_CHECKED;
    const float scale = 1.0f - ((float)distance_percent / 100.0f * 0.42f);
    const int surface_width = (int)(640.0f * scale);
    const int surface_height = (int)(360.0f * scale);
    RECT surface = {
        preview.left + ((preview.right - preview.left - surface_width) / 2),
        preview.top + ((preview.bottom - preview.top - surface_height) / 2),
        0,
        0
    };
    wchar_t status[192] = {0};
    wchar_t values[192] = {0};
    GetClientRect(window, &client);
    surface.right = surface.left + surface_width;
    surface.bottom = surface.top + surface_height;

    fill_rect(dc, &client, RGB(14, 19, 29));
    fill_rect(dc, &preview, RGB(19, 27, 40));
    draw_room_grid(dc, &preview);
    draw_text_line(dc, 24, 20, RGB(235, 241, 255), L"visionOS SpatialSurface v0.1 - Windows contract simulator");
    draw_text_line(dc, 24, 48, RGB(151, 169, 198), L"Shared data and fallback validation; this is not a Vision Pro emulator.");

    draw_display_list(dc, &surface, (float)opacity_percent / 100.0f,
        status, sizeof(status) / sizeof(status[0]));
    draw_text_line(dc, 42, 632, RGB(210, 221, 242), L"Surface opacity");
    draw_text_line(dc, 402, 632, RGB(210, 221, 242), L"Simulated distance");
    draw_text_line(dc, 24, client.bottom - 34, RGB(126, 216, 165), status);
    swprintf_s(
        values,
        sizeof(values) / sizeof(values[0]),
        L"opacity=%d%%  distance=%d%%  presentation=%s",
        opacity_percent,
        distance_percent,
        fallback ? L"window fallback" : L"volume projection");
    draw_text_line(dc, 24, client.bottom - 58, RGB(235, 241, 255), values);
    EndPaint(window, &paint);
}

static LRESULT CALLBACK window_proc(HWND window, UINT message, WPARAM wparam, LPARAM lparam) {
    (void)wparam;
    (void)lparam;
    switch (message) {
        case WM_CREATE:
            g_opacity_slider = CreateWindowW(
                TRACKBAR_CLASSW, L"", WS_CHILD | WS_VISIBLE | TBS_AUTOTICKS,
                38, 654, 300, 34, window, (HMENU)(INT_PTR)CONTROL_OPACITY,
                GetModuleHandleW(NULL), NULL);
            SendMessageW(g_opacity_slider, TBM_SETRANGE, TRUE, MAKELPARAM(0, 100));
            SendMessageW(g_opacity_slider, TBM_SETPOS, TRUE, 75);
            g_distance_slider = CreateWindowW(
                TRACKBAR_CLASSW, L"", WS_CHILD | WS_VISIBLE | TBS_AUTOTICKS,
                398, 654, 300, 34, window, (HMENU)(INT_PTR)CONTROL_DISTANCE,
                GetModuleHandleW(NULL), NULL);
            SendMessageW(g_distance_slider, TBM_SETRANGE, TRUE, MAKELPARAM(0, 100));
            SendMessageW(g_distance_slider, TBM_SETPOS, TRUE, 35);
            g_fallback_checkbox = CreateWindowW(
                L"BUTTON", L"Force 2D fallback", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
                735, 654, 190, 28, window, (HMENU)(INT_PTR)CONTROL_FALLBACK,
                GetModuleHandleW(NULL), NULL);
            return 0;
        case WM_HSCROLL:
        case WM_COMMAND:
            InvalidateRect(window, NULL, FALSE);
            return 0;
        case WM_PAINT:
            paint_window(window);
            return 0;
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
        default:
            return DefWindowProcW(window, message, wparam, lparam);
    }
}

int WINAPI wWinMain(
    HINSTANCE instance,
    HINSTANCE previous_instance,
    PWSTR command_line,
    int show_command) {
    static const wchar_t class_name[] = L"FacetWireVisionOSWindowsSpike";
    INITCOMMONCONTROLSEX controls = {sizeof(controls), ICC_BAR_CLASSES};
    WNDCLASSEXW window_class = {0};
    HWND window = NULL;
    MSG message = {0};
    (void)previous_instance;
    (void)command_line;

    if (!InitCommonControlsEx(&controls) ||
        fwui_context_create(&g_context) != FWUI_STATUS_OK) {
        return 1;
    }

    window_class.cbSize = sizeof(window_class);
    window_class.lpfnWndProc = window_proc;
    window_class.hInstance = instance;
    window_class.hCursor = LoadCursorW(NULL, MAKEINTRESOURCEW(32512));
    window_class.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    window_class.lpszClassName = class_name;
    if (RegisterClassExW(&window_class) == 0) {
        fwui_context_destroy(g_context);
        return 2;
    }

    window = CreateWindowExW(
        0,
        class_name,
        L"FacetWire visionOS Host Spike - Windows Simulator",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        970,
        780,
        NULL,
        NULL,
        instance,
        NULL);
    if (window == NULL) {
        fwui_context_destroy(g_context);
        return 3;
    }
    ShowWindow(window, show_command);
    UpdateWindow(window);
    while (GetMessageW(&message, NULL, 0, 0) > 0) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
    fwui_context_destroy(g_context);
    g_context = NULL;
    return (int)message.wParam;
}
