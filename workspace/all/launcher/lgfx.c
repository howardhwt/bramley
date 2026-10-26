#include <ctype.h>
#include <dirent.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "defines.h"
#include "api.h"
#include "utils.h"
#include "lgfx.h"

#define SHADOW_SIZE 256
#define SHADOW_INNER 160

static int screen_w, screen_h;
static int fb_w, fb_h;
static GLuint vao, vbo, p_tex, p_fill, p_shine, p_down, p_up;

typedef struct { GLuint tex, fbo; int w, h; } Target;
static Target snap, down[4], up[3];
static int snapshot_ready;
static LTex* shadow;
static TTF_Font* fonts[LFONT_COUNT];
static int font_sizes[LFONT_COUNT];

// Inter has no Chinese, Japanese or Korean characters. Each character is drawn with the first font
// in this chain that has it, so mixed text such as "百變天后 · 鄧紫棋" draws in one line. Source Han Sans
// (SIL OFL) ships in the Brick's stock firmware with 44,812 characters; NextUI's font1.ttf covers
// fewer; the macOS fonts only stand in when testing on a desktop. Missing files are skipped.
static const char* fallback_files[] = {
	"/usr/trimui/res/full.ttf",
	RES_PATH "/font1.ttf",
	"/System/Library/Fonts/Hiragino Sans GB.ttc",
	"/System/Library/Fonts/AppleSDGothicNeo.ttc",
};
#define FALLBACK_COUNT (int)(sizeof(fallback_files) / sizeof(fallback_files[0]))
static TTF_Font* fallbacks[LFONT_COUNT][FALLBACK_COUNT];
static char fallback_tried[LFONT_COUNT][FALLBACK_COUNT];

static const char* vs_src =
	"in vec2 a_pos;\n"
	"uniform vec4 u_rect;\n"
	"out vec2 v_uv;\n"
	"void main() {\n"
	"	v_uv = a_pos;\n"
	"	vec2 p = u_rect.xy + a_pos * u_rect.zw;\n"
	"	gl_Position = vec4(p * 2.0 - 1.0, 0.0, 1.0);\n"
	"}\n";

static const char* fs_sdf =
	"float sd_rrect(vec2 p, vec2 b, float r) {\n"
	"	vec2 q = abs(p) - b + r;\n"
	"	return length(max(q, 0.0)) + min(max(q.x, q.y), 0.0) - r;\n"
	"}\n";

static const char* fs_tex =
	"uniform sampler2D u_tex;\n"
	"uniform vec4 u_px;\n"
	"uniform float u_radius;\n"
	"uniform vec4 u_uv;\n"
	"uniform float u_alpha;\n"
	"in vec2 v_uv; FRAG_DECL\n"
	"void main() {\n"
	"	vec4 c = texture(u_tex, u_uv.xy + v_uv * u_uv.zw);\n"
	"	float a = 1.0;\n"
	"	if (u_radius > 0.0) {\n"
	"		vec2 local = gl_FragCoord.xy - (u_px.xy + u_px.zw * 0.5);\n"
	"		a = clamp(0.5 - sd_rrect(local, u_px.zw * 0.5, u_radius), 0.0, 1.0);\n"
	"	}\n"
	"	frag = vec4(c.rgb, c.a * a * u_alpha);\n"
	"}\n";

static const char* fs_fill =
	"uniform vec4 u_px;\n"
	"uniform float u_radius;\n"
	"uniform vec4 u_top;\n"
	"uniform vec4 u_bottom;\n"
	"uniform float u_stroke;\n"
	"in vec2 v_uv; FRAG_DECL\n"
	"void main() {\n"
	"	vec2 local = gl_FragCoord.xy - (u_px.xy + u_px.zw * 0.5);\n"
	"	float d = sd_rrect(local, u_px.zw * 0.5, u_radius);\n"
	"	float k = clamp((gl_FragCoord.y - u_px.y) / u_px.w, 0.0, 1.0);\n"
	"	vec4 c = mix(u_bottom, u_top, k);\n"
	"	float a = clamp(0.5 - d, 0.0, 1.0);\n"
	"	if (u_stroke > 0.0) a *= clamp(u_stroke + 0.5 + d, 0.0, 1.0);\n"
	"	frag = vec4(c.rgb, c.a * a);\n"
	"}\n";

static const char* fs_shine =
	"uniform vec4 u_px;\n"
	"uniform float u_radius;\n"
	"uniform float u_t;\n"
	"in vec2 v_uv; FRAG_DECL\n"
	"void main() {\n"
	"	vec2 local = gl_FragCoord.xy - (u_px.xy + u_px.zw * 0.5);\n"
	"	float d = sd_rrect(local, u_px.zw * 0.5, u_radius);\n"
	"	vec2 p = (gl_FragCoord.xy - u_px.xy) / u_px.zw;\n"
	"	float x = p.x + (1.0 - p.y) * 0.5 - (u_t * 2.2 - 0.6);\n"
	"	float band = exp(-x * x / 0.012) * 0.32;\n"
	"	frag = vec4(1.0, 1.0, 1.0, band * clamp(0.5 - d, 0.0, 1.0));\n"
	"}\n";

