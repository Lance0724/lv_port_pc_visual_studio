#include "ui.h"




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

