#include "mui_element.h"

#include "nrf_log.h"

void mui_element_scrollbar(mui_canvas_t *p_canvas, uint32_t pos, uint16_t total)
{
    uint8_t width = mui_canvas_get_width(p_canvas);
    uint8_t height = mui_canvas_get_height(p_canvas);
    // prevent overflows
    mui_canvas_set_draw_color(p_canvas, 0);
    mui_canvas_draw_box(p_canvas, width - 4, 0, 4, height);
    // dot line
    mui_canvas_set_draw_color(p_canvas, 1);
    for (uint8_t i = 0; i < height; i += 2)
    {
        mui_canvas_draw_dot(p_canvas, width - 2, i);
    }
    // Position block
    if (total)
    {
        float block_h = ((float) height) / total;
        mui_canvas_draw_box(p_canvas, width - 3, block_h * pos, 3, block_h > 1 ? block_h : 1);
    }
}

static bool mui_element_rest_fits(mui_canvas_t *p_canvas, const char *p, uint16_t room)
{
    const char *end = strchr(p, '\n');
    if (end == NULL)
    {
        return mui_canvas_get_utf8_width(p_canvas, p) <= room;
    }
    if (end[1] != '\0')
    {
        return false;
    }

    char line[64];
    size_t len = (size_t) (end - p);
    if (len >= sizeof(line))
    {
        return false;
    }
    memcpy(line, p, len);
    line[len] = '\0';
    return mui_canvas_get_utf8_width(p_canvas, line) <= room;
}

void mui_element_autowrap_text(mui_canvas_t *p_canvas, uint8_t x, uint8_t y, uint8_t w, uint8_t h, const char *text)
{
    uint8_t font_height = mui_canvas_current_font_height(p_canvas);
    uint8_t ellipsis_w = mui_canvas_get_utf8_width(p_canvas, "...");
    uint16_t right = (uint16_t) x + w;
    uint16_t bottom = (uint16_t) y + h;
    uint16_t xi = x;
    uint16_t yi = y;

    const char *p = text;
    char utf8[5];
    bool last_line = (yi + font_height) >= bottom;
    bool fits = !last_line || mui_element_rest_fits(p_canvas, p, w);

    while (*p != 0 && yi < bottom)
    {
        if (*p == '\n')
        {
            if (last_line)
            {
                if (!fits)
                {
                    mui_canvas_draw_utf8(p_canvas, xi, yi, "...");
                }
                break;
            }
            xi = x;
            yi += font_height;
            p++;
            last_line = (yi + font_height) >= bottom;
            fits = !last_line || mui_element_rest_fits(p_canvas, p, w);
            continue;
        }

        uint8_t utf8_size = mui_canvas_get_utf8_bytes(p);
        memcpy(utf8, p, utf8_size);
        utf8[utf8_size] = '\0';
        uint8_t utf8_x = mui_canvas_get_utf8_width(p_canvas, utf8);
        if ((utf8_x + xi) > right)
        {
            if (last_line)
            {
                break;
            }
            xi = x;
            yi += font_height;
            last_line = (yi + font_height) >= bottom;
            fits = !last_line || mui_element_rest_fits(p_canvas, p, w);
        }
        if (!fits && ((xi + utf8_x + ellipsis_w) > right))
        {
            mui_canvas_draw_utf8(p_canvas, xi, yi, "...");
            break;
        }
        uint8_t utf8_w = mui_canvas_draw_utf8(p_canvas, xi, yi, utf8);
        xi += utf8_w;
        p += utf8_size;
    }
}