// GLSL dialect of the context NextUI created: ES 3.x on the device; on a desktop it may be a
// legacy GLSL 1.20 context, so shaders are written once and adapted by these headers.
static int glsl_legacy, glsl_es;

// Dual Kawase blur (Bjorge, SIGGRAPH 2015), the version measured on the Brick in spikes/blur.
static const char* fs_down =
	"uniform sampler2D u_tex;\n"
	"uniform vec2 u_half;\n"
	"in vec2 v_uv; FRAG_DECL\n"
	"void main() {\n"
	"	vec2 h = u_half;\n"
	"	vec4 s = texture(u_tex, v_uv) * 4.0;\n"
	"	s += texture(u_tex, v_uv - h);\n"
	"	s += texture(u_tex, v_uv + h);\n"
	"	s += texture(u_tex, v_uv + vec2(h.x, -h.y));\n"
	"	s += texture(u_tex, v_uv - vec2(h.x, -h.y));\n"
	"	frag = s / 8.0;\n"
	"}\n";

static const char* fs_up =
	"uniform sampler2D u_tex;\n"
	"uniform vec2 u_half;\n"
	"in vec2 v_uv; FRAG_DECL\n"
	"void main() {\n"
	"	vec2 h = u_half;\n"
	"	vec4 s = texture(u_tex, v_uv + vec2(-h.x * 2.0, 0.0));\n"
	"	s += texture(u_tex, v_uv + vec2(-h.x, h.y)) * 2.0;\n"
	"	s += texture(u_tex, v_uv + vec2(0.0, h.y * 2.0));\n"
	"	s += texture(u_tex, v_uv + vec2(h.x, h.y)) * 2.0;\n"
	"	s += texture(u_tex, v_uv + vec2(h.x * 2.0, 0.0));\n"
	"	s += texture(u_tex, v_uv + vec2(h.x, -h.y)) * 2.0;\n"
	"	s += texture(u_tex, v_uv + vec2(0.0, -h.y * 2.0));\n"
	"	s += texture(u_tex, v_uv + vec2(-h.x, -h.y)) * 2.0;\n"
	"	frag = s / 12.0;\n"
	"}\n";

static const char* shader_header(GLenum type) {
	if (glsl_es) return "#version 300 es\nprecision mediump float;\n#define FRAG_DECL out vec4 frag;\n";
	if (!glsl_legacy) return "#version 330 core\n#define FRAG_DECL out vec4 frag;\n";
	if (type == GL_VERTEX_SHADER) return "#version 120\n#define in attribute\n#define out varying\n";
	return "#version 120\n#define in varying\n#define texture texture2D\n#define FRAG_DECL\n#define frag gl_FragColor\n";
}

static void detect_glsl(void) {
	const char* version = (const char*)glGetString(GL_SHADING_LANGUAGE_VERSION);
	LOG_info("launcher GL: %s, GLSL: %s\n", glGetString(GL_VERSION), version ? version : "?");
	if (!version) return;
	glsl_es = strstr(version, "ES") != NULL;
	int major = 0, minor = 0;
	const char* digits = version;
	while (*digits && (*digits < '0' || *digits > '9')) digits++;
	sscanf(digits, "%d.%d", &major, &minor);
	glsl_legacy = !glsl_es && major * 100 + minor < 330;
}

static GLuint compile(GLenum type, const char* a, const char* b) {
	const char* srcs[3] = { shader_header(type), a, b ? b : "" };
	GLuint s = glCreateShader(type);
	GLint ok;
	char msg[1024];

	glShaderSource(s, 3, srcs, NULL);
	glCompileShader(s);
	glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
	if (!ok) {
		glGetShaderInfoLog(s, sizeof(msg), NULL, msg);
		LOG_error("launcher shader compile failed: %s\n", msg);
	}
	return s;
}

static GLuint program(const char* fs_a, const char* fs_b) {
	GLuint p = glCreateProgram();
	GLint ok;
	char msg[1024];

	glAttachShader(p, compile(GL_VERTEX_SHADER, vs_src, NULL));
	glAttachShader(p, compile(GL_FRAGMENT_SHADER, fs_a, fs_b));
	glBindAttribLocation(p, 0, "a_pos");
	glLinkProgram(p);
	glGetProgramiv(p, GL_LINK_STATUS, &ok);
	if (!ok) {
		glGetProgramInfoLog(p, sizeof(msg), NULL, msg);
		LOG_error("launcher program link failed: %s\n", msg);
	}
	return p;
}

