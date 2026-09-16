#include "inventory_ui.h"
#include "../utils/text.h"
#include <GL/freeglut.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

enum {
  INVENTORY_UI_SLOT_SIZE = 18,
  INVENTORY_UI_PREVIEW_X = 26,
  INVENTORY_UI_PREVIEW_Y = 8,
  INVENTORY_UI_PREVIEW_WIDTH = 49,
  INVENTORY_UI_PREVIEW_HEIGHT = 70,
};

static int roundedEdge(const InventoryUILayout* layout, int logical) {
  return (int)lroundf(logical * layout->scale);
}

static InventoryUIRect logicalRect(const InventoryUILayout* layout, int x, int y, int width, int height) {
  int left = layout->panel.x + roundedEdge(layout, x);
  int top = layout->panel.y + roundedEdge(layout, y);
  int right = layout->panel.x + roundedEdge(layout, x + width);
  int bottom = layout->panel.y + roundedEdge(layout, y + height);
  return (InventoryUIRect){left, top, right - left, bottom - top};
}

static bool pointInRect(const InventoryUIRect* rect, int x, int y) {
  return rect && x >= rect->x && y >= rect->y && x < rect->x + rect->width && y < rect->y + rect->height;
}

bool inventoryUILayout(int framebufferWidth, int framebufferHeight, InventoryUILayout* layout) {
  if (!layout || framebufferWidth <= 0 || framebufferHeight <= 0)
    return false;

  float fit = fminf((float)framebufferWidth / INVENTORY_UI_LOGICAL_WIDTH, (float)framebufferHeight / INVENTORY_UI_LOGICAL_HEIGHT);
  float scale;
  if (fit < 1.0f) {
    scale = fit;
  } else {
    scale = floorf(fminf((float)framebufferWidth / 320.0f, (float)framebufferHeight / 240.0f));
    if (scale < 1.0f)
      scale = 1.0f;
  }
  if (!isfinite(scale) || scale <= 0)
    return false;

  int width = (int)lroundf(INVENTORY_UI_LOGICAL_WIDTH * scale);
  int height = (int)lroundf(INVENTORY_UI_LOGICAL_HEIGHT * scale);
  if (width > framebufferWidth)
    width = framebufferWidth;
  if (height > framebufferHeight)
    height = framebufferHeight;
  *layout = (InventoryUILayout){
      .framebufferWidth = framebufferWidth,
      .framebufferHeight = framebufferHeight,
      .panel = {(framebufferWidth - width) / 2, (framebufferHeight - height) / 2, width, height},
      .scale = scale,
  };
  return true;
}

static bool slotLogicalRect(InventorySlotRef slot, int* x, int* y) {
  if (!x || !y)
    return false;
  switch (slot.kind) {
  case INVENTORY_SLOT_CARRIED:
    if (slot.index >= INVENTORY_CARRIED_SLOT_COUNT)
      return false;
    if (slot.index < INVENTORY_HOTBAR_SLOT_COUNT) {
      *x = 8 + slot.index * INVENTORY_UI_SLOT_SIZE;
      *y = 142;
      return true;
    }
    slot.index -= INVENTORY_HOTBAR_SLOT_COUNT;
    *x = 8 + (slot.index % 9) * INVENTORY_UI_SLOT_SIZE;
    *y = 84 + (slot.index / 9) * INVENTORY_UI_SLOT_SIZE;
    return true;
  case INVENTORY_SLOT_ARMOR:
    if (slot.index >= INVENTORY_ARMOR_SLOT_COUNT)
      return false;
    *x = 8;
    *y = 8 + slot.index * INVENTORY_UI_SLOT_SIZE;
    return true;
  case INVENTORY_SLOT_OFFHAND:
    if (slot.index != 0)
      return false;
    *x = 77;
    *y = 62;
    return true;
  case INVENTORY_SLOT_CRAFTING:
    if (slot.index >= INVENTORY_CRAFTING_SLOT_COUNT)
      return false;
    *x = 98 + (slot.index % 2) * INVENTORY_UI_SLOT_SIZE;
    *y = 18 + (slot.index / 2) * INVENTORY_UI_SLOT_SIZE;
    return true;
  case INVENTORY_SLOT_RESULT:
    if (slot.index != 0)
      return false;
    *x = 154;
    *y = 28;
    return true;
  case INVENTORY_SLOT_CURSOR:
  default:
    return false;
  }
}

