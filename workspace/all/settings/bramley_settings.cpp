// Bramley: Settings drawn with the launcher's GL kit. See bramley_settings.hpp.
#include "bramley_settings.hpp"

#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <unistd.h>
#include <algorithm>
#include <atomic>
#include <map>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <thread>
#include <vector>

#include "menu.hpp"
#include "keyboardprompt.hpp"
#include "wifimenu.hpp"
#include "colorpickermenu.hpp"
#include "bramley_battery.h"
#include "bramley_update.h"
#include "bramley_pages.hpp"

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
	static const KeyboardState &keyboard(KeyboardPrompt *k) { return k->state.keyboard; }
	static ExitCode exit_code(KeyboardPrompt *k) { return k->state.exit_code; }
	static const WIFI_network &network(Wifi::NetworkItem *n) { return n->net; }
	static bool connected(Wifi::NetworkItem *n) { return n->connected; }
	struct Picker
	{
		int r, g, b, a, selected;
		const std::vector<ColorPreset> *presets;
		const std::string *label;
	};
	static Picker picker(ColorPickerMenu *p) { return {p->r, p->g, p->b, p->a, p->selected, &p->presets, &p->label_}; }
	static const KeyboardLayout &layout(KeyboardPrompt *k) { return k->getCurrentLayout(k->state); }
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
bool hosted; // running inside the launcher
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
	{"System", "system", "System", 1},          {"Display", "display", "Display", 1},
	{"Appearance", "appearance", "Appearance", 1}, {"Lights", "lights", "Lights", 1},
	{"In-Game", "in-game", "In-Game", 2},       {"Assignments", "buttons", "Buttons", 2},
	{"FN switch", "fn-switch", "FN Switch", 2}, {"Battery", "battery", "Battery", 3},
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
	int switching = bramley_wifi_switching;
	if (radio.wifi_seen == 0 && on)
		radio.wifi_on_at = now; // just switched on: give it time to find and join a network
	radio.wifi_seen = on;
	radio.wifi_busy = false;
	if (switching)
	{
		radio.wifi = switching == 1 ? "Turning on Wi-Fi\xE2\x80\xA6" : "Turning off Wi-Fi\xE2\x80\xA6";
		radio.wifi_busy = true;
	}
	else if (!on)
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
	if (bramley_bt_switching)
		radio.bt = bramley_bt_switching == 1 ? "Turning on Bluetooth\xE2\x80\xA6" : "Turning off Bluetooth\xE2\x80\xA6";
	else if (!PLAT_bluetoothEnabled())
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
		lgfx_draw_fill(right - w, y, w, h, h * 0.5f, BRAMLEY_ACCENT, 1);
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
// Signal strength as three rising bars, and a padlock for a secured network, drawn right to left
// from right. Returns the x where whatever comes next should end.
float draw_network_marks(const WIFI_network &net, float right, float cy, bool focused)
{
	int bars = net.rssi >= -60 ? 3 : net.rssi >= -70 ? 2 : 1;
	float c = focused ? 0.1f : 1, x = right - 26;
	for (int i = 0; i < 3; i++)
	{
		float bh = 7 + i * 5;
		lgfx_draw_fill(x + i * 9, cy + 9 - bh, 6, bh, 2, c, c, c, i < bars ? 0.95f : 0.25f);
	}
	x -= 12;
	if (net.security != SECURITY_NONE)
	{
		// the shackle as a ring, half hidden behind the body
		lgfx_draw_ring(x - 12, cy - 11, 10, 14, 5, 2.2f, c, c, c, 0.85f);
		lgfx_draw_fill(x - 14, cy - 3, 14, 11, 3, c, c, c, 0.85f);
		x -= 22;
	}
	return x - 6;
}

// Set while the Network page is drawn, so the connected network's row says so.
bool marking_wifi;

void draw_row(AbstractMenuItem *item, float x, float y, float w, float h, bool focused, bool active)
{
	if (bramley_clock_row_disabled(item))
	{
		// set from the internet: shown, but resting; a faint card marks focus
		if (focused)
			lgfx_draw_fill(x, y, w, h, 12, 1, 1, 1, 0.16f);
		active = false, focused = false;
	}
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
	{
		// a Wi-Fi network: signal bars and a lock at the right, as iOS shows them
		if (auto *n = dynamic_cast<Wifi::NetworkItem *>(item))
		{
			lgfx_set_dim(1);
			right = draw_network_marks(BramleyView::network(n), right, y + h * 0.5f, focused);
		}
		bool connected_here = marking_wifi && !radio.ssid.empty() && item->getName() == radio.ssid;
		if (auto *n = dynamic_cast<Wifi::NetworkItem *>(item))
			connected_here = connected_here || BramleyView::connected(n);
		if (connected_here)
		{
			// the connected network: a green dot and "Connected"
			float tw = text_w(LFONT_BODY, "Connected");
			lgfx_set_dim(focused ? 0.45f : 1);
			text(LFONT_BODY, "Connected", right, ty, focused ? 1 : 0.7f, 1);
			lgfx_set_dim(1);
			lgfx_draw_fill(right - tw - 22, y + h * 0.5f - 6, 12, 12, 6, BRAMLEY_ACCENT, 1);
		}
		else if (item->getSubMenu() && !dynamic_cast<Wifi::NetworkItem *>(item))
		{
			if (dynamic_cast<SoftwareUpdateMenu *>(item->getSubMenu()) && update_badge())
			{
				lgfx_set_dim(1);
				lgfx_draw_fill(right - 36, y + h * 0.5f - 6, 12, 12, 6, BRAMLEY_ALERT, 1); // an update waits
			}
			lgfx_set_dim(focused ? 0.35f : 1);
			text(LFONT_BODY, "\xE2\x80\xBA", right, ty, focused ? 1 : 0.4f, 1);
		}
		break;
	}
	}
	lgfx_set_dim(1);
}

// Scroll position per list, so the focused row stays in view.
std::map<MenuList *, int> scroll;

// The Date or Time picker, when one is open: drawn in place of its row, in draw_list.
ClockPicker *open_picker;
const float PICKER_H = 200;