static LTex* upload(int w, int h, const void* rgba, int pixel_art) {
	LTex* tex = calloc(1, sizeof(LTex));
	tex->w = w;
	tex->h = h;
	glGenTextures(1, &tex->id);
	glBindTexture(GL_TEXTURE_2D, tex->id);
	// NextUI's video code changes the unpack state for its own uploads; ours are tightly packed.
	glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
	glPixelStorei(GL_UNPACK_SKIP_PIXELS, 0);
	glPixelStorei(GL_UNPACK_SKIP_ROWS, 0);
	glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba);
	glGenerateMipmap(GL_TEXTURE_2D);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, pixel_art ? GL_NEAREST : GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	return tex;
}

// Soft shadow made once: alpha falls off with distance from a rounded square.
static LTex* make_shadow(void) {
	Uint32* px = malloc(SHADOW_SIZE * SHADOW_SIZE * 4);
	float half = SHADOW_INNER * 0.5f, radius = 20.0f, soft = (SHADOW_SIZE - SHADOW_INNER) * 0.5f;
	for (int y = 0; y < SHADOW_SIZE; y++) {
		for (int x = 0; x < SHADOW_SIZE; x++) {
			float qx = fabsf(x - SHADOW_SIZE * 0.5f) - half + radius;
			float qy = fabsf(y - SHADOW_SIZE * 0.5f) - half + radius;
			float ox = qx > 0 ? qx : 0, oy = qy > 0 ? qy : 0;
			float d = sqrtf(ox * ox + oy * oy) + fminf(fmaxf(qx, qy), 0.0f) - radius;
			float a = 1.0f - (d + soft * 0.3f) / soft;
			a = a < 0 ? 0 : a > 1 ? 1 : a;
			px[y * SHADOW_SIZE + x] = (Uint32)(a * a * 200) << 24;
		}
	}
	LTex* tex = upload(SHADOW_SIZE, SHADOW_SIZE, px, 0);
	free(px);
	return tex;
}

static Target make_target(int w, int h) {
	Target t = { 0, 0, w, h };
	glGenTextures(1, &t.tex);
	glBindTexture(GL_TEXTURE_2D, t.tex);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glGenFramebuffers(1, &t.fbo);
	glBindFramebuffer(GL_FRAMEBUFFER, t.fbo);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, t.tex, 0);
	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
		LOG_error("launcher framebuffer %dx%d incomplete\n", w, h);
	return t;
}

static void free_target(Target* t) {
	if (t->fbo) glDeleteFramebuffers(1, &t->fbo);
	if (t->tex) glDeleteTextures(1, &t->tex);
	t->fbo = t->tex = 0;
}

// A font the user put in /mnt/SDCARD/Fonts replaces Inter by weight: a file name containing
// "regular", "semibold" (else "medium") or "bold" (not "semibold" or "extrabold"), case ignored,
// italics skipped. Nothing of the kind ships with the launcher; this is how someone uses a font
// they own on their own card. Returns 1 and the path if one matched.
static int user_font(const char* weight, char* out, size_t size) {
	char dir_path[256];
	snprintf(dir_path, sizeof(dir_path), "%s/Fonts", SDCARD_PATH);
	DIR* dh = opendir(dir_path);
	if (!dh) return 0;
	char best[256] = "", medium[256] = "";
	struct dirent* dp;
	while ((dp = readdir(dh))) {
		char lower[256];
		snprintf(lower, sizeof(lower), "%s", dp->d_name);
		for (char* c = lower; *c; c++) *c = (char)tolower((unsigned char)*c);
		if (lower[0] == '.' || strstr(lower, "italic")) continue;
		if (!suffixMatch(".ttf", lower) && !suffixMatch(".otf", lower)) continue;
		int match = 0;
		if (!strcmp(weight, "regular")) match = strstr(lower, "regular") != NULL;
		else if (!strcmp(weight, "semibold")) {
			match = strstr(lower, "semibold") != NULL;
			if (!match && strstr(lower, "medium") && (!medium[0] || strcmp(dp->d_name, medium) < 0))
				snprintf(medium, sizeof(medium), "%s", dp->d_name);
		} else
			match = strstr(lower, "bold") && !strstr(lower, "semibold") && !strstr(lower, "extrabold");
		// Of several matches (Display and Text, say) the first by name wins, so the choice is stable.
		if (match && (!best[0] || strcmp(dp->d_name, best) < 0)) snprintf(best, sizeof(best), "%s", dp->d_name);
	}
	closedir(dh);
	if (!best[0] && medium[0]) snprintf(best, sizeof(best), "%s", medium);
	if (!best[0]) return 0;
	snprintf(out, size, "%s/%s", dir_path, best);
	return 1;
}

