#ifndef ITEM_RENDERER_H
#define ITEM_RENDERER_H

#include <GL/glew.h>
#include "../world/dropped_items.h"

// Reuses the HUD material images. Owns no textures or per-frame buffers.
void renderDroppedItems(const DroppedItems* drops, const Mat4 view, const Mat4 projection, const GLuint blockTextures[6]);

#endif
