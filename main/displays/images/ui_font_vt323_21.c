/*******************************************************************************
 * Size: 21 px
 * Bpp: 1
 * Opts: --bpp 1 --size 21 --no-compress --font VT323-Regular.ttf --range 32-127 --format lvgl -o ui_font_vt323_21.c
 ******************************************************************************/

#ifdef LV_LVGL_H_INCLUDE_SIMPLE
#include "lvgl.h"
#else
#include "lvgl/lvgl.h"
#endif

#ifndef UI_FONT_VT323_21
#define UI_FONT_VT323_21 1
#endif

#if UI_FONT_VT323_21

/*-----------------
 *    BITMAPS
 *----------------*/

/*Store the image of the glyphs*/
static LV_ATTRIBUTE_LARGE_CONST const uint8_t glyph_bitmap[] = {
    /* U+0020 " " */
    0x0,

    /* U+0021 "!" */
    0xff, 0xff, 0xc3, 0xc0,

    /* U+0022 "\"" */
    0xde, 0xf6,

    /* U+0023 "#" */
    0x6c, 0xdb, 0xff, 0xf6, 0xcd, 0x9b, 0x7f, 0xfe,
    0xd9, 0xb0,

    /* U+0024 "$" */
    0x30, 0x61, 0xf3, 0xe7, 0x5e, 0xfd, 0xf8, 0x7c,
    0xf8, 0xdf, 0xbf, 0x6f, 0x9f, 0x18, 0x30,

    /* U+0025 "%" */
    0x66, 0xcf, 0xb7, 0x67, 0x8f, 0x4, 0x8, 0x2c,
    0xfd, 0xfe, 0x6c, 0xc0,

    /* U+0026 "&" */
    0x38, 0x71, 0xe3, 0xc3, 0x66, 0xdf, 0xac, 0xd9,
    0xbf, 0x7b, 0xb7, 0x60,

    /* U+0027 "'" */
    0xfc,

    /* U+0028 "(" */
    0x29, 0x25, 0xb6, 0xd9, 0x24, 0x89,

    /* U+0029 ")" */
    0x84, 0x42, 0x23, 0x33, 0x33, 0x22, 0x44, 0x88,

    /* U+002A "*" */
    0x30, 0xcf, 0xff, 0x30, 0xcf, 0xff, 0x30,

    /* U+002B "+" */
    0x30, 0xc3, 0xc, 0xff, 0xf3, 0xc, 0x30,

    /* U+002C "," */
    0xff, 0xb7, 0xb0,

    /* U+002D "-" */
    0xff,

    /* U+002E "." */
    0xfc,

    /* U+002F "/" */
    0x6, 0x18, 0x30, 0x60, 0x83, 0x6, 0x8, 0x10,
    0x60, 0x83, 0x6, 0xc, 0x30, 0x60,

    /* U+0030 "0" */
    0x30, 0xc4, 0x92, 0xcf, 0x3c, 0xf3, 0xcd, 0x24,
    0x8c, 0x30,

    /* U+0031 "1" */
    0x30, 0xc7, 0x1c, 0x30, 0xc3, 0xc, 0x30, 0xc3,
    0x3f, 0x7c,

    /* U+0032 "2" */
    0x79, 0xe4, 0xb3, 0xcc, 0x31, 0x86, 0x31, 0x86,
    0x3f, 0xfc,

    /* U+0033 "3" */
    0x79, 0xe4, 0xb3, 0xcc, 0x33, 0x8e, 0xf, 0x3c,
    0xde, 0x78,

    /* U+0034 "4" */
    0x1c, 0x1c, 0x2c, 0x2c, 0x2c, 0x2c, 0x4c, 0x4c,
    0xfe, 0x7e, 0xc, 0xc, 0xc,

    /* U+0035 "5" */
    0xfd, 0xfb, 0x6, 0xd, 0x9b, 0x3b, 0x76, 0x7,
    0x9b, 0x33, 0xc7, 0x80,

    /* U+0036 "6" */
    0x1c, 0x76, 0x18, 0xc3, 0xd, 0xf7, 0xed, 0xb6,
    0xc6, 0x18,

    /* U+0037 "7" */
    0xff, 0xf0, 0xc3, 0x8, 0x21, 0x84, 0x30, 0xc3,
    0x18, 0x60,

    /* U+0038 "8" */
    0x79, 0xe4, 0xb3, 0xcf, 0x37, 0x9e, 0xcf, 0x3c,
    0xde, 0x78,

    /* U+0039 "9" */
    0x30, 0xc4, 0x33, 0xcf, 0x37, 0xdf, 0xc, 0x30,
    0xdc, 0x70,

    /* U+003A ":" */
    0xfc, 0x3, 0xf0,

    /* U+003B ";" */
    0xff, 0x80, 0x7, 0xfd, 0xbd, 0x80,

    /* U+003C "<" */
    0x18, 0xcc, 0x66, 0x33, 0x18, 0x61, 0x8c, 0x31,
    0x80,

    /* U+003D "=" */
    0xff, 0xc0, 0xf, 0xfc,

    /* U+003E ">" */
    0xc6, 0x18, 0xc3, 0x18, 0x63, 0x33, 0x19, 0x8c,
    0x0,

    /* U+003F "?" */
    0x78, 0xf1, 0x26, 0x3c, 0x60, 0xc6, 0xc, 0x30,
    0x60, 0x1, 0x83, 0x0,

    /* U+0040 "@" */
    0x38, 0xd9, 0xb6, 0xfd, 0xfa, 0xf5, 0xeb, 0xd7,
    0xbd, 0x83, 0x3, 0xc7, 0x80,

    /* U+0041 "A" */
    0x30, 0xf1, 0xe3, 0x46, 0xcd, 0x9b, 0x36, 0x7c,
    0xfb, 0x1e, 0x3c, 0x60,

    /* U+0042 "B" */
    0xfd, 0xfb, 0x16, 0x3c, 0x78, 0xff, 0x7e, 0xc7,
    0x8f, 0x1f, 0xef, 0xc0,

    /* U+0043 "C" */
    0x3c, 0x79, 0x9b, 0x3c, 0x18, 0x30, 0x60, 0xc0,
    0xcd, 0x99, 0xe3, 0xc0,

    /* U+0044 "D" */
    0xf9, 0xf3, 0x36, 0x6c, 0x78, 0xf1, 0xe3, 0xc7,
    0x9b, 0x37, 0xcf, 0x80,

    /* U+0045 "E" */
    0xff, 0xf1, 0x8c, 0x63, 0xff, 0xc6, 0x31, 0xff,
    0x80,

    /* U+0046 "F" */
    0xff, 0xf1, 0x8c, 0x63, 0xff, 0xc6, 0x31, 0x8c,
    0x0,

    /* U+0047 "G" */
    0x3c, 0x79, 0x9b, 0x3c, 0x18, 0x33, 0xe7, 0xc6,
    0xcd, 0x99, 0xe3, 0xc0,

    /* U+0048 "H" */
    0xc7, 0x8f, 0x1e, 0x3c, 0x78, 0xff, 0xff, 0xc7,
    0x8f, 0x1e, 0x3c, 0x60,

    /* U+0049 "I" */
    0xff, 0x66, 0x66, 0x66, 0x66, 0x6f, 0xf0,

    /* U+004A "J" */
    0x3c, 0xf1, 0x86, 0x18, 0x61, 0x86, 0x1b, 0x6d,
    0x9e, 0x78,

    /* U+004B "K" */
    0xc7, 0x8f, 0x36, 0x6d, 0x9b, 0x3c, 0x78, 0xd9,
    0x9b, 0x36, 0x3c, 0x60,

    /* U+004C "L" */
    0xc3, 0xc, 0x30, 0xc3, 0xc, 0x30, 0xc3, 0xc,
    0x3f, 0xfc,

    /* U+004D "M" */
    0xc7, 0x8f, 0xbf, 0x7e, 0xfd, 0xff, 0xff, 0xf7,
    0xef, 0xde, 0x3c, 0x60,

    /* U+004E "N" */
    0xc7, 0x8f, 0x1f, 0x3f, 0x7e, 0xfd, 0xeb, 0xdf,
    0x9f, 0x3e, 0x3c, 0x60,

    /* U+004F "O" */
    0x78, 0xf1, 0xb3, 0x6c, 0x78, 0xf1, 0xe3, 0xc6,
    0xd9, 0xb3, 0xc7, 0x80,

    /* U+0050 "P" */
    0xfd, 0xfb, 0x16, 0x3c, 0x78, 0xff, 0x7e, 0xc1,
    0x83, 0x6, 0xc, 0x0,

    /* U+0051 "Q" */
    0x78, 0xf1, 0xa3, 0x6c, 0x78, 0xf1, 0xe3, 0xc6,
    0xd9, 0xb3, 0xc7, 0x81, 0x83, 0x3, 0x6,

    /* U+0052 "R" */
    0xfd, 0xfb, 0x3e, 0x7c, 0xd9, 0xbe, 0x7c, 0xcd,
    0x9b, 0x36, 0x3c, 0x60,

    /* U+0053 "S" */
    0x7c, 0xf9, 0x16, 0x3c, 0x78, 0x1f, 0x3e, 0x7,
    0x8f, 0x1b, 0xe7, 0xc0,

    /* U+0054 "T" */
    0xfe, 0x7e, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18,
    0x18, 0x18, 0x18, 0x18, 0x18,

    /* U+0055 "U" */
    0xc7, 0x8f, 0x1e, 0x3c, 0x78, 0xf1, 0xe3, 0xc7,
    0x8f, 0x1b, 0xe7, 0xc0,

    /* U+0056 "V" */
    0xc7, 0x8f, 0x1e, 0x3c, 0x6d, 0x9b, 0x36, 0x6c,
    0xd9, 0xe0, 0x81, 0x0,

    /* U+0057 "W" */
    0xd7, 0xaf, 0x5e, 0xbd, 0x7a, 0xf5, 0xaa, 0x74,
    0xe9, 0xb3, 0x66, 0xc0,

    /* U+0058 "X" */
    0xc7, 0x8d, 0x13, 0x66, 0xcd, 0x8c, 0x18, 0x6c,
    0xd9, 0xb6, 0x3c, 0x60,

    /* U+0059 "Y" */
    0xc7, 0x8f, 0x1a, 0x26, 0xcd, 0x9e, 0x18, 0x30,
    0x60, 0xc1, 0x83, 0x0,

    /* U+005A "Z" */
    0xff, 0xf0, 0xc3, 0x18, 0x63, 0xc, 0x63, 0xc,
    0x3f, 0xfc,

    /* U+005B "[" */
    0xff, 0x6d, 0xb6, 0xdb, 0x6d, 0xbf,

    /* U+005C "\\" */
    0xc0, 0xc1, 0x83, 0x2, 0x6, 0xc, 0x8, 0x10,
    0x30, 0x20, 0x60, 0xc1, 0x81, 0x83,

    /* U+005D "]" */
    0xfd, 0xb6, 0xdb, 0x6d, 0xb6, 0xff,

    /* U+005E "^" */
    0x30, 0x61, 0xe3, 0x66, 0xd8, 0xf1, 0x80,

    /* U+005F "_" */
    0xff, 0xfc,

    /* U+0060 "`" */
    0xd9, 0xb0,

    /* U+0061 "a" */
    0x78, 0xf0, 0x30, 0x67, 0xd9, 0xb3, 0x3f, 0x7e,

    /* U+0062 "b" */
    0xc1, 0x83, 0x6, 0xd, 0x9b, 0x3b, 0x76, 0xc7,
    0x9b, 0x35, 0x8b, 0x0,

    /* U+0063 "c" */
    0x3c, 0x79, 0x9b, 0x3c, 0xc, 0xd9, 0x9e, 0x3c,

    /* U+0064 "d" */
    0x6, 0xc, 0x18, 0x33, 0x66, 0xf1, 0xe3, 0xc7,
    0x8f, 0x19, 0xb3, 0x60,

    /* U+0065 "e" */
    0x3c, 0x79, 0x9b, 0x3f, 0xef, 0xd8, 0x1e, 0x3c,

    /* U+0066 "f" */
    0x7b, 0xd8, 0xcf, 0xfd, 0x8c, 0x63, 0x19, 0xef,
    0x0,

    /* U+0067 "g" */
    0x3e, 0x7d, 0x9b, 0x3c, 0x78, 0xdb, 0x9b, 0x36,
    0x18, 0x31, 0xe3, 0xc0,

    /* U+0068 "h" */
    0xc1, 0x83, 0x6, 0xf, 0xdf, 0xb1, 0xe3, 0xc7,
    0x8f, 0x1e, 0x3c, 0x60,

    /* U+0069 "i" */
    0x30, 0xc0, 0x0, 0x71, 0xc3, 0xc, 0x30, 0xc3,
    0x3f, 0x7c,

    /* U+006A "j" */
    0x18, 0xc0, 0x7, 0xbc, 0x63, 0x18, 0xc6, 0x31,
    0x98, 0xdc, 0xe0,

    /* U+006B "k" */
    0xc1, 0x83, 0x6, 0xc, 0xd9, 0xb6, 0x6c, 0xf1,
    0xb3, 0x66, 0x7c, 0xe0,

    /* U+006C "l" */
    0x71, 0xc3, 0xc, 0x30, 0xc3, 0xc, 0x30, 0xc3,
    0x3f, 0x7c,

    /* U+006D "m" */
    0xbd, 0x7b, 0xdf, 0xbf, 0x7e, 0xfd, 0xfb, 0xf6,

    /* U+006E "n" */
    0xfd, 0xfb, 0x1e, 0x3c, 0x78, 0xf1, 0xe3, 0xc6,

    /* U+006F "o" */
    0x78, 0xf1, 0xb3, 0x6c, 0x6d, 0x9b, 0x3c, 0x78,

    /* U+0070 "p" */
    0xd9, 0xb3, 0xb7, 0x6c, 0x79, 0xb3, 0x7c, 0xf9,
    0x83, 0x6, 0xc, 0x0,

    /* U+0071 "q" */
    0x3e, 0x7d, 0x9b, 0x3c, 0x78, 0xd9, 0x9b, 0x36,
    0xc, 0x18, 0x30, 0x60,

    /* U+0072 "r" */
    0xdf, 0xbd, 0x9b, 0x36, 0xc, 0x18, 0x78, 0xf0,

    /* U+0073 "s" */
    0x79, 0xec, 0x30, 0x78, 0x30, 0xfe, 0x78,

    /* U+0074 "t" */
    0x30, 0x60, 0xc1, 0x8f, 0xef, 0xcc, 0x18, 0x30,
    0x60, 0xc0, 0xf1, 0xe0,

    /* U+0075 "u" */
    0xc7, 0x8f, 0x1e, 0x3c, 0x78, 0xf1, 0xbf, 0x7e,

    /* U+0076 "v" */
    0xc7, 0x8d, 0xb3, 0x66, 0xcd, 0x9e, 0x8, 0x10,

    /* U+0077 "w" */
    0xd7, 0xaf, 0x5a, 0xa7, 0x4e, 0x9b, 0x36, 0x6c,

    /* U+0078 "x" */
    0xcf, 0x37, 0x9e, 0x31, 0xe7, 0xb3, 0xcc,

    /* U+0079 "y" */
    0xc7, 0x8d, 0x9b, 0x36, 0xcf, 0x9e, 0xc, 0x18,
    0x20, 0x43, 0x6, 0x0,

    /* U+007A "z" */
    0xff, 0xf1, 0x86, 0x33, 0xc, 0x3f, 0xfc,

    /* U+007B "{" */
    0x19, 0x8c, 0x63, 0x18, 0xdc, 0xe1, 0x8c, 0x63,
    0x18, 0x63,

    /* U+007C "|" */
    0xff, 0xff, 0xff, 0xff,

    /* U+007D "}" */
    0xe1, 0x8c, 0x63, 0x18, 0xc3, 0x19, 0x8c, 0x63,
    0x1b, 0x9c,

    /* U+007E "~" */
    0x67, 0xef, 0xdf, 0xaf, 0xdf, 0xb3, 0x0
};


