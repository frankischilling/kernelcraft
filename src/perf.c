#include "perf.h"
#include "log.h"
#include <time.h>
#include <string.h>

double perf_now_sec(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec * 1e-9;
}

void perf_init(PerfStats* p) {
    memset(p, 0, sizeof(*p));
    p->log_interval_sec = 2.0; /* Log every 2 seconds */
    p->last_log_time = perf_now_sec();
    p->min_frame_time_ms = 999999.0;
}

void perf_frame_start(PerfStats* p) {
    p->chunk_stream_time_ms = 0.0;
    p->chunk_rebuild_time_ms = 0.0;
    p->render_time_ms = 0.0;
    p->chunks_loaded = 0;
    p->chunks_dirty = 0;
    p->chunks_meshed = 0;
    p->chunks_uploaded = 0;
}

void perf_frame_end(PerfStats* p) {
    p->total_frames++;
    p->frame_count_since_last_log++;

    p->total_frame_time_ms += p->frame_time_ms;
    if (p->frame_time_ms < p->min_frame_time_ms) {
        p->min_frame_time_ms = p->frame_time_ms;
    }
    if (p->frame_time_ms > p->max_frame_time_ms) {
        p->max_frame_time_ms = p->frame_time_ms;
    }

    /* Calculate FPS */
    if (p->frame_time_ms > 0.0) {
        p->fps = 1000.0 / p->frame_time_ms;
    }

    /* Check if we should log */
    double now = perf_now_sec();
    if (now - p->last_log_time >= p->log_interval_sec) {
        perf_log_stats(p);

        /* Reset accumulators */
        p->frame_count_since_last_log = 0;
        p->total_frame_time_ms = 0.0;
        p->min_frame_time_ms = 999999.0;
        p->max_frame_time_ms = 0.0;
        p->last_log_time = now;
    }
}

void perf_log_stats(PerfStats* p) {
    if (p->frame_count_since_last_log == 0) return;

    double avg_frame_time = p->total_frame_time_ms / (double)p->frame_count_since_last_log;

    KC_INFO("=== Performance Stats ===");
    KC_INFO("FPS: %.1f (%.2fms avg, %.2fms min, %.2fms max)",
             p->fps, avg_frame_time, p->min_frame_time_ms, p->max_frame_time_ms);
    KC_INFO("Frame Time: %.2fms (stream: %.2fms, rebuild: %.2fms, render: %.2fms)",
             p->frame_time_ms,
             p->chunk_stream_time_ms,
             p->chunk_rebuild_time_ms,
             p->render_time_ms);
    KC_INFO("Chunks: %d loaded, %d dirty, %d meshed, %d uploaded",
             p->chunks_loaded, p->chunks_dirty, p->chunks_meshed, p->chunks_uploaded);
    KC_INFO("Total Frames: %llu", (unsigned long long)p->total_frames);
}
