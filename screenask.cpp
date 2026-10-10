#define WIN32_LEAN_AND_MEAN
#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif

#include <windows.h>
#include <windowsx.h>
#include <shellapi.h>
#include <shlobj.h>
#include <winhttp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <math.h>
#include <wchar.h>
#include <wctype.h>
#include <string>
#include "WebView2.h"


/* minimal gdiplus flat-api declarations so this stays plain c */
typedef struct GpImage GpImage;
typedef struct GpBitmap GpBitmap;
typedef int GpStatus;
typedef void *DebugEventProc;
typedef struct {
    UINT GdiplusVersion;
    DebugEventProc DebugEventCallback;
    BOOL SuppressBackgroundThread;
    BOOL SuppressExternalCodecs;
} GdiplusStartupInput;

extern "C" {
    extern GpStatus WINAPI GdiplusStartup(ULONG_PTR *token, const GdiplusStartupInput *input, void *output);
    extern void WINAPI GdiplusShutdown(ULONG_PTR token);
    extern GpStatus WINAPI GdipCreateBitmapFromHBITMAP(HBITMAP hbm, HPALETTE hpal, GpBitmap **bitmap);
    extern GpStatus WINAPI GdipSaveImageToFile(GpImage *image, const WCHAR *filename, const CLSID *clsidEncoder, void *encoderParams);
    extern GpStatus WINAPI GdipDisposeImage(GpImage *image);
extern GpStatus WINAPI GdipLoadImageFromFile(const WCHAR *filename, GpImage **image);
extern GpStatus WINAPI GdipGetImageWidth(GpImage *image, UINT *width);
extern GpStatus WINAPI GdipGetImageHeight(GpImage *image, UINT *height);
typedef struct GpGraphics GpGraphics;
extern GpStatus WINAPI GdipCreateFromHDC(HDC hdc, GpGraphics **graphics);
extern GpStatus WINAPI GdipDeleteGraphics(GpGraphics *graphics);
extern GpStatus WINAPI GdipDrawImageRectI(GpGraphics *graphics, GpImage *image, INT x, INT y, INT width, INT height);
}

#define APP_CLASS L"ScreenAskFloatingBar"
#define APP_TITLE L"screenask"
#define HOTKEY_ID 1
#define ESC_HOTKEY_ID 2
#define TIMER_ID 1001
#define CONTROL_OVERLAY_CLASS L"ScreenAskControlOverlay"
#define WM_ASK_DONE (WM_APP + 10)
#define WM_ASK_FAIL (WM_APP + 11)
#define WM_SUBMIT (WM_APP + 20)
#define WM_HIDE_BAR (WM_APP + 21)
#define WM_SETTINGS (WM_APP + 22)
#define WM_TEXT_CHANGED (WM_APP + 23)
#define WM_SEARCH_READY (WM_APP + 32)
#define WM_OPEN_SEARCH_BROWSER (WM_APP + 33)
#define WM_TASK_STATUS (WM_APP + 30)
#define SETTINGS_SAVE_ID 102
#define SETTINGS_CANCEL_ID 103
#define SETTINGS_CLOSE_ID 105
#define BROWSER_BTN_ID 104
#ifndef WDA_EXCLUDEFROMCAPTURE
#define WDA_EXCLUDEFROMCAPTURE 0x00000011
#endif

typedef enum {
    SEARCH_IDLE = 0,
    SEARCH_LOADING = 1,
    SEARCH_ACTIVE = 2
} search_state_t;

#define OLLAMA_HOST L"127.0.0.1"
#define OLLAMA_PORT 11434
#define OLLAMA_MODEL L"hf.co/ggml-org/SmolVLM-500M-Instruct-GGUF:Q8_0"
#define GROQ_MODEL L"qwen/qwen3.8-27b"
#define REQUEST_TIMEOUT 300000

static HWND g_hwnd = NULL;
static HWND g_edit = NULL;
static HWND g_browser_button = NULL;
static wchar_t g_search_url[8192] = {0};
static HWND g_arrow_l = NULL;
static HWND g_arrow_r = NULL;
static HFONT g_font = NULL;
static HBRUSH g_edit_brush = NULL;
static HBRUSH g_settings_brush = NULL;
static HBRUSH g_settings_edit_brush = NULL;
static HBITMAP g_preview = NULL;
static WNDPROC g_old_edit_proc = NULL;
static WNDPROC g_old_browser_button_proc = NULL;
static int g_browser_button_pressed = 0;
static wchar_t g_image_path[MAX_PATH] = {0};
static int g_bar_w = 810;
static int g_window_w = 810;
static int g_bar_x = 0;
static int g_bar_h = 54;
static float g_bar_h_float = 54.0f;
static float g_bar_h_v = 0.0f;
static int g_bar_target_h = 54;
static int g_visible = 0;
static int g_busy = 0;
static int g_screenshot_enabled = 1;
static float g_arrow_l_x = -100.0f;
static float g_arrow_r_x = 1000.0f;
static float g_arrow_l_v = 30.0f;
static float g_arrow_r_v = -30.0f;
static float g_arrow_l_bounce = 0.0f;
static float g_arrow_r_bounce = 0.0f;
static float g_arrow_l_bounce_v = 0.0f;
static float g_arrow_r_bounce_v = 0.0f;
static int g_arrow_phase = 0;
static HFONT g_arrow_font = NULL;
static ULONG_PTR g_gdiplus_token = 0;
static ULONGLONG g_request_start = 0;
static int g_throbber_frame = 0;
static wchar_t g_groq_key[512] = {0};
static HWND g_settings = NULL;
static HWND g_settings_key = NULL;
static int run_direct_shortcut(const char *p);
#define MAX_SLASH_COMMANDS 8
static HWND g_settings_cmd[MAX_SLASH_COMMANDS] = {0};
static HWND g_settings_action[MAX_SLASH_COMMANDS] = {0};
static char g_slash_cmd[MAX_SLASH_COMMANDS][32] = {
    "/google", "/youtube", "/yt", "/run", "/open", "/music", "/ask", "/sys"
};
static char g_slash_action[MAX_SLASH_COMMANDS][32] = {
    "google", "youtube", "youtube", "run", "open", "music", "ask", "run"
};
static ICoreWebView2Environment *g_webview_env = NULL;
static ICoreWebView2Controller *g_webview_controller = NULL;
static ICoreWebView2 *g_webview = NULL;
static HMODULE g_webview_loader = NULL;
/* Use a disposable WebView2 profile instead of the app's/default Edge profile. */
static wchar_t g_webview_temp_profile[MAX_PATH] = {0};

static EventRegistrationToken g_nav_token = {};
static int g_webview_ready = 0;
static int g_task_active = 0;
static search_state_t g_search_state = SEARCH_IDLE;
static volatile LONG g_task_stop = 0;
static int g_task_step = 0;
static int g_task_max_steps = 40;
static char g_task_last_action[2048] = {0};
static HWND g_task_target_window = NULL;
static char *g_task_prompt = NULL;
static HWND g_control_overlay = NULL;
static int g_control_overlay_alpha = 0;
static int g_control_overlay_pulse = 0;
static int g_arrow_target_x = 0;
static int g_arrow_target_y = 0;
static int g_arrow_target_valid = 0;
static ULONGLONG g_task_last_action_ms = 0;
static int g_task_last_action_x = -1;
static int g_task_last_action_y = -1;
static char g_task_last_action_type[32] = {0};


/* integrated glass bar assets: rendered by the existing ScreenAsk window */
#define GLASS_BAR_W 810
#define GLASS_BAR_H 54
#define UI_HIT_SEARCH 1
#define UI_HIT_CAMERA 2
#define UI_HIT_SLIDER 3
#define UI_HIT_SETTINGS 4

#include "glassbar.h"

static HBITMAP g_glass_bitmap = NULL;
static HDC g_glass_dc = NULL;
static BYTE *g_glass_pixels = NULL;
static BYTE *g_glass_raw = NULL;
static BYTE *g_glass_scratch = NULL;
static BOOL g_bar_focused = FALSE;
static BOOL g_slider_dragging = FALSE;
static BOOL g_webview_resizing = FALSE;
static POINT g_webview_resize_start = {0, 0};
static int g_webview_resize_start_w = 810;
static int g_webview_resize_start_h = 620;
static int g_webview_focused_height = 620;
static int g_swallow_next_enter_char = 0;
static int g_reasoning_value = 0; /* 0 = instant by default; moving right enables thinking */
static int g_ui_hover_hit = 0;
static float g_hover_x = 30.0f;
static float g_hover_x_velocity = 0.0f;
static float g_hover_y_offset = 0.0f;
static float g_hover_y_velocity = 0.0f;
static int g_hover_bounce_tick = 0;
static ULONGLONG g_last_blur_frame = 0;
static HFONT g_slider_label_font = NULL;
static int g_glass_assets_ok = 0;
static volatile LONG g_glass_capture_busy = 0;
static int g_glass_w = GLASS_BAR_W;
static int g_glass_h = GLASS_BAR_H;
static float g_bar_w_float = 810.0f;
static float g_bar_w_velocity = 0.0f;
static int g_bar_w_target = 810;

static int create_glass_surface(void) {
    BITMAPINFO bmi;
    ZeroMemory(&bmi, sizeof(bmi));
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    /* One reusable surface large enough for the full expanded response panel. */
    bmi.bmiHeader.biWidth = GLASS_BAR_W;
    bmi.bmiHeader.biHeight = -700;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;
    g_glass_bitmap = CreateDIBSection(NULL, &bmi, DIB_RGB_COLORS,
                                      (void **)&g_glass_pixels, NULL, 0);
    if (!g_glass_bitmap || !g_glass_pixels) return 0;
    g_glass_dc = CreateCompatibleDC(NULL);
    if (!g_glass_dc) return 0;
    SelectObject(g_glass_dc, g_glass_bitmap);
    g_glass_raw = (BYTE *)malloc((size_t)GLASS_BAR_W * 700 * 4);
    g_glass_scratch = (BYTE *)malloc((size_t)GLASS_BAR_W * 700 * 4);
    if (!g_glass_raw || !g_glass_scratch) return 0;
    return 1;
}

static void destroy_glass_surface(void) {
    if (g_glass_dc) DeleteDC(g_glass_dc);
    if (g_glass_bitmap) DeleteObject(g_glass_bitmap);
    free(g_glass_raw); free(g_glass_scratch);
    g_glass_dc = NULL; g_glass_bitmap = NULL; g_glass_pixels = NULL;
    g_glass_raw = NULL; g_glass_scratch = NULL;
}

static int clamp_int(int x, int lo, int hi) {
    if (x < lo) return lo;
    if (x > hi) return hi;
    return x;
}

/* fast separable box blur; no per-pixel 7x7/9x9 nested loops */
static void blur_glass_pixels(void) {
    const int radius = 5;
    const int span = radius * 2 + 1;
    int x, y, c, k;
    const int w = g_glass_w;
    const int h = g_glass_h;
    if (w < 1 || h < 1 || w > GLASS_BAR_W || h > 700) return;
    memcpy(g_glass_raw, g_glass_pixels, (size_t)w * h * 4);
    for (y = 0; y < h; ++y) {
        for (c = 0; c < 3; ++c) {
            unsigned sum = 0;
            for (k = -radius; k <= radius; ++k)
                sum += g_glass_raw[((size_t)y * w + clamp_int(k, 0, w - 1)) * 4 + c];
            for (x = 0; x < w; ++x) {
                g_glass_scratch[((size_t)y * w + x) * 4 + c] = (BYTE)(sum / span);
                sum -= g_glass_raw[((size_t)y * w + clamp_int(x - radius, 0, w - 1)) * 4 + c];
                sum += g_glass_raw[((size_t)y * w + clamp_int(x + radius + 1, 0, w - 1)) * 4 + c];
            }
        }
    }
    for (x = 0; x < w; ++x) {
        for (c = 0; c < 3; ++c) {
            unsigned sum = 0;
            for (k = -radius; k <= radius; ++k)
                sum += g_glass_scratch[((size_t)clamp_int(k, 0, h - 1) * w + x) * 4 + c];
            for (y = 0; y < h; ++y) {
                BYTE base = (BYTE)(sum / span);
                g_glass_pixels[((size_t)y * w + x) * 4 + c] = base;
                sum -= g_glass_scratch[((size_t)clamp_int(y - radius, 0, h - 1) * w + x) * 4 + c];
                sum += g_glass_scratch[((size_t)clamp_int(y + radius + 1, 0, h - 1) * w + x) * 4 + c];
            }
        }
    }
    for (y = 0; y < h; ++y)
        for (x = 0; x < w; ++x)
            g_glass_pixels[((size_t)y * w + x) * 4 + 3] = 255;
}

static void capture_live_glass(HDC target) {
    RECT wr;
    HDC screen;
    ULONGLONG now = GetTickCount64();
    if (!target || !g_hwnd || !IsWindow(g_hwnd) || !g_glass_dc ||
        !g_glass_pixels || !g_glass_raw || !g_glass_scratch || !g_glass_bitmap) return;
    g_glass_w = g_window_w;
    g_glass_h = g_bar_h;
    if (g_glass_w < 1 || g_glass_w > GLASS_BAR_W || g_glass_h < 1 || g_glass_h > 700) return;
    if (InterlockedCompareExchange(&g_glass_capture_busy, 1, 0) != 0) {
        BitBlt(target, 0, 0, g_glass_w, g_glass_h, g_glass_dc, 0, 0, SRCCOPY);
        return;
    }
    if (g_last_blur_frame && now - g_last_blur_frame < 32) {
        BitBlt(target, 0, 0, g_glass_w, g_glass_h, g_glass_dc, 0, 0, SRCCOPY);
        InterlockedExchange(&g_glass_capture_busy, 0);
        return;
    }
    if (!GetWindowRect(g_hwnd, &wr)) {
        InterlockedExchange(&g_glass_capture_busy, 0);
        return;
    }
    screen = GetDC(NULL);
    if (!screen) {
        InterlockedExchange(&g_glass_capture_busy, 0);
        return;
    }
    if (BitBlt(g_glass_dc, 0, 0, g_glass_w, g_glass_h,
               screen, wr.left, wr.top, SRCCOPY | CAPTUREBLT)) {
        ReleaseDC(NULL, screen);
        blur_glass_pixels();
        BitBlt(target, 0, 0, g_glass_w, g_glass_h, g_glass_dc, 0, 0, SRCCOPY);
        g_last_blur_frame = now;
    } else {
        ReleaseDC(NULL, screen);
    }
    InterlockedExchange(&g_glass_capture_busy, 0);
}

/* Prime the backdrop while ScreenAsk is hidden. This makes the first frame
   after every Alt+Space reopen use fresh desktop pixels instead of a cached
   image that might include the previous bar frame. Live refresh resumes after
   the window is visible. */
static BOOL capture_hidden_glass_surface(void) {
    RECT wr;
    HDC screen;
    BOOL copied;
    if (!g_hwnd || !IsWindow(g_hwnd) || IsWindowVisible(g_hwnd) ||
        !g_glass_dc || !g_glass_pixels || !g_glass_raw || !g_glass_scratch ||
        !g_glass_bitmap) return FALSE;

    g_glass_w = g_window_w;
    g_glass_h = g_bar_h;
    if (g_glass_w < 1 || g_glass_w > GLASS_BAR_W ||
        g_glass_h < 1 || g_glass_h > 700) return FALSE;
    if (!GetWindowRect(g_hwnd, &wr)) return FALSE;
    screen = GetDC(NULL);
    if (!screen) return FALSE;
    copied = BitBlt(g_glass_dc, 0, 0, g_glass_w, g_glass_h,
                    screen, wr.left, wr.top, SRCCOPY | CAPTUREBLT);
    ReleaseDC(NULL, screen);
    if (!copied) return FALSE;
    blur_glass_pixels();
    g_last_blur_frame = GetTickCount64();
    return TRUE;
}

static int ui_hit_test(int x, int y) {
    return glassbar_hit_test(x, y);
}

static float ui_hit_center(int hit) {
    switch (hit) {
        case UI_HIT_SEARCH: return 30.0f;
        case UI_HIT_CAMERA: return 601.0f;
        case UI_HIT_SLIDER: return 674.0f;
        case UI_HIT_SETTINGS: return 767.0f;
        default: return g_hover_x;
    }
}

static void ui_set_hover(int hit) {
    if (hit < 0 || hit > UI_HIT_SETTINGS) hit = 0;
    if (!g_hwnd || !IsWindow(g_hwnd)) return;
    if (hit == g_ui_hover_hit) return;
    g_ui_hover_hit = hit;
    if (hit) {
        g_hover_y_offset = -4.0f;
        g_hover_y_velocity = 0.0f;
        g_hover_bounce_tick = 0;
        if (g_hover_x < 0.0f || g_hover_x > (float)GLASS_BAR_W)
            g_hover_x = ui_hit_center(hit);
    }
    InvalidateRect(g_hwnd, NULL, FALSE);
}

static void update_ui_spring(void) {
    if (!g_hwnd || !IsWindow(g_hwnd) || !g_visible) return;
    if (g_ui_hover_hit < UI_HIT_SEARCH || g_ui_hover_hit > UI_HIT_SETTINGS) return;
    {
        float dx = ui_hit_center(g_ui_hover_hit) - g_hover_x;
        g_hover_x_velocity += dx * 0.24f;
        g_hover_x_velocity *= 0.72f;
        g_hover_x += g_hover_x_velocity;
        g_hover_y_velocity += (0.0f - g_hover_y_offset) * 0.24f;
        g_hover_y_velocity *= 0.72f;
        g_hover_y_offset += g_hover_y_velocity;
        if (fabsf(dx) < 0.25f && fabsf(g_hover_x_velocity) < 0.25f) {
            g_hover_x = ui_hit_center(g_ui_hover_hit); g_hover_x_velocity = 0.0f;
        }
        if (fabsf(g_hover_y_offset) < 0.25f && fabsf(g_hover_y_velocity) < 0.25f) {
            g_hover_y_offset = 0.0f; g_hover_y_velocity = 0.0f;
            if (++g_hover_bounce_tick >= 44) {
                g_hover_y_offset = -3.2f;
                g_hover_bounce_tick = 0;
            }
        } else {
            g_hover_bounce_tick = 0;
        }
    }
}

static const char *reasoning_guidance(void) {
    if (g_reasoning_value < 34) return "use fast, lightweight reasoning; prioritize the direct answer and minimal output.";
    if (g_reasoning_value < 67) return "use balanced reasoning: check the important details, then answer concisely.";
    return "prioritize accuracy on multi-step requests; check the important details carefully, then give a brief, direct final answer without revealing private chain-of-thought.";
}

static int reasoning_output_budget(void) {
    return 96 + (g_reasoning_value * 4);
}

static wchar_t *utf8_to_wide(const char *src, int len) {
    int n;
    wchar_t *out;
    if (!src) return NULL;
    if (len < 0) len = (int)strlen(src);
    n = MultiByteToWideChar(CP_UTF8, 0, src, len, NULL, 0);
    if (n <= 0) return NULL;
    out = (wchar_t *)calloc((size_t)n + 1, sizeof(wchar_t));
    if (!out) return NULL;
    MultiByteToWideChar(CP_UTF8, 0, src, len, out, n);
    out[n] = L'\0';
    return out;
}

static char *wide_to_utf8(const wchar_t *src) {
    int n;
    char *out;
    if (!src) return NULL;
    n = WideCharToMultiByte(CP_UTF8, 0, src, -1, NULL, 0, NULL, NULL);
    if (n <= 0) return NULL;
    out = (char *)calloc((size_t)n, 1);
    if (!out) return NULL;
    WideCharToMultiByte(CP_UTF8, 0, src, -1, out, n, NULL, NULL);
    return out;
}

static int append_char(char **dst, size_t *len, size_t *cap, char c) {
    char *tmp;
    if (*len + 2 > *cap) {
        size_t newcap = (*cap == 0) ? 256 : (*cap * 2);
        tmp = (char *)realloc(*dst, newcap);
        if (!tmp) return 0;
        *dst = tmp;
        *cap = newcap;
    }
    (*dst)[(*len)++] = c;
    (*dst)[*len] = 0;
    return 1;
}

static int append_utf8_cp(char **dst, size_t *len, size_t *cap, unsigned cp) {
    if (cp <= 0x7f) return append_char(dst, len, cap, (char)cp);
    if (cp <= 0x7ff) {
        if (!append_char(dst, len, cap, (char)(0xc0 | (cp >> 6)))) return 0;
        return append_char(dst, len, cap, (char)(0x80 | (cp & 0x3f)));
    }
    if (cp <= 0xffff) {
        if (!append_char(dst, len, cap, (char)(0xe0 | (cp >> 12)))) return 0;
        if (!append_char(dst, len, cap, (char)(0x80 | ((cp >> 6) & 0x3f)))) return 0;
        return append_char(dst, len, cap, (char)(0x80 | (cp & 0x3f)));
    }
    if (cp <= 0x10ffff) {
        if (!append_char(dst, len, cap, (char)(0xf0 | (cp >> 18)))) return 0;
        if (!append_char(dst, len, cap, (char)(0x80 | ((cp >> 12) & 0x3f)))) return 0;
        if (!append_char(dst, len, cap, (char)(0x80 | ((cp >> 6) & 0x3f)))) return 0;
        return append_char(dst, len, cap, (char)(0x80 | (cp & 0x3f)));
    }
    return append_char(dst, len, cap, '?');
}

static char *json_unescape(const char *src, size_t len) {
    char *out = NULL;
    size_t olen = 0, cap = 0, i = 0;

    while (i < len) {
        char c = src[i++];
        if (c != '\\') {
            if (!append_char(&out, &olen, &cap, c)) goto fail;
            continue;
        }
        if (i >= len) break;
        c = src[i++];
        switch (c) {
            case '"': if (!append_char(&out, &olen, &cap, '"')) goto fail; break;
            case '\\': if (!append_char(&out, &olen, &cap, '\\')) goto fail; break;
            case '/': if (!append_char(&out, &olen, &cap, '/')) goto fail; break;
            case 'b': if (!append_char(&out, &olen, &cap, '\b')) goto fail; break;
            case 'f': if (!append_char(&out, &olen, &cap, '\f')) goto fail; break;
            case 'n': if (!append_char(&out, &olen, &cap, '\n')) goto fail; break;
            case 'r': if (!append_char(&out, &olen, &cap, '\r')) goto fail; break;
            case 't': if (!append_char(&out, &olen, &cap, '\t')) goto fail; break;
            case 'u': {
                unsigned cp = 0;
                int j;
                for (j = 0; j < 4 && i < len; ++j) {
                    char h = src[i++];
                    cp <<= 4;
                    if (h >= '0' && h <= '9') cp += (unsigned)(h - '0');
                    else if (h >= 'a' && h <= 'f') cp += (unsigned)(h - 'a' + 10);
                    else if (h >= 'A' && h <= 'F') cp += (unsigned)(h - 'A' + 10);
                    else cp = 0xfffd;
                }
                if (!append_utf8_cp(&out, &olen, &cap, cp)) goto fail;
                break;
            }
            default:
                if (!append_char(&out, &olen, &cap, c)) goto fail;
                break;
        }
    }

    if (!out) out = _strdup("");
    return out;
fail:
    free(out);
    return NULL;
}

