/*
 * TARS uses LVGL's 8 px UNSCII bitmap font for the dense flight-log column.
 * Build a private copy for this board so enabling the font does not change
 * the LVGL configuration used by other boards.
 */
#define LV_FONT_UNSCII_8 1
#include "../../../managed_components/lvgl__lvgl/src/font/lv_font_unscii_8.c"
