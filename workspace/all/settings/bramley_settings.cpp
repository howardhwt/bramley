// Bramley: Settings drawn with the launcher's GL kit. See bramley_settings.hpp.
#include "bramley_settings.hpp"

#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <unistd.h>
#include <atomic>
#include <map>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <vector>

#include "menu.hpp"

extern "C"
{
#include "lgfx.h"
#include "bramley.h"
#include "lwallpaper.h"
}

#define W 1024
#define H 768

// The friend of MenuList and AbstractMenuItem (menu.hpp): reads lists, items and selections.
struct BramleyView
{
	static MenuItemType type(MenuList *l) { return l->type; }
	static int selected(MenuList *l) { return l->scope.selected; }
	static std::shared_mutex &lock(MenuList *l) { return l->itemLock; }
	static std::vector<AbstractMenuItem *> &items(MenuList *l) { return l->items; }
	static std::any live(AbstractMenuItem *i) { return i->on_get ? i->on_get() : i->getValue(); }
};

namespace
{

// Milliseconds since this process started, for the startup log.
double since_start()
{
	struct timespec ts;
#ifdef CLOCK_BOOTTIME
	clock_gettime(CLOCK_BOOTTIME, &ts);
#else
	return -1; // the desktop build has no /proc
#endif
	unsigned long long start = 0;
	if (FILE *f = fopen("/proc/self/stat", "r"))
	{
		char buf[1024];
		size_t n = fread(buf, 1, sizeof(buf) - 1, f);
		fclose(f);
		buf[n] = 0;
		char *p = strrchr(buf, ')');
		for (int field = 2; p && field < 22; field++)
			p = strchr(p + 1, ' ');
		if (p)
			start = strtoull(p + 1, nullptr, 10);
	}
	if (!start)
		return -1;
	return (ts.tv_sec + ts.tv_nsec / 1e9 - (double)start / sysconf(_SC_CLK_TCK)) * 1000;
}

bool ready;
LTex *backdrop;

// Text textures, kept between frames; Settings only redraws when something changes.
std::map<std::string, LTex *> text_cache;

LTex *text_tex(int font, const std::string &s)
{
	std::string key = std::to_string(font) + "|" + s;
	auto it = text_cache.find(key);
	if (it != text_cache.end())
		return it->second;
	if (text_cache.size() > 400)
	{
		for (auto &kv : text_cache)
			ltex_free(kv.second);
		text_cache.clear();
	}
	LTex *t = ltex_text(font, s.c_str(), 0);
	text_cache[key] = t;
	return t;
}

float text_w(int font, const std::string &s)
{
	LTex *t = text_tex(font, s);
	return t ? t->w : 0;
}

void text(int font, const std::string &s, float x, float y, float alpha, int right = 0)
{
	LTex *t = text_tex(font, s);
	if (t)
		lgfx_draw_tex(t, right ? x - t->w : x, y, t->w, t->h, 0, alpha);
}

std::map<std::string, LTex *> icons;

LTex *icon(const std::string &slug, int size)
{
	std::string key = slug + "-" + std::to_string(size);
	auto it = icons.find(key);
	if (it != icons.end())
		return it->second;
	char path[512];
	snprintf(path, sizeof(path), "%s/settings/%s.png", BRAMLEY_RES_DIR, key.c_str());
	LTex *t = ltex_load(path, 0);
	icons[key] = t;
	return t;
}

// NextUI's section names, mapped to our icon, our display name and our group.
struct Section
{
	const char *name, *slug, *shown;
	int group;
};
const Section sections[] = {
	{"Network", "network", "Network", 0},       {"Bluetooth", "bluetooth", "Bluetooth", 0},
	{"Display", "display", "Display", 1},       {"Appearance", "appearance", "Appearance", 1},
	{"In-Game", "in-game", "In-Game", 2},       {"Assignments", "buttons", "Buttons", 2},
	{"FN switch", "fn-switch", "FN Switch", 2}, {"System", "system", "System", 3},
	{"About", "about", "About", 3},
};

const Section *section_of(const std::string &name)
{
	for (auto &s : sections)
		if (name == s.name)
			return &s;
	return nullptr;
}

std::string shown_name(const std::string &name)
{
	const Section *s = section_of(name);
	return s ? s->shown : name;
}

// Wi-Fi and Bluetooth status for the summary cards, read at most every two seconds.
struct Radio
{
	Uint32 at;
	std::string wifi, bt, ssid; // ssid: the network Wi-Fi is connected to, if any
	bool wifi_busy;             // switching on, looking for networks or joining one
	Uint32 wifi_on_at;          // when Wi-Fi was seen switching on
	int wifi_seen;              // -1 before the first look, else whether Wi-Fi was on
} radio = {0, "", "", "", false, 0, -1};

// Networks in the Wi-Fi list at the last frame, or -1 when the Network page was not drawn.
int wifi_networks = -1;
// Set when a frame drew a spinner, so the next frame comes soon and it keeps turning.
bool spinning;

void update_radio()
{
	Uint32 now = SDL_GetTicks();
	if (radio.at && now - radio.at < (radio.wifi_busy ? 500u : 2000u))
		return;
	radio.at = now;
	radio.ssid.clear();
	bool on = PLAT_wifiEnabled();
	if (radio.wifi_seen == 0 && on)
		radio.wifi_on_at = now; // just switched on: give it time to find and join a network
	radio.wifi_seen = on;
	radio.wifi_busy = false;
	if (!on)
		radio.wifi = "Wi-Fi is off";
	else
	{
		struct WIFI_connection c = {};
		if (PLAT_wifiConnected() && PLAT_wifiConnection(&c) == 0 && c.valid && c.ssid[0])
		{
			radio.ssid = c.ssid;
			radio.wifi = std::string("Connected to ") + c.ssid;
		}
		else if (wifi_networks == 0)
		{
			radio.wifi = "Looking for networks\xE2\x80\xA6";
			radio.wifi_busy = true;
		}
		else if (radio.wifi_on_at && now - radio.wifi_on_at < 20000)
		{
			radio.wifi = "Connecting\xE2\x80\xA6";
			radio.wifi_busy = true;
		}
		else
			radio.wifi = "Not connected";
	}
	if (!PLAT_bluetoothEnabled())
		radio.bt = "Bluetooth is off";
	else
		radio.bt = PLAT_btIsConnected() ? "Connected" : "On, nothing connected";
}

// A turning ring of dots, as iOS shows while it works.
void draw_spinner(float cx, float cy, float r)
{
	int step = (SDL_GetTicks() / 90) % 8;
	for (int i = 0; i < 8; i++)
	{
		float a = (float)i / 8 * 6.2832f, d = r * 0.22f;
		float alpha = 0.15f + 0.85f * (float)((i - step + 8) % 8) / 7;
		lgfx_draw_fill(cx + cosf(a) * r - d, cy + sinf(a) * r - d, d * 2, d * 2, d, 1, 1, 1, alpha);
	}
	spinning = true;
}

// Values: how a setting's current value is drawn.
enum Kind
{
	K_NONE,   // no value: a section or an action
	K_TEXT,   // a value shown as text
	K_SWITCH, // On and Off
	K_SLIDER, // a long numeric range
	K_COLOR,  // a colour swatch
};

struct Value
{
	Kind kind;
	std::string label;
	float frac;
	bool on;
	uint32_t color;
};

// A whole number, optionally ending in a percent sign.
bool number(std::string s, int *out)
{
	if (!s.empty() && s.back() == '%')
		s.pop_back();
	if (s.empty())
		return false;
	char *end;
	long n = strtol(s.c_str(), &end, 10);
	*out = (int)n;
	return *end == 0;
}

// A level such as volume or brightness: evenly spaced numbers, the first entry may be a word
// such as "Muted" or "Off".
bool is_level(const std::vector<std::string> &labels)
{
	if (labels.size() < 11)
		return false;
	int first, a, b;
	size_t start = number(labels[0], &first) ? 0 : 1;
	if (!number(labels[start], &a) || !number(labels[start + 1], &b) || b == a)
		return false;
	for (size_t i = start; i < labels.size(); i++)
	{
		int n;
		if (!number(labels[i], &n) || n != a + (int)(i - start) * (b - a))
			return false;
	}
	return true;
}

Value value_of(AbstractMenuItem *item)
{
	Value v = {K_NONE, "", 0, false, 0};
	std::vector<std::any> values = item->getValues();
	if (values.empty() || item->getType() == ListItemType::Button || item->getType() == ListItemType::Custom)
		return v;
	v.label = item->getLabel();
	std::vector<std::string> labels = item->getLabels();
	if (item->getType() == ListItemType::Color)
	{
		try
		{
			v.color = std::any_cast<uint32_t>(BramleyView::live(item));
			v.kind = K_COLOR;
			return v;
		}
		catch (const std::bad_any_cast &)
		{
		}
	}
	if (labels.size() == 2 && ((labels[0] == "Off" && labels[1] == "On") || (labels[0] == "On" && labels[1] == "Off")))
	{
		v.kind = K_SWITCH;
		v.on = v.label == "On";
		return v;
	}
	if (is_level(labels))
	{
		for (size_t i = 0; i < labels.size(); i++)
			if (labels[i] == v.label)
				v.frac = (float)i / (labels.size() - 1);
		v.kind = K_SLIDER;
		return v;
	}
	v.kind = v.label.empty() || item->getType() == ListItemType::Custom ? K_NONE : K_TEXT;
	return v;
}

void draw_switch(float right, float y, bool on, bool focused)
{
	float w = 58, h = 34;
	if (on)
		lgfx_draw_fill(right - w, y, w, h, h * 0.5f, 0.20f, 0.78f, 0.35f, 1);
	else
		lgfx_draw_fill(right - w, y, w, h, h * 0.5f, focused ? 0 : 1, focused ? 0 : 1, focused ? 0 : 1, focused ? 0.15f : 0.22f);
	float k = h - 6;
	lgfx_draw_fill(on ? right - k - 3 : right - w + 3, y + 3, k, k, k * 0.5f, 1, 1, 1, 1);
}

void draw_slider(float right, float cy, float frac, bool focused)
{
	float sw = 200, sx = right - sw, sy = cy - 3, c = focused ? 0.1f : 1;
	lgfx_draw_fill(sx, sy, sw, 6, 3, focused ? 0 : 1, focused ? 0 : 1, focused ? 0 : 1, 0.2f);
	lgfx_draw_fill(sx, sy, sw * frac, 6, 3, c, c, c, 0.95f);
	lgfx_draw_fill(sx + sw * frac - 12, sy - 9, 24, 24, 12, 1, 1, 1, 1);
	if (focused)
		lgfx_draw_ring(sx + sw * frac - 12, sy - 9, 24, 24, 12, 1, 0, 0, 0, 0.25f);
}

// One setting row in the pane.
// Set while the Network page is drawn, so the connected network's row says so.
bool marking_wifi;

void draw_row(AbstractMenuItem *item, float x, float y, float w, float h, bool focused, bool active)
{
	if (focused)
	{
		x -= 6, w += 12, y -= 3, h += 6;
		lgfx_draw_shadow(x, y, w, h, 6, 0.32f);
		lgfx_draw_fill(x, y, w, h, 12, 1, 1, 1, 0.96f);
		lgfx_set_dim(0.1f);
	}
	else
		lgfx_draw_fill(x, y, w, h, 12, 1, 1, 1, 0.06f);
	float ty = y + (h - 30) * 0.5f, right = x + w - 20;
	text(LFONT_BODY, item->getName(), x + 20, ty, focused ? 1 : active ? 0.92f : 0.6f);
	Value v = value_of(item);
	switch (v.kind)
	{
	case K_SWITCH:
		lgfx_set_dim(1);
		draw_switch(right, y + (h - 34) * 0.5f, v.on, focused);
		break;
	case K_SLIDER:
		lgfx_set_dim(1);
		draw_slider(right, y + h * 0.5f, v.frac, focused);
		break;
	case K_COLOR:
	{
		lgfx_set_dim(1);
		// NextUI's colours are RGBA, red in the top byte
		float s = 28, r = (v.color >> 24 & 0xFF) / 255.0f, g = (v.color >> 16 & 0xFF) / 255.0f, b = (v.color >> 8 & 0xFF) / 255.0f;
		lgfx_draw_fill(right - s, y + (h - s) * 0.5f, s, s, s * 0.5f, r, g, b, 1);
		lgfx_draw_ring(right - s, y + (h - s) * 0.5f, s, s, s * 0.5f, 1.5f, focused ? 0 : 1, focused ? 0 : 1, focused ? 0 : 1, 0.3f);
		break;
	}
	case K_TEXT:
	{
		lgfx_set_dim(focused ? 0.45f : 1);
		// on the focused row, arrows show that left and right change the value
		std::string label = focused && item->getValues().size() > 1 ? "\xE2\x80\xB9  " + v.label + "  \xE2\x80\xBA" : v.label;
		text(LFONT_BODY, label, right, ty, focused ? 1 : 0.5f, 1);
		break;
	}
	case K_NONE:
		if (marking_wifi && !radio.ssid.empty() && item->getName() == radio.ssid)
		{
			// the connected network: a green dot and "Connected", as iOS marks it
			float tw = text_w(LFONT_BODY, "Connected");
			lgfx_set_dim(focused ? 0.45f : 1);
			text(LFONT_BODY, "Connected", right, ty, focused ? 1 : 0.7f, 1);
			lgfx_set_dim(1);
			lgfx_draw_fill(right - tw - 22, y + h * 0.5f - 6, 12, 12, 6, 0.20f, 0.78f, 0.35f, 1);
		}
		else if (item->getSubMenu())
		{
			lgfx_set_dim(focused ? 0.35f : 1);
			text(LFONT_BODY, "\xE2\x80\xBA", right, ty, focused ? 1 : 0.4f, 1);
		}
		break;
	}
	lgfx_set_dim(1);
}

// Scroll position per list, so the focused row stays in view.
std::map<MenuList *, int> scroll;

void draw_list(MenuList *list, float x, float y, float w, float bottom, bool active)
{
	std::shared_lock<std::shared_mutex> lock(BramleyView::lock(list));
	auto &items = BramleyView::items(list);
	int n = items.size(), sel = active ? BramleyView::selected(list) : -1;
	const float rh = 48, gap = 6;
	if (marking_wifi)
		wifi_networks = n > 2 ? n - 2 : 0; // the first two rows are the Wi-Fi switches
	// leave room under the rows for the focused row's description
	int visible = (int)((bottom - y - 64) / (rh + gap));
	if (visible < 1)
		visible = 1;
	int &top = scroll[list];
	if (sel >= 0)
	{
		if (sel < top)
			top = sel;
		if (sel >= top + visible)
			top = sel - visible + 1;
	}
	if (top > n - visible)
		top = n - visible;
	if (top < 0)
		top = 0;
	float yy = y;
	for (int i = top; i < n && i < top + visible; i++, yy += rh + gap)
		draw_row(items[i], x + 20, yy, w - 40, rh, i == sel, active);
	if (top > 0)
		text(LFONT_SMALL, "\xE2\x96\xB2", x + w * 0.5f, y - 26, 0.35f);
	if (top + visible < n)
		text(LFONT_SMALL, "\xE2\x96\xBC", x + w * 0.5f, yy - 6, 0.35f);
	if (sel >= 0 && sel < n && !items[sel]->getDesc().empty())
	{
		LTex *d = ltex_text_wrapped(LFONT_SMALL, items[sel]->getDesc().c_str(), (int)w - 80, 2, 0);
		if (d)
		{
			lgfx_draw_tex(d, x + 40, yy + 20, d->w, d->h, 0, 0.5f);
			ltex_free(d);
		}
	}
}

void draw_summary(const Section *s, float x, float y, float w, float h)
{
	update_radio();
	bool wifi = !strcmp(s->slug, "network");
	float is = 150, ix = x + (w - is) * 0.5f, iy = y + 110;
	lgfx_draw_shadow(ix, iy, is, is, 12, 0.5f);
	if (LTex *ic = icon(s->slug, 150))
		lgfx_draw_tex(ic, ix, iy, is, is, 0, 1);
	float ty = iy + is + 28;
	LTex *t = text_tex(LFONT_TITLE, s->shown);
	if (t)
	{
		lgfx_draw_tex(t, x + (w - t->w) * 0.5f, ty, t->w, t->h, 0, 1);
		ty += t->h + 10;
	}
	t = text_tex(LFONT_BODY, wifi ? radio.wifi : radio.bt);
	if (t)
	{
		lgfx_draw_tex(t, x + (w - t->w) * 0.5f, ty, t->w, t->h, 0, 0.8f);
		ty += t->h;
	}
	t = text_tex(LFONT_BODY, wifi ? "A   Choose a network" : "A   Open Bluetooth");
	if (t)
	{
		float bw = t->w + 64, bh = 52, bx = x + (w - bw) * 0.5f, by = ty + 26;
		lgfx_draw_fill(bx, by, bw, bh, bh * 0.5f, 1, 1, 1, 0.14f);
		lgfx_draw_tex(t, bx + 32, by + (bh - t->h) * 0.5f, t->w, t->h, 0, 0.95f);
	}
}

void draw_top_bar()
{
	text(LFONT_LABEL, "Settings", 48, 30, 0.55f);
	time_t now = time(NULL);
	struct tm tm;
	localtime_r(&now, &tm);
	char clock[16], pct[16];
	strftime(clock, sizeof(clock), "%H:%M", &tm);
	snprintf(pct, sizeof(pct), "%d%%", PWR_getBattery());
	text(LFONT_LABEL, pct, W - 48, 30, 0.8f, 1);
	text(LFONT_LABEL, clock, W - 48 - text_w(LFONT_LABEL, pct) - 28, 30, 0.8f, 1);
}

// The chosen wallpaper behind Settings, blurred. Bramley's own wallpapers ship a small blurred
// copy, which loads in a few milliseconds; any other picture is blurred once here.
bool backdrop_stale = true;

void make_backdrop()
{
	ltex_free(backdrop);
	backdrop = nullptr;
	backdrop_stale = false;
	Wallpaper w;
	wallpaper_current(&w);
	if (w.blur[0] && (backdrop = ltex_load(w.blur, 0)))
		return;
	LTex *wall = w.path[0] ? ltex_load(w.path, 0) : nullptr;
	lgfx_begin_snapshot();
	if (wall)
		lgfx_draw_wallpaper(wall, 0, 0);
	else
	{
		const float top[4] = {0.20f, 0.17f, 0.36f, 1}, bottom[4] = {0.10f, 0.12f, 0.22f, 1};
		lgfx_draw_gradient(0, 0, W, H, 0, top, bottom);
	}
	lgfx_end_snapshot();
	ltex_free(wall);
}

// Development aid, as in the launcher: BRAMLEY_DEBUG_KEYS="D D a S" presses Down, Down, A, then saves a
// screenshot to $BRAMLEY_DEBUG_SHOT-<n>.bmp. One step every 0.6 s, from an SDL timer so the script
// keeps going while NextUI's own code draws a page. Unused on the device.
std::atomic<int> shots_wanted;

Uint32 debug_tick(Uint32 interval, void *)
{
	static const char *keys = getenv("BRAMLEY_DEBUG_KEYS");
	static SDL_Keycode held;
	if (held)
	{
		SDL_Event up = {};
		up.type = SDL_KEYUP;
		up.key.keysym.sym = held;
		up.key.keysym.scancode = SDL_GetScancodeFromKey(held);
		SDL_PushEvent(&up);
		held = 0;
		return interval;
	}
	while (*keys == ' ')
		keys++;
	if (!*keys)
		return 0;
	char k = *keys++;
	SDL_Keycode code = k == 'R' ? SDLK_RIGHT : k == 'L' ? SDLK_LEFT : k == 'U' ? SDLK_UP : k == 'D' ? SDLK_DOWN
	                 : k == 'a' ? SDLK_s : k == 'b' ? SDLK_a : 0;
	if (code)
	{
		SDL_Event e = {};
		e.type = SDL_KEYDOWN;
		e.key.keysym.sym = code;
		e.key.keysym.scancode = SDL_GetScancodeFromKey(code);
		SDL_PushEvent(&e);
		held = code;
	}
	else if (k == 'S')
		shots_wanted++;
	return interval;
}

void save_shot()
{
	static int shot;
	int w, h;
	SDL_GL_GetDrawableSize(SDL_GL_GetCurrentWindow(), &w, &h);
	SDL_Surface *img = SDL_CreateRGBSurfaceWithFormat(0, w, h, 32, SDL_PIXELFORMAT_ABGR8888);
	std::vector<Uint8> rgba(w * h * 4);
	glReadPixels(0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, rgba.data());
	for (int y = 0; y < h; y++)
		memcpy((Uint8 *)img->pixels + y * img->pitch, rgba.data() + (h - 1 - y) * w * 4, w * 4);
	char path[512];
	snprintf(path, sizeof(path), "%s-%d.bmp", getenv("BRAMLEY_DEBUG_SHOT") ? getenv("BRAMLEY_DEBUG_SHOT") : "/tmp/bramley-settings", shot++);
	SDL_SaveBMP(img, path);
	SDL_FreeSurface(img);
}

// The pages open below the sections list: chain[0] is the root, and each next list is the page
// opened from the focused item of the one before. titles[i] names the page chain[i + 1].
void open_chain(MenuList *root, std::vector<MenuList *> &chain, std::vector<std::string> &titles)
{
	chain = {root};
	titles.clear();
	while (true)
	{
		MenuList *l = chain.back();
		std::shared_lock<std::shared_mutex> lock(BramleyView::lock(l));
		auto &items = BramleyView::items(l);
		int sel = BramleyView::selected(l);
		if (sel < 0 || sel >= (int)items.size() || !items[sel]->isDeferred() || !items[sel]->getSubMenu())
			break;
		titles.push_back(items[sel]->getName());
		chain.push_back(items[sel]->getSubMenu());
	}
}

void draw_scene(MenuList *root, const std::vector<MenuList *> &chain, const std::vector<std::string> &titles)
{
	if (backdrop)
		lgfx_draw_tex(backdrop, 0, 0, W, H, 0, 1);
	else
		lgfx_draw_snapshot(1);
	lgfx_draw_fill(0, 0, W, H, 0, 0, 0, 0, 0.30f);
	draw_top_bar();

	// Sidebar: the sections, with a small gap between groups.
	int depth = chain.size() - 1;
	AbstractMenuItem *focused_section = nullptr;
	{
		std::shared_lock<std::shared_mutex> lock(BramleyView::lock(root));
		auto &items = BramleyView::items(root);
		int sel = BramleyView::selected(root);
		float x = 40, w = 330, rh = 52, gap = 6, y = 84;
		int last_group = -1;
		for (int i = 0; i < (int)items.size(); i++)
		{
			const Section *s = section_of(items[i]->getName());
			int group = s ? s->group : 9;
			if (last_group >= 0 && group != last_group)
				y += 14;
			last_group = group;
			bool focused = i == sel, open = depth > 0;
			float fx = x, fw = w, fy = y, fh = rh;
			if (focused && !open)
			{
				fx -= 8, fw += 16, fy -= 3, fh += 6;
				lgfx_draw_shadow(fx, fy, fw, fh, 6, 0.32f);
				lgfx_draw_fill(fx, fy, fw, fh, 14, 1, 1, 1, 0.96f);
			}
			else
				lgfx_draw_fill(fx, fy, fw, fh, 14, 1, 1, 1, focused ? 0.22f : 0.07f);
			if (s)
				if (LTex *ic = icon(s->slug, 36))
					lgfx_draw_tex(ic, fx + 14, fy + (fh - 36) * 0.5f, 36, 36, 0, open && !focused ? 0.5f : 1);
			float ty = fy + (fh - 30) * 0.5f;
			if (focused && !open)
				lgfx_set_dim(0.1f);
			text(LFONT_BODY, shown_name(items[i]->getName()), fx + (s ? 64 : 20), ty, open ? (focused ? 0.95f : 0.45f) : (focused ? 1 : 0.92f));
			lgfx_set_dim(1);
			if (focused)
				focused_section = items[i];
			y += rh + gap;
		}
	}

	// Pane: the focused section, previewed, or the open page.
	float px = 410, pw = W - px - 40, py = 84, pb = H - 70;
	lgfx_draw_fill(px, py, pw, pb - py, 22, 0, 0, 0, 0.18f);
	if (depth == 0)
	{
		const Section *s = focused_section ? section_of(focused_section->getName()) : nullptr;
		MenuList *sub = focused_section ? focused_section->getSubMenu() : nullptr;
		if (s && (!strcmp(s->slug, "network") || !strcmp(s->slug, "bluetooth")))
			draw_summary(s, px, py, pw, pb - py);
		else if (focused_section)
		{
			text(LFONT_TITLE, shown_name(focused_section->getName()), px + 28, py + 20, 1);
			if (sub && BramleyView::type(sub) != MenuItemType::Custom)
				draw_list(sub, px, py + 90, pw, pb, false);
		}
		text(LFONT_SMALL, "A or \xE2\x80\xBA  Open        B  Exit", 40, H - 40, 0.6f);
	}
	else
	{
		float ty = py + 20;
		if (depth >= 2)
		{
			text(LFONT_BODY, "\xE2\x80\xB9  " + shown_name(titles[depth - 2]), px + 28, py + 18, 0.5f);
			ty = py + 52;
		}
		text(LFONT_TITLE, shown_name(titles[depth - 1]), px + 28, ty, 1);
		float rows = ty + 70;
		bool network = depth == 1 && titles[0] == "Network";
		if (depth == 1 && (network || titles[0] == "Bluetooth"))
		{
			// a live status line under the title, with a spinner while Wi-Fi is busy
			update_radio();
			float sx = px + 28;
			if (network && radio.wifi_busy)
			{
				draw_spinner(sx + 9, ty + 53, 8);
				sx += 28;
			}
			text(LFONT_SMALL, network ? radio.wifi : radio.bt, sx, ty + 42, 0.6f);
			rows = ty + 84;
		}
		marking_wifi = network;
		draw_list(chain.back(), px, rows, pw, pb, true);
		marking_wifi = false;
		if (network && radio.wifi_seen == 1 && wifi_networks == 0)
		{
			// the list is still empty while the radio looks: say so where the networks will appear
			float y = rows + 2 * 54 + 16;
			draw_spinner(px + 48 + 9, y + 15, 8);
			text(LFONT_BODY, "Looking for networks\xE2\x80\xA6", px + 48 + 30, y, 0.55f);
		}
		if (!network)
			wifi_networks = -1;
		text(LFONT_SMALL, "A  Change        \xE2\x80\xB9 \xE2\x80\xBA  Adjust        B  Back", 40, H - 40, 0.6f);
	}

}

// An overlay message from NextUI ("Enabling WiFi...", "Connecting..."), drawn over the page.
std::string overlay_text;
int overlay_mode;

void draw_overlay()
{
	lgfx_draw_fill(0, 0, W, H, 0, 0, 0, 0, 0.45f);
	LTex *t = ltex_text_wrapped(LFONT_BODY, overlay_text.c_str(), 440, 6, 1);
	float cw = 520, ch = (t ? t->h : 30) + 80 + (overlay_mode ? 50 : 0), cx = (W - cw) * 0.5f, cy = (H - ch) * 0.5f;
	lgfx_draw_shadow(cx, cy, cw, ch, 14, 0.5f);
	lgfx_draw_fill(cx, cy, cw, ch, 24, 0.13f, 0.13f, 0.17f, 0.96f);
	if (t)
	{
		lgfx_draw_tex(t, cx + (cw - t->w) * 0.5f, cy + 40, t->w, t->h, 0, 0.95f);
		ltex_free(t);
	}
	if (overlay_mode)
	{
		const char *hint = overlay_mode == (int)OverlayDismissMode::DismissOnA ? "A  OK" : "B  Close";
		LTex *h = text_tex(LFONT_LABEL, hint);
		if (h)
			lgfx_draw_tex(h, cx + (cw - h->w) * 0.5f, cy + ch - 56, h->w, h->h, 0, 0.7f);
	}
}

// A page NextUI draws itself (the keyboard, the colour picker): its own code draws it in software
// into NextUI's screen surface, and that surface is shown here. NextUI's GPU path (GFX_flip)
// is never used, because its renderer does not expect the GL state these frames leave.
void draw_classic(MenuList *root, SDL_Surface *screen, const SDL_Rect &list, const SDL_Rect &title)
{
	SDL_FillRect(screen, NULL, SDL_MapRGB(screen->format, 0, 0, 0));
	root->draw(screen, list, title);
	if (LTex *t = ltex_from_surface(screen, 0))
	{
		lgfx_draw_tex(t, 0, 0, W, H, 0, 1);
		ltex_free(t);
	}
}

struct Frame
{
	MenuList *root;
	SDL_Surface *screen;
	SDL_Rect list, title;
} last;
SDL_threadID main_thread;

// Draws one whole frame, without presenting it.
void draw_frame(const Frame &f)
{
	std::vector<MenuList *> chain;
	std::vector<std::string> titles;
	open_chain(f.root, chain, titles);
	lgfx_begin_frame();
	if (BramleyView::type(chain.back()) == MenuItemType::Custom)
		draw_classic(f.root, f.screen, f.list, f.title);
	else
		draw_scene(f.root, chain, titles);
	if (MenuList::isOverlayVisible())
		draw_overlay();
}

} // namespace