bool inventoryUISlotRect(const InventoryUILayout* layout, InventorySlotRef slot, InventoryUIRect* rect) {
  if (!layout || !rect || layout->scale <= 0)
    return false;
  int x, y;
  if (!slotLogicalRect(slot, &x, &y))
    return false;
  *rect = logicalRect(layout, x, y, INVENTORY_UI_SLOT_SIZE, INVENTORY_UI_SLOT_SIZE);
  return true;
}

bool inventoryUIHitTest(const InventoryUILayout* layout, int mouseX, int mouseY, InventorySlotRef* slot) {
  if (!layout || !slot)
    return false;
  static const InventorySlotKind kinds[] = {INVENTORY_SLOT_RESULT, INVENTORY_SLOT_CRAFTING, INVENTORY_SLOT_ARMOR, INVENTORY_SLOT_OFFHAND, INVENTORY_SLOT_CARRIED};
  for (size_t kindIndex = 0; kindIndex < sizeof(kinds) / sizeof(*kinds); kindIndex++) {
    int count = 0;
    switch (kinds[kindIndex]) {
    case INVENTORY_SLOT_RESULT:
    case INVENTORY_SLOT_OFFHAND:
      count = 1;
      break;
    case INVENTORY_SLOT_CRAFTING:
      count = INVENTORY_CRAFTING_SLOT_COUNT;
      break;
    case INVENTORY_SLOT_ARMOR:
      count = INVENTORY_ARMOR_SLOT_COUNT;
      break;
    case INVENTORY_SLOT_CARRIED:
      count = INVENTORY_CARRIED_SLOT_COUNT;
      break;
    default:
      break;
    }
    for (int index = 0; index < count; index++) {
      InventorySlotRef candidate = {kinds[kindIndex], (uint8_t)index};
      InventoryUIRect rect;
      if (inventoryUISlotRect(layout, candidate, &rect) && pointInRect(&rect, mouseX, mouseY)) {
        *slot = candidate;
        return true;
      }
    }
  }
  return false;
}

bool inventoryUIInit(InventoryUI* ui) {
  if (!ui)
    return false;
  *ui = (InventoryUI){0};
  glGenFramebuffers(1, &ui->previewFramebuffer);
  glGenTextures(1, &ui->previewTexture);
  glGenRenderbuffers(1, &ui->previewDepth);
  if (ui->previewFramebuffer && ui->previewTexture && ui->previewDepth)
    return true;
  inventoryUICleanup(ui);
  return false;
}

void inventoryUICleanup(InventoryUI* ui) {
  if (!ui)
    return;
  glDeleteRenderbuffers(1, &ui->previewDepth);
  glDeleteTextures(1, &ui->previewTexture);
  glDeleteFramebuffers(1, &ui->previewFramebuffer);
  *ui = (InventoryUI){0};
}

