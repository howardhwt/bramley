// Motion preview of Bramley's first-boot setup (docs/plans/v1-standalone.md, step 5), drawn with
// the launcher's GL kit and recorded frame by frame into a video. Not shipped.
//   DYLD_LIBRARY_PATH=/var/tmp/nextui/lib ./build/mock <home.png> <wallpaper-blur.jpg> <out.mp4>
// Scenes: the boot splash; the mark rising into "Welcome to Bramley" with the Wi-Fi list, joining
// a network and Continue; a push to Time Zone, choosing a city; Home with a card that explains
// MENU and SELECT.
#include <math.h>
#include <msettings.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "defines.h"
#include "api.h"
#include "lgfx.h"

#define W 1024
#define H 768
#define FPS 60

static LTex *mark, *home, *wall;
static float T; // seconds into the video

// --- motion helpers ---
static float clamp01(float x) { return x < 0 ? 0 : x > 1 ? 1 : x; }
static float ease_out(float x) { x = clamp01(x); return 1 - powf(1 - x, 3); }
static float ease_in_out(float x) { x = clamp01(x); return x < 0.5f ? 4 * x * x * x : 1 - powf(-2 * x + 2, 3) / 2; }
// a soft spring, settling with a small overshoot, as the launcher's focus does
static float spring(float x) {
	x = clamp01(x);
	return 1 - expf(-6.5f * x) * cosf(7.5f * x);
}
static float span(float a, float b) { return clamp01((T - a) / (b - a)); }
static float mix(float a, float b, float t) { return a + (b - a) * t; }

static float text(int font, const char* s, float x, float y, float alpha, int align) {
	if (alpha <= 0.01f) return 0;
	LTex* t = ltex_text(font, s, 0);
	if (!t) return 0;
	float w = t->w;
	lgfx_draw_tex(t, align == 1 ? x - t->w : align == 2 ? x - t->w * 0.5f : x, y, t->w, t->h, 0, alpha);
	ltex_free(t);
	return w;
}

static float text_w(int font, const char* s) {
	LTex* t = ltex_text(font, s, 0);
	float w = t ? t->w : 0;
	ltex_free(t);
	return w;
}

static void wrapped(const char* s, float x, float y, float w, float alpha) {
	LTex* t = ltex_text_wrapped(LFONT_BODY, s, (int)w, 2, 1);
	if (!t) return;
	lgfx_draw_tex(t, x + (w - t->w) * 0.5f, y, t->w, t->h, 0, alpha);
	ltex_free(t);
}

static void spinner(float cx, float cy, float r, float alpha) {
	for (int i = 0; i < 8; i++) {
		float a = i * 6.2831853f / 8 + T * 6.0f;
		float k = (float)((i + (int)(T * 12)) % 8) / 8.0f;
		lgfx_draw_fill(cx + cosf(a) * r - 2, cy + sinf(a) * r - 2, 4, 4, 2, 1, 1, 1, alpha * (0.25f + 0.75f * k));
	}
}

// Wi-Fi strength as three rising bars.
static void bars(float right, float cy, int level, float alpha, int dark) {
	float c = dark ? 0.1f : 1;
	for (int i = 0; i < 3; i++) {
		float h = 7 + i * 5, x = right - 30 + i * 9;
		lgfx_draw_fill(x, cy + 8 - h, 5, h, 2, c, c, c, alpha * (i < level ? 0.9f : 0.25f));
	}
}

// A row as Settings draws it: glass at rest, white and lifted when focused.
static void row(float x, float y, float w, float h, float focus, float alpha) {
	if (alpha <= 0.01f) return;
	if (focus > 0.01f) {
		float g = 6 * focus;
		lgfx_draw_shadow(x - g, y - g * 0.5f, w + 2 * g, h + g, 6, 0.32f * alpha * focus);
		lgfx_draw_fill(x - g, y - g * 0.5f, w + 2 * g, h + g, 12, 1, 1, 1, 0.96f * alpha * focus);
	}
	lgfx_draw_fill(x, y, w, h, 12, 1, 1, 1, 0.06f * alpha * (1 - focus));
}

static void hint(const char* s, float alpha) { text(LFONT_SMALL, s, 40, H - 40, 0.6f * alpha, 0); }

