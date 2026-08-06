#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <inttypes.h>

#include "util.h"
#include "../assets/custom_fonts.h"

/* One LVGL layer per status frame — avoids init/finish per primitive. */
static lv_obj_t *frame_canvas;
static lv_layer_t frame_layer;
static bool frame_active;

void canvas_frame_begin(lv_obj_t *canvas)
{
	frame_canvas = canvas;
	lv_canvas_init_layer(canvas, &frame_layer);
	frame_active = true;
}

void canvas_frame_end(void)
{
	if (frame_active && frame_canvas != NULL) {
		lv_canvas_finish_layer(frame_canvas, &frame_layer);
	}
	frame_active = false;
	frame_canvas = NULL;
}

void fill_background(lv_obj_t *canvas)
{
	lv_canvas_fill_bg(canvas, LVGL_BACKGROUND, LV_OPA_COVER);
}

void init_label_dsc(lv_draw_label_dsc_t *label_dsc, lv_color_t color, const lv_font_t *font,
		    lv_text_align_t align)
{
	lv_draw_label_dsc_init(label_dsc);
	label_dsc->color = color;
	label_dsc->font = font;
	label_dsc->align = align;
	/* Frame-batched canvas draws flush at canvas_frame_end(); copy text so
	 * stack buffers (battery %, layer fallback) stay valid until then. */
	label_dsc->text_local = true;
}

void init_rect_dsc(lv_draw_rect_dsc_t *rect_dsc, lv_color_t bg_color)
{
	lv_draw_rect_dsc_init(rect_dsc);
	rect_dsc->bg_color = bg_color;
}

void init_line_dsc(lv_draw_line_dsc_t *line_dsc, lv_color_t color, uint8_t width)
{
	lv_draw_line_dsc_init(line_dsc);
	line_dsc->color = color;
	line_dsc->width = width;
}

static lv_layer_t *draw_layer(lv_obj_t *canvas, lv_layer_t *scratch)
{
	if (frame_active && frame_canvas == canvas) {
		return &frame_layer;
	}

	lv_canvas_init_layer(canvas, scratch);
	return scratch;
}

static void finish_scratch(lv_obj_t *canvas, lv_layer_t *scratch, lv_layer_t *used)
{
	if (used == scratch) {
		lv_canvas_finish_layer(canvas, scratch);
	}
}

void canvas_draw_rect(lv_obj_t *canvas, lv_coord_t x, lv_coord_t y, lv_coord_t w, lv_coord_t h,
		      lv_draw_rect_dsc_t *draw_dsc)
{
	lv_layer_t scratch;
	lv_layer_t *layer = draw_layer(canvas, &scratch);
	lv_area_t coords = {x, y, x + w - 1, y + h - 1};

	lv_draw_rect(layer, draw_dsc, &coords);
	finish_scratch(canvas, &scratch, layer);
}

void canvas_draw_text(lv_obj_t *canvas, lv_coord_t x, lv_coord_t y, lv_coord_t max_w,
		      lv_draw_label_dsc_t *draw_dsc, const char *txt)
{
	lv_layer_t scratch;
	lv_layer_t *layer = draw_layer(canvas, &scratch);
	lv_area_t coords = {x, y, x + max_w, y + SCREEN_HEIGHT};

	draw_dsc->text = txt;
	lv_draw_label(layer, draw_dsc, &coords);
	finish_scratch(canvas, &scratch, layer);
}

void canvas_draw_img(lv_obj_t *canvas, lv_coord_t x, lv_coord_t y, const lv_image_dsc_t *src,
		     lv_draw_image_dsc_t *draw_dsc)
{
	lv_layer_t scratch;
	lv_layer_t *layer = draw_layer(canvas, &scratch);
	lv_area_t coords = {x, y, x + src->header.w - 1, y + src->header.h - 1};

	draw_dsc->src = src;
	lv_draw_image(layer, draw_dsc, &coords);
	finish_scratch(canvas, &scratch, layer);
}

void canvas_draw_battery_chip(lv_obj_t *canvas, lv_coord_t x, lv_coord_t y, char side,
			      uint8_t level, bool charging)
{
	lv_draw_rect_dsc_t box_dsc;
	lv_draw_rect_dsc_t ink_dsc;
	lv_draw_label_dsc_t pct_dsc;
	char pct[10];
	/* Identical chip geometry for L and R (shared Y from callers). */
	const lv_coord_t chip = 12;
	const lv_coord_t gly_x = x + 2;
	const lv_coord_t gly_y = y + 2;
	/*
	 * 8×8 bitmaps (MSB = left). Same cell size keeps L/R visually matched.
	 * Rows top→bottom.
	 */
	static const uint8_t glyph_l[8] = {
		0b10000000, 0b10000000, 0b10000000, 0b10000000,
		0b10000000, 0b10000000, 0b10000000, 0b11111110,
	};
	static const uint8_t glyph_r[8] = {
		0b11111100, 0b10000010, 0b10000010, 0b11111100,
		0b10010000, 0b10001000, 0b10000100, 0b10000010,
	};
	const uint8_t *glyph = (side == 'L' || side == 'l') ? glyph_l : glyph_r;

	init_rect_dsc(&box_dsc, LVGL_FOREGROUND);
	canvas_draw_rect(canvas, x, y, chip, chip, &box_dsc);

	init_rect_dsc(&ink_dsc, LVGL_BACKGROUND);
	for (lv_coord_t row = 0; row < 8; row++) {
		uint8_t bits = glyph[row];

		for (lv_coord_t col = 0; col < 8; col++) {
			if (bits & (0x80 >> col)) {
				canvas_draw_rect(canvas, gly_x + col, gly_y + row, 1, 1, &ink_dsc);
			}
		}
	}

	if (charging) {
		snprintk(pct, sizeof(pct), "%" PRIu8 "%%+", level);
	} else {
		snprintk(pct, sizeof(pct), "%" PRIu8 "%%", level);
	}
	init_label_dsc(&pct_dsc, LVGL_FOREGROUND, &quinquefive_8, LV_TEXT_ALIGN_LEFT);
	canvas_draw_text(canvas, x + chip + 2, y + 2, 48, &pct_dsc, pct);
}
