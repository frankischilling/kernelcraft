#include "chat.h"
#include <stdio.h>
#include <string.h>

void openChat(Chat* chat) {
  chat->open = true;
}

void cancelChat(Chat* chat) {
  chat->open = false;
  chat->length = 0;
  chat->input[0] = '\0';
}

bool appendChatCharacter(Chat* chat, unsigned codepoint) {
  if (!chat->open || codepoint < 32 || codepoint > 126 || chat->length >= CHAT_INPUT_CAPACITY - 1)
    return false;
  chat->input[chat->length++] = (char)codepoint;
  chat->input[chat->length] = '\0';
  return true;
}

void backspaceChat(Chat* chat) {
  if (chat->open && chat->length)
    chat->input[--chat->length] = '\0';
}

static char* nextMessage(Chat* chat) {
  if (chat->count == CHAT_HISTORY_CAPACITY) {
    memmove(chat->messages, chat->messages + 1, (CHAT_HISTORY_CAPACITY - 1) * sizeof(chat->messages[0]));
    chat->count--;
  }
  return chat->messages[chat->count++];
}

static bool timeCommand(const char* text, unsigned* tick) {
  char command[16], action[16], argument[CHAT_INPUT_CAPACITY], extra[2];
  if (sscanf(text, "%15s %15s %127s %1s", command, action, argument, extra) != 3 || strcmp(command, "/time") || strcmp(action, "set"))
    return false;
  if (!strcmp(argument, "day")) {
    *tick = 6000;
    return true;
  }
  if (!strcmp(argument, "night")) {
    *tick = 18000;
    return true;
  }
  unsigned value = 0;
  for (const char* digit = argument; *digit; digit++) {
    if (*digit < '0' || *digit > '9' || value > (DAY_NIGHT_TICKS_PER_DAY - 1 - (unsigned)(*digit - '0')) / 10)
      return false;
    value = value * 10 + (unsigned)(*digit - '0');
  }
  *tick = value;
  return true;
}

static bool moonCommand(const char* text, MoonPhase* phase) {
  char command[16], action[16], argument[CHAT_INPUT_CAPACITY], extra[2];
  if (sscanf(text, "%15s %15s %127s %1s", command, action, argument, extra) != 3 || strcmp(command, "/moon") || strcmp(action, "set"))
    return false;
  for (int i = 0; i < MOON_PHASE_COUNT; i++) {
    if (!strcmp(argument, moonPhaseName((MoonPhase)i)) || (argument[0] == '0' + i && argument[1] == '\0')) {
      *phase = (MoonPhase)i;
      return true;
    }
  }
  return false;
}

void submitChat(Chat* chat, DayNightClock* clock) {
  if (!chat->open)
    return;
  const char* text = chat->input;
  while (*text == ' ')
    text++;
  if (*text == '/') {
    unsigned tick;
    MoonPhase phase;
    if (timeCommand(text, &tick)) {
      clock->tick = tick;
      clock->remainder = 0;
      clock->active = false;
      snprintf(nextMessage(chat), CHAT_MESSAGE_CAPACITY, "[System] Time set to %u.", tick);
    } else if (moonCommand(text, &phase)) {
      clock->moonPhase = phase;
      clock->remainder = 0;
      clock->active = false;
      snprintf(nextMessage(chat), CHAT_MESSAGE_CAPACITY, "[System] Moon set to %s.", moonPhaseName(phase));
    } else {
      snprintf(nextMessage(chat), CHAT_MESSAGE_CAPACITY, "[System] Use /time set day|night|0..23999 or /moon set 0..7 (0=full, 4=new)");
    }
  } else if (*text) {
    snprintf(nextMessage(chat), CHAT_MESSAGE_CAPACITY, "[Local] %s", text);
  }
  cancelChat(chat);
}
