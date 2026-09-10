#include "world/chat.h"

static void typeChat(Chat* chat, const char* text) {
  openChat(chat);
  for (; *text; text++)
    CHECK(appendChatCharacter(chat, (unsigned char)*text));
}

static void test_chat(void) {
  Chat chat = {0};
  DayNightClock clock;
  initDayNight(&clock);
  CHECK(!appendChatCharacter(&chat, 'x'));
  typeChat(&chat, "/time set night");
  clock.remainder = 0.5;
  clock.active = true;
  submitChat(&chat, &clock);
  CHECK(clock.tick == 18000 && clock.remainder == 0 && !clock.active);
  CHECK(!chat.open && !chat.length && chat.count == 1 && strstr(chat.messages[0], "18000"));
  const char* valid[] = {"/time set day", "/time set 1200", " /time   set 0  ", "/time set 23999"};
  const unsigned ticks[] = {6000, 1200, 0, 23999};
  for (int i = 0; i < 4; i++) {
    typeChat(&chat, valid[i]);
    submitChat(&chat, &clock);
    CHECK(clock.tick == ticks[i]);
  }
  const char* invalid[] = {"/time",
                           "/time set",
                           "/time set -1",
                           "/time set +1",
                           "/time set 24000",
                           "/time set 1.2",
                           "/time set 12x",
                           "/time set 1200 extra",
                           "/time set 99999999999999999999999999999",
                           "/time set Night",
                           "/unknown"};
  for (size_t i = 0; i < sizeof(invalid) / sizeof(invalid[0]); i++) {
    clock.remainder = 0.25;
    typeChat(&chat, invalid[i]);
    submitChat(&chat, &clock);
    CHECK(clock.tick == 23999 && clock.remainder == 0.25);
    CHECK(chat.count && strstr(chat.messages[chat.count - 1], "Use /time set"));
  }
  typeChat(&chat, "hello local world");
  submitChat(&chat, &clock);
  CHECK(chat.count && strstr(chat.messages[chat.count - 1], "[Local] hello local world"));
  size_t count = chat.count;
  typeChat(&chat, "   ");
  submitChat(&chat, &clock);
  CHECK(chat.count == count && !chat.open);
  typeChat(&chat, "/time set day");
  cancelChat(&chat);
  submitChat(&chat, &clock);
  CHECK(clock.tick == 23999 && chat.count == count && chat.input[0] == '\0');
  openChat(&chat);
  CHECK(!appendChatCharacter(&chat, '\n') && !appendChatCharacter(&chat, 0) && !appendChatCharacter(&chat, 0x1f600));
  for (int i = 0; i < CHAT_INPUT_CAPACITY - 1; i++)
    CHECK(appendChatCharacter(&chat, 'w'));
  CHECK(!appendChatCharacter(&chat, 'x') && chat.length == CHAT_INPUT_CAPACITY - 1 && strlen(chat.input) == chat.length);
  backspaceChat(&chat);
  CHECK(appendChatCharacter(&chat, 'z') && chat.input[CHAT_INPUT_CAPACITY - 2] == 'z');
  for (int i = 0; i < CHAT_INPUT_CAPACITY + 1; i++)
    backspaceChat(&chat);
  CHECK(chat.length == 0 && chat.input[0] == '\0');
  cancelChat(&chat);
  chat = (Chat){0};
  for (int i = 0; i < 10; i++) {
    char message[16];
    snprintf(message, sizeof(message), "message %d", i);
    typeChat(&chat, message);
    submitChat(&chat, &clock);
  }
  CHECK(chat.count == CHAT_HISTORY_CAPACITY);
  CHECK(!strcmp(chat.messages[0], "[Local] message 2") && !strcmp(chat.messages[7], "[Local] message 9"));
  puts("Local chat text, history, command validation, and clock checks finished");
}
