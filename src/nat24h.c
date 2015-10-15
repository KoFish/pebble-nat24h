// vim: tw=100 sw=4 ts=4
#include <pebble.h>

#include "anim.h"

#undef DEBUG

#define BORDER 10
#define BACKGROUND GColorBlack

#define INDICATOR_WIDTH 6

static Window *s_main_window = NULL;
static Layer *s_base_layer = NULL;
static Layer *s_indicator_layer = NULL;
static TextLayer *s_time_layer = NULL;

typedef struct {
    int hours;
    int minutes;
    char str[6];
    int32_t hour_angle;
    int32_t min_angle;
} Time;

static Time s_last_time;
static int32_t s_anim_time;

static uint16_t s_watch_size, s_anim_watch_size;
static uint16_t s_indicator_length;
static uint16_t s_notch_scale;

static GPoint calculate_point(GPoint center, int32_t angle, int32_t offset) {
    GPoint point = {
        .y = center.y + (-cos_lookup(angle) * offset / TRIG_MAX_RATIO),
        .x = center.x + (sin_lookup(angle) * offset / TRIG_MAX_RATIO)
    };
    return point;
}

static void draw_radial_line(GPoint center, int32_t angle, uint16_t width, int32_t offset,
                             uint16_t length, GContext *ctx) {
    GPoint p_start = center, p_end;
    if (offset < 0) p_start = calculate_point(center, angle + TRIG_MAX_RATIO/2, -offset);
    else if (offset > 0) p_start = calculate_point(center, angle, offset);

    p_end = calculate_point(center, angle, (length + offset));

    graphics_context_set_stroke_width(ctx, width);
    graphics_draw_line(ctx, p_start, p_end);
}

static void draw_hour_indicator(GPoint center, int32_t angle, GContext *ctx) {
    graphics_context_set_stroke_color(ctx, GColorWhite);
    if (is_animating()) {
        draw_radial_line(center, s_anim_time, INDICATOR_WIDTH, 0, s_indicator_length, ctx);
    } else {
        draw_radial_line(center, angle, INDICATOR_WIDTH, 0, s_indicator_length, ctx);
    }
}

static void draw_minute_indicator(GPoint center, int32_t angle, GContext *ctx) {
    graphics_context_set_fill_color(ctx, GColorWhite);
    graphics_fill_circle(ctx, calculate_point(center, angle, s_indicator_length), 3);
}

static void indicator_layer_update(Layer *layer, GContext *ctx) {
    graphics_context_set_antialiased(ctx, true);
    GRect bounds = layer_get_bounds(layer);
    GPoint center = grect_center_point(&bounds);
    draw_minute_indicator(center, s_last_time.min_angle, ctx);
    draw_hour_indicator(center, s_last_time.hour_angle, ctx);
}

/* offset = How many parts of a full circle to offset the start with.
 * qangle = How many parts of a full circle each step should be.
 * count = How many step this is notch is at.
 */
static void draw_notch(GPoint center, int32_t offset, int32_t qangle, uint16_t count, uint16_t width,
                           uint16_t outer, uint16_t length, GContext *ctx) {
    int32_t angle = (TRIG_MAX_ANGLE * count / qangle - TRIG_MAX_ANGLE / offset);
    /*** Currently unused
     *uint16_t inner = outer - length;
     *draw_radial_line(center, angle / s_notch_scale, width, inner, length, ctx);
     */
    graphics_fill_circle(ctx, calculate_point(center, angle, outer), length/2);
}

static void base_layer_update(Layer *layer, GContext *ctx) {
    int i;
    GRect bounds = layer_get_bounds(layer);
    GPoint center = grect_center_point(&bounds);
    graphics_context_set_fill_color(ctx, BACKGROUND);
    graphics_fill_rect(ctx, bounds, 0, GCornerNone);

    graphics_context_set_stroke_color(ctx, GColorWhite);
    graphics_context_set_stroke_width(ctx, 3);
    /*graphics_draw_circle(ctx, center, s_anim_watch_size - 10);*/

    graphics_context_set_stroke_color(ctx, GColorLightGray);

    graphics_context_set_fill_color(ctx, GColorLightGray);
    int width, length, offset;
    for (i = -12 ; i < 12 ; i++) {
        // Noon
        if (i == 0)           { offset =  3; length = 10, width = 5; }
        // Midnight
        else if (i == -12)       { offset =  0; length =  5; width = 5; }
        // Quarter of a day
        else if ((i % 6) == 0) { offset =  0; length =  5; width = 3; }
        else if ((i % 3) == 0) { offset =  0; length =  5; width = 2; }
        else                   { offset =  0; length =  3; width = 1; }
        draw_notch(center, 0, 24, i, width, s_anim_watch_size - 10 + offset, length, ctx);
    }
}

