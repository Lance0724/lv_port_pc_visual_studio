#include <Windows.h>

#include <LvglWindowsIconResource.h>

#include "lvgl/lvgl.h"
#include "lvgl/examples/lv_examples.h"
#include "lvgl/demos/lv_demos.h"

#include <stdio.h>
#include <stdlib.h>

#include "ui.h"

void vhud();

int main()
{
    lv_init();

    /*
     * Optional workaround for users who wants UTF-8 console output.
     * If you don't want that behavior can comment them out.
     *
     * Suggested by jinsc123654.
     */
#if LV_TXT_ENC == LV_TXT_ENC_UTF8
    SetConsoleCP(CP_UTF8);
    SetConsoleOutputCP(CP_UTF8);
#endif

    int32_t zoom_level = 100;
    bool allow_dpi_override = false;
    bool simulator_mode = true;
    lv_display_t* display = lv_windows_create_display(
        L"LVGL Windows Simulator Display 1",
        800,
        480,
        zoom_level,
        allow_dpi_override,
        simulator_mode);
    if (!display)
    {
        return -1;
    }

    HWND window_handle = lv_windows_get_display_window_handle(display);
    if (!window_handle)
    {
        return -1;
    }

    HICON icon_handle = LoadIconW(
        GetModuleHandleW(NULL),
        MAKEINTRESOURCE(IDI_LVGL_WINDOWS));
    if (icon_handle)
    {
        SendMessageW(
            window_handle,
            WM_SETICON,
            TRUE,
            (LPARAM)icon_handle);
        SendMessageW(
            window_handle,
            WM_SETICON,
            FALSE,
            (LPARAM)icon_handle);
    }

    lv_indev_t* pointer_indev = lv_windows_acquire_pointer_indev(display);
    if (!pointer_indev)
    {
        return -1;
    }

    lv_indev_t* keypad_indev = lv_windows_acquire_keypad_indev(display);
    if (!keypad_indev)
    {
        return -1;
    }

    lv_indev_t* encoder_indev = lv_windows_acquire_encoder_indev(display);
    if (!encoder_indev)
    {
        return -1;
    }

    // lv_demo_widgets();
    // lv_demo_benchmark();
    // lv_demo_music();

    // lv_opa_t opa_values[2] = {0xff, 0x80};
    // uint32_t opa;
    // int cnt = 0;
    // for(opa = 0; opa < 2; opa++) {
    //     uint32_t i;
    //     for(i = 0; i < _LV_DEMO_RENDER_SCENE_NUM; i++) {
    //         lv_demo_render((lv_demo_render_scene_t)i, opa_values[opa]);

    //         if (cnt > 25) {
    //             break;
    //         }
    //         cnt++;
    //         // break;
    //     }
    // }

    // lv_demo_transform();

    // ui();
    vhud();
    // lv_example_canvas_2();

    while (1)
    {
        uint32_t time_till_next = lv_timer_handler();
        lv_delay_ms(time_till_next);
    }

    return 0;
}

lv_obj_t * card;

static void rollSlider_event_cb(lv_event_t * e);
static void pitchSlider_event_cb(lv_event_t * e);

lv_obj_t* labelRoll;
lv_obj_t* labelPitch;
lv_obj_t* labelOffx;
lv_obj_t* labelOffy;

void guide_lines(lv_obj_t* ai_hole_panel, lv_obj_t* card);
void lv_roll_scale(lv_obj_t* ai_hole_panel, lv_obj_t* card);