static TTF_Font* open_font(const char* file, int size) {
	char path[256];
	const char* weight = strstr(file, "SemiBold") ? "semibold" : strstr(file, "Bold") ? "bold" : "regular";
	if (user_font(weight, path, sizeof(path))) {
		TTF_Font* font = TTF_OpenFont(path, size);
		if (font) {
			LOG_info("launcher font: %s\n", path);
			return font;
		}
		LOG_error("launcher could not open %s: %s\n", path, TTF_GetError());
	}
	snprintf(path, sizeof(path), "%s/%s", RES_PATH, file);
	TTF_Font* font = TTF_OpenFont(path, size);
	if (!font) {
		LOG_error("launcher could not open %s: %s\n", path, TTF_GetError());
		snprintf(path, sizeof(path), "%s/font1.ttf", RES_PATH);
		font = TTF_OpenFont(path, size);
	}
	return font;
}

// Next UTF-8 code point, or -1 at the end of the string.
static int next_codepoint(const char** p) {
	const unsigned char* s = (const unsigned char*)*p;
	if (!*s) return -1;
	int c = *s, n = c < 0x80 ? 0 : c < 0xE0 ? 1 : c < 0xF0 ? 2 : 3;
	c &= n == 0 ? 0x7F : n == 1 ? 0x1F : n == 2 ? 0x0F : 0x07;
	s++;
	for (int i = 0; i < n && (*s & 0xC0) == 0x80; i++, s++) c = (c << 6) | (*s & 0x3F);
	*p = (const char*)s;
	return c;
}

static int font_has(TTF_Font* f, int c) {
	// SDL_ttf 2.0.13 on the device can only ask about characters up to U+FFFF.
	return c <= 0x20 || (c <= 0xFFFF && TTF_GlyphIsProvided(f, (Uint16)c));
}

// Font i of the chain for size `font`: 0 is Inter, then the fallbacks, opened when first needed.
static TTF_Font* chain_font(int font, int i) {
	if (i == 0) return fonts[font];
	i--;
	if (!fallback_tried[font][i]) {
		fallback_tried[font][i] = 1;
		if (exists((char*)fallback_files[i])) fallbacks[font][i] = TTF_OpenFont(fallback_files[i], font_sizes[font]);
	}
	return fallbacks[font][i];
}

static int chain_for(int font, int c) {
	for (int i = 0; i <= FALLBACK_COUNT; i++) {
		TTF_Font* f = chain_font(font, i);
		if (f && font_has(f, c)) return i;
	}
	return 0;
}

typedef struct {
	int font_index;
	int start, end; // byte range in the text
} Run;

// Splits text into runs of characters that share a font. Returns how many.
static int make_runs(int font, const char* text, Run* runs, int max_runs) {
	int count = 0;
	const char* p = text;
	while (*p && count < max_runs) {
		const char* before = p;
		int c = next_codepoint(&p);
		int fi = c == ' ' && count ? runs[count - 1].font_index : chain_for(font, c);
		if (count && runs[count - 1].font_index == fi) runs[count - 1].end = (int)(p - text);
		else runs[count++] = (Run){ fi, (int)(before - text), (int)(p - text) };
	}
	return count;
}

static int text_width(int font, const char* text) {
	Run runs[64];
	int n = make_runs(font, text, runs, 64), total = 0;
	for (int i = 0; i < n; i++) {
		char piece[512];
		snprintf(piece, sizeof(piece), "%.*s", runs[i].end - runs[i].start, text + runs[i].start);
		int w = 0, h = 0;
		TTF_SizeUTF8(chain_font(font, runs[i].font_index), piece, &w, &h);
		total += w;
	}
	return total;
}

// Copies text into out, cut to max_w with an ellipsis when it is wider.
static void fit_text(int font, const char* text, int max_w, char* out, size_t size) {
	snprintf(out, size, "%s", text);
	if (max_w <= 0 || text_width(font, out) <= max_w) return;
	int len = (int)strlen(out);
	while (len > 0) {
		len--;
		while (len > 0 && (out[len] & 0xC0) == 0x80) len--; // keep UTF-8 sequences whole
		while (len > 0 && out[len - 1] == ' ') len--;
		snprintf(out + len, size - len, "...");
		if (text_width(font, out) <= max_w) return;
	}
}

