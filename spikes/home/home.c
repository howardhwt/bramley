/* Home Spike: can a tvOS-style home screen with motion, depth and translucency hold 60 fps?
 *
 * Three rows of six cards with rounded box art, a shadow image made once at load under each
 * card, and titles. Focus moves every 0.5 s; the focused card springs to 1.15x with a bigger
 * shadow, rows scroll with springs, and the wallpaper shifts for parallax.
 *
 * Every 6 s a Now Playing panel opens. On that frame the home screen is drawn into a texture
 * and blurred once (snapshot blur). The panel then slides in over the frozen blur for 2 s.
 *
 * Normal run: 60 s; snapshot blur at 1024x768 for the first 30 s, 256x192 for the next 30 s.
 * --stress: no panel; the home screen is drawn 1, 2, 3 and 4 times per frame for 10 s each.
 * Every 5 s fps, worst frame and slow frames go to the log, and every snapshot frame is logged.
 * B, MENU, Esc or Q quits.
 */
#include <SDL.h>
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef HOME_DEVICE
#include <GLES3/gl3.h>
#define SHADER_HEADER "#version 300 es\nprecision mediump float;\n"
#define DEFAULT_LOG "/mnt/SDCARD/spike/home-spike.log"
#else
#define GL_SILENCE_DEPRECATION
#include <OpenGL/gl3.h>
#define SHADER_HEADER "#version 330 core\n"
#define DEFAULT_LOG "home-spike.log"
#endif

#define W 1024
#define H 768
#define JOY_B 0
#define JOY_MENU 8
#define ROWS 3
#define COLS 6
#define CARD 180.0f
#define GAP 36.0f
#define SHADOW_PAD 40

static FILE *log_fp;

static void logline(const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	vfprintf(stdout, fmt, ap);
	va_end(ap);
	fputc('\n', stdout);
	if (!log_fp) return;
	va_start(ap, fmt);
	vfprintf(log_fp, fmt, ap);
	va_end(ap);
	fputc('\n', log_fp);
	fflush(log_fp);
}

/* ---------- shaders ---------- */

static const char *vs_src =
	"in vec2 a_pos;\n"
	"uniform vec4 u_rect;\n"
	"out vec2 v_uv;\n"
	"void main() {\n"
	"	v_uv = a_pos;\n"
	"	vec2 p = u_rect.xy + a_pos * u_rect.zw;\n"
	"	gl_Position = vec4(p * 2.0 - 1.0, 0.0, 1.0);\n"
	"}\n";

static const char *fs_sdf =
	"float sd_rrect(vec2 p, vec2 b, float r) {\n"
	"	vec2 q = abs(p) - b + r;\n"
	"	return length(max(q, 0.0)) + min(max(q.x, q.y), 0.0) - r;\n"
	"}\n";

/* Textured quad with an optional rounded-corner mask, a UV window and an alpha multiplier. */
static const char *fs_rtex =
	"uniform sampler2D u_tex;\n"
	"uniform vec4 u_px;\n"
	"uniform float u_radius;\n"
	"uniform vec4 u_uv;\n"
	"uniform float u_alpha;\n"
	"in vec2 v_uv; out vec4 frag;\n"
	"void main() {\n"
	"	vec4 c = texture(u_tex, u_uv.xy + v_uv * u_uv.zw);\n"
	"	float a = 1.0;\n"
	"	if (u_radius > 0.0) {\n"
	"		vec2 local = gl_FragCoord.xy - (u_px.xy + u_px.zw * 0.5);\n"
	"		a = clamp(0.5 - sd_rrect(local, u_px.zw * 0.5, u_radius), 0.0, 1.0);\n"
	"	}\n"
	"	frag = vec4(c.rgb, c.a * a * u_alpha);\n"
	"}\n";