static bool resizePreview(InventoryUI* ui, int width, int height) {
  if (!ui || !ui->previewFramebuffer || !ui->previewTexture || !ui->previewDepth || width <= 0 || height <= 0)
    return false;
  if (ui->previewWidth == width && ui->previewHeight == height)
    return true;

  GLint activeTexture, texture, drawFramebuffer, readFramebuffer, renderbuffer, unpackBuffer;
  glGetIntegerv(GL_ACTIVE_TEXTURE, &activeTexture);
  glActiveTexture(GL_TEXTURE0);
  glGetIntegerv(GL_TEXTURE_BINDING_2D, &texture);
  glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &drawFramebuffer);
  glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &readFramebuffer);
  glGetIntegerv(GL_RENDERBUFFER_BINDING, &renderbuffer);
  glGetIntegerv(GL_PIXEL_UNPACK_BUFFER_BINDING, &unpackBuffer);

  glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
  glBindTexture(GL_TEXTURE_2D, ui->previewTexture);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
  glBindRenderbuffer(GL_RENDERBUFFER, ui->previewDepth);
  glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, width, height);
  glBindFramebuffer(GL_FRAMEBUFFER, ui->previewFramebuffer);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, ui->previewTexture, 0);
  glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, ui->previewDepth);
  glDrawBuffer(GL_COLOR_ATTACHMENT0);
  glReadBuffer(GL_COLOR_ATTACHMENT0);
  bool ready = glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;

  glBindFramebuffer(GL_DRAW_FRAMEBUFFER, (GLuint)drawFramebuffer);
  glBindFramebuffer(GL_READ_FRAMEBUFFER, (GLuint)readFramebuffer);
  glBindRenderbuffer(GL_RENDERBUFFER, (GLuint)renderbuffer);
  glBindBuffer(GL_PIXEL_UNPACK_BUFFER, (GLuint)unpackBuffer);
  glBindTexture(GL_TEXTURE_2D, (GLuint)texture);
  glActiveTexture((GLenum)activeTexture);
  if (!ready)
    return false;
  ui->previewWidth = width;
  ui->previewHeight = height;
  return true;
}

static PlayerEquipmentVisuals inventoryEquipment(const Inventory* inventory) {
  PlayerEquipmentVisuals equipment = {0};
  if (!inventory)
    return equipment;
  equipment.helmet = inventory->armor[INVENTORY_ARMOR_HEAD].count && inventoryItemArmorSlot(inventory->armor[INVENTORY_ARMOR_HEAD].item) == INVENTORY_ARMOR_HEAD;
  equipment.chestplate = inventory->armor[INVENTORY_ARMOR_CHEST].count && inventoryItemArmorSlot(inventory->armor[INVENTORY_ARMOR_CHEST].item) == INVENTORY_ARMOR_CHEST;
  equipment.leggings = inventory->armor[INVENTORY_ARMOR_LEGS].count && inventoryItemArmorSlot(inventory->armor[INVENTORY_ARMOR_LEGS].item) == INVENTORY_ARMOR_LEGS;
  equipment.boots = inventory->armor[INVENTORY_ARMOR_FEET].count && inventoryItemArmorSlot(inventory->armor[INVENTORY_ARMOR_FEET].item) == INVENTORY_ARMOR_FEET;
  return equipment;
}

static float clampUnit(float value) {
  return fmaxf(-1.0f, fminf(1.0f, value));
}