bool bramley_settings_overlay(const std::string &message, int mode)
{
	overlay_text = message;
	overlay_mode = mode;
	// NextUI shows these right before blocking (switching Wi-Fi can take seconds), so draw now.
	if (!ready || !last.root || SDL_ThreadID() != main_thread)
		return ready; // the next frame shows it
	draw_frame(last);
	lgfx_end_frame();
	return true;
}

int bramley_settings_frame(MenuList *root, SDL_Surface *screen, const SDL_Rect &list, const SDL_Rect &title)
{
	static bool debugging;
	if (!debugging && getenv("BRAMLEY_DEBUG_KEYS"))
	{
		debugging = true;
		SDL_InitSubSystem(SDL_INIT_TIMER);
		SDL_AddTimer(300, debug_tick, nullptr);
	}
	if (!root || !screen)
		return -1;

	static int frames;
	double t0 = frames < 2 ? since_start() : 0;
	if (!ready)
	{
		lgfx_init(W, H);
		double t1 = since_start();
		make_backdrop();
		LOG_info("bramley_settings: first call at %.0f ms, lgfx %.0f ms, backdrop %.0f ms\n", t0, t1 - t0, since_start() - t1);
		main_thread = SDL_ThreadID();
		ready = true;
	}
	if (backdrop_stale)
		make_backdrop(); // the wallpaper was changed in Appearance
	last = {root, screen, list, title};
	spinning = false;
	draw_frame(last);
	if (shots_wanted > 0)
	{
		shots_wanted--;
		save_shot();
	}
	lgfx_end_frame();
	if (frames < 2)
		LOG_info("bramley_settings: frame %d drawn in %.0f ms, at %.0f ms\n", frames, since_start() - t0, since_start());
	frames++;
	// a spinner, or a debug script waiting for screenshots, keeps frames coming
	return debugging || spinning ? 1 : 0;
}