static const char *fs_fill =
	"uniform vec4 u_px;\n"
	"uniform float u_radius;\n"
	"uniform vec4 u_color;\n"
	"in vec2 v_uv; out vec4 frag;\n"
	"void main() {\n"
	"	vec2 local = gl_FragCoord.xy - (u_px.xy + u_px.zw * 0.5);\n"
	"	float d = sd_rrect(local, u_px.zw * 0.5, u_radius);\n"
	"	vec3 col = mix(u_color.rgb, vec3(1.0), clamp(1.0 - abs(d + 1.0), 0.0, 1.0) * 0.4);\n"
	"	frag = vec4(col, u_color.a * clamp(0.5 - d, 0.0, 1.0));\n"
	"}\n";

static const char *fs_down =
	"uniform sampler2D u_tex;\n"
	"uniform vec2 u_half;\n"
	"in vec2 v_uv; out vec4 frag;\n"
	"void main() {\n"
	"	vec2 h = u_half;\n"
	"	vec4 s = texture(u_tex, v_uv) * 4.0;\n"
	"	s += texture(u_tex, v_uv - h);\n"
	"	s += texture(u_tex, v_uv + h);\n"
	"	s += texture(u_tex, v_uv + vec2(h.x, -h.y));\n"
	"	s += texture(u_tex, v_uv - vec2(h.x, -h.y));\n"
	"	frag = s / 8.0;\n"
	"}\n";

static const char *fs_up =
	"uniform sampler2D u_tex;\n"
	"uniform vec2 u_half;\n"
	"in vec2 v_uv; out vec4 frag;\n"
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

static GLuint compile(GLenum type, const char *a, const char *b)
{
	const char *srcs[3] = { SHADER_HEADER, a, b ? b : "" };
	GLuint s = glCreateShader(type);
	GLint ok;
	char msg[1024];

	glShaderSource(s, 3, srcs, NULL);
	glCompileShader(s);
	glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
	if (!ok) {
		glGetShaderInfoLog(s, sizeof msg, NULL, msg);
		logline("shader compile failed: %s", msg);
		exit(1);
	}
	return s;
}

static GLuint program(const char *fs_a, const char *fs_b)
{
	GLuint p = glCreateProgram();
	GLint ok;
	char msg[1024];

	glAttachShader(p, compile(GL_VERTEX_SHADER, vs_src, NULL));
	glAttachShader(p, compile(GL_FRAGMENT_SHADER, fs_a, fs_b));
	glBindAttribLocation(p, 0, "a_pos");
	glLinkProgram(p);
	glGetProgramiv(p, GL_LINK_STATUS, &ok);
	if (!ok) {
		glGetProgramInfoLog(p, sizeof msg, NULL, msg);
		logline("program link failed: %s", msg);
		exit(1);
	}
	return p;
}

/* ---------- textures and targets ---------- */

typedef struct { GLuint tex, fbo; int w, h; } Target;

static GLuint make_tex(int w, int h, const void *pixels)
{
	GLuint t;
	glGenTextures(1, &t);
	glBindTexture(GL_TEXTURE_2D, t);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	return t;
}

static Target make_target(int w, int h)
{
	Target t = { make_tex(w, h, NULL), 0, w, h };
	glGenFramebuffers(1, &t.fbo);
	glBindFramebuffer(GL_FRAMEBUFFER, t.fbo);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, t.tex, 0);
	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
		logline("framebuffer %dx%d incomplete", w, h);
		exit(1);
	}
	return t;
}

static void bind_target(const Target *t)
{
	glBindFramebuffer(GL_FRAMEBUFFER, t->fbo);
	glViewport(0, 0, t->w, t->h);
}

static GLuint p_rtex, p_fill, p_down, p_up;

/* All positions are in 1024x768 pixels with a bottom-left origin. */
static void draw_tex(GLuint tex, float x, float y, float w, float h, float radius, float alpha,
                     float u0, float v0, float us, float vs)
{
	glUseProgram(p_rtex);
	glBindTexture(GL_TEXTURE_2D, tex);
	glUniform4f(glGetUniformLocation(p_rtex, "u_px"), x, y, w, h);
	glUniform1f(glGetUniformLocation(p_rtex, "u_radius"), radius);
	glUniform4f(glGetUniformLocation(p_rtex, "u_uv"), u0, v0, us, vs);
	glUniform1f(glGetUniformLocation(p_rtex, "u_alpha"), alpha);
	glUniform4f(glGetUniformLocation(p_rtex, "u_rect"), x / W, y / H, w / W, h / H);
	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
}

