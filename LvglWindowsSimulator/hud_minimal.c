/**
 * @file hud_minimal.c
 * @brief Minimal HUD layout (reference design #2) for a 320x172 panel.
 *
 * Structure, back to front:
 *   - dark screen background,
 *   - a full width horizon card (sky gradient over ground gradient) whose
 *     centre is the horizon line; roll rotates it, pitch moves it vertically,
 *   - the pitch ladder, drawn inside the card so it turns with the horizon,
 *   - a fixed layer: roll scale arc, roll limit marks, sky pointers and the
 *     aircraft symbol,
 *   - airspeed / altitude readouts drawn over the horizon,
 *   - the top status bar and the bottom bar.
 *
 * Implementation notes (kept MCU friendly, see
 * Documents/Lvgl95Review-And-ESP32S3Porting.md):
 *   - every object is created once; hud_minimal_update() only writes text,
 *     moves the throttle bar and applies the horizon transform,
 *   - integers only: no float, no float formatting, trigonometry through
 *     lv_trigo_sin/lv_trigo_cos,
 *   - the ladder and the roll scale are draw callbacks instead of dozens of
 *     objects. Two gotchas they have to respect:
 *       * lv_draw_* works in absolute screen coordinates, so the root's origin
 *         is added to the panel relative geometry,
 *       * an object paints its own draw event before its children, so anything
 *         that must sit on top of the sky/ground needs its own overlay object.
 */

#include "hud.h"

/* ---------------------------------------------------------------- geometry */

#define HUD_W           320
#define HUD_H           172

#define TOP_H           27
#define BOT_H           34      /* the reference design gives the status line room */
#define MID_Y           TOP_H
#define MID_H           (HUD_H - TOP_H - BOT_H)         /* 111 */
/* Keep enough sky and ground on screen when the background is rotated. */
#define MID_CY          (MID_Y + MID_H / 2)              /* 82, the horizon line */

#define CARD_W          HUD_W
#define CARD_H          320
#define CARD_CY         (CARD_H / 2)                    /* horizon inside the card */
#define CARD_Y          (MID_CY - CARD_CY)

#define PITCH_PX_X100   190     /* 1.90 px per degree, *100 to stay integral */
#define PITCH_VERTICAL_DDEG 900 /* +-90.0 degrees: horizon is outside the view */

#define ROLL_R          64      /* roll scale radius, from the aircraft symbol */
#define ROLL_MARK_MAX   60      /* +-60 degrees on the printed scale */

#define NUM_W           96      /* width of the left/right readout block */

/* The readouts sit on a dark panel that fades into the horizon window, as in
 * the reference design: flat dark up to SHADE_FLAT, fully transparent at
 * SHADE_W. Covers the horizon but sits under the pointers and the readouts. */
#define SHADE_W         130
#define SHADE_FLAT      55

/* --------------------------------------------------------------- the theme */

const hud_theme_t hud_theme_dark =
{
    .bg            = LV_COLOR_MAKE(0x02, 0x0C, 0x1C),
    .bar           = LV_COLOR_MAKE(0x04, 0x0A, 0x16),
    .line          = LV_COLOR_MAKE(0x1E, 0x2A, 0x38),
    .text          = LV_COLOR_MAKE(0xE8, 0xF2, 0xFB),
    .text_dim      = LV_COLOR_MAKE(0x8F, 0xA3, 0xB8),
    .accent        = LV_COLOR_MAKE(0x13, 0xD4, 0x60),
    .warn          = LV_COLOR_MAKE(0xFF, 0x3C, 0x3B),
    .caution       = LV_COLOR_MAKE(0xF3, 0xDF, 0x15),
    .sky_top       = LV_COLOR_MAKE(0x01, 0x20, 0x50),
    .sky_bottom    = LV_COLOR_MAKE(0x05, 0x65, 0xB0),
    .ground_top    = LV_COLOR_MAKE(0x5C, 0x43, 0x30),
    .ground_bottom = LV_COLOR_MAKE(0x45, 0x2B, 0x13),
    .track         = LV_COLOR_MAKE(0x1B, 0x2A, 0x3A),
    .font_xl       = &lv_font_montserrat_48,
    .font_xs       = &lv_font_montserrat_40,
    .font_l        = &lv_font_montserrat_20,
    .font_m        = &lv_font_montserrat_14,
    .font_s        = &lv_font_montserrat_12,
};