static char *json_escape(const char *src) {
    static const char hex[] = "0123456789abcdef";
    char *out = NULL;
    size_t len = 0, cap = 0, i;

    if (!src) return _strdup("");
    for (i = 0; src[i]; ++i) {
        unsigned char c = (unsigned char)src[i];
        if (c == '"' || c == '\\') {
            if (!append_char(&out, &len, &cap, '\\')) goto fail;
            if (!append_char(&out, &len, &cap, (char)c)) goto fail;
        } else if (c == '\n' || c == '\r' || c == '\t') {
            if (!append_char(&out, &len, &cap, '\\')) goto fail;
            if (!append_char(&out, &len, &cap, c == '\n' ? 'n' : (c == '\r' ? 'r' : 't'))) goto fail;
        } else if (c < 0x20) {
            if (!append_char(&out, &len, &cap, '\\')) goto fail;
            if (!append_char(&out, &len, &cap, 'u')) goto fail;
            if (!append_char(&out, &len, &cap, '0')) goto fail;
            if (!append_char(&out, &len, &cap, '0')) goto fail;
            if (!append_char(&out, &len, &cap, hex[(c >> 4) & 15])) goto fail;
            if (!append_char(&out, &len, &cap, hex[c & 15])) goto fail;
        } else {
            if (!append_char(&out, &len, &cap, (char)c)) goto fail;
        }
    }
    return out ? out : _strdup("");
fail:
    free(out);
    return NULL;
}


static char *wide_to_utf8_alloc(const wchar_t *src) {
    return wide_to_utf8(src);
}

static char *base64_encode(const unsigned char *data, size_t len) {
    static const char table[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    size_t outlen = ((len + 2) / 3) * 4;
    char *out = (char *)malloc(outlen + 1);
    size_t i = 0, o = 0;

    if (!out) return NULL;
    while (i + 3 <= len) {
        unsigned v = ((unsigned)data[i] << 16) | ((unsigned)data[i + 1] << 8) | (unsigned)data[i + 2];
        out[o++] = table[(v >> 18) & 63];
        out[o++] = table[(v >> 12) & 63];
        out[o++] = table[(v >> 6) & 63];
        out[o++] = table[v & 63];
        i += 3;
    }
    if (i < len) {
        unsigned v = (unsigned)data[i] << 16;
        out[o++] = table[(v >> 18) & 63];
        if (i + 1 < len) {
            v |= (unsigned)data[i + 1] << 8;
            out[o++] = table[(v >> 12) & 63];
            out[o++] = table[(v >> 6) & 63];
            out[o++] = '=';
        } else {
            out[o++] = table[(v >> 12) & 63];
            out[o++] = '=';
            out[o++] = '=';
        }
    }
    out[o] = 0;
    return out;
}

static int read_file_bytes(const wchar_t *path, unsigned char **data_out, size_t *len_out) {
    HANDLE h;
    LARGE_INTEGER size;
    unsigned char *buf;
    DWORD read;

    *data_out = NULL;
    *len_out = 0;
    h = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                    NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) return 0;
    if (!GetFileSizeEx(h, &size) || size.QuadPart <= 0 || (uint64_t)size.QuadPart > 0xffffffffULL) {
        CloseHandle(h);
        return 0;
    }
    buf = (unsigned char *)malloc((size_t)size.QuadPart);
    if (!buf) {
        CloseHandle(h);
        return 0;
    }
    if (!ReadFile(h, buf, (DWORD)size.QuadPart, &read, NULL) || read != (DWORD)size.QuadPart) {
        free(buf);
        CloseHandle(h);
        return 0;
    }
    CloseHandle(h);
    *data_out = buf;
    *len_out = (size_t)size.QuadPart;
    return 1;
}

static int save_hbitmap_as_jpeg(HBITMAP hbmp, const wchar_t *path) {
    static const wchar_t jpg_clsid_text[] = L"{557CF401-1A04-11D3-9A73-0000F81EF32E}";
    CLSID jpg_clsid;
    GpBitmap *bitmap = NULL;
    GpStatus st;

    if (!hbmp || !path) return 0;
    /* jpeg encoder clsid: {557CF401-1A04-11D3-9A73-0000F81EF32E} */
    jpg_clsid.Data1 = 0x557CF401;
    jpg_clsid.Data2 = 0x1A04;
    jpg_clsid.Data3 = 0x11D3;
    jpg_clsid.Data4[0] = 0x9A; jpg_clsid.Data4[1] = 0x73;
    jpg_clsid.Data4[2] = 0x00; jpg_clsid.Data4[3] = 0x00;
    jpg_clsid.Data4[4] = 0xF8; jpg_clsid.Data4[5] = 0x1E;
    jpg_clsid.Data4[6] = 0xF3; jpg_clsid.Data4[7] = 0x2E;
    st = GdipCreateBitmapFromHBITMAP(hbmp, NULL, &bitmap);
    if (st != 0 || !bitmap) return 0;
    st = GdipSaveImageToFile((GpImage *)bitmap, path, &jpg_clsid, NULL);
    GdipDisposeImage((GpImage *)bitmap);
    return st == 0;
}

static HBITMAP capture_screen(void) {
    int x = GetSystemMetrics(SM_XVIRTUALSCREEN);
    int y = GetSystemMetrics(SM_YVIRTUALSCREEN);
    int w = GetSystemMetrics(SM_CXVIRTUALSCREEN);
    int h = GetSystemMetrics(SM_CYVIRTUALSCREEN);
    HDC src = GetDC(NULL);
    HDC mem;
    HBITMAP bmp;
    HGDIOBJ old;

    if (!src || w <= 0 || h <= 0) {
        if (src) ReleaseDC(NULL, src);
        return NULL;
    }

    mem = CreateCompatibleDC(src);
    bmp = CreateCompatibleBitmap(src, w, h);
    if (!mem || !bmp) {
        if (mem) DeleteDC(mem);
        if (bmp) DeleteObject(bmp);
        ReleaseDC(NULL, src);
        return NULL;
    }

    old = SelectObject(mem, bmp);
    SetStretchBltMode(mem, COLORONCOLOR);
    BitBlt(mem, 0, 0, w, h, src, x, y, SRCCOPY | CAPTUREBLT);
    SelectObject(mem, old);
    DeleteDC(mem);
    ReleaseDC(NULL, src);
    return bmp;
}

static int save_temp_screenshot(HBITMAP *out_bmp) {
    wchar_t temp_dir[MAX_PATH];
    wchar_t file_path[MAX_PATH];
    DWORD n;
    HBITMAP bmp;

    if (!GetTempPathW(MAX_PATH, temp_dir)) return 0;
    n = GetTempFileNameW(temp_dir, L"sas", 0, file_path);
    if (!n) return 0;
    DeleteFileW(file_path);

    swprintf(g_image_path, MAX_PATH, L"%s%s.jpg", temp_dir, L"screenask-screenshot");
    DeleteFileW(g_image_path);

    bmp = capture_screen();
    if (!bmp) return 0;
    if (!save_hbitmap_as_jpeg(bmp, g_image_path)) {
        DeleteObject(bmp);
        g_image_path[0] = 0;
        return 0;
    }

    if (out_bmp) *out_bmp = bmp;
    else DeleteObject(bmp);
    return 1;
}

static void set_clipboard_text(HWND owner, const wchar_t *text) {
    size_t bytes;
    HGLOBAL mem;
    wchar_t *dst;

    if (!text) return;
    bytes = (wcslen(text) + 1) * sizeof(wchar_t);
    mem = GlobalAlloc(GMEM_MOVEABLE, bytes);
    if (!mem) return;
    dst = (wchar_t *)GlobalLock(mem);
    if (!dst) {
        GlobalFree(mem);
        return;
    }
    memcpy(dst, text, bytes);
    GlobalUnlock(mem);

    if (OpenClipboard(owner)) {
        EmptyClipboard();
        if (!SetClipboardData(CF_UNICODETEXT, mem)) GlobalFree(mem);
        CloseClipboard();
    } else {
        GlobalFree(mem);
    }
}

