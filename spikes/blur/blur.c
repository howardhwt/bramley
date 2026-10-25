/* Blur Spike: can the Brick blur a moving panel over a wallpaper at 1024x768 and hold 60 fps?
 *
 * A rounded panel slides over a wallpaper with moving tiles, and shows a live Dual Kawase
 * blur of the whole frame behind it. Every mode downsamples to 128x96, so the blur looks the
 * same in each; the modes differ only in the resolution of the final upsample:
 *   FULL 1024x768, HALF 512x384, QUARTER 256x192, NONE (no blur, baseline).
 * The whole frame is blurred every frame, which is the worst case.
 *
 * D-pad left/right (or arrow keys) switches mode. B, MENU, Esc or Q quits.
 * --auto runs FULL 60 s, HALF 30 s, QUARTER 30 s, NONE 15 s and exits.
 * --novsync asks for swap interval 0, so fps is no longer capped at the refresh rate.
 * Every 5 seconds the average fps, worst frame, slow frame count and render time go to the log.
 * Render time is measured with glFinish. On the Brick glFinish waits for the display, so it reads 16 ms there.
 * --stress runs FULL with the blur repeated K times per frame, K = 1, 2, 3, 4, 6, 8, 12, 16, for 10 s each.
 * The largest K that holds 60 fps is the GPU headroom, in units of one full-frame blur.
 */
#include <SDL.h>
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef BLUR_DEVICE
#include <GLES3/gl3.h>
#define SHADER_HEADER "#version 300 es\nprecision mediump float;\n"
#define DEFAULT_LOG "/mnt/SDCARD/spike/blur-spike.log"
#else
#define GL_SILENCE_DEPRECATION
#include <OpenGL/gl3.h>
#define SHADER_HEADER "#version 330 core\n"
#define DEFAULT_LOG "blur-spike.log"
#endif

#define W 1024
#define H 768
#define JOY_B 0
#define JOY_MENU 8

enum { MODE_FULL, MODE_HALF, MODE_QUARTER, MODE_NONE, MODE_COUNT };
static const char *mode_name[MODE_COUNT] = { "FULL 1024X768", "HALF 512X384", "QUARTER 256X192", "NONE" };
static const float auto_secs[MODE_COUNT] = { 60, 30, 30, 15 };

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
	"uniform vec4 u_rect;\n" /* x, y, w, h in 0..1 of the target, bottom-left origin */
	"out vec2 v_uv;\n"
	"void main() {\n"
	"	v_uv = a_pos;\n"
	"	vec2 p = u_rect.xy + a_pos * u_rect.zw;\n"
	"	gl_Position = vec4(p * 2.0 - 1.0, 0.0, 1.0);\n"
	"}\n";

static const char *fs_tex =
	"uniform sampler2D u_tex;\n"
	"in vec2 v_uv; out vec4 frag;\n"
	"void main() { frag = texture(u_tex, v_uv); }\n";

static const char *fs_sdf =
	"float sd_rrect(vec2 p, vec2 b, float r) {\n"
	"	vec2 q = abs(p) - b + r;\n"
	"	return length(max(q, 0.0)) + min(max(q.x, q.y), 0.0) - r;\n"
	"}\n";

static const char *fs_tile =
	"uniform vec4 u_px;\n" /* x, y, w, h in pixels */
	"uniform float u_radius;\n"
	"uniform vec4 u_color;\n"
	"in vec2 v_uv; out vec4 frag;\n"
	"void main() {\n"
	"	vec2 local = gl_FragCoord.xy - (u_px.xy + u_px.zw * 0.5);\n"
	"	float d = sd_rrect(local, u_px.zw * 0.5, u_radius);\n"
	"	frag = vec4(u_color.rgb, u_color.a * clamp(0.5 - d, 0.0, 1.0));\n"
	"}\n";

