#ifndef CHAT_H
#define CHAT_H

#include "day_night.h"
#include <stddef.h>

#define CHAT_INPUT_CAPACITY 128
#define CHAT_MESSAGE_CAPACITY 160
#define CHAT_HISTORY_CAPACITY 8

typedef struct {
  bool open;
  char input[CHAT_INPUT_CAPACITY];
  size_t length, count;
  char messages[CHAT_HISTORY_CAPACITY][CHAT_MESSAGE_CAPACITY];
} Chat;

void openChat(Chat* chat);
void cancelChat(Chat* chat);
// The current bitmap font supports printable ASCII. Other codepoints are ignored.
bool appendChatCharacter(Chat* chat, unsigned codepoint);
void backspaceChat(Chat* chat);
void submitChat(Chat* chat, DayNightClock* clock);

#endif