/* ------------------------------------------------------------------ widget */

static struct
{
    const hud_theme_t * th;
    lv_obj_t * root;

    /* top bar */
    lv_obj_t * mode;
    lv_obj_t * sats;
    lv_obj_t * hdg_val;
    lv_obj_t * batt_icon;
    lv_obj_t * batt_val;

    /* horizon */
    lv_obj_t * attitude_backdrop;
    lv_obj_t * card;
    int8_t backdrop_side;       /* -1 ground, +1 sky */
    bool card_hidden;

    /* readouts: both values deliberately share the same fixed face */
    lv_obj_t * spd_val;
    lv_obj_t * alt_val;

    /* bottom bar */
    lv_obj_t * vs_icon;
    lv_obj_t * vs_val;
    lv_obj_t * thr_val;
    lv_obj_t * thr_bar;
    lv_obj_t * home_val;
} g;

/* ------------------------------------------------------------- small helpers */

static lv_obj_t * make_box(lv_obj_t * parent, int32_t x, int32_t y, int32_t w, int32_t h,
                           lv_color_t color)
{
    lv_obj_t * o = lv_obj_create(parent);
    lv_obj_remove_style_all(o);
    lv_obj_set_pos(o, x, y);
    lv_obj_set_size(o, w, h);
    lv_obj_set_style_bg_color(o, color, 0);
    lv_obj_set_style_bg_opa(o, LV_OPA_COVER, 0);
    lv_obj_remove_flag(o, (lv_obj_flag_t)(LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE |
                                          LV_OBJ_FLAG_SCROLL_CHAIN_HOR | LV_OBJ_FLAG_SCROLL_CHAIN_VER));
    return o;
}

static lv_obj_t * make_transparent(lv_obj_t * parent, int32_t x, int32_t y, int32_t w, int32_t h)
{
    lv_obj_t * o = lv_obj_create(parent);
    lv_obj_remove_style_all(o);
    lv_obj_set_pos(o, x, y);
    lv_obj_set_size(o, w, h);
    lv_obj_remove_flag(o, (lv_obj_flag_t)(LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE |
                                          LV_OBJ_FLAG_SCROLL_CHAIN_HOR | LV_OBJ_FLAG_SCROLL_CHAIN_VER));
    return o;
}

static lv_obj_t * make_label(lv_obj_t * parent, const hud_theme_t * th, const lv_font_t * font,
                             lv_color_t color, const char * text)
{
    lv_obj_t * l = lv_label_create(parent);
    lv_obj_remove_style_all(l);
    lv_obj_set_style_text_font(l, font, 0);
    lv_obj_set_style_text_color(l, color, 0);
    lv_label_set_text(l, text);
    return l;
}

static void draw_line(lv_layer_t * layer, lv_color_t color, int32_t width,
                      int32_t x1, int32_t y1, int32_t x2, int32_t y2)
{
    lv_draw_line_dsc_t dsc;
    lv_draw_line_dsc_init(&dsc);
    dsc.color = color;
    dsc.width = width;
    dsc.round_start = 1;
    dsc.round_end = 1;
    dsc.p1.x = x1;
    dsc.p1.y = y1;
    dsc.p2.x = x2;
    dsc.p2.y = y2;
    lv_draw_line(layer, &dsc);
}

/** @brief Screen coordinates of the HUD's top left corner. */
static void hud_origin(int32_t * ox, int32_t * oy)
{
    lv_area_t rc;
    lv_obj_get_coords(g.root, &rc);
    *ox = rc.x1;
    *oy = rc.y1;
}

