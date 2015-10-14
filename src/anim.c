#include "anim.h"

bool s_animating;

bool is_animating(void) {
    return s_animating;
}

static void animation_started(Animation *anim, void *context) {
    s_animating = true;
}

static void animation_stopped(Animation *anim, bool stopped, void *context) {
    s_animating = false;
}

void animate(int duration, int delay, AnimationCurve ease, AnimationImplementation *implementation,
             bool handlers) {
    Animation *anim = animation_create();
    animation_set_duration(anim, duration);
    animation_set_delay(anim, delay);
    animation_set_curve(anim, ease);
    animation_set_implementation(anim, implementation);
    if (handlers) {
        animation_set_handlers(anim, (AnimationHandlers) {
                .started = animation_started,
                .stopped = animation_stopped
        }, NULL);
    }
    animation_schedule(anim);
}

int anim_percentage(AnimationProgress dist_normalized, int max) {
    return (int)(float)(((float)dist_normalized / (float)ANIMATION_NORMALIZED_MAX) * (float)max);
}
