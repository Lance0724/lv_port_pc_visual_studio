/**
 * @file hud_simulator.cpp
 * @brief Windows-only controls and data source for the portable HUD preview.
 *
 * Nothing in this file is required by the HUD when it is moved to an MCU.
 * It owns the fake data, controls, and LVGL timer, then feeds snapshots through
 * the same hud_minimal_update() API that a real telemetry source would use.
 */

#include "hud_simulator.h"

#include <stdint.h>

namespace {

constexpr int32_t kAnimationTimerPeriodMs = 40;
constexpr uint32_t kDefaultAnimationPeriodMs = 4000;
constexpr int32_t kPanelPadding = 8;
constexpr int32_t kColumnGap = 12;
constexpr int32_t kColumnWidth = 386;
constexpr int32_t kRowHeight = 27;
constexpr int32_t kLabelWidth = 92;
constexpr int32_t kValueWidth = 82;
constexpr int32_t kSliderWidth = kColumnWidth - kLabelWidth - kValueWidth - 12;

constexpr lv_color_t kPanelColor = LV_COLOR_MAKE(12, 18, 27);
constexpr lv_color_t kPanelLineColor = LV_COLOR_MAKE(43, 57, 73);
constexpr lv_color_t kTextColor = LV_COLOR_MAKE(224, 232, 240);
constexpr lv_color_t kDimTextColor = LV_COLOR_MAKE(143, 160, 178);
constexpr lv_color_t kAccentColor = LV_COLOR_MAKE(84, 210, 151);
constexpr lv_color_t kTrackColor = LV_COLOR_MAKE(38, 51, 66);

const char kModes[] = "AUTO\nLOITER\nRTL\nMANUAL";
const char * const kModeNames[] = { "AUTO", "LOITER", "RTL", "MANUAL" };

struct simulator_state_t;

enum class field_t : uint8_t {
    Roll,
    Pitch,
    Heading,
    Speed,
    Altitude,
    VerticalSpeed,
    Throttle,
    BatteryVoltage,
    BatteryPercent,
    HomeDistance,
    Satellites,
};

struct slider_binding_t {
    lv_obj_t * slider;
    lv_obj_t * value;
    field_t field;
};

struct simulator_state_t {
    lv_obj_t * status;
    lv_obj_t * start_label;
    lv_obj_t * period_slider;
    lv_obj_t * period_value;
    lv_obj_t * mode;
    lv_obj_t * armed;
    lv_timer_t * timer;
    slider_binding_t sliders[11];
    uint32_t phase_ms;
    uint32_t last_tick;
    uint32_t animation_period_ms;
    bool running;
    hud_data_t data;
};

simulator_state_t g{};

lv_obj_t * make_label(lv_obj_t * parent, const char * text, const lv_font_t * font,
                      lv_color_t color)
{
    lv_obj_t * label = lv_label_create(parent);
    lv_label_set_text(label, text);
    lv_obj_set_style_text_font(label, font, 0);
    lv_obj_set_style_text_color(label, color, 0);
    return label;
}

lv_obj_t * make_button(lv_obj_t * parent, const char * text, int32_t x, int32_t width)
{
    lv_obj_t * button = lv_button_create(parent);
    lv_obj_set_pos(button, x, 3);
    lv_obj_set_size(button, width, 30);
    lv_obj_t * label = make_label(button, text, &lv_font_montserrat_12, kTextColor);
    lv_obj_center(label);
    return button;
}

void configure_slider(lv_obj_t * slider)
{
    lv_obj_set_style_bg_color(slider, kTrackColor, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(slider, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(slider, 3, LV_PART_MAIN);
    lv_obj_set_style_bg_color(slider, kAccentColor, LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(slider, LV_OPA_COVER, LV_PART_INDICATOR);
    lv_obj_set_style_radius(slider, 3, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(slider, kTextColor, LV_PART_KNOB);
    lv_obj_set_style_bg_opa(slider, LV_OPA_COVER, LV_PART_KNOB);
}

int32_t get_field(field_t field)
{
    switch(field) {
        case field_t::Roll:             return g.data.roll_ddeg;
        case field_t::Pitch:            return g.data.pitch_ddeg;
        case field_t::Heading:          return g.data.heading_ddeg;
        case field_t::Speed:            return g.data.speed_kmh;
        case field_t::Altitude:         return g.data.alt_m;
        case field_t::VerticalSpeed:    return g.data.vs_cms;
        case field_t::Throttle:         return g.data.thr_pct;
        case field_t::BatteryVoltage:   return g.data.batt_mv;
        case field_t::BatteryPercent:   return g.data.batt_pct;
        case field_t::HomeDistance:     return g.data.home_m;
        case field_t::Satellites:       return g.data.sats;
    }
    return 0;
}

void set_field(field_t field, int32_t value)
{
    switch(field) {
        case field_t::Roll:             g.data.roll_ddeg = value; break;
        case field_t::Pitch:            g.data.pitch_ddeg = value; break;
        case field_t::Heading:          g.data.heading_ddeg = value; break;
        case field_t::Speed:            g.data.speed_kmh = value; break;
        case field_t::Altitude:         g.data.alt_m = value; break;
        case field_t::VerticalSpeed:    g.data.vs_cms = value; break;
        case field_t::Throttle:         g.data.thr_pct = value; break;
        case field_t::BatteryVoltage:   g.data.batt_mv = value; break;
        case field_t::BatteryPercent:   g.data.batt_pct = value; break;
        case field_t::HomeDistance:     g.data.home_m = value; break;
        case field_t::Satellites:       g.data.sats = value; break;
    }
}

void format_field(char * buffer, size_t buffer_size, field_t field, int32_t value)
{
    switch(field) {
        case field_t::Roll:
        case field_t::Pitch:
            lv_snprintf(buffer, buffer_size, "%s%d.%d deg", value < 0 ? "-" : "",
                        (int)(value < 0 ? -value : value) / 10,
                        (int)((value < 0 ? -value : value) % 10));
            break;
        case field_t::Heading:
            lv_snprintf(buffer, buffer_size, "%d.%d deg", (int)(value / 10),
                        (int)((value < 0 ? -value : value) % 10));
            break;
        case field_t::Speed:
            lv_snprintf(buffer, buffer_size, "%d km/h", (int)value);
            break;
        case field_t::Altitude:
            lv_snprintf(buffer, buffer_size, "%d m", (int)value);
            break;
        case field_t::VerticalSpeed:
            lv_snprintf(buffer, buffer_size, "%s%d.%d m/s", value < 0 ? "-" : "",
                        (int)(value < 0 ? -value : value) / 100,
                        (int)(((value < 0 ? -value : value) % 100) / 10));
            break;
        case field_t::Throttle:
        case field_t::BatteryPercent:
            lv_snprintf(buffer, buffer_size, "%d %%", (int)value);
            break;
        case field_t::BatteryVoltage:
            lv_snprintf(buffer, buffer_size, "%d.%d V", (int)(value / 1000),
                        (int)((value < 0 ? -value : value) % 1000) / 100);
            break;
        case field_t::HomeDistance:
            lv_snprintf(buffer, buffer_size, "%d m", (int)value);
            break;
        case field_t::Satellites:
            lv_snprintf(buffer, buffer_size, "%d", (int)value);
            break;
    }
}

void refresh_field_controls()
{
    for(size_t i = 0; i < sizeof(g.sliders) / sizeof(g.sliders[0]); i++) {
        const int32_t value = get_field(g.sliders[i].field);
        if(lv_slider_get_value(g.sliders[i].slider) != value) {
            lv_slider_set_value(g.sliders[i].slider, value, LV_ANIM_OFF);
        }

        char text[24];
        format_field(text, sizeof(text), g.sliders[i].field, value);
        lv_label_set_text(g.sliders[i].value, text);
    }

    uint32_t selected_mode = lv_dropdown_get_selected(g.mode);
    if(selected_mode < sizeof(kModeNames) / sizeof(kModeNames[0]) && g.data.mode != kModeNames[selected_mode]) {
        for(uint32_t i = 0; i < sizeof(kModeNames) / sizeof(kModeNames[0]); i++) {
            if(g.data.mode == kModeNames[i]) {
                lv_dropdown_set_selected(g.mode, i);
                break;
            }
        }
    }

    if(g.data.armed) lv_obj_add_state(g.armed, LV_STATE_CHECKED);
    else lv_obj_clear_state(g.armed, LV_STATE_CHECKED);
}

void refresh_hud()
{
    hud_minimal_update(&g.data);
    refresh_field_controls();
}

int32_t triangle_value(int32_t min_value, int32_t max_value, uint32_t phase, uint32_t period)
{
    const uint32_t cycle = period * 2U;
    const uint32_t position = phase % cycle;
    const uint32_t distance = position <= period ? position : cycle - position;
    const int64_t span = (int64_t)max_value - min_value;
    return min_value + (int32_t)((span * distance + period / 2U) / period);
}

void apply_animation(uint32_t phase)
{
    g.data.roll_ddeg = triangle_value(-450, 450, phase, g.animation_period_ms);
    g.data.pitch_ddeg = triangle_value(-300, 300, phase, g.animation_period_ms);
    g.data.heading_ddeg = triangle_value(0, 3590, phase, g.animation_period_ms);
    g.data.speed_kmh = triangle_value(0, 120, phase, g.animation_period_ms);
    g.data.alt_m = triangle_value(0, 2000, phase, g.animation_period_ms);
    g.data.vs_cms = triangle_value(-500, 500, phase, g.animation_period_ms);
    g.data.thr_pct = triangle_value(0, 100, phase, g.animation_period_ms);
    g.data.batt_mv = triangle_value(11000, 16800, phase, g.animation_period_ms);
    g.data.batt_pct = triangle_value(10, 100, phase, g.animation_period_ms);
    g.data.home_m = triangle_value(0, 2000, phase, g.animation_period_ms);
}

void update_status()
{
    lv_label_set_text(g.status, g.running ? "ANIMATION  |  triangle wave" : "STOPPED  |  manual values");
    lv_obj_set_style_text_color(g.status, g.running ? kAccentColor : kDimTextColor, 0);
    lv_label_set_text(g.start_label, g.running ? "Stop animation" : "Start animation");
}

void animation_timer_cb(lv_timer_t * timer)
{
    (void)timer;
    if(!g.running) return;

    const uint32_t now = lv_tick_get();
    g.phase_ms += now - g.last_tick;
    g.last_tick = now;
    apply_animation(g.phase_ms);
    refresh_hud();
}

void animation_button_cb(lv_event_t * event)
{
    if(lv_event_get_code(event) != LV_EVENT_CLICKED) return;

    g.running = !g.running;
    g.last_tick = lv_tick_get();
    if(g.running) lv_timer_resume(g.timer);
    else lv_timer_pause(g.timer);
    update_status();
}

void reset_button_cb(lv_event_t * event)
{
    if(lv_event_get_code(event) != LV_EVENT_CLICKED) return;

    g.running = false;
    g.phase_ms = 0;
    g.last_tick = lv_tick_get();
    lv_timer_pause(g.timer);

    g.data.mode = "AUTO";
    g.data.armed = true;
    g.data.sats = 16;
    g.data.heading_ddeg = 870;
    g.data.roll_ddeg = -40;
    g.data.pitch_ddeg = 30;
    g.data.speed_kmh = 46;
    g.data.alt_m = 123;
    g.data.vs_cms = 140;
    g.data.thr_pct = 46;
    g.data.home_m = 380;
    g.data.batt_mv = 15600;
    g.data.batt_pct = 76;
    refresh_hud();
    update_status();
}

void period_slider_cb(lv_event_t * event)
{
    if(lv_event_get_code(event) != LV_EVENT_VALUE_CHANGED) return;

    g.animation_period_ms = (uint32_t)lv_slider_get_value(g.period_slider);
    lv_label_set_text_fmt(g.period_value, "%d ms", (int)g.animation_period_ms);
    g.phase_ms = 0;
}

void data_slider_cb(lv_event_t * event)
{
    if(lv_event_get_code(event) != LV_EVENT_VALUE_CHANGED) return;

    slider_binding_t * binding = static_cast<slider_binding_t *>(lv_event_get_user_data(event));
    set_field(binding->field, lv_slider_get_value(binding->slider));
    refresh_hud();
}

void mode_dropdown_cb(lv_event_t * event)
{
    if(lv_event_get_code(event) != LV_EVENT_VALUE_CHANGED) return;

    const uint32_t selected = lv_dropdown_get_selected(g.mode);
    if(selected < sizeof(kModeNames) / sizeof(kModeNames[0])) {
        g.data.mode = kModeNames[selected];
        refresh_hud();
    }
}

void armed_switch_cb(lv_event_t * event)
{
    if(lv_event_get_code(event) != LV_EVENT_VALUE_CHANGED) return;

    g.data.armed = lv_obj_has_state(g.armed, LV_STATE_CHECKED);
    refresh_hud();
}

lv_obj_t * create_data_row(lv_obj_t * parent, int32_t x, int32_t y, const char * name,
                           field_t field, int32_t min_value, int32_t max_value, int32_t value)
{
    lv_obj_t * label = make_label(parent, name, &lv_font_montserrat_12, kDimTextColor);
    lv_obj_set_pos(label, x, y + 5);
    lv_obj_set_width(label, kLabelWidth);

    lv_obj_t * slider = lv_slider_create(parent);
    lv_obj_set_pos(slider, x + kLabelWidth, y + 8);
    lv_obj_set_size(slider, kSliderWidth, 10);
    lv_slider_set_range(slider, min_value, max_value);
    lv_slider_set_value(slider, value, LV_ANIM_OFF);
    configure_slider(slider);

    lv_obj_t * value_label = make_label(parent, "", &lv_font_montserrat_12, kTextColor);
    lv_obj_set_pos(value_label, x + kLabelWidth + kSliderWidth + 8, y + 4);
    lv_obj_set_width(value_label, kValueWidth);

    const size_t index = sizeof(g.sliders) / sizeof(g.sliders[0]);
    for(size_t i = 0; i < index; i++) {
        if(g.sliders[i].slider == nullptr) {
            g.sliders[i] = { slider, value_label, field };
            lv_obj_add_event_cb(slider, data_slider_cb, LV_EVENT_VALUE_CHANGED, &g.sliders[i]);
            break;
        }
    }

    char text[24];
    format_field(text, sizeof(text), field, value);
    lv_label_set_text(value_label, text);
    return slider;
}

} // namespace

extern "C" void hud_simulator_create(lv_obj_t * parent, const hud_data_t * initial)
{
    lv_memzero(&g, sizeof(g));
    g.data = *initial;
    g.animation_period_ms = kDefaultAnimationPeriodMs;

    lv_obj_set_style_bg_color(parent, kPanelColor, 0);
    lv_obj_set_style_bg_opa(parent, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(parent, 1, 0);
    lv_obj_set_style_border_color(parent, kPanelLineColor, 0);
    lv_obj_set_style_pad_all(parent, 0, 0);

    lv_obj_t * title = make_label(parent, "SIMULATED TELEMETRY", &lv_font_montserrat_14, kTextColor);
    lv_obj_set_pos(title, kPanelPadding, 10);

    lv_obj_t * start_button = make_button(parent, "Start animation", 185, 140);
    g.start_label = lv_obj_get_child(start_button, 0);
    lv_obj_add_event_cb(start_button, animation_button_cb, LV_EVENT_CLICKED, nullptr);

    lv_obj_t * reset_button = make_button(parent, "Reset", 333, 70);
    lv_obj_add_event_cb(reset_button, reset_button_cb, LV_EVENT_CLICKED, nullptr);

    lv_obj_t * period_label = make_label(parent, "Period", &lv_font_montserrat_12, kDimTextColor);
    lv_obj_set_pos(period_label, 420, 5);
    lv_obj_set_width(period_label, 48);

    g.period_slider = lv_slider_create(parent);
    lv_obj_set_pos(g.period_slider, 470, 13);
    lv_obj_set_size(g.period_slider, 180, 10);
    lv_slider_set_range(g.period_slider, 1000, 10000);
    lv_slider_set_value(g.period_slider, (int32_t)g.animation_period_ms, LV_ANIM_OFF);
    configure_slider(g.period_slider);
    lv_obj_add_event_cb(g.period_slider, period_slider_cb, LV_EVENT_VALUE_CHANGED, nullptr);

    g.period_value = make_label(parent, "4000 ms", &lv_font_montserrat_12, kTextColor);
    lv_obj_set_pos(g.period_value, 658, 5);
    lv_obj_set_width(g.period_value, 70);

    g.status = make_label(parent, "STOPPED  |  manual values", &lv_font_montserrat_12, kDimTextColor);
    lv_obj_set_pos(g.status, kPanelPadding, 34);

    lv_obj_t * separator = lv_obj_create(parent);
    lv_obj_remove_style_all(separator);
    lv_obj_set_pos(separator, 0, 55);
    lv_obj_set_size(separator, 800, 1);
    lv_obj_set_style_bg_color(separator, kPanelLineColor, 0);
    lv_obj_set_style_bg_opa(separator, LV_OPA_COVER, 0);

    lv_obj_t * left = lv_obj_create(parent);
    lv_obj_remove_style_all(left);
    lv_obj_set_pos(left, kPanelPadding, 62);
    lv_obj_set_size(left, kColumnWidth, 210);

    lv_obj_t * right = lv_obj_create(parent);
    lv_obj_remove_style_all(right);
    lv_obj_set_pos(right, kPanelPadding + kColumnWidth + kColumnGap, 62);
    lv_obj_set_size(right, kColumnWidth, 210);

    create_data_row(left, 0, 0, "Roll", field_t::Roll, -1800, 1800, g.data.roll_ddeg);
    create_data_row(left, 0, kRowHeight, "Pitch", field_t::Pitch, -900, 900, g.data.pitch_ddeg);
    create_data_row(left, 0, kRowHeight * 2, "Heading", field_t::Heading, 0, 3590, g.data.heading_ddeg);
    create_data_row(left, 0, kRowHeight * 3, "Speed", field_t::Speed, 0, 200, g.data.speed_kmh);
    create_data_row(left, 0, kRowHeight * 4, "Altitude", field_t::Altitude, -100, 5000, g.data.alt_m);
    create_data_row(left, 0, kRowHeight * 5, "Vert speed", field_t::VerticalSpeed, -1500, 1500, g.data.vs_cms);
    create_data_row(left, 0, kRowHeight * 6, "Throttle", field_t::Throttle, 0, 100, g.data.thr_pct);

    create_data_row(right, 0, 0, "Battery V", field_t::BatteryVoltage, 9000, 16800, g.data.batt_mv);
    create_data_row(right, 0, kRowHeight, "Battery %", field_t::BatteryPercent, 0, 100, g.data.batt_pct);
    create_data_row(right, 0, kRowHeight * 2, "Home", field_t::HomeDistance, 0, 5000, g.data.home_m);
    create_data_row(right, 0, kRowHeight * 3, "Satellites", field_t::Satellites, 0, 24, g.data.sats);

    lv_obj_t * mode_label = make_label(right, "Mode", &lv_font_montserrat_12, kDimTextColor);
    lv_obj_set_pos(mode_label, 0, kRowHeight * 4 + 5);
    lv_obj_set_width(mode_label, kLabelWidth);
    g.mode = lv_dropdown_create(right);
    lv_obj_set_pos(g.mode, kLabelWidth, kRowHeight * 4);
    lv_obj_set_size(g.mode, 120, 24);
    lv_dropdown_set_options(g.mode, kModes);
    lv_dropdown_set_selected(g.mode, 0);
    lv_obj_add_event_cb(g.mode, mode_dropdown_cb, LV_EVENT_VALUE_CHANGED, nullptr);

    lv_obj_t * armed_label = make_label(right, "Armed", &lv_font_montserrat_12, kDimTextColor);
    lv_obj_set_pos(armed_label, 220, kRowHeight * 4 + 5);
    g.armed = lv_switch_create(right);
    lv_obj_set_pos(g.armed, 270, kRowHeight * 4);
    lv_obj_set_size(g.armed, 44, 22);
    if(g.data.armed) lv_obj_add_state(g.armed, LV_STATE_CHECKED);
    lv_obj_add_event_cb(g.armed, armed_switch_cb, LV_EVENT_VALUE_CHANGED, nullptr);

    g.timer = lv_timer_create(animation_timer_cb, kAnimationTimerPeriodMs, nullptr);
    lv_timer_pause(g.timer);
    refresh_hud();
}