static void draw_fill(float x, float y, float w, float h, float radius, float r, float g, float b, float a)
{
	glUseProgram(p_fill);
	glUniform4f(glGetUniformLocation(p_fill, "u_px"), x, y, w, h);
	glUniform1f(glGetUniformLocation(p_fill, "u_radius"), radius);
	glUniform4f(glGetUniformLocation(p_fill, "u_color"), r, g, b, a);
	glUniform4f(glGetUniformLocation(p_fill, "u_rect"), (x - 2) / W, (y - 2) / H, (w + 4) / W, (h + 4) / H);
	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
}

/* Rows are written bottom-up so texture row 0 is the bottom of the screen, as GL expects. */
static GLuint make_wallpaper(const char *bmp_path)
{
	Uint32 *px = malloc(W * H * 4);
	SDL_Surface *bmp = bmp_path ? SDL_LoadBMP(bmp_path) : NULL;

	if (bmp) {
		SDL_Surface *s = SDL_ConvertSurfaceFormat(bmp, SDL_PIXELFORMAT_ABGR8888, 0);
		for (int y = 0; y < H; y++)
			for (int x = 0; x < W; x++) {
				int sx = x * s->w / W, sy = y * s->h / H;
				px[(H - 1 - y) * W + x] = ((Uint32 *)((Uint8 *)s->pixels + sy * s->pitch))[sx];
			}
		logline("wallpaper: %s (%dx%d)", bmp_path, bmp->w, bmp->h);
		SDL_FreeSurface(s);
		SDL_FreeSurface(bmp);
	} else {
		/* Four soft colour fields plus fine diagonal stripes, so the blur is easy to see. */
		static const float blobs[4][5] = {
			{ 0.15f, 0.20f, 0.95f, 0.35f, 0.40f },
			{ 0.85f, 0.25f, 0.95f, 0.55f, 0.20f },
			{ 0.30f, 0.85f, 0.20f, 0.75f, 0.85f },
			{ 0.80f, 0.80f, 0.55f, 0.25f, 0.90f },
		};
		for (int y = 0; y < H; y++)
			for (int x = 0; x < W; x++) {
				float u = (float)x / W, v = (float)y / H, r = 0.08f, g = 0.08f, b = 0.12f;
				for (int i = 0; i < 4; i++) {
					float dx = u - blobs[i][0], dy = (v - blobs[i][1]) * 0.75f;
					float k = expf(-(dx * dx + dy * dy) * 9.0f);
					r += blobs[i][2] * k; g += blobs[i][3] * k; b += blobs[i][4] * k;
				}
				if (((x + y) / 6) % 4 == 0) { r *= 0.75f; g *= 0.75f; b *= 0.75f; }
				int R = r > 1 ? 255 : (int)(r * 255), G = g > 1 ? 255 : (int)(g * 255), B = b > 1 ? 255 : (int)(b * 255);
				px[(H - 1 - y) * W + x] = 0xff000000u | (B << 16) | (G << 8) | R;
			}
		logline("wallpaper: generated");
	}
	GLuint t = make_tex(W, H, px);
	free(px);
	return t;
}