// One line of text in white, each run in its own font, all on one baseline.
static SDL_Surface* render_line(int font, const char* text) {
	Run runs[64];
	int n = make_runs(font, text, runs, 64);
	SDL_Surface* parts[64] = { 0 };
	int ascent = 0, descent = 0, width = 0;
	for (int i = 0; i < n; i++) {
		TTF_Font* f = chain_font(font, runs[i].font_index);
		if (!f) continue;
		char piece[512];
		snprintf(piece, sizeof(piece), "%.*s", runs[i].end - runs[i].start, text + runs[i].start);
		parts[i] = TTF_RenderUTF8_Blended(f, piece, (SDL_Color){ 255, 255, 255, 255 });
		if (!parts[i]) continue;
		if (TTF_FontAscent(f) > ascent) ascent = TTF_FontAscent(f);
		if (-TTF_FontDescent(f) > descent) descent = -TTF_FontDescent(f);
		width += parts[i]->w;
	}
	if (!width) return NULL;
	SDL_Surface* out = SDL_CreateRGBSurfaceWithFormat(0, width, ascent + descent, 32, SDL_PIXELFORMAT_ARGB8888);
	SDL_FillRect(out, NULL, 0);
	int x = 0;
	for (int i = 0; i < n; i++) {
		if (!parts[i]) continue;
		TTF_Font* f = chain_font(font, runs[i].font_index);
		SDL_SetSurfaceBlendMode(parts[i], SDL_BLENDMODE_NONE);
		SDL_Rect dst = { x, ascent - TTF_FontAscent(f), parts[i]->w, parts[i]->h };
		SDL_BlitSurface(parts[i], NULL, out, &dst);
		x += parts[i]->w;
		SDL_FreeSurface(parts[i]);
	}
	return out;
}

void lgfx_init(int w, int h) {
	static const float quad[8] = { 0, 0, 1, 0, 0, 1, 1, 1 };

	screen_w = w;
	screen_h = h;
	SDL_GL_GetDrawableSize(SDL_GL_GetCurrentWindow(), &fb_w, &fb_h);
	if (fb_w <= 0 || fb_h <= 0) {
		fb_w = w;
		fb_h = h;
	}

	detect_glsl();
	p_tex = program(fs_sdf, fs_tex);
	p_fill = program(fs_sdf, fs_fill);
	p_shine = program(fs_sdf, fs_shine);
	p_down = program(fs_down, NULL);
	p_up = program(fs_up, NULL);

	glGenVertexArrays(1, &vao);
	glBindVertexArray(vao);
	glGenBuffers(1, &vbo);
	glBindBuffer(GL_ARRAY_BUFFER, vbo);
	glBufferData(GL_ARRAY_BUFFER, sizeof(quad), quad, GL_STATIC_DRAW);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, 0);

	shadow = make_shadow();

	// Snapshot blur targets at framebuffer size, and one blur now so the driver prepares them
	// before the first panel opens (the first use took 32 ms on the Brick in spikes/home).
	snap = make_target(fb_w, fb_h);
	down[0] = snap;
	down[1] = make_target(fb_w / 2, fb_h / 2);
	down[2] = make_target(fb_w / 4, fb_h / 4);
	down[3] = make_target(fb_w / 8, fb_h / 8);
	up[0] = make_target(fb_w, fb_h);
	up[1] = make_target(fb_w / 2, fb_h / 2);
	up[2] = make_target(fb_w / 4, fb_h / 4);
	lgfx_begin_snapshot();
	lgfx_end_snapshot();
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glFinish();

	if (!TTF_WasInit()) TTF_Init();
	static const struct { const char* file; int size; } spec[LFONT_COUNT] = {
		[LFONT_SMALL] = { "Inter-Regular.ttf", 18 },  [LFONT_LABEL] = { "Inter-SemiBold.ttf", 19 },
		[LFONT_BODY] = { "Inter-Regular.ttf", 23 },   [LFONT_TITLE] = { "Inter-Bold.ttf", 30 },
		[LFONT_HERO] = { "Inter-Bold.ttf", 46 },
	};
	for (int i = 0; i < LFONT_COUNT; i++) {
		font_sizes[i] = spec[i].size;
		fonts[i] = open_font(spec[i].file, spec[i].size);
	}
}

void lgfx_quit(void) {
	for (int i = 0; i < LFONT_COUNT; i++) {
		if (fonts[i]) TTF_CloseFont(fonts[i]);
		fonts[i] = NULL;
		for (int j = 0; j < FALLBACK_COUNT; j++) {
			if (fallbacks[i][j]) TTF_CloseFont(fallbacks[i][j]);
			fallbacks[i][j] = NULL;
		}
	}
	ltex_free(shadow);
	free_target(&snap);
	for (int i = 1; i < 4; i++) free_target(&down[i]);
	for (int i = 0; i < 3; i++) free_target(&up[i]);
	glDeleteProgram(p_down);
	glDeleteProgram(p_up);
	glDeleteBuffers(1, &vbo);
	glDeleteVertexArrays(1, &vao);
	glDeleteProgram(p_tex);
	glDeleteProgram(p_fill);
	glDeleteProgram(p_shine);
}

