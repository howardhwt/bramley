// Drawing layer for the launcher: rounded textured quads, rounded fills and text, in GL on
// the context NextUI's GFX_init() creates. Coordinates are 1024x768 screen pixels, top-left origin.
#ifndef LGFX_H
#define LGFX_H

#include "sdl.h"

typedef struct {
	GLuint id;
	int w, h;
} LTex;

enum { LFONT_SMALL, LFONT_BODY, LFONT_TITLE, LFONT_COUNT };

void lgfx_init(int screen_w, int screen_h);
void lgfx_quit(void);

void lgfx_begin_frame(void);
void lgfx_end_frame(void);

// Textures are uploaded top row first. pixel_art uses nearest-neighbour magnification.
LTex* ltex_from_surface(SDL_Surface* surface, int pixel_art);
LTex* ltex_load(const char* path, int pixel_art);
LTex* ltex_text(int font, const char* text, int max_w);
void ltex_free(LTex* tex);

// Draws tex into the rect, cropping it to fill the rect without stretching.
void lgfx_draw_tex(LTex* tex, float x, float y, float w, float h, float radius, float alpha);
void lgfx_draw_fill(float x, float y, float w, float h, float radius, float r, float g, float b, float a);
void lgfx_draw_shadow(float x, float y, float w, float h, float lift, float alpha);
void lgfx_draw_wallpaper(LTex* tex, float offset_x, float offset_y);

#endif