/*---------------------
 *  GLYPH DESCRIPTION
 *--------------------*/

static const lv_font_fmt_txt_glyph_dsc_t glyph_dsc[] = {
    {.bitmap_index = 0, .adv_w = 0, .box_w = 0, .box_h = 0, .ofs_x = 0, .ofs_y = 0} /* id = 0 reserved */,
    {.bitmap_index = 0, .adv_w = 134, .box_w = 1, .box_h = 1, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 1, .adv_w = 134, .box_w = 2, .box_h = 13, .ofs_x = 3, .ofs_y = 0},
    {.bitmap_index = 5, .adv_w = 134, .box_w = 5, .box_h = 3, .ofs_x = 2, .ofs_y = 11},
    {.bitmap_index = 7, .adv_w = 134, .box_w = 7, .box_h = 11, .ofs_x = 1, .ofs_y = 2},
    {.bitmap_index = 17, .adv_w = 134, .box_w = 7, .box_h = 17, .ofs_x = 1, .ofs_y = -2},
    {.bitmap_index = 32, .adv_w = 134, .box_w = 7, .box_h = 13, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 44, .adv_w = 134, .box_w = 7, .box_h = 13, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 56, .adv_w = 134, .box_w = 2, .box_h = 3, .ofs_x = 3, .ofs_y = 11},
    {.bitmap_index = 57, .adv_w = 134, .box_w = 3, .box_h = 16, .ofs_x = 3, .ofs_y = -2},
    {.bitmap_index = 63, .adv_w = 134, .box_w = 4, .box_h = 16, .ofs_x = 2, .ofs_y = -2},
    {.bitmap_index = 71, .adv_w = 134, .box_w = 6, .box_h = 9, .ofs_x = 1, .ofs_y = 2},
    {.bitmap_index = 78, .adv_w = 134, .box_w = 6, .box_h = 9, .ofs_x = 1, .ofs_y = 2},
    {.bitmap_index = 85, .adv_w = 134, .box_w = 3, .box_h = 7, .ofs_x = 3, .ofs_y = -4},
    {.bitmap_index = 88, .adv_w = 134, .box_w = 4, .box_h = 2, .ofs_x = 2, .ofs_y = 5},
    {.bitmap_index = 89, .adv_w = 134, .box_w = 2, .box_h = 3, .ofs_x = 3, .ofs_y = 0},
    {.bitmap_index = 90, .adv_w = 134, .box_w = 7, .box_h = 16, .ofs_x = 1, .ofs_y = -2},
    {.bitmap_index = 104, .adv_w = 134, .box_w = 6, .box_h = 13, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 114, .adv_w = 134, .box_w = 6, .box_h = 13, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 124, .adv_w = 134, .box_w = 6, .box_h = 13, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 134, .adv_w = 134, .box_w = 6, .box_h = 13, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 144, .adv_w = 134, .box_w = 8, .box_h = 13, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 157, .adv_w = 134, .box_w = 7, .box_h = 13, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 169, .adv_w = 134, .box_w = 6, .box_h = 13, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 179, .adv_w = 134, .box_w = 6, .box_h = 13, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 189, .adv_w = 134, .box_w = 6, .box_h = 13, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 199, .adv_w = 134, .box_w = 6, .box_h = 13, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 209, .adv_w = 134, .box_w = 2, .box_h = 10, .ofs_x = 3, .ofs_y = 0},
    {.bitmap_index = 212, .adv_w = 134, .box_w = 3, .box_h = 14, .ofs_x = 3, .ofs_y = -4},
    {.bitmap_index = 218, .adv_w = 134, .box_w = 5, .box_h = 13, .ofs_x = 2, .ofs_y = 0},
    {.bitmap_index = 227, .adv_w = 134, .box_w = 5, .box_h = 6, .ofs_x = 2, .ofs_y = 3},
    {.bitmap_index = 231, .adv_w = 134, .box_w = 5, .box_h = 13, .ofs_x = 2, .ofs_y = 0},
    {.bitmap_index = 240, .adv_w = 134, .box_w = 7, .box_h = 13, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 252, .adv_w = 134, .box_w = 7, .box_h = 14, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 265, .adv_w = 134, .box_w = 7, .box_h = 13, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 277, .adv_w = 134, .box_w = 7, .box_h = 13, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 289, .adv_w = 134, .box_w = 7, .box_h = 13, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 301, .adv_w = 134, .box_w = 7, .box_h = 13, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 313, .adv_w = 134, .box_w = 5, .box_h = 13, .ofs_x = 2, .ofs_y = 0},
    {.bitmap_index = 322, .adv_w = 134, .box_w = 5, .box_h = 13, .ofs_x = 2, .ofs_y = 0},
    {.bitmap_index = 331, .adv_w = 134, .box_w = 7, .box_h = 13, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 343, .adv_w = 134, .box_w = 7, .box_h = 13, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 355, .adv_w = 134, .box_w = 4, .box_h = 13, .ofs_x = 2, .ofs_y = 0},
    {.bitmap_index = 362, .adv_w = 134, .box_w = 6, .box_h = 13, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 372, .adv_w = 134, .box_w = 7, .box_h = 13, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 384, .adv_w = 134, .box_w = 6, .box_h = 13, .ofs_x = 2, .ofs_y = 0},
    {.bitmap_index = 394, .adv_w = 134, .box_w = 7, .box_h = 13, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 406, .adv_w = 134, .box_w = 7, .box_h = 13, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 418, .adv_w = 134, .box_w = 7, .box_h = 13, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 430, .adv_w = 134, .box_w = 7, .box_h = 13, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 442, .adv_w = 134, .box_w = 7, .box_h = 17, .ofs_x = 1, .ofs_y = -4},
    {.bitmap_index = 457, .adv_w = 134, .box_w = 7, .box_h = 13, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 469, .adv_w = 134, .box_w = 7, .box_h = 13, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 481, .adv_w = 134, .box_w = 8, .box_h = 13, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 494, .adv_w = 134, .box_w = 7, .box_h = 13, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 506, .adv_w = 134, .box_w = 7, .box_h = 13, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 518, .adv_w = 134, .box_w = 7, .box_h = 13, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 530, .adv_w = 134, .box_w = 7, .box_h = 13, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 542, .adv_w = 134, .box_w = 7, .box_h = 13, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 554, .adv_w = 134, .box_w = 6, .box_h = 13, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 564, .adv_w = 134, .box_w = 3, .box_h = 16, .ofs_x = 3, .ofs_y = -2},
    {.bitmap_index = 570, .adv_w = 134, .box_w = 7, .box_h = 16, .ofs_x = 1, .ofs_y = -2},
    {.bitmap_index = 584, .adv_w = 134, .box_w = 3, .box_h = 16, .ofs_x = 2, .ofs_y = -2},
    {.bitmap_index = 590, .adv_w = 134, .box_w = 7, .box_h = 7, .ofs_x = 1, .ofs_y = 5},
    {.bitmap_index = 597, .adv_w = 134, .box_w = 7, .box_h = 2, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 599, .adv_w = 134, .box_w = 3, .box_h = 4, .ofs_x = 2, .ofs_y = 11},
    {.bitmap_index = 601, .adv_w = 134, .box_w = 7, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 609, .adv_w = 134, .box_w = 7, .box_h = 13, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 621, .adv_w = 134, .box_w = 7, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 629, .adv_w = 134, .box_w = 7, .box_h = 13, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 641, .adv_w = 134, .box_w = 7, .box_h = 9, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 649, .adv_w = 134, .box_w = 5, .box_h = 13, .ofs_x = 2, .ofs_y = 0},
    {.bitmap_index = 658, .adv_w = 134, .box_w = 7, .box_h = 13, .ofs_x = 1, .ofs_y = -4},
    {.bitmap_index = 670, .adv_w = 134, .box_w = 7, .box_h = 13, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 682, .adv_w = 134, .box_w = 6, .box_h = 13, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 692, .adv_w = 134, .box_w = 5, .box_h = 17, .ofs_x = 2, .ofs_y = -4},
    {.bitmap_index = 703, .adv_w = 134, .box_w = 7, .box_h = 13, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 715, .adv_w = 134, .box_w = 6, .box_h = 13, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 725, .adv_w = 134, .box_w = 7, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 733, .adv_w = 134, .box_w = 7, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 741, .adv_w = 134, .box_w = 7, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 749, .adv_w = 134, .box_w = 7, .box_h = 13, .ofs_x = 1, .ofs_y = -4},
    {.bitmap_index = 761, .adv_w = 134, .box_w = 7, .box_h = 13, .ofs_x = 1, .ofs_y = -4},
    {.bitmap_index = 773, .adv_w = 134, .box_w = 7, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 781, .adv_w = 134, .box_w = 6, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 788, .adv_w = 134, .box_w = 7, .box_h = 13, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 800, .adv_w = 134, .box_w = 7, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 808, .adv_w = 134, .box_w = 7, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 816, .adv_w = 134, .box_w = 7, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 824, .adv_w = 134, .box_w = 6, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 831, .adv_w = 134, .box_w = 7, .box_h = 13, .ofs_x = 1, .ofs_y = -4},
    {.bitmap_index = 843, .adv_w = 134, .box_w = 6, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 850, .adv_w = 134, .box_w = 5, .box_h = 16, .ofs_x = 2, .ofs_y = -2},
    {.bitmap_index = 860, .adv_w = 134, .box_w = 2, .box_h = 16, .ofs_x = 3, .ofs_y = -2},
    {.bitmap_index = 864, .adv_w = 134, .box_w = 5, .box_h = 16, .ofs_x = 2, .ofs_y = -2},
    {.bitmap_index = 874, .adv_w = 134, .box_w = 7, .box_h = 7, .ofs_x = 1, .ofs_y = 4}
};

