#ifndef PLAYER_RENDERER_H
#define PLAYER_RENDERER_H

#include <GL/glew.h>
#include "../world/day_night.h"
#include "../world/player_model.h"
#include "../math/math.h"
#include <stdbool.h>

typedef struct {
  GLuint program;
  GLuint texture;
  GLuint vao;
  GLuint vbo;
  GLint viewProjectionLocation;
  GLint feetLocation;
  GLint rootYawLocation;
  GLint rootScaleLocation;
  GLint partSizeLocation;
  GLint partPivotLocation;
  GLint partCenterLocation;
  GLint partTranslationLocation;
  GLint partRotationLocation;
  GLint partScaleLocation;
  GLint shellInflationLocation;
  GLint viewModelLocation;
  GLint viewModelTransformLocation;
  GLint lightDirectionLocation;
  GLint lightColorLocation;
  GLint skyColorLocation;
  GLint groundColorLocation;
  GLint outerLayerLocation;
  GLint outerPassLocation;
  GLint skinLocation;
  GLint solidColorEnabledLocation;
  GLint solidColorLocation;
  bool fractionalAlpha;
} PlayerRenderer;

typedef struct {
  bool helmet;
  bool chestplate;
  bool leggings;
  bool boots;
} PlayerEquipmentVisuals;

// Initializes a renderer from one exact 64x64 RGBA skin. The skin uses source
// PNG coordinates with a top-left origin and is sampled nearest/clamped.
bool initPlayerRenderer(PlayerRenderer* renderer, const char* skinPath);
void cleanupPlayerRenderer(PlayerRenderer* renderer);

// Draw the six-part avatar in world space. Caller supplies the same view and
// projection used for terrain; the pass preserves every OpenGL state it changes.
void renderPlayerModel(const PlayerRenderer* renderer, Vec3 feet, const PlayerModelPose* pose, const Mat4 view, const Mat4 projection, const DayNightState* daylight);

// Draw code-defined armor primitives over the skinned world model. The armor
// follows the same posed joints and lighting while keeping skin geometry/assets
// unchanged. Call after renderPlayerModel so the inflated pieces sit above it.
void renderPlayerEquipment(const PlayerRenderer* renderer, Vec3 feet, const PlayerModelPose* pose, const PlayerEquipmentVisuals* equipment, const Mat4 view, const Mat4 projection,
                           const DayNightState* daylight);

// Draw one skinned arm into the caller's currently bound camera-space target.
// The supplied transform places the unposed arm cuboid; base skin and sleeve use
// the arm's own atlas regions. Private-target depth is tested and written so a
// held item rendered in the same target can occlude/intersect the arm naturally.
// The helper preserves the GL state it changes and never clears or composites.
// Call with translucent=false for opaque depth, then true after all foreground
// opaque geometry for fractional sleeve alpha.
void renderPlayerViewArm(const PlayerRenderer* renderer, PlayerModelPart arm, const Mat4 projection, const Mat4 cameraTransform, const DayNightState* daylight, bool translucent);

// Draw the skin's right arm as a camera-space first-person view model. This pass
// never reads or writes the world depth buffer, leaving selection/raycast depth
// and later HUD rendering independent of the hand.
void renderPlayerHand(const PlayerRenderer* renderer, const PlayerModelPose* pose, float aspect, const DayNightState* daylight);

#endif
