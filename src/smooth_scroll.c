
#include <math.h>
#include "../include/app.h"

#define ROW_HEIGHT 48.0f
#define VISIBLE_ROWS 7

static float clampf(float v, float lo, float hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

void list_select(BrowserList *list, int index) {
    if (!list || list->count <= 0) return;
    if (index < 0) index = 0;
    if (index >= list->count) index = list->count - 1;
    list->selected = index;

    int anchor = VISIBLE_ROWS / 2;
    float target = (float)(index - anchor) * ROW_HEIGHT;
    float max_scroll = (float)(list->count - VISIBLE_ROWS) * ROW_HEIGHT;
    if (max_scroll < 0) max_scroll = 0;
    list->target_scroll_y = clampf(target, 0.0f, max_scroll);
}

void list_update_scroll(BrowserList *list, float dt, int enabled) {
    if (!list) return;
    if (!enabled) {
        list->scroll_y = list->target_scroll_y;
        return;
    }

    float diff = list->target_scroll_y - list->scroll_y;
    float speed = 12.0f;
    float step = diff * speed * dt;

    if (fabsf(diff) < 0.25f)
        list->scroll_y = list->target_scroll_y;
    else
        list->scroll_y += step;
}