// --- scenes ---
static void backdrop(float alpha) {
	lgfx_draw_fill(0, 0, W, H, 0, 0, 0, 0, 1);
	if (wall) lgfx_draw_tex(wall, 0, 0, W, H, 0, alpha);
	lgfx_draw_fill(0, 0, W, H, 0, 0, 0, 0, 0.25f * alpha);
}

// The mark: from the splash (150 px, 20 px above the middle) up into the welcome header.
static void draw_mark(void) {
	float k = ease_in_out(span(1.55f, 2.35f));
	float out = 1 - ease_in_out(span(6.85f, 7.2f)); // leaves with the Wi-Fi page
	float s = mix(150, 84, k), cx = W * 0.5f - (1 - out) * 320, cy = mix(H * 0.5f - 20, 112, k);
	lgfx_draw_tex(mark, cx - s * 0.5f, cy - s * 0.5f, s, s, 0, out);
}

static void splash(void) {
	float a = 1 - ease_out(span(1.5f, 1.9f));
	float bw = 200, bx = (W - bw) * 0.5f, by = H * 0.5f + 110;
	float p = fminf(1, T / 1.1f);
	lgfx_draw_fill(bx, by, bw, 6, 3, 0.24f, 0.24f, 0.25f, a);
	lgfx_draw_fill(bx, by, fmaxf(6, bw * ease_out(p)), 6, 3, 0.92f, 0.92f, 0.92f, a);
}

static const char* nets[] = { "Orchard", "Orchard-5G", "Neighbours", "Coffee House" };
static const int levels[] = { 3, 3, 2, 1 };

static void wifi(void) {
	float in = ease_out(span(2.0f, 2.7f));
	float leave = ease_in_out(span(6.85f, 7.2f));
	float a = in * (1 - leave), dx = -leave * 320;
	if (a <= 0.01f) return;
	float rise = (1 - in) * 24;
	text(LFONT_TITLE, "Welcome to Bramley", W * 0.5f + dx, 176 + rise, a, 2);
	wrapped("Choose a Wi-Fi network. Bramley uses it to set the clock and to find updates.", 232 + dx, 228 + rise, 560, 0.6f * a);

	// focus: Orchard-5G, then Orchard; then the Continue button
	float x = 212 + dx, w = 600, rh = 52, gap = 8, y0 = 312;
	int join = T > 4.75f;
	float f_list = 1 - ease_out(span(6.05f, 6.3f));
	float pos = mix(1, 0, spring(span(3.85f, 4.35f))); // 1: second row, 0: first row
	if (T < 3.3f) pos = 0;
	else if (T < 3.85f) pos = spring(span(3.3f, 3.75f));
	for (int i = 0; i < 4; i++) {
		float st = ease_out(span(2.35f + i * 0.08f, 2.85f + i * 0.08f));
		float y = y0 + i * (rh + gap) + (1 - st) * 18;
		float focus = fmaxf(0, 1 - fabsf(pos - i)) * f_list;
		row(x, y, w, rh, focus, a * st);
		int dark = focus > 0.5f;
		if (dark) lgfx_set_dim(0.1f);
		text(LFONT_BODY, nets[i], x + 20, y + 11, a * st * (dark ? 1 : 0.92f), 0);
		float right = x + w - 20;
		if (i == 0 && join) {
			if (T < 5.75f) {
				spinner(right - 50, y + rh * 0.5f, 8, a * st);
				text(LFONT_BODY, "Joining\xE2\x80\xA6", right - 70, y + 11, a * st * 0.6f, 1);
			} else {
				float c = ease_out(span(5.75f, 6.0f));
				text(LFONT_BODY, "Connected", right - 40, y + 11, a * st * c * (dark ? 1 : 0.7f), 1);
				lgfx_set_dim(1);
				lgfx_draw_fill(right - 40 - text_w(LFONT_BODY, "Connected") - 22, y + rh * 0.5f - 6, 12, 12, 6, 0.290f, 0.698f, 0.471f, a * c);
				if (dark) lgfx_set_dim(0.1f);
			}
		}
		bars(right, y + rh * 0.5f, levels[i], a * st, dark);
		lgfx_set_dim(1);
	}
	// the press on Orchard
	if (T > 4.6f && T < 4.8f) lgfx_draw_fill(x - 6, y0 - 3, w + 12, rh + 6, 12, 0, 0, 0, 0.08f * a);

	// Continue, once connected
	float c = ease_out(span(5.9f, 6.3f));
	if (c > 0.01f) {
		float bw = 240, bh = 54, bx = W * 0.5f - bw * 0.5f + dx, by = y0 + 4 * (rh + gap) + 28 + (1 - c) * 12;
		float press = T > 6.6f && T < 6.8f ? 0.97f : 1;
		float pw = bw * press, ph = bh * press;
		float fx = bx + (bw - pw) * 0.5f, fy = by + (bh - ph) * 0.5f;
		lgfx_draw_shadow(fx, fy, pw, ph, 8, 0.3f * a * c);
		lgfx_draw_fill(fx, fy, pw, ph, ph * 0.5f, 0.290f, 0.698f, 0.471f, a * c);
		text(LFONT_BODY, "Continue", W * 0.5f + dx, by + 12, a * c, 2);
	}
	hint(T < 6.1f ? "A  Join        B  Set up later" : "A  Continue        B  Back", a);
}

