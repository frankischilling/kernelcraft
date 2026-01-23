#pragma once
#include <stdint.h>
#include <stdbool.h>

typedef struct PerfStats {
    /* Frame timing */
    double frame_time_ms;
    double fps;

    /* Frame counters */
    uint64_t total_frames;
    uint64_t frame_count_since_last_log;

    /* Timing accumulators (reset each log interval) */
    double total_frame_time_ms;
    double min_frame_time_ms;
    double max_frame_time_ms;

    /* Subsystem timing (current frame) */
    double chunk_stream_time_ms;
    double chunk_rebuild_time_ms;
    double render_time_ms;

    /* Chunk manager stats */
    int chunks_loaded;
    int chunks_dirty;
    int chunks_meshed;
    int chunks_uploaded;

    /* Timing */
    double last_log_time;
    double log_interval_sec;
} PerfStats;

void perf_init(PerfStats* p);
void perf_frame_start(PerfStats* p);
void perf_frame_end(PerfStats* p);
void perf_log_stats(PerfStats* p);

/* Timing helpers */
double perf_now_sec(void);

#define PERF_TIME_BLOCK(stats, field) \
    { \
        double perf_start_##field = perf_now_sec();

#define PERF_TIME_BLOCK_END(stats, field) \
        (stats)->field = (perf_now_sec() - perf_start_##field) * 1000.0; \
    }