void vhud()
{
    lv_obj_t * main_cont_col = lv_obj_create(lv_screen_active());
    lv_obj_set_size(main_cont_col, 500, 500);
    // lv_obj_align(main_cont_col, LV_ALIGN_TOP_MID, 0, 5);
    lv_obj_set_flex_flow(main_cont_col, LV_FLEX_FLOW_COLUMN);
    lv_obj_remove_flag(main_cont_col, (lv_obj_flag_t)(LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_SCROLL_CHAIN_HOR));

    lv_obj_t * view_cont_row = lv_obj_create(main_cont_col);
    lv_obj_set_width(view_cont_row, lv_pct(100));
    // lv_obj_set_size(view_cont_row, 500, 300);
    
    // lv_obj_align_to(view_cont_row, cont_row, LV_ALIGN_OUT_BOTTOM_MID, 0, 5);
    lv_obj_set_flex_flow(view_cont_row, LV_FLEX_FLOW_ROW);
    lv_obj_remove_flag(view_cont_row, (lv_obj_flag_t)(LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_SCROLL_CHAIN_HOR));

    lv_obj_t* ai_hole_panel = lv_obj_create(view_cont_row);

    lv_obj_t * status_cont_col = lv_obj_create(view_cont_row);
    lv_obj_set_size(status_cont_col, 500, 500);
    // lv_obj_align(status_cont_col, LV_ALIGN_TOP_MID, 0, 5);
    lv_obj_set_flex_flow(status_cont_col, LV_FLEX_FLOW_COLUMN);

    labelRoll = lv_label_create(status_cont_col);
    labelPitch = lv_label_create(status_cont_col);
    labelOffx = lv_label_create(status_cont_col);
    labelOffy = lv_label_create(status_cont_col);

    static lv_style_t style_sky_gnd;
    static int32_t column_dsc[] = {100, LV_GRID_TEMPLATE_LAST};   /*1 columns with 100 ps width*/
    static int32_t row_dsc[] = {99, 99, LV_GRID_TEMPLATE_LAST}; /*2 99 px tall rows*/
    lv_style_init(&style_sky_gnd);
    lv_style_set_width(&style_sky_gnd, 100);
    lv_style_set_height(&style_sky_gnd, 198);
    lv_style_set_layout(&style_sky_gnd, LV_LAYOUT_GRID);
    lv_style_set_grid_column_dsc_array(&style_sky_gnd, column_dsc);
    lv_style_set_grid_row_dsc_array(&style_sky_gnd, row_dsc);
    lv_style_set_radius(&style_sky_gnd, 0);
    lv_style_set_border_width(&style_sky_gnd, 0);
    lv_style_set_pad_left(&style_sky_gnd, 0);
    lv_style_set_pad_top(&style_sky_gnd, 0);
    lv_style_set_transform_pivot_x(&style_sky_gnd, 50);   // lv_obj_set_style_transform_pivot_y
    lv_style_set_transform_pivot_y(&style_sky_gnd, 99);

    lv_obj_set_size(ai_hole_panel, 100, 100);
    lv_obj_set_style_radius(ai_hole_panel, LV_RADIUS_CIRCLE, 0);
    lv_obj_remove_flag(ai_hole_panel, (lv_obj_flag_t)(LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_SCROLL_CHAIN_HOR));
    lv_obj_set_style_clip_corner(ai_hole_panel, true, 0); // 儿子超出部分隐藏
    lv_obj_set_style_border_width(ai_hole_panel, 0, 0);

    card = lv_obj_create(ai_hole_panel);
    lv_obj_add_style(card, &style_sky_gnd, 0);
    lv_obj_set_style_pad_row(card, 0, 0);
    lv_obj_set_style_pad_column(card, 0, 0);
    lv_obj_remove_flag(card, (lv_obj_flag_t)(LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_SCROLL_CHAIN_HOR));
    lv_obj_scroll_to_view(card, LV_ANIM_OFF);

    static lv_style_t style_sky;
    lv_style_init(&style_sky);
    lv_style_set_bg_color(&style_sky, lv_color_make(0, 128, 255));
    lv_style_set_bg_grad_color( &style_sky, lv_color_make(195, 225, 255));
    lv_style_set_bg_grad_dir( &style_sky, LV_GRAD_DIR_VER );
    lv_style_set_radius(&style_sky, 0);
    lv_style_set_border_width(&style_sky, 0);

    static lv_style_t style_gnd;
    lv_style_init(&style_gnd);
    lv_style_set_bg_color(&style_gnd, lv_color_make(153, 76, 0));
    lv_style_set_bg_grad_color( &style_gnd, lv_color_make(46, 23, 0));
    lv_style_set_bg_grad_dir( &style_gnd, LV_GRAD_DIR_VER );
    lv_style_set_radius(&style_gnd, 0);
    lv_style_set_border_width(&style_gnd, 0);

    lv_obj_t *sky = lv_obj_create(card);
	lv_obj_set_width(sky, 100);
	lv_obj_set_height(sky, 99);
    lv_obj_add_style(sky, &style_sky, 0);
    lv_obj_set_grid_cell(sky, LV_GRID_ALIGN_CENTER, 0, 1, LV_GRID_ALIGN_CENTER, 0, 1);
    lv_obj_set_style_pad_row(sky, 0, 0);
    lv_obj_set_style_pad_column(sky, 0, 0);

    lv_obj_t *gnd = lv_obj_create(card);
	lv_obj_set_width(gnd, 100);
	lv_obj_set_height(gnd, 99);
    lv_obj_add_style(gnd, &style_gnd, 0);
    lv_obj_set_grid_cell(gnd, LV_GRID_ALIGN_CENTER, 0, 1, LV_GRID_ALIGN_CENTER, 1, 1);
    lv_obj_set_style_pad_row(gnd, 0, 0);
    lv_obj_set_style_pad_column(gnd, 0, 0);

    lv_obj_set_style_opa(card, LV_OPA_COVER, 0);
    lv_obj_center(card);

    guide_lines(ai_hole_panel, card);
    lv_roll_scale(ai_hole_panel, card);


    lv_obj_t * pRollSlider = lv_slider_create(main_cont_col);
    lv_obj_set_width(pRollSlider, lv_pct(95));
    // lv_obj_set_pos(pRollSlider, 0, 180);
    lv_obj_align(pRollSlider, LV_ALIGN_BOTTOM_MID, 0, -20);
    lv_obj_add_event_cb(pRollSlider, rollSlider_event_cb, LV_EVENT_VALUE_CHANGED, NULL);
    lv_slider_set_range(pRollSlider, -1800, 1800);
    lv_slider_set_value(pRollSlider, 0, LV_ANIM_OFF);
    
    lv_obj_center(pRollSlider);

    lv_obj_t * pPitchSlider = lv_slider_create(main_cont_col);
    lv_obj_set_width(pPitchSlider, lv_pct(95));
    lv_obj_set_pos(pPitchSlider, 0, 280);
    lv_obj_align(pPitchSlider, LV_ALIGN_BOTTOM_MID, 0, -20);
    lv_obj_add_event_cb(pPitchSlider, pitchSlider_event_cb, LV_EVENT_VALUE_CHANGED, NULL);
    lv_slider_set_range(pPitchSlider, 49, -49);
    lv_slider_set_value(pPitchSlider, 0, LV_ANIM_OFF);
}