static char *find_json_string(const char *json, const char *key) {
    char needle[96];
    const char *p;
    const char *start;
    int escaped = 0;
    size_t i;

    if (!json || !key) return NULL;
    if (snprintf(needle, sizeof(needle), "\"%s\"", key) <= 0) return NULL;
    p = strstr(json, needle);
    if (!p) return NULL;
    p += strlen(needle);
    while (*p && (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n' || *p == ':')) ++p;
    if (*p != '"') return NULL;
    ++p;
    start = p;

    for (i = 0; p[i]; ++i) {
        char c = p[i];
        if (!escaped && c == '"') return json_unescape(start, i);
        if (!escaped && c == '\\') escaped = 1;
        else escaped = 0;
    }
    return NULL;
}


static int response_token_limit(const char *prompt, int has_image) {
    size_t n = prompt ? strlen(prompt) : 0;
    if (has_image) return 256;
    if (n > 700) return 512;
    return 256;
}

static const char *g_ai_instruction = "only answer the user. only answers. no markdown, no explanations. be brief and direct.";

static void load_groq_key_from_file(void) {
    wchar_t path[MAX_PATH];
    wchar_t *slash;
    FILE *fp;
    size_t n;

    g_groq_key[0] = 0;
    if (!GetModuleFileNameW(NULL, path, MAX_PATH)) return;
    path[MAX_PATH - 1] = 0;
    slash = wcsrchr(path, L'\\');
    if (!slash) slash = wcsrchr(path, L'/');
    if (!slash) return;
    slash[1] = 0;
    if (wcslen(path) + 8 >= MAX_PATH) return;
    wcscat(path, L"key.txt");

    fp = _wfopen(path, L"r, ccs=UTF-8");
    if (!fp) return;
    if (fgetws(g_groq_key, (int)(sizeof(g_groq_key) / sizeof(g_groq_key[0])), fp)) {
        n = wcslen(g_groq_key);
        while (n > 0 && (g_groq_key[n - 1] == L'\r' || g_groq_key[n - 1] == L'\n' ||
                         g_groq_key[n - 1] == L' ' || g_groq_key[n - 1] == L'\t')) {
            g_groq_key[--n] = 0;
        }
        {
            size_t start = 0;
            while (g_groq_key[start] == L' ' || g_groq_key[start] == L'\t') ++start;
            if (start) memmove(g_groq_key, g_groq_key + start, (wcslen(g_groq_key + start) + 1) * sizeof(wchar_t));
        }
    }
    fclose(fp);
}

static char *groq_request(const char *prompt, const char *image64, DWORD *http_status) {
    HINTERNET session = NULL, connect = NULL, request = NULL;
    char *eprompt = NULL, *key8 = NULL, *body = NULL, *response = NULL;
    size_t body_len, resp_len = 0, resp_cap = 0;
    DWORD status = 0, status_size = sizeof(status), avail = 0, got = 0;
    int ok = 0;

    if (http_status) *http_status = 0;
    if (!g_groq_key[0]) return NULL;

    {
        size_t n = strlen(prompt) + strlen(g_ai_instruction) + 3;
        char *combined = (char *)malloc(n);
        if (!combined) goto cleanup;
        snprintf(combined, n, "%s\n%s", g_ai_instruction, prompt);
        eprompt = json_escape(combined);
        free(combined);
    }

    key8 = wide_to_utf8_alloc(g_groq_key);
    if (!eprompt || !key8) goto cleanup;

    {
        int has_image = image64 && image64[0];
        int max_tokens = response_token_limit(prompt, has_image);
        const char *effort = "none";
        double temperature = 0.7;

        /* The untouched/leftmost position stays in fast instruct mode.
           Any deliberate movement to the right enables Qwen thinking. */
        if (g_reasoning_value > 0) {
            if (g_reasoning_value < 34) effort = "low";
            else if (g_reasoning_value < 67) effort = "medium";
            else effort = "high";
            temperature = 1.0;
        }

        if (has_image) {
            body_len = strlen(eprompt) + strlen(image64) + 1400;
            body = (char *)malloc(body_len);
            if (!body) goto cleanup;
            snprintf(body, body_len,
                "{\"model\":\"%ls\",\"messages\":[{\"role\":\"user\",\"content\":[{\"type\":\"text\",\"text\":\"%s\"},{\"type\":\"image_url\",\"image_url\":{\"url\":\"data:image/jpeg;base64,%s\"}}]}],\"temperature\":%.1f,\"reasoning_effort\":\"%s\",\"max_completion_tokens\":%d,\"stream\":false}",
                GROQ_MODEL, eprompt, image64, temperature, effort, max_tokens);
        } else {
            body_len = strlen(eprompt) + 1000;
            body = (char *)malloc(body_len);
            if (!body) goto cleanup;
            snprintf(body, body_len,
                "{\"model\":\"%ls\",\"messages\":[{\"role\":\"user\",\"content\":\"%s\"}],\"temperature\":%.1f,\"reasoning_effort\":\"%s\",\"max_completion_tokens\":%d,\"stream\":false}",
                GROQ_MODEL, eprompt, temperature, effort, max_tokens);
        }
    }

    session = WinHttpOpen(L"screenask/1.0", WINHTTP_ACCESS_TYPE_NO_PROXY,
                          WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!session) goto cleanup;
    WinHttpSetTimeouts(session, REQUEST_TIMEOUT, REQUEST_TIMEOUT, REQUEST_TIMEOUT, REQUEST_TIMEOUT);

    connect = WinHttpConnect(session, L"api.groq.com", INTERNET_DEFAULT_HTTPS_PORT, 0);
    if (!connect) goto cleanup;

    request = WinHttpOpenRequest(connect, L"POST", L"/openai/v1/chat/completions", NULL,
                                 WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
                                 WINHTTP_FLAG_SECURE | WINHTTP_FLAG_REFRESH);
    if (!request) goto cleanup;
    WinHttpSetTimeouts(request, REQUEST_TIMEOUT, REQUEST_TIMEOUT, REQUEST_TIMEOUT, REQUEST_TIMEOUT);

    {
        wchar_t headers[2048];
        swprintf(headers, 2048,
                 L"Content-Type: application/json\r\nAccept: application/json\r\nAuthorization: Bearer %ls\r\n",
                 g_groq_key);
        if (!WinHttpSendRequest(request, headers, (DWORD)-1L,
                                body, (DWORD)strlen(body), (DWORD)strlen(body), 0)) goto cleanup;
    }

    if (!WinHttpReceiveResponse(request, NULL)) goto cleanup;
    if (WinHttpQueryHeaders(request,
                            WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                            WINHTTP_HEADER_NAME_BY_INDEX,
                            &status, &status_size, WINHTTP_NO_HEADER_INDEX)) {
        if (http_status) *http_status = status;
    }

    for (;;) {
        char *tmp;
        if (!WinHttpQueryDataAvailable(request, &avail)) goto cleanup;
        if (!avail) break;
        if (resp_len + avail + 1 > resp_cap) {
            size_t newcap = resp_cap ? resp_cap * 2 : 8192;
            while (newcap < resp_len + avail + 1) newcap *= 2;
            tmp = (char *)realloc(response, newcap);
            if (!tmp) goto cleanup;
            response = tmp;
            resp_cap = newcap;
        }
        if (!WinHttpReadData(request, response + resp_len, avail, &got)) goto cleanup;
        if (!got) break;
        resp_len += got;
    }

    if (!response) response = _strdup("");
    else response[resp_len] = 0;
    ok = 1;

cleanup:
    if (request) WinHttpCloseHandle(request);
    if (connect) WinHttpCloseHandle(connect);
    if (session) WinHttpCloseHandle(session);
    free(eprompt);
    free(key8);
    free(body);
    if (!ok) {
        free(response);
        response = NULL;
    }
    return response;
}


static char *ollama_request(const char *prompt, const char *image64, DWORD *http_status) {
    HINTERNET session = NULL, connect = NULL, request = NULL;
    char *eprompt = NULL;
    char *body = NULL;
    char *response = NULL;
    size_t body_len;
    size_t resp_len = 0, resp_cap = 0;
    DWORD status = 0, status_size = sizeof(status);
    DWORD avail = 0, got = 0;
    int ok = 0;

    if (http_status) *http_status = 0;

    {
        size_t n = strlen(prompt) + strlen(g_ai_instruction) + strlen(reasoning_guidance()) + 8;
        char *combined = (char *)malloc(n);
        if (!combined) goto cleanup;
        snprintf(combined, n, "%s\n%s\n%s", g_ai_instruction, reasoning_guidance(), prompt);
        eprompt = json_escape(combined);
        free(combined);
    }
    if (!eprompt) goto cleanup;

    body_len = strlen(eprompt) + strlen(image64) + 256;
    body = (char *)malloc(body_len);
    if (!body) goto cleanup;

    if (image64 && image64[0]) {
        snprintf(body, body_len,
                 "{\"model\":\"%ls\",\"prompt\":\"%s\",\"images\":[\"%s\"],\"stream\":false,\"options\":{\"num_predict\":%d}}",
                 OLLAMA_MODEL, eprompt, image64, reasoning_output_budget());
    } else {
        snprintf(body, body_len,
                 "{\"model\":\"%ls\",\"prompt\":\"%s\",\"stream\":false,\"options\":{\"num_predict\":%d}}",
                 OLLAMA_MODEL, eprompt, reasoning_output_budget());
    }

    session = WinHttpOpen(L"screenask/1.0", WINHTTP_ACCESS_TYPE_NO_PROXY,
                          WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!session) goto cleanup;
    WinHttpSetTimeouts(session, REQUEST_TIMEOUT, REQUEST_TIMEOUT, REQUEST_TIMEOUT, REQUEST_TIMEOUT);

    connect = WinHttpConnect(session, OLLAMA_HOST, OLLAMA_PORT, 0);
    if (!connect) goto cleanup;

    request = WinHttpOpenRequest(connect, L"POST", L"/api/generate", NULL,
                                 WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
                                 WINHTTP_FLAG_REFRESH);
    if (!request) goto cleanup;
    WinHttpSetTimeouts(request, REQUEST_TIMEOUT, REQUEST_TIMEOUT, REQUEST_TIMEOUT, REQUEST_TIMEOUT);

    if (!WinHttpSendRequest(request,
                            L"Content-Type: application/json\r\nAccept: application/json\r\n",
                            (DWORD)-1L,
                            body, (DWORD)strlen(body), (DWORD)strlen(body), 0)) goto cleanup;

    if (!WinHttpReceiveResponse(request, NULL)) goto cleanup;

    if (WinHttpQueryHeaders(request,
                            WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                            WINHTTP_HEADER_NAME_BY_INDEX,
                            &status, &status_size, WINHTTP_NO_HEADER_INDEX)) {
        if (http_status) *http_status = status;
    }

    for (;;) {
        char *tmp;
        if (!WinHttpQueryDataAvailable(request, &avail)) goto cleanup;
        if (!avail) break;

        if (resp_len + avail + 1 > resp_cap) {
            size_t newcap = resp_cap ? resp_cap * 2 : 8192;
            while (newcap < resp_len + avail + 1) newcap *= 2;
            tmp = (char *)realloc(response, newcap);
            if (!tmp) goto cleanup;
            response = tmp;
            resp_cap = newcap;
        }
        if (!WinHttpReadData(request, response + resp_len, avail, &got)) goto cleanup;
        if (!got) break;
        resp_len += got;
    }

    if (!response) {
        response = _strdup("");
    } else {
        response[resp_len] = 0;
    }
    ok = 1;

cleanup:
    if (request) WinHttpCloseHandle(request);
    if (connect) WinHttpCloseHandle(connect);
    if (session) WinHttpCloseHandle(session);
    free(eprompt);
    free(body);
    if (!ok) {
        free(response);
        response = NULL;
    }
    return response;
}

typedef struct {
    char *prompt;
    char *image64;
} AskArgs;


static int task_prompt_like(const char *p) {
    static const char *verbs[] = {
        "open ", "close ", "click ", "double click ", "right click ",
        "press ", "type ", "search ", "go to ", "navigate ", "scroll ",
        "switch ", "select ", "drag ", "move ", "enter ", "find ",
        "launch ", "start ", "refresh ", "copy ", "paste ", "download ",
        "create ", "make ", "new ", "rename ", "delete ", "save ",
        "minimize ", "maximize ", "restore ", "show ", "hide ",
        "connect ", "disconnect ", "turn on ", "turn off ", "enable ",
        "disable ", "choose ", "check ", "uncheck ", "run ", "execute ",
        "create file ", "make a file ", "delete file ", "copy file ", "move file ",
        "launch ", "play ", "pause ", "resume ", "mute ", "unmute ", "volume ",
        "open a new tab",
        "open new tab"
    };
    size_t i;
    if (!p || !p[0]) return 0;
    while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') ++p;
    for (i = 0; i < sizeof(verbs) / sizeof(verbs[0]); ++i)
        if (_strnicmp(p, verbs[i], strlen(verbs[i])) == 0) return 1;
    if (strstr(p, " and ") || strstr(p, " then ") || strstr(p, " please ")) return 1;
    return 0;
}

static int json_int_key(const char *json, const char *key, int *out) {
    char needle[96];
    const char *p;
    char *end;
    long v;
    if (!json || !key || !out) return 0;
    snprintf(needle, sizeof(needle), "\"%s\"", key);
    p = strstr(json, needle);
    if (!p) return 0;
    p = strchr(p + strlen(needle), ':');
    if (!p) return 0;
    v = strtol(p + 1, &end, 10);
    if (end == p + 1 || v < -100000 || v > 100000) return 0;
    *out = (int)v;
    return 1;
}

static char *json_key_object(const char *json, const char *key) {
    char needle[96];
    const char *p, *start, *q;
    int depth = 0, in_string = 0, escaped = 0;
    size_t n;
    snprintf(needle, sizeof(needle), "\"%s\"", key);
    p = strstr(json, needle);
    if (!p) return NULL;
    p = strchr(p + strlen(needle), ':');
    if (!p) return NULL;
    while (*++p && (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n')) {}
    if (*p != '{') return NULL;
    start = p; q = p;
    while (*q) {
        char c = *q++;
        if (in_string) {
            if (escaped) escaped = 0;
            else if (c == '\\') escaped = 1;
            else if (c == '"') in_string = 0;
            continue;
        }
        if (c == '"') in_string = 1;
        else if (c == '{') ++depth;
        else if (c == '}' && --depth == 0) break;
    }
    if (depth != 0) return NULL;
    n = (size_t)(q - start);
    {
        char *out = (char *)malloc(n + 1);
        if (!out) return NULL;
        memcpy(out, start, n); out[n] = 0;
        return out;
    }
}

static int json_bool_key(const char *json, const char *key, int *out) {
    char needle[96];
    const char *p;
    snprintf(needle, sizeof(needle), "\"%s\"", key);
    p = strstr(json, needle);
    if (!p) return 0;
    p = strchr(p + strlen(needle), ':');
    if (!p) return 0;
    ++p;
    while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') ++p;
    if (!strncmp(p, "true", 4)) { *out = 1; return 1; }
    if (!strncmp(p, "false", 5)) { *out = 0; return 1; }
    return 0;
}

static char *json_action_string(const char *json, const char *key) {
    char *s = find_json_string(json, key);
    return s;
}

static int action_key_allowed(const char *type) {
    return type && (!strcmp(type, "click") || !strcmp(type, "double_click") ||
                    !strcmp(type, "right_click") || !strcmp(type, "move") ||
                    !strcmp(type, "drag") || !strcmp(type, "scroll") ||
                    !strcmp(type, "type") || !strcmp(type, "keypress") ||
                    !strcmp(type, "wait") || !strcmp(type, "run_command") ||
                    !strcmp(type, "launch_app") || !strcmp(type, "open_file") ||
                    !strcmp(type, "create_file") || !strcmp(type, "delete_file") ||
                    !strcmp(type, "copy_file") || !strcmp(type, "move_file") ||
                    !strcmp(type, "rename_file") || !strcmp(type, "create_folder") ||
                    !strcmp(type, "delete_folder") || !strcmp(type, "file_exists") ||
                    !strcmp(type, "list_folder") || !strcmp(type, "read_file") ||
                    !strcmp(type, "append_file") || !strcmp(type, "media_play_pause") || !strcmp(type, "media_next") ||
                    !strcmp(type, "media_previous") || !strcmp(type, "volume_up") ||
                    !strcmp(type, "volume_down") || !strcmp(type, "mute"));
}

static int is_system_action(const char *type) {
    return type && (!strcmp(type, "run_command") || !strcmp(type, "launch_app") ||
                    !strcmp(type, "open_file") || !strcmp(type, "create_file") ||
                    !strcmp(type, "delete_file") || !strcmp(type, "copy_file") ||
                    !strcmp(type, "move_file") || !strcmp(type, "rename_file") ||
                    !strcmp(type, "create_folder") || !strcmp(type, "delete_folder") ||
                    !strcmp(type, "file_exists") || !strcmp(type, "list_folder") ||
                    !strcmp(type, "read_file") || !strcmp(type, "append_file") || !strcmp(type, "media_play_pause") ||
                    !strcmp(type, "media_next") || !strcmp(type, "media_previous") ||
                    !strcmp(type, "volume_up") || !strcmp(type, "volume_down") ||
                    !strcmp(type, "mute"));
}

static int send_key_vk(WORD vk);
static void post_task_status(const char *msg);
static void resize_response_box(const wchar_t *text);

static int send_media_key(WORD vk) {
    return send_key_vk(vk);
}

static wchar_t *action_wide_string(const char *obj, const char *key) {
    char *s = json_action_string(obj, key);
    wchar_t *w = s ? utf8_to_wide(s, -1) : NULL;
    free(s);
    return w;
}

static int run_shell_command(const wchar_t *command) {
    STARTUPINFOW si;
    PROCESS_INFORMATION pi;
    wchar_t *cmdline;
    size_t n;
    DWORD exit_code = STILL_ACTIVE;
    if (!command || !command[0]) return 0;
    n = wcslen(command) + 32;
    cmdline = (wchar_t *)calloc(n, sizeof(wchar_t));
    if (!cmdline) return 0;
    _snwprintf(cmdline, n, L"cmd.exe /d /c %s", command);
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    ZeroMemory(&pi, sizeof(pi));
    if (!CreateProcessW(NULL, cmdline, NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
        free(cmdline);
        return 0;
    }
    WaitForSingleObject(pi.hProcess, 15000);
    if (GetExitCodeProcess(pi.hProcess, &exit_code) && exit_code == STILL_ACTIVE) {
        CloseHandle(pi.hThread); CloseHandle(pi.hProcess); free(cmdline);
        return 1;
    }
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    free(cmdline);
    return exit_code == 0;
}

static int write_text_file_utf8(const wchar_t *path, const char *text) {
    HANDLE h; DWORD written; int len;
    if (!path || !path[0]) return 0;
    if (!text) text = "";
    h = CreateFileW(path, GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) return 0;
    len = (int)strlen(text);
    if (len == 0) { CloseHandle(h); return 1; }
    if (!WriteFile(h, text, (DWORD)len, &written, NULL) || written != (DWORD)len) {
        CloseHandle(h); return 0;
    }
    CloseHandle(h);
    return 1;
}

static int send_key_vk(WORD vk) {
    INPUT in[2];
    ZeroMemory(in, sizeof(in));
    in[0].type = INPUT_KEYBOARD; in[0].ki.wVk = vk;
    in[1].type = INPUT_KEYBOARD; in[1].ki.wVk = vk; in[1].ki.dwFlags = KEYEVENTF_KEYUP;
    return SendInput(2, in, sizeof(INPUT)) == 2;
}

static WORD key_name_to_vk(const char *key) {
    if (!key) return 0;
    if (!_stricmp(key, "ENTER") || !_stricmp(key, "RETURN")) return VK_RETURN;
    if (!_stricmp(key, "TAB")) return VK_TAB;
    if (!_stricmp(key, "ESC") || !_stricmp(key, "ESCAPE")) return VK_ESCAPE;
    if (!_stricmp(key, "BACKSPACE")) return VK_BACK;
    if (!_stricmp(key, "DELETE") || !_stricmp(key, "DEL")) return VK_DELETE;
    if (!_stricmp(key, "SPACE")) return VK_SPACE;
    if (!_stricmp(key, "UP")) return VK_UP;
    if (!_stricmp(key, "DOWN")) return VK_DOWN;
    if (!_stricmp(key, "LEFT")) return VK_LEFT;
    if (!_stricmp(key, "RIGHT")) return VK_RIGHT;
    if (!_stricmp(key, "HOME")) return VK_HOME;
    if (!_stricmp(key, "END")) return VK_END;
    if (strlen(key) == 1) {
        char c = key[0];
        if (c >= 'a' && c <= 'z') c = (char)(c - 'a' + 'A');
        if ((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')) return (WORD)c;
    }
    if ((key[0] == 'F' || key[0] == 'f') && key[1] >= '1' && key[1] <= '9' && !key[2])
        return (WORD)(VK_F1 + (key[1] - '1'));
    return 0;
}

static int send_hotkey(const char *key) {
    char buf[64];
    char *part;
    WORD mods[3];
    int mod_count = 0;
    WORD vk = 0;
    INPUT in[8];
    int n = 0, i;

    if (!key || !key[0]) return 0;
    strncpy(buf, key, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = 0;

    part = buf;
    while (part && *part) {
        char *next = strchr(part, '+');
        if (next) *next = 0;

        if (!_stricmp(part, "CTRL") || !_stricmp(part, "CONTROL")) {
            if (mod_count >= 3) return 0;
            mods[mod_count++] = VK_CONTROL;
        } else if (!_stricmp(part, "ALT")) {
            if (mod_count >= 3) return 0;
            mods[mod_count++] = VK_MENU;
        } else if (!_stricmp(part, "SHIFT")) {
            if (mod_count >= 3) return 0;
            mods[mod_count++] = VK_SHIFT;
        } else if (!_stricmp(part, "WIN") || !_stricmp(part, "WINDOWS")) {
            if (mod_count >= 3) return 0;
            mods[mod_count++] = VK_LWIN;
        } else {
            vk = key_name_to_vk(part);
            if (!vk) return 0;
        }

        if (!next) break;
        part = next + 1;
    }

    /* a hotkey needs at least one modifier and a final key */
    if (!vk || mod_count == 0) return 0;

    ZeroMemory(in, sizeof(in));
    for (i = 0; i < mod_count; ++i) {
        in[n].type = INPUT_KEYBOARD;
        in[n].ki.wVk = mods[i];
        ++n;
    }
    in[n].type = INPUT_KEYBOARD;
    in[n].ki.wVk = vk;
    ++n;
    in[n].type = INPUT_KEYBOARD;
    in[n].ki.wVk = vk;
    in[n].ki.dwFlags = KEYEVENTF_KEYUP;
    ++n;
    for (i = mod_count - 1; i >= 0; --i) {
        in[n].type = INPUT_KEYBOARD;
        in[n].ki.wVk = mods[i];
        in[n].ki.dwFlags = KEYEVENTF_KEYUP;
        ++n;
    }

    return SendInput((UINT)n, in, sizeof(INPUT)) == (UINT)n;
}

static int execute_task_action(const char *obj) {
    char *type = json_action_string(obj, "type");
    int x = 0, y = 0, ms = 350;
    int ok = 0;
    if (!action_key_allowed(type)) { free(type); return 0; }
    json_int_key(obj, "ms", &ms);
    if (ms < 80) ms = 80; if (ms > 5000) ms = 5000;

    if (!strcmp(type, "run_command")) {
        wchar_t *command = action_wide_string(obj, "command");
        ok = run_shell_command(command);
        free(command);
    } else if (!strcmp(type, "launch_app") || !strcmp(type, "open_file")) {
        wchar_t *target = action_wide_string(obj, !strcmp(type, "launch_app") ? "app" : "path");
        if (target && target[0]) {
            HINSTANCE r = ShellExecuteW(NULL, L"open", target, NULL, NULL, SW_SHOWNORMAL);
            ok = ((INT_PTR)r > 32);
        }
        free(target);
    } else if (!strcmp(type, "create_file")) {
        char *content = json_action_string(obj, "content");
        wchar_t *path = action_wide_string(obj, "path");
        ok = write_text_file_utf8(path, content ? content : "");
        free(path); free(content);
    } else if (!strcmp(type, "delete_file")) {
        wchar_t *path = action_wide_string(obj, "path");
        ok = path && DeleteFileW(path);
        free(path);
    } else if (!strcmp(type, "copy_file") || !strcmp(type, "move_file")) {
        wchar_t *src = action_wide_string(obj, "source");
        wchar_t *dst = action_wide_string(obj, "destination");
        if (src && dst) {
            if (!strcmp(type, "copy_file")) ok = CopyFileW(src, dst, FALSE);
            else ok = MoveFileExW(src, dst, MOVEFILE_COPY_ALLOWED | MOVEFILE_REPLACE_EXISTING);
        }
        free(src); free(dst);
    } else if (!strcmp(type, "create_folder")) {
        wchar_t *path = action_wide_string(obj, "path");
        ok = path && CreateDirectoryW(path, NULL);
        free(path);
    } else if (!strcmp(type, "delete_folder")) {
        wchar_t *path = action_wide_string(obj, "path");
        ok = path && RemoveDirectoryW(path);
        free(path);
    } else if (!strcmp(type, "file_exists")) {
        wchar_t *path = action_wide_string(obj, "path");
        DWORD a = path ? GetFileAttributesW(path) : INVALID_FILE_ATTRIBUTES;
        ok = (a != INVALID_FILE_ATTRIBUTES);
        free(path);
    } else if (!strcmp(type, "list_folder")) {
        wchar_t *path = action_wide_string(obj, "path");
        wchar_t pattern[32768];
        WIN32_FIND_DATAW fd; HANDLE h; int count = 0;
        if (path && path[0]) {
            _snwprintf(pattern, sizeof(pattern)/sizeof(pattern[0]), L"%s\\*", path);
            h = FindFirstFileW(pattern, &fd);
            if (h != INVALID_HANDLE_VALUE) {
                do { if (wcscmp(fd.cFileName, L".") && wcscmp(fd.cFileName, L"..")) ++count; } while (FindNextFileW(h, &fd));
                FindClose(h); ok = 1;
            }
        }
        free(path);
        if (ok) { char msg[128]; snprintf(msg, sizeof(msg), "screenask: folder contains %d item(s)", count); post_task_status(msg); }
    } else if (!strcmp(type, "read_file")) {
        wchar_t *path = action_wide_string(obj, "path");
        unsigned char *data = NULL; size_t len = 0;
        if (path && read_file_bytes(path, &data, &len)) {
            size_t show = len < 1200 ? len : 1200;
            char *out = (char *)malloc(show + 1);
            if (out) { memcpy(out, data, show); out[show] = 0; post_task_status(out); free(out); ok = 1; }
            free(data);
        }
        free(path);
    } else if (!strcmp(type, "append_file")) {
        wchar_t *path = action_wide_string(obj, "path");
        char *content = json_action_string(obj, "content");
        HANDLE h = path ? CreateFileW(path, FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL) : INVALID_HANDLE_VALUE;
        if (h != INVALID_HANDLE_VALUE) {
            DWORD bytes = 0;
            if (content) ok = WriteFile(h, content, (DWORD)strlen(content), &bytes, NULL);
            CloseHandle(h);
        }
        free(path); free(content);
    } else if (!strcmp(type, "media_play_pause")) {
        ok = send_media_key(VK_MEDIA_PLAY_PAUSE);
    } else if (!strcmp(type, "media_next")) {
        ok = send_media_key(VK_MEDIA_NEXT_TRACK);
    } else if (!strcmp(type, "media_previous")) {
        ok = send_media_key(VK_MEDIA_PREV_TRACK);
    } else if (!strcmp(type, "volume_up")) {
        ok = send_media_key(VK_VOLUME_UP);
    } else if (!strcmp(type, "volume_down")) {
        ok = send_media_key(VK_VOLUME_DOWN);
    } else if (!strcmp(type, "mute")) {
        ok = send_media_key(VK_VOLUME_MUTE);
    } else if (!strcmp(type, "click") || !strcmp(type, "double_click") ||
               !strcmp(type, "right_click") || !strcmp(type, "move") || !strcmp(type, "drag")) {
        int sx = GetSystemMetrics(SM_XVIRTUALSCREEN), sy = GetSystemMetrics(SM_YVIRTUALSCREEN);
        int sw = GetSystemMetrics(SM_CXVIRTUALSCREEN), sh = GetSystemMetrics(SM_CYVIRTUALSCREEN);
        int x2 = 0, y2 = 0;
        if (!json_int_key(obj, "x", &x) || !json_int_key(obj, "y", &y) ||
            x < 0 || x > 1000 || y < 0 || y > 1000 || sw <= 0 || sh <= 0) { free(type); return 0; }
        x = sx + (x * sw) / 1000; y = sy + (y * sh) / 1000;
        SetCursorPos(x, y);
        if (!strcmp(type, "move")) ok = 1;
        else if (!strcmp(type, "drag")) {
            if (!json_int_key(obj, "x2", &x2) || !json_int_key(obj, "y2", &y2) ||
                x2 < 0 || x2 > 1000 || y2 < 0 || y2 > 1000) { free(type); return 0; }
            x2 = sx + (x2 * sw) / 1000; y2 = sy + (y2 * sh) / 1000;
            INPUT in; ZeroMemory(&in, sizeof(in)); in.type = INPUT_MOUSE; in.mi.dwFlags = MOUSEEVENTF_LEFTDOWN;
            ok = SendInput(1, &in, sizeof(INPUT)) == 1;
            Sleep(120); SetCursorPos(x2, y2);
            in.mi.dwFlags = MOUSEEVENTF_LEFTUP;
            if (ok) ok = SendInput(1, &in, sizeof(INPUT)) == 1;
        } else {
            INPUT in[4]; int n = 2; ZeroMemory(in, sizeof(in));
            if (!strcmp(type, "right_click")) {
                in[0].type = INPUT_MOUSE; in[0].mi.dwFlags = MOUSEEVENTF_RIGHTDOWN;
                in[1].type = INPUT_MOUSE; in[1].mi.dwFlags = MOUSEEVENTF_RIGHTUP;
            } else {
                in[0].type = INPUT_MOUSE; in[0].mi.dwFlags = MOUSEEVENTF_LEFTDOWN;
                in[1].type = INPUT_MOUSE; in[1].mi.dwFlags = MOUSEEVENTF_LEFTUP;
                if (!strcmp(type, "double_click")) { in[2] = in[0]; in[3] = in[1]; n = 4; }
            }
            ok = SendInput(n, in, sizeof(INPUT)) == (UINT)n;
        }
    } else if (!strcmp(type, "scroll")) {
        int amount = 0; if (!json_int_key(obj, "amount", &amount)) amount = 5;
        if (amount < -20) amount = -20; if (amount > 20) amount = 20;
        INPUT in; ZeroMemory(&in, sizeof(in)); in.type = INPUT_MOUSE;
        in.mi.dwFlags = MOUSEEVENTF_WHEEL; in.mi.mouseData = (DWORD)(amount * WHEEL_DELTA);
        ok = SendInput(1, &in, sizeof(INPUT)) == 1;
    } else if (!strcmp(type, "type")) {
        char *txt = json_action_string(obj, "text");
        if (txt && txt[0]) {
            wchar_t *w = utf8_to_wide(txt, -1);
            if (w) { size_t i; ok = 1;
                for (i = 0; w[i]; ++i) { INPUT in[2]; ZeroMemory(in, sizeof(in));
                    in[0].type = INPUT_KEYBOARD; in[0].ki.wScan = w[i]; in[0].ki.dwFlags = KEYEVENTF_UNICODE;
                    in[1] = in[0]; in[1].ki.dwFlags = KEYEVENTF_UNICODE | KEYEVENTF_KEYUP;
                    if (SendInput(2, in, sizeof(INPUT)) != 2) { ok = 0; break; }
                } free(w);
            }
        } free(txt);
    } else if (!strcmp(type, "keypress")) {
        char *key = json_action_string(obj, "key");
        if (key) ok = send_hotkey(key);
        if (!ok && key) { WORD vk = key_name_to_vk(key); if (vk) ok = send_key_vk(vk); }
        free(key);
    } else if (!strcmp(type, "wait")) { Sleep((DWORD)ms); ok = 1; }
    if (ok && strcmp(type, "wait") && !is_system_action(type)) Sleep((DWORD)ms);
    free(type); return ok;
}

static void post_task_status(const char *msg) {
    if (msg) PostMessageW(g_hwnd, WM_TASK_STATUS, 0, (LPARAM)_strdup(msg));
}

static ULONGLONG g_task_last_api_ms = 0;
static int g_task_rate_retry = 0;

static char *task_api_request(const char *prompt, const char *image64, DWORD *status_out) {
    ULONGLONG now, wait_ms;
    char *raw = NULL;
    DWORD status = 0;
    int attempt;

    for (attempt = 0; attempt < 5 && InterlockedCompareExchange(&g_task_stop,0,0)==0; ++attempt) {
        now = GetTickCount64();
        if (g_task_last_api_ms) {
            ULONGLONG elapsed = now - g_task_last_api_ms;
            if (elapsed < 1500) Sleep((DWORD)(1500 - elapsed));
        }

        if (g_groq_key[0]) raw = groq_request(prompt, image64, &status);
        else raw = ollama_request(prompt, image64, &status);
        g_task_last_api_ms = GetTickCount64();
        if (status_out) *status_out = status;

        if (status != 429) {
            g_task_rate_retry = 0;
            return raw;
        }

        free(raw);
        raw = NULL;
        wait_ms = 2000ULL << attempt;
        if (wait_ms > 30000) wait_ms = 30000;
        ++g_task_rate_retry;
        {
            char msg[128];
            snprintf(msg, sizeof(msg), "screenask: rate limited — retrying in %llus...",
                     (unsigned long long)((wait_ms + 999) / 1000));
            post_task_status(msg);
        }
        while (wait_ms > 0 && InterlockedCompareExchange(&g_task_stop,0,0)==0) {
            DWORD chunk = wait_ms > 250 ? 250 : (DWORD)wait_ms;
            Sleep(chunk);
            wait_ms -= chunk;
        }
    }

    if (status_out) *status_out = 429;
    return NULL;
}

static int task_get_step(const char *prompt, const char *image64, const char *last_action,
                        char **action_out, int *done_out, char **answer_out) {
    char instruction[8192]; char *raw = NULL, *text = NULL, *action = NULL; DWORD status = 0;
    *action_out = NULL; *done_out = 0; *answer_out = NULL;
    snprintf(instruction, sizeof(instruction),
        "you are screenask's general-purpose desktop automation planner. "
        "use the screenshot as the source of truth and complete the user's request through the visible GUI. "
        "take small actions and verify the result in the next screenshot. "
        "return ONLY valid JSON: {\"done\":false,\"answer\":\"\",\"action\":{\"type\":\"click|double_click|right_click|move|drag|scroll|type|keypress|wait|run_command|launch_app|open_file|create_file|delete_file|copy_file|move_file|rename_file|create_folder|delete_folder|file_exists|list_folder|read_file|append_file|media_play_pause|media_next|media_previous|volume_up|volume_down|mute\"}}. "
        "click/double_click/right_click/move require x,y. drag requires x,y,x2,y2. coordinates are normalized 0..1000. "
        "type requires text. keypress requires key such as ENTER, TAB, ESC, CTRL+L, CTRL+T, CTRL+C, CTRL+V, UP, DOWN, LEFT, RIGHT, HOME, END, DELETE. "
        "scroll requires amount: positive up, negative down. wait requires ms. choose exactly one action. "
        "run_command requires command. launch_app requires app. open_file requires path. create_file requires path and optional content. delete_file requires path. copy_file, move_file and rename_file require source and destination. create_folder, delete_folder, file_exists and list_folder require path. read_file requires path. append_file requires path and content. "
        "media_play_pause/media_next/media_previous/volume_up/volume_down/mute need no coordinates. "
        "prefer direct system actions for files, applications, media, volume and ordinary commands instead of clicking through the GUI. "
        "for a direct system action that completely satisfies the request, set done:true and include that action with terminal:true. "
        "use menus, context menus, buttons, links, fields, keyboard shortcuts and scrolling as needed. "
        "do not repeat an action merely because the result is not visually obvious. "
        "a context menu being open is NEVER completion. a New submenu being open is NEVER completion. a menu item merely being highlighted is NEVER completion. "
        "if a menu or submenu is open, continue by choosing the requested item and then verify the resulting screen. "
        "for creating a text file in file explorer, right-click empty space, open New, choose Text Document, wait for it to appear, then locate and open the newly created file. "
        "for a browser request to open a new tab and search, create exactly one new tab, then CTRL+L, type the search, ENTER, and verify the results page. "
        "do not claim done until the visible final result is actually present. if the user asked to open something, the requested application/document must visibly be open. if the user asked to create something, it must visibly exist before completion. "
        "never purchase, send messages, change accounts, download unknown files, or bypass security without confirmation. "
        "user task: %s. last action: %s",
        prompt ? prompt : "", last_action && last_action[0] ? last_action : "none");
    raw = task_api_request(instruction, image64, &status);
    if (!raw || status < 200 || status >= 300) { free(raw); return 0; }
    text = g_groq_key[0] ? find_json_string(raw, "content") : find_json_string(raw, "response"); free(raw);
    if (!text) return 0;
    { char *a = strchr(text, '{'); if (!a) { free(text); return 0; }
      { int done = 0; json_bool_key(a, "done", &done); *done_out = done;
        action = json_key_object(a, "action"); if (action) *action_out = action;
        { char *ans = find_json_string(a, "answer"); *answer_out = ans ? ans : _strdup(done ? "done" : ""); }
      }
    }
    free(text); return 1;
}

static int task_verify_done(const char *prompt, const char *image64) {
    char instruction[6144];
    char *raw = NULL, *text = NULL;
    char *check = NULL;
    DWORD status = 0;
    int yes = 0;

    snprintf(instruction, sizeof(instruction),
        "you are the final visual verifier for a desktop automation task. "
        "look ONLY at the supplied screenshot and decide whether the user's task is visibly complete right now. "
        "return exactly one word: YES or NO. "
        "a context menu, submenu, dialog waiting for a choice, highlighted menu item, or intermediate screen is NEVER complete. "
        "if the task says create and open a file, the file must be visibly opened in the requested application. "
        "if the task says search, the results page must be visible. "
        "if anything still needs to be clicked, typed, selected, opened, or confirmed, answer NO. "
        "user task: %s",
        prompt ? prompt : "");

    raw = task_api_request(instruction, image64, &status);
    if (!raw || status < 200 || status >= 300) {
        free(raw);
        return 0;
    }

    text = g_groq_key[0] ? find_json_string(raw, "content") : find_json_string(raw, "response");
    if (!text) text = _strdup(raw);
    free(raw);
    if (!text) return 0;

    check = text;
    while (*check == ' ' || *check == '\t' || *check == '\r' || *check == '\n' || *check == '"') ++check;
    if (!_strnicmp(check, "YES", 3)) yes = 1;
    free(text);
    return yes;
}


static void update_control_overlay(void);

static void set_action_target(const char *obj) {
    char *type = json_action_string(obj, "type");
    int nx = 500, ny = 500, x2 = 0, y2 = 0;
    int sx = GetSystemMetrics(SM_XVIRTUALSCREEN), sy = GetSystemMetrics(SM_YVIRTUALSCREEN);
    int sw = GetSystemMetrics(SM_CXVIRTUALSCREEN), sh = GetSystemMetrics(SM_CYVIRTUALSCREEN);
    if (!type || sw <= 0 || sh <= 0) {
        free(type);
        return;
    }
    if (json_int_key(obj, "x", &nx) && json_int_key(obj, "y", &ny)) {
        if (!strcmp(type, "drag") && json_int_key(obj, "x2", &x2) && json_int_key(obj, "y2", &y2)) {
            nx = (nx + x2) / 2;
            ny = (ny + y2) / 2;
        }
        if (nx < 0) nx = 0; if (nx > 1000) nx = 1000;
        if (ny < 0) ny = 0; if (ny > 1000) ny = 1000;
        g_arrow_target_x = sx + (nx * sw) / 1000;
        g_arrow_target_y = sy + (ny * sh) / 1000;
        g_arrow_target_valid = 1;
    } else if (g_arrow_target_valid) {
        /* keep the last meaningful target for keyboard/type/wait steps */
    } else {
        g_arrow_target_x = sx + sw / 2;
        g_arrow_target_y = sy + sh / 2;
        g_arrow_target_valid = 1;
    }
    /* make the arrows visibly bounce whenever automation targets something new. */
    g_arrow_l_bounce_v = -10.0f;
    g_arrow_r_bounce_v = 10.0f;
    g_arrow_phase = 0;
    free(type);
}

static void show_control_overlay(void) {
    if (!g_control_overlay) return;
    g_control_overlay_pulse = 0;
    g_control_overlay_alpha = 150;
    SetWindowPos(g_control_overlay, HWND_TOPMOST, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_SHOWWINDOW);
    update_control_overlay();
}

static void hide_control_overlay(void) {
    if (g_control_overlay) ShowWindow(g_control_overlay, SW_HIDE);
    g_control_overlay_alpha = 0;
}

static void update_control_overlay(void) {
    int w, h, x0, y0, x, y;
    HDC screen_dc, mem_dc;
    HBITMAP dib, old_bmp;
    BITMAPINFO bi;
    void *bits = NULL;
    BLENDFUNCTION bf;
    POINT dst, src;
    SIZE size;
    int pulse, max_edge = 150;
    BYTE *px;

    if (!g_control_overlay || !g_task_active) return;
    w = GetSystemMetrics(SM_CXVIRTUALSCREEN);
    h = GetSystemMetrics(SM_CYVIRTUALSCREEN);
    x0 = GetSystemMetrics(SM_XVIRTUALSCREEN);
    y0 = GetSystemMetrics(SM_YVIRTUALSCREEN);
    if (w <= 0 || h <= 0) return;

    pulse = 125 + (int)(45.0 * sin((double)g_control_overlay_pulse * 0.09));
    if (pulse < 75) pulse = 75;
    if (pulse > max_edge) pulse = max_edge;

    ZeroMemory(&bi, sizeof(bi));
    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth = w;
    bi.bmiHeader.biHeight = -h;
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;

    screen_dc = GetDC(NULL);
    mem_dc = CreateCompatibleDC(screen_dc);
    dib = CreateDIBSection(screen_dc, &bi, DIB_RGB_COLORS, &bits, NULL, 0);
    if (!mem_dc || !dib || !bits) {
        if (dib) DeleteObject(dib);
        if (mem_dc) DeleteDC(mem_dc);
        if (screen_dc) ReleaseDC(NULL, screen_dc);
        return;
    }
    old_bmp = (HBITMAP)SelectObject(mem_dc, dib);
    px = (BYTE *)bits;
    for (y = 0; y < h; ++y) {
        int dy = y < h - 1 - y ? y : h - 1 - y;
        for (x = 0; x < w; ++x) {
            int dx = x < w - 1 - x ? x : w - 1 - x;
            int edge = dx < dy ? dx : dy;
            int a = 0;
            if (edge < 95) {
                a = (95 - edge) * pulse / 95;
                if (a > pulse) a = pulse;
            }
            /* premultiplied yellow */
            px[0] = (BYTE)((40 * a) / 255);
            px[1] = (BYTE)((205 * a) / 255);
            px[2] = (BYTE)((255 * a) / 255);
            px[3] = (BYTE)a;
            px += 4;
        }
    }

    dst.x = x0; dst.y = y0;
    src.x = 0; src.y = 0;
    size.cx = w; size.cy = h;
    bf.BlendOp = AC_SRC_OVER;
    bf.BlendFlags = 0;
    bf.SourceConstantAlpha = 255;
    bf.AlphaFormat = AC_SRC_ALPHA;
    UpdateLayeredWindow(g_control_overlay, screen_dc, &dst, &size,
                        mem_dc, &src, 0, &bf, ULW_ALPHA);

    SelectObject(mem_dc, old_bmp);
    DeleteObject(dib);
    DeleteDC(mem_dc);
    ReleaseDC(NULL, screen_dc);
}

static LRESULT CALLBACK control_overlay_proc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    (void)wParam; (void)lParam;
    if (msg == WM_NCHITTEST) return HTTRANSPARENT;
    if (msg == WM_ERASEBKGND) return 1;
    if (msg == WM_PAINT) return 0;
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

static void position_arrow_windows(void);

static DWORD WINAPI task_thread(LPVOID param) {
    char *prompt = (char *)param;
    int step;
    g_task_last_api_ms = 0;
    g_task_rate_retry = 0;
    g_task_last_action_ms = 0;
    g_task_last_action_x = -1;
    g_task_last_action_y = -1;
    g_task_last_action_type[0] = 0;
    for (step = 1; step <= g_task_max_steps && InterlockedCompareExchange(&g_task_stop,0,0)==0; ++step) {
        HBITMAP bmp = NULL; unsigned char *img = NULL; size_t img_len = 0; char *image64 = NULL;
        char *action = NULL, *answer = NULL; int done = 0; int ok;
        g_task_step = step;
        post_task_status("screenask: planning next action...");
        ShowWindow(g_hwnd, SW_HIDE);
        if (g_arrow_l) ShowWindow(g_arrow_l, SW_HIDE);
        if (g_arrow_r) ShowWindow(g_arrow_r, SW_HIDE);
        hide_control_overlay();

        /* restore the window that was underneath screenask before automation */
        if (g_task_target_window && IsWindow(g_task_target_window) &&
            g_task_target_window != g_hwnd) {
            /* do not SW_RESTORE: that would de-maximize a maximized browser. */
            SetForegroundWindow(g_task_target_window);
            BringWindowToTop(g_task_target_window);
        }
        Sleep(350);
        if (!save_temp_screenshot(&bmp)) { post_task_status("screenask: couldn't capture screen"); break; }
        if (bmp) DeleteObject(bmp);
        if (g_arrow_target_valid) {
            show_control_overlay();
            if (g_arrow_l) ShowWindow(g_arrow_l, SW_SHOWNA);
            if (g_arrow_r) ShowWindow(g_arrow_r, SW_SHOWNA);
            position_arrow_windows();
        }
        if (!read_file_bytes(g_image_path, &img, &img_len)) { post_task_status("screenask: couldn't read screenshot"); break; }
        image64 = base64_encode(img, img_len); free(img);
        if (!image64) break;
        ok = task_get_step(prompt, image64, g_task_last_action, &action, &done, &answer);
        if (!ok) { free(image64); post_task_status("screenask: automation planner failed"); break; }
        /* opening a context menu is always an intermediate step.
           never let the planner finish immediately after a right-click. */
        if (done && action) {
            int terminal = 0;
            char *atype = json_action_string(action, "type");
            json_bool_key(action, "terminal", &terminal);
            if (terminal && is_system_action(atype)) {
                int executed = execute_task_action(action);
                if (executed) {
                    if (answer && answer[0]) post_task_status(answer);
                    else post_task_status("screenask: task complete");
                } else {
                    post_task_status("screenask: system action failed");
                }
                free(atype); free(action); free(answer); free(image64);
                break;
            }
            free(atype);
        }
        if (done && g_task_last_action[0]) {
            char *last_type = json_action_string(g_task_last_action, "type");
            if (last_type && !strcmp(last_type, "right_click")) {
                done = 0;
                free(action);
                action = _strdup("{\"type\":\"wait\",\"ms\":500}");
                if (answer) { free(answer); answer = NULL; }
                answer = _strdup("");
            }
            free(last_type);
        }
        if (done) {
            int verified = task_verify_done(prompt, image64);
            if (verified) {
                if (answer && answer[0]) post_task_status(answer); else post_task_status("screenask: task complete");
                free(action); free(answer); free(image64); break;
            }
            free(action); free(answer); free(image64);
            post_task_status("screenask: checking what remains...");
            Sleep(250);
            continue;
        }
        if (!action) { free(answer); free(image64); post_task_status("screenask: invalid action"); break; }
        /* don't repeatedly double-click the same spot while the window is already open.
           this prevents an already-open document from being toggled/minimized. */
        {
            char *atype = json_action_string(action, "type");
            int ax = -1, ay = -1;
            ULONGLONG now = GetTickCount64();
            if (atype && json_int_key(action, "x", &ax) && json_int_key(action, "y", &ay) &&
                !strcmp(atype, "double_click") &&
                !strcmp(g_task_last_action_type, "double_click") &&
                ax == g_task_last_action_x && ay == g_task_last_action_y &&
                now - g_task_last_action_ms < 5000) {
                post_task_status("screenask: waiting for the opened item...");
                Sleep(1200);
                free(atype);
                free(action);
                free(answer);
                free(image64);
                continue;
            }
            free(atype);
        }

        {
            char *system_type = json_action_string(action, "type");
            if (!is_system_action(system_type)) {
                set_action_target(action);
                show_control_overlay();
                if (g_arrow_l) ShowWindow(g_arrow_l, SW_SHOWNA);
                if (g_arrow_r) ShowWindow(g_arrow_r, SW_SHOWNA);
                position_arrow_windows();
            } else {
                post_task_status("screenask: using system tool...");
            }
            free(system_type);
        }
        if (!execute_task_action(action)) { free(action); free(answer); free(image64); post_task_status("screenask: invalid or failed action"); break; }

        /* give UI changes time to settle before the next visual decision. */
        {
            char *atype = json_action_string(action, "type");
            int cooldown = 450;
            if (atype && is_system_action(atype)) cooldown = 120;
            else if (atype && !strcmp(atype, "double_click")) cooldown = 1600;
            else if (atype && !strcmp(atype, "click")) cooldown = 750;
            else if (atype && !strcmp(atype, "right_click")) cooldown = 850;
            else if (atype && !strcmp(atype, "type")) cooldown = 400;
            Sleep(cooldown);
            if (atype && json_int_key(action, "x", &g_task_last_action_x) && json_int_key(action, "y", &g_task_last_action_y)) {
                strncpy(g_task_last_action_type, atype, sizeof(g_task_last_action_type) - 1);
                g_task_last_action_type[sizeof(g_task_last_action_type) - 1] = 0;
                g_task_last_action_ms = GetTickCount64();
            } else {
                g_task_last_action_type[0] = 0;
                g_task_last_action_x = g_task_last_action_y = -1;
            }
            free(atype);
        }

        free(image64);
        strncpy(g_task_last_action, action, sizeof(g_task_last_action) - 1);
        g_task_last_action[sizeof(g_task_last_action) - 1] = 0;
        free(action); free(answer);
    }
    hide_control_overlay();
    if (g_arrow_l) ShowWindow(g_arrow_l, SW_HIDE);
    if (g_arrow_r) ShowWindow(g_arrow_r, SW_HIDE);
    UnregisterHotKey(g_hwnd, ESC_HOTKEY_ID);
    if (step > g_task_max_steps) post_task_status("screenask: task stopped at 40 steps");
    InterlockedExchange(&g_task_stop, 1);
    PostMessageW(g_hwnd, WM_TASK_STATUS, 1, (LPARAM)_strdup("automation finished"));
    free(prompt);
    return 0;
}

static DWORD WINAPI ask_thread(LPVOID param) {
    AskArgs *args = (AskArgs *)param;
    DWORD status = 0;
    char *raw = NULL;
    char *answer = NULL;

    if (g_groq_key[0])
        raw = groq_request(args->prompt, args->image64, &status);
    else
        raw = ollama_request(args->prompt, args->image64, &status);

    if (!raw) {
        char msg[192];
        snprintf(msg, sizeof(msg), "%s request failed (http %lu).",
                 g_groq_key[0] ? "groq" : "ollama", (unsigned long)status);
        PostMessageW(g_hwnd, WM_ASK_FAIL, 0, (LPARAM)_strdup(msg));
    } else if (status < 200 || status >= 300) {
        char *err = find_json_string(raw, "error");
        char *msg;
        if (!err && g_groq_key[0]) err = find_json_string(raw, "message");
        if (err) {
            size_t n = strlen(err) + 32;
            msg = (char *)malloc(n);
            if (msg) snprintf(msg, n, "%s error: %s", g_groq_key[0] ? "groq" : "ollama", err);
            free(err);
        } else {
            msg = _strdup(g_groq_key[0] ? "groq returned an error." : "ollama returned an error.");
        }
        free(raw);
        PostMessageW(g_hwnd, WM_ASK_FAIL, 0, (LPARAM)(msg ? msg : _strdup("ollama returned an error.")));
    } else {
        /* Groq's OpenAI-compatible chat-completions API returns the answer
           in choices[0].message.content, not a top-level "text" field. */
        if (g_groq_key[0])
            answer = find_json_string(raw, "content");
        else
            answer = find_json_string(raw, "response");
        if (!answer) {
            /* Keep the raw response long enough to show a small diagnostic.
               This distinguishes an empty/model-format response from a parser bug. */
            const char *near_choice = raw ? strstr(raw, "\"choices\"") : NULL;
            const char *near_content = raw ? strstr(raw, "\"content\"") : NULL;
            const char *response_snippet = near_choice ? near_choice : (near_content ? near_content : raw);
            char diagnostic[320];
            if (g_groq_key[0]) {
                snprintf(diagnostic, sizeof(diagnostic),
                         "groq answer parse failed (http %lu): %.210s",
                         (unsigned long)status, response_snippet ? response_snippet : "empty response body");
            } else {
                snprintf(diagnostic, sizeof(diagnostic), "ai returned no response.");
            }
            free(raw);
            PostMessageW(g_hwnd, WM_ASK_FAIL, 0, (LPARAM)_strdup(diagnostic));
        } else {
            free(raw);
            PostMessageW(g_hwnd, WM_ASK_DONE, 0, (LPARAM)answer);
        }
    }

    free(args->prompt);
    free(args->image64);
    free(args);
    return 0;
}

static int prompt_has(const char *p, const char *s) {
    size_t n;
    if (!p || !s) return 0;
    n = strlen(s);
    if (!n) return 1;
    for (; *p; ++p) {
        size_t i = 0;
        while (i < n && p[i] &&
               (unsigned char)tolower((unsigned char)p[i]) ==
               (unsigned char)tolower((unsigned char)s[i])) ++i;
        if (i == n) return 1;
    }
    return 0;
}

static void focus_shortcut_target(void) {
    if (g_task_target_window && IsWindow(g_task_target_window) && g_task_target_window != g_hwnd) {
        SetForegroundWindow(g_task_target_window);
        BringWindowToTop(g_task_target_window);
        Sleep(80);
    }
}

static int direct_create_desktop_file(const char *p) {
    const char *q = NULL;
    char name[260] = {0};
    char path8[1024];
    wchar_t wpath[1024];
    wchar_t wcontent[2] = L"";
    wchar_t wdesktop[MAX_PATH];
    char lower[2048];
    size_t li = 0;
    int n = 0;

    if (!p) return 0;
    while (p[li] && li + 1 < sizeof(lower)) {
        lower[li] = (char)tolower((unsigned char)p[li]);
        ++li;
    }
    lower[li] = 0;
    if (!prompt_has(p, "desktop")) return 0;
    if (!(prompt_has(p, "create") || prompt_has(p, "make") || prompt_has(p, "new file"))) return 0;

    q = strstr(lower, "desktop");
    if (!q) return 0;
    q = p;
    while (*q && n < (int)sizeof(name) - 1) {
        if (*q == '\'' || *q == '"') {
            const char quote = *q++;
            int j = 0;
            while (*q && *q != quote && j < (int)sizeof(name) - 1) name[j++] = *q++;
            name[j] = 0;
            if (j > 0) break;
        }
        q++;
    }
    if (!name[0]) {
        const char *a = strstr(lower, "create ");
        if (!a) a = strstr(lower, "make ");
        if (a) {
            a += 6;
            while (*a == ' ' || *a == '\'' || *a == '"') a++;
            n = 0;
            while (*a && !isspace((unsigned char)*a) && *a != '\'' && *a != '"' &&
                   *a != ',' && *a != '.' && n < (int)sizeof(name)-1) name[n++] = *a++;
            name[n] = 0;
        }
    }
    if (!name[0]) {
        const char *a = p;
        while ((a = strstr(a, ".txt")) != NULL) {
            const char *b = a;
            while (b > p && !isspace((unsigned char)b[-1]) && b[-1] != '\'' && b[-1] != '"') --b;
            n = 0;
            while (b < a && n < (int)sizeof(name)-1) name[n++] = *b++;
            name[n] = 0;
            if (name[0]) break;
            ++a;
        }
    }
    if (!name[0]) return 0;
    if (!strchr(name, '.')) strcat(name, ".txt");

    if (SHGetFolderPathW(NULL, CSIDL_DESKTOPDIRECTORY, NULL, SHGFP_TYPE_CURRENT, wdesktop) != S_OK)
        return 0;
    {
        wchar_t full[1024];
        _snwprintf(full, 1024, L"%s\\%hs", wdesktop, name);
        full[1023] = 0;
        return write_text_file_utf8(full, "") ? 1 : 0;
    }
}


static void trim_prompt(char *p) {
    char *start;
    char *end;
    if (!p) return;
    start = p;
    while (*start && isspace((unsigned char)*start)) ++start;
    if (start != p) memmove(p, start, strlen(start) + 1);
    end = p + strlen(p);
    while (end > p && isspace((unsigned char)end[-1])) --end;
    *end = 0;
}


static int starts_with_ci(const char *s, const char *prefix) {
    size_t n;
    if (!s || !prefix) return 0;
    n = strlen(prefix);
    return _strnicmp(s, prefix, n) == 0;
}

static char *url_encode_utf8(const char *s) {
    static const char hex[] = "0123456789ABCDEF";
    size_t n = strlen(s), i, o = 0;
    char *out = (char *)malloc(n * 3 + 1);
    if (!out) return NULL;
    for (i = 0; i < n; ++i) {
        unsigned char c = (unsigned char)s[i];
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
            (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' || c == '~') {
            out[o++] = (char)c;
        } else if (c == ' ') {
            out[o++] = '+';
        } else {
            out[o++] = '%'; out[o++] = hex[c >> 4]; out[o++] = hex[c & 15];
        }
    }
    out[o] = 0;
    return out;
}

static void search_set_url(const wchar_t *url) {
    if (!url) {
        g_search_url[0] = L'\0';
        return;
    }
    wcsncpy(g_search_url, url, (sizeof(g_search_url) / sizeof(g_search_url[0])) - 1);
    g_search_url[(sizeof(g_search_url) / sizeof(g_search_url[0])) - 1] = L'\0';
}

static void search_button_show(int show) {
    if (!g_browser_button) return;
    ShowWindow(g_browser_button, show ? SW_SHOWNOACTIVATE : SW_HIDE);
}

static void position_browser_button(void) {
    RECT r;
    if (!g_hwnd || !g_browser_button) return;
    GetWindowRect(g_hwnd, &r);
    SetWindowPos(g_browser_button, HWND_TOPMOST,
                 r.left + 12, r.bottom + 8, 150, 32,
                 SWP_NOACTIVATE | (g_search_state == SEARCH_IDLE ? SWP_HIDEWINDOW : SWP_SHOWWINDOW));
}

static void open_search_in_browser(void) {
    if (!g_search_url[0] || !g_visible || g_search_state == SEARCH_IDLE) return;
    ShellExecuteW(NULL, L"open", g_search_url, NULL, NULL, SW_SHOWNORMAL);
}

/* Accept browser handoff only from a real mouse release on the visible button.
   This prevents an unrelated/default BN_CLICKED notification from launching
   a second browser when a slash command opens the embedded page. */
static LRESULT CALLBACK browser_button_proc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (!g_old_browser_button_proc) return DefWindowProcW(hwnd, msg, wParam, lParam);
    /* The browser handoff is mouse-only, never a default/keyboard activation. */
    if (msg == WM_KEYDOWN && (wParam == VK_RETURN || wParam == VK_SPACE))
        return 0;
    if (msg == WM_LBUTTONDOWN) {
        g_browser_button_pressed = 1;
        return CallWindowProcW(g_old_browser_button_proc, hwnd, msg, wParam, lParam);
    }
    if (msg == WM_CAPTURECHANGED) {
        g_browser_button_pressed = 0;
        return CallWindowProcW(g_old_browser_button_proc, hwnd, msg, wParam, lParam);
    }
    if (msg == WM_LBUTTONUP) {
        RECT client;
        POINT pt;
        BOOL clicked = FALSE;
        LRESULT result;
        pt.x = GET_X_LPARAM(lParam);
        pt.y = GET_Y_LPARAM(lParam);
        if (g_browser_button_pressed && GetClientRect(hwnd, &client) && PtInRect(&client, pt))
            clicked = TRUE;
        g_browser_button_pressed = 0;
        result = CallWindowProcW(g_old_browser_button_proc, hwnd, msg, wParam, lParam);
        if (clicked && g_hwnd && IsWindow(g_hwnd))
            PostMessageW(g_hwnd, WM_OPEN_SEARCH_BROWSER, 0, 0);
        return result;
    }
    return CallWindowProcW(g_old_browser_button_proc, hwnd, msg, wParam, lParam);
}

static void resize_embedded_webview(void) {
    if (!g_webview_controller || !g_hwnd) return;
    RECT bounds;
    GetClientRect(g_hwnd, &bounds);
    /* Keep a narrow strip outside the WebView at the bottom so the resize
       grip is always above the webpage and can never be covered by it. */
    if (g_search_state != SEARCH_IDLE) {
        bounds.bottom -= 22;
        if (bounds.bottom < 1) bounds.bottom = 1;
    }
    g_webview_controller->put_Bounds(bounds);
}

static void show_embedded_webview(int show) {
    if (!g_webview_controller) return;
    g_webview_controller->put_IsVisible(show ? TRUE : FALSE);
    if (show) {
        resize_embedded_webview();
        SetFocus(g_hwnd);
    }
}

static void close_embedded_webview(void) {
    g_webview_ready = 0;
    if (g_webview_controller) {
        g_webview_controller->Close();
        g_webview_controller->Release();
        g_webview_controller = NULL;
    }
    if (g_webview) {
        g_webview->Release();
        g_webview = NULL;
    }
    if (g_webview_env) {
        g_webview_env->Release();
        g_webview_env = NULL;
    }
    if (g_webview_loader) {
        FreeLibrary(g_webview_loader);
        g_webview_loader = NULL;
    }
}

class screenask_nav_handler : public ICoreWebView2NavigationStartingEventHandler {
    ULONG refs = 1;
public:
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **ppv) override {
        if (!ppv) return E_POINTER;
        *ppv = NULL;
        if (riid == IID_IUnknown || IsEqualGUID(riid, IID_ICoreWebView2NavigationStartingEventHandler)) {
            *ppv = static_cast<ICoreWebView2NavigationStartingEventHandler *>(this);
            AddRef();
            return S_OK;
        }
        return E_NOINTERFACE;
    }
    ULONG STDMETHODCALLTYPE AddRef() override { return ++refs; }
    ULONG STDMETHODCALLTYPE Release() override { ULONG n = --refs; if (!n) delete this; return n; }
    HRESULT STDMETHODCALLTYPE Invoke(ICoreWebView2 *, ICoreWebView2NavigationStartingEventArgs *args) override {
        if (args) {
            LPWSTR uri = NULL;
            if (SUCCEEDED(args->get_Uri(&uri)) && uri) {
                search_set_url(uri);
                CoTaskMemFree(uri);
            }
        }
        return S_OK;
    }
};

class screenask_nav_complete_handler : public ICoreWebView2NavigationCompletedEventHandler {
    ULONG refs = 1;
public:
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **ppv) override {
        if (!ppv) return E_POINTER;
        *ppv = NULL;
        if (riid == IID_IUnknown || IsEqualGUID(riid, IID_ICoreWebView2NavigationCompletedEventHandler)) {
            *ppv = static_cast<ICoreWebView2NavigationCompletedEventHandler *>(this);
            AddRef();
            return S_OK;
        }
        return E_NOINTERFACE;
    }
    ULONG STDMETHODCALLTYPE AddRef() override { return ++refs; }
    ULONG STDMETHODCALLTYPE Release() override { ULONG n = --refs; if (!n) delete this; return n; }
    HRESULT STDMETHODCALLTYPE Invoke(ICoreWebView2 *, ICoreWebView2NavigationCompletedEventArgs *args) override {
        BOOL ok = TRUE;
        if (args) args->get_IsSuccess(&ok);
        if (g_hwnd && IsWindow(g_hwnd)) PostMessageW(g_hwnd, WM_SEARCH_READY, ok ? 0 : 1, 0);
        return S_OK;
    }
};

class screenask_controller_handler : public ICoreWebView2CreateCoreWebView2ControllerCompletedHandler {
    ULONG refs = 1;
public:
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **ppv) override {
        if (!ppv) return E_POINTER;
        *ppv = NULL;
        if (riid == IID_IUnknown || IsEqualGUID(riid, IID_ICoreWebView2CreateCoreWebView2ControllerCompletedHandler)) {
            *ppv = static_cast<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler *>(this);
            AddRef();
            return S_OK;
        }
        return E_NOINTERFACE;
    }
    ULONG STDMETHODCALLTYPE AddRef() override { return ++refs; }
    ULONG STDMETHODCALLTYPE Release() override { ULONG n = --refs; if (!n) delete this; return n; }
    HRESULT STDMETHODCALLTYPE Invoke(HRESULT hr, ICoreWebView2Controller *controller) override {
        if (FAILED(hr) || !controller) {
            g_webview_ready = 0;
            if (g_hwnd) PostMessageW(g_hwnd, WM_SEARCH_READY, 1, 0);
            return S_OK;
        }

        g_webview_controller = controller;
        g_webview_controller->AddRef();

        if (FAILED(g_webview_controller->get_CoreWebView2(&g_webview)) || !g_webview) {
            g_webview_controller->Release();
            g_webview_controller = NULL;
            if (g_hwnd) PostMessageW(g_hwnd, WM_SEARCH_READY, 1, 0);
            return S_OK;
        }

        g_webview_ready = 1;

        ICoreWebView2Settings *settings = NULL;
        if (SUCCEEDED(g_webview->get_Settings(&settings)) && settings) {
            settings->put_IsScriptEnabled(TRUE);
            settings->put_AreDefaultScriptDialogsEnabled(TRUE);
            settings->put_IsWebMessageEnabled(TRUE);
            settings->put_AreDevToolsEnabled(FALSE);
            /* These newer autofill settings are not exposed by every bundled WebView2 SDK.
               Session data is isolated by the temporary user-data profile instead. */
            settings->Release();
        }

        screenask_nav_handler *nav = new screenask_nav_handler();
        g_webview->add_NavigationStarting(nav, &g_nav_token);
        nav->Release();

        screenask_nav_complete_handler *complete = new screenask_nav_complete_handler();
        EventRegistrationToken complete_token = {};
        g_webview->add_NavigationCompleted(complete, &complete_token);
        complete->Release();

        show_embedded_webview(1);

        if (g_search_url[0]) {
            HRESULT nav_hr = g_webview->Navigate(g_search_url);
            if (FAILED(nav_hr) && g_hwnd)
                PostMessageW(g_hwnd, WM_SEARCH_READY, 1, 0);
        }
        return S_OK;
    }
};

class screenask_environment_handler : public ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler {
    ULONG refs = 1;
public:
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **ppv) override {
        if (!ppv) return E_POINTER;
        *ppv = NULL;
        if (riid == IID_IUnknown || IsEqualGUID(riid, IID_ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler)) {
            *ppv = static_cast<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler *>(this);
            AddRef();
            return S_OK;
        }
        return E_NOINTERFACE;
    }
    ULONG STDMETHODCALLTYPE AddRef() override { return ++refs; }
    ULONG STDMETHODCALLTYPE Release() override { ULONG n = --refs; if (!n) delete this; return n; }
    HRESULT STDMETHODCALLTYPE Invoke(HRESULT hr, ICoreWebView2Environment *env) override {
        if (FAILED(hr) || !env) {
            if (g_hwnd) PostMessageW(g_hwnd, WM_SEARCH_READY, 1, 0);
            return S_OK;
        }
        g_webview_env = env;
        g_webview_env->AddRef();

        screenask_controller_handler *controller = new screenask_controller_handler();
        HRESULT create_hr = g_webview_env->CreateCoreWebView2Controller(g_hwnd, controller);
        controller->Release();
        if (FAILED(create_hr) && g_hwnd)
            PostMessageW(g_hwnd, WM_SEARCH_READY, 1, 0);
        return S_OK;
    }
};

static int ensure_webview_environment(void) {
    if (g_webview_env && g_webview_controller && g_webview) return 1;

    if (!g_webview_loader) {
        g_webview_loader = LoadLibraryW(L"WebView2Loader.dll");
        if (!g_webview_loader) return 0;
    }

    typedef HRESULT (STDAPICALLTYPE *create_env_fn)(PCWSTR, PCWSTR, ICoreWebView2EnvironmentOptions *, ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler *);
    create_env_fn fn = (create_env_fn)GetProcAddress(g_webview_loader, "CreateCoreWebView2EnvironmentWithOptions");
    if (!fn) return 0;

    /* Keep cookies, cache, and history out of the user's normal Edge profile.
       This per-process profile is stored under TEMP and removed at shutdown. */
    if (!g_webview_temp_profile[0]) {
        wchar_t temp_path[MAX_PATH];
        DWORD n = GetTempPathW(MAX_PATH, temp_path);
        if (!n || n >= MAX_PATH) return 0;
        if (swprintf(g_webview_temp_profile, MAX_PATH,
                     L"%lsScreenAsk-WebView-%lu", temp_path,
                     (unsigned long)GetCurrentProcessId()) < 0) return 0;
        if (!CreateDirectoryW(g_webview_temp_profile, NULL) &&
            GetLastError() != ERROR_ALREADY_EXISTS) {
            g_webview_temp_profile[0] = 0;
            return 0;
        }
    }

    screenask_environment_handler *handler = new screenask_environment_handler();
    HRESULT hr = fn(NULL, g_webview_temp_profile, NULL, handler);
    handler->Release();
    return SUCCEEDED(hr);
}

static void delete_temp_tree(const wchar_t *path) {
    wchar_t pattern[MAX_PATH * 2];
    WIN32_FIND_DATAW fd;
    HANDLE find;
    if (!path || !path[0]) return;
    swprintf(pattern, sizeof(pattern) / sizeof(pattern[0]), L"%ls\\*", path);
    find = FindFirstFileW(pattern, &fd);
    if (find != INVALID_HANDLE_VALUE) {
        do {
            wchar_t child[MAX_PATH * 2];
            if (!wcscmp(fd.cFileName, L".") || !wcscmp(fd.cFileName, L"..")) continue;
            swprintf(child, sizeof(child) / sizeof(child[0]), L"%ls\\%ls", path, fd.cFileName);
            if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                delete_temp_tree(child);
                RemoveDirectoryW(child);
            } else {
                SetFileAttributesW(child, FILE_ATTRIBUTE_NORMAL);
                DeleteFileW(child);
            }
        } while (FindNextFileW(find, &fd));
        FindClose(find);
    }
}

static void clear_preview(void);

static void search_enter_layout(void) {
    g_search_state = SEARCH_LOADING;
    /* Give the embedded webpage a much taller default viewport. */
    g_bar_target_h = 800;
    g_bar_w = 760;
    g_window_w = 760;
    g_bar_w_float = 760.0f;
    g_bar_w_velocity = 0.0f;
    g_bar_w_target = 760;
    g_bar_h = 54;
    g_bar_h_float = 54.0f;
    g_bar_h_v = 0.0f;
    g_busy = 0;
    g_screenshot_enabled = 0;
    clear_preview();
    g_request_start = GetTickCount64();
    g_throbber_frame = 0;

    if (g_edit) {
        ShowWindow(g_edit, SW_HIDE);
        EnableWindow(g_edit, FALSE);
    }
    if (g_arrow_l) ShowWindow(g_arrow_l, SW_HIDE);
    if (g_arrow_r) ShowWindow(g_arrow_r, SW_HIDE);

    search_button_show(1);
    show_embedded_webview(1);
    resize_embedded_webview();
    position_browser_button();
}

static void search_leave_layout(void) {
    g_search_state = SEARCH_IDLE;
    g_bar_target_h = 54;
    g_bar_h = 54;
    g_bar_h_float = 54.0f;
    g_bar_h_v = 0.0f;
    show_embedded_webview(0);

    if (g_edit) {
        ShowWindow(g_edit, SW_SHOW);
        EnableWindow(g_edit, TRUE);
    }
    search_button_show(0);
}

static void stop_search_webview(void) {
    show_embedded_webview(0);
    search_leave_layout();
}

static int launch_search_webview(const char *url8) {
    wchar_t *url = NULL;
    if (!url8 || !url8[0] || !g_hwnd) return 0;

    url = utf8_to_wide(url8, -1);
    if (!url) return 0;

    stop_search_webview();
    search_set_url(url);
    search_enter_layout();

    if (!ensure_webview_environment()) {
        search_leave_layout();
        free(url);
        return 0;
    }

    if (g_webview) {
        HRESULT hr = g_webview->Navigate(g_search_url);
        if (FAILED(hr)) {
            search_leave_layout();
            free(url);
            return 0;
        }
    }

    free(url);
    return 1;
}

static int search_slash_command(const char *command_name, const char *argument) {
    char *encoded;
    char url[4096];

    if (!command_name || !argument) return 0;

    if (!_stricmp(command_name, "/google")) {
        encoded = url_encode_utf8(argument);
        if (!encoded) return 1;
        snprintf(url, sizeof(url), "https://www.google.com/search?q=%s", encoded);
        free(encoded);

        if (launch_search_webview(url)) {
            post_task_status("screenask: google");
        } else {
            post_task_status("screenask: webview unavailable");
        }
        return 1;
    }

    if (!_stricmp(command_name, "/youtube") || !_stricmp(command_name, "/yt")) {
        encoded = url_encode_utf8(argument);
        if (!encoded) return 1;
        snprintf(url, sizeof(url), "https://www.youtube.com/results?search_query=%s", encoded);
        free(encoded);

        if (launch_search_webview(url)) {
            post_task_status("screenask: youtube");
        } else {
            post_task_status("screenask: webview unavailable");
        }
        return 1;
    }

    /* Open the Windows Settings app directly, just like the dedicated
       search commands above handle their own action instead of falling
       through to the generic shell-command path. */
    if (!_stricmp(command_name, "/sys")) {
        HINSTANCE result = ShellExecuteW(NULL, L"open", L"ms-settings:", NULL, NULL, SW_SHOWNORMAL);
        if ((INT_PTR)result <= 32) {
            post_task_status("screenask: couldn't open windows settings");
        } else {
            post_task_status("screenask: opening windows settings");
        }
        return 1;
    }

    return 0;
}


typedef struct { wchar_t *command; } SlashRunArgs;

static DWORD WINAPI slash_run_thread(LPVOID parameter) {
    SlashRunArgs *args = (SlashRunArgs *)parameter;
    if (args) {
        run_shell_command(args->command);
        free(args->command);
        free(args);
    }
    return 0;
}

typedef struct { wchar_t *name; int found; } FocusWindowArgs;

static int contains_wide_case_insensitive(const wchar_t *text, const wchar_t *needle) {
    size_t n;
    const wchar_t *p;
    if (!text || !needle || !needle[0]) return 0;
    n = wcslen(needle);
    for (p = text; *p; ++p) {
        if (_wcsnicmp(p, needle, n) == 0) return 1;
    }
    return 0;
}

static int open_windows_run_dialog(void) {
    INPUT in[4];
    ZeroMemory(in, sizeof(in));
    in[0].type = INPUT_KEYBOARD;
    in[0].ki.wVk = VK_LWIN;
    in[1].type = INPUT_KEYBOARD;
    in[1].ki.wVk = 'R';
    in[2].type = INPUT_KEYBOARD;
    in[2].ki.wVk = 'R';
    in[2].ki.dwFlags = KEYEVENTF_KEYUP;
    in[3].type = INPUT_KEYBOARD;
    in[3].ki.wVk = VK_LWIN;
    in[3].ki.dwFlags = KEYEVENTF_KEYUP;
    return SendInput(4, in, sizeof(INPUT)) == 4;
}

static BOOL CALLBACK find_window_by_title(HWND hwnd, LPARAM lparam) {
    FocusWindowArgs *args = (FocusWindowArgs *)lparam;
    wchar_t title[512];
    if (!IsWindowVisible(hwnd) || GetWindow(hwnd, GW_OWNER)) return TRUE;
    title[0] = 0;
    GetWindowTextW(hwnd, title, (int)(sizeof(title) / sizeof(title[0])));
    if (title[0] && contains_wide_case_insensitive(title, args->name)) {
        args->found = 1;
        ShowWindow(hwnd, SW_RESTORE);
        ShowWindow(hwnd, SW_MAXIMIZE);
        SetForegroundWindow(hwnd);
        return FALSE;
    }
    return TRUE;
}

static int focus_maximize_window(const char *title_utf8) {
    FocusWindowArgs args;
    wchar_t *wide;
    if (!title_utf8 || !title_utf8[0]) return 0;
    wide = utf8_to_wide(title_utf8, -1);
    if (!wide) return 0;
    args.name = wide;
    args.found = 0;
    EnumWindows(find_window_by_title, (LPARAM)&args);
    free(wide);
    return args.found;
}

static int slash_command(const char *prompt) {
    int i;
    const char *space;
    const char *arg;
    char cmd[64];

    if (!prompt || prompt[0] != '/') return 0;

    /* />window title: accept both />chrome and />[chrome], case-insensitively. */
    if (prompt[1] == '>') {
        const char *title = prompt + 2;
        char title_buf[512];
        size_t len;
        while (*title == ' ' || *title == '\t') ++title;
        len = strlen(title);
        while (len && (title[len - 1] == ' ' || title[len - 1] == '\t')) --len;
        if (len >= 2 && title[0] == '[' && title[len - 1] == ']') {
            ++title;
            len -= 2;
        }
        if (len >= sizeof(title_buf)) len = sizeof(title_buf) - 1;
        memcpy(title_buf, title, len);
        title_buf[len] = '\0';
        if (!title_buf[0]) {
            post_task_status("screenask: usage: />window name (or />[window name])");
        } else if (!focus_maximize_window(title_buf)) {
            post_task_status("screenask: window not found");
        } else {
            post_task_status("screenask: focused window");
        }
        return 1;
    }

    space = strchr(prompt, ' ');
    if (space) {
        size_t n = (size_t)(space - prompt);
        if (n >= sizeof(cmd)) n = sizeof(cmd) - 1;
        memcpy(cmd, prompt, n);
        cmd[n] = '\0';
        arg = space + 1;
        while (*arg == ' ') ++arg;
    } else {
        strncpy(cmd, prompt, sizeof(cmd) - 1);
        cmd[sizeof(cmd) - 1] = '\0';
        arg = "";
    }

    if (!_stricmp(cmd, "/help")) {
        MessageBoxW(g_hwnd,
            L"ScreenAsk commands\n\n"
            L"/help - show this command list\n"
            L"/google <query> - search Google in the WebView\n"
            L"/youtube <query> or /yt <query> - search YouTube\n"
            L"/run <command> - execute the supplied command\n"
            L"/open <app, file, or URL> - open a target\n"
            L"/music <action> - control media or volume\n"
            L"/sys - open Windows Settings\n"
            L"/> <window title> - focus and maximize a matching window\n"
            L"/ask <prompt> - send a prompt to the assistant",
            L"ScreenAsk Help", MB_OK | MB_ICONINFORMATION);
        return 1;
    }

    if (search_slash_command(cmd, arg))
        return 1;

    for (i = 0; i < MAX_SLASH_COMMANDS; ++i) {
        if (!_stricmp(cmd, g_slash_cmd[i])) {
            if (!_stricmp(g_slash_action[i], "run")) {
                if (arg[0]) {
                    SlashRunArgs *args = (SlashRunArgs *)calloc(1, sizeof(*args));
                    if (args) {
                        args->command = utf8_to_wide(arg, -1);
                        if (args->command) {
                            HANDLE thread = CreateThread(NULL, 0, slash_run_thread, args, 0, NULL);
                            if (thread) CloseHandle(thread);
                            else { free(args->command); free(args); post_task_status("screenask: couldn't start command"); }
                        } else free(args);
                    }
                } else {
                    post_task_status("screenask: usage: /run command");
                }
                return 1;
            }
            if (!_stricmp(g_slash_action[i], "open")) {
                if (!arg[0]) {
                    post_task_status("screenask: usage: /open app, file, or url");
                    return 1;
                }
                wchar_t *w = utf8_to_wide(arg, -1);
                if (w) {
                    HINSTANCE result = ShellExecuteW(NULL, L"open", w, NULL, NULL, SW_SHOWNORMAL);
                    if ((INT_PTR)result <= 32)
                        post_task_status("screenask: couldn't open target");
                    free(w);
                } else {
                    post_task_status("screenask: couldn't read target");
                }
                return 1;
            }
            if (!_stricmp(g_slash_action[i], "music")) {
                if (!arg[0]) {
                    post_task_status("screenask: usage: /music action");
                } else if (!run_direct_shortcut(arg)) {
                    post_task_status("screenask: unknown music action");
                }
                return 1;
            }
            if (!_stricmp(g_slash_action[i], "ask")) return 0;
        }
    }

    return 0;
}

static int direct_shortcut_intent(const char *p) {
    if (!p) return 0;

    /* media / volume */
    if (prompt_has(p, "pause music") || prompt_has(p, "pause my music") ||
        prompt_has(p, "pause the music") || prompt_has(p, "play music") ||
        prompt_has(p, "play my music") || prompt_has(p, "resume music") ||
        prompt_has(p, "resume my music") || prompt_has(p, "play/pause music") ||
        prompt_has(p, "pause playback") || prompt_has(p, "playback")) return 1;
    if (prompt_has(p, "next song") || prompt_has(p, "next track") ||
        prompt_has(p, "skip song") || prompt_has(p, "skip track")) return 1;
    if (prompt_has(p, "previous song") || prompt_has(p, "previous track") ||
        prompt_has(p, "last song") || prompt_has(p, "last track")) return 1;
    if (prompt_has(p, "volume up") || prompt_has(p, "turn volume up") ||
        prompt_has(p, "louder") || prompt_has(p, "increase volume")) return 1;
    if (prompt_has(p, "volume down") || prompt_has(p, "turn volume down") ||
        prompt_has(p, "quieter") || prompt_has(p, "decrease volume")) return 1;
    if (prompt_has(p, "mute") || prompt_has(p, "unmute") || prompt_has(p, "toggle mute")) return 1;

    /* desktop file creation is always local and must never enter ai automation.
       accept natural variations such as "create test.txt on my desktop" and
       "create 'test.txt' on my desktop". */
    if (prompt_has(p, "desktop") &&
        (prompt_has(p, "create") || prompt_has(p, "make") ||
         prompt_has(p, "new file") || prompt_has(p, "text file") ||
         prompt_has(p, "file on my desktop"))) return 1;

    /* common keyboard actions: these are instant and do not need ai. */
    if (!_stricmp(p, "copy") || !_stricmp(p, "copy this") ||
        prompt_has(p, "copy selected")) return 1;
    if (!_stricmp(p, "paste") || !_stricmp(p, "paste this")) return 1;
    if (!_stricmp(p, "cut") || !_stricmp(p, "cut this")) return 1;
    if (!_stricmp(p, "select all") || !_stricmp(p, "select everything")) return 1;
    if (!_stricmp(p, "undo") || !_stricmp(p, "undo that")) return 1;
    if (!_stricmp(p, "redo") || !_stricmp(p, "redo that")) return 1;
    if (!_stricmp(p, "save") || !_stricmp(p, "save this")) return 1;
    if (!_stricmp(p, "refresh") || !_stricmp(p, "reload")) return 1;
    if (!_stricmp(p, "new tab") || !_stricmp(p, "open new tab") ||
        !_stricmp(p, "open a new tab") || !_stricmp(p, "new browser tab")) return 1;
    if (!_stricmp(p, "close tab") || !_stricmp(p, "close this tab")) return 1;
    if (!_stricmp(p, "next tab") || !_stricmp(p, "switch to next tab")) return 1;
    if (!_stricmp(p, "previous tab") || !_stricmp(p, "switch to previous tab")) return 1;
    if (!_stricmp(p, "close window") || !_stricmp(p, "close this window")) return 1;
    if (!_stricmp(p, "minimize window") || !_stricmp(p, "minimize this window")) return 1;
    if (!_stricmp(p, "maximize window") || !_stricmp(p, "maximize this window")) return 1;
    if (!_stricmp(p, "focus address bar") || !_stricmp(p, "focus url bar") ||
        !_stricmp(p, "focus search bar")) return 1;
    return 0;
}

static int run_direct_shortcut(const char *p) {
    if (!p) return 0;

    if (direct_create_desktop_file(p)) return 1;

    /* media */
    if (prompt_has(p, "next song") || prompt_has(p, "next track") ||
        prompt_has(p, "skip song") || prompt_has(p, "skip track"))
        return send_media_key(VK_MEDIA_NEXT_TRACK);
    if (prompt_has(p, "previous song") || prompt_has(p, "previous track") ||
        prompt_has(p, "last song") || prompt_has(p, "last track"))
        return send_media_key(VK_MEDIA_PREV_TRACK);
    if (prompt_has(p, "volume up") || prompt_has(p, "turn volume up") ||
        prompt_has(p, "louder") || prompt_has(p, "increase volume"))
        return send_media_key(VK_VOLUME_UP);
    if (prompt_has(p, "volume down") || prompt_has(p, "turn volume down") ||
        prompt_has(p, "quieter") || prompt_has(p, "decrease volume"))
        return send_media_key(VK_VOLUME_DOWN);
    if (prompt_has(p, "mute") || prompt_has(p, "unmute") || prompt_has(p, "toggle mute"))
        return send_media_key(VK_VOLUME_MUTE);
    if (prompt_has(p, "pause music") || prompt_has(p, "pause my music") ||
        prompt_has(p, "pause the music") || prompt_has(p, "play music") ||
        prompt_has(p, "play my music") || prompt_has(p, "resume music") ||
        prompt_has(p, "resume my music") || prompt_has(p, "play/pause music") ||
        prompt_has(p, "pause playback") || prompt_has(p, "playback"))
        return send_media_key(VK_MEDIA_PLAY_PAUSE);

    /* editing / browser / window shortcuts */
    if (!_stricmp(p, "copy") || !_stricmp(p, "copy this") || prompt_has(p, "copy selected")) return send_hotkey("CTRL+C");
    if (!_stricmp(p, "paste") || !_stricmp(p, "paste this")) return send_hotkey("CTRL+V");
    if (!_stricmp(p, "cut") || !_stricmp(p, "cut this")) return send_hotkey("CTRL+X");
    if (!_stricmp(p, "select all") || !_stricmp(p, "select everything")) return send_hotkey("CTRL+A");
    if (!_stricmp(p, "undo") || !_stricmp(p, "undo that")) return send_hotkey("CTRL+Z");
    if (!_stricmp(p, "redo") || !_stricmp(p, "redo that")) return send_hotkey("CTRL+Y");
    if (!_stricmp(p, "save") || !_stricmp(p, "save this")) return send_hotkey("CTRL+S");
    if (!_stricmp(p, "refresh") || !_stricmp(p, "reload")) return send_key_vk(VK_F5);
    if (!_stricmp(p, "new tab") || !_stricmp(p, "open new tab") ||
        !_stricmp(p, "open a new tab") || !_stricmp(p, "new browser tab")) {
        focus_shortcut_target();
        Sleep(120);
        return send_hotkey("CTRL+T");
    }
    if (!_stricmp(p, "close tab") || !_stricmp(p, "close this tab")) { focus_shortcut_target(); return send_hotkey("CTRL+W"); }
    if (!_stricmp(p, "next tab") || !_stricmp(p, "switch to next tab")) return send_hotkey("CTRL+TAB");
    if (!_stricmp(p, "previous tab") || !_stricmp(p, "switch to previous tab")) return send_hotkey("CTRL+SHIFT+TAB");
    if (!_stricmp(p, "close window") || !_stricmp(p, "close this window")) return send_hotkey("ALT+F4");
    if (!_stricmp(p, "minimize window") || !_stricmp(p, "minimize this window")) return send_hotkey("WIN+DOWN");
    if (!_stricmp(p, "maximize window") || !_stricmp(p, "maximize this window")) return send_hotkey("WIN+UP");
    if (!_stricmp(p, "focus address bar") || !_stricmp(p, "focus url bar") || !_stricmp(p, "focus search bar")) return send_hotkey("CTRL+L");
    return 0;
}

static void hide_bar(void);

static void submit_prompt(void) {
    int wlen;
    wchar_t *wbuf = NULL;
    char *prompt8 = NULL;
    unsigned char *img = NULL;
    size_t img_len = 0;
    char *image64 = NULL;
    AskArgs *args = NULL;
    HANDLE thread;
    HBITMAP fresh_bmp = NULL;

    if (g_busy) return;
    wlen = GetWindowTextLengthW(g_edit);
    if (wlen <= 0) return;

    wbuf = (wchar_t *)calloc((size_t)wlen + 1, sizeof(wchar_t));
    if (!wbuf) return;
    GetWindowTextW(g_edit, wbuf, wlen + 1);
    prompt8 = wide_to_utf8(wbuf);
    free(wbuf);
    if (prompt8) trim_prompt(prompt8);
    if (!prompt8 || !prompt8[0]) {
        free(prompt8);
        return;
    }

    /* hard bypass: these commands are never allowed to reach task_prompt_like(). */
    if (prompt_has(prompt8, "desktop") &&
        (prompt_has(prompt8, "create") || prompt_has(prompt8, "make") ||
         prompt_has(prompt8, "new file") || prompt_has(prompt8, "text file") ||
         prompt_has(prompt8, "file"))) {
        hide_bar();
        if (!direct_create_desktop_file(prompt8))
            SetWindowTextW(g_edit, L"couldn't create desktop file");
        free(prompt8);
        return;
    }

    /* slash commands are local routes and must never fall through to the ai api. */
    if (prompt8[0] == '/') {
        if (slash_command(prompt8)) {
            /* Clear the command after dispatch so the next command can be typed
               immediately. Do this only after slash_command has copied/used its
               argument, and do not clear /ask because it deliberately falls
               through to the normal AI request path. */
            SetWindowTextW(g_edit, L"");
            PostMessageW(g_hwnd, WM_TEXT_CHANGED, 0, 0);
            free(prompt8);
            return;
        }
    }

    if (direct_shortcut_intent(prompt8)) {
        /* common local actions are handled instantly without an ai request. */
        hide_bar();
        if (!run_direct_shortcut(prompt8)) {
            SetWindowTextW(g_edit, L"couldn't perform shortcut");
        }
        free(prompt8);
        return;
    }

    if (task_prompt_like(prompt8)) {
        HANDLE task;
        hide_bar();
        InterlockedExchange(&g_task_stop, 0);
        g_task_active = 1;
        g_task_step = 0;
        g_task_last_action[0] = 0;
        g_arrow_target_valid = 0;
        g_busy = 1;
        RegisterHotKey(g_hwnd, ESC_HOTKEY_ID, 0, VK_ESCAPE);
        ShowWindow(g_edit, SW_HIDE);
        InvalidateRect(g_hwnd, NULL, FALSE);
        task = CreateThread(NULL, 0, task_thread, prompt8, 0, NULL);
        if (!task) { g_task_active = 0; g_busy = 0; ShowWindow(g_edit, SW_SHOW); SetWindowTextW(g_edit, L"couldn't start automation"); free(prompt8); }
        else CloseHandle(task);
        return;
    }

    if (g_screenshot_enabled) {
        /* capture a fresh screen for every request, so the camera icon always
           attaches the current screen rather than an older cached image. */
        if (!save_temp_screenshot(&fresh_bmp)) {
            free(prompt8);
            SetWindowTextW(g_edit, L"couldn't capture screenshot");
            return;
        }
        if (fresh_bmp) {
            if (g_preview) DeleteObject(g_preview);
            g_preview = fresh_bmp;
            fresh_bmp = NULL;
        }

        if (!read_file_bytes(g_image_path, &img, &img_len)) {
            free(prompt8);
            SetWindowTextW(g_edit, L"couldn't read screenshot");
            return;
        }
        image64 = base64_encode(img, img_len);
        free(img);
        if (!image64) {
            free(prompt8);
            SetWindowTextW(g_edit, L"couldn't prepare screenshot");
            return;
        }
    } else {
        image64 = _strdup("");
        if (!image64) {
            free(prompt8);
            return;
        }
    }

    args = (AskArgs *)calloc(1, sizeof(*args));
    if (!args) {
        free(prompt8);
        free(image64);
        return;
    }
    args->prompt = prompt8;
    args->image64 = image64;

    g_busy = 1;
    g_request_start = GetTickCount64();
    g_throbber_frame = 0;
    ShowWindow(g_edit, SW_HIDE);
    InvalidateRect(g_hwnd, NULL, FALSE);

    thread = CreateThread(NULL, 0, ask_thread, args, 0, NULL);
    if (!thread) {
        g_busy = 0;
        ShowWindow(g_edit, SW_SHOW);
        EnableWindow(g_edit, TRUE);
        SetWindowTextW(g_edit, L"couldn't start request");
        free(args->prompt);
        free(args->image64);
        free(args);
        return;
    }
    CloseHandle(thread);
}

static LRESULT CALLBACK edit_proc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (msg == WM_ERASEBKGND) return 1;
    /* Enter is handled in WM_CHAR below.  TranslateMessage can queue WM_CHAR
       even when WM_KEYDOWN is consumed, so submitting from WM_KEYDOWN can still
       allow the edit control to insert a newline afterwards. */
    if (msg == WM_KEYDOWN && wParam == VK_ESCAPE) {
        PostMessageW(g_hwnd, WM_HIDE_BAR, 0, 0);
        return 0;
    }

    /* keep the normal textbox shortcuts working explicitly */
    if (msg == WM_KEYDOWN && (GetKeyState(VK_CONTROL) & 0x8000)) {
        if (wParam == 'A') {
            SendMessageW(hwnd, EM_SETSEL, 0, -1);
            return 0;
        }
        if (wParam == 'C') {
            SendMessageW(hwnd, WM_COPY, 0, 0);
            return 0;
        }
        if (wParam == 'V') {
            SendMessageW(hwnd, WM_PASTE, 0, 0);
            PostMessageW(g_hwnd, WM_TEXT_CHANGED, 0, 0);
            return 0;
        }
        if (wParam == 'X') {
            SendMessageW(hwnd, WM_CUT, 0, 0);
            PostMessageW(g_hwnd, WM_TEXT_CHANGED, 0, 0);
            return 0;
        }
        if (wParam == VK_BACK) {
            /* ctrl+backspace: delete the word immediately before the caret */
            DWORD sel_start = 0, sel_end = 0;
            int len = GetWindowTextLengthW(hwnd);
            wchar_t *buf = NULL;
            int pos;
            int start;

            SendMessageW(hwnd, EM_GETSEL, (WPARAM)&sel_start, (LPARAM)&sel_end);
            if (sel_start != sel_end) {
                SendMessageW(hwnd, EM_REPLACESEL, TRUE, (LPARAM)L"");
                PostMessageW(g_hwnd, WM_TEXT_CHANGED, 0, 0);
                return 0;
            }

            pos = (int)sel_start;
            if (pos <= 0 || len <= 0) return 0;

            buf = (wchar_t *)calloc((size_t)len + 1, sizeof(wchar_t));
            if (!buf) return 0;
            GetWindowTextW(hwnd, buf, len + 1);

            start = pos;
            while (start > 0 && iswspace(buf[start - 1])) --start;
            while (start > 0 && !iswspace(buf[start - 1])) --start;

            SendMessageW(hwnd, EM_SETSEL, (WPARAM)start, (LPARAM)pos);
            SendMessageW(hwnd, EM_REPLACESEL, TRUE, (LPARAM)L"");
            PostMessageW(g_hwnd, WM_TEXT_CHANGED, 0, 0);
            free(buf);
            return 0;
        }
    }

    /* swallow control/backspace character messages so ctrl+backspace can never
       leave the old 8/127 control glyph behind in the textbox. */
    if (msg == WM_CHAR) {
        LRESULT r;
        if (wParam == 13 || wParam == 10) {
            if (GetKeyState(VK_SHIFT) & 0x8000) {
                /* Let Shift+Enter insert a line break, but swallow the paired
                   carriage-return/line-feed character if the control emits it. */
                g_swallow_next_enter_char = 1;
                SendMessageW(hwnd, EM_REPLACESEL, TRUE, (LPARAM)L"\r\n");
                g_arrow_l_bounce_v -= 18.0f;
                g_arrow_r_bounce_v += 18.0f;
                PostMessageW(g_hwnd, WM_TEXT_CHANGED, 0, 0);
                return 0;
            }
            SendMessageW(g_hwnd, WM_SUBMIT, 0, 0);
            return 0;
        }
        if (g_swallow_next_enter_char && (wParam == 10 || wParam == 13)) {
            g_swallow_next_enter_char = 0;
            return 0;
        }
        if (wParam != 10 && wParam != 13) g_swallow_next_enter_char = 0;
        if ((GetKeyState(VK_CONTROL) & 0x8000) && (wParam == 8 || wParam == 127))
            return 0;
        if (wParam >= 32 || wParam == 8 || wParam == 127 || wParam == 13 || wParam == 10) {
            if (wParam >= 32 || wParam == 13 || wParam == 10) {
                g_arrow_l_bounce_v -= 14.0f;
                g_arrow_r_bounce_v += 14.0f;
            }
            r = CallWindowProcW(g_old_edit_proc, hwnd, msg, wParam, lParam);
            PostMessageW(g_hwnd, WM_TEXT_CHANGED, 0, 0);
            return r;
        }
    }
    return CallWindowProcW(g_old_edit_proc, hwnd, msg, wParam, lParam);
}

static void place_centered(void) {
    RECT work;
    int sw = GetSystemMetrics(SM_CXSCREEN);
    int x = (sw - g_window_w) / 2;
    int y = 0;

    if (SystemParametersInfoW(SPI_GETWORKAREA, 0, &work, 0)) {
        if (x < work.left + 10) x = work.left + 10;
        if (x + g_window_w > work.right - 10) x = work.right - g_window_w - 10;
        y = work.top;
    }
    SetWindowPos(g_hwnd, HWND_TOPMOST, x, y, g_window_w, g_bar_h, SWP_NOACTIVATE);
}

static void make_round(HWND hwnd);

static int measure_response_height(const wchar_t *text) {
    HDC dc;
    HFONT old_font;
    TEXTMETRICW tm;
    RECT edit_rc;
    int line_count;
    int line_h;
    int content_h;
    int new_h;

    (void)text;

    if (!g_edit) return 54;

    dc = GetDC(g_edit);
    if (!dc) return 54;

    old_font = (HFONT)SelectObject(dc, g_font);
    GetTextMetricsW(dc, &tm);
    line_h = tm.tmHeight + tm.tmExternalLeading;
    SelectObject(dc, old_font);
    ReleaseDC(g_edit, dc);

    if (line_h < 1) line_h = 19;

    /* ask the actual edit control how many wrapped lines it has.
       this matches its real width and margins instead of estimating them. */
    line_count = (int)SendMessageW(g_edit, EM_GETLINECOUNT, 0, 0);
    if (line_count < 1) line_count = 1;

    GetClientRect(g_edit, &edit_rc);
    content_h = line_count * line_h;
    new_h = content_h + 40;

    if (new_h < 54) new_h = 54;
    if (new_h > 700) new_h = 700;
    return new_h;
}

static void resize_response_box(const wchar_t *text) {
    g_bar_target_h = measure_response_height(text);
    if (g_bar_target_h < 54) g_bar_target_h = 54;
    if (g_bar_target_h > 700) g_bar_target_h = 700;

    /* start the same kind of spring motion as the arrows */
    g_bar_h_v = (float)(g_bar_target_h - g_bar_h_float) * 0.18f;
}

static void update_bar_width(void) {
    float distance, next_width;
    int next_w;
    RECT wr;
    if (!g_visible) return;
    if (g_search_state == SEARCH_LOADING) g_bar_w_target = 760;
    else if (g_busy && g_search_state == SEARCH_IDLE) g_bar_w_target = 810;
    distance = (float)g_bar_w_target - g_bar_w_float;
    /* Reuse the exact spring and damping used by the arrow bounce. */
    g_bar_w_velocity += distance * 0.24f;
    g_bar_w_velocity *= 0.72f;
    next_width = g_bar_w_float + g_bar_w_velocity;
    if (fabsf(distance) < 0.35f && fabsf(g_bar_w_velocity) < 0.35f) {
        next_width = (float)g_bar_w_target;
        g_bar_w_velocity = 0.0f;
    }
    /* Let the spring pass its target briefly. Hard-clamping to 790..810
       removed all visible overshoot, making focus/defocus look static. */
    if (next_width < (g_bar_focused ? 695.0f : 190.0f)) {
        next_width = g_bar_focused ? 695.0f : 190.0f;
        if (g_bar_w_velocity < 0.0f) g_bar_w_velocity = -g_bar_w_velocity * 0.42f;
    }
    if (next_width > (g_bar_focused ? 815.0f : 215.0f)) {
        next_width = g_bar_focused ? 815.0f : 215.0f;
        if (g_bar_w_velocity > 0.0f) g_bar_w_velocity = -g_bar_w_velocity * 0.42f;
    }
    next_w = (int)(next_width + 0.5f);
    g_bar_w_float = next_width;
    if (next_w == g_window_w) return;
    g_window_w = g_bar_w = next_w;
    if (GetWindowRect(g_hwnd, &wr)) {
        int center = (wr.left + wr.right) / 2;
        int x = center - next_w / 2;
        SetWindowPos(g_hwnd, NULL, x, wr.top, next_w, g_bar_h,
                     SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOOWNERZORDER);
    }
    if (g_edit && g_search_state == SEARCH_IDLE && g_bar_focused)
        SetWindowPos(g_edit, NULL, 58, 9, g_bar_w - 300, g_bar_h - 18,
                     SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOOWNERZORDER);
    make_round(g_hwnd);
    g_last_blur_frame = 0;
    InvalidateRect(g_hwnd, NULL, FALSE);
}

static void update_response_box(void) {
    float distance;
    float next_float;
    int next_h;

    int min_h;
    if (!g_visible) return;

    /* The compact unfocused bar must be allowed to reach half height.
       Keep the normal 54px minimum while focused or showing active content. */
    min_h = (!g_bar_focused && g_search_state == SEARCH_IDLE && !g_busy) ? 27 : 54;
    if (g_bar_target_h < min_h) g_bar_target_h = min_h;

    distance = (float)g_bar_target_h - g_bar_h_float;
    g_bar_h_v += distance * 0.24f;
    g_bar_h_v *= 0.72f;
    next_float = g_bar_h_float + g_bar_h_v;

    if (fabsf(distance) < 0.35f && fabsf(g_bar_h_v) < 0.35f) {
        next_float = (float)g_bar_target_h;
        g_bar_h_v = 0.0f;
    }

    if (next_float < (float)min_h) next_float = (float)min_h;
    if (next_float > 700.0f) next_float = 700.0f;
    next_h = (int)(next_float + 0.5f);

    if (next_h != g_bar_h) {
        g_bar_h = next_h;
        g_bar_h_float = next_float;

        SetWindowPos(g_hwnd, NULL, 0, 0, g_window_w, g_bar_h,
                     SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOOWNERZORDER | SWP_NOREDRAW);
        make_round(g_hwnd);
        if (g_search_state == SEARCH_IDLE && g_bar_focused && g_edit) {
            SendMessageW(g_edit, WM_SETREDRAW, FALSE, 0);
            SetWindowPos(g_edit, NULL, 58, 9, g_bar_w - 300, g_bar_h - 18,
                         SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOOWNERZORDER | SWP_NOREDRAW);
            SendMessageW(g_edit, WM_SETREDRAW, TRUE, 0);
        }
        RedrawWindow(g_hwnd, NULL, NULL, RDW_INVALIDATE | RDW_ALLCHILDREN | RDW_NOERASE);
    } else {
        g_bar_h_float = next_float;
    }

    if (g_search_state != SEARCH_IDLE) resize_embedded_webview();
    position_arrow_windows();
    if (g_search_url[0] && g_browser_button && g_visible) position_browser_button();
}

static void make_round(HWND hwnd) {
    HRGN rgn = CreateRoundRectRgn(0, 0, g_window_w + 1, g_bar_h + 1, 32, 32);
    if (rgn) SetWindowRgn(hwnd, rgn, FALSE);
}


static void show_bar(void) {
    HBITMAP bmp = NULL;
    g_screenshot_enabled = 1;

    if (!save_temp_screenshot(&bmp)) {
        MessageBoxW(g_hwnd, L"couldn't capture the screen.", APP_TITLE, MB_ICONERROR);
        return;
    }

    if (g_preview) DeleteObject(g_preview);
    g_preview = bmp;
    g_arrow_l_x = -100.0f;
    g_arrow_r_x = (float)g_window_w + 100.0f;
    g_arrow_l_v = 30.0f;
    g_arrow_r_v = -30.0f;
    g_arrow_phase = 0;
    g_arrow_l_bounce = -72.0f;
    g_arrow_r_bounce = 72.0f;
    g_arrow_l_bounce_v = 0.0f;
    g_arrow_r_bounce_v = 0.0f;
    g_arrow_target_valid = 0;
    g_busy = 0;
    search_leave_layout();
    g_bar_h = 54;
    g_bar_h_float = 54.0f;
    g_bar_h_v = 0.0f;
    g_bar_target_h = 54;
    g_last_blur_frame = 0;
    InterlockedExchange(&g_glass_capture_busy, 0);
    g_ui_hover_hit = 0;
    g_hover_x = 30.0f;
    g_hover_x_velocity = 0.0f;
    g_hover_y_offset = 0.0f;
    g_hover_y_velocity = 0.0f;
    g_hover_bounce_tick = 0;

    g_bar_w_target = 810;
    g_bar_w = g_window_w = 810;
    g_bar_w_float = 810.0f;
    g_bar_w_velocity = 0.0f;
    g_glass_w = 810;
    g_glass_h = 54;
    place_centered();

    /* Capture the desktop with the window hidden and display affinity disabled.
       Some DWM configurations leave an excluded-window black rectangle in a
       screen DC capture after the first hide/show cycle. Re-enable exclusion
       only after the hidden backdrop has been captured and blurred. */
    SetWindowDisplayAffinity(g_hwnd, WDA_NONE);
    if (!capture_hidden_glass_surface()) {
        /* Keep the last valid blurred frame instead of replacing it with black. */
        if (g_glass_dc && g_last_blur_frame == 0) {
            HBRUSH fallback = CreateSolidBrush(RGB(28, 38, 52));
            RECT glass_rc = { 0, 0, g_glass_w, g_glass_h };
            if (fallback) {
                FillRect(g_glass_dc, &glass_rc, fallback);
                DeleteObject(fallback);
            }
        }
    }
    SetWindowDisplayAffinity(g_hwnd, WDA_EXCLUDEFROMCAPTURE);

    g_visible = 1;
    g_bar_focused = TRUE;
    ShowWindow(g_hwnd, SW_SHOWNORMAL);
    SetWindowPos(g_hwnd, HWND_TOPMOST, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW | SWP_NOOWNERZORDER);
    ui_set_hover(0);
    if (g_browser_button) ShowWindow(g_browser_button, SW_HIDE);
    SetWindowTextW(g_edit, L"");
    EnableWindow(g_edit, TRUE);
    SetForegroundWindow(g_hwnd);
    SetFocus(g_edit);
    position_arrow_windows();
    InvalidateRect(g_hwnd, NULL, FALSE);
    if (g_edit) InvalidateRect(g_edit, NULL, FALSE);
    SetTimer(g_hwnd, TIMER_ID, 16, NULL);
}

static void clear_preview(void) {
    if (g_preview) {
        DeleteObject(g_preview);
        g_preview = NULL;
    }
    if (g_image_path[0]) {
        DeleteFileW(g_image_path);
        g_image_path[0] = 0;
    }
    InvalidateRect(g_hwnd, NULL, FALSE);
}

static void hide_bar(void) {
    g_slider_dragging = FALSE;
    if (GetCapture() == g_hwnd) ReleaseCapture();
    ui_set_hover(0);
    g_hover_x_velocity = 0.0f;
    g_hover_y_velocity = 0.0f;
    g_hover_y_offset = 0.0f;
    g_hover_bounce_tick = 0;
    KillTimer(g_hwnd, TIMER_ID);
    if (g_arrow_l) ShowWindow(g_arrow_l, SW_HIDE);
    if (g_arrow_r) ShowWindow(g_arrow_r, SW_HIDE);
    stop_search_webview();
    search_button_show(0);
    ShowWindow(g_hwnd, SW_HIDE);
    g_visible = 0;
    g_busy = 0;

    if (g_preview) {
        DeleteObject(g_preview);
        g_preview = NULL;
    }
    if (g_image_path[0]) {
        DeleteFileW(g_image_path);
        g_image_path[0] = 0;
    }
}

static LRESULT CALLBACK arrow_proc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    (void)wParam;
    (void)lParam;

    switch (msg) {
        case WM_ERASEBKGND:
            return 1;
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            HFONT old;
            SetBkMode(hdc, TRANSPARENT);
            SetTextColor(hdc, RGB(218, 218, 224));
            old = (HFONT)SelectObject(hdc, g_arrow_font);
            if (hwnd == g_arrow_l)
                TextOutW(hdc, 3, 2, L">", 1);
            else
                TextOutW(hdc, 3, 2, L"<", 1);
            SelectObject(hdc, old);
            EndPaint(hwnd, &ps);
            return 0;
        }
        default:
            return DefWindowProcW(hwnd, msg, wParam, lParam);
    }
}