static bool renderPreview(InventoryUI* ui, const InventoryUIRect* rect, const Inventory* inventory, const PlayerRenderer* renderer, const PlayerModelPose* playerPose,
                          const DayNightState* daylight, int mouseX, int mouseY) {
  if (!ui || !rect || !inventory || !renderer || !playerPose || !daylight || rect->width < 2 || rect->height < 2 || !resizePreview(ui, rect->width, rect->height))
    return false;

  GLint drawFramebuffer, readFramebuffer, activeTexture;
  glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &drawFramebuffer);
  glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &readFramebuffer);
  glGetIntegerv(GL_ACTIVE_TEXTURE, &activeTexture);
  glPushAttrib(GL_ALL_ATTRIB_BITS);
  glBindFramebuffer(GL_FRAMEBUFFER, ui->previewFramebuffer);
  glViewport(0, 0, rect->width, rect->height);
  glDisable(GL_SCISSOR_TEST);
  glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
  glClearColor(0, 0, 0, 0);
  glClearDepth(1.0);
  glDepthMask(GL_TRUE);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  float centerX = rect->x + rect->width * 0.5f;
  float centerY = rect->y + rect->height * 0.52f;
  float lookX = clampUnit((mouseX - centerX) / fmaxf(rect->width * 0.7f, 1.0f));
  float lookY = clampUnit((mouseY - centerY) / fmaxf(rect->height * 0.7f, 1.0f));
  PlayerModelPose pose = *playerPose;
  pose.rootYaw = lookX * 0.45f;
  pose.parts[PLAYER_MODEL_HEAD].rotation.y = lookX * 0.30f;
  pose.parts[PLAYER_MODEL_HEAD].rotation.x = -lookY * 0.38f;

  Vec3 eye = {0, 0.9f, -4.0f};
  Vec3 center = {0, 0.9f, 0};
  Vec3 up = {0, 1, 0};
  Mat4 view, projection;
  mat4_lookAt(view, &eye, &center, &up);
  mat4_perspective(projection, 30.0f, (float)rect->width / rect->height, 0.1f, 20.0f);
  renderPlayerModel(renderer, (Vec3){0}, &pose, view, projection, daylight);
  PlayerEquipmentVisuals equipment = inventoryEquipment(inventory);
  renderPlayerEquipment(renderer, (Vec3){0}, &pose, &equipment, view, projection, daylight);

  glBindFramebuffer(GL_DRAW_FRAMEBUFFER, (GLuint)drawFramebuffer);
  glBindFramebuffer(GL_READ_FRAMEBUFFER, (GLuint)readFramebuffer);
  glPopAttrib();
  glActiveTexture((GLenum)activeTexture);
  return true;
}

static float glBottom(const TextState* state, const InventoryUIRect* rect) {
  return (float)(state->viewport[3] - rect->y - rect->height);
}

static void fillRect(const TextState* state, const InventoryUIRect* rect, float r, float g, float b, float a) {
  float bottom = glBottom(state, rect);
  glColor4f(r, g, b, a);
  glBegin(GL_QUADS);
  glVertex2f((float)rect->x, bottom);
  glVertex2f((float)(rect->x + rect->width), bottom);
  glVertex2f((float)(rect->x + rect->width), bottom + rect->height);
  glVertex2f((float)rect->x, bottom + rect->height);
  glEnd();
}

static void drawTexturedRect(const TextState* state, const InventoryUIRect* rect, GLuint texture, bool topOriginSource) {
  if (!texture || rect->width <= 0 || rect->height <= 0)
    return;
  float bottom = glBottom(state, rect);
  glEnable(GL_TEXTURE_2D);
  glBindTexture(GL_TEXTURE_2D, texture);
  glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
  glColor4f(1, 1, 1, 1);
  glBegin(GL_QUADS);
  glTexCoord2f(0, topOriginSource ? 1.0f : 0.0f);
  glVertex2f((float)rect->x, bottom);
  glTexCoord2f(1, topOriginSource ? 1.0f : 0.0f);
  glVertex2f((float)(rect->x + rect->width), bottom);
  glTexCoord2f(1, topOriginSource ? 0.0f : 1.0f);
  glVertex2f((float)(rect->x + rect->width), bottom + rect->height);
  glTexCoord2f(0, topOriginSource ? 0.0f : 1.0f);
  glVertex2f((float)rect->x, bottom + rect->height);
  glEnd();
  glDisable(GL_TEXTURE_2D);
}

static InventoryUIRect insetRect(InventoryUIRect rect, int inset) {
  rect.x += inset;
  rect.y += inset;
  rect.width -= inset * 2;
  rect.height -= inset * 2;
  if (rect.width < 0)
    rect.width = 0;
  if (rect.height < 0)
    rect.height = 0;
  return rect;
}

static void itemColor(uint16_t item, float* r, float* g, float* b) {
  uint32_t color = inventoryItemColor(item);
  *r = ((color >> 16) & 0xff) / 255.0f;
  *g = ((color >> 8) & 0xff) / 255.0f;
  *b = (color & 0xff) / 255.0f;
}