uint16_t mui_element_autowrap_text_box(mui_canvas_t *p_canvas, uint16_t x, uint16_t y, uint16_t w, uint16_t h,
                                       uint16_t offset_y, uint8_t square_r, const char *text)
{
    mui_rect_t clip_win_prev;
    mui_rect_t clip_win_cur;
    mui_canvas_get_clip_window(p_canvas, &clip_win_prev);
    clip_win_cur.x = x;
    clip_win_cur.y = y;
    clip_win_cur.w = w;
    clip_win_cur.h = h;
    mui_canvas_set_clip_window(p_canvas, &clip_win_cur);

    uint8_t font_height = mui_canvas_current_font_height(p_canvas);
    uint16_t xi = x;
    uint16_t text_y = y + font_height;
    uint16_t yi = text_y;
    uint16_t wi = w - 4;

    const char *p = text;
    char utf8[5];

    while (*p != 0)
    {
        if (*p == '\n')
        {
            xi = x;
            yi += font_height;
            p++;
            continue;
        }

        wi = w - 4; // scrollbar width
        if (yi < text_y + square_r)
        {
            wi -= square_r;
        }

        uint8_t utf8_size = mui_canvas_get_utf8_bytes(p);
        memcpy(utf8, p, utf8_size);
        utf8[utf8_size] = '\0';
        uint8_t utf8_x = mui_canvas_get_utf8_width(p_canvas, utf8);
        if (utf8_x + 1 + xi > x + wi)
        {
            xi = x;
            yi += font_height;
        }
        if (yi > offset_y)
        {
            mui_canvas_draw_utf8(p_canvas, xi, yi - offset_y, utf8);
        }
        xi += utf8_x + 1; // 1 pix for margin
        p += utf8_size;
    }

    uint16_t total = (yi - y + h - 1) / h;
    uint16_t pos = offset_y / h;

    if (total > 1)
    {
        pos = pos < total - 1 ? pos : total - 1;

        // scrollbar
        uint8_t width = w;
        uint8_t height = h;
        // prevent overflows
        mui_canvas_set_draw_color(p_canvas, 0);
        mui_canvas_draw_box(p_canvas, width - 4, y, 4, height);
        // dot line
        mui_canvas_set_draw_color(p_canvas, 1);
        for (uint8_t i = y; i < y + height; i += 2)
        {
            mui_canvas_draw_dot(p_canvas, width - 2, i);
        }
        // Position block
        if (total)
        {
            float block_h = ((float) height) / total;
            mui_canvas_draw_box(p_canvas, width - 3, y + block_h * pos, 3, block_h > 1 ? block_h : 1);
        }
    }

    mui_canvas_set_clip_window(p_canvas, &clip_win_prev);
    return yi - y;
}

uint16_t mui_element_text_height(mui_canvas_t *p_canvas, uint8_t w, const char *text)
{
    uint8_t font_height = mui_canvas_current_font_height(p_canvas);
    uint8_t xi = 0;
    uint16_t yi = 0;

    const char *p = text;
    char utf8[5];

    while (*p != 0)
    {
        uint8_t utf8_size = mui_canvas_get_utf8_bytes(p);
        memcpy(utf8, p, utf8_size);
        utf8[utf8_size] = '\0';
        uint8_t utf8_x = mui_canvas_get_utf8_width(p_canvas, utf8);
        if (utf8_x + xi > w)
        {
            xi = 0;
            yi += font_height;
        }
        xi += utf8_x;
        p += utf8_size;
    }
    return yi + font_height;
}

void mui_element_button(mui_canvas_t *p_canvas, uint8_t x, uint8_t y, const char *str, uint8_t selected)
{
    uint8_t font_height = mui_canvas_current_font_height(p_canvas);
    uint8_t button_width = mui_canvas_get_utf8_width(p_canvas, str);

    if (selected)
    {
        mui_canvas_set_draw_color(p_canvas, 1);
        mui_canvas_draw_box(p_canvas, x, y + 2, button_width + 4, font_height + 2);
        mui_canvas_set_draw_color(p_canvas, 0);
        mui_canvas_draw_utf8(p_canvas, x + 2, y + font_height, str);
        mui_canvas_set_draw_color(p_canvas, 1);
    }
    else
    {
        mui_canvas_set_draw_color(p_canvas, 1);
        mui_canvas_draw_rframe(p_canvas, x, y + 2, button_width + 4, font_height, 1);
        mui_canvas_draw_utf8(p_canvas, x + 2, y + font_height, str);
    }
}
