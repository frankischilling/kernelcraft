#pragma once

/* Window */
#define KC_WINDOW_TITLE       "Kernelcraft (rewrite)"
#define KC_WINDOW_W           1280
#define KC_WINDOW_H           720

/* Timing */
#define KC_FIXED_DT           (1.0 / 60.0)  /* used for simulation step (optional) */

/* Timing budgets (ms per frame) */
#define KC_MESH_MS_BUDGET     2.0f
#define KC_UPLOAD_MS_BUDGET   1.0f

/* Camera */
#define KC_MOUSE_SENS         0.005f
#define KC_MOVE_SPEED         6.0f
#define KC_SPRINT_MULT        2.5f

/* World */
#define KC_CHUNK_X            16
#define KC_CHUNK_Y            16
#define KC_CHUNK_Z            16