static void drawEquipmentIcon(const TextState* state, const InventoryUIRect* rect, uint16_t item) {
  if (rect->width < 8 || rect->height < 8)
    return;
  float r, g, b;
  itemColor(item, &r, &g, &b);
  InventoryUIRect area = insetRect(*rect, (rect->width + 11) / 12);
  int unit = area.width / 5;
  if (unit < 1)
    unit = 1;
  if (item == ITEM_LEATHER_HELMET) {
    InventoryUIRect crown = {area.x + unit, area.y + unit, area.width - unit * 2, area.height - unit * 2};
    fillRect(state, &crown, r, g, b, 1);
    InventoryUIRect opening = {crown.x + unit, crown.y + crown.height / 2, crown.width - unit * 2, crown.height / 2};
    fillRect(state, &opening, 0.12f, 0.12f, 0.12f, 1);
  } else if (item == ITEM_LEATHER_CHESTPLATE) {
    InventoryUIRect torso = {area.x + unit, area.y + unit * 2, area.width - unit * 2, area.height - unit * 3};
    InventoryUIRect shoulder = {area.x, area.y + unit, area.width, unit * 2};
    fillRect(state, &torso, r, g, b, 1);
    fillRect(state, &shoulder, r, g, b, 1);
  } else if (item == ITEM_LEATHER_LEGGINGS) {
    InventoryUIRect waist = {area.x + unit, area.y + unit, area.width - unit * 2, unit * 2};
    InventoryUIRect left = {area.x + unit, area.y + unit * 3, unit * 2, area.height - unit * 4};
    InventoryUIRect right = {area.x + area.width - unit * 3, area.y + unit * 3, unit * 2, area.height - unit * 4};
    fillRect(state, &waist, r, g, b, 1);
    fillRect(state, &left, r, g, b, 1);
    fillRect(state, &right, r, g, b, 1);
  } else if (item == ITEM_LEATHER_BOOTS) {
    InventoryUIRect left = {area.x + unit, area.y + area.height / 2, unit * 2, area.height / 2 - unit};
    InventoryUIRect right = {area.x + area.width - unit * 3, area.y + area.height / 2, unit * 2, area.height / 2 - unit};
    fillRect(state, &left, r, g, b, 1);
    fillRect(state, &right, r, g, b, 1);
  } else {
    fillRect(state, &area, r, g, b, 1);
  }
}

static void drawStackIcon(const TextState* state, const InventoryUIRect* rect, ItemStack stack, const GLuint blockTextures[INVENTORY_UI_BLOCK_TEXTURE_COUNT]) {
  if (!stack.count || stack.item == ITEM_NONE)
    return;
  int inset = rect->width / INVENTORY_UI_SLOT_SIZE;
  if (inset < 1)
    inset = 1;
  InventoryUIRect icon = insetRect(*rect, inset);
  if (stack.item >= ITEM_GRASS_BLOCK && stack.item <= ITEM_STONE_BRICKS && blockTextures)
    drawTexturedRect(state, &icon, blockTextures[stack.item - ITEM_GRASS_BLOCK], true);
  else
    drawEquipmentIcon(state, &icon, stack.item);
}

static TextState countTextState(const TextState* state, float scale) {
  TextState text = *state;
  text.font = scale >= 2.5f ? GLUT_BITMAP_HELVETICA_18 : scale >= 1.5f ? GLUT_BITMAP_HELVETICA_12 : GLUT_BITMAP_HELVETICA_10;
  text.fontHeight = glutBitmapHeight(text.font);
  return text;
}

static void drawStackCount(const TextState* state, const InventoryUIRect* rect, ItemStack stack) {
  if (stack.count <= 1)
    return;
  char count[8];
  snprintf(count, sizeof(count), "%u", (unsigned)stack.count);
  int width = textWidth(state, count);
  if (width > rect->width || state->fontHeight + 2 > rect->height)
    return;
  int margin = rect->width - width >= 2 ? 1 : 0;
  float x = rect->x + rect->width - width - margin;
  float baseline = rect->y + rect->height - 1;
  if (x < 0 || x + width > state->viewport[2] || baseline < 0 || baseline > state->viewport[3])
    return;
  glColor3f(0.05f, 0.05f, 0.05f);
  renderText(state, count, x + 1, baseline + 1);
  glColor3f(1, 1, 1);
  renderText(state, count, x, baseline);
}