static const char *fs_panel =
	"uniform sampler2D u_tex;\n"
	"uniform vec2 u_screen;\n"
	"uniform vec4 u_px;\n"
	"uniform float u_radius;\n"
	"uniform float u_tint;\n"
	"in vec2 v_uv; out vec4 frag;\n"
	"void main() {\n"
	"	vec2 local = gl_FragCoord.xy - (u_px.xy + u_px.zw * 0.5);\n"
	"	float d = sd_rrect(local, u_px.zw * 0.5, u_radius);\n"
	"	vec3 bg = texture(u_tex, gl_FragCoord.xy / u_screen).rgb;\n"
	"	vec3 col = mix(bg, vec3(1.0), u_tint);\n"
	"	col = mix(col, vec3(1.0), clamp(1.0 - abs(d + 1.0), 0.0, 1.0) * 0.4);\n"
	"	frag = vec4(col, clamp(0.5 - d, 0.0, 1.0));\n"
	"}\n";

/* Dual Kawase (Bjorge, SIGGRAPH 2015). u_half is half a source texel. */
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

/* ---------- render targets ---------- */

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

static void draw_rect(GLuint prog, float x, float y, float w, float h)
{
	glUniform4f(glGetUniformLocation(prog, "u_rect"), x, y, w, h);
	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
}

/* ---------- wallpaper ---------- */

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

/* ---------- 5x7 HUD font ---------- */

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

#define HUD_W 512
#define HUD_H 40
#define HUD_SCALE 2

static void hud_render(Uint32 *px, const char *l1, const char *l2)
{
	const char *lines[2] = { l1, l2 };
	memset(px, 0, HUD_W * HUD_H * 4);
	for (int y = 0; y < HUD_H; y++)
		for (int x = 0; x < HUD_W; x++) px[y * HUD_W + x] = 0x99000000u;
	for (int l = 0; l < 2; l++)
		for (int i = 0; lines[l][i]; i++) {
			const char *g = glyph(lines[l][i] >= 'a' && lines[l][i] <= 'z' ? lines[l][i] - 32 : lines[l][i]);
			if (!g) continue;
			for (int gy = 0; gy < 7; gy++)
				for (int gx = 0; gx < 5; gx++) {
					if (g[gy * 5 + gx] != '#') continue;
					for (int sy = 0; sy < HUD_SCALE; sy++)
						for (int sx = 0; sx < HUD_SCALE; sx++) {
							int x = 4 + i * 6 * HUD_SCALE + gx * HUD_SCALE + sx;
							int y = 3 + l * 9 * HUD_SCALE + gy * HUD_SCALE + sy;
							if (x < HUD_W && y < HUD_H) px[(HUD_H - 1 - y) * HUD_W + x] = 0xffffffffu;
						}
				}
		}
}

/* ---------- main ---------- */

int main(int argc, char **argv)
{
	int auto_mode = 0, vsync = 1, stress = 0;
	const char *log_path = DEFAULT_LOG, *bmp_path = NULL, *shot_prefix = NULL;

	for (int i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "--auto")) auto_mode = 1;
		else if (!strcmp(argv[i], "--novsync")) vsync = 0;
		else if (!strcmp(argv[i], "--stress")) stress = 1;
		else if (!strcmp(argv[i], "--log") && i + 1 < argc) log_path = argv[++i];
		else if (!strcmp(argv[i], "--wallpaper") && i + 1 < argc) bmp_path = argv[++i];
		else if (!strcmp(argv[i], "--shot") && i + 1 < argc) shot_prefix = argv[++i];
	}
	log_fp = fopen(log_path, "a");
	logline("=== blur spike start, %s, %s", stress ? "stress" : auto_mode ? "auto" : "manual", vsync ? "vsync" : "novsync");

	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_JOYSTICK) < 0) {
		logline("SDL_Init failed: %s", SDL_GetError());
		return 1;
	}

	/* Same order as NextUI's generic_video.c, which is known to work on the Brick. */
	SDL_SetHint(SDL_HINT_RENDER_DRIVER, "opengl");
	SDL_Window *win = SDL_CreateWindow("Blur Spike", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
	                                   W, H, SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN);
	if (!win) { logline("SDL_CreateWindow failed: %s", SDL_GetError()); return 1; }
	SDL_Renderer *ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
	if (!ren) logline("SDL_CreateRenderer failed (continuing): %s", SDL_GetError());