static void position_arrow_windows(void) {
    RECT wr;
    int base_y;
    if (!g_hwnd || !g_arrow_l || !g_arrow_r) return;

    if (g_task_active && g_arrow_target_valid) {
        int vx = GetSystemMetrics(SM_XVIRTUALSCREEN);
        int vy = GetSystemMetrics(SM_YVIRTUALSCREEN);
        int vw = GetSystemMetrics(SM_CXVIRTUALSCREEN);
        int vh = GetSystemMetrics(SM_CYVIRTUALSCREEN);
        int ay = g_arrow_target_y - 21;
        int lx = g_arrow_target_x - 66 + (int)g_arrow_l_bounce;
        int rx = g_arrow_target_x + 24 + (int)g_arrow_r_bounce;
        if (lx < vx + 4) lx = vx + 4;
        if (rx < vx + 4) rx = vx + 4;
        if (lx > vx + vw - 46) lx = vx + vw - 46;
        if (rx > vx + vw - 46) rx = vx + vw - 46;
        if (ay < vy + 4) ay = vy + 4;
        if (ay > vy + vh - 46) ay = vy + vh - 46;
        SetWindowPos(g_arrow_l, HWND_TOPMOST, lx, ay, 42, 42,
                     SWP_NOACTIVATE | SWP_SHOWWINDOW);
        SetWindowPos(g_arrow_r, HWND_TOPMOST, rx, ay, 42, 42,
                     SWP_NOACTIVATE | SWP_SHOWWINDOW);
        return;
    }

    GetWindowRect(g_hwnd, &wr);
    base_y = wr.top + (g_bar_h - 42) / 2;

    SetWindowPos(g_arrow_l, HWND_TOPMOST,
                 wr.left - 48 + (int)g_arrow_l_bounce,
                 base_y,
                 42, 42,
                 SWP_NOACTIVATE | SWP_SHOWWINDOW);

    SetWindowPos(g_arrow_r, HWND_TOPMOST,
                 wr.right + 6 + (int)g_arrow_r_bounce,
                 base_y,
                 42, 42,
                 SWP_NOACTIVATE | SWP_SHOWWINDOW);
}