static ItemStack stackForSlot(const Inventory* inventory, InventorySlotRef slot) {
  return slot.kind == INVENTORY_SLOT_RESULT ? inventoryCraftResult(inventory) : inventoryGet(inventory, slot);
}

static void drawSlot(const TextState* state, const InventoryUIRect* rect, bool hovered) {
  if (rect->width <= 0 || rect->height <= 0)
    return;
  if (rect->width < 3 || rect->height < 3) {
    fillRect(state, rect, hovered ? 0.50f : 0.36f, hovered ? 0.50f : 0.36f, hovered ? 0.50f : 0.36f, 1);
    return;
  }
  int bevel = rect->width / INVENTORY_UI_SLOT_SIZE;
  if (bevel < 1)
    bevel = 1;
  int maximumBevel = (rect->width < rect->height ? rect->width : rect->height) / 2;
  if (bevel > maximumBevel)
    bevel = maximumBevel;
  fillRect(state, rect, 0.50f, 0.50f, 0.50f, 1);
  InventoryUIRect top = {rect->x, rect->y, rect->width, bevel};
  InventoryUIRect left = {rect->x, rect->y, bevel, rect->height};
  InventoryUIRect bottom = {rect->x, rect->y + rect->height - bevel, rect->width, bevel};
  InventoryUIRect right = {rect->x + rect->width - bevel, rect->y, bevel, rect->height};
  fillRect(state, &top, 0.20f, 0.20f, 0.20f, 1);
  fillRect(state, &left, 0.20f, 0.20f, 0.20f, 1);
  fillRect(state, &bottom, 0.78f, 0.78f, 0.78f, 1);
  fillRect(state, &right, 0.78f, 0.78f, 0.78f, 1);
  InventoryUIRect inner = insetRect(*rect, bevel);
  fillRect(state, &inner, hovered ? 0.50f : 0.36f, hovered ? 0.50f : 0.36f, hovered ? 0.50f : 0.36f, 1);
}

static void drawSlotHint(const TextState* state, const InventoryUIRect* rect, const char* hint) {
  int width = textWidth(state, hint);
  if (width + 2 > rect->width || state->fontHeight + 2 > rect->height)
    return;
  glColor3f(0.63f, 0.63f, 0.63f);
  renderText(state, hint, rect->x + (rect->width - width) * 0.5f, rect->y + rect->height * 0.67f);
}

static void drawSectionLabel(const TextState* state, const InventoryUILayout* layout, const char* label, int logicalX, int logicalBaseline, int logicalWidth) {
  int x = layout->panel.x + roundedEdge(layout, logicalX);
  int baseline = layout->panel.y + roundedEdge(layout, logicalBaseline);
  int available = roundedEdge(layout, logicalWidth);
  if (state->fontHeight + 2 > roundedEdge(layout, 12) || textWidth(state, label) > available)
    return;
  glColor3f(0.24f, 0.24f, 0.24f);
  renderText(state, label, (float)x, (float)baseline);
}

static void drawCraftArrow(const TextState* state, const InventoryUILayout* layout) {
  InventoryUIRect shaft = logicalRect(layout, 136, 31, 12, 4);
  InventoryUIRect head = logicalRect(layout, 146, 28, 5, 10);
  fillRect(state, &shaft, 0.34f, 0.34f, 0.34f, 1);
  float bottom = glBottom(state, &head);
  glColor4f(0.34f, 0.34f, 0.34f, 1);
  glBegin(GL_TRIANGLES);
  glVertex2f((float)head.x, bottom);
  glVertex2f((float)head.x, bottom + head.height);
  glVertex2f((float)(head.x + head.width), bottom + head.height * 0.5f);
  glEnd();
}