void lv_roll_scale(lv_obj_t* ai_hole_panel, lv_obj_t* card)
{
    lv_obj_t * scale = lv_scale_create(ai_hole_panel);
    lv_obj_set_size(scale, 80, 80);
    lv_scale_set_mode(scale, LV_SCALE_MODE_ROUND_OUTER);
    lv_obj_set_style_line_width(scale, 2, 0);
    lv_obj_set_style_bg_opa(scale, LV_OPA_TRANSP, 0);
    lv_obj_set_style_radius(scale, LV_RADIUS_CIRCLE, 0);
    lv_scale_set_label_show(scale, true);
    lv_scale_set_total_tick_count(scale, 19);
    lv_scale_set_major_tick_every(scale, 3);
    lv_obj_set_style_length(scale, 5, LV_PART_ITEMS);
    lv_obj_set_style_length(scale, 10, LV_PART_INDICATOR);
    lv_scale_set_range(scale, -90, 90);
    lv_scale_set_angle_range(scale, 90);
    lv_scale_set_rotation(scale, 225);

    lv_obj_align(scale, LV_ALIGN_CENTER, 0, 0);
}

void guide_lines(lv_obj_t* ai_hole_panel, lv_obj_t* card)
{
    static lv_style_t style_line;
    lv_style_init(&style_line);
    lv_style_set_line_width(&style_line, 2);
    lv_style_set_line_color(&style_line, lv_palette_main(LV_PALETTE_RED));
    lv_style_set_line_rounded(&style_line, true);

    static lv_style_t style_line_type;
    lv_style_init(&style_line_type);
    lv_style_set_line_width(&style_line_type, 2);
    lv_style_set_line_color(&style_line_type, lv_palette_lighten(LV_PALETTE_GREY, 5));
    lv_style_set_line_rounded(&style_line_type, true);

    static lv_style_t style_line_type1;
    lv_style_init(&style_line_type1);
    lv_style_set_line_width(&style_line_type1, 1);
    lv_style_set_line_color(&style_line_type1, lv_palette_lighten(LV_PALETTE_GREY, 2));
    lv_style_set_line_rounded(&style_line_type1, true);

    static lv_point_precise_t line_points[] = { 
        {36,   38}, {40,   38}, // 指向红心 + 0
        {38,   36}, {38,   40}, // 指向红心 + 2
        {35,   99}, {65,   99}, // base line 0  + 4
        {45,  107}, {55,  107}, // base line 1  + 6
        {40,  115}, {60,  115}, // base line 2  + 8
        {45,  124}, {55,  124}, // base line 3  + 10
        {40,  132}, {60,  132}, // base line 4  + 12
        {45,  140}, {55,  140}, // base line 5  + 14
        {40,  148}, {60,  148}, // base line 6  + 16


        {45,   90}, {55,   90}, // base line-1  + 18
        {40,   82}, {60,   82}, // base line-2  + 20
        {45,   74}, {55,   74}, // base line-3  + 22
        {40,   66}, {60,   66}, // base line-1  + 24
        {45,   58}, {55,   58}, // base line-2  + 26
        {40,   50}, {60,   50}  // base line-3  + 28
         };



    lv_obj_t * lineBaseA = lv_line_create(ai_hole_panel);
    lv_line_set_points(lineBaseA, line_points, 2);
    lv_obj_add_style(lineBaseA, &style_line, 0);

    lv_obj_t * lineBaseB = lv_line_create(ai_hole_panel);
    lv_line_set_points(lineBaseB, line_points + 2, 2);
    lv_obj_add_style(lineBaseB, &style_line, 0);

    lv_obj_t * line0 = lv_line_create(card);
    lv_line_set_points(line0, line_points + 4, 2);
    lv_obj_add_style(line0, &style_line_type, 0);

    lv_obj_t * line1 = lv_line_create(card);
    lv_line_set_points(line1, line_points + 6, 2);
    lv_style_set_line_color(&style_line_type, lv_palette_lighten(LV_PALETTE_GREY, 1));
    lv_obj_add_style(line1, &style_line_type1, 0);

    lv_obj_t * line2 = lv_line_create(card);
    lv_line_set_points(line2, line_points + 8, 2);
    lv_obj_add_style(line2, &style_line_type1, 0);

    lv_obj_t * line3 = lv_line_create(card);
    lv_line_set_points(line3, line_points + 10, 2);
    lv_obj_add_style(line3, &style_line_type1, 0);

    lv_obj_t * line4 = lv_line_create(card);
    lv_line_set_points(line4, line_points + 12, 2);
    lv_obj_add_style(line4, &style_line_type1, 0);

    lv_obj_t * line5 = lv_line_create(card);
    lv_line_set_points(line5, line_points + 14, 2);
    lv_obj_add_style(line5, &style_line_type1, 0);

    lv_obj_t * line6 = lv_line_create(card);
    lv_line_set_points(line6, line_points + 16, 2);
    lv_obj_add_style(line6, &style_line_type1, 0);

    line1 = lv_line_create(card);
    lv_line_set_points(line1, line_points + 18, 2);
    lv_style_set_line_color(&style_line_type, lv_palette_lighten(LV_PALETTE_GREY, 1));
    lv_obj_add_style(line1, &style_line_type1, 0);

    line2 = lv_line_create(card);
    lv_line_set_points(line2, line_points + 20, 2);
    lv_obj_add_style(line2, &style_line_type1, 0);

    line3 = lv_line_create(card);
    lv_line_set_points(line3, line_points + 22, 2);
    lv_obj_add_style(line3, &style_line_type1, 0);

    line4 = lv_line_create(card);
    lv_line_set_points(line4, line_points + 24, 2);
    lv_obj_add_style(line4, &style_line_type1, 0);

    line5 = lv_line_create(card);
    lv_line_set_points(line5, line_points + 26, 2);
    lv_obj_add_style(line5, &style_line_type1, 0);

    line6 = lv_line_create(card);
    lv_line_set_points(line6, line_points + 28, 2);
    lv_obj_add_style(line6, &style_line_type1, 0);


}

