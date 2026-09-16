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
  GLint shellScaleLocation;
  GLint viewModelLocation;
  GLint viewModelOffsetLocation;
  GLint viewModelScaleLocation;
  GLint lightDirectionLocation;
  GLint lightColorLocation;
  GLint skyColorLocation;
  GLint groundColorLocation;
  GLint outerLayerLocation;
  GLint outerPassLocation;
  GLint skinLocation;
  bool fractionalAlpha;
} PlayerRenderer;

// Initializes a renderer from one exact 64x64 RGBA skin. The skin uses source
// PNG coordinates with a top-left origin and is sampled nearest/clamped.
bool initPlayerRenderer(PlayerRenderer* renderer, const char* skinPath);
void cleanupPlayerRenderer(PlayerRenderer* renderer);

// Draw the six-part avatar in world space. Caller supplies the same view and
// projection used for terrain; the pass preserves every OpenGL state it changes.
void renderPlayerModel(const PlayerRenderer* renderer, Vec3 feet, const PlayerModelPose* pose, const Mat4 view, const Mat4 projection, const DayNightState* daylight);

// Draw the skin's right arm as a camera-space first-person view model. This pass
// never reads or writes the world depth buffer, leaving selection/raycast depth
// and later HUD rendering independent of the hand.
void renderPlayerHand(const PlayerRenderer* renderer, const PlayerModelPose* pose, float aspect, const DayNightState* daylight);

#endif
