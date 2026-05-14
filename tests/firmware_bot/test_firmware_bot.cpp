#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "BotPolicy.h"
#include "FirmwareBot.h"

static void test_channel_policy() {
  assert(BotPolicy::classifyChannel(NULL, 0, true) == BOT_CHANNEL_DM);
  assert(BotPolicy::decide(BOT_CHANNEL_DM) == BOT_POLICY_ALLOW_NORMAL);
  assert(BotPolicy::classifyChannel("Public", 6, false) == BOT_CHANNEL_PUBLIC);
  assert(BotPolicy::decide(BOT_CHANNEL_PUBLIC) == BOT_POLICY_IGNORE);
  assert(BotPolicy::classifyChannel("#bot", 4, false) == BOT_CHANNEL_BOT);
  assert(BotPolicy::decide(BOT_CHANNEL_BOT) == BOT_POLICY_ALLOW_NORMAL);
  assert(BotPolicy::classifyChannel("testing", 7, false) == BOT_CHANNEL_TESTING);
  assert(BotPolicy::decide(BOT_CHANNEL_TESTING) == BOT_POLICY_ALLOW_NORMAL);
  assert(BotPolicy::classifyChannel("#emergency", 10, false) == BOT_CHANNEL_EMERGENCY);
  assert(BotPolicy::decide(BOT_CHANNEL_EMERGENCY) == BOT_POLICY_EMERGENCY_FORWARD);
  assert(BotPolicy::classifyChannel("#botnet", 7, false) == BOT_CHANNEL_OTHER);
  assert(BotPolicy::decide(BOT_CHANNEL_OTHER) == BOT_POLICY_IGNORE);
}

static void test_normalize_text() {
  char out[32];
  size_t written = 0;
  const char text[] = "  hi\tthere\n\x01mesh  ";
  assert(FirmwareBot::normalizeText(text, sizeof(text) - 1, out, sizeof(out), &written) == BOT_WRITE_OK);
  assert(strcmp(out, "hi there mesh") == 0);
  assert(written == strlen(out));

  const char with_nul[] = { 'h', 'i', 0, '!', 0 };
  assert(FirmwareBot::normalizeText(with_nul, sizeof(with_nul), out, sizeof(out), &written) == BOT_WRITE_OK);
  assert(strcmp(out, "hi") == 0);

  const char utf8[] = "!hello café";
  assert(FirmwareBot::normalizeText(utf8, sizeof(utf8) - 1, out, sizeof(out), &written) == BOT_WRITE_OK);
  assert(strcmp(out, "!hello café") == 0);
}

static void test_normalize_truncation() {
  char out[6];
  size_t written = 0;
  assert(FirmwareBot::normalizeText("abcdef", 6, out, sizeof(out), &written) == BOT_WRITE_TRUNCATED);
  assert(strcmp(out, "abcde") == 0);
  assert(written == 5);
  assert(FirmwareBot::normalizeText("abc", 3, NULL, 0, &written) == BOT_WRITE_NO_SPACE);

  char max_input[BOT_MAX_TEXT_LEN + 8];
  for (size_t i = 0; i < sizeof(max_input); i++) max_input[i] = 'x';
  char max_output[BOT_MAX_TEXT_LEN + 1];
  assert(FirmwareBot::normalizeText(max_input, BOT_MAX_TEXT_LEN, max_output, sizeof(max_output), &written) == BOT_WRITE_OK);
  assert(written == BOT_MAX_TEXT_LEN);
  assert(max_output[BOT_MAX_TEXT_LEN] == 0);
  assert(FirmwareBot::normalizeText(max_input, BOT_MAX_TEXT_LEN + 1, max_output, sizeof(max_output), &written) == BOT_WRITE_TRUNCATED);
  assert(written == BOT_MAX_TEXT_LEN);
}

static void test_parse_command() {
  BotCommand command;
  assert(!FirmwareBot::parseCommand("hello bot", 9, &command));
  assert(FirmwareBot::parseCommand("!PING", 5, &command));
  assert(command.id == BOT_COMMAND_PING);
  assert(strcmp(command.name, "ping") == 0);
  assert(command.args_len == 0);

  assert(FirmwareBot::parseCommand("/roll: 2d6", 10, &command));
  assert(command.id == BOT_COMMAND_DICE);
  assert(strcmp(command.name, "roll") == 0);
  assert(strcmp(command.args, "2d6") == 0);

  assert(FirmwareBot::parseCommand("!help, please", 13, &command));
  assert(command.id == BOT_COMMAND_HELP);
  assert(strcmp(command.args, "please") == 0);

  assert(FirmwareBot::parseCommand("!wat", 4, &command));
  assert(command.id == BOT_COMMAND_UNKNOWN);

  assert(!FirmwareBot::parseCommand("!", 1, &command));
}

static void test_response_write() {
  char out[8];
  size_t written = 0;
  assert(FirmwareBot::writeResponse(out, sizeof(out), "pong", 4, &written) == BOT_WRITE_OK);
  assert(strcmp(out, "pong") == 0);
  assert(written == 4);

  assert(FirmwareBot::writeResponse(out, sizeof(out), "123456789", 9, &written) == BOT_WRITE_TRUNCATED);
  assert(strcmp(out, "1234567") == 0);
  assert(written == 7);

  assert(FirmwareBot::writeResponse(NULL, 0, "x", 1, &written) == BOT_WRITE_NO_SPACE);
  assert(FirmwareBot::writeResponse(out, sizeof(out), NULL, 1, &written) == BOT_WRITE_NO_SPACE);
  assert(out[0] == 0);
}

static BotMessage make_message(const char* channel, const char* text) {
  BotMessage message;
  memset(&message, 0, sizeof(message));
  message.channel_kind = BotPolicy::classifyChannel(channel, strlen(channel), false);
  strncpy(message.channel_name, channel, sizeof(message.channel_name) - 1);
  strncpy(message.sender_name, "alice", sizeof(message.sender_name) - 1);
  for (size_t i = 0; i < sizeof(message.sender_key_prefix); i++) message.sender_key_prefix[i] = (uint8_t)(i + 1);
  message.sender_timestamp = 12345;
  strncpy(message.text, text, sizeof(message.text) - 1);
  message.text_len = strlen(message.text);
  return message;
}

static void test_fingerprint() {
  BotMessage a = make_message("#bot", "!PING");
  BotMessage b = make_message("bot", "!ping");
  BotMessage c = make_message("#testing", "!ping");
  BotFingerprint fa = FirmwareBot::fingerprintFor(a);
  BotFingerprint fb = FirmwareBot::fingerprintFor(b);
  BotFingerprint fc = FirmwareBot::fingerprintFor(c);
  assert(fa.value == fb.value);
  assert(fa.value != fc.value);

  b.sender_key_prefix[0] = 99;
  assert(fa.value != FirmwareBot::fingerprintFor(b).value);
}

int main() {
  test_channel_policy();
  test_normalize_text();
  test_normalize_truncation();
  test_parse_command();
  test_response_write();
  test_fingerprint();
  printf("firmware_bot tests passed\n");
  return 0;
}
