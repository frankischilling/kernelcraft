#include "config.h"
#include "log.h"
#include "platform.h"
#include "renderer.h"
#include "camera.h"
#include "math.h"
#include "chunk_manager.h"

#include <GL/gl.h>
#include <math.h>

int main(void) {
    log_init();

    KC_Platform plat;
    if (!kc_platform_init(&plat, KC_WINDOW_W, KC_WINDOW_H, KC_WINDOW_TITLE)) {
        KC_ERR("platform init failed");
        return 1;
    }

    KC_Renderer rnd;
    if (!kc_renderer_init(&rnd)) {
        KC_ERR("renderer init failed");
        kc_platform_shutdown(&plat);
        return 1;
    }

    kc_platform_set_mouse_capture(&plat, true);

    KC_Camera cam;
    kc_camera_init(&cam);

    KC_ChunkManager cm;
    kc_chunkman_init(&cm, 32);

    double last = kc_platform_time_sec();

    while (!plat.should_close) {
        double now = kc_platform_time_sec();
        double dt = now - last;
        if (dt > 0.25) dt = 0.25;
        last = now;

        kc_platform_poll(&plat);

        if (plat.input.keys_pressed[KC_KEY_Q]) plat.should_close = true;
        if (plat.input.keys_pressed[KC_KEY_ESCAPE]) {
            kc_platform_set_mouse_capture(&plat, !plat.mouse_captured);
        }

        kc_camera_update(&cam, &plat.input, (float)dt, KC_MOVE_SPEED);

        kc_chunkman_stream_around(&cm, cam.pos);
        kc_chunkman_rebuild_dirty(&cm, &rnd, 128);

        glViewport(0, 0, plat.width, plat.height);
        glClearColor(0.53f, 0.81f, 0.92f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        float aspect = (plat.height > 0) ? ((float)plat.width / (float)plat.height) : 1.0f;
        m4 proj = m4_perspective(70.0f * (float)M_PI / 180.0f, aspect, 0.1f, 800.0f);
        m4 view = kc_camera_view(&cam);
        m4 vp = m4_mul(proj, view);

        kc_renderer_begin(&rnd);
        kc_chunkman_draw(&cm, &rnd, vp);
        kc_renderer_end(&rnd);

        kc_platform_swap(&plat);
    }

    kc_chunkman_shutdown(&cm, &rnd);
    kc_renderer_shutdown(&rnd);
    kc_platform_shutdown(&plat);
    return 0;
}
