// Drawing layer for the launcher: rounded textured quads, rounded fills and text, in GL on
// the context NextUI's GFX_init() creates. Coordinates are 1024x768 screen pixels, top-left origin.
#ifndef LGFX_H
#define LGFX_H

#include "sdl.h"

typedef struct {
	GLuint id;
	int w, h;
} LTex;

// SMALL and BODY are Inter Regular, LABEL is SemiBold, TITLE and HERO are Bold.
// Bramley's interface colors (docs/brand/BRAND.md), as r, g, b arguments for lgfx_draw_fill.
#define BRAMLEY_ACCENT 0.290f, 0.698f, 0.471f // #4AB278: on, selected, progress
#define BRAMLEY_ALERT 0.898f, 0.282f, 0.302f  // #E5484D: warnings only

enum { LFONT_SMALL, LFONT_LABEL, LFONT_BODY, LFONT_TITLE, LFONT_HERO, LFONT_COUNT };

void lgfx_init(int screen_w, int screen_h);
void lgfx_quit(void);
// Runs one snapshot blur so the driver prepares its targets. Call it once after the first frame,
// where its cost (about 40 ms the first time) is not on the way to the screen.
void lgfx_warm_up(void);
// For programs that also draw through SDL's renderer: save the GL state before a frame of ours
// and restore it after the swap, so SDL finds it as it left it.
void lgfx_save_gl_state(void);
void lgfx_restore_gl_state(void);

void lgfx_begin_frame(void);
void lgfx_end_frame(void);

// Textures are uploaded top row first. pixel_art uses nearest-neighbour magnification.
LTex* ltex_from_surface(SDL_Surface* surface, int pixel_art);
LTex* ltex_load(const char* path, int pixel_art);
LTex* ltex_text(int font, const char* text, int max_w);
// Word-wrapped text of at most max_lines lines, centred or left aligned; the last line ends in an
// ellipsis if cut.
LTex* ltex_text_wrapped(int font, const char* text, int max_w, int max_lines, int centered);
void ltex_free(LTex* tex);

// Draws tex into the rect, cropping it to fill the rect without stretching.
void lgfx_draw_tex(LTex* tex, float x, float y, float w, float h, float radius, float alpha);
void lgfx_draw_fill(float x, float y, float w, float h, float radius, float r, float g, float b, float a);
// Vertical gradient from the top colour to the bottom colour.
void lgfx_draw_gradient(float x, float y, float w, float h, float radius, const float top[4], const float bottom[4]);
// A soft diagonal band of light across a rounded rect; t runs from 0 to 1 as it sweeps.
// Outline of a rounded rect, width pixels wide, drawn just inside the rect.
void lgfx_draw_ring(float x, float y, float w, float h, float radius, float width, float r, float g, float b, float a);
void lgfx_draw_shine(float x, float y, float w, float h, float radius, float t);
// Draws art to fill the rect: cropped when its shape is close to the rect's, otherwise shown whole
// over a blurred, darkened copy of itself.
void lgfx_draw_art(LTex* tex, float x, float y, float w, float h, float radius, float alpha);
// Draws tex scaled to fit inside the rect without cropping.
void lgfx_draw_tex_fit(LTex* tex, float x, float y, float w, float h, float radius, float alpha);
// Darkens everything drawn with the texture calls until set back to 1; 0.1 turns white text near black.
void lgfx_set_dim(float dim);
void lgfx_draw_shadow(float x, float y, float w, float h, float lift, float alpha);
void lgfx_draw_wallpaper(LTex* tex, float offset_x, float offset_y);

// Snapshot blur: draw a frame between lgfx_begin_snapshot() and lgfx_end_snapshot() and it is
// blurred once into a texture that lgfx_draw_snapshot() then shows as a frozen background.
void lgfx_begin_snapshot(void);
void lgfx_end_snapshot(void);
void lgfx_draw_snapshot(float alpha);

#endif
