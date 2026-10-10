#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include "glassbar.h"

/* GDI+ flat API declarations. The host starts GDI+ before calling us. */
typedef struct GpImage GpImage;
typedef struct GpGraphics GpGraphics;
typedef int GpStatus;
extern GpStatus WINAPI GdipLoadImageFromFile(const WCHAR *filename, GpImage **image);
extern GpStatus WINAPI GdipGetImageWidth(GpImage *image, UINT *width);
extern GpStatus WINAPI GdipGetImageHeight(GpImage *image, UINT *height);
extern GpStatus WINAPI GdipCreateFromHDC(HDC hdc, GpGraphics **graphics);
extern GpStatus WINAPI GdipDeleteGraphics(GpGraphics *graphics);
extern GpStatus WINAPI GdipDrawImageRectI(GpGraphics *graphics, GpImage *image,
                                           INT x, INT y, INT width, INT height);
extern GpStatus WINAPI GdipDisposeImage(GpImage *image);

#define GLASSBAR_W 810
#define GLASSBAR_H 54
#define HIT_SEARCH 1
#define HIT_CAMERA 2
#define HIT_SLIDER 3
#define HIT_SETTINGS 4

typedef struct {
    GpImage *image;
    int width;
    int height;
} GlassImage;

static GlassImage g_background;
static GlassImage g_search;
static GlassImage g_camera;
static GlassImage g_settings;
static GlassImage g_active;
static GlassImage g_track;
static GlassImage g_knob;
static GlassImage g_divider;
static int g_loaded;

static void glassbar_asset_path(const wchar_t *name, wchar_t path[MAX_PATH])
{
    DWORD n = GetModuleFileNameW(NULL, path, MAX_PATH);
    wchar_t *slash;
    if (!n || n >= MAX_PATH) {
        path[0] = L'\0';
        return;
    }
    slash = wcsrchr(path, L'\\');
    if (!slash) {
        path[0] = L'\0';
        return;
    }
    slash[1] = L'\0';
    if (wcslen(path) + wcslen(L"assets\\") + wcslen(name) + 1 >= MAX_PATH) {
        path[0] = L'\0';
        return;
    }
    wcscat(path, L"assets\\");
    wcscat(path, name);
}

static int load_image(const wchar_t *filename, GlassImage *out)
{
    wchar_t path[MAX_PATH];
    UINT width = 0, height = 0;
    memset(out, 0, sizeof(*out));
    glassbar_asset_path(filename, path);
    if (!path[0]) return 0;
    if (GdipLoadImageFromFile(path, &out->image) != 0 || !out->image) return 0;
    if (GdipGetImageWidth(out->image, &width) != 0 ||
        GdipGetImageHeight(out->image, &height) != 0 ||
        width == 0 || height == 0 || width > 4096 || height > 4096) {
        GdipDisposeImage(out->image);
        memset(out, 0, sizeof(*out));
        return 0;
    }
    out->width = (int)width;
    out->height = (int)height;
    return 1;
}

static void release_image(GlassImage *img)
{
    if (img->image) GdipDisposeImage(img->image);
    memset(img, 0, sizeof(*img));
}

void glassbar_release_assets(void)
{
    release_image(&g_background);
    release_image(&g_search);
    release_image(&g_camera);
    release_image(&g_settings);
    release_image(&g_active);
    release_image(&g_track);
    release_image(&g_knob);
    release_image(&g_divider);
    g_loaded = 0;
}

int glassbar_load_assets(void)
{
    glassbar_release_assets();
    if (!load_image(L"bar_bg.png", &g_background) ||
        !load_image(L"search.png", &g_search) ||
        !load_image(L"camera.png", &g_camera) ||
        !load_image(L"settings.png", &g_settings) ||
        !load_image(L"button_active.png", &g_active) ||
        !load_image(L"slider_track.png", &g_track) ||
        !load_image(L"slider_knob.png", &g_knob) ||
        !load_image(L"divider.png", &g_divider)) {
        glassbar_release_assets();
        return 0;
    }
    g_loaded = 1;
    return 1;
}