static void update_arrows(void) {
    g_arrow_l_bounce_v += (0.0f - g_arrow_l_bounce) * 0.24f;
    g_arrow_r_bounce_v += (0.0f - g_arrow_r_bounce) * 0.24f;
    g_arrow_l_bounce_v *= 0.72f;
    g_arrow_r_bounce_v *= 0.72f;
    g_arrow_l_bounce += g_arrow_l_bounce_v;
    g_arrow_r_bounce += g_arrow_r_bounce_v;

    position_arrow_windows();

    if (g_arrow_phase < 60) {
        ++g_arrow_phase;
        if (g_arrow_phase == 8) {
            g_arrow_l_bounce_v = -6.5f;
            g_arrow_r_bounce_v = 6.5f;
        }
    } else {
        g_arrow_l_bounce_v += sinf((float)g_arrow_phase * 0.18f) * 0.05f;
        g_arrow_r_bounce_v -= sinf((float)g_arrow_phase * 0.18f) * 0.05f;
        ++g_arrow_phase;
        if (g_arrow_phase > 100000) g_arrow_phase = 60;
    }
}

static void draw_preview(HDC hdc, RECT r) {
    HDC mem;
    HGDIOBJ old;
    BITMAP bm;
    int sw, sh, dw, dh, dx, dy;

    if (!g_preview) {
        HPEN p = CreatePen(PS_SOLID, 1, RGB(55, 55, 62));
        HBRUSH b = CreateSolidBrush(RGB(31, 31, 35));
        HGDIOBJ op = SelectObject(hdc, p);
        HGDIOBJ ob = SelectObject(hdc, b);
        RoundRect(hdc, r.left, r.top, r.right, r.bottom, 14, 14);
        SelectObject(hdc, op);
        SelectObject(hdc, ob);
        DeleteObject(p);
        DeleteObject(b);
        return;
    }

    if (!GetObjectW(g_preview, sizeof(bm), &bm)) return;
    sw = bm.bmWidth;
    sh = bm.bmHeight;
    if (sw <= 0 || sh <= 0) return;

    dw = r.right - r.left;
    dh = (int)((double)sh * dw / sw);
    if (dh > r.bottom - r.top) {
        dh = r.bottom - r.top;
        dw = (int)((double)sw * dh / sh);
    }
    dx = r.left + ((r.right - r.left) - dw) / 2;
    dy = r.top + ((r.bottom - r.top) - dh) / 2;

    mem = CreateCompatibleDC(hdc);
    if (!mem) return;
    old = SelectObject(mem, g_preview);
    SetStretchBltMode(hdc, HALFTONE);
    StretchBlt(hdc, dx, dy, dw, dh, mem, 0, 0, sw, sh, SRCCOPY);
    SelectObject(mem, old);
    DeleteDC(mem);
}

