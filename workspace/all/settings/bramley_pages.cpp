// Bramley: Date & Time, Lights and Test Buttons. See bramley_pages.hpp.
#include "bramley_pages.hpp"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <unistd.h>

#include "colorpickermenu.hpp"

extern "C"
{
#include "bramley.h"
#include "utils.h"
}

///////////////////////////////////////////////////////////
// Date & Time

static const char *months[12] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};

static int days_in(int month, int year)
{
    static const int d[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    bool leap = (year % 4 == 0 && year % 100 != 0) || year % 400 == 0;
    return month == 2 && leap ? 29 : d[month - 1];
}

ClockPicker::ClockPicker(bool time) : MenuList(MenuItemType::Custom, time ? "Time" : "Date", {}), time(time) {}

void ClockPicker::load()
{
    time_t now = ::time(nullptr);
    struct tm tm;
    localtime_r(&now, &tm);
    column = 0;
    if (!time)
    {
        value = {tm.tm_mday, tm.tm_mon + 1, tm.tm_year + 1900};
        lo = {1, 1, 2024};
        hi = {days_in(tm.tm_mon + 1, tm.tm_year + 1900), 12, 2050};
    }
    else if (CFG_getClock24H())
    {
        value = {tm.tm_hour, tm.tm_min};
        lo = {0, 0};
        hi = {23, 59};
    }
    else
    {
        int h = tm.tm_hour % 12;
        value = {h ? h : 12, tm.tm_min, tm.tm_hour >= 12};
        lo = {1, 0, 0};
        hi = {12, 59, 1};
    }
}

std::string ClockPicker::label(int col, int v) const
{
    char s[16];
    if (!time && col == 1)
        return months[v - 1];
    if (time && col == 2)
        return v ? "PM" : "AM";
    snprintf(s, sizeof(s), time && col == 1 ? "%02d" : (time && CFG_getClock24H() ? "%02d" : "%d"), v);
    return s;
}

InputReactionHint ClockPicker::handleInput(int &dirty, int &quit)
{
    auto step = [&](int d) {
        int n = hi[column] - lo[column] + 1;
        value[column] = lo[column] + ((value[column] - lo[column] + d) % n + n) % n;
        if (!time)
        {
            // a shorter month or a non-leap year pulls the day back
            hi[0] = days_in(value[1], value[2]);
            if (value[0] > hi[0])
                value[0] = hi[0];
        }
        dirty = 1;
    };
    if (PAD_justRepeated(BTN_UP))
        step(-1);
    else if (PAD_justRepeated(BTN_DOWN))
        step(1);
    else if (PAD_justRepeated(BTN_LEFT) && column > 0)
        column--, dirty = 1;
    else if (PAD_justRepeated(BTN_RIGHT) && column < columns() - 1)
        column++, dirty = 1;
    else if (PAD_justPressed(BTN_B))
        quit = 1, dirty = 1;
    else if (PAD_justPressed(BTN_A))
    {
        time_t now = ::time(nullptr);
        struct tm tm;
        localtime_r(&now, &tm);
        int y = tm.tm_year + 1900, mo = tm.tm_mon + 1, d = tm.tm_mday, h = tm.tm_hour, mi = tm.tm_min;
        if (!time)
            d = value[0], mo = value[1], y = value[2];
        else
        {
            h = value[0], mi = value[1];
            if (columns() == 3)
                h = h % 12 + (value[2] ? 12 : 0);
        }
        if (!strcmp(PLATFORM, "desktop")) // never set the Mac's clock
            LOG_info("bramley: would set the clock to %d-%02d-%02d %02d:%02d\n", y, mo, d, h, mi);
        else
            PLAT_setDateTime(y, mo, d, h, mi, 0);
        quit = 1, dirty = 1;
    }
    return NoOp;
}

namespace
{
    // The Date or Time row: shows the clock, opens its picker, and rests while the clock is automatic.
    class ClockItem : public MenuItem
    {
    public:
        ClockItem(bool time, ClockPicker *picker)
            : MenuItem(ListItemType::Generic, time ? "Time" : "Date",
                       time ? "Press A to change the time." : "Press A to change the date.",
                       {std::any(0)}, {std::string("")}, nullptr, nullptr, nullptr,
                       [picker](AbstractMenuItem &item) -> InputReactionHint {
                           if (TIME_getNetworkTimeSync())
                               return NoOp;
                           picker->load();
                           return DeferToSubmenu(item);
                       },
                       picker),
              time(time) {}
        const std::string getLabel() const override
        {
            time_t now = ::time(nullptr);
            struct tm tm;
            localtime_r(&now, &tm);
            char s[32];
            if (!time)
                snprintf(s, sizeof(s), "%d %s %d", tm.tm_mday, months[tm.tm_mon], tm.tm_year + 1900);
            else if (CFG_getClock24H())
                strftime(s, sizeof(s), "%H:%M", &tm);
            else
            {
                int h = tm.tm_hour % 12;
                snprintf(s, sizeof(s), "%d:%02d %s", h ? h : 12, tm.tm_min, tm.tm_hour >= 12 ? "PM" : "AM");
            }
            return s;
        }
        bool time;
    };
}

bool bramley_clock_row_disabled(AbstractMenuItem *item)
{
    return dynamic_cast<ClockItem *>(item) && TIME_getNetworkTimeSync();
}

MenuList *bramley_date_time_menu(std::vector<AbstractMenuItem *> clock_items)
{
    clock_items.push_back(new ClockItem(false, new ClockPicker(false)));
    clock_items.push_back(new ClockItem(true, new ClockPicker(true)));
    return new MenuList(MenuItemType::Fixed, "Date & Time", clock_items);
}

///////////////////////////////////////////////////////////
// Time zone

// "America/Argentina/Buenos_Aires" -> "Buenos Aires"; region "America"
static std::string city_of(const std::string &zone)
{
    std::string c = zone.substr(zone.rfind('/') + 1);
    std::replace(c.begin(), c.end(), '_', ' ');
    return c;
}

static std::string region_of(const std::string &zone)
{
    size_t slash = zone.find('/');
    return slash == std::string::npos ? "Other" : zone.substr(0, slash);
}

// Every zone NextUI knows, sorted by region, then city.
static void list_zones(std::vector<std::string> &zones, std::vector<std::string> &regions)
{
    static char list[MAX_TIMEZONES][MAX_TZ_LENGTH];
    int n = 0;
    TIME_getTimezones(list, &n);
    for (int i = 0; i < n; i++)
        zones.push_back(list[i]);
    std::stable_sort(zones.begin(), zones.end(), [](const std::string &a, const std::string &b) {
        std::string ra = region_of(a), rb = region_of(b);
        return ra != rb ? ra < rb : city_of(a) < city_of(b);
    });
    for (auto &z : zones)
        regions.push_back(region_of(z));
}

namespace
{
    // a city row: shows its region, and a tick on the current zone
    class ZoneRow : public MenuItem
    {
    public:
        using MenuItem::MenuItem;
        std::string zone, region;
        const std::string getLabel() const override
        {
            return zone == TIME_getCurrentTimezone() ? region + "   \xE2\x9C\x93" : region;
        }
    };
}

TimezoneMenu::TimezoneMenu() : MenuList(MenuItemType::Fixed, "Time Zone", {})
{
    list_zones(zones, regions);
    for (size_t i = 0; i < zones.size(); i++)
    {
        std::string zone = zones[i];
        auto *row = new ZoneRow(ListItemType::Generic, city_of(zone), "", std::vector<std::any>{std::any(0)}, std::vector<std::string>{""},
                                nullptr, nullptr, nullptr, [zone](AbstractMenuItem &) -> InputReactionHint {
                                    TIME_setCurrentTimezone(zone.c_str());
                                    return Exit;
                                });
        row->zone = zone;
        row->region = regions[i];
        items.push_back(row);
    }
    scope.count = (int)items.size();
}

void TimezoneMenu::select_current()
{
    std::string now = TIME_getCurrentTimezone();
    for (size_t i = 0; i < zones.size(); i++)
        if (zones[i] == now)
            scope.selected = (int)i;
}

InputReactionHint TimezoneMenu::handleInput(int &dirty, int &quit)
{
    if (PAD_justRepeated(BTN_L1) || PAD_justRepeated(BTN_R1))
    {
        int i = scope.selected, n = (int)regions.size();
        if (PAD_justRepeated(BTN_R1))
        {
            while (i < n - 1 && regions[i] == regions[scope.selected])
                i++;
        }
        else
        {
            // to the start of this region, or of the one before when already there
            if (i > 0 && regions[i - 1] == regions[i])
                while (i > 0 && regions[i - 1] == regions[scope.selected])
                    i--;
            else if (i > 0)
            {
                i--;
                while (i > 0 && regions[i - 1] == regions[i])
                    i--;
            }
        }
        scope.selected = i;
        dirty = 1;
        return NoOp;
    }
    if (PAD_justPressed(BTN_B) || PAD_justRepeated(BTN_LEFT))
    {
        quit = 1;
        dirty = 1;
        return NoOp;
    }
    return MenuList::handleInput(dirty, quit);
}

namespace
{
    class TimezoneItem : public MenuItem
    {
    public:
        TimezoneItem(TimezoneMenu *menu)
            : MenuItem(ListItemType::Generic, "Time zone", "", {std::any(0)}, {std::string("")}, nullptr, nullptr, nullptr,
                       [menu](AbstractMenuItem &item) -> InputReactionHint {
                           menu->select_current();
                           return DeferToSubmenu(item);
                       },
                       menu) {}
        const std::string getLabel() const override { return city_of(TIME_getCurrentTimezone()); }
    };
}

AbstractMenuItem *bramley_timezone_item()
{
    return new TimezoneItem(new TimezoneMenu());
}

///////////////////////////////////////////////////////////
// Lights

// NextUI's effect ids, in the order shown, with plain names.
static const struct
{
    int id;
    const char *name;
} effects[] = {{4, "Static"}, {2, "Breathe"}, {5, "Blink"}, {1, "Fade"}, {3, "Slow breathe"}, {6, "Double blink"}};
static const int speeds[] = {300, 600, 1000, 2000, 3500};
static const char *speed_names[] = {"Very fast", "Fast", "Medium", "Slow", "Very slow"};
#define LIGHTS_SAVED BRAMLEY_STATE_DIR "/lights-off.txt"

static LightsMenu *lights_menu;

int LightsMenu::count() { return LEDS_getCount(); }

std::string LightsMenu::zone_name(int i)
{
    std::string n = lightsDefault[i].name;
    if (n == "f1")
        return "F1";
    if (n == "f2")
        return "F2";
    if (n == "m")
        return "Top bar";
    if (n == "lr")
        return "Shoulders";
    return n;
}

uint32_t LightsMenu::zone_rgb(int i) { return lightsDefault[i].color1; }

bool LightsMenu::on() { return !exists((char *)LIGHTS_SAVED); }

// Shows the change on the LEDs and saves it where LedControl keeps it.
static void apply_lights()
{
    LEDS_setProfile(LIGHT_PROFILE_DEFAULT);
    LEDS_updateLeds(false);
    char path[256];
    snprintf(path, sizeof(path), SHARED_USERDATA_PATH "/%s", PLAT_getLedSettingsFile());
    FILE *f = fopen(path, "w");
    if (!f)
        return;
    for (int i = 0; i < LEDS_getCount(); i++)
    {
        LightSettings &l = lightsDefault[i];
        fprintf(f, "[%s]\neffect=%d\ncolor1=0x%06X\ncolor2=0x%06X\nspeed=%d\nbrightness=%d\ntrigger=%d\nfilename=%s\ninbrightness=%i\n\n",
                l.name, l.effect, l.color1, l.color2, l.speed, l.brightness, l.trigger, l.filename, l.inbrightness);
    }
    fclose(f);
}

// Runs set on the chosen light, or on every light while they match.
template <typename F>
static void each(F set)
{
    LightsMenu *m = lights_menu;
    for (int i = 0; i < LightsMenu::count(); i++)
        if (m->same || i == m->zone)
            set(lightsDefault[i]);
    apply_lights();
}

static LightSettings &chosen() { return lightsDefault[lights_menu ? lights_menu->zone : 0]; }
static bool all_same() { return lights_menu && lights_menu->same; }

static std::vector<AbstractMenuItem *> light_rows();

LightsMenu::LightsMenu() : MenuList(MenuItemType::Fixed, "Lights", light_rows())
{
    lights_menu = this;
    // all lights start matched when they already are
    same = true;
    for (int i = 1; i < count(); i++)
        if (lightsDefault[i].effect != lightsDefault[0].effect || lightsDefault[i].color1 != lightsDefault[0].color1 ||
            lightsDefault[i].brightness != lightsDefault[0].brightness)
            same = false;
    refresh();
}

// The rows read and change the chosen light through lights_menu, set once the menu exists.
static std::vector<AbstractMenuItem *> light_rows()
{
    std::vector<std::any> effect_values, speed_values, bright_values, colour_value = {std::any((uint32_t)0)};
    std::vector<std::string> effect_labels, speed_labels, bright_labels;
    for (auto &e : effects)
        if (e.id <= PLAT_getLedEffectCount())
            effect_values.push_back(e.id), effect_labels.push_back(e.name);
    for (int i = 0; i < 5; i++)
        speed_values.push_back(speeds[i]), speed_labels.push_back(speed_names[i]);
    for (int b = 5; b <= 100; b += 5)
        bright_values.push_back(b), bright_labels.push_back(std::to_string(b) + "%");

    ColorPickerMenu *picker = new ColorPickerMenu(0xFFFFFFFF, [](const std::any &v) {
        uint32_t c = std::any_cast<uint32_t>(v) >> 8;
        each([c](LightSettings &l) { l.color1 = c, l.color2 = c; });
    }, {}, "Colour");

    std::vector<AbstractMenuItem *> rows = {
        new MenuItem{ListItemType::Generic, "Lights", "Turns every light on the Brick on or off.", {false, true}, {"Off", "On"},
                     []() -> std::any { return LightsMenu::on(); },
                     [](const std::any &v) {
                         // off keeps each light's brightness in a file, so on brings it back
                         if (!std::any_cast<bool>(v))
                         {
                             FILE *f = fopen(LIGHTS_SAVED, "w");
                             if (!f)
                                 return;
                             for (int i = 0; i < LightsMenu::count(); i++)
                                 fprintf(f, "%d\n", lightsDefault[i].brightness), lightsDefault[i].brightness = 0;
                             fclose(f);
                         }
                         else
                         {
                             FILE *f = fopen(LIGHTS_SAVED, "r");
                             for (int i = 0; i < LightsMenu::count(); i++)
                                 if (!f || fscanf(f, "%d", &lightsDefault[i].brightness) != 1 || lightsDefault[i].brightness == 0)
                                     lightsDefault[i].brightness = 100;
                             if (f)
                                 fclose(f);
                             unlink(LIGHTS_SAVED);
                         }
                         apply_lights();
                         lights_menu->refresh();
                     }},
        new MenuItem{ListItemType::Generic, "Same for all lights", "Changes below apply to every light at once.", {false, true}, {"Off", "On"},
                     []() -> std::any { return all_same(); },
                     [](const std::any &v) {
                         lights_menu->same = std::any_cast<bool>(v);
                         if (lights_menu->same)
                         {
                             // match every light to the one chosen
                             LightSettings c = chosen();
                             each([c](LightSettings &l) { l.effect = c.effect, l.color1 = c.color1, l.color2 = c.color2, l.speed = c.speed, l.brightness = c.brightness; });
                         }
                     }},
        new MenuItem{ListItemType::Generic, "Effect", "", effect_values, effect_labels,
                     []() -> std::any { return chosen().effect; },
                     [](const std::any &v) { int e = std::any_cast<int>(v); each([e](LightSettings &l) { l.effect = e; }); }},
        new MenuItem{ListItemType::Color, "Colour", "", colour_value, {""},
                     []() -> std::any { return (uint32_t)(chosen().color1 << 8 | 0xFF); }, nullptr, nullptr,
                     [picker](AbstractMenuItem &item) -> InputReactionHint {
                         picker->reset(chosen().color1 << 8 | 0xFF, {}, "Colour");
                         return DeferToSubmenu(item);
                     },
                     picker},
        new MenuItem{ListItemType::Generic, "Brightness", "", bright_values, bright_labels,
                     []() -> std::any { return chosen().brightness < 5 ? 5 : chosen().brightness / 5 * 5; },
                     [](const std::any &v) { int b = std::any_cast<int>(v); each([b](LightSettings &l) { l.brightness = b; }); }},
        new MenuItem{ListItemType::Generic, "Speed", "How fast the light breathes or blinks.", speed_values, speed_labels,
                     []() -> std::any {
                         int s = chosen().speed, best = speeds[0];
                         for (int v : speeds)
                             if (abs(v - s) < abs(best - s))
                                 best = v;
                         return best;
                     },
                     [](const std::any &v) { int s = std::any_cast<int>(v); each([s](LightSettings &l) { l.speed = s; }); }},
    };
    return rows;
}

LightsMenu::~LightsMenu()
{
    if (lights_menu == this)
        lights_menu = nullptr;
}

void LightsMenu::refresh()
{
    for (auto *item : items)
        if (auto *m = dynamic_cast<MenuItem *>(item))
            m->reselect();
}

InputReactionHint LightsMenu::handleInput(int &dirty, int &quit)
{
    if (!same && (PAD_justPressed(BTN_L1) || PAD_justPressed(BTN_R1)))
    {
        int n = count();
        zone = (zone + (PAD_justPressed(BTN_R1) ? 1 : n - 1)) % n;
        refresh();
        dirty = 1;
        return NoOp;
    }
    return MenuList::handleInput(dirty, quit);
}

MenuList *bramley_lights_menu()
{
    if (LEDS_getCount() <= 0)
        return nullptr;
    return new LightsMenu();
}

///////////////////////////////////////////////////////////
// Test Buttons

TestButtonsMenu::TestButtonsMenu() : MenuList(MenuItemType::Custom, "Test Buttons", {})
{
    buttons = {
        {"Up", BTN_DPAD_UP}, {"Down", BTN_DPAD_DOWN}, {"Left", BTN_DPAD_LEFT}, {"Right", BTN_DPAD_RIGHT},
        {"A", BTN_A}, {"B", BTN_B}, {"X", BTN_X}, {"Y", BTN_Y},
        {"L1", BTN_L1}, {"L2", BTN_L2}, {"R1", BTN_R1}, {"R2", BTN_R2},
        {"F1", BTN_FN1}, {"F2", BTN_FN2},
        {"MENU", BTN_MENU}, {"SELECT", BTN_SELECT}, {"START", BTN_START},
        {"Volume up", BTN_PLUS}, {"Volume down", BTN_MINUS},
    };
    // only the buttons this device has
    buttons.erase(std::remove_if(buttons.begin(), buttons.end(), [](const Button &b) { return b.btn == BTN_NONE; }), buttons.end());
    for (auto &b : buttons)
        b.held = b.tested = false;
}

int TestButtonsMenu::tested() const
{
    int n = 0;
    for (auto &b : buttons)
        n += b.tested;
    return n;
}

std::string TestButtonsMenu::held_names() const
{
    std::string s;
    for (auto &b : buttons)
        if (b.held)
            s += (s.empty() ? "" : "  \xC2\xB7  ") + std::string(b.name);
    return s;
}

InputReactionHint TestButtonsMenu::handleInput(int &dirty, int &quit)
{
    for (auto &b : buttons)
    {
        bool held = PAD_isPressed(b.btn);
        if (held != b.held)
            dirty = 1;
        // a press counts when it starts here, so the A that opened the page does not
        if (held && PAD_justPressed(b.btn))
            b.tested = true;
        b.held = held;
    }
    if (PAD_isPressed(BTN_SELECT) && PAD_isPressed(BTN_START))
    {
        for (auto &b : buttons)
            b.held = b.tested = false; // fresh next time
        quit = 1;
        dirty = 1;
    }
    return NoOp;
}

AbstractMenuItem *bramley_test_buttons_item()
{
    return new MenuItem{ListItemType::Generic, "Test Buttons", "Check that every button works.", {}, {}, nullptr, nullptr, DeferToSubmenu,
                        new TestButtonsMenu()};
}
