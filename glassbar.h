#ifndef SCREENASK_GLASSBAR_H
#define SCREENASK_GLASSBAR_H

#include <windows.h>

#ifdef __cplusplus
extern "C" {
#endif

int glassbar_load_assets(void);
void glassbar_release_assets(void);
int glassbar_hit_test(int x, int y);
void glassbar_draw(HDC hdc, int draw_background, int camera_enabled,
                   int hover_hit, int reasoning_value, float hover_x,
                   float hover_y_offset, HFONT label_font);

#ifdef __cplusplus
}
#endif

#endif