/* Box art stand-in: a two-colour gradient with a ring, one per column. */
static GLuint make_card_art(int i)
{
	enum { S = 256 };
	Uint32 *px = malloc(S * S * 4);
	float hue = i / (float)COLS * 6.2832f;
	float r0 = 0.5f + 0.5f * cosf(hue), g0 = 0.5f + 0.5f * cosf(hue + 2.09f), b0 = 0.5f + 0.5f * cosf(hue + 4.19f);
	for (int y = 0; y < S; y++)
		for (int x = 0; x < S; x++) {
			float v = (float)y / S, dx = x - S * 0.5f, dy = y - S * 0.55f, d = sqrtf(dx * dx + dy * dy);
			float ring = d > 50 && d < 70 ? 0.35f : 0.0f;
			float r = r0 * (0.45f + 0.55f * v) + ring, g = g0 * (0.45f + 0.55f * v) + ring, b = b0 * (0.45f + 0.55f * v) + ring;
			int R = r > 1 ? 255 : (int)(r * 255), G = g > 1 ? 255 : (int)(g * 255), B = b > 1 ? 255 : (int)(b * 255);
			px[y * S + x] = 0xff000000u | (B << 16) | (G << 8) | R;
		}
	GLuint t = make_tex(S, S, px);
	free(px);
	return t;
}

/* Soft shadow made once: alpha falls off with distance from a rounded square. */
static GLuint make_shadow(void)
{
	enum { S = 256 + 2 * SHADOW_PAD * 256 / 180 };
	Uint32 *px = malloc(S * S * 4);
	float half = 128.0f, radius = 24.0f, soft = SHADOW_PAD * 256.0f / 180.0f;
	for (int y = 0; y < S; y++)
		for (int x = 0; x < S; x++) {
			float qx = fabsf(x - S * 0.5f) - half + radius, qy = fabsf(y - S * 0.5f) - half + radius;
			float ox = qx > 0 ? qx : 0, oy = qy > 0 ? qy : 0;
			float d = sqrtf(ox * ox + oy * oy) + fminf(fmaxf(qx, qy), 0.0f) - radius;
			float a = 1.0f - (d + soft * 0.3f) / soft;
			a = a < 0 ? 0 : a > 1 ? 1 : a;
			px[y * S + x] = ((Uint32)(a * a * 200) << 24);
		}
	GLuint t = make_tex(S, S, px);
	free(px);
	return t;
}

/* 5x7 font shared with the blur spike. */
static const char *glyph(char c)
{
	switch (c) {
	case 'A': return ".###.#...##...#######...##...##...#";
	case 'B': return "####.#...##...#####.#...##...#####.";
	case 'C': return ".###.#...##....#....#....#...#.###.";
	case 'D': return "####.#...##...##...##...##...#####.";
	case 'E': return "######....#....####.#....#....#####";
	case 'F': return "######....#....####.#....#....#....";
	case 'G': return ".###.#...##....#.####...##...#.####";
	case 'H': return "#...##...##...#######...##...##...#";
	case 'I': return ".###...#....#....#....#....#...###.";
	case 'K': return "#...##..#.#.#..##...#.#..#..#.#...#";
	case 'L': return "#....#....#....#....#....#....#####";
	case 'M': return "#...###.###.#.##.#.##...##...##...#";
	case 'N': return "#...##...###..##.#.##..###...##...#";
	case 'O': return ".###.#...##...##...##...##...#.###.";
	case 'P': return "####.#...##...#####.#....#....#....";
	case 'Q': return ".###.#...##...##...##.#.##..#..##.#";
	case 'R': return "####.#...##...#####.#.#..#..#.#...#";
	case 'S': return ".#####....#.....###.....#....#####.";
	case 'T': return "#####..#....#....#....#....#....#..";
	case 'U': return "#...##...##...##...##...##...#.###.";
	case 'V': return "#...##...##...##...##...#.#.#...#..";
	case 'W': return "#...##...##...##.#.##.#.##.#.#.#.#.";
	case 'X': return "#...##...#.#.#...#...#.#.#...##...#";
	case 'Y': return "#...##...#.#.#...#....#....#....#..";
	case '0': return ".###.#...##..###.#.###..##...#.###.";
	case '1': return "..#...##....#....#....#....#...###.";
	case '2': return ".###.#...#....#...#...#...#...#####";
	case '3': return "#####...#...#.....#.....##...#.###.";
	case '4': return "...#...##..#.#.#..#.#####...#....#.";
	case '5': return "######....####.....#....##...#.###.";
	case '6': return "..##..#...#....####.#...##...#.###.";
	case '7': return "#####....#...#...#...#....#....#...";
	case '8': return ".###.#...##...#.###.#...##...#.###.";
	case '9': return ".###.#...##...#.####....#...#..##..";
	case '.': return "..........................##...##..";
	case ':': return "......##...##.........##...##......";
	case '/': return ".........#...#...#...#...#.........";
	case '-': return "...............#####...............";
	case '<': return "...#...#...#...#.....#.....#.....#.";
	case '>': return ".#.....#.....#.....#...#...#...#...";
	default: return NULL;
	}
}