// Bramley's order for the sections, with small gaps between groups (see sections[] above).
// Sections it does not know keep their place after these.
void bramley_order_sections(std::vector<AbstractMenuItem *> &items)
{
	auto rank = [](AbstractMenuItem *item) {
		for (size_t i = 0; i < sizeof(sections) / sizeof(sections[0]); i++)
			if (item->getName() == sections[i].name)
				return (int)i;
		return 1000;
	};
	std::stable_sort(items.begin(), items.end(), [&](AbstractMenuItem *a, AbstractMenuItem *b) { return rank(a) < rank(b); });
}

// Adds Wallpaper at the top of Appearance. Left and right step through the wallpapers, and the
// Settings background changes at once as a preview.
void bramley_add_appearance_items(std::vector<AbstractMenuItem *> &items)
{
	static Wallpaper all[64];
	int n = wallpaper_list(all, 64);
	if (n == 0)
		return;
	std::vector<std::any> values;
	std::vector<std::string> labels;
	for (int i = 0; i < n; i++)
	{
		values.push_back(std::string(all[i].id));
		labels.push_back(all[i].name);
	}
	items.insert(items.begin(), new MenuItem{ListItemType::Generic, "Wallpaper",
		"The picture behind the home screen and Settings. Add your own to a Wallpapers folder on the card.",
		values, labels,
		[]() -> std::any {
			Wallpaper w;
			wallpaper_current(&w);
			return std::string(w.id);
		},
		[](const std::any &v) {
			wallpaper_choose(std::any_cast<std::string>(v).c_str());
			backdrop_stale = true;
		}});
}