int glassbar_hit_test(int x, int y)
{
    if (y < 0 || y > GLASSBAR_H) return 0;
    if (x >= 8 && x <= 53) return HIT_SEARCH;
    if (x >= 578 && x <= 620) return HIT_CAMERA;
    if (x >= 621 && x <= 728) return HIT_SLIDER;
    /* Settings is clickable only over the pixels occupied by the current
       settings asset. The previous UI used a larger, stale hit rectangle. */
    if (g_settings.image &&
        x >= 749 && x < 749 + g_settings.width &&
        y >= 13 && y < 13 + g_settings.height)
        return HIT_SETTINGS;
    return 0;
}

static void draw_image(GpGraphics *graphics, const GlassImage *img, int x, int y)
{
    if (graphics && img && img->image)
        GdipDrawImageRectI(graphics, img->image, x, y, img->width, img->height);
}

void glassbar_draw(HDC hdc, int draw_background, int camera_enabled,
                   int hover_hit, int reasoning_value, float hover_x,
                   float hover_y_offset, HFONT label_font)
{
    GpGraphics *graphics = NULL;
    HFONT old_font = NULL;
    HPEN pen = NULL;
    HGDIOBJ old_pen = NULL;
    int cx, top_y, bottom_y;
    POINT pts[3];
    (void)pts;
    if (!hdc || !g_loaded) return;

    if (reasoning_value < 0) reasoning_value = 0;
    if (reasoning_value > 100) reasoning_value = 100;
    if (GdipCreateFromHDC(hdc, &graphics) != 0 || !graphics) return;

    if (draw_background) draw_image(graphics, &g_background, 0, 0);
    if (hover_hit == HIT_SEARCH) draw_image(graphics, &g_active, 12, 8);
    if (camera_enabled) draw_image(graphics, &g_active, 582, 8);
    if (hover_hit == HIT_SETTINGS) draw_image(graphics, &g_active, 741, 8);
    draw_image(graphics, &g_search, 16, 13);
    draw_image(graphics, &g_camera, 587, 13);
    draw_image(graphics, &g_track, 625, 23);
    draw_image(graphics, &g_divider, 730, 12);
    draw_image(graphics, &g_settings, 749, 13);
    draw_image(graphics, &g_knob, 630 + (reasoning_value * 72) / 100, 19);
    GdipDeleteGraphics(graphics);

    if (label_font) {
        old_font = (HFONT)SelectObject(hdc, label_font);
        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, RGB(210, 222, 239));
        TextOutW(hdc, 625, 38, L"fast", 4);
        TextOutW(hdc, 694, 38, L"deep", 4);
        SelectObject(hdc, old_font);
    }

    if (hover_hit < HIT_SEARCH || hover_hit > HIT_SETTINGS) return;
    cx = (int)(hover_x + 0.5f);
    top_y = 1 + (int)(hover_y_offset + 0.5f);
    bottom_y = 45 - (int)(hover_y_offset + 0.5f);
    if (top_y < 0) top_y = 0;
    if (top_y > GLASSBAR_H - 8) top_y = GLASSBAR_H - 8;
    if (bottom_y < 4) bottom_y = 4;
    if (bottom_y > GLASSBAR_H - 6) bottom_y = GLASSBAR_H - 6;
    pen = CreatePen(PS_SOLID, 2, RGB(232, 243, 255));
    if (!pen) return;
    old_pen = SelectObject(hdc, pen);
    SetBkMode(hdc, TRANSPARENT);
    MoveToEx(hdc, cx - 5, top_y, NULL);
    LineTo(hdc, cx, top_y + 4);
    LineTo(hdc, cx + 5, top_y);
    MoveToEx(hdc, cx - 5, bottom_y + 4, NULL);
    LineTo(hdc, cx, bottom_y);
    LineTo(hdc, cx + 5, bottom_y + 4);
    if (old_pen && old_pen != HGDI_ERROR) SelectObject(hdc, old_pen);
    DeleteObject(pen);
}