static bool fitTooltipLabel(const TextState* state, const char* name, int availableWidth, char label[64]) {
  if (!name || !*name || availableWidth <= 0)
    return false;
  snprintf(label, 64, "%s", name);
  if (textWidth(state, label) <= availableWidth)
    return true;
  const char* dots = "...";
  int dotsWidth = textWidth(state, dots);
  if (dotsWidth > availableWidth)
    return false;
  size_t length = strlen(label);
  while (length && textWidth(state, label) + dotsWidth > availableWidth)
    label[--length] = '\0';
  if (!length)
    return false;
  strncat(label, dots, 63 - strlen(label));
  return true;
}

static void drawTooltip(const TextState* state, const InventoryUILayout* layout, ItemStack stack, int mouseX, int mouseY) {
  if (!stack.count || stack.item == ITEM_NONE)
    return;
  const char* name = inventoryItemName(stack.item);
  if (!name || !*name)
    return;
  int padding = 4;
  if (layout->framebufferWidth <= padding * 2 || layout->framebufferHeight < state->fontHeight + padding * 2)
    return;
  char label[64];
  if (!fitTooltipLabel(state, name, layout->framebufferWidth - padding * 2, label))
    return;
  int width = textWidth(state, label) + padding * 2;
  int height = state->fontHeight + padding * 2;
  int x = mouseX + 10;
  int y = mouseY + 10;
  if (x + width > layout->framebufferWidth)
    x = mouseX - width - 10;
  if (y + height > layout->framebufferHeight)
    y = mouseY - height - 10;
  x = x < 0 ? 0 : x > layout->framebufferWidth - width ? layout->framebufferWidth - width : x;
  y = y < 0 ? 0 : y > layout->framebufferHeight - height ? layout->framebufferHeight - height : y;
  InventoryUIRect rect = {x, y, width, height};
  fillRect(state, &rect, 0.04f, 0.02f, 0.07f, 0.96f);
  InventoryUIRect inner = insetRect(rect, 1);
  fillRect(state, &inner, 0.12f, 0.06f, 0.18f, 0.98f);
  glColor3f(1, 1, 1);
  renderText(state, label, x + padding, y + padding + state->fontHeight - 2);
}

