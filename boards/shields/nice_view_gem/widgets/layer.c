#include <zephyr/kernel.h>
#include <stdio.h>
#include <inttypes.h>
#include <string.h>

#include "layer.h"
#include "../assets/custom_fonts.h"
#include <zmk/physical_layouts.h>
#include <zmk/keymap.h>
#include <zmk/matrix.h>

LV_IMG_DECLARE(pegasus);

#define BASE_LAYER_INDEX 0

void draw_layer_status(lv_obj_t *canvas, const struct status_state *state) {
    /* BASE: show pegasus icon instead of the layer name text. */
    if (state->layer_index == BASE_LAYER_INDEX) {
        lv_draw_image_dsc_t img_dsc;
        lv_draw_image_dsc_init(&img_dsc);
        /* 72x62 icon centered in the middle of the 144x168 panel. */
        const lv_coord_t x = (SCREEN_WIDTH - 72) / 2;
        const lv_coord_t y = 52;
        canvas_draw_img(canvas, x, y, &pegasus, &img_dsc);
        return;
    }

    lv_draw_label_dsc_t label_dsc;
    init_label_dsc(&label_dsc, LVGL_FOREGROUND, &quinquefive_24, LV_TEXT_ALIGN_CENTER);

    char fallback_layer_name[16];

    const char *layer_name =
        zmk_keymap_layer_name(zmk_keymap_layer_index_to_id(state->layer_index));

    if (layer_name == NULL || layer_name[0] == '\0') {
        sprintf(fallback_layer_name, "L#%" PRIu8, state->layer_index);
        layer_name = fallback_layer_name;
    }

    canvas_draw_text(canvas, 0, 70, SCREEN_WIDTH, &label_dsc, layer_name);
}
