#ifndef ITEM_RENDERER_H
#define ITEM_RENDERER_H

#include <GL/glew.h>
#include "../world/dropped_items.h"
#include "../world/item_model.h"
#include "../world/player_model.h"
#include "../world/day_night.h"

enum { ITEM_ICON_SIZE = 128 };

typedef struct {
  GLuint framebuffer, color, depth;
  int width, height;
} ItemRenderTarget;

typedef struct {
  GLuint program, compositeProgram, vao, vbo, materials;
  GLint quadFirst;
  GLuint icons[ITEM_ID_LAST + 1];
  GLint first[ITEM_ID_LAST + 1];
  GLsizei count[ITEM_ID_LAST + 1];
  GLint modelLocation, viewProjectionLocation, colorLocation;
  GLint lightDirectionLocation, lightColorLocation, skyColorLocation, groundColorLocation;
  ItemRenderTarget heldTarget;
} ItemRenderer;

// One immutable mesh buffer and material array serve inventory, world, and held
// items. GUI icons are rendered once from those meshes into transparent textures.
// Initialization, drawing, and cleanup require the current GL 3.3 context.
bool initItemRenderer(ItemRenderer* renderer);
void cleanupItemRenderer(ItemRenderer* renderer);
// Icon textures have OpenGL's bottom-left origin. Empty/invalid items return 0.
GLuint itemRendererIcon(const ItemRenderer* renderer, uint16_t item);

// Draw a solid item in the caller's world target, preserving all changed state.
void renderItemModel(const ItemRenderer* renderer, uint16_t item, const Mat4 model, const Mat4 view, const Mat4 projection, const DayNightState* daylight);
void renderDroppedItems(const ItemRenderer* renderer, const DroppedItems* drops, const Mat4 view, const Mat4 projection, const DayNightState* daylight);
void renderPlayerHeldItems(const ItemRenderer* renderer, Vec3 feet, const PlayerModelPose* pose, ItemStack mainHand, ItemStack offhand, const Mat4 view, const Mat4 projection,
                           const DayNightState* daylight);

// First-person items share a reusable private depth target. The composite never
// reads or changes world depth. False reports a failed target allocation/resize.
bool renderHeldItems(ItemRenderer* renderer, ItemStack mainHand, ItemStack offhand, const PlayerModelPose* pose, float aspect, const DayNightState* daylight);

#endif
