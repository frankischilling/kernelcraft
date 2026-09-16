#ifndef SELECTION_H
#define SELECTION_H

#include "../utils/raycast.h"
#include "../world/edit.h"

void drawSelection(const Ray* selection, const Mat4 view, const Mat4 projection);
void drawBlockBreaking(const BlockBreaking* breaking, const Ray* selection, const Mat4 view, const Mat4 projection);

#endif