void lgfx_begin_frame(void) {
	// NextUI's own drawing (sleep, power off) can change GL state, so set ours every frame.
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glViewport(0, 0, fb_w, fb_h);
	glBindVertexArray(vao);
	glBindBuffer(GL_ARRAY_BUFFER, vbo);
	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, 0);
	glEnableVertexAttribArray(0);
	glActiveTexture(GL_TEXTURE0);
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_SCISSOR_TEST);
	glClearColor(0, 0, 0, 1);
	glClear(GL_COLOR_BUFFER_BIT);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

void lgfx_end_frame(void) {
	SDL_GL_SwapWindow(SDL_GL_GetCurrentWindow());
}

LTex* ltex_from_surface(SDL_Surface* surface, int pixel_art) {
	if (!surface) return NULL;
	SDL_Surface* rgba = SDL_ConvertSurfaceFormat(surface, SDL_PIXELFORMAT_ABGR8888, 0);
	if (!rgba) return NULL;
	Uint8* packed = malloc(rgba->w * rgba->h * 4);
	for (int y = 0; y < rgba->h; y++)
		memcpy(packed + y * rgba->w * 4, (Uint8*)rgba->pixels + y * rgba->pitch, rgba->w * 4);
	LTex* tex = upload(rgba->w, rgba->h, packed, pixel_art);
	free(packed);
	SDL_FreeSurface(rgba);
	return tex;
}

LTex* ltex_load(const char* path, int pixel_art) {
	SDL_Surface* surface = IMG_Load(path);
	if (!surface) return NULL;
	LTex* tex = ltex_from_surface(surface, pixel_art);
	SDL_FreeSurface(surface);
	return tex;
}

// Renders text in white. Text wider than max_w is cut and ends in an ellipsis.
LTex* ltex_text(int font, const char* text, int max_w) {
	if (!fonts[font] || !text || !text[0]) return NULL;
	char line[512];
	fit_text(font, text, max_w, line, sizeof(line));
	SDL_Surface* surface = render_line(font, line);
	LTex* tex = ltex_from_surface(surface, 0);
	SDL_FreeSurface(surface);
	return tex;
}

static int is_cjk(int c) {
	return c >= 0x2E80 && c <= 0xFFEF;
}

LTex* ltex_text_wrapped(int font, const char* text, int max_w, int max_lines, int centered) {
	if (!fonts[font] || !text || !text[0] || max_lines < 1) return NULL;
	if (max_lines > 8) max_lines = 8;

	// Greedy line breaking. A line may break after a space, or before or after a Chinese,
	// Japanese or Korean character, since those are written without spaces.
	char lines[8][512];
	int count = 0;
	const char* rest = text;
	while (*rest && count < max_lines) {
		while (*rest == ' ') rest++;
		if (!*rest) break;
		if (count == max_lines - 1) {
			fit_text(font, rest, max_w, lines[count++], sizeof(lines[0]));
			break;
		}
		const char* p = rest;
		const char* last_break = NULL;
		int prev = 0;
		while (*p) {
			const char* here = p;
			int c = next_codepoint(&p);
			if (here > rest && (c == ' ' || is_cjk(c) || is_cjk(prev))) last_break = here;
			char trial[512];
			snprintf(trial, sizeof(trial), "%.*s", (int)(p - rest), rest);
			if (text_width(font, trial) > max_w && here > rest) {
				p = last_break ? last_break : here;
				break;
			}
			prev = c;
		}
		snprintf(lines[count], sizeof(lines[0]), "%.*s", (int)(p - rest), rest);
		int len = (int)strlen(lines[count]);
		while (len > 0 && lines[count][len - 1] == ' ') lines[count][--len] = '\0';
		count++;
		rest = p;
	}

	SDL_Surface* rendered[8] = { 0 };
	int total_h = 0, total_w = 0;
	for (int i = 0; i < count; i++) {
		rendered[i] = render_line(font, lines[i]);
		if (!rendered[i]) continue;
		total_h += rendered[i]->h;
		if (rendered[i]->w > total_w) total_w = rendered[i]->w;
	}
	if (!total_w || !total_h) return NULL;

	SDL_Surface* out = SDL_CreateRGBSurfaceWithFormat(0, total_w, total_h, 32, SDL_PIXELFORMAT_ARGB8888);
	SDL_FillRect(out, NULL, 0);
	int y = 0;
	for (int i = 0; i < count; i++) {
		if (!rendered[i]) continue;
		SDL_SetSurfaceBlendMode(rendered[i], SDL_BLENDMODE_NONE);
		SDL_Rect dst = { centered ? (total_w - rendered[i]->w) / 2 : 0, y, rendered[i]->w, rendered[i]->h };
		SDL_BlitSurface(rendered[i], NULL, out, &dst);
		y += rendered[i]->h;
		SDL_FreeSurface(rendered[i]);
	}
	LTex* tex = ltex_from_surface(out, 0);
	SDL_FreeSurface(out);
	return tex;
}