/* Renders a string once into a white-on-transparent texture. */
static GLuint make_text(const char *s, int scale, int *out_w, int *out_h)
{
	int n = (int)strlen(s), w = n * 6 * scale + 2, h = 7 * scale + 2;
	Uint32 *px = calloc(w * h, 4);
	for (int i = 0; i < n; i++) {
		const char *g = glyph(s[i] >= 'a' && s[i] <= 'z' ? s[i] - 32 : s[i]);
		if (!g) continue;
		for (int gy = 0; gy < 7; gy++)
			for (int gx = 0; gx < 5; gx++) {
				if (g[gy * 5 + gx] != '#') continue;
				for (int sy = 0; sy < scale; sy++)
					for (int sx = 0; sx < scale; sx++) {
						int x = 1 + i * 6 * scale + gx * scale + sx, y = 1 + gy * scale + sy;
						px[(h - 1 - y) * w + x] = 0xffffffffu;
					}
			}
	}
	GLuint t = make_tex(w, h, px);
	free(px);
	*out_w = w;
	*out_h = h;
	return t;
}

typedef struct { float x, v; } Spring;

static void spring_step(Spring *s, float target, float dt)
{
	const float k = 260.0f, c = 2.0f * sqrtf(k) * 0.72f;
	for (int i = 0; i < 4; i++) {
		float a = k * (target - s->x) - c * s->v;
		s->v += a * dt / 4;
		s->x += s->v * dt / 4;
	}
}

typedef struct { GLuint tex; int w, h; } Text;

static GLuint wallpaper, card_art[COLS], shadow;
static Text row_title[ROWS], card_title[ROWS][COLS], np_title, np_artist;
static Spring scale_spring[ROWS][COLS], row_scroll[ROWS], parallax;

/* Draws the full home screen into whatever framebuffer is bound. */
static void draw_home(int focus_row, int focus_col)
{
	glDisable(GL_BLEND);
	float px = parallax.x * 12.0f;
	draw_tex(wallpaper, 0, 0, W, H, 0, 1, 0.02f + px / W, 0.02f, 0.96f, 0.96f);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	for (int r = 0; r < ROWS; r++) {
		float row_y = H - 250 - r * 235.0f;
		draw_tex(row_title[r].tex, 60 - px * 0.5f, row_y + CARD + 18, row_title[r].w, row_title[r].h, 0, 0.9f, 0, 0, 1, 1);
		for (int c = 0; c < COLS; c++) {
			float s = scale_spring[r][c].x, size = CARD * s;
			float cx = 60 + CARD * 0.5f + c * (CARD + GAP) - row_scroll[r].x - px;
			float cy = row_y + CARD * 0.5f;
			float lift = (s - 1.0f) * 60.0f;
			float sh = size + 2 * SHADOW_PAD * s;
			draw_tex(shadow, cx - sh * 0.5f, cy - sh * 0.5f - 8 - lift * 1.5f, sh, sh, 0, 0.6f + (s - 1.0f) * 2.0f,
			         0, 0, 1, 1);
			draw_tex(card_art[c], cx - size * 0.5f, cy - size * 0.5f + lift, size, size, 22 * s, 1, 0, 0, 1, 1);
			if (r == focus_row && c == focus_col)
				draw_tex(card_title[r][c].tex, cx - card_title[r][c].w * 0.5f, cy - size * 0.5f - 30, card_title[r][c].w,
				         card_title[r][c].h, 0, 1, 0, 0, 1, 1);
		}
	}
	glDisable(GL_BLEND);
}

