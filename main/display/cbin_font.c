#include "cbin_font.h"
#include <string.h>
#include <esp_log.h>

#define TAG "CBinFont"

/**
 * LVGL 9.x built-in font format callbacks (declared in lv_font_fmt_txt.h)
 * We declare them here to avoid pulling in the full internal header.
 */
bool lv_font_get_glyph_dsc_fmt_txt(const lv_font_t* font, lv_font_glyph_dsc_t* dsc_out,
                                    uint32_t unicode_letter, uint32_t unicode_letter_next);
const void* lv_font_get_bitmap_fmt_txt(lv_font_glyph_dsc_t* g_dsc, lv_draw_buf_t* draw_buf);

/* =========================================================================
 * Image CBin Loader
 * ========================================================================= */

lv_img_dsc_t* cbin_img_dsc_create(uint8_t* bin_addr) {
    if (!bin_addr) {
        ESP_LOGE(TAG, "cbin_img_dsc_create: null bin_addr");
        return NULL;
    }

    lv_img_dsc_t* img = (lv_img_dsc_t*)lv_malloc(sizeof(lv_img_dsc_t));
    if (!img) {
        ESP_LOGE(TAG, "cbin_img_dsc_create: malloc failed");
        return NULL;
    }

    /* Copy the image header from CBin data */
    memcpy(img, bin_addr, sizeof(lv_img_dsc_t));

    /* Fix up the data pointer: stored as offset from bin_addr base */
    if (img->data) {
        img->data = (const uint8_t*)bin_addr + (uintptr_t)(img->data);
    }

    return img;
}

/* =========================================================================
 * Font CBin Loader
 * ========================================================================= */

lv_font_t* cbin_font_create(uint8_t* bin_addr) {
    if (!bin_addr) {
        ESP_LOGE(TAG, "cbin_font_create: null bin_addr");
        return NULL;
    }

    lv_font_t* font = (lv_font_t*)lv_malloc(sizeof(lv_font_t));
    if (!font) {
        ESP_LOGE(TAG, "cbin_font_create: malloc font failed");
        return NULL;
    }

    /* Copy font header from CBin data */
    memcpy(font, bin_addr, sizeof(lv_font_t));

    /* Fix up dsc pointer: stored as offset from bin_addr base */
    if (font->dsc) {
        const lv_font_fmt_txt_dsc_t* dsc_in_flash =
            (const lv_font_fmt_txt_dsc_t*)(bin_addr + (uintptr_t)(font->dsc));

        /* Allocate a mutable copy of the font descriptor */
        lv_font_fmt_txt_dsc_t* dsc_copy =
            (lv_font_fmt_txt_dsc_t*)lv_malloc(sizeof(lv_font_fmt_txt_dsc_t));
        if (!dsc_copy) {
            ESP_LOGE(TAG, "cbin_font_create: malloc dsc failed");
            lv_free(font);
            return NULL;
        }
        memcpy(dsc_copy, dsc_in_flash, sizeof(lv_font_fmt_txt_dsc_t));

        /* Fix up child pointers (offsets → absolute addresses in flash) */
        if (dsc_copy->glyph_dsc) {
            dsc_copy->glyph_dsc = (const void*)(bin_addr + (uintptr_t)(dsc_copy->glyph_dsc));
        }
        if (dsc_copy->cmaps) {
            dsc_copy->cmaps = (const void*)(bin_addr + (uintptr_t)(dsc_copy->cmaps));
        }
        if (dsc_copy->kern_dsc) {
            dsc_copy->kern_dsc = (const void*)(bin_addr + (uintptr_t)(dsc_copy->kern_dsc));
        }

        font->dsc = dsc_copy;
    }

    /* Fallback font pointer: also stored as offset */
    if (font->fallback) {
        font->fallback = (const lv_font_t*)(bin_addr + (uintptr_t)(font->fallback));
    }

    /* Set LVGL 9.x built-in font format callbacks */
    font->get_glyph_dsc = lv_font_get_glyph_dsc_fmt_txt;
    font->get_glyph_bitmap = lv_font_get_bitmap_fmt_txt;
    font->release_glyph = NULL;

    return font;
}

void cbin_font_delete(lv_font_t* font) {
    if (!font) return;

    if (font->dsc) {
        lv_free((void*)font->dsc);
    }
    lv_free(font);
}