void ltex_free(LTex* tex) {
	if (!tex) return;
	glDeleteTextures(1, &tex->id);
	free(tex);
}

// Converts a top-left screen rect to GL's bottom-left framebuffer pixels.
static void to_fb(float x, float y, float w, float h, float* out) {
	float sx = (float)fb_w / screen_w, sy = (float)fb_h / screen_h;
	out[0] = x * sx;
	out[1] = (screen_h - y - h) * sy;
	out[2] = w * sx;
	out[3] = h * sy;
}

static void set_rect(GLuint prog, float x, float y, float w, float h) {
	glUniform4f(glGetUniformLocation(prog, "u_rect"), x / screen_w, (screen_h - y - h) / screen_h, w / screen_w,
	            h / screen_h);
}

static void draw_tex_uv(LTex* tex, float x, float y, float w, float h, float radius, float alpha, float u0, float v0,
                        float us, float vs) {
	float px[4];
	to_fb(x, y, w, h, px);
	glUseProgram(p_tex);
	glBindTexture(GL_TEXTURE_2D, tex->id);
	glUniform1i(glGetUniformLocation(p_tex, "u_tex"), 0);
	glUniform4f(glGetUniformLocation(p_tex, "u_px"), px[0], px[1], px[2], px[3]);
	glUniform1f(glGetUniformLocation(p_tex, "u_radius"), radius * fb_w / screen_w);
	// Textures are stored top row first, so v runs from 1 at the bottom of the quad to 0 at the top.
	glUniform4f(glGetUniformLocation(p_tex, "u_uv"), u0, v0 + vs, us, -vs);
	glUniform1f(glGetUniformLocation(p_tex, "u_alpha"), alpha);
	set_rect(p_tex, x, y, w, h);
	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
}

void lgfx_draw_tex(LTex* tex, float x, float y, float w, float h, float radius, float alpha) {
	if (!tex) return;
	float tex_aspect = (float)tex->w / tex->h, rect_aspect = w / h;
	float u0 = 0, v0 = 0, us = 1, vs = 1;
	if (tex_aspect > rect_aspect) {
		us = rect_aspect / tex_aspect;
		u0 = (1 - us) * 0.5f;
	} else if (tex_aspect < rect_aspect) {
		vs = tex_aspect / rect_aspect;
		v0 = (1 - vs) * 0.5f;
	}
	draw_tex_uv(tex, x, y, w, h, radius, alpha, u0, v0, us, vs);
}

void lgfx_draw_gradient(float x, float y, float w, float h, float radius, const float top[4], const float bottom[4]) {
	float px[4];
	to_fb(x, y, w, h, px);
	glUseProgram(p_fill);
	glUniform4f(glGetUniformLocation(p_fill, "u_px"), px[0], px[1], px[2], px[3]);
	glUniform1f(glGetUniformLocation(p_fill, "u_radius"), radius * fb_w / screen_w);
	glUniform4f(glGetUniformLocation(p_fill, "u_top"), top[0], top[1], top[2], top[3]);
	glUniform4f(glGetUniformLocation(p_fill, "u_bottom"), bottom[0], bottom[1], bottom[2], bottom[3]);
	glUniform1f(glGetUniformLocation(p_fill, "u_stroke"), 0);
	set_rect(p_fill, x - 2, y - 2, w + 4, h + 4);
	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
}

void lgfx_draw_fill(float x, float y, float w, float h, float radius, float r, float g, float b, float a) {
	const float c[4] = { r, g, b, a };
	lgfx_draw_gradient(x, y, w, h, radius, c, c);
}

void lgfx_draw_ring(float x, float y, float w, float h, float radius, float width, float r, float g, float b, float a) {
	float px[4];
	to_fb(x, y, w, h, px);
	glUseProgram(p_fill);
	glUniform4f(glGetUniformLocation(p_fill, "u_px"), px[0], px[1], px[2], px[3]);
	glUniform1f(glGetUniformLocation(p_fill, "u_radius"), radius * fb_w / screen_w);
	glUniform4f(glGetUniformLocation(p_fill, "u_top"), r, g, b, a);
	glUniform4f(glGetUniformLocation(p_fill, "u_bottom"), r, g, b, a);
	glUniform1f(glGetUniformLocation(p_fill, "u_stroke"), width * fb_w / screen_w);
	set_rect(p_fill, x - 2, y - 2, w + 4, h + 4);
	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
}