/* ---------- main ---------- */

int main(int argc, char **argv)
{
	int stress = 0;
	const char *log_path = DEFAULT_LOG, *shot_prefix = NULL;

	for (int i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "--stress")) stress = 1;
		else if (!strcmp(argv[i], "--log") && i + 1 < argc) log_path = argv[++i];
		else if (!strcmp(argv[i], "--shot") && i + 1 < argc) shot_prefix = argv[++i];
	}
	log_fp = fopen(log_path, "a");
	logline("=== home spike start, %s", stress ? "stress" : "normal");

	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_JOYSTICK) < 0) {
		logline("SDL_Init failed: %s", SDL_GetError());
		return 1;
	}

	/* Same order as NextUI's generic_video.c, which is known to work on the Brick. */
	SDL_SetHint(SDL_HINT_RENDER_DRIVER, "opengl");
	SDL_Window *win = SDL_CreateWindow("Home Spike", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
	                                   W, H, SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN);
	if (!win) { logline("SDL_CreateWindow failed: %s", SDL_GetError()); return 1; }
	SDL_Renderer *ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
	if (!ren) logline("SDL_CreateRenderer failed (continuing): %s", SDL_GetError());
#ifdef HOME_DEVICE
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
#else
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
#endif
	SDL_GLContext ctx = SDL_GL_CreateContext(win);
	if (!ctx) { logline("SDL_GL_CreateContext failed: %s", SDL_GetError()); return 1; }
	SDL_GL_MakeCurrent(win, ctx);
	SDL_GL_SetSwapInterval(1);
	int dw, dh;
	SDL_GL_GetDrawableSize(win, &dw, &dh);
	logline("SDL video driver: %s, drawable %dx%d, GL_RENDERER: %s", SDL_GetCurrentVideoDriver(), dw, dh,
	        glGetString(GL_RENDERER));
	if (SDL_NumJoysticks() > 0) SDL_JoystickOpen(0);

	p_rtex = program(fs_sdf, fs_rtex);
	p_fill = program(fs_sdf, fs_fill);
	p_down = program(fs_down, NULL);
	p_up = program(fs_up, NULL);

	static const float quad[8] = { 0, 0, 1, 0, 0, 1, 1, 1 };
	GLuint vao, vbo;
	glGenVertexArrays(1, &vao);
	glBindVertexArray(vao);
	glGenBuffers(1, &vbo);
	glBindBuffer(GL_ARRAY_BUFFER, vbo);
	glBufferData(GL_ARRAY_BUFFER, sizeof quad, quad, GL_STATIC_DRAW);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, 0);

	wallpaper = make_wallpaper(NULL);
	shadow = make_shadow();
	for (int c = 0; c < COLS; c++) card_art[c] = make_card_art(c);
	static const char *rows[ROWS] = { "CONTINUE PLAYING", "CONSOLES", "APPS" };
	for (int r = 0; r < ROWS; r++) {
		row_title[r].tex = make_text(rows[r], 3, &row_title[r].w, &row_title[r].h);
		for (int c = 0; c < COLS; c++) {
			char name[32];
			snprintf(name, sizeof name, "GAME %d%d", r + 1, c + 1);
			card_title[r][c].tex = make_text(name, 2, &card_title[r][c].w, &card_title[r][c].h);
			scale_spring[r][c].x = 1.0f;
		}
	}
	np_title.tex = make_text("SLEEPING PRINCESS", 3, &np_title.w, &np_title.h);
	np_artist.tex = make_text("G.E.M.", 2, &np_artist.w, &np_artist.h);

	Target snap = make_target(W, H);
	Target down[4] = { snap, make_target(W / 2, H / 2), make_target(W / 4, H / 4), make_target(W / 8, H / 8) };
	Target up[3] = { make_target(W, H), make_target(W / 2, H / 2), make_target(W / 4, H / 4) };
	const Target *blurred = NULL;

	double freq = (double)SDL_GetPerformanceFrequency();
	Uint64 t0 = SDL_GetPerformanceCounter(), last = t0;
	double win_start = 0, stage_start = 0, next_focus = 0.5;
	int win_frames = 0, win_slow = 0, focus_row = 0, focus_col = 0, running = 1;
	int stage = 0, reps = 1, panel_open = 0, log_snapshot_frame = 0, snapshot_quarter = 0;
	double win_worst = 0;
	Spring panel_y = { -400, 0 };
	char label[48];
	static const double shot_t[2] = { 3.5, 5.5 };
	int shots_done = 0;

	while (running) {
		SDL_Event e;
		while (SDL_PollEvent(&e)) {
			if (e.type == SDL_QUIT) running = 0;
			else if (e.type == SDL_KEYDOWN && (e.key.keysym.sym == SDLK_ESCAPE || e.key.keysym.sym == SDLK_q)) running = 0;
			else if (e.type == SDL_JOYBUTTONDOWN && (e.jbutton.button == JOY_B || e.jbutton.button == JOY_MENU)) running = 0;
		}

		Uint64 now = SDL_GetPerformanceCounter();
		double t = (now - t0) / freq, dt = (now - last) / freq;
		float fdt = dt > 0.05 ? 0.05f : (float)dt;
		last = now;

		if (log_snapshot_frame) {
			logline("t=%5.1fs snapshot frame (%s blur) took %.1f ms", t, snapshot_quarter ? "256X192" : "1024X768",
			        dt * 1000);
			log_snapshot_frame = 0;
		}

		/* Schedule: stress steps reps every 10 s; normal runs 60 s with a panel every 6 s. */
		int new_stage = stage;
		if (stress) {
			if (t - stage_start >= 10.0) {
				if (reps == 4) running = 0;
				else new_stage = stage + 1;
			}
		} else {
			if (t >= 60.0) running = 0;
			snapshot_quarter = t >= 30.0;
		}
		if (stress) snprintf(label, sizeof label, "HOME X%d", reps);
		else snprintf(label, sizeof label, "HOME + SNAPSHOT %s", snapshot_quarter ? "256X192" : "1024X768");

		if (dt > 0) {
			win_frames++;
			if (dt > win_worst) win_worst = dt;
			if (dt > 1.25 / 60.0) win_slow++;
		}
		if (win_frames && (t - win_start >= 5.0 || new_stage != stage || !running)) {
			logline("t=%5.1fs %s avg_fps=%.1f worst_ms=%.1f slow_frames=%d/%d", t, label,
			        win_frames / (t - win_start), win_worst * 1000, win_slow, win_frames);
			win_start = t; win_frames = 0; win_slow = 0; win_worst = 0;
		}
		if (new_stage != stage) {
			stage = new_stage;
			reps = stage + 1;
			stage_start = win_start = t;
		}
		if (!running) break;

		/* Panel: open at 4 s of every 6 s cycle, close at 6 s. */
		double cycle = fmod(t, 6.0);
		int want_panel = !stress && cycle >= 4.0;
		int snapshot_now = want_panel && !panel_open;
		panel_open = want_panel;

		/* Motion only runs while the home screen is live; behind a panel it is frozen. */
		if (!panel_open) {
			if (t >= next_focus) {
				next_focus = t + 0.5;
				if (++focus_col == COLS) { focus_col = 0; focus_row = (focus_row + 1) % ROWS; }
			}
			for (int r = 0; r < ROWS; r++) {
				for (int c = 0; c < COLS; c++)
					spring_step(&scale_spring[r][c], r == focus_row && c == focus_col ? 1.15f : 1.0f, fdt);
				float target = r == focus_row && focus_col > 2 ? (focus_col - 2) * (CARD + GAP) : 0;
				spring_step(&row_scroll[r], target, fdt);
			}
			spring_step(&parallax, (focus_col - 2.5f) / 2.5f, fdt);
			panel_y.x = -400;
			panel_y.v = 0;
		}

		if (snapshot_now) {
			/* The one expensive frame: home into a texture, then blur it once. */
			bind_target(&snap);
			draw_home(focus_row, focus_col);
			glUseProgram(p_down);
			for (int i = 1; i <= 3; i++) {
				bind_target(&down[i]);
				glBindTexture(GL_TEXTURE_2D, down[i - 1].tex);
				glUniform2f(glGetUniformLocation(p_down, "u_half"), 0.5f / down[i - 1].w, 0.5f / down[i - 1].h);
				glUniform4f(glGetUniformLocation(p_down, "u_rect"), 0, 0, 1, 1);
				glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
			}
			const Target *src = &down[3];
			glUseProgram(p_up);
			for (int i = 2; i >= (snapshot_quarter ? 2 : 0); i--) {
				bind_target(&up[i]);
				glBindTexture(GL_TEXTURE_2D, src->tex);
				glUniform2f(glGetUniformLocation(p_up, "u_half"), 0.5f / src->w, 0.5f / src->h);
				glUniform4f(glGetUniformLocation(p_up, "u_rect"), 0, 0, 1, 1);
				glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
				src = &up[i];
			}
			blurred = src;
			log_snapshot_frame = 1;
		}

		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		glViewport(0, 0, dw, dh);
		if (panel_open) {
			/* Frozen blurred snapshot, slightly dimmed, plus the sliding Now Playing panel. */
			spring_step(&panel_y, 0, fdt);
			glDisable(GL_BLEND);
			draw_tex(blurred->tex, 0, 0, W, H, 0, 1, 0, 0, 1, 1);
			glEnable(GL_BLEND);
			glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
			float pw = 720, ph = 300, x = (W - pw) * 0.5f, y = 120 + panel_y.x;
			draw_fill(x, y, pw, ph, 36, 1, 1, 1, 0.22f);
			draw_tex(shadow, x + 30 - 20, y + 40 - 28, 260, 260, 0, 0.7f, 0, 0, 1, 1);
			draw_tex(card_art[2], x + 40, y + 40, 220, 220, 22, 1, 0, 0, 1, 1);
			draw_tex(np_title.tex, x + 290, y + 200, np_title.w, np_title.h, 0, 1, 0, 0, 1, 1);
			draw_tex(np_artist.tex, x + 290, y + 165, np_artist.w, np_artist.h, 0, 0.8f, 0, 0, 1, 1);
			draw_fill(x + 290, y + 80, 380, 8, 4, 1, 1, 1, 0.35f);
			draw_fill(x + 290, y + 80, 380 * (float)((cycle - 4.0) / 2.0), 8, 4, 1, 1, 1, 0.9f);
			glDisable(GL_BLEND);
		} else {
			for (int r = 0; r < reps; r++) draw_home(focus_row, focus_col);
		}

		/* --shot PREFIX saves the live home screen at 3.5 s and the open panel at 5.5 s. */
		if (shot_prefix && shots_done < 2 && t >= shot_t[shots_done]) {
			char path[512];
			Uint8 *rgba = malloc(dw * dh * 4);
			SDL_Surface *img = SDL_CreateRGBSurfaceWithFormat(0, dw, dh, 32, SDL_PIXELFORMAT_ABGR8888);
			glReadPixels(0, 0, dw, dh, GL_RGBA, GL_UNSIGNED_BYTE, rgba);
			for (int y = 0; y < dh; y++)
				memcpy((Uint8 *)img->pixels + y * img->pitch, rgba + (dh - 1 - y) * dw * 4, dw * 4);
			snprintf(path, sizeof path, "%s-%d.bmp", shot_prefix, shots_done);
			SDL_SaveBMP(img, path);
			SDL_FreeSurface(img);
			free(rgba);
			shots_done++;
		}

		SDL_GL_SwapWindow(win);
	}

	logline("=== home spike end");
	if (log_fp) fclose(log_fp);
	SDL_GL_DeleteContext(ctx);
	if (ren) SDL_DestroyRenderer(ren);
	SDL_DestroyWindow(win);
	SDL_Quit();
	return 0;
}
