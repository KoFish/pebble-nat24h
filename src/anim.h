#ifndef __ANIM_H__
#define __ANIM_H__

#include <pebble.h>

bool is_animating(void);
void animate(int, int, AnimationCurve, AnimationImplementation*, bool);
int anim_percentage(AnimationProgress, int);

#endif // __ANIM_H__
