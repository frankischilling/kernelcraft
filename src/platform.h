#pragma once
#include <stdbool.h>
#include <stdint.h>

typedef enum KC_Key {
    KC_KEY_UNKNOWN = 0,
    KC_KEY_W,
    KC_KEY_A,
    KC_KEY_S,
    KC_KEY_D,
    KC_KEY_SPACE,
    KC_KEY_LSHIFT,
    KC_KEY_ESCAPE,
    KC_KEY_Q,
    KC_KEY_E,
    KC_KEY_F,
    KC_KEY_COUNT
} KC_Key;

typedef struct KC_Input {
    bool keys[KC_KEY_COUNT];
    bool keys_pressed[KC_KEY_COUNT];  /* true only on the frame key went down */
    bool keys_released[KC_KEY_COUNT]; /* true only on the frame key went up   */

    int mouse_dx;
    int mouse_dy;
} KC_Input;

typedef struct KC_Platform {
    int width;
    int height;
    bool should_close;

    /* mouse capture (relative-like mode) */
    bool mouse_captured;

    KC_Input input;

    /* opaque platform data */
    void* native;
} KC_Platform;

bool kc_platform_init(KC_Platform* p, int w, int h, const char* title);
void kc_platform_shutdown(KC_Platform* p);

/* Call once per frame */
void kc_platform_poll(KC_Platform* p);
void kc_platform_swap(KC_Platform* p);

double kc_platform_time_sec(void);

/* Mouse capture toggling */
void kc_platform_set_mouse_capture(KC_Platform* p, bool enabled);