static void tick_handler(struct tm *tick_time, TimeUnits changed) {
    s_last_time.hours = tick_time->tm_hour;
    s_last_time.minutes = tick_time->tm_min;
#ifndef DEBUG
    s_last_time.hour_angle = s_last_time.hours * TRIG_MAX_RATIO / 24;
    s_last_time.hour_angle += s_last_time.minutes * TRIG_MAX_RATIO / (60 * 24);
#else
    s_last_time.hour_angle = tick_time->tm_sec * TRIG_MAX_RATIO / 60;
#endif
    s_last_time.min_angle = s_last_time.minutes * TRIG_MAX_RATIO / 60;
    s_last_time.hour_angle += TRIG_MAX_RATIO / 2;
    clock_copy_time_string(s_last_time.str, sizeof(s_last_time.str));

    if (s_time_layer != NULL) text_layer_set_text(s_time_layer, s_last_time.str);

    if (s_indicator_layer != NULL) layer_mark_dirty(s_indicator_layer);
}

static void setup_time_layer() {
    text_layer_set_text(s_time_layer, s_last_time.str);
    text_layer_set_background_color(s_time_layer, BACKGROUND);
    text_layer_set_text_color(s_time_layer, GColorLightGray);
    text_layer_set_text_alignment(s_time_layer, GTextAlignmentCenter);
}

static void main_window_load(Window *window) {
    Layer *window_layer = window_get_root_layer(window);
    GRect bounds = layer_get_bounds(window_layer);

    s_base_layer = layer_create(GRect(0, 0, bounds.size.w, bounds.size.w));
    s_indicator_layer = layer_create(GRect(0, 0, bounds.size.w, bounds.size.w));
    s_time_layer = text_layer_create(GRect(0, bounds.size.w, bounds.size.w, bounds.size.h - bounds.size.w));
    setup_time_layer();
    s_watch_size = s_anim_watch_size = bounds.size.w/2;
    s_indicator_length = s_watch_size - 20;
    s_anim_watch_size = 30;
    s_notch_scale = 100;
    layer_set_update_proc(s_base_layer, base_layer_update);
    layer_set_update_proc(s_indicator_layer, indicator_layer_update);

    layer_add_child(s_base_layer, s_indicator_layer);
    layer_add_child(window_layer, s_base_layer);
    layer_add_child(window_layer, text_layer_get_layer(s_time_layer));
}

static void main_window_unload(Window *window) {
    layer_destroy(s_indicator_layer);
    layer_destroy(s_base_layer);
    text_layer_destroy(s_time_layer);
}

static void indicator_update(Animation *anim, AnimationProgress dist_normalized) {
    s_anim_time = anim_percentage(dist_normalized, s_last_time.hour_angle);
    layer_mark_dirty(s_indicator_layer);
}

static void notch_update(Animation *anim, AnimationProgress dist_normalized) {
    s_notch_scale = 100 / anim_percentage(dist_normalized, 100);
    layer_mark_dirty(s_base_layer);
}

static void radius_update(Animation *anim, AnimationProgress dist_normalized) {
    s_anim_watch_size = anim_percentage(dist_normalized, s_watch_size - 30) + 30;
    s_indicator_length = s_anim_watch_size > 20 ? s_anim_watch_size - 20 : 0;
    layer_mark_dirty(s_base_layer);
    layer_mark_dirty(s_indicator_layer);
}

static void init() {
    srand(time(NULL));

    time_t t = time(NULL);
    struct tm *time_now = localtime(&t);
    tick_handler(time_now, MINUTE_UNIT);

    s_main_window = window_create();
    window_set_background_color(s_main_window, BACKGROUND);
    window_set_window_handlers(s_main_window, (WindowHandlers) {
            .load = main_window_load,
            .unload = main_window_unload,
    });
    window_stack_push(s_main_window, true);

#ifdef DEBUG
    tick_timer_service_subscribe(SECOND_UNIT, tick_handler);
#else
    tick_timer_service_subscribe(MINUTE_UNIT, tick_handler);
#endif

    AnimationImplementation size_anim_impl = { .update = radius_update };
    animate(600, 0, AnimationCurveEaseOut, &size_anim_impl, false);

    AnimationImplementation notch_anim_impl = { .update = notch_update };
    animate(600, 100, AnimationCurveEaseOut, &notch_anim_impl, false);

    AnimationImplementation indicator_anim_impl = { .update = indicator_update };
    animate(900, 0, AnimationCurveEaseOut, &indicator_anim_impl, true);
}

static void deinit() {
    window_destroy(s_main_window);
}

int main() {
    init();
    app_event_loop();
    deinit();
}
