/**
 * @file hud.h
 * @brief Flight HUD screens for the LVGL Windows simulator.
 *
 * The HUD code is plain LVGL: it never touches Win32, so the same files can be
 * compiled into the ESP32-S3 project later. A HUD is built into any parent
 * object, which is the screen on an MCU (lv_screen_active()) and a fixed size
 * "panel" object in the simulator (so the PC preview keeps the real pixel size).
 *
 * Theme switching is prepared by keeping the palette + fonts in hud_theme_t and
 * the live flight data in hud_data_t: every layout shares both, so adding
 * another design only means adding another hud_*_create().
 */

#ifndef HUD_H
#define HUD_H

#include "lvgl/lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Palette and fonts shared by every HUD layout.
 */
typedef struct
{
    lv_color_t bg;              /**< Screen background. */
    lv_color_t bar;             /**< Top/bottom bar background. */
    lv_color_t line;            /**< Separators, borders. */
    lv_color_t text;            /**< Primary text. */
    lv_color_t text_dim;        /**< Units and captions. */
    lv_color_t accent;          /**< Armed / nominal green. */
    lv_color_t warn;            /**< Red. */
    lv_color_t caution;         /**< Yellow. */
    lv_color_t sky_top;         /**< Horizon sky gradient, top. */
    lv_color_t sky_bottom;      /**< Horizon sky gradient, at the horizon. */
    lv_color_t ground_top;      /**< Horizon ground gradient, at the horizon. */
    lv_color_t ground_bottom;   /**< Horizon ground gradient, bottom. */
    lv_color_t track;           /**< Bar track. */
    const lv_font_t * font_xl;  /**< Largest numeric readouts (e.g. montserrat_48). */
    const lv_font_t * font_xs;  /**< Fallback for long values (e.g. montserrat_40). */
    const lv_font_t * font_l;   /**< Values in the bars (e.g. montserrat_20). */
    const lv_font_t * font_m;   /**< Units and captions (e.g. montserrat_14). */
    const lv_font_t * font_s;   /**< Smallest captions (e.g. montserrat_12). */
} hud_theme_t;

/** @brief Dark theme, taken from the reference design. */
extern const hud_theme_t hud_theme_dark;

/**
 * @brief One snapshot of the flight state.
 *
 * Integers only on purpose: no soft float and no float formatting on the MCU.
 * Values mirror the MAVLink fields the HUD needs (VFR_HUD / GPS_RAW_INT /
 * SYS_STATUS / BATTERY_STATUS / MISSION_CURRENT / HEARTBEAT).
 */
typedef struct
{
    const char * mode;      /**< Flight mode text, e.g. "AUTO". */
    bool armed;             /**< HEARTBEAT armed state; drives the mode colour. */
    int32_t sats;           /**< GPS satellites used. */
    int32_t heading_ddeg;   /**< Heading in 0.1 deg, 0..3599. */
    int32_t roll_ddeg;      /**< Roll in 0.1 deg, right wing down positive. */
    int32_t pitch_ddeg;     /**< Pitch in 0.1 deg, nose up positive. */
    int32_t speed_kmh;      /**< Airspeed, km/h. */
    int32_t alt_m;          /**< Altitude, m. */
    int32_t vs_cms;         /**< Climb rate, cm/s, signed. */
    int32_t thr_pct;        /**< Throttle, 0..100. */
    int32_t home_m;         /**< Distance to home, m (displayed up to 4 digits). */
    int32_t batt_mv;        /**< Battery voltage, mV. */
    int32_t batt_pct;       /**< Battery remaining, 0..100. */
} hud_data_t;

/**
 * @brief Create the minimal HUD layout (reference design #2) inside @p parent.
 * @param parent Object that hosts the screen; sized by the caller (320x172).
 * @param theme  Palette/fonts to use, or NULL for hud_theme_dark.
 * @return The root object of the created screen.
 */
lv_obj_t * hud_minimal_create(lv_obj_t * parent, const hud_theme_t * theme);

/**
 * @brief Push a new flight state into the minimal HUD.
 * @param d New values; the struct is copied, so it may live on the stack.
 */
void hud_minimal_update(const hud_data_t * d);

#ifdef __cplusplus
}
#endif

#endif /* HUD_H */
