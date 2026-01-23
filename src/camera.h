#pragma once
#include "math.h"
#include "platform.h"

typedef struct KC_Camera {
    v3 pos;
    float yaw;   /* radians */
    float pitch; /* radians */
    float sm_mdx;
    float sm_mdy;
} KC_Camera;

void kc_camera_init(KC_Camera* c);
void kc_camera_update(KC_Camera* c, const KC_Input* in, float dt, float move_speed);

m4 kc_camera_view(const KC_Camera* c);