void inventoryUIDraw(InventoryUI* ui, const Inventory* inventory, const PlayerRenderer* playerRenderer, const PlayerModelPose* playerPose, const DayNightState* daylight,
                     const GLuint blockTextures[INVENTORY_UI_BLOCK_TEXTURE_COUNT], int framebufferWidth, int framebufferHeight, int mouseX, int mouseY) {
  if (!ui || !inventory || framebufferWidth <= 0 || framebufferHeight <= 0)
    return;
  InventoryUILayout layout;
  if (!inventoryUILayout(framebufferWidth, framebufferHeight, &layout))
    return;

  InventoryUIRect preview = logicalRect(&layout, INVENTORY_UI_PREVIEW_X, INVENTORY_UI_PREVIEW_Y, INVENTORY_UI_PREVIEW_WIDTH, INVENTORY_UI_PREVIEW_HEIGHT);
  bool previewReady = renderPreview(ui, &preview, inventory, playerRenderer, playerPose, daylight, mouseX, mouseY);

  GLint activeTexture, texture0, sampler0;
  glGetIntegerv(GL_ACTIVE_TEXTURE, &activeTexture);
  glActiveTexture(GL_TEXTURE0);
  glGetIntegerv(GL_TEXTURE_BINDING_2D, &texture0);
  glGetIntegerv(GL_SAMPLER_BINDING, &sampler0);
  glActiveTexture((GLenum)activeTexture);
  glPushAttrib(GL_ALL_ATTRIB_BITS);
  glViewport(0, 0, framebufferWidth, framebufferHeight);
  glDisable(GL_SCISSOR_TEST);
  glActiveTexture(GL_TEXTURE0);
  glBindSampler(0, 0);
  glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

  TextState state;
  beginText(&state);
  glDisable(GL_DEPTH_TEST);
  glDisable(GL_CULL_FACE);
  glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glBlendEquation(GL_FUNC_ADD);
  glDisable(GL_TEXTURE_2D);

  InventoryUIRect screen = {0, 0, framebufferWidth, framebufferHeight};
  fillRect(&state, &screen, 0, 0, 0, 0.48f);
  fillRect(&state, &layout.panel, 0.72f, 0.72f, 0.72f, 1);
  int panelInset = (int)lroundf(layout.scale);
  if (panelInset < 1)
    panelInset = 1;
  InventoryUIRect panelInner = insetRect(layout.panel, panelInset);
  fillRect(&state, &panelInner, 0.78f, 0.78f, 0.78f, 1);
  fillRect(&state, &preview, 0.15f, 0.15f, 0.15f, 1);
  if (previewReady)
    drawTexturedRect(&state, &preview, ui->previewTexture, false);
  drawCraftArrow(&state, &layout);

  InventorySlotRef hovered = {0};
  bool hasHovered = inventoryUIHitTest(&layout, mouseX, mouseY, &hovered);

  const struct {
    InventorySlotKind kind;
    int count;
  } groups[] = {
      {INVENTORY_SLOT_ARMOR, INVENTORY_ARMOR_SLOT_COUNT},     {INVENTORY_SLOT_OFFHAND, 1}, {INVENTORY_SLOT_CRAFTING, INVENTORY_CRAFTING_SLOT_COUNT}, {INVENTORY_SLOT_RESULT, 1},
      {INVENTORY_SLOT_CARRIED, INVENTORY_CARRIED_SLOT_COUNT},
  };

  TextState smallText = countTextState(&state, layout.scale);
  drawSectionLabel(&smallText, &layout, "Crafting", 98, 15, 54);
  for (size_t group = 0; group < sizeof(groups) / sizeof(*groups); group++)
    for (int index = 0; index < groups[group].count; index++) {
      InventorySlotRef slot = {groups[group].kind, (uint8_t)index};
      InventoryUIRect rect;
      if (!inventoryUISlotRect(&layout, slot, &rect))
        continue;
      bool isHovered = hasHovered && hovered.kind == slot.kind && hovered.index == slot.index;
      drawSlot(&state, &rect, isHovered);
      ItemStack stack = stackForSlot(inventory, slot);
      if (!stack.count && slot.kind == INVENTORY_SLOT_ARMOR) {
        static const char* hints[] = {"H", "C", "L", "B"};
        drawSlotHint(&smallText, &rect, hints[slot.index]);
      } else if (!stack.count && slot.kind == INVENTORY_SLOT_OFFHAND) {
        drawSlotHint(&smallText, &rect, "O");
      }
      drawStackIcon(&state, &rect, stack, blockTextures);
      drawStackCount(&smallText, &rect, stack);
    }

  ItemStack cursor = inventory->cursor;
  if (cursor.count) {
    int size = (int)lroundf(INVENTORY_UI_SLOT_SIZE * layout.scale);
    if (size < 8)
      size = 8;
    InventoryUIRect cursorRect = {mouseX - size / 2, mouseY - size / 2, size, size};
    drawStackIcon(&state, &cursorRect, cursor, blockTextures);
    drawStackCount(&smallText, &cursorRect, cursor);
  } else if (hasHovered) {
    drawTooltip(&smallText, &layout, stackForSlot(inventory, hovered), mouseX, mouseY);
  }

  endText(&state);
  glPopAttrib();
  glActiveTexture(GL_TEXTURE0);
  glBindSampler(0, (GLuint)sampler0);
  glBindTexture(GL_TEXTURE_2D, (GLuint)texture0);
  glActiveTexture((GLenum)activeTexture);
}