void draw_picker(ClockPicker *p, const std::string &name, float x, float y, float w)
{
	x -= 6, w += 12;
	lgfx_draw_shadow(x, y - 3, w, PICKER_H, 6, 0.32f);
	lgfx_draw_fill(x, y - 3, w, PICKER_H, 14, 1, 1, 1, 0.96f);
	lgfx_set_dim(0.1f);
	text(LFONT_BODY, name, x + 26, y + 9, 1);
	int n = p->columns();
	float cw = 120, cx = x + (w - n * cw) * 0.5f, cy = y + 52;
	for (int c = 0; c < n; c++)
	{
		float mx = cx + c * cw + cw * 0.5f;
		int v = p->value[c], span = p->hi[c] - p->lo[c] + 1;
		int before = p->lo[c] + ((v - p->lo[c] - 1) % span + span) % span, after = p->lo[c] + (v - p->lo[c] + 1) % span;
		lgfx_set_dim(1);
		if (c == p->column)
			lgfx_draw_fill(cx + c * cw + 8, cy + 44, cw - 16, 48, 12, BRAMLEY_ACCENT, 1);
		else
			lgfx_draw_fill(cx + c * cw + 8, cy + 44, cw - 16, 48, 12, 0, 0, 0, 0.07f);
		lgfx_set_dim(c == p->column ? 1 : 0.1f);
		std::string now = p->label(c, v);
		text(LFONT_BODY, now, mx - text_w(LFONT_BODY, now) * 0.5f, cy + 53, 1);
		lgfx_set_dim(0.1f);
		std::string b = p->label(c, before), a = p->label(c, after);
		text(LFONT_BODY, b, mx - text_w(LFONT_BODY, b) * 0.5f, cy + 6, 0.22f);
		text(LFONT_BODY, a, mx - text_w(LFONT_BODY, a) * 0.5f, cy + 100, 0.22f);
	}
	lgfx_set_dim(1);
}

// The strip of lights on the Lights page: each light in its colour, the chosen one outlined (all
// of them while they match).
const float STRIP_H = 92;

void draw_light_strip(LightsMenu *m, float x, float y, float w)
{
	int n = LightsMenu::count();
	float gap = 10, sw = (w - (n - 1) * gap) / n, sh = 76;
	bool on = LightsMenu::on();
	for (int i = 0; i < n; i++)
	{
		bool chosen = m->same || i == m->zone;
		float zx = x + i * (sw + gap);
		lgfx_draw_fill(zx, y, sw, sh, 14, 1, 1, 1, chosen ? 0.20f : 0.06f);
		if (chosen)
			lgfx_draw_ring(zx, y, sw, sh, 14, 2.5f, 1, 1, 1, 0.9f);
		uint32_t c = LightsMenu::zone_rgb(i);
		float r = (c >> 16 & 0xFF) / 255.0f, g = (c >> 8 & 0xFF) / 255.0f, b = (c & 0xFF) / 255.0f;
		// the LED colours are dim values, so lift them to show the hue
		float top = std::max(r, std::max(g, b));
		if (top > 0)
			r /= top, g /= top, b /= top;
		if (!on)
			r = g = b = 0.35f;
		float lw = sw * 0.6f, lx = zx + (sw - lw) * 0.5f;
		lgfx_draw_fill(lx - 4, y + 14, lw + 8, 18, 9, r, g, b, on ? 0.35f : 0.15f);
		lgfx_draw_fill(lx, y + 18, lw, 10, 5, r, g, b, 1);
		std::string name = LightsMenu::zone_name(i);
		text(LFONT_SMALL, name, zx + (sw - text_w(LFONT_SMALL, name)) * 0.5f, y + 42, chosen ? 1 : 0.6f);
	}
}

void draw_list(MenuList *list, float x, float y, float w, float bottom, bool active)
{
	std::shared_lock<std::shared_mutex> lock(BramleyView::lock(list));
	auto &items = BramleyView::items(list);
	int n = items.size(), sel = active ? BramleyView::selected(list) : -1;
	const float rh = 48, gap = 6;
	LightsMenu *lights = dynamic_cast<LightsMenu *>(list);
	ClockPicker *picker = active ? open_picker : nullptr;
	if (marking_wifi)
		wifi_networks = n > 2 ? n - 2 : 0; // the first two rows are the Wi-Fi switches
	// leave room under the rows for the focused row's description, the strip and an open picker
	float room = bottom - y - 64 - (lights ? STRIP_H : 0) - (picker ? PICKER_H - rh : 0);
	int visible = (int)(room / (rh + gap));
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
	for (int i = top; i < n && i < top + visible; i++)
	{
		if (lights && i == 2)
		{
			draw_light_strip(lights, x + 20, yy + 4, w - 40);
			yy += STRIP_H;
		}
		if (picker && i == sel)
		{
			draw_picker(picker, items[i]->getName(), x + 20, yy, w - 40);
			yy += PICKER_H + gap + 2;
			continue;
		}
		draw_row(items[i], x + 20, yy, w - 40, rh, i == sel && !picker, active);
		yy += rh + gap;
	}
	if (top > 0)
		text(LFONT_SMALL, "\xE2\x96\xB2", x + w * 0.5f, y - 26, 0.35f);
	if (top + visible < n)
		text(LFONT_SMALL, "\xE2\x96\xBC", x + w * 0.5f, yy - 6, 0.35f);
	AbstractMenuItem *focused = sel >= 0 && sel < n ? items[sel] : nullptr;
	std::string desc = focused ? focused->getDesc() : "";
	if (focused && bramley_clock_row_disabled(focused))
		desc = "Set from the internet. Turn off Set automatically to change it.";
	std::replace(desc.begin(), desc.end(), '\n', ' '); // NextUI breaks lines by hand; ours wrap
	if (!picker && !desc.empty())
	{
		LTex *d = ltex_text_wrapped(LFONT_SMALL, desc.c_str(), (int)w - 80, 2, 0);
		if (d)
		{
			lgfx_draw_tex(d, x + 40, yy + 20, d->w, d->h, 0, 0.5f);
			ltex_free(d);
		}
	}
}

