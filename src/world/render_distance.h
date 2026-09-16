#ifndef RENDER_DISTANCE_H
#define RENDER_DISTANCE_H

#include "chunk.h"

// Horizontal distance to chunk centers, in chunks. The finite world stays loaded.
enum { WORLD_RENDER_DISTANCE_MIN = 1, WORLD_RENDER_DISTANCE_DEFAULT = 12, WORLD_RENDER_DISTANCE_MAX = CHUNKS_PER_AXIS };

#endif