/**
 * @brief Select the dominant attitude background and handle vertical flight.
 *
 * At +-90 degrees pitch the Euler roll angle is singular: the horizon is
 * outside the view and roll must not rotate the opposite half-plane back onto
 * the screen. Hide the finite card at that boundary and expose a full-band
 * sky/ground backdrop instead.
 */
static void update_attitude_backdrop(int32_t pitch_ddeg)
{
    const int8_t side = pitch_ddeg < 0 ? -1 : 1;
    if(side != g.backdrop_side) {
        const hud_theme_t * th = g.th;
        const lv_color_t top = side < 0 ? th->ground_top : th->sky_top;
        const lv_color_t bottom = side < 0 ? th->ground_bottom : th->sky_bottom;
        lv_obj_set_style_bg_color(g.attitude_backdrop, top, 0);
        lv_obj_set_style_bg_grad_color(g.attitude_backdrop, bottom, 0);
        g.backdrop_side = side;
    }

    const bool vertical = pitch_ddeg <= -PITCH_VERTICAL_DDEG ||
                          pitch_ddeg >= PITCH_VERTICAL_DDEG;
    if(vertical == g.card_hidden) return;

    if(vertical) lv_obj_add_flag(g.card, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_remove_flag(g.card, LV_OBJ_FLAG_HIDDEN);
    g.card_hidden = vertical;
}


/* ------------------------------------------------------------ draw callbacks */

/** @brief Pitch ladder, painted over the sky/ground inside the rotating card. */
static void ladder_draw_cb(lv_event_t * e)
{
    /* The reference design's ladder: a line every 5 degrees, the 10 and 20 deg
     * ones long and labelled, the 5 and 15 deg ones short. That is also what
     * makes the 20 deg rungs read as double lines. The strings are literals
     * with static lifetime on purpose: the draw task outlives this callback, so
     * a stack buffer would be dead by the time it is rasterised. */
    typedef struct
    {
        int32_t deg;
        int32_t half;
        const char * txt;   /* NULL: not a labelled rung */
    } rung_t;
    static const rung_t rungs[] =
    {
        {   5,  7, NULL }, {  10, 16, "10" }, {  15,  9, NULL }, {  20, 15, "20" },
        {  -5,  7, NULL }, { -10, 16, "-10" }, { -15,  9, NULL }, { -20, 15, "-20" },
    };

    lv_layer_t * layer = lv_event_get_layer(e);
    const hud_theme_t * th = g.th;

    int32_t ox, oy;
    hud_origin(&ox, &oy);
    const int32_t cx = ox + HUD_W / 2;
    /* The draw callback uses absolute screen coordinates. CARD_CY is local to
     * the enlarged card (320 px), so using it here placed the zero-pitch
     * reference 78 px below the actual card/horizon centre. */
    const int32_t cy = oy + MID_CY;

    for(uint32_t i = 0; i < sizeof(rungs) / sizeof(rungs[0]); i++) {
        const rung_t * r = &rungs[i];
        /* positive pitch (nose up) is drawn above the horizon, as in an
         * attitude indicator: the whole ladder moves with the horizon card */
        const int32_t yy = cy - r->deg * PITCH_PX_X100 / 100;

        draw_line(layer, th->text, 1, cx - r->half, yy, cx + r->half, yy);
        if(r->txt == NULL) continue;

        /* the label is painted from the same geometry, so it can never drift
         * away from its rung */
        lv_draw_label_dsc_t ld;
        lv_draw_label_dsc_init(&ld);
        ld.color = th->text;
        ld.font = th->font_s;
        ld.text = r->txt;
        ld.text_static = 1;
        for(int32_t s = -1; s <= 1; s += 2) {
            /* lv_draw_label baseline aligns the first line: the glyph top lands
             * at area.y1 + line_height + (line_height - base_line) - glyph
             * height, i.e. ~9 px below the area's top for this font. The box is
             * therefore shifted up to centre the digits on the rung, and made
             * tall enough not to clip them. */
            lv_area_t la = { cx + s * (r->half + 14) - 10, yy - 14,
                             cx + s * (r->half + 14) + 10, yy + 8 };
            lv_draw_label(layer, &ld, &la);
        }
    }
}

/** @brief Fixed layer: roll scale, limit marks, sky pointers, aircraft symbol. */
static void fixed_draw_cb(lv_event_t * e)
{
    lv_layer_t * layer = lv_event_get_layer(e);
    const hud_theme_t * th = g.th;

    int32_t ox, oy;
    hud_origin(&ox, &oy);
    const int32_t cx = ox + HUD_W / 2;
    const int32_t cy = oy + MID_CY;

    /* roll scale arc: 0 degrees points up */
    lv_draw_arc_dsc_t arc;
    lv_draw_arc_dsc_init(&arc);
    arc.color = th->text_dim;
    arc.width = 1;
    arc.center.x = cx;
    arc.center.y = cy;
    arc.radius = ROLL_R;
    arc.start_angle = 270 - ROLL_MARK_MAX;
    arc.end_angle = 270 + ROLL_MARK_MAX;
    lv_draw_arc(layer, &arc);

    static const int32_t marks[] = { 60, 45, 30, 20, 10, 0 };
    for(uint32_t i = 0; i < sizeof(marks) / sizeof(marks[0]); i++) {
        for(int32_t s = -1; s <= 1; s += 2) {
            int32_t a = 270 + s * marks[i];
            int32_t dx = lv_trigo_cos(a);
            int32_t dy2 = lv_trigo_sin(a);
            int32_t r_in = ROLL_R - ((marks[i] == 0) ? 9 : (marks[i] % 30 == 0 ? 7 : 4));
            draw_line(layer, th->text, (marks[i] == 0) ? 2 : 1,
                      cx + (ROLL_R * dx) / 32767, cy + (ROLL_R * dy2) / 32767,
                      cx + (r_in * dx) / 32767, cy + (r_in * dy2) / 32767);
        }
    }

    /* fixed roll pointer, hanging under the top bar */
    lv_draw_triangle_dsc_t tri;
    lv_draw_triangle_dsc_init(&tri);
    tri.color = th->accent;
    tri.opa = LV_OPA_COVER;
    tri.p[0].x = cx - 6;
    tri.p[0].y = oy + MID_Y + 3;
    tri.p[1].x = cx + 6;
    tri.p[1].y = oy + MID_Y + 3;
    tri.p[2].x = cx;
    tri.p[2].y = oy + MID_Y + 11;
    lv_draw_triangle(layer, &tri);

    /* roll limit marks at +-30 degrees */
    for(int32_t s = -1; s <= 1; s += 2) {
        int32_t a = 270 + s * 30;
        int32_t dx = lv_trigo_cos(a);
        int32_t dy2 = lv_trigo_sin(a);
        draw_line(layer, th->warn, 2,
                  cx + ((ROLL_R + 9) * dx) / 32767, cy + ((ROLL_R + 9) * dy2) / 32767,
                  cx + ((ROLL_R + 2) * dx) / 32767, cy + ((ROLL_R + 2) * dy2) / 32767);
    }

    /* sky pointers: green bar with a yellow tip, either side of the horizon */
    lv_draw_rect_dsc_t r;
    lv_draw_rect_dsc_init(&r);
    r.bg_opa = LV_OPA_COVER;
    for(int32_t s = -1; s <= 1; s += 2) {
        int32_t x = cx + s * 62;
        r.bg_color = th->caution;
        lv_area_t a = { x + 2, cy - 1, x + 10, cy + 1 };
        if(s < 0) {
            a.x1 = x - 10;
            a.x2 = x - 2;
        }
        lv_draw_rect(layer, &r, &a);

        r.bg_color = th->accent;
        lv_area_t b = { x - 2, cy - 5, x + 2, cy + 5 };
        lv_draw_rect(layer, &r, &b);
    }

    /* aircraft symbol: wing + fin + centre dot */
    draw_line(layer, th->text, 2, cx - 43, cy + 2, cx - 12, cy);
    draw_line(layer, th->text, 2, cx - 12, cy, cx + 12, cy);
    draw_line(layer, th->text, 2, cx + 12, cy, cx + 43, cy + 2);
    draw_line(layer, th->text, 2, cx, cy - 6, cx, cy + 1);

    lv_draw_rect_dsc_t dot;
    lv_draw_rect_dsc_init(&dot);
    dot.bg_opa = LV_OPA_COVER;
    dot.bg_color = th->text;
    dot.radius = LV_RADIUS_CIRCLE;
    lv_area_t da = { cx - 2, cy - 2, cx + 2, cy + 2 };
    lv_draw_rect(layer, &dot, &da);
}

/* ------------------------------------------------------------------- create */

/**
 * @brief Dark side panels that carry the readouts, fading into the window.
 *
 * The reference design does not run the horizon under the numbers: the outer
 * part of the middle band is near black and blends into the horizon window.
 * A two stop horizontal gradient does this in one object per side (stop 0 is
 * the left edge, stop 1 the right edge) and LVGL's software renderer
 * interpolates the per stop opacity, so no extra buffer or blend pass is
 * needed. Neither shade rotates with the horizon.
 */
static void build_shades(const hud_theme_t * th)
{
    const int32_t flat = SHADE_FLAT * 255 / SHADE_W;

    for(int32_t s = 0; s < 2; s++) {
        lv_obj_t * o = make_box(g.root, (s == 0) ? 0 : HUD_W - SHADE_W, MID_Y,
                                SHADE_W, MID_H, th->bar);
        lv_obj_set_style_bg_grad_color(o, th->bar, 0);
        lv_obj_set_style_bg_grad_dir(o, LV_GRAD_DIR_HOR, 0);
        if(s == 0) {
            lv_obj_set_style_bg_main_stop(o, flat, 0);
            lv_obj_set_style_bg_grad_stop(o, 255, 0);
            lv_obj_set_style_bg_main_opa(o, LV_OPA_COVER, 0);
            lv_obj_set_style_bg_grad_opa(o, LV_OPA_TRANSP, 0);
        }
        else {
            lv_obj_set_style_bg_main_stop(o, 0, 0);
            lv_obj_set_style_bg_grad_stop(o, 255 - flat, 0);
            lv_obj_set_style_bg_main_opa(o, LV_OPA_TRANSP, 0);
            lv_obj_set_style_bg_grad_opa(o, LV_OPA_COVER, 0);
        }
    }
}

static void build_top_bar(const hud_theme_t * th)
{
    lv_obj_t * bar = make_box(g.root, 0, 0, HUD_W, TOP_H, th->bar);
    make_box(bar, 0, TOP_H - 1, HUD_W, 1, th->line);

    g.mode = make_label(bar, th, th->font_l, th->accent, "AUTO");
    lv_obj_align(g.mode, LV_ALIGN_LEFT_MID, 8, 0);

    /* anchored to the mode label, so a longer mode name cannot be overrun */
    g.sats = make_label(bar, th, th->font_s, th->text, LV_SYMBOL_GPS " GPS 16");
    lv_obj_align_to(g.sats, g.mode, LV_ALIGN_OUT_RIGHT_MID, 10, 0);

    g.hdg_val = make_label(bar, th, th->font_l, th->text, "---\xC2\xB0");
    lv_obj_align(g.hdg_val, LV_ALIGN_CENTER, 20, 0);

    g.batt_icon = make_label(bar, th, th->font_m, th->accent, LV_SYMBOL_BATTERY_3);
    lv_obj_align(g.batt_icon, LV_ALIGN_RIGHT_MID, -74, 0);

    g.batt_val = make_label(bar, th, th->font_m, th->text, "---");
    lv_obj_align(g.batt_val, LV_ALIGN_RIGHT_MID, -6, 0);
}

static void build_horizon(const hud_theme_t * th)
{
    /* The finite rotating card can leave the viewport at extreme pitch. This
     * full-band layer supplies the correct dominant half-plane underneath it
     * and becomes the complete attitude background at +-90 degrees. */
    g.attitude_backdrop = make_box(g.root, 0, MID_Y, HUD_W, MID_H, th->sky_top);
    lv_obj_set_style_bg_grad_color(g.attitude_backdrop, th->sky_bottom, 0);
    lv_obj_set_style_bg_grad_dir(g.attitude_backdrop, LV_GRAD_DIR_VER, 0);
    g.backdrop_side = 1;

    g.card = lv_obj_create(g.root);
    lv_obj_remove_style_all(g.card);
    lv_obj_set_pos(g.card, 0, CARD_Y);
    lv_obj_set_size(g.card, CARD_W, CARD_H);
    lv_obj_remove_flag(g.card, (lv_obj_flag_t)(LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE |
                                               LV_OBJ_FLAG_SCROLL_CHAIN_HOR | LV_OBJ_FLAG_SCROLL_CHAIN_VER));
    lv_obj_set_style_transform_pivot_x(g.card, CARD_W / 2, 0);
    lv_obj_set_style_transform_pivot_y(g.card, CARD_CY, 0);

    lv_obj_t * sky = make_box(g.card, 0, 0, CARD_W, CARD_CY, th->sky_top);
    lv_obj_set_style_bg_grad_color(sky, th->sky_bottom, 0);
    lv_obj_set_style_bg_grad_dir(sky, LV_GRAD_DIR_VER, 0);

    lv_obj_t * gnd = make_box(g.card, 0, CARD_CY, CARD_W, CARD_H - CARD_CY, th->ground_top);
    lv_obj_set_style_bg_grad_color(gnd, th->ground_bottom, 0);
    lv_obj_set_style_bg_grad_dir(gnd, LV_GRAD_DIR_VER, 0);

    /* the ladder needs its own overlay: sky/ground are drawn after the card's
     * own draw event. Rungs and their labels are painted there, from one set of
     * coordinates, so they always stay aligned. */
    lv_obj_t * ladder = make_transparent(g.card, 0, 0, CARD_W, CARD_H);
    lv_obj_add_event_cb(ladder, ladder_draw_cb, LV_EVENT_DRAW_MAIN, NULL);

    /* dark readout panels over the horizon, under the pointers and numbers */
    build_shades(th);

    /* fixed layer above the horizon */
    lv_obj_t * fixed = make_transparent(g.root, 0, MID_Y, HUD_W, MID_H);
    lv_obj_add_event_cb(fixed, fixed_draw_cb, LV_EVENT_DRAW_MAIN, NULL);
}

static void build_readouts(const hud_theme_t * th)
{
    /* Keep both primary readouts at the same slightly reduced size. Montserrat
     * 40 fits the four-digit altitude inside the fixed 96 px block. */
    g.spd_val = make_label(g.root, th, th->font_xs, th->text, "0");
    lv_obj_set_width(g.spd_val, NUM_W);
    lv_obj_set_style_text_align(g.spd_val, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(g.spd_val, LV_LABEL_LONG_MODE_CLIP);
    lv_obj_align(g.spd_val, LV_ALIGN_TOP_LEFT, 2, MID_Y + 22);

    lv_obj_t * spd_unit = make_label(g.root, th, th->font_m, th->text, "km/h");
    lv_obj_align(spd_unit, LV_ALIGN_TOP_LEFT, 30, MID_Y + 66);
    lv_obj_t * spd_cap = make_label(g.root, th, th->font_s, th->text_dim, "SPD");
    lv_obj_align(spd_cap, LV_ALIGN_TOP_LEFT, 42, MID_Y + 86);

    g.alt_val = make_label(g.root, th, th->font_xs, th->text, "0");
    lv_obj_set_width(g.alt_val, NUM_W);
    lv_obj_set_style_text_align(g.alt_val, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(g.alt_val, LV_LABEL_LONG_MODE_CLIP);
    lv_obj_align(g.alt_val, LV_ALIGN_TOP_RIGHT, -2, MID_Y + 22);

    lv_obj_t * alt_unit = make_label(g.root, th, th->font_m, th->text, "m");
    lv_obj_align(alt_unit, LV_ALIGN_TOP_RIGHT, -50, MID_Y + 66);
    lv_obj_t * alt_cap = make_label(g.root, th, th->font_s, th->text_dim, "ALT");
    lv_obj_align(alt_cap, LV_ALIGN_TOP_RIGHT, -52, MID_Y + 86);
}

static void build_bottom_bar(const hud_theme_t * th)
{
    lv_obj_t * bar = make_box(g.root, 0, HUD_H - BOT_H, HUD_W, BOT_H, th->bar);
    make_box(bar, 0, 0, HUD_W, 1, th->line);

    /* Bottom bar budget, left to right, worst case text widths at the fonts
     * above: vs value "-12.3 m/s" (~66 px), "THR", "100%" (~35 px), the bar, the
     * home icon, "HOME" (~34 px) and the home value "1420 m" (~44 px). */
    g.vs_icon = make_label(bar, th, th->font_l, th->accent, LV_SYMBOL_UP);
    lv_obj_align(g.vs_icon, LV_ALIGN_LEFT_MID, 6, -4);

    g.vs_val = make_label(bar, th, th->font_m, th->text, "--");
    lv_obj_align(g.vs_val, LV_ALIGN_LEFT_MID, 24, -4);

    lv_obj_t * vs_cap = make_label(bar, th, th->font_s, th->text_dim, "VS");
    lv_obj_align(vs_cap, LV_ALIGN_LEFT_MID, 26, 8);

    make_box(bar, 94, 4, 1, BOT_H - 8, th->line);

    lv_obj_t * thr_cap = make_label(bar, th, th->font_s, th->text_dim, "THR");
    lv_obj_align(thr_cap, LV_ALIGN_LEFT_MID, 98, 0);

    g.thr_val = make_label(bar, th, th->font_m, th->text, "--%");
    lv_obj_align(g.thr_val, LV_ALIGN_LEFT_MID, 128, 0);

    g.thr_bar = lv_bar_create(bar);
    lv_obj_remove_style_all(g.thr_bar);
    lv_obj_set_size(g.thr_bar, 38, 8);
    lv_obj_align(g.thr_bar, LV_ALIGN_LEFT_MID, 165, 0);
    lv_bar_set_range(g.thr_bar, 0, 100);
    lv_obj_set_style_radius(g.thr_bar, 4, 0);
    lv_obj_set_style_bg_color(g.thr_bar, th->track, 0);
    lv_obj_set_style_bg_opa(g.thr_bar, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(g.thr_bar, 4, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(g.thr_bar, th->accent, LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(g.thr_bar, LV_OPA_COVER, LV_PART_INDICATOR);

    make_box(bar, 207, 4, 1, BOT_H - 8, th->line);

    /* the home group is chained (icon -> caption -> value) so the runtime text
     * width decides the spacing instead of a hand measured offset */
    lv_obj_t * home_icon = make_label(bar, th, th->font_m, th->text, LV_SYMBOL_HOME);
    lv_obj_align(home_icon, LV_ALIGN_LEFT_MID, 211, 0);

    lv_obj_t * home_cap = make_label(bar, th, th->font_s, th->text_dim, "HOME");
    lv_obj_align_to(home_cap, home_icon, LV_ALIGN_OUT_RIGHT_MID, 4, 0);

    g.home_val = make_label(bar, th, th->font_s, th->text, "---");
    lv_obj_align_to(g.home_val, home_cap, LV_ALIGN_OUT_RIGHT_MID, 4, 0);
}

lv_obj_t * hud_minimal_create(lv_obj_t * parent, const hud_theme_t * theme)
{
    const hud_theme_t * th = theme ? theme : &hud_theme_dark;
    lv_memzero(&g, sizeof(g));
    g.th = th;

    g.root = lv_obj_create(parent);
    lv_obj_remove_style_all(g.root);
    lv_obj_set_size(g.root, HUD_W, HUD_H);
    lv_obj_set_style_bg_color(g.root, th->bg, 0);
    lv_obj_set_style_bg_opa(g.root, LV_OPA_COVER, 0);
    lv_obj_remove_flag(g.root, (lv_obj_flag_t)(LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE |
                                               LV_OBJ_FLAG_SCROLL_CHAIN_HOR | LV_OBJ_FLAG_SCROLL_CHAIN_VER));

    build_horizon(th);      /* back */
    build_readouts(th);     /* over the horizon */
    build_top_bar(th);      /* opaque bars last: they clip the horizon overflow */
    build_bottom_bar(th);

    return g.root;
}

/* ------------------------------------------------------------------- update */

void hud_minimal_update(const hud_data_t * d)
{
    const hud_theme_t * th = g.th;

    /* top bar */
    lv_label_set_text(g.mode, d->mode ? d->mode : "--");
    lv_obj_set_style_text_color(g.mode, d->armed ? th->accent : th->text_dim, 0);
    lv_label_set_text_fmt(g.sats, LV_SYMBOL_GPS " GPS %d", (int)d->sats);

    int32_t hdg = ((d->heading_ddeg / 10) % 360 + 360) % 360;
    lv_label_set_text_fmt(g.hdg_val, "%03d\xC2\xB0", (int)hdg);

    const char * batt_sym = LV_SYMBOL_BATTERY_EMPTY;
    if(d->batt_pct > 75) batt_sym = LV_SYMBOL_BATTERY_FULL;
    else if(d->batt_pct > 50) batt_sym = LV_SYMBOL_BATTERY_3;
    else if(d->batt_pct > 25) batt_sym = LV_SYMBOL_BATTERY_2;
    else if(d->batt_pct > 10) batt_sym = LV_SYMBOL_BATTERY_1;
    lv_label_set_text(g.batt_icon, batt_sym);
    lv_obj_set_style_text_color(g.batt_icon, d->batt_pct > 25 ? th->accent : th->warn, 0);
    lv_label_set_text_fmt(g.batt_val, "%d.%dV %d%%",
                          (int)(d->batt_mv / 1000), (int)((d->batt_mv % 1000) / 100), (int)d->batt_pct);

    /* The primary readouts share one fixed face: four-digit altitude remains
     * readable instead of causing only the right side to fall back to 20 px. */
    char buf[16];
    lv_snprintf(buf, sizeof(buf), "%d", (int)d->speed_kmh);
    lv_label_set_text(g.spd_val, buf);

    lv_snprintf(buf, sizeof(buf), "%d", (int)d->alt_m);
    lv_label_set_text(g.alt_val, buf);

    /* Roll rotates the finite card and pitch moves it vertically. At vertical
     * pitch the card is hidden: roll is singular there and must not bring the
     * opposite sky/ground half-plane back into view. */
    update_attitude_backdrop(d->pitch_ddeg);
    int32_t ty = d->pitch_ddeg * PITCH_PX_X100 / 1000;
    lv_obj_set_style_transform_rotation(g.card, d->roll_ddeg, 0);
    lv_obj_set_style_translate_x(g.card, 0, 0);
    lv_obj_set_style_translate_y(g.card, ty, 0);

    /* bottom bar */
    int32_t vs = d->vs_cms;
    const char * sign = vs < 0 ? "-" : "+";
    int32_t vs_abs = vs < 0 ? -vs : vs;
    lv_label_set_text(g.vs_icon, vs < 0 ? LV_SYMBOL_DOWN : LV_SYMBOL_UP);
    lv_obj_set_style_text_color(g.vs_icon, vs < 0 ? th->warn : th->accent, 0);
    lv_label_set_text_fmt(g.vs_val, "%s%d.%d m/s", sign, (int)(vs_abs / 100), (int)((vs_abs % 100) / 10));

    lv_label_set_text_fmt(g.thr_val, "%d%%", (int)d->thr_pct);
    lv_bar_set_value(g.thr_bar, d->thr_pct, LV_ANIM_OFF);

    lv_label_set_text_fmt(g.home_val, "%d m", (int)d->home_m);
}