// Test Buttons: the Brick Hammer drawn to its real layout (TrimUI's product photos), each button
// lit blue while held and green once pressed.
void draw_test_buttons(TestButtonsMenu *t, float px, float pw)
{
	auto state = [&](const char *name) {
		for (auto &b : t->buttons)
			if (!strcmp(b.name, name))
				return b.held ? 1 : b.tested ? 2 : 0;
		return 0;
	};
	auto colour = [](int st, float &r, float &g, float &b, float &a) {
		if (st == 1)
			r = 1, g = 1, b = 1, a = 1; // held: bright
		else if (st == 2)
			r = 0.290f, g = 0.698f, b = 0.471f, a = 0.9f; // tested: the accent
		else
			r = g = b = 1, a = 0.14f;
	};
	auto key = [&](float x, float y, float w, float h, float radius, const char *name, const char *label) {
		float r, g, b, a;
		int st = state(name);
		colour(st, r, g, b, a);
		lgfx_draw_fill(x, y, w, h, radius, r, g, b, a);
		if (label)
			text(LFONT_SMALL, label, x + (w - text_w(LFONT_SMALL, label)) * 0.5f, y + (h - 22) * 0.5f, st ? 1 : 0.7f);
	};
	char count[32];
	snprintf(count, sizeof(count), "%d of %d tested", t->tested(), (int)t->buttons.size());
	text(LFONT_SMALL, count, px + pw - 28, 146, 0.6f, 1);

	float bh = 466, bw = bh * 0.657f, bx = px + (pw - bw) * 0.5f, by = 214;
	auto RX = [&](float v) { return bx + v * bw; };
	auto RY = [&](float v) { return by + v * bh; };
	// shoulders on the top edge, volume on the left side, power on the right
	float tw = bw * 0.2f, th = 22, ty = by - th - 2;
	// L1 and R1 sit outside, L2 and R2 inside (checked on the device)
	key(RX(0.04f), ty, tw, th, 7, "L1", "L1");
	key(RX(0.04f) + tw + 6, ty, tw, th, 7, "L2", "L2");
	key(RX(0.96f) - 2 * tw - 6, ty, tw, th, 7, "R2", "R2");
	key(RX(0.96f) - tw, ty, tw, th, 7, "R1", "R1");
	key(bx - 9, RY(0.06f), 6, bh * 0.07f, 3, "Volume up", nullptr);
	key(bx - 9, RY(0.16f), 6, bh * 0.07f, 3, "Volume down", nullptr);
	lgfx_draw_fill(bx + bw + 3, RY(0.19f), 6, bh * 0.06f, 3, 1, 1, 1, 0.08f); // power: not tested
	// body and screen, with the buttons held now
	lgfx_draw_fill(bx, by, bw, bh, 22, 1, 1, 1, 0.05f);
	lgfx_draw_ring(bx, by, bw, bh, 22, 2, 1, 1, 1, 0.18f);
	lgfx_draw_fill(RX(0.03f), RY(0.02f), bw * 0.94f, bh * 0.48f, 8, 0, 0, 0, 0.40f);
	std::string held = t->held_names();
	if (held.empty())
		held = "Press any button";
	text(LFONT_BODY, held, bx + (bw - text_w(LFONT_BODY, held)) * 0.5f, RY(0.24f) - 14, held == "Press any button" ? 0.4f : 0.9f);
	// F1 and F2
	float fw = bw * 0.11f;
	key(RX(0.424f) - fw * 0.5f, RY(0.579f) - 5, fw, 10, 5, "F1", nullptr);
	key(RX(0.572f) - fw * 0.5f, RY(0.579f) - 5, fw, 10, 5, "F2", nullptr);
	text(LFONT_SMALL, "F1", RX(0.424f) - text_w(LFONT_SMALL, "F1") * 0.5f, RY(0.579f) + 8, 0.45f);
	text(LFONT_SMALL, "F2", RX(0.572f) - text_w(LFONT_SMALL, "F2") * 0.5f, RY(0.579f) + 8, 0.45f);
	// d-pad: the cross, then each arm in its own state
	float cx = RX(0.225f), cy = RY(0.658f), k = bw * 0.09f;
	lgfx_draw_fill(cx - k * 0.5f, cy - k * 1.5f, k, k * 3, 5, 1, 1, 1, 0.14f);
	lgfx_draw_fill(cx - k * 1.5f, cy - k * 0.5f, k * 3, k, 5, 1, 1, 1, 0.14f);
	const struct { const char *n; float dx, dy; } arms[4] = {{"Up", 0, -1}, {"Down", 0, 1}, {"Left", -1, 0}, {"Right", 1, 0}};
	for (auto &a : arms)
		if (state(a.n))
			key(cx + a.dx * k - k * 0.5f, cy + a.dy * k - k * 0.5f, k, k, 5, a.n, nullptr);
	// face buttons
	float fx = RX(0.779f), fy = RY(0.658f), sp = bw * 0.105f, d = bw * 0.108f;
	key(fx - d * 0.5f, fy - sp - d * 0.5f, d, d, d * 0.5f, "X", "X");
	key(fx - sp - d * 0.5f, fy - d * 0.5f, d, d, d * 0.5f, "Y", "Y");
	key(fx + sp - d * 0.5f, fy - d * 0.5f, d, d, d * 0.5f, "A", "A");
	key(fx - d * 0.5f, fy + sp - d * 0.5f, d, d, d * 0.5f, "B", "B");
	// MENU, SELECT, START with the marks printed on them
	float rd = bw * 0.075f, ry = RY(0.905f);
	const char *names[3] = {"MENU", "SELECT", "START"};
	const float xs[3] = {0.361f, 0.499f, 0.637f};
	for (int i = 0; i < 3; i++)
	{
		float c = RX(xs[i]);
		key(c - rd * 0.5f, ry - rd * 0.5f, rd, rd, rd * 0.5f, names[i], nullptr);
		if (i == 0)
			for (int j = 0; j < 3; j++)
			{
				float an = j * 2.094f - 1.571f;
				lgfx_draw_fill(c + cosf(an) * 4.5f - 2.5f, ry + sinf(an) * 4.5f - 2.5f, 5, 5, 2.5f, 1, 1, 1, 0.7f);
			}
		else if (i == 1)
			lgfx_draw_fill(c - 5, ry - 4, 10, 8, 1.5f, 1, 1, 1, 0.7f);
		else
			text(LFONT_SMALL, "\xE2\x96\xB6", c - text_w(LFONT_SMALL, "\xE2\x96\xB6") * 0.5f + 1, ry - 11, 0.7f);
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

// Battery: the level now with the time left, the level over the last hours of use as bars (green
// while charging, red at 20% or less), what the log knows about charges, then the Show row.
int battery_range = 24 * 3600;
BatteryHistory battery;
Uint32 battery_at;
int battery_loaded_range;

std::string hours_minutes(int seconds)
{
	int m = seconds / 60;
	char s[32];
	if (m >= 60)
		snprintf(s, sizeof(s), "%d h %d min", m / 60, m % 60);
	else
		snprintf(s, sizeof(s), "%d min", m);
	return s;
}

void draw_battery(MenuList *menu, float px, float py, float pw, float pb, bool active)
{
	int level = PWR_getBattery();
	bool charging = PWR_isCharging();
	if (!battery_at || SDL_GetTicks() - battery_at > 30000 || battery_loaded_range != battery_range)
	{
		battery_history(&battery, battery_range, level);
		battery_at = SDL_GetTicks() | 1;
		battery_loaded_range = battery_range;
	}
	float x = px + 28;
	text(LFONT_TITLE, "Battery", x, py + 20, 1);
	std::string pct = std::to_string(level) + "%";
	float hw = text_w(LFONT_HERO, pct);
	text(LFONT_HERO, pct, x, py + 70, 1);
	std::string line = charging ? "Charging" : battery.remaining >= 0 ? "About " + hours_minutes(battery.remaining) + " left" : "Time left shows after 20 min";
	text(LFONT_BODY, line, x + hw + 20, py + 78, 0.9f);
	text(LFONT_SMALL, charging ? "Connected to power" : "Not charging", x + hw + 20, py + 110, 0.55f);

	// the chart
	float cx = px + 20, cw = pw - 40, cy = py + 160, ch = 250;
	lgfx_draw_fill(cx, cy, cw, ch, 16, 1, 1, 1, 0.06f);
	text(LFONT_LABEL, "BATTERY LEVEL", cx + 18, cy + 14, 0.55f);
	float gx = cx + 18, gw = cw - 18 - 58, gy = cy + 50, gh = ch - 50 - 40;
	for (int p = 0; p <= 100; p += 50)
	{
		float ly = gy + gh * (1 - p / 100.0f);
		lgfx_draw_fill(gx, ly, gw, 1, 0, 1, 1, 1, 0.12f);
		text(LFONT_SMALL, std::to_string(p) + "%", cx + cw - 16, ly - 11, 0.45f, 1);
	}
	float slot = gw / BATTERY_SLOTS;
	bool any = false;
	for (int i = 0; i < BATTERY_SLOTS; i++)
	{
		const BatterySlot &b = battery.slot[i];
		if (b.level < 0)
			continue;
		any = true;
		float bh = std::max(2.0f, gh * b.level / 100.0f), bx = gx + i * slot + 1.5f;
		if (b.charging)
			lgfx_draw_fill(bx, gy + gh - bh, slot - 3, bh, 2, BRAMLEY_ACCENT, 1);
		else if (b.level <= 20)
			lgfx_draw_fill(bx, gy + gh - bh, slot - 3, bh, 2, BRAMLEY_ALERT, 0.9f);
		else
			lgfx_draw_fill(bx, gy + gh - bh, slot - 3, bh, 2, 1, 1, 1, 0.85f);
	}
	if (!any)
	{
		const std::string none = "No battery history yet";
		text(LFONT_BODY, none, gx + (gw - text_w(LFONT_BODY, none)) * 0.5f, gy + gh * 0.5f - 15, 0.5f);
	}
	int hours = battery_range / 3600;
	for (int i = 0; i <= 4; i++)
	{
		std::string t = i == 4 ? "Now" : std::to_string(hours * (4 - i) / 4) + (i == 0 ? " h ago" : " h");
		float tx = gx + gw * i / 4.0f, tw = text_w(LFONT_SMALL, t);
		text(LFONT_SMALL, t, i == 0 ? tx : i == 4 ? tx - tw : tx - tw * 0.5f, gy + gh + 10, 0.45f);
	}

	// what the log knows about charges
	float sy = cy + ch + 22, sw = (pw - 56) / 3;
	auto stat = [&](float sx, const char *label, const std::string &value) {
		text(LFONT_LABEL, label, sx, sy, 0.55f);
		text(LFONT_BODY, value, sx, sy + 24, 0.95f);
	};
	const std::string dash = "\xE2\x80\x94";
	stat(x, "SINCE CHARGE", battery.since_charge >= 0 ? hours_minutes(battery.since_charge) : dash);
	stat(x + sw, "LAST CHARGED", battery.charged_to >= 0 ? "To " + std::to_string(battery.charged_to) + "%" : dash);
	stat(x + 2 * sw, "LONGEST", battery.longest > 0 ? hours_minutes(battery.longest) : dash);
	draw_list(menu, px, sy + 74, pw, pb, active);
}

// Software Update: the Bramley icon with what is installed, or a card with the new version, what
// is new and the download's progress; then the rows.
std::string megabytes(long long bytes)
{
	char s[32];
	snprintf(s, sizeof(s), "%.0f MB", bytes / 1048576.0);
	return s;
}

std::string when_checked(time_t t)
{
	struct tm then, now;
	time_t n = time(NULL);
	localtime_r(&t, &then);
	localtime_r(&n, &now);
	char s[64];
	if (then.tm_year == now.tm_year && then.tm_yday == now.tm_yday)
		strftime(s, sizeof(s), CFG_getClock24H() ? "Last checked today at %H:%M" : "Last checked today at %I:%M %p", &then);
	else
		strftime(s, sizeof(s), "Last checked %d %b", &then);
	return s;
}

void draw_wrapped(const std::string &s, float x, float y, float w, int lines, float alpha, bool centered)
{
	std::string flat = s;
	std::replace(flat.begin(), flat.end(), '\n', ' '); // the wrapper does not break at newlines
	LTex *t = ltex_text_wrapped(LFONT_SMALL, flat.c_str(), (int)w, lines, centered);
	if (!t)
		return;
	lgfx_draw_tex(t, centered ? x + (w - t->w) * 0.5f : x, y, t->w, t->h, 0, alpha);
	ltex_free(t);
}

void draw_software_update(SoftwareUpdateMenu *menu, float px, float y, float pw, float pb)
{
	UpdateInfo u;
	update_get(&u);
	static LTex *mark;
	if (!mark)
		mark = ltex_load(BRAMLEY_RES_DIR "/brand/icon-176.png", 0);
	if (u.state == UPDATE_CHECKING || u.state == UPDATE_DOWNLOADING || u.state == UPDATE_INSTALLING)
		spinning = true; // the progress keeps moving
	float x = px + 20, w = pw - 40;
	if (u.newer && u.state != UPDATE_CHECKING)
	{
		float ch = 214;
		lgfx_draw_fill(x, y, w, ch, 16, 1, 1, 1, 0.06f);
		if (mark)
			lgfx_draw_tex(mark, x + 20, y + 20, 64, 64, 0, 1);
		text(LFONT_BODY, std::string("Bramley ") + u.version, x + 100, y + 24, 1);
		std::string sub = megabytes(u.size);
		if (u.state == UPDATE_READY)
			sub += "  \xC2\xB7  Downloaded and checked";
		text(LFONT_SMALL, sub, x + 100, y + 58, 0.55f);
		float ny = y + 104;
		if (u.state == UPDATE_DOWNLOADING)
		{
			float frac = u.size > 0 ? std::min(1.0f, (float)u.done / u.size) : 0;
			text(LFONT_SMALL, "Downloading\xE2\x80\xA6  " + megabytes(u.done) + " of " + megabytes(u.size), x + 20, ny + 8, 0.8f);
			lgfx_draw_fill(x + 20, ny + 46, w - 40, 8, 4, 1, 1, 1, 0.14f);
			if (frac > 0)
				lgfx_draw_fill(x + 20, ny + 46, std::max(8.0f, (w - 40) * frac), 8, 4, BRAMLEY_ACCENT, 1);
		}
		else if (u.state == UPDATE_INSTALLING)
		{
			draw_spinner(x + 29, ny + 19, 8);
			text(LFONT_SMALL, "Installing\xE2\x80\xA6 The device restarts in a moment.", x + 50, ny + 8, 0.8f);
		}
		else if (u.state == UPDATE_FAILED && u.error[0])
		{
			lgfx_draw_fill(x + 20, ny + 9, 12, 12, 6, BRAMLEY_ALERT, 1);
			draw_wrapped(u.error, x + 44, ny, w - 64, 3, 0.85f, false);
		}
		else
			draw_wrapped(u.notes, x + 20, ny, w - 40, 4, 0.75f, false);
		y += ch + 18;
	}
	else
	{
		float cx = px + pw * 0.5f;
		if (mark)
			lgfx_draw_tex(mark, cx - 36, y + 4, 72, 72, 0, 1);
		std::string v = std::string("Bramley ") + u.current;
		text(LFONT_BODY, v, cx - text_w(LFONT_BODY, v) * 0.5f, y + 90, 1);
		if (u.state == UPDATE_CHECKING)
		{
			std::string c = "Checking for updates\xE2\x80\xA6";
			float cw = text_w(LFONT_SMALL, c);
			draw_spinner(cx - cw * 0.5f - 6, y + 138, 8);
			text(LFONT_SMALL, c, cx - cw * 0.5f + 14, y + 127, 0.6f);
		}
		else
		{
			std::string line = u.state == UPDATE_FAILED && u.error[0] ? u.error
			                 : u.state == UPDATE_CURRENT        ? "Bramley is up to date"
			                                                    : "Not checked for updates yet";
			draw_wrapped(line, px + 40, y + 126, pw - 80, 2, 0.6f, true);
		}
		if (u.checked && u.state != UPDATE_CHECKING)
		{
			std::string c = when_checked(u.checked);
			text(LFONT_SMALL, c, cx - text_w(LFONT_SMALL, c) * 0.5f, y + 182, 0.4f);
		}
		y += 228;
	}
	draw_list(menu, px, y, pw, pb, true);
}

// About's title: the Bramley icon and name in place of the word "About" (docs/brand).
void draw_about_title(float x, float y)
{
	static LTex *mark;
	if (!mark)
		mark = ltex_load(BRAMLEY_RES_DIR "/brand/icon-64.png", 0);
	if (mark)
		lgfx_draw_tex(mark, x, y - 6, 56, 56, 0, 1);
	text(LFONT_TITLE, "Bramley", x + (mark ? 72 : 0), y, 1);
}

// The app name in the top left; the launcher's Files sets its own while it borrows the keyboard.
const char *top_label = "Settings";

void draw_top_bar()
{
	text(LFONT_LABEL, top_label, 48, 30, 0.55f);
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

// True while a Settings frame is drawn into the blur snapshot for Now Playing; the snapshot
// texture cannot be read while it is being drawn.
bool capturing;

void draw_backdrop()
{
	if (backdrop)
		lgfx_draw_tex(backdrop, 0, 0, W, H, 0, 1);
	else if (!capturing)
		lgfx_draw_snapshot(1);
	else
		lgfx_draw_fill(0, 0, W, H, 0, 0.08f, 0.08f, 0.12f, 1);
}

// Development aid, as in the launcher: BRAMLEY_DEBUG_KEYS="D D a S" presses Down, Down, A, then saves a
// screenshot to $BRAMLEY_DEBUG_SHOT-<n>.bmp. One step every 0.6 s (a press is held 150 ms), from an SDL timer so the script
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
		return 450; // then a pause before the next step
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
	return 150; // a press is held below the repeat delay
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
	draw_backdrop();
	lgfx_draw_fill(0, 0, W, H, 0, 0, 0, 0, 0.30f);
	draw_top_bar();

	// Sidebar: the sections, with a small gap between groups.
	int depth = chain.size() - 1;
	AbstractMenuItem *focused_section = nullptr;
	{
		std::shared_lock<std::shared_mutex> lock(BramleyView::lock(root));
		auto &items = BramleyView::items(root);
		int sel = BramleyView::selected(root);
		float x = 40, w = 330, rh = 46, gap = 5, y = 84;
		int last_group = -1;
		for (int i = 0; i < (int)items.size(); i++)
		{
			const Section *s = section_of(items[i]->getName());
			int group = s ? s->group : 9;
			if (last_group >= 0 && group != last_group)
				y += 12;
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
					lgfx_draw_tex(ic, fx + 14, fy + (fh - 32) * 0.5f, 32, 32, 0, open && !focused ? 0.5f : 1);
			float ty = fy + (fh - 30) * 0.5f;
			if (focused && !open)
				lgfx_set_dim(0.1f);
			text(LFONT_BODY, shown_name(items[i]->getName()), fx + (s ? 60 : 20), ty, open ? (focused ? 0.95f : 0.45f) : (focused ? 1 : 0.92f));
			if (s && !strcmp(s->slug, "system") && update_badge())
			{
				lgfx_set_dim(1);
				lgfx_draw_fill(fx + fw - 30, fy + fh * 0.5f - 6, 12, 12, 6, BRAMLEY_ALERT, 1); // an update waits
			}
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
		else if (s && !strcmp(s->slug, "battery") && sub)
			draw_battery(sub, px, py, pw, pb, false);
		else if (focused_section)
		{
			if (s && !strcmp(s->slug, "about"))
				draw_about_title(px + 28, py + 20);
			else
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
		if (depth == 1 && titles[0] == "About")
			draw_about_title(px + 28, ty);
		else
			text(LFONT_TITLE, shown_name(titles[depth - 1]), px + 28, ty, 1);
		float rows = ty + 70;
		bool network = depth == 1 && titles[0] == "Network";
		if (depth == 1 && (network || titles[0] == "Bluetooth"))
		{
			// a live status line under the title, with a spinner while Wi-Fi is busy
			update_radio();
			float sx = px + 28;
			if ((network && radio.wifi_busy) || (!network && bramley_bt_switching))
			{
				draw_spinner(sx + 9, ty + 53, 8);
				sx += 28;
			}
			text(LFONT_SMALL, network ? radio.wifi : radio.bt, sx, ty + 42, 0.6f);
			rows = ty + 84;
		}
		marking_wifi = network;
		auto *tester = dynamic_cast<TestButtonsMenu *>(chain.back());
		if (depth == 1 && titles[0] == "Battery")
			draw_battery(chain.back(), px, py, pw, pb, true);
		else if (auto *update = dynamic_cast<SoftwareUpdateMenu *>(chain.back()))
			draw_software_update(update, px, rows - 10, pw, pb);
		else if (tester)
			draw_test_buttons(tester, px, pw);
		else
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
		const char *hint = "A  Change        \xE2\x80\xB9 \xE2\x80\xBA  Adjust        B  Back";
		if (tester)
			hint = "Hold SELECT and START to leave";
		else if (open_picker)
			hint = "\xE2\x80\xB9 \xE2\x80\xBA  Column        \xE2\x86\x91 \xE2\x86\x93  Change        A  Set        B  Cancel";
		else if (dynamic_cast<TimezoneMenu *>(chain.back()))
			hint = "A  Choose        L1 R1  Region        B  Back";
		else if (dynamic_cast<LightsMenu *>(chain.back()) && !static_cast<LightsMenu *>(chain.back())->same)
			hint = "L1 R1  Light        A  Change        \xE2\x80\xB9 \xE2\x80\xBA  Adjust        B  Back";
		text(LFONT_SMALL, hint, 40, H - 40, 0.6f);
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

// The on-screen keyboard, in the style of tvOS: the text in a wide field, keys as rounded tiles
// with the focused key lifted on white, and Done on its own blue key. NextUI's KeyboardPrompt
// still holds the text and handles the buttons.
void draw_keyboard(KeyboardPrompt *kp)
{
	const KeyboardState &k = BramleyView::keyboard(kp);
	const KeyboardLayout &layout = BramleyView::layout(kp);
	lgfx_draw_fill(0, 0, W, H, 0, 0, 0, 0, 0.25f);
	draw_top_bar();

	if (!k.title.empty())
	{
		LTex *t = text_tex(LFONT_TITLE, k.title);
		if (t)
			lgfx_draw_tex(t, (W - t->w) * 0.5f, 72, t->w, t->h, 0, 1);
	}

	// the text field, with a caret at the cursor (L1 and R1 move it)
	float fx = 112, fw = W - 224, fy = 130, fh = 64;
	lgfx_draw_fill(fx, fy, fw, fh, fh * 0.5f, 1, 1, 1, 0.12f);
	lgfx_draw_ring(fx, fy, fw, fh, fh * 0.5f, 1.5f, 1, 1, 1, 0.25f);
	float tx = fx + 28, ty = fy + (fh - 30) * 0.5f, caret = tx;
	if (!k.current_text.empty())
	{
		LTex *t = ltex_text(LFONT_BODY, k.current_text.c_str(), 0);
		size_t c = k.cursor < k.current_text.size() ? k.cursor : k.current_text.size();
		std::string before = k.current_text.substr(0, c);
		LTex *b = before.empty() ? nullptr : ltex_text(LFONT_BODY, before.c_str(), 0);
		float bw = b ? b->w : 0;
		if (t)
		{
			// keep the caret in view: scroll so it sits inside the field
			float avail = fw - 70, shift = 0;
			if (t->w > avail)
				shift = std::min(t->w - avail, std::max(0.0f, bw - avail * 0.8f));
			// draw only what fits in the field
			float vis = std::min(avail, t->w - shift);
			lgfx_draw_tex_part(t, shift, 0, vis, t->h, tx, ty, vis, t->h, 1);
			caret = tx + bw - shift;
			ltex_free(t);
		}
		ltex_free(b);
	}
	else
		text(LFONT_BODY, "Type with the keys below", tx, ty, 0.35f);
	if ((SDL_GetTicks() / 530) % 2 == 0)
		lgfx_draw_fill(caret + 1, fy + 16, 2.5f, fh - 32, 1, BRAMLEY_ACCENT, 1);
	spinning = true; // keeps the caret blinking

	// the keys, each row centred
	const float ks = 60, gap = 8, top = 248;
	int rows = (int)layout.size();
	for (int r = 0; r < rows; r++)
	{
		const auto &row = layout[r];
		std::vector<std::pair<int, float>> keys; // column and width
		for (int c = 0; c < (int)row.size(); c++)
		{
			const std::string &key = row[c];
			if (key.empty() || key[0] == 0)
				continue;
			float w = key == "space" ? 380 : (key == "shift" || key == "enter") ? 132 : ks;
			keys.push_back({c, w});
		}
		float total = -gap;
		for (auto &kw : keys)
			total += kw.second + gap;
		float x = (W - total) * 0.5f, y = top + r * (ks + gap);
		for (auto &kw : keys)
		{
			const std::string &key = row[kw.first];
			float w = kw.second, kx = x, ky = y, kh = ks;
			bool focused = r == k.row && kw.first == k.col;
			bool done = key == "enter";
			if (focused)
			{
				kx -= 4, ky -= 4, w += 8, kh += 8;
				lgfx_draw_shadow(kx, ky, w, kh, 8, 0.4f);
				lgfx_draw_fill(kx, ky, w, kh, 14, 1, 1, 1, 1);
				lgfx_set_dim(0.1f);
			}
			else if (done)
				lgfx_draw_fill(kx, ky, w, kh, 12, BRAMLEY_ACCENT, 1);
			else
				lgfx_draw_fill(kx, ky, w, kh, 12, 1, 1, 1, key == "shift" || key == "space" ? 0.10f : 0.14f);
			std::string label = key;
			if (key == "shift")
				label = k.layout == 0 ? "ABC" : k.layout == 1 ? "#+=" : "abc";
			else if (key == "space")
				label = "space";
			else if (key == "enter")
				label = "Done";
			LTex *t = text_tex(key.size() > 1 ? LFONT_LABEL : LFONT_BODY, label);
			if (t)
				lgfx_draw_tex(t, kx + (w - t->w) * 0.5f, ky + (kh - t->h) * 0.5f, t->w, t->h, 0, focused ? 1 : 0.95f);
			lgfx_set_dim(1);
			x += kw.second + gap;
		}
	}
	text(LFONT_SMALL, "A  Type        B  Delete        L1 R1  Move        SELECT  Shift        X  Done        Y  Cancel", W * 0.5f - 375, H - 44, 0.6f);
}

// A horizontal colour ramp from a to b, drawn as thin bands (the kit's gradients are vertical).
void draw_ramp(float x, float y, float w, float h, const float a[3], const float b[3])
{
	// round caps in the end colours, square bands between them
	lgfx_draw_fill(x, y, h, h, h * 0.5f, a[0], a[1], a[2], 1);
	lgfx_draw_fill(x + w - h, y, h, h, h * 0.5f, b[0], b[1], b[2], 1);
	const int n = 32;
	float bx = x + h * 0.5f, bw = (w - h) / n;
	for (int i = 0; i < n; i++)
	{
		float t = (i + 0.5f) / n;
		lgfx_draw_fill(bx + i * bw, y, bw + 0.5f, h, 0, a[0] + (b[0] - a[0]) * t, a[1] + (b[1] - a[1]) * t,
		               a[2] + (b[2] - a[2]) * t, 1);
	}
}

// The colour picker: the colour large on the left with its hex value, the red, green, blue and
// alpha sliders on the right with ramps showing where each would take the colour, and the other
// theme colours below as rows to copy from. NextUI's ColorPickerMenu still holds the colour and
// handles the buttons.
void draw_color_picker(ColorPickerMenu *cp)
{
	BramleyView::Picker p = BramleyView::picker(cp);
	lgfx_draw_fill(0, 0, W, H, 0, 0, 0, 0, 0.25f);
	draw_top_bar();
	LTex *t = text_tex(LFONT_TITLE, *p.label);
	if (t)
		lgfx_draw_tex(t, (W - t->w) * 0.5f, 64, t->w, t->h, 0, 1);

	// the colour, over a checkerboard so its transparency shows
	float sx = 72, sy = 140, ss = 240;
	lgfx_draw_shadow(sx, sy, ss, ss, 10, 0.45f);
	for (int i = 0; i < 6; i++)
		for (int j = 0; j < 6; j++)
		{
			float c = (i + j) % 2 ? 0.75f : 0.95f;
			lgfx_draw_fill(sx + i * 40, sy + j * 40, 40, 40, 0, c, c, c, 1);
		}
	lgfx_draw_fill(sx, sy, ss, ss, 0, p.r / 255.0f, p.g / 255.0f, p.b / 255.0f, p.a / 255.0f);
	lgfx_draw_ring(sx, sy, ss, ss, 0, 2, 1, 1, 1, 0.35f);
	char hex[16];
	snprintf(hex, sizeof(hex), "#%02X%02X%02X%02X", p.r, p.g, p.b, p.a);
	t = text_tex(LFONT_LABEL, hex);
	if (t)
		lgfx_draw_tex(t, sx + (ss - t->w) * 0.5f, sy + ss + 20, t->w, t->h, 0, 0.8f);

	// the sliders
	float px = 360, pw = W - px - 48, rh = 56, gap = 8, y = 136;
	const char *names[4] = {"R", "G", "B", "A"};
	int vals[4] = {p.r, p.g, p.b, p.a};
	for (int i = 0; i < 4; i++, y += rh + gap)
	{
		bool f = i == p.selected;
		float x = px, w = pw, yy = y, hh = rh;
		if (f)
		{
			x -= 6, w += 12, yy -= 3, hh += 6;
			lgfx_draw_shadow(x, yy, w, hh, 6, 0.32f);
			lgfx_draw_fill(x, yy, w, hh, 14, 1, 1, 1, 0.96f);
		}
		else
			lgfx_draw_fill(x, yy, w, hh, 14, 1, 1, 1, 0.07f);
		float ty = yy + (hh - 30) * 0.5f;
		if (f)
			lgfx_set_dim(0.1f);
		text(LFONT_LABEL, names[i], x + 22, ty + 2, 1);
		char v[8];
		snprintf(v, sizeof(v), "%02X", vals[i]);
		text(LFONT_LABEL, v, x + w - 22, ty + 2, f ? 1 : 0.6f, 1);
		lgfx_set_dim(1);
		// the ramp runs this channel from 0 to 255 with the others as they are
		float lo[3] = {p.r / 255.0f, p.g / 255.0f, p.b / 255.0f}, hi[3] = {lo[0], lo[1], lo[2]};
		if (i < 3)
			lo[i] = 0, hi[i] = 1;
		else
			lo[0] = lo[1] = lo[2] = 0.15f, hi[0] = hi[1] = hi[2] = 0.9f;
		float tx = x + 60, tw = w - 140, cy = yy + hh * 0.5f;
		draw_ramp(tx, cy - 7, tw, 14, lo, hi);
		float kx = tx + tw * vals[i] / 255.0f;
		lgfx_draw_fill(kx - 13, cy - 13, 26, 26, 13, 1, 1, 1, 1);
		lgfx_draw_ring(kx - 13, cy - 13, 26, 26, 13, 1.5f, 0, 0, 0, 0.3f);
	}

	// the other theme colors, to copy from with X
	const auto &presets = *p.presets;
	if (!presets.empty())
	{
		text(LFONT_SMALL, "Other colors", px + 4, y + 6, 0.5f);
		y += 40;
		const float prh = 46, pgap = 6;
		int visible = (int)((H - 80 - y) / (prh + pgap));
		if (visible < 1)
			visible = 1;
		int sel = p.selected - 4, top = 0;
		if (sel >= visible)
			top = sel - visible + 1;
		for (int k = top; k < (int)presets.size() && k < top + visible; k++, y += prh + pgap)
		{
			bool f = k == sel;
			float x = px, w = pw, yy = y, hh = prh;
			if (f)
			{
				x -= 6, w += 12, yy -= 3, hh += 6;
				lgfx_draw_shadow(x, yy, w, hh, 6, 0.32f);
				lgfx_draw_fill(x, yy, w, hh, 12, 1, 1, 1, 0.96f);
			}
			else
				lgfx_draw_fill(x, yy, w, hh, 12, 1, 1, 1, 0.06f);
			uint32_t c = presets[k].color;
			float cr = (c >> 24 & 0xFF) / 255.0f, cg = (c >> 16 & 0xFF) / 255.0f, cb = (c >> 8 & 0xFF) / 255.0f;
			lgfx_draw_fill(x + 18, yy + (hh - 26) * 0.5f, 26, 26, 13, cr, cg, cb, 1);
			lgfx_draw_ring(x + 18, yy + (hh - 26) * 0.5f, 26, 26, 13, 1.5f, f ? 0 : 1, f ? 0 : 1, f ? 0 : 1, 0.3f);
			float ty = yy + (hh - 30) * 0.5f;
			if (f)
				lgfx_set_dim(0.1f);
			text(LFONT_BODY, presets[k].label, x + 60, ty, 1);
			lgfx_set_dim(f ? 0.45f : 1);
			text(LFONT_BODY, f ? "X  Use" : "", x + w - 20, ty, f ? 1 : 0.5f, 1);
			lgfx_set_dim(1);
		}
	}
	text(LFONT_SMALL, "\xE2\x80\xB9 \xE2\x80\xBA  Adjust        L1 R1  Faster        X  Use color        A  Done        B  Cancel",
	     W * 0.5f - 340, H - 44, 0.6f);
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
void draw_frame(const Frame &f, bool begin = true)
{
	std::vector<MenuList *> chain;
	std::vector<std::string> titles;
	open_chain(f.root, chain, titles);
	if (begin)
		lgfx_begin_frame();
	KeyboardPrompt *kp = dynamic_cast<KeyboardPrompt *>(chain.back());
	if (kp)
	{
		draw_backdrop();
		draw_keyboard(kp);
	}
	else if (auto *cp = dynamic_cast<ColorPickerMenu *>(chain.back()))
	{
		draw_backdrop();
		draw_color_picker(cp);
	}
	else if (auto *picker = dynamic_cast<ClockPicker *>(chain.back()))
	{
		// the picker opens in place, so the page under it is drawn with it
		chain.pop_back();
		titles.pop_back();
		open_picker = picker;
		draw_scene(f.root, chain, titles);
		open_picker = nullptr;
	}
	else if (dynamic_cast<TestButtonsMenu *>(chain.back()))
		draw_scene(f.root, chain, titles);
	else if (BramleyView::type(chain.back()) == MenuItemType::Custom)
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
		SDL_AddTimer(150, debug_tick, nullptr); // a press is held 150 ms, below the repeat delay
	}
	if (!root || !screen)
		return -1;

	static int frames;
	double t0 = frames < 2 ? since_start() : 0;
	if (!ready)
	{
		if (!hosted)
			lgfx_init(W, H); // inside the launcher, the launcher's kit is already running
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

std::atomic<int> bramley_wifi_switching{0}, bramley_bt_switching{0};

void bramley_switch_radio(std::atomic<int> &flag, bool on, std::function<void()> work)
{
	int idle = 0;
	if (!flag.compare_exchange_strong(idle, on ? 1 : 2))
		return;
	radio.at = 0; // show the new state at the next frame
	std::thread([&flag, work]() {
		work();
		flag = 0;
		radio.at = 0;
	}).detach();
}

MenuList *bramley_battery_menu()
{
	std::vector<AbstractMenuItem *> items = {new MenuItem{ListItemType::Generic, "Show",
		"Counts only the time the Brick was on.",
		{6 * 3600, 12 * 3600, 24 * 3600, 48 * 3600}, {"Last 6 hours", "Last 12 hours", "Last 24 hours", "Last 2 days"},
		[]() -> std::any { return battery_range; },
		[](const std::any &v) { battery_range = std::any_cast<int>(v); }}};
	return new MenuList(MenuItemType::Fixed, "Battery", items);
}

MenuList *bramley_settings_root()
{
	return last.root;
}

bool bramley_is_level(const std::vector<std::string> &labels)
{
	return is_level(labels);
}

void (*bramley_host_tick_fn)(void);

void bramley_host_tick()
{
	if (bramley_host_tick_fn)
		bramley_host_tick_fn();
}

void bramley_delete_menu_tree(MenuList *root, const std::vector<MenuList *> &keep)
{
	// Collect every page first: deleting a list deletes its items, which hold the links.
	std::vector<MenuList *> all = {root};
	for (size_t i = 0; i < all.size(); i++)
	{
		std::shared_lock<std::shared_mutex> lock(BramleyView::lock(all[i]));
		for (AbstractMenuItem *item : BramleyView::items(all[i]))
		{
			MenuList *sub = item->getSubMenu();
			if (sub && std::find(all.begin(), all.end(), sub) == all.end() &&
			    std::find(keep.begin(), keep.end(), sub) == keep.end())
				all.push_back(sub);
		}
	}
	for (MenuList *l : all)
		delete l;
}

int settings_run(SDL_Surface *host_screen);

std::string reopen_section; // the section to show when Settings reopens after Now Playing

void bramley_restore_section(MenuList *root)
{
	if (!reopen_section.empty())
		root->selectByName(reopen_section);
	reopen_section.clear();
}

void settings_request_quit();

extern "C" void bramley_close_settings(int keep_place)
{
	if (keep_place && last.root)
	{
		reopen_section = last.root->getSelectedItemName();
		// leave this Settings frame in the blur snapshot, so Now Playing opens over Settings
		lgfx_begin_snapshot();
		capturing = true;
		draw_frame(last, false);
		capturing = false;
		lgfx_end_snapshot();
	}
	settings_request_quit();
}

extern "C" int bramley_open_settings(SDL_Surface *screen, void (*tick)(void))
{
	hosted = true;
	bramley_host_tick_fn = tick;
	backdrop_stale = true; // the wallpaper may have changed since last time
	int code = settings_run(screen);
	// The pages are gone: forget everything that pointed into them.
	last = {};
	scroll.clear();
	radio.at = 0;
	bramley_host_tick_fn = nullptr;
	return code;
}

extern "C" int bramley_settings_typing(void)
{
	if (!last.root)
		return 0;
	std::vector<MenuList *> chain;
	std::vector<std::string> titles;
	open_chain(last.root, chain, titles);
	return BramleyView::type(chain.back()) == MenuItemType::Custom; // SELECT belongs to the page
}

// For the launcher (C): the Bramley keyboard on its own, for a name. Runs until A on Done or B,
// drawing backdrop() under the keyboard each frame. Returns 1 with the text in out, 0 if cancelled.
extern "C" int bramley_text_input(const char *app, const char *title, const char *initial, char *out, size_t size,
                                  void (*backdrop)(void), void (*after)(void))
{
	KeyboardPrompt kp(title);
	kp.setInitialText(initial ? initial : "");
	const char *was = top_label;
	top_label = app;
	int dirty = 1, done = 0, ok = 0;
	while (!done)
	{
		GFX_startFrame();
		PAD_poll();
		int quit = 0;
		InputReactionHint r = kp.handleInput(dirty, quit);
		if (quit || r == InputReactionHint::Exit)
		{
			const KeyboardState &k = BramleyView::keyboard(&kp);
			ok = BramleyView::exit_code(&kp) == ExitCode::Success;
			if (ok)
				snprintf(out, size, "%s", k.final_text.c_str());
			done = 1;
		}
		PWR_update(&dirty, NULL, NULL, NULL);
		static const int scripted = getenv("LAUNCHER_DEBUG_KEYS") != NULL; // development: keep frames coming
		if (!dirty && !scripted)
		{
			SDL_Delay(16);
			continue;
		}
		lgfx_begin_frame();
		if (backdrop)
			backdrop();
		draw_keyboard(&kp);
		if (after)
			after();
		lgfx_end_frame();
		dirty = 0;
	}
	top_label = was;
	return ok;
}