#ifdef BLUR_DEVICE
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
	int swap = SDL_GL_SetSwapInterval(vsync);
	int dw, dh;
	SDL_GL_GetDrawableSize(win, &dw, &dh);
	logline("SDL video driver: %s, drawable %dx%d, swap interval %d %s", SDL_GetCurrentVideoDriver(), dw, dh,
	        vsync, swap == 0 ? "accepted" : "rejected");
	logline("GL_VENDOR: %s", glGetString(GL_VENDOR));
	logline("GL_RENDERER: %s", glGetString(GL_RENDERER));
	logline("GL_VERSION: %s", glGetString(GL_VERSION));

	SDL_Joystick *joy = SDL_NumJoysticks() > 0 ? SDL_JoystickOpen(0) : NULL;
	if (joy) logline("joystick: %s", SDL_JoystickName(joy));

	GLuint p_tex = program(fs_tex, NULL);
	GLuint p_tile = program(fs_sdf, fs_tile);
	GLuint p_panel = program(fs_sdf, fs_panel);
	GLuint p_down = program(fs_down, NULL);
	GLuint p_up = program(fs_up, NULL);

	static const float quad[8] = { 0, 0, 1, 0, 0, 1, 1, 1 };
	GLuint vao, vbo;
	glGenVertexArrays(1, &vao);
	glBindVertexArray(vao);
	glGenBuffers(1, &vbo);
	glBindBuffer(GL_ARRAY_BUFFER, vbo);
	glBufferData(GL_ARRAY_BUFFER, sizeof quad, quad, GL_STATIC_DRAW);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, 0);

	GLuint wallpaper = make_wallpaper(bmp_path);
	Target scene = make_target(W, H);
	Target down[4] = { scene, make_target(W / 2, H / 2), make_target(W / 4, H / 4), make_target(W / 8, H / 8) };
	Target up[3] = { make_target(W, H), make_target(W / 2, H / 2), make_target(W / 4, H / 4) };

	Uint32 *hud_px = malloc(HUD_W * HUD_H * 4);
	GLuint hud = make_tex(HUD_W, HUD_H, NULL);

	static const int stress_reps[] = { 1, 2, 3, 4, 6, 8, 12, 16 };
	const int stress_stages = sizeof stress_reps / sizeof stress_reps[0];
	int mode = MODE_FULL, running = 1, shot_mode = -1, stage = 0, reps = 1;
	char label[48];
	if (stress) auto_mode = 0;
	double freq = (double)SDL_GetPerformanceFrequency();
	Uint64 t0 = SDL_GetPerformanceCounter(), last = t0;
	double mode_start = 0, win_start = 0, hud_start = 0;
	int win_frames = 0, win_slow = 0, hud_frames = 0;
	double win_worst = 0, hud_fps = 0, win_render = 0, win_render_max = 0, render_ms = 0;

	while (running) {
		int new_mode = mode;
		SDL_Event e;
		while (SDL_PollEvent(&e)) {
			if (e.type == SDL_QUIT) running = 0;
			else if (e.type == SDL_KEYDOWN) {
				SDL_Keycode k = e.key.keysym.sym;
				if (k == SDLK_ESCAPE || k == SDLK_q) running = 0;
				else if (k == SDLK_RIGHT) new_mode = (mode + 1) % MODE_COUNT, auto_mode = 0;
				else if (k == SDLK_LEFT) new_mode = (mode + MODE_COUNT - 1) % MODE_COUNT, auto_mode = 0;
			} else if (e.type == SDL_JOYHATMOTION) {
				if (e.jhat.value & SDL_HAT_RIGHT) new_mode = (mode + 1) % MODE_COUNT, auto_mode = 0;
				else if (e.jhat.value & SDL_HAT_LEFT) new_mode = (mode + MODE_COUNT - 1) % MODE_COUNT, auto_mode = 0;
			} else if (e.type == SDL_JOYBUTTONDOWN) {
				if (e.jbutton.button == JOY_B || e.jbutton.button == JOY_MENU) running = 0;
			}
		}

		Uint64 now = SDL_GetPerformanceCounter();
		double t = (now - t0) / freq, dt = (now - last) / freq;
		last = now;

		if (auto_mode && t - mode_start >= auto_secs[mode]) {
			if (mode + 1 == MODE_COUNT) running = 0;
			else new_mode = mode + 1;
		}
		int new_stage = stage;
		if (stress) {
			new_mode = MODE_FULL;
			if (t - mode_start >= 10.0) {
				if (stage + 1 == stress_stages) running = 0;
				else new_stage = stage + 1;
			}
		}
		if (stress) snprintf(label, sizeof label, "FULL X%d", reps);
		else snprintf(label, sizeof label, "%s", mode_name[mode]);

		/* Count the frame that just finished, then roll the 5 s window. */
		if (dt > 0) {
			win_frames++;
			hud_frames++;
			if (dt > win_worst) win_worst = dt;
			if (dt > 1.25 / 60.0) win_slow++;
			win_render += render_ms;
			if (render_ms > win_render_max) win_render_max = render_ms;
		}
		if (win_frames && (t - win_start >= 5.0 || new_mode != mode || new_stage != stage || !running)) {
			logline("t=%5.1fs mode=%s avg_fps=%.1f worst_ms=%.1f slow_frames=%d/%d render_ms avg=%.2f max=%.2f",
			        t, label, win_frames / (t - win_start), win_worst * 1000, win_slow, win_frames,
			        win_render / win_frames, win_render_max);
			win_start = t; win_frames = 0; win_slow = 0; win_worst = 0; win_render = 0; win_render_max = 0;
		}
		if (new_mode != mode || new_stage != stage) {
			mode = new_mode;
			stage = new_stage;
			reps = stress ? stress_reps[stage] : 1;
			mode_start = win_start = t;
			win_frames = win_slow = 0;
			win_worst = win_render = win_render_max = 0;
		}
		if (!running) break;

		Uint64 render_start = SDL_GetPerformanceCounter();

		/* 1. Scene: wallpaper plus a row of moving tiles, rendered fresh every frame. */
		bind_target(&scene);
		glDisable(GL_BLEND);
		glUseProgram(p_tex);
		glBindTexture(GL_TEXTURE_2D, wallpaper);
		draw_rect(p_tex, 0, 0, 1, 1);
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glUseProgram(p_tile);
		glUniform1f(glGetUniformLocation(p_tile, "u_radius"), 28);
		for (int i = 0; i < 8; i++) {
			float x = fmodf(i * 150.0f + (float)t * 90.0f, W + 150.0f) - 150.0f, y = 300, s = 128;
			float hue = i / 8.0f * 6.2832f;
			glUniform4f(glGetUniformLocation(p_tile, "u_px"), x, y, s, s);
			glUniform4f(glGetUniformLocation(p_tile, "u_color"), 0.5f + 0.5f * cosf(hue),
			            0.5f + 0.5f * cosf(hue + 2.09f), 0.5f + 0.5f * cosf(hue + 4.19f), 1.0f);
			draw_rect(p_tile, (x - 2) / W, (y - 2) / H, (s + 4) / W, (s + 4) / H);
		}
		glDisable(GL_BLEND);

		/* 2. Blur: down to 128x96, then up to the mode's output size. */
		const Target *blurred = &scene;
		for (int r = 0; r < reps && mode != MODE_NONE; r++) {
			glUseProgram(p_down);
			for (int i = 1; i <= 3; i++) {
				bind_target(&down[i]);
				glBindTexture(GL_TEXTURE_2D, down[i - 1].tex);
				glUniform2f(glGetUniformLocation(p_down, "u_half"), 0.5f / down[i - 1].w, 0.5f / down[i - 1].h);
				draw_rect(p_down, 0, 0, 1, 1);
			}
			int stop = mode == MODE_FULL ? 0 : mode == MODE_HALF ? 1 : 2;
			const Target *src = &down[3];
			glUseProgram(p_up);
			for (int i = 2; i >= stop; i--) {
				bind_target(&up[i]);
				glBindTexture(GL_TEXTURE_2D, src->tex);
				glUniform2f(glGetUniformLocation(p_up, "u_half"), 0.5f / src->w, 0.5f / src->h);
				draw_rect(p_up, 0, 0, 1, 1);
				src = &up[i];
			}
			blurred = src;
		}

		/* 3. Screen: scene, then the sliding panel, then the HUD. */
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		glViewport(0, 0, dw, dh);
		glUseProgram(p_tex);
		glBindTexture(GL_TEXTURE_2D, scene.tex);
		draw_rect(p_tex, 0, 0, 1, 1);

		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		float pw = 600, ph = 300, px = (W - pw) * 0.5f * (1.0f + sinf((float)t * 0.8f)), py = (H - ph) * 0.5f;
		float sx = (float)dw / W, sy = (float)dh / H;
		glUseProgram(p_panel);
		glBindTexture(GL_TEXTURE_2D, blurred->tex);
		glUniform2f(glGetUniformLocation(p_panel, "u_screen"), (float)dw, (float)dh);
		glUniform4f(glGetUniformLocation(p_panel, "u_px"), px * sx, py * sy, pw * sx, ph * sy);
		glUniform1f(glGetUniformLocation(p_panel, "u_radius"), 36 * sx);
		glUniform1f(glGetUniformLocation(p_panel, "u_tint"), mode == MODE_NONE ? 0.55f : 0.18f);
		draw_rect(p_panel, (px - 2) / W, (py - 2) / H, (pw + 4) / W, (ph + 4) / H);

		if (t - hud_start >= 0.5) {
			char l1[64], l2[64];
			hud_fps = hud_frames / (t - hud_start);
			hud_start = t;
			hud_frames = 0;
			snprintf(l1, sizeof l1, "%5.1f FPS  %s", hud_fps, label);
			snprintf(l2, sizeof l2, "%s  < > MODE", stress ? "STRESS" : auto_mode ? "AUTO" : "MANUAL");
			hud_render(hud_px, l1, l2);
			glBindTexture(GL_TEXTURE_2D, hud);
			glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, HUD_W, HUD_H, GL_RGBA, GL_UNSIGNED_BYTE, hud_px);
		}
		glUseProgram(p_tex);
		glBindTexture(GL_TEXTURE_2D, hud);
		draw_rect(p_tex, 8.0f / W, (H - 8.0f - HUD_H) / H, (float)HUD_W / W, (float)HUD_H / H);
		glDisable(GL_BLEND);
		glFinish();
		render_ms = (SDL_GetPerformanceCounter() - render_start) * 1000.0 / freq;

		/* --shot PREFIX saves one BMP per mode, 3 s after the mode starts. */
		if (shot_prefix && shot_mode != mode && t - mode_start >= 3.0) {
			char path[512];
			Uint8 *rgba = malloc(dw * dh * 4);
			SDL_Surface *img = SDL_CreateRGBSurfaceWithFormat(0, dw, dh, 32, SDL_PIXELFORMAT_ABGR8888);
			glReadPixels(0, 0, dw, dh, GL_RGBA, GL_UNSIGNED_BYTE, rgba);
			for (int y = 0; y < dh; y++)
				memcpy((Uint8 *)img->pixels + y * img->pitch, rgba + (dh - 1 - y) * dw * 4, dw * 4);
			snprintf(path, sizeof path, "%s-%d.bmp", shot_prefix, mode);
			SDL_SaveBMP(img, path);
			SDL_FreeSurface(img);
			free(rgba);
			shot_mode = mode;
		}

		SDL_GL_SwapWindow(win);
	}

	logline("=== blur spike end");
	if (log_fp) fclose(log_fp);
	SDL_GL_DeleteContext(ctx);
	if (ren) SDL_DestroyRenderer(ren);
	SDL_DestroyWindow(win);
	SDL_Quit();
	return 0;
}
