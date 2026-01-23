#pragma once
#include "math.h"
#include "platform.h"
#include "chunk_manager.h"
#include <stdbool.h>

typedef struct KC_Player {
    v3 pos;       /* feet position */
    v3 vel;       /* units/sec */
    float half_w; /* AABB half width (x/z) */
    float height; /* AABB height */
    float eye_y;  /* camera eye offset from feet */
    bool on_ground;
} KC_Player;

void kc_player_init(KC_Player* p, v3 start_feet);
void kc_player_step(KC_Player* p, const KC_Input* in, const KC_ChunkManager* cm, float dt, float yaw);
