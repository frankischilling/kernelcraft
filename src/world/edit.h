#ifndef EDIT_H
#define EDIT_H

#include "cube.h"

#define EDIT_REACH 6.0f

// Editing reserves a 0.6-wide, 1.8-high body with the eye 1.62 above its feet,
// including in debug flight. This exclusion is not movement collision.
bool editTarget(Vec3 eye, Vec3 direction, int selectedBlock, bool place);

#endif