static void paint_window(HWND hwnd, HDC hdc) {
    RECT rc;
    HDC memdc;
    HBITMAP membmp, oldbmp;
    HBRUSH bg;
    HPEN border;
    HGDIOBJ oldb, oldp;

    GetClientRect(hwnd, &rc);
    memdc = CreateCompatibleDC(hdc);
    if (!memdc) return;
    membmp = CreateCompatibleBitmap(hdc, rc.right - rc.left, rc.bottom - rc.top);
    if (!membmp) {
        DeleteDC(memdc);
        return;
    }
    oldbmp = (HBITMAP)SelectObject(memdc, membmp);

    if (g_search_state == SEARCH_IDLE && g_glass_assets_ok) {
        BOOL compact_bar = (rc.bottom <= GLASS_BAR_H);
        /* Always capture the desktop for the first visible frame too.  The old
           fallback painted an opaque navy rectangle whenever the cached glass
           surface was not ready yet, which looked like the background had been
           darkened on startup/reopen.  capture_live_glass() refreshes the pixels
           and applies blur without adding a dark tint. */
        capture_live_glass(memdc);
        if (!compact_bar) {
            /* outline only: an opaque fill here hides the captured blurred
               backdrop whenever the bar expands. */
            HPEN shell_pen = CreatePen(PS_SOLID, 1, RGB(140, 164, 187));
            HGDIOBJ old_brush = SelectObject(memdc, GetStockObject(HOLLOW_BRUSH));
            HGDIOBJ old_pen = SelectObject(memdc, shell_pen);
            RoundRect(memdc, 0, 0, rc.right, rc.bottom, 32, 32);
            SelectObject(memdc, old_brush);
            SelectObject(memdc, old_pen);
            DeleteObject(shell_pen);
        }
        /* In the idle defocused state, the HWND itself is resized by the
           width/height spring. Do not paint the full control row and rely on
           the rounded region to clip it: render only the blurred glass shell.
           All controls return when the bar regains focus. */
        if (g_bar_focused || g_busy || g_search_state != SEARCH_IDLE) {
            glassbar_draw(memdc, compact_bar ? 1 : 0, g_screenshot_enabled,
                          g_ui_hover_hit, g_reasoning_value, g_hover_x,
                          g_hover_y_offset, g_slider_label_font);
        } else {
            HPEN shell_pen = CreatePen(PS_SOLID, 1, RGB(140, 164, 187));
            HGDIOBJ old_brush = SelectObject(memdc, GetStockObject(HOLLOW_BRUSH));
            HGDIOBJ old_pen = SelectObject(memdc, shell_pen);
            RoundRect(memdc, 0, 0, rc.right, rc.bottom, 28, 28);
            SelectObject(memdc, old_brush);
            SelectObject(memdc, old_pen);
            DeleteObject(shell_pen);
        }
    } else {
        if (g_search_state != SEARCH_IDLE) {
            /* Avoid the flat gray slab behind WebView commands: use the same live
               desktop-glass capture as the idle bar, without an extra dark overlay. */
            capture_live_glass(memdc);
        } else {
            bg = CreateSolidBrush(RGB(24, 24, 27));
            FillRect(memdc, &rc, bg);
            DeleteObject(bg);
        }
        border = CreatePen(PS_SOLID, 1, RGB(50, 50, 56));
        oldb = SelectObject(memdc, GetStockObject(NULL_BRUSH));
        oldp = SelectObject(memdc, border);
        RoundRect(memdc, 1, 1, g_bar_w - 1, g_bar_h - 1, 20, 20);
        SelectObject(memdc, oldb);
        SelectObject(memdc, oldp);
        DeleteObject(border);

        /* Draw the resize grip in the reserved bottom strip, outside the
           WebView controller bounds so the webpage cannot cover it. */
        if (g_search_state != SEARCH_IDLE && g_bar_w > 30 && g_bar_h > 30) {
            HPEN grip_pen = CreatePen(PS_SOLID, 2, RGB(190, 198, 210));
            HGDIOBJ old_grip_pen = SelectObject(memdc, grip_pen);
            int gx = g_bar_w - 9;
            int gy = g_bar_h - 7;
            MoveToEx(memdc, gx - 14, gy, NULL); LineTo(memdc, gx, gy - 14);
            MoveToEx(memdc, gx - 9, gy, NULL); LineTo(memdc, gx, gy - 9);
            MoveToEx(memdc, gx - 4, gy, NULL); LineTo(memdc, gx, gy - 4);
            SelectObject(memdc, old_grip_pen);
            DeleteObject(grip_pen);
        }

        if (g_search_state == SEARCH_IDLE && g_screenshot_enabled) {
            RECT preview;
            preview.left = g_bar_w - 108;
            preview.top = 13;
            preview.right = g_bar_w - 58;
            preview.bottom = g_bar_h - 13;
            draw_preview(memdc, preview);
        }
    }

    if (g_busy || g_search_state == SEARCH_LOADING) {
        static const wchar_t *dots[] = { L"·", L"·", L"·", L"·", L"·", L"·", L"·", L"·" };
        wchar_t status[64];
        ULONGLONG elapsed_ms = GetTickCount64() - g_request_start;
        double elapsed = (double)elapsed_ms / 1000.0;
        int whole = (int)elapsed;
        int tenths = (int)((elapsed - whole) * 10.0);
        int i;
        HFONT status_font;
        HFONT old_font;
        SetBkMode(memdc, TRANSPARENT);
        SetTextColor(memdc, RGB(150, 150, 156));

        status_font = CreateFontW(-16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                  DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                  CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
        old_font = (HFONT)SelectObject(memdc, status_font);

        if (g_search_state == SEARCH_LOADING)
            wsprintfW(status, L"loading webpage...  %d.%ds", whole, tenths);
        else
            wsprintfW(status, L"thinking...  %d.%ds", whole, tenths);
        TextOutW(memdc, 49, (g_bar_h - 16) / 2, status, (int)wcslen(status));

        for (i = 0; i < 8; ++i) {
            double a = ((double)i + (double)g_throbber_frame * 0.45) * 0.78539816339;
            int cx = 25 + (int)(cos(a) * 9.0);
            int cy = 47 + (int)(sin(a) * 9.0);
            int d = (i == (g_throbber_frame % 8)) ? 4 : 3;
            HBRUSH dot = CreateSolidBrush(i == (g_throbber_frame % 8) ? RGB(205, 205, 211) : RGB(82, 82, 88));
            HGDIOBJ old_dot = SelectObject(memdc, dot);
            Ellipse(memdc, cx - d / 2, cy - d / 2, cx + (d + 1) / 2, cy + (d + 1) / 2);
            SelectObject(memdc, old_dot);
            DeleteObject(dot);
        }

        SelectObject(memdc, old_font);
        DeleteObject(status_font);
    }

    /* Do not blur or darken the fully composed bar here. The live backdrop is
       already captured and blurred before the controls are drawn above it. */

    BitBlt(hdc, 0, 0, rc.right, rc.bottom, memdc, 0, 0, SRCCOPY);
    SelectObject(memdc, oldbmp);
    DeleteObject(membmp);
    DeleteDC(memdc);
}

static void paint_settings_button(HDC hdc, const DRAWITEMSTRUCT *dis, const wchar_t *text) {
    HBRUSH bg;
    HPEN pen;
    HGDIOBJ oldb, oldp;
    RECT r = dis->rcItem;
    BOOL down = (dis->itemState & ODS_SELECTED) != 0;

    bg = CreateSolidBrush(down ? RGB(48, 48, 54) : RGB(35, 35, 40));
    pen = CreatePen(PS_SOLID, 1, RGB(62, 62, 70));
    oldb = SelectObject(hdc, bg);
    oldp = SelectObject(hdc, pen);
    RoundRect(hdc, r.left, r.top, r.right, r.bottom, 10, 10);
    SelectObject(hdc, oldb);
    SelectObject(hdc, oldp);
    DeleteObject(bg);
    DeleteObject(pen);

    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, RGB(230, 230, 235));
    DrawTextW(hdc, text, -1, &r, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
}

static LRESULT CALLBACK settings_proc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (msg == WM_KEYDOWN && wParam == VK_ESCAPE) {
        DestroyWindow(hwnd);
        return 0;
    }
    switch (msg) {
        case WM_CREATE: {
            HWND label;
            HWND save;
            HWND cancel;
            int i;

            g_settings_brush = CreateSolidBrush(RGB(24, 24, 27));
            g_settings_edit_brush = CreateSolidBrush(RGB(31, 31, 35));

            label = CreateWindowExW(0, L"STATIC", L"groq api key",
                                    WS_CHILD | WS_VISIBLE,
                                    20, 16, 390, 22, hwnd, NULL,
                                    GetModuleHandleW(NULL), NULL);

            g_settings_key = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", g_groq_key,
                                              WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL | ES_PASSWORD,
                                              20, 42, 420, 32, hwnd, NULL,
                                              GetModuleHandleW(NULL), NULL);

            label = CreateWindowExW(0, L"STATIC", L"slash commands",
                                    WS_CHILD | WS_VISIBLE,
                                    20, 86, 180, 22, hwnd, NULL,
                                    GetModuleHandleW(NULL), NULL);

            label = CreateWindowExW(0, L"STATIC", L"command",
                                    WS_CHILD | WS_VISIBLE,
                                    20, 112, 120, 18, hwnd, NULL,
                                    GetModuleHandleW(NULL), NULL);
            label = CreateWindowExW(0, L"STATIC", L"action",
                                    WS_CHILD | WS_VISIBLE,
                                    158, 112, 190, 18, hwnd, NULL,
                                    GetModuleHandleW(NULL), NULL);

            for (i = 0; i < MAX_SLASH_COMMANDS; ++i) {
                wchar_t wc1[64], wc2[64];
                int y = 134 + i * 40;
                MultiByteToWideChar(CP_UTF8, 0, g_slash_cmd[i], -1, wc1, 64);
                MultiByteToWideChar(CP_UTF8, 0, g_slash_action[i], -1, wc2, 64);
                g_settings_cmd[i] = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", wc1,
                    WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
                    20, y, 120, 28, hwnd, NULL, GetModuleHandleW(NULL), NULL);
                g_settings_action[i] = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", wc2,
                    WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
                    158, y, 190, 28, hwnd, NULL, GetModuleHandleW(NULL), NULL);
                SendMessageW(g_settings_cmd[i], WM_SETFONT, (WPARAM)g_font, TRUE);
                SendMessageW(g_settings_action[i], WM_SETFONT, (WPARAM)g_font, TRUE);
            }

            {
                HWND close_button = CreateWindowExW(0, L"BUTTON", L"x",
                    WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | WS_TABSTOP,
                    410, 10, 28, 28, hwnd, (HMENU)SETTINGS_CLOSE_ID,
                    GetModuleHandleW(NULL), NULL);
                if (close_button) SendMessageW(close_button, WM_SETFONT, (WPARAM)g_font, TRUE);
            }

            save = CreateWindowExW(0, L"BUTTON", L"save",
                                   WS_CHILD | WS_VISIBLE | BS_OWNERDRAW | WS_TABSTOP,
                                   260, 470, 86, 36, hwnd,
                                   (HMENU)SETTINGS_SAVE_ID, GetModuleHandleW(NULL), NULL);
            cancel = CreateWindowExW(0, L"BUTTON", L"cancel",
                                     WS_CHILD | WS_VISIBLE | BS_OWNERDRAW | WS_TABSTOP,
                                     354, 470, 86, 36, hwnd,
                                     (HMENU)SETTINGS_CANCEL_ID, GetModuleHandleW(NULL), NULL);

            SendMessageW(g_settings_key, WM_SETFONT, (WPARAM)g_font, TRUE);
            SendMessageW(save, WM_SETFONT, (WPARAM)g_font, TRUE);
            SendMessageW(cancel, WM_SETFONT, (WPARAM)g_font, TRUE);
            SetFocus(g_settings_key);
            return 0;
        }

        case WM_DRAWITEM: {
            LPDRAWITEMSTRUCT dis = (LPDRAWITEMSTRUCT)lParam;
            if (dis->CtlType == ODT_BUTTON) {
                if (dis->CtlID == SETTINGS_SAVE_ID)
                    paint_settings_button(dis->hDC, dis, L"save");
                else if (dis->CtlID == SETTINGS_CANCEL_ID)
                    paint_settings_button(dis->hDC, dis, L"cancel");
                return TRUE;
            }
            break;
        }

        case WM_CTLCOLOREDIT: {
            HDC dc = (HDC)wParam;
            SetTextColor(dc, RGB(235, 235, 238));
            SetBkColor(dc, RGB(31, 31, 35));
            return (LRESULT)g_settings_edit_brush;
        }

        case WM_CTLCOLORSTATIC: {
            HDC dc = (HDC)wParam;
            SetTextColor(dc, RGB(170, 170, 178));
            SetBkColor(dc, RGB(24, 24, 27));
            return (LRESULT)g_settings_brush;
        }

        case WM_COMMAND:
            if (LOWORD(wParam) == SETTINGS_CLOSE_ID) {
                DestroyWindow(hwnd);
                return 0;
            }
            if (LOWORD(wParam) == SETTINGS_SAVE_ID) {
                int i;
                wchar_t a[64], b[64];
                GetWindowTextW(g_settings_key, g_groq_key, 512);
                for (i = 0; i < MAX_SLASH_COMMANDS; ++i) {
                    GetWindowTextW(g_settings_cmd[i], a, 64);
                    GetWindowTextW(g_settings_action[i], b, 64);
                    WideCharToMultiByte(CP_UTF8, 0, a, -1, g_slash_cmd[i], 32, NULL, NULL);
                    WideCharToMultiByte(CP_UTF8, 0, b, -1, g_slash_action[i], 32, NULL, NULL);
                }
                DestroyWindow(hwnd);
                return 0;
            }
            if (LOWORD(wParam) == SETTINGS_CANCEL_ID) {
                DestroyWindow(hwnd);
                return 0;
            }
            break;

        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            RECT rc;
            HBRUSH bg = CreateSolidBrush(RGB(24, 24, 27));
            HPEN pen = CreatePen(PS_SOLID, 1, RGB(55, 55, 62));
            HGDIOBJ oldb, oldp;
            GetClientRect(hwnd, &rc);
            oldb = SelectObject(hdc, bg);
            oldp = SelectObject(hdc, pen);
            RoundRect(hdc, 1, 1, rc.right - 1, rc.bottom - 1, 22, 22);
            SelectObject(hdc, oldb);
            SelectObject(hdc, oldp);
            DeleteObject(bg);
            DeleteObject(pen);
            SetBkMode(hdc, TRANSPARENT);
            SetTextColor(hdc, RGB(235, 235, 238));
            TextOutW(hdc, 20, 16, L"screenask settings", 18);
            EndPaint(hwnd, &ps);
            return 0;
        }

        case WM_ERASEBKGND:
            return 1;

        case WM_CLOSE:
            DestroyWindow(hwnd);
            return 0;

        case WM_DESTROY:
            if (g_settings == hwnd) {
                g_settings = NULL;
                g_settings_key = NULL;
            }
            if (g_settings_brush) { DeleteObject(g_settings_brush); g_settings_brush = NULL; }
            if (g_settings_edit_brush) { DeleteObject(g_settings_edit_brush); g_settings_edit_brush = NULL; }
            SetForegroundWindow(g_hwnd);
            SetFocus(g_edit);
            return 0;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