static const char* cities[] = { "Chicago", "Halifax", "Los Angeles", "New York", "Toronto", "Vancouver" };

static void timezone(void) {
	float in = ease_out(span(7.15f, 7.7f));
	float leave = ease_in_out(span(10.4f, 10.9f));
	float a = in * (1 - leave), dx = (1 - in) * 320;
	if (a <= 0.01f) return;
	text(LFONT_TITLE, "Time Zone", W * 0.5f + dx, 104, a, 2);
	wrapped("The clock is set from the internet. Choose your city for the right hour.", 232 + dx, 156, 560, 0.6f * a);
	text(LFONT_LABEL, "AMERICA", 232 + dx, 236, 0.55f * a, 0);
	text(LFONT_LABEL, "L1  R1  Region", 792 + dx, 236, 0.4f * a, 1);
	float x = 212 + dx, w = 600, rh = 52, gap = 8, y0 = 268;
	// focus starts on the guess (Toronto) and moves up to New York
	float pos = mix(4, 3, spring(span(8.4f, 8.9f)));
	int chosen = T > 9.45f ? 3 : 4;
	float choose = ease_out(span(9.45f, 9.75f));
	for (int i = 0; i < 6; i++) {
		float st = ease_out(span(7.3f + i * 0.06f, 7.8f + i * 0.06f));
		float y = y0 + i * (rh + gap);
		float focus = fmaxf(0, 1 - fabsf(pos - i));
		row(x, y, w, rh, focus, a * st);
		int dark = focus > 0.5f;
		if (dark) lgfx_set_dim(0.1f);
		text(LFONT_BODY, cities[i], x + 20, y + 11, a * st * (dark ? 1 : 0.92f), 0);
		char t[32];
		snprintf(t, sizeof(t), "%s", i == 0 ? "13:52" : i == 1 ? "15:52" : i == 2 ? "11:52" : i == 5 ? "11:52" : "14:52");
		float right = x + w - 20;
		text(LFONT_BODY, t, right, y + 11, a * st * (dark ? 0.6f : 0.45f), 1);
		lgfx_set_dim(1);
		if (i == chosen) {
			float k = i == 3 ? choose : 1;
			float tw = text_w(LFONT_BODY, t);
			lgfx_draw_fill(right - tw - 30, y + rh * 0.5f - 6, 12, 12, 6, 0.290f, 0.698f, 0.471f, a * st * k);
		}
	}
	hint(T < 9.6f ? "A  Choose        L1 R1  Region        B  Skip" : "A  Continue        B  Back", a);
}

