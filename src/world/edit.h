#ifndef EDIT_H
#define EDIT_H

#include "cube.h"

#define EDIT_REACH 6.0f

// Pass the player's actual feet to share exact collision bounds with placement.
bool editTarget(Vec3 eye, Vec3 direction, Vec3 bodyFeet, int selectedBlock, bool place);

#endif
