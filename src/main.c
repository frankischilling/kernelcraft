#include "config.h"
#include "log.h"
#include "platform.h"
#include "renderer.h"
#include "camera.h"
#include "math.h"
#include "chunk_manager.h"
#include "perf.h"
#include "player.h"

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

    /* Player */
    KC_Player player;
    kc_player_init(&player, (v3){ 8.0f, 3.0f, 20.0f }); /* feet on/above grass */
    double sim_acc = 0.0;

    /* Performance monitoring */
    PerfStats perf;
    perf_init(&perf);

    double last = kc_platform_time_sec();

    while (!plat.should_close) {
        double now = kc_platform_time_sec();
        double dt = now - last;
        if (dt > 0.25) dt = 0.25;
        last = now;

        /* Start performance timing */
        perf_frame_start(&perf);
        double frame_start = perf_now_sec();

        kc_platform_poll(&plat);

        if (plat.input.keys_pressed[KC_KEY_Q]) plat.should_close = true;
        if (plat.input.keys_pressed[KC_KEY_ESCAPE]) {
            kc_platform_set_mouse_capture(&plat, !plat.mouse_captured);
        }

        kc_camera_update(&cam, &plat.input, (float)dt, 0.0f); /* mouse look with real dt, no movement */

        /* Fixed-step physics */
        sim_acc += dt;
        while (sim_acc >= KC_FIXED_DT) {
            kc_player_step(&player, &plat.input, &cm, (float)KC_FIXED_DT, cam.yaw);
            sim_acc -= KC_FIXED_DT;
        }

        /* Camera follows player (eye position) */
        cam.pos = v3_add(player.pos, (v3){0.0f, player.eye_y, 0.0f});

        /* Time chunk streaming */
        PERF_TIME_BLOCK(&perf, chunk_stream_time_ms) {
            kc_chunkman_stream_around(&cm, cam.pos);
        } PERF_TIME_BLOCK_END(&perf, chunk_stream_time_ms);

        /* Time chunk rebuilding */
        PERF_TIME_BLOCK(&perf, chunk_rebuild_time_ms) {
            kc_chunkman_rebuild_dirty(&cm, &rnd, cam.pos, KC_MESH_MS_BUDGET, KC_UPLOAD_MS_BUDGET);
        } PERF_TIME_BLOCK_END(&perf, chunk_rebuild_time_ms);

        glViewport(0, 0, plat.width, plat.height);
        glClearColor(0.53f, 0.81f, 0.92f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        float aspect = (plat.height > 0) ? ((float)plat.width / (float)plat.height) : 1.0f;
        m4 proj = m4_perspective(70.0f * (float)M_PI / 180.0f, aspect, 0.1f, 800.0f);
        m4 view = kc_camera_view(&cam);
        m4 vp = m4_mul(proj, view);

        /* Time rendering */
        PERF_TIME_BLOCK(&perf, render_time_ms) {
            kc_renderer_begin(&rnd);
            kc_chunkman_draw(&cm, &rnd, vp);
            kc_renderer_end(&rnd);
        } PERF_TIME_BLOCK_END(&perf, render_time_ms);

        kc_platform_swap(&plat);

        /* Gather chunk statistics */
        KC_ChunkStats chunk_stats;
        kc_chunkman_get_stats(&cm, &chunk_stats);
        perf.chunks_loaded = chunk_stats.loaded_chunks;
        perf.chunks_dirty = chunk_stats.dirty_chunks;

        /* End performance timing */
        double frame_end = perf_now_sec();
        perf.frame_time_ms = (frame_end - frame_start) * 1000.0;
        perf_frame_end(&perf);
    }

    kc_chunkman_shutdown(&cm, &rnd);
    kc_renderer_shutdown(&rnd);
    kc_platform_shutdown(&plat);
    return 0;
}
