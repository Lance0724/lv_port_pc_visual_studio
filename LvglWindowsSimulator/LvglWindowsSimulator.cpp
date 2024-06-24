#include <Windows.h>

#include <LvglWindowsIconResource.h>

#include "lvgl/lvgl.h"
#include "lvgl/examples/lv_examples.h"
#include "lvgl/demos/lv_demos.h"

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

    while (1)
    {
        uint32_t time_till_next = lv_timer_handler();
        Sleep(time_till_next);
    }

    return 0;
}

#define CANVAS_WIDTH  200
#define CANVAS_HEIGHT  250
static lv_style_t style_sky;
static lv_style_t style_ground;
lv_obj_t * card;

void init_anim(lv_obj_t * card);

void vhud()
{
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
    // lv_style_set_radius(&style_sky_gnd, LV_RADIUS_CIRCLE); // Display rectangle as circel

    card = lv_obj_create(lv_screen_active());
    lv_obj_add_style(card, &style_sky_gnd, 0);
    lv_obj_set_style_pad_row(card, 0, 0);
    lv_obj_set_style_pad_column(card, 0, 0);
    lv_obj_remove_flag(card, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_SCROLL_CHAIN_HOR);
    lv_obj_scroll_to_view(card, LV_ANIM_OFF);

    static lv_style_t style_sky;
    lv_style_init(&style_sky);
    lv_style_set_bg_color(&style_sky, lv_color_make(0, 128, 255));
    // lv_style_set_bg_color(&style_sky, lv_color_make(255, 0, 0));
    lv_style_set_radius(&style_sky, 0);
    lv_style_set_border_width(&style_sky, 0);

    static lv_style_t style_gnd;
    lv_style_init(&style_gnd);
    lv_style_set_bg_color(&style_gnd, lv_color_make(153, 76, 0));
    lv_style_set_radius(&style_gnd, 0);
    lv_style_set_border_width(&style_gnd, 0);

    // lv_obj_t * foo = lv_label_create(card);
    // lv_label_set_text(foo, "foo");
    // lv_obj_add_style(foo, &style_sky, 0);
    // lv_obj_set_grid_cell(foo, LV_GRID_ALIGN_CENTER, 0, 1, LV_GRID_ALIGN_CENTER, 0, 1);
    // lv_obj_set_style_text_font(foo, &lv_font_montserrat_18, 0);

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


    init_anim(card);
}


static void anim_canvas_cb(void* var, int32_t deg)
{
    lv_obj_set_style_transform_rotation(card, deg * 10, 0);
}

void init_anim(lv_obj_t * card)
{
    lv_anim_t anim_canvas;
    lv_anim_init(&anim_canvas);
    lv_anim_set_var(&anim_canvas, NULL);
    lv_anim_set_values(&anim_canvas, -90,90);
    lv_anim_set_time(&anim_canvas, 5000);
    lv_anim_set_exec_cb(&anim_canvas, anim_canvas_cb);
    lv_anim_set_path_cb(&anim_canvas, lv_anim_path_ease_in_out);
    lv_anim_set_repeat_delay(&anim_canvas, 1000);
    lv_anim_set_repeat_count(&anim_canvas, LV_ANIM_REPEAT_INFINITE);
    lv_anim_start(&anim_canvas);
}


void vhud2()
{
    lv_draw_rect_dsc_t sky_rect_dsc;
    lv_draw_rect_dsc_init(&sky_rect_dsc);
    sky_rect_dsc.radius = 0;
    sky_rect_dsc.bg_opa = LV_OPA_COVER;
    sky_rect_dsc.bg_color = lv_color_make(0, 128, 255); // (0, 128, 255) SKY_BLUE   (153, 76, 0) BROWN

    lv_draw_rect_dsc_t gnd_rect_dsc;
    lv_draw_rect_dsc_init(&gnd_rect_dsc);
    gnd_rect_dsc.radius = 0;
    gnd_rect_dsc.bg_opa = LV_OPA_COVER;
    gnd_rect_dsc.bg_color = lv_color_make(153, 76, 0); // (0, 128, 255) SKY_BLUE   (153, 76, 0) BROWN

    LV_DRAW_BUF_DEFINE(draw_buf_16bpp, CANVAS_WIDTH, CANVAS_HEIGHT, LV_COLOR_FORMAT_RGB565);

    lv_obj_t * canvas = lv_canvas_create(lv_screen_active());
    lv_canvas_set_draw_buf(canvas, &draw_buf_16bpp);
    lv_obj_center(canvas);
    lv_canvas_fill_bg(canvas, lv_palette_lighten(LV_PALETTE_GREY, 3), LV_OPA_COVER);

    lv_layer_t layer;
    lv_canvas_init_layer(canvas, &layer);

    lv_area_t coords_sky_rect = {30, 20, 130, 119};
    lv_draw_rect(&layer, &sky_rect_dsc, &coords_sky_rect);

    lv_area_t coords_gnd_rect = {30, 120, 130, 218};
    lv_draw_rect(&layer, &gnd_rect_dsc, &coords_gnd_rect);

    // lv_obj_set_style_transform_rotation(&layer, 90 * 10, 0);

    lv_canvas_finish_layer(canvas, &layer);
}