void lgfx_draw_shine(float x, float y, float w, float h, float radius, float t) {
	float px[4];
	to_fb(x, y, w, h, px);
	glUseProgram(p_shine);
	glUniform4f(glGetUniformLocation(p_shine, "u_px"), px[0], px[1], px[2], px[3]);
	glUniform1f(glGetUniformLocation(p_shine, "u_radius"), radius * fb_w / screen_w);
	glUniform1f(glGetUniformLocation(p_shine, "u_t"), t);
	set_rect(p_shine, x, y, w, h);
	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
}

void lgfx_draw_tex_fit(LTex* tex, float x, float y, float w, float h, float radius, float alpha) {
	if (!tex) return;
	float scale = fminf(w / tex->w, h / tex->h);
	float dw = tex->w * scale, dh = tex->h * scale;
	draw_tex_uv(tex, x + (w - dw) * 0.5f, y + (h - dh) * 0.5f, dw, dh, radius, alpha, 0, 0, 1, 1);
}

// Shadow under a card rect; lift moves it down and makes it larger and softer.
void lgfx_draw_shadow(float x, float y, float w, float h, float lift, float alpha) {
	float pad_x = w * (SHADOW_SIZE - SHADOW_INNER) / (2.0f * SHADOW_INNER);
	float pad_y = h * (SHADOW_SIZE - SHADOW_INNER) / (2.0f * SHADOW_INNER);
	float grow = 1.0f + lift * 0.01f;
	float sw = (w + 2 * pad_x) * grow, sh = (h + 2 * pad_y) * grow;
	draw_tex_uv(shadow, x + w * 0.5f - sw * 0.5f, y + h * 0.5f - sh * 0.5f + 8 + lift, sw, sh, 0, alpha, 0, 0, 1, 1);
}

// Wallpaper slightly larger than the screen, so parallax can shift it without showing edges.
void lgfx_draw_wallpaper(LTex* tex, float offset_x, float offset_y) {
	if (!tex) return;
	glDisable(GL_BLEND);
	draw_tex_uv(tex, 0, 0, screen_w, screen_h, 0, 1, 0.02f + offset_x / screen_w, 0.02f + offset_y / screen_h, 0.96f,
	            0.96f);
	glEnable(GL_BLEND);
}

void lgfx_begin_snapshot(void) {
	glBindFramebuffer(GL_FRAMEBUFFER, snap.fbo);
	glViewport(0, 0, fb_w, fb_h);
	glBindVertexArray(vao);
	glBindBuffer(GL_ARRAY_BUFFER, vbo);
	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, 0);
	glEnableVertexAttribArray(0);
	glActiveTexture(GL_TEXTURE0);
	glClearColor(0, 0, 0, 1);
	glClear(GL_COLOR_BUFFER_BIT);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

static void blur_pass(GLuint prog, const Target* src, const Target* dst) {
	glBindFramebuffer(GL_FRAMEBUFFER, dst->fbo);
	glViewport(0, 0, dst->w, dst->h);
	glBindTexture(GL_TEXTURE_2D, src->tex);
	glUniform1i(glGetUniformLocation(prog, "u_tex"), 0);
	glUniform2f(glGetUniformLocation(prog, "u_half"), 0.5f / src->w, 0.5f / src->h);
	glUniform4f(glGetUniformLocation(prog, "u_rect"), 0, 0, 1, 1);
	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
}

void lgfx_end_snapshot(void) {
	glDisable(GL_BLEND);
	glUseProgram(p_down);
	for (int i = 1; i <= 3; i++) blur_pass(p_down, &down[i - 1], &down[i]);
	glUseProgram(p_up);
	blur_pass(p_up, &down[3], &up[2]);
	blur_pass(p_up, &up[2], &up[1]);
	blur_pass(p_up, &up[1], &up[0]);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glViewport(0, 0, fb_w, fb_h);
	glEnable(GL_BLEND);
	snapshot_ready = 1;
}

void lgfx_draw_snapshot(float alpha) {
	if (!snapshot_ready) return;
	// Framebuffer textures are stored bottom row first, so no flip here.
	LTex tex = { up[0].tex, up[0].w, up[0].h };
	glUseProgram(p_tex);
	glBindTexture(GL_TEXTURE_2D, tex.id);
	glUniform1i(glGetUniformLocation(p_tex, "u_tex"), 0);
	glUniform4f(glGetUniformLocation(p_tex, "u_px"), 0, 0, fb_w, fb_h);
	glUniform1f(glGetUniformLocation(p_tex, "u_radius"), 0);
	glUniform4f(glGetUniformLocation(p_tex, "u_uv"), 0, 0, 1, 1);
	glUniform1f(glGetUniformLocation(p_tex, "u_alpha"), alpha);
	set_rect(p_tex, 0, 0, screen_w, screen_h);
	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
}
