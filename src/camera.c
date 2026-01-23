#include "camera.h"
#include "config.h"
#include <math.h>

#ifndef KC_MOUSE_SMOOTHING
#define KC_MOUSE_SMOOTHING 18.0f
#endif

static float clampf(float x, float a, float b) {
    return (x < a) ? a : (x > b) ? b : x;
}

/* yaw=0 -> forward = (0,0,-1) */
static v3 forward_dir(float yaw, float pitch) {
    float cy = cosf(yaw), sy = sinf(yaw);
    float cp = cosf(pitch), sp = sinf(pitch);
    return v3_norm((v3){ sy * cp, sp, -cy * cp });
}

/* yaw-only forward for movement (no pitch drift) */
static v3 forward_flat(float yaw) {
    float cy = cosf(yaw), sy = sinf(yaw);
    return v3_norm((v3){ sy, 0.0f, -cy });
}

void kc_camera_init(KC_Camera* c) {
    c->pos = (v3){ 8.0f, 6.0f, 20.0f };
    c->yaw = 0.0f;     /* faces -Z */
    c->pitch = 0.0f;

    c->sm_mdx = 0.0f;
    c->sm_mdy = 0.0f;
}

void kc_camera_update(KC_Camera* c, const KC_Input* in, float dt, float move_speed) {
    float mdx = (float)in->mouse_dx;
    float mdy = (float)in->mouse_dy;

    mdx = clampf(mdx, -500.0f, 500.0f);
    mdy = clampf(mdy, -500.0f, 500.0f);

    float alpha = 1.0f;
    if (dt > 0.0f) alpha = 1.0f - expf(-KC_MOUSE_SMOOTHING * dt);

    c->sm_mdx += (mdx - c->sm_mdx) * alpha;
    c->sm_mdy += (mdy - c->sm_mdy) * alpha;

    c->yaw   += c->sm_mdx * KC_MOUSE_SENS;
    c->pitch -= c->sm_mdy * KC_MOUSE_SENS;

    const float limit = 1.55f; /* just under 90 degrees */
    if (c->pitch > limit) c->pitch = limit;
    if (c->pitch < -limit) c->pitch = -limit;

    v3 up = (v3){0,1,0};
    v3 fwd = forward_flat(c->yaw);
    v3 right = v3_norm(v3_cross(fwd, up));

    v3 vel = (v3){0,0,0};
    if (in->keys[KC_KEY_W]) vel = v3_add(vel, fwd);
    if (in->keys[KC_KEY_S]) vel = v3_sub(vel, fwd);
    if (in->keys[KC_KEY_D]) vel = v3_add(vel, right);
    if (in->keys[KC_KEY_A]) vel = v3_sub(vel, right);
    if (in->keys[KC_KEY_SPACE]) vel = v3_add(vel, up);
    if (in->keys[KC_KEY_LSHIFT]) vel = v3_sub(vel, up);

    if (v3_len(vel) > 0.001f) vel = v3_norm(vel);
    c->pos = v3_add(c->pos, v3_mul(vel, move_speed * dt));
}

m4 kc_camera_view(const KC_Camera* c) {
    v3 fwd = forward_dir(c->yaw, c->pitch);
    return m4_look(c->pos, v3_add(c->pos, fwd), (v3){0,1,0});
}