/* sin() * 1000 for 0..90 degrees, 1 degree steps (used by sin_milli()) */
static const int16_t sin_table_0_90[91] = {
       0,   17,   35,   52,   70,   87,  105,  122,  139,  156,  174,  191,  208,  225,  242,
     259,  276,  292,  309,  326,  342,  358,  375,  391,  407,  423,  438,  454,  469,  485,
     500,  515,  530,  545,  559,  574,  588,  602,  616,  629,  643,  656,  669,  682,  695,
     707,  719,  731,  743,  755,  766,  777,  788,  799,  809,  819,  829,  839,  848,  857,
     866,  875,  883,  891,  899,  906,  914,  921,  927,  934,  940,  946,  951,  956,  961,
     966,  970,  974,  978,  982,  985,  988,  990,  993,  995,  996,  998,  999,  999, 1000,
    1000
};

/* Linear interpolation on the 0..90 degree table, deci_deg is in 0.1 degree units (0..900) */
static int32_t sin_interp(int32_t deci_deg)
{
    int32_t index = deci_deg / 10;
    int32_t frac = deci_deg % 10;
    if(index >= 90) {
        return sin_table_0_90[90];
    }
    return (sin_table_0_90[index] * (10 - frac) + sin_table_0_90[index + 1] * frac) / 10;
}