// Home, and the card that says how to get around.
static void home_scene(void) {
	float in = ease_out(span(10.55f, 11.2f));
	if (in <= 0.01f) return;
	float z = mix(1.04f, 1, in);
	float w = W * z, h = H * z;
	lgfx_draw_tex(home, (W - w) * 0.5f, (H - h) * 0.5f, w, h, 0, in);
	float c = spring(span(11.4f, 12.1f)) * (1 - ease_in_out(span(14.6f, 15.0f)));
	if (c <= 0.01f) return;
	float cw = 560, ch = 236, cx = (W - cw) * 0.5f, cy = H - ch - 64 + (1 - c) * 40;
	lgfx_draw_shadow(cx, cy, cw, ch, 18, 0.5f * c);
	lgfx_draw_fill(cx, cy, cw, ch, 26, 0.11f, 0.11f, 0.13f, 0.92f * c);
	lgfx_draw_ring(cx, cy, cw, ch, 26, 1, 1, 1, 1, 0.10f * c);
	text(LFONT_TITLE, "You\xE2\x80\x99re all set", W * 0.5f, cy + 24, c, 2);
	const char* keys[] = { "MENU", "SELECT" };
	const char* what[] = { "Quick settings, from anywhere", "Now Playing, for music and books" };
	for (int i = 0; i < 2; i++) {
		float ry = cy + 84 + i * 46;
		float kw = text_w(LFONT_LABEL, keys[i]) + 20, col = text_w(LFONT_LABEL, "SELECT") + 20; // one column for both lines
		lgfx_draw_fill(cx + 48, ry, kw, 30, 8, 1, 1, 1, 0.14f * c);
		text(LFONT_LABEL, keys[i], cx + 58, ry + 5, 0.9f * c, 0);
		text(LFONT_BODY, what[i], cx + 48 + col + 16, ry, 0.85f * c, 0);
	}
	float press = T > 14.3f && T < 14.5f ? 0.97f : 1;
	float bw = 200 * press, bh = 46 * press;
	lgfx_draw_fill(W * 0.5f - bw * 0.5f, cy + ch - 66 + (46 - bh) * 0.5f, bw, bh, bh * 0.5f, 0.290f, 0.698f, 0.471f, c);
	text(LFONT_BODY, "Get Started", W * 0.5f, cy + ch - 58, c, 2);
}

static void frame(void) {
	lgfx_begin_frame();
	float bd = ease_out(span(1.6f, 2.4f)) * (1 - ease_out(span(10.55f, 11.0f)));
	backdrop(bd);
	if (T < 2.0f) splash();
	if (T < 7.5f) draw_mark();
	wifi();
	timezone();
	home_scene();
}

int main(int argc, char* argv[]) {
	if (argc < 4) {
		fprintf(stderr, "usage: mock <home.png> <wallpaper-blur.jpg> <out.mp4>\n");
		return 1;
	}
	InitSettings();
	GFX_init(MODE_MAIN);
	lgfx_init(W, H);
	mark = ltex_load(RES_PATH "/bramley/brand/mark-150.png", 0);
	home = ltex_load(argv[1], 0);
	wall = ltex_load(argv[2], 0);
	if (!mark || !home || !wall) {
		fprintf(stderr, "missing an image\n");
		return 1;
	}
	int dw, dh;
	SDL_GL_GetDrawableSize(SDL_GL_GetCurrentWindow(), &dw, &dh);
	char cmd[1024];
	snprintf(cmd, sizeof(cmd),
	         "ffmpeg -y -loglevel error -f rawvideo -pix_fmt rgba -s %dx%d -r %d -i - -vf vflip,scale=1024:768 "
	         "-c:v libx264 -pix_fmt yuv420p -crf 18 '%s'",
	         dw, dh, FPS, argv[3]);
	FILE* ff = popen(cmd, "w");
	if (!ff) return 1;
	unsigned char* px = malloc((size_t)dw * dh * 4);
	int frames = (int)(15.4f * FPS);
	for (int i = 0; i < frames; i++) {
		T = (float)i / FPS;
		frame();
		glReadPixels(0, 0, dw, dh, GL_RGBA, GL_UNSIGNED_BYTE, px);
		fwrite(px, 1, (size_t)dw * dh * 4, ff);
		SDL_GL_SwapWindow(SDL_GL_GetCurrentWindow());
		if (i == (int)(5.0f * FPS) || i == (int)(9.0f * FPS) || i == (int)(13.0f * FPS)) {
			// a still of each scene, for a quick look
			char p[512];
			snprintf(p, sizeof(p), "%s-%d.raw", argv[3], i / FPS);
			FILE* f = fopen(p, "wb");
			if (f) {
				fwrite(px, 1, (size_t)dw * dh * 4, f);
				fclose(f);
			}
		}
	}
	pclose(ff);
	printf("%d frames, %dx%d\n", frames, dw, dh);
	return 0;
}
