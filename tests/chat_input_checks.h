#include <string.h>

static Vec3 beforeChatPosition;
static unsigned beforeChatTick;
static int beforeChatSlot;
static bool beforeChatDebug, beforeChatWireframe;

static void chatFrame(GLFWwindow* window) {
  InputState* input = glfwGetWindowUserPointer(window);
  GLFWkeyfun key = glfwSetKeyCallback(window, NULL);
  GLFWcharfun character = glfwSetCharCallback(window, NULL);
  glfwSetKeyCallback(window, key);
  glfwSetCharCallback(window, character);
  CHECK(key && character);
  if (frame == 74) {
    beforeChatPosition = input->camera->position;
    beforeChatTick = input->clock.tick;
    beforeChatSlot = selectedHotbarSlot();
    beforeChatDebug = input->showDebug;
    beforeChatWireframe = input->wireframe;
    input->saveRequested = false;
    mouseButtonCallback(window, GLFW_MOUSE_BUTTON_LEFT, GLFW_PRESS, 0);
    key(window, GLFW_KEY_ENTER, 0, GLFW_PRESS, 0);
    CHECK(input->chat.open && !input->breakHeld && !input->jumpRequested);
    key(window, GLFW_KEY_ENTER, 0, GLFW_REPEAT, 0);
    const char* text = "/time set nighx";
    for (; *text; text++)
      character(window, (unsigned char)*text);
    key(window, GLFW_KEY_BACKSPACE, 0, GLFW_REPEAT, 0);
    character(window, 't');
    CHECK(!strcmp(input->chat.input, "/time set night"));
    const int blocked[] = {GLFW_KEY_1, GLFW_KEY_F, GLFW_KEY_F3, GLFW_KEY_F4, GLFW_KEY_F5, GLFW_KEY_SPACE, GLFW_KEY_W};
    for (size_t i = 0; i < sizeof(blocked) / sizeof(blocked[0]); i++)
      key(window, blocked[i], 0, GLFW_PRESS, 0);
    CHECK(input->flying && !input->saveRequested && !input->jumpRequested && !input->runInput.running);
    CHECK(selectedHotbarSlot() == beforeChatSlot && input->showDebug == beforeChatDebug && input->wireframe == beforeChatWireframe);
    float yaw = input->camera->yaw, pitch = input->camera->pitch;
    mouseCallback(window, 6000, 7000);
    mouseCallback(window, 9000, 11000);
    CHECK(input->camera->yaw == yaw && input->camera->pitch == pitch);
    mouseButtonCallback(window, GLFW_MOUSE_BUTTON_LEFT, GLFW_PRESS, 0);
    mouseButtonCallback(window, GLFW_MOUSE_BUTTON_RIGHT, GLFW_PRESS, 0);
    CHECK(!input->breakHeld);
    pressedKey = GLFW_KEY_W;
  }
  if (frame == 75) {
    CHECK(input->clock.tick == beforeChatTick && input->chat.open && !input->chat.count);
    CHECK(vec3_distance(&input->camera->position, &beforeChatPosition) == 0);
    pressedKey = -1;
    key(window, GLFW_KEY_ENTER, 0, GLFW_PRESS, 0);
    CHECK(input->clock.tick == 18000 && !input->chat.open && input->chat.count == 1);
  }
  if (frame == 76 || frame == 77 || frame == 79 || frame == 80) {
    const char* text = frame == 76 ? "/time set day" : frame == 77 ? "/time set 1200" : frame == 79 ? "/time set 24000" : "hello local world";
    key(window, GLFW_KEY_ENTER, 0, GLFW_PRESS, 0);
    for (; *text; text++)
      character(window, (unsigned char)*text);
    if (frame != 77)
      key(window, GLFW_KEY_ENTER, 0, GLFW_PRESS, 0);
    if (frame == 79)
      CHECK(input->clock.tick == 1200 && strstr(input->chat.messages[input->chat.count - 1], "Use /time set"));
    if (frame == 80)
      CHECK(strstr(input->chat.messages[input->chat.count - 1], "[Local] hello local world"));
  }
  if (frame == 78)
    key(window, GLFW_KEY_ENTER, 0, GLFW_PRESS, 0);
  if (frame == 81) {
    key(window, GLFW_KEY_ENTER, 0, GLFW_PRESS, 0);
    const char* text = "/time set day";
    for (; *text; text++)
      character(window, (unsigned char)*text);
    focused = GLFW_FALSE;
    windowFocusCallback(window, focused);
    character(window, 'x');
    key(window, GLFW_KEY_ENTER, 0, GLFW_PRESS, 0);
    CHECK(input->chat.open && !strcmp(input->chat.input, "/time set day"));
    focused = GLFW_TRUE;
    iconified = true;
    character(window, 'x');
    key(window, GLFW_KEY_ENTER, 0, GLFW_PRESS, 0);
    iconified = false;
    zeroFramebuffer = true;
    character(window, 'x');
    key(window, GLFW_KEY_ENTER, 0, GLFW_PRESS, 0);
    zeroFramebuffer = false;
    CHECK(input->chat.open && !strcmp(input->chat.input, "/time set day") && input->clock.tick == 1200);
  }
  if (frame == 82) {
    key(window, GLFW_KEY_ESCAPE, 0, GLFW_PRESS, 0);
    CHECK(!input->chat.open && input->clock.tick == 1200 && cursorMode == GLFW_CURSOR_NORMAL);
    key(window, GLFW_KEY_KP_ENTER, 0, GLFW_PRESS, 0);
    const char* text = "/time set night";
    for (; *text; text++)
      character(window, (unsigned char)*text);
    key(window, GLFW_KEY_KP_ENTER, 0, GLFW_PRESS, 0);
    key(window, GLFW_KEY_ENTER, 0, GLFW_REPEAT, 0);
    CHECK(!input->chat.open && input->clock.tick == 18000 && cursorMode == GLFW_CURSOR_NORMAL);
  }
  if (frame == 83) {
    key(window, GLFW_KEY_ESCAPE, 0, GLFW_PRESS, 0);
    key(window, GLFW_KEY_ENTER, 0, GLFW_PRESS, 0);
    key(window, GLFW_KEY_ESCAPE, 0, GLFW_PRESS, 0);
    CHECK(cursorMode == GLFW_CURSOR_DISABLED && !input->chat.open);
    float yaw = input->camera->yaw, pitch = input->camera->pitch;
    mouseCallback(window, -40000, 40000);
    CHECK(input->camera->yaw == yaw && input->camera->pitch == pitch);
  }
}