/* sin() * 1000 for any angle, deci_deg is in 0.1 degree units */
static int32_t sin_milli(int32_t deci_deg)
{
    int32_t a = deci_deg % 3600;
    if(a < 0) {
        a += 3600;
    }
    if(a <= 900) {
        return sin_interp(a);
    }
    if(a <= 1800) {
        return sin_interp(1800 - a);
    }
    if(a <= 2700) {
        return -sin_interp(a - 1800);
    }
    return -sin_interp(3600 - a);
}

int32_t angle = 0;
int32_t pitch = 0;

void update_ai()
{
    lv_obj_set_style_transform_rotation(card, angle, 0);

    /* angle is in 0.1 degree units. The maths below is the integer version of the
     * original sin()/cos() code, so no soft float formatting is needed. */
    int32_t abs_angle = angle < 0 ? -angle : angle;
    int32_t deg = abs_angle / 10;
    int32_t sin_v = sin_milli(abs_angle);           /* 0 .. 1000 */
    int32_t cos_v = sin_milli(abs_angle + 900);     /* -1000 .. 1000 */
    int32_t roll_sign = angle > 0 ? 1 : -1;
    int32_t cos_abs = cos_v < 0 ? -cos_v : cos_v;

    int32_t x_offset = pitch * roll_sign * sin_v / 1000;
    int32_t y_offset = -(pitch * cos_v) / 1000;

    lv_label_set_text_fmt(labelRoll, "roll: %d", (int)angle);
    lv_label_set_text_fmt(labelPitch, "roll: %d pitch: %d", (int)angle, (int)pitch);
    lv_label_set_text_fmt(labelOffx, "offsetX: %d %d %d.%03d",
                          (int)x_offset, (int)deg, (int)(sin_v / 1000), (int)(sin_v % 1000));
    lv_label_set_text_fmt(labelOffy, "offsetY: %d %d %s%d.%03d",
                          (int)y_offset, (int)deg, cos_v < 0 ? "-" : "",
                          (int)(cos_abs / 1000), (int)(cos_abs % 1000));

    /* translate_x/y offsets the laid out (centered) position; lv_obj_set_pos() would
     * replace it and make the card jump on the first slider move */
    lv_obj_set_style_translate_x(card, x_offset, 0);
    lv_obj_set_style_translate_y(card, y_offset, 0);
}


static void rollSlider_event_cb(lv_event_t * e)
{
    lv_obj_t * slider = (lv_obj_t *)lv_event_get_target(e);
    angle = lv_slider_get_value(slider);

    update_ai();
}

static void pitchSlider_event_cb(lv_event_t * e)
{
    lv_obj_t * slider = (lv_obj_t *)lv_event_get_target(e);
    pitch = lv_slider_get_value(slider);
    update_ai();    
}