static void open_settings(void) {
    WNDCLASSEXW wc;
    RECT work;
    int x, y;

    if (g_settings) {
        SetForegroundWindow(g_settings);
        SetFocus(g_settings_key);
        return;
    }

    ZeroMemory(&wc, sizeof(wc));
    wc.cbSize = sizeof(wc);
    wc.hInstance = GetModuleHandleW(NULL);
    wc.lpfnWndProc = settings_proc;
    wc.lpszClassName = L"ScreenAskSettings";
    wc.hCursor = LoadCursorW(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)GetStockObject(NULL_BRUSH);
    RegisterClassExW(&wc);

    x = 0;
    y = 0;
    if (SystemParametersInfoW(SPI_GETWORKAREA, 0, &work, 0)) {
        x = work.left + (work.right - work.left - 460) / 2;
        y = work.top + (work.bottom - work.top - 530) / 2;
    }

    g_settings = CreateWindowExW(WS_EX_TOOLWINDOW | WS_EX_TOPMOST,
                                 L"ScreenAskSettings", L"",
                                 WS_POPUP,
                                 x, y, 460, 530, g_hwnd, NULL,
                                 GetModuleHandleW(NULL), NULL);
    if (g_settings) {
        HRGN rgn = CreateRoundRectRgn(0, 0, 461, 531, 22, 22);
        if (rgn) SetWindowRgn(g_settings, rgn, TRUE);
        ShowWindow(g_settings, SW_SHOW);
        UpdateWindow(g_settings);
        SetForegroundWindow(g_settings);
        SetFocus(g_settings_key);
    }
}