/*---------------------
 *  CHARACTER MAPPING
 *--------------------*/



/*Collect the unicode lists and glyph_id offsets*/
static const lv_font_fmt_txt_cmap_t cmaps[] =
{
    {
        .range_start = 32, .range_length = 95, .glyph_id_start = 1,
        .unicode_list = NULL, .glyph_id_ofs_list = NULL, .list_length = 0, .type = LV_FONT_FMT_TXT_CMAP_FORMAT0_TINY
    }
};



/*--------------------
 *  ALL CUSTOM DATA
 *--------------------*/

#if LVGL_VERSION_MAJOR == 8
/*Store all the custom data of the font*/
static  lv_font_fmt_txt_glyph_cache_t cache;
#endif

#if LVGL_VERSION_MAJOR >= 8
static const lv_font_fmt_txt_dsc_t font_dsc = {
#else
static lv_font_fmt_txt_dsc_t font_dsc = {
#endif
    .glyph_bitmap = glyph_bitmap,
    .glyph_dsc = glyph_dsc,
    .cmaps = cmaps,
    .kern_dsc = NULL,
    .kern_scale = 0,
    .cmap_num = 1,
    .bpp = 1,
    .kern_classes = 0,
    .bitmap_format = 0,
#if LVGL_VERSION_MAJOR == 8
    .cache = &cache
#endif
};



/*-----------------
 *  PUBLIC FONT
 *----------------*/

/*Initialize a public general font descriptor*/
#if LVGL_VERSION_MAJOR >= 8
const lv_font_t ui_font_vt323_21 = {
#else
lv_font_t ui_font_vt323_21 = {
#endif
    .get_glyph_dsc = lv_font_get_glyph_dsc_fmt_txt,    /*Function pointer to get glyph's data*/
    .get_glyph_bitmap = lv_font_get_bitmap_fmt_txt,    /*Function pointer to get glyph's bitmap*/
    .line_height = 19,          /*The maximum line height required by the font*/
    .base_line = 4,             /*Baseline measured from the bottom of the line*/
#if !(LVGL_VERSION_MAJOR == 6 && LVGL_VERSION_MINOR == 0)
    .subpx = LV_FONT_SUBPX_NONE,
#endif
#if LV_VERSION_CHECK(7, 4, 0) || LVGL_VERSION_MAJOR >= 8
    .underline_position = -3,
    .underline_thickness = 1,
#endif
    .dsc = &font_dsc,          /*The custom font data. Will be accessed by `get_glyph_bitmap/dsc` */
#if LV_VERSION_CHECK(8, 2, 0) || LVGL_VERSION_MAJOR >= 9
    .fallback = NULL,
#endif
    .user_data = NULL,
};



#endif /*#if UI_FONT_VT323_21*/

