// Bramley: Settings pages that replace NextUI's stock apps. Date & Time (from Clock), Lights (from
// LedControl) and Test Buttons (from Input). The pages keep NextUI's menu model; bramley_settings.cpp
// draws them. Design: mockups in spikes/apps-mock (date-time, lights, test-buttons-real).
#pragma once

#include <string>
#include <vector>

#include "menu.hpp"

// The date or the time being changed: columns that move with up and down, opened in place under
// its row. A sets the clock (as NextUI's Clock app did), B leaves it unchanged.
class ClockPicker : public MenuList
{
public:
    explicit ClockPicker(bool time);
    // Reads the clock now; called when the row opens.
    void load();
    InputReactionHint handleInput(int &dirty, int &quit) override;

    bool time;                       // false: day, month, year; true: hour, minute (and AM/PM)
    std::vector<int> value, lo, hi;  // one per column
    int column = 0;
    std::string label(int col, int v) const;
    int columns() const { return (int)value.size(); }
};

// Time zone: every zone as a list of cities, the current one marked. Up and down move, L1 and R1
// jump between regions, A sets the zone.
class TimezoneMenu : public MenuList
{
public:
    TimezoneMenu();
    InputReactionHint handleInput(int &dirty, int &quit) override;
    void select_current();
    std::vector<std::string> zones, regions;
};
// The Time zone row, showing the current city.
AbstractMenuItem *bramley_timezone_item();

// Date & Time: NextUI's three clock settings, then Date and Time rows that open a ClockPicker.
// While the clock is set automatically, Date and Time are shown dimmed and do nothing.
MenuList *bramley_date_time_menu(std::vector<AbstractMenuItem *> clock_items);
// True for the Date and Time rows while the clock is set from the internet.
bool bramley_clock_row_disabled(AbstractMenuItem *item);

// Lights: on or off, whether all lights match, then the chosen light's effect, colour, brightness
// and speed. L1 and R1 choose the light. Changes show on the LEDs at once and are saved to the
// same settings file NextUI's LedControl app writes.
class LightsMenu : public MenuList
{
public:
    LightsMenu();
    ~LightsMenu() override;
    InputReactionHint handleInput(int &dirty, int &quit) override;
    int zone = 0;
    bool same = false;
    // the lights on this device, by NextUI's name for each (f1, f2, m, lr)
    static int count();
    static std::string zone_name(int i);
    static uint32_t zone_rgb(int i);
    static bool on();
    void refresh(); // re-reads every row for the chosen light
};
MenuList *bramley_lights_menu();

// Test Buttons: every button on the Brick, lit while held and marked once pressed. Holding SELECT
// and START together leaves.
class TestButtonsMenu : public MenuList
{
public:
    TestButtonsMenu();
    InputReactionHint handleInput(int &dirty, int &quit) override;
    // one per button: held now, pressed at least once
    struct Button
    {
        const char *name;
        int btn;
        bool held, tested;
    };
    std::vector<Button> buttons;
    int tested() const;
    std::string held_names() const;
};
// The Test Buttons row for the Buttons section.
AbstractMenuItem *bramley_test_buttons_item();