static LRESULT CALLBACK wnd_proc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE:
            g_font = CreateFontW(-16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                 DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                 CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
            g_edit_brush = (HBRUSH)GetStockObject(NULL_BRUSH);
            g_glass_assets_ok = glassbar_load_assets() && create_glass_surface();
            if (!g_glass_assets_ok) {
                MessageBoxW(hwnd, L"The glass bar could not initialize. Check assets\\ and try again; the standard bar will be used as a fallback.", APP_TITLE, MB_ICONWARNING | MB_OK);
            }
            g_slider_label_font = CreateFontW(-10, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
            SetWindowDisplayAffinity(hwnd, WDA_EXCLUDEFROMCAPTURE);
            g_edit = CreateWindowExW(WS_EX_TRANSPARENT, L"EDIT", L"", WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_AUTOVSCROLL | ES_WANTRETURN,
                                     g_bar_x + 58, 9, g_bar_w - 300, g_bar_h - 18,
                                     hwnd, (HMENU)100, GetModuleHandleW(NULL), NULL);
            if (!g_edit) return -1;
            /* A WS_POPUP window uses hMenu as a menu handle, not a control ID.
               Passing BROWSER_BTN_ID here can make CreateWindowExW fail with
               ERROR_INVALID_MENU_HANDLE and causes the whole WM_CREATE to fail.
               Create it with a NULL menu, then assign the command ID afterward. */
            SetLastError(ERROR_SUCCESS);
            g_browser_button = CreateWindowExW(WS_EX_TOOLWINDOW | WS_EX_TOPMOST, L"BUTTON", L"move to browser",
                                               WS_POPUP | BS_PUSHBUTTON | WS_TABSTOP,
                                               0, 0, 150, 32, hwnd, NULL,
                                               GetModuleHandleW(NULL), NULL);
            if (!g_browser_button) {
                DWORD browser_button_error = GetLastError();
                wchar_t browser_button_message[256];
                _snwprintf(browser_button_message, 255,
                           L"Could not create Move to Browser button (Win32 error %lu).", 
                           (unsigned long)browser_button_error);
                browser_button_message[255] = L'\0';
                MessageBoxW(hwnd, browser_button_message, APP_TITLE, MB_ICONERROR | MB_OK);
                return -1;
            }
            SetWindowLongPtrW(g_browser_button, GWLP_ID, (LONG_PTR)BROWSER_BTN_ID);
            g_old_browser_button_proc = (WNDPROC)SetWindowLongPtrW(
                g_browser_button, GWLP_WNDPROC, (LONG_PTR)browser_button_proc);
            SendMessageW(g_browser_button, WM_SETFONT, (WPARAM)g_font, TRUE);
            SendMessageW(g_edit, WM_SETFONT, (WPARAM)g_font, TRUE);
            SendMessageW(g_edit, EM_SETMARGINS, EC_LEFTMARGIN | EC_RIGHTMARGIN, MAKELONG(4, 4));
                    g_old_edit_proc = (WNDPROC)SetWindowLongPtrW(g_edit, GWLP_WNDPROC, (LONG_PTR)edit_proc);
            make_round(hwnd);
            return 0;

        case WM_OPEN_SEARCH_BROWSER:
            if (g_visible && g_search_state != SEARCH_IDLE)
                open_search_in_browser();
            return 0;

        case WM_COMMAND:
            /* Ignore BN_CLICKED notifications: external navigation is handled
               only by WM_OPEN_SEARCH_BROWSER from a confirmed mouse click. */
            if (LOWORD(wParam) == BROWSER_BTN_ID && HIWORD(wParam) == BN_CLICKED)
                return 0;
            break;

        case WM_SIZE:
            if (g_webview_controller) {
                resize_embedded_webview();
                if (wParam == SIZE_MINIMIZED) g_webview_controller->put_IsVisible(FALSE);
                else if (g_search_state != SEARCH_IDLE) g_webview_controller->put_IsVisible(TRUE);
            }
            return 0;

        case WM_MOVE:
            if (g_webview_controller) g_webview_controller->NotifyParentWindowPositionChanged();
            return 0;

        case WM_ACTIVATE: {
            BOOL now_focused = (LOWORD(wParam) != WA_INACTIVE);
            if (now_focused != g_bar_focused) {
                g_bar_focused = now_focused;
                /* Hide the edit child while idle and defocused. The HWND's own
                   width/height animate down; this is not a clipping-only effect. */
                if (g_edit && g_search_state == SEARCH_IDLE && !g_busy) {
                    ShowWindow(g_edit, g_bar_focused ? SW_SHOW : SW_HIDE);
                }
                /* Defocused width stays compact. For WebView commands, halve the
                   actual current custom height on blur and restore it on focus. */
                g_bar_w_target = g_bar_focused ? 810 : 203;
                if (g_search_state != SEARCH_IDLE) {
                    if (g_bar_focused) {
                        g_bar_target_h = g_webview_focused_height;
                    } else {
                        g_webview_focused_height = g_bar_h > 0 ? g_bar_h : g_bar_target_h;
                        g_bar_target_h = g_webview_focused_height / 2;
                        if (g_bar_target_h < 27) g_bar_target_h = 27;
                    }
                    g_bar_h_v = (float)(g_bar_target_h - g_bar_h_float) * 0.18f;
                } else if (!g_busy) {
                    g_bar_target_h = g_bar_focused ? 54 : 27;
                    g_bar_h_v = (float)(g_bar_target_h - g_bar_h_float) * 0.18f;
                }
                /* Kick the same damped spring used by the arrow bounce so the
                   width visibly overshoots and settles instead of just easing. */
                g_bar_w_velocity = g_bar_focused ? 6.0f : -6.0f;
                g_last_blur_frame = 0;
                InvalidateRect(hwnd, NULL, FALSE);
                if (g_edit) InvalidateRect(g_edit, NULL, FALSE);
            }
            return DefWindowProcW(hwnd, msg, wParam, lParam);
        }

        case WM_MOUSEMOVE: {
            TRACKMOUSEEVENT tme;
            POINT pt;
            int hit;
            if (hwnd != g_hwnd || !g_visible) return 0;
            ZeroMemory(&tme, sizeof(tme));
            tme.cbSize = sizeof(tme); tme.dwFlags = TME_LEAVE; tme.hwndTrack = hwnd;
            TrackMouseEvent(&tme);
            pt.x = GET_X_LPARAM(lParam); pt.y = GET_Y_LPARAM(lParam);
            if (g_webview_resizing && (wParam & MK_LBUTTON) && g_search_state != SEARCH_IDLE) {
                /* WebView commands resize vertically only; keep the original width. */
                int nw = g_webview_resize_start_w;
                int nh = g_webview_resize_start_h + (pt.y - g_webview_resize_start.y);
                if (nh < 260) nh = 260;
                if (nh > 1200) nh = 1200;
                g_bar_w_target = nw;
                g_bar_w_float = (float)nw;
                g_bar_w = g_window_w = nw;
                g_bar_target_h = nh;
                g_webview_focused_height = nh;
                g_bar_h_float = (float)nh;
                g_bar_h = nh;
                g_bar_h_v = 0.0f;
                /* Keep the WebView bar horizontally centered during corner resizing.
                   A previous SWP_NOMOVE resize could preserve a stale left edge after
                   a defocused drag/restore, making the window appear to jump off-screen. */
                {
                    RECT work = {0};
                    RECT wr = {0};
                    POINT center_pt;
                    int x;
                    int y;
                    HMONITOR mon = MonitorFromWindow(g_hwnd, MONITOR_DEFAULTTONEAREST);
                    MONITORINFO mi;
                    ZeroMemory(&mi, sizeof(mi));
                    mi.cbSize = sizeof(mi);
                    if (mon && GetMonitorInfoW(mon, &mi)) {
                        work = mi.rcWork;
                    } else {
                        work.left = 0;
                        work.right = GetSystemMetrics(SM_CXSCREEN);
                    }
                    x = work.left + ((work.right - work.left) - nw) / 2;
                    if (x < work.left + 8) x = work.left + 8;
                    if (x + nw > work.right - 8) x = work.right - nw - 8;
                    if (x < work.left) x = work.left;
                    y = 0;
                    if (GetWindowRect(g_hwnd, &wr)) y = wr.top;
                    SetWindowPos(g_hwnd, NULL, x, y, nw, nh,
                                 SWP_NOZORDER | SWP_NOACTIVATE);
                }
                make_round(g_hwnd);
                resize_embedded_webview();
                position_browser_button();
                InvalidateRect(g_hwnd, NULL, FALSE);
                return 0;
            }
            hit = ui_hit_test(pt.x, pt.y);
            ui_set_hover(hit);
            if (g_slider_dragging && (wParam & MK_LBUTTON)) {
                int value = ((pt.x - 630) * 100) / 72;
                if (value < 0) value = 0; if (value > 100) value = 100;
                g_reasoning_value = value;
                InvalidateRect(hwnd, NULL, FALSE);
            }
            return 0;
        }

        case WM_MOUSELEAVE:
            if (hwnd == g_hwnd) ui_set_hover(0);
            return 0;

        case WM_LBUTTONUP:
            if (g_webview_resizing) {
                g_webview_resizing = FALSE;
                ReleaseCapture();
                return 0;
            }
            if (g_slider_dragging) {
                g_slider_dragging = FALSE;
                ReleaseCapture();
            }
            return 0;

        case WM_HOTKEY:
            if (wParam == ESC_HOTKEY_ID && g_task_active) {
                InterlockedExchange(&g_task_stop, 1);
                hide_control_overlay();
                post_task_status("screenask: automation stopped");
                return 0;
            }
            if (wParam == HOTKEY_ID) {
                if (g_visible) {
                    hide_bar();
                } else {
                    HWND previous = GetForegroundWindow();
                    if (previous && previous != hwnd)
                        g_task_target_window = previous;
                    show_bar();
                }
            }
            return 0;

        case WM_SEARCH_READY:
            if (g_search_state == SEARCH_LOADING) {
                g_search_state = SEARCH_ACTIVE;
                g_throbber_frame = 0;
                InvalidateRect(hwnd, NULL, FALSE);
            }
            return 0;

        case WM_TIMER:
            if (wParam == TIMER_ID && g_visible) {
                update_arrows();
                update_ui_spring();
                update_bar_width();
                /* Keep the backdrop genuinely live in BOTH focus states.
                   capture_live_glass() refreshes at most every 32 ms, but it
                   only runs from paint_window(), so the timer must request a
                   repaint even when the bar is unfocused.  Without this,
                   the blurred image remains a stale snapshot after defocus. */
                if (g_search_state == SEARCH_IDLE && !g_busy &&
                    (g_last_blur_frame == 0 ||
                     GetTickCount64() - g_last_blur_frame >= 32)) {
                    InvalidateRect(hwnd, NULL, FALSE);
                    if (g_edit) InvalidateRect(g_edit, NULL, FALSE);
                }
                if (g_task_active) {
                    ++g_control_overlay_pulse;
                    update_control_overlay();
                    position_arrow_windows();
                }
                update_response_box();
                if (g_busy || g_search_state == SEARCH_LOADING) {
                    ++g_throbber_frame;
                    InvalidateRect(hwnd, NULL, FALSE);
                }
            }
            return 0;

        case WM_LBUTTONDOWN: {
            int x = GET_X_LPARAM(lParam);
            int y = GET_Y_LPARAM(lParam);
            int hit = ui_hit_test(x, y);
            /* Grab the visible bottom-right corner grip to resize height only. */
            if (g_search_state != SEARCH_IDLE &&
                x >= g_bar_w - 30 && y >= g_bar_h - 24) {
                g_webview_resizing = TRUE;
                g_webview_resize_start.x = x;
                g_webview_resize_start.y = y;
                g_webview_resize_start_w = g_window_w;
                g_webview_resize_start_h = g_bar_h;
                SetCapture(hwnd);
                return 0;
            }
            /* In webpage mode, let the clear middle of the compact header act
               as a drag handle, so the embedded WebView bar can be moved again
               without stealing clicks from the search, camera, settings, or
               slider controls. */
            if (g_search_state != SEARCH_IDLE && y >= 0 && y < 54 &&
                x >= 180 && x < 590) {
                ReleaseCapture();
                SendMessageW(hwnd, WM_NCLBUTTONDOWN, HTCAPTION, 0);
                return 0;
            }
            if (hit == UI_HIT_SEARCH) {
                if (g_edit) SetFocus(g_edit);
                return 0;
            }
            if (hit == UI_HIT_CAMERA) {
                g_screenshot_enabled = !g_screenshot_enabled;
                if (!g_screenshot_enabled) {
                    clear_preview();
                } else {
                    HBITMAP camera_bmp = NULL;
                    if (save_temp_screenshot(&camera_bmp)) {
                        if (g_preview) DeleteObject(g_preview);
                        g_preview = camera_bmp;
                    } else {
                        g_screenshot_enabled = 0;
                        MessageBoxW(hwnd, L"couldn't capture the screen.", APP_TITLE, MB_ICONERROR);
                    }
                }
                InvalidateRect(hwnd, NULL, FALSE);
                return 0;
            }
            if (hit == UI_HIT_SLIDER) {
                g_slider_dragging = TRUE;
                SetCapture(hwnd);
                g_reasoning_value = ((x - 630) * 100) / 72;
                if (g_reasoning_value < 0) g_reasoning_value = 0;
                if (g_reasoning_value > 100) g_reasoning_value = 100;
                InvalidateRect(hwnd, NULL, FALSE);
                return 0;
            }
            if (hit == UI_HIT_SETTINGS) {
                open_settings();
                return 0;
            }
            break;
        }

        case WM_SUBMIT:
            submit_prompt();
            return 0;

        case WM_HIDE_BAR:
            if (g_task_active) { InterlockedExchange(&g_task_stop, 1); return 0; }
            if (!g_busy) hide_bar();
            return 0;

        case WM_TEXT_CHANGED:
            if (g_edit && g_visible && !g_busy) {
                int len = GetWindowTextLengthW(g_edit);
                wchar_t *text = (wchar_t *)calloc((size_t)len + 1, sizeof(wchar_t));
                if (text) {
                    GetWindowTextW(g_edit, text, len + 1);
                    resize_response_box(text);
                    free(text);
                }
            }
            return 0;

        case WM_TASK_STATUS: {
            char *msg8 = (char *)lParam;
            if (g_search_state != SEARCH_IDLE) {
                if (msg8) free(msg8);
                return 0;
            }
            if (wParam == 1) {
                g_task_active = 0;
                g_busy = 0;
                hide_control_overlay();
                UnregisterHotKey(hwnd, ESC_HOTKEY_ID);
                ShowWindow(hwnd, SW_SHOWNORMAL);
                SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
                if (g_arrow_l) ShowWindow(g_arrow_l, SW_SHOW);
                if (g_arrow_r) ShowWindow(g_arrow_r, SW_SHOW);
                ShowWindow(g_edit, SW_SHOW);
                EnableWindow(g_edit, TRUE);
                if (msg8 && msg8[0]) {
                    wchar_t *w = utf8_to_wide(msg8, -1);
                    if (w) { SetWindowTextW(g_edit, w); resize_response_box(w); free(w); }
                } else {
                    SetWindowTextW(g_edit, L"");
                    resize_response_box(L"");
                }
                SetFocus(g_edit);
                if (msg8) free(msg8);
                InvalidateRect(hwnd, NULL, FALSE);
                return 0;
            }
            if (msg8) {
                /* Status text is never prompt text. Do not write asynchronous
                   status messages into the edit control; this prevents focus
                   loss or a failed browser action from destroying user input. */
                free(msg8);
                InvalidateRect(hwnd, NULL, FALSE);
            }
            return 0;
        }

        case WM_ASK_DONE: {
            char *answer8 = (char *)lParam;
            wchar_t *answerw = utf8_to_wide(answer8, -1);
            if (!answerw) answerw = _wcsdup(L"ai returned an unreadable response.");
            SetWindowTextW(g_edit, answerw);
            resize_response_box(answerw);
            set_clipboard_text(g_hwnd, answerw);
            g_busy = 0;
            ShowWindow(g_edit, SW_SHOW);
            EnableWindow(g_edit, TRUE);
            InvalidateRect(g_hwnd, NULL, FALSE);
            SetFocus(g_edit);
            free(answerw);
            free(answer8);
            return 0;
        }

        case WM_ASK_FAIL: {
            char *msg8 = (char *)lParam;
            wchar_t *msgw = utf8_to_wide(msg8, -1);
            if (!msgw) msgw = _wcsdup(L"ai request failed.");
            SetWindowTextW(g_edit, msgw);
            resize_response_box(msgw);
            g_busy = 0;
            ShowWindow(g_edit, SW_SHOW);
            EnableWindow(g_edit, TRUE);
            InvalidateRect(g_hwnd, NULL, FALSE);
            SetFocus(g_edit);
            free(msgw);
            free(msg8);
            return 0;
        }

        case WM_CTLCOLOREDIT: {
            HDC dc = (HDC)wParam;
            SetTextColor(dc, RGB(241, 247, 253));
            SetBkMode(dc, TRANSPARENT);
            return (LRESULT)GetStockObject(NULL_BRUSH);
        }

        case WM_ERASEBKGND:
            return 1;

        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            paint_window(hwnd, hdc);
            EndPaint(hwnd, &ps);
            return 0;
        }

        case WM_DESTROY:
            InterlockedExchange(&g_task_stop, 1);
            if (g_webview_controller) {
                g_webview_controller->Close();
                g_webview_controller->Release();
                g_webview_controller = NULL;
            }
            if (g_webview) {
                g_webview->Release();
                g_webview = NULL;
            }
            if (g_webview_env) {
                g_webview_env->Release();
                g_webview_env = NULL;
            }
            if (g_webview_loader) {
                FreeLibrary(g_webview_loader);
                g_webview_loader = NULL;
            }
            /* WebView2 may take a moment to release its profile files. */
            if (g_webview_temp_profile[0]) {
                Sleep(250);
                delete_temp_tree(g_webview_temp_profile);
                RemoveDirectoryW(g_webview_temp_profile);
                g_webview_temp_profile[0] = 0;
            }
            hide_control_overlay();
            UnregisterHotKey(hwnd, ESC_HOTKEY_ID);
            if (g_control_overlay) DestroyWindow(g_control_overlay);
            if (g_settings) DestroyWindow(g_settings);
            UnregisterHotKey(hwnd, HOTKEY_ID);
            KillTimer(hwnd, TIMER_ID);
            if (g_preview) DeleteObject(g_preview);
            if (g_font) DeleteObject(g_font);
            if (g_slider_label_font) DeleteObject(g_slider_label_font);
            if (g_edit_brush && g_edit_brush != (HBRUSH)GetStockObject(NULL_BRUSH)) DeleteObject(g_edit_brush);
            glassbar_release_assets();
            destroy_glass_surface();
            if (g_image_path[0]) DeleteFileW(g_image_path);
            PostQuitMessage(0);
            return 0;

        default:
            return DefWindowProcW(hwnd, msg, wParam, lParam);
    }
    /* Defensive fallback for compilers that do not prove every path returns. */
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

int WINAPI wWinMain(HINSTANCE hinst, HINSTANCE prev, PWSTR cmd, int show) {
    WNDCLASSEXW wc;
    MSG msg;
    GdiplusStartupInput input;

    (void)prev;
    (void)cmd;
    (void)show;

    load_groq_key_from_file();

    ZeroMemory(&input, sizeof(input));
    input.GdiplusVersion = 1;
    if (GdiplusStartup(&g_gdiplus_token, &input, NULL) != 0) {
        MessageBoxW(NULL, L"couldn't start gdi+.", APP_TITLE, MB_ICONERROR);
        return 1;
    }

    ZeroMemory(&wc, sizeof(wc));
    wc.cbSize = sizeof(wc);
    wc.hInstance = hinst;
    wc.lpfnWndProc = wnd_proc;
    wc.lpszClassName = APP_CLASS;
    wc.hCursor = LoadCursorW(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    if (!RegisterClassExW(&wc)) {
        GdiplusShutdown(g_gdiplus_token);
        return 1;
    }

    {
        WNDCLASSEXW owc;
        ZeroMemory(&owc, sizeof(owc));
        owc.cbSize = sizeof(owc);
        owc.hInstance = hinst;
        owc.lpfnWndProc = control_overlay_proc;
        owc.lpszClassName = CONTROL_OVERLAY_CLASS;
        owc.hCursor = LoadCursorW(NULL, IDC_ARROW);
        owc.hbrBackground = (HBRUSH)GetStockObject(NULL_BRUSH);
        if (!RegisterClassExW(&owc)) {
            GdiplusShutdown(g_gdiplus_token);
            return 1;
        }
    }

    {
        WNDCLASSEXW awc;
        ZeroMemory(&awc, sizeof(awc));
        awc.cbSize = sizeof(awc);
        awc.hInstance = hinst;
        awc.lpfnWndProc = arrow_proc;
        awc.lpszClassName = L"ScreenAskArrow";
        awc.hCursor = LoadCursorW(NULL, IDC_ARROW);
        awc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
        if (!RegisterClassExW(&awc)) {
            GdiplusShutdown(g_gdiplus_token);
            return 1;
        }
    }

    g_hwnd = CreateWindowExW(
        WS_EX_TOOLWINDOW | WS_EX_TOPMOST,
        APP_CLASS, APP_TITLE, WS_POPUP,
        0, 0, g_window_w, g_bar_h,
        NULL, NULL, hinst, NULL);
    if (!g_hwnd) {
        GdiplusShutdown(g_gdiplus_token);
        return 1;
    }

    g_control_overlay = CreateWindowExW(
        WS_EX_TOOLWINDOW | WS_EX_TOPMOST | WS_EX_LAYERED | WS_EX_NOACTIVATE | WS_EX_TRANSPARENT,
        CONTROL_OVERLAY_CLASS, L"", WS_POPUP,
        0, 0, GetSystemMetrics(SM_CXVIRTUALSCREEN), GetSystemMetrics(SM_CYVIRTUALSCREEN),
        NULL, NULL, hinst, NULL);
    if (!g_control_overlay) {
        DestroyWindow(g_hwnd);
        GdiplusShutdown(g_gdiplus_token);
        return 1;
    }

    g_arrow_font = CreateFontW(-30, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                               DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                               CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");

    g_arrow_l = CreateWindowExW(
        WS_EX_TOOLWINDOW | WS_EX_TOPMOST | WS_EX_LAYERED | WS_EX_NOACTIVATE,
        L"ScreenAskArrow", L"", WS_POPUP,
        0, 0, 42, 42, NULL, NULL, hinst, NULL);
    g_arrow_r = CreateWindowExW(
        WS_EX_TOOLWINDOW | WS_EX_TOPMOST | WS_EX_LAYERED | WS_EX_NOACTIVATE,
        L"ScreenAskArrow", L"", WS_POPUP,
        0, 0, 42, 42, NULL, NULL, hinst, NULL);

    if (!g_arrow_l || !g_arrow_r) {
        if (g_arrow_l) DestroyWindow(g_arrow_l);
        if (g_arrow_r) DestroyWindow(g_arrow_r);
        DestroyWindow(g_hwnd);
        GdiplusShutdown(g_gdiplus_token);
        return 1;
    }

    SetLayeredWindowAttributes(g_arrow_l, RGB(0, 0, 0), 0, LWA_COLORKEY);
    SetLayeredWindowAttributes(g_arrow_r, RGB(0, 0, 0), 0, LWA_COLORKEY);

    make_round(g_hwnd);

    if (!RegisterHotKey(g_hwnd, HOTKEY_ID, MOD_ALT | MOD_NOREPEAT, VK_SPACE)) {
        MessageBoxW(NULL,
                    L"couldn't register alt + space. another app may already be using it.",
                    APP_TITLE, MB_ICONERROR);
        DestroyWindow(g_hwnd);
        GdiplusShutdown(g_gdiplus_token);
        return 1;
    }

    ShowWindow(g_hwnd, SW_HIDE);

    while (GetMessageW(&msg, NULL, 0, 0) > 0) {
        if (g_settings && msg.message == WM_KEYDOWN && msg.wParam == VK_ESCAPE &&
            (msg.hwnd == g_settings || IsChild(g_settings, msg.hwnd))) {
            DestroyWindow(g_settings);
            continue;
        }
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    GdiplusShutdown(g_gdiplus_token);
    return 0;
}
