#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "BotCommands.h"
#include "BotPolicy.h"
#include "EmergencyForwarder.h"
#include "FirmwareBot.h"

static void test_channel_policy() {
  assert(BotPolicy::classifyChannel(NULL, 0, true) == BOT_CHANNEL_DM);
  assert(BotPolicy::decide(BOT_CHANNEL_DM) == BOT_POLICY_ALLOW_NORMAL);
  assert(BotPolicy::isNormalAllowed(BOT_CHANNEL_DM));
  assert(BotPolicy::classifyChannel("Public", 6, false) == BOT_CHANNEL_PUBLIC);
  assert(BotPolicy::classifyChannel("public", 6, false) == BOT_CHANNEL_OTHER);
  assert(BotPolicy::classifyChannel("#Public", 7, false) == BOT_CHANNEL_OTHER);
  assert(BotPolicy::decide(BOT_CHANNEL_PUBLIC) == BOT_POLICY_IGNORE);
  assert(!BotPolicy::isNormalAllowed(BOT_CHANNEL_PUBLIC));
  assert(BotPolicy::classifyChannel("#bot", 4, false) == BOT_CHANNEL_BOT);
  assert(BotPolicy::decide(BOT_CHANNEL_BOT) == BOT_POLICY_ALLOW_NORMAL);
  assert(BotPolicy::isNormalAllowed(BOT_CHANNEL_BOT));
  assert(BotPolicy::classifyChannel("testing", 7, false) == BOT_CHANNEL_TESTING);
  assert(BotPolicy::decide(BOT_CHANNEL_TESTING) == BOT_POLICY_ALLOW_NORMAL);
  assert(BotPolicy::isNormalAllowed(BOT_CHANNEL_TESTING));
  assert(BotPolicy::classifyChannel("#emergency", 10, false) == BOT_CHANNEL_EMERGENCY);
  assert(BotPolicy::classifyChannel("emergency", 9, false) == BOT_CHANNEL_OTHER);
  assert(BotPolicy::classifyChannel("#Emergency", 10, false) == BOT_CHANNEL_OTHER);
  assert(BotPolicy::decide(BOT_CHANNEL_EMERGENCY) == BOT_POLICY_EMERGENCY_FORWARD);
  assert(BotPolicy::isEmergency(BOT_CHANNEL_EMERGENCY));
  assert(!BotPolicy::isNormalAllowed(BOT_CHANNEL_EMERGENCY));
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
  const char* body = NULL;
  size_t body_len = 0;
  char sender[BOT_MAX_SENDER_NAME_LEN + 1];
  assert(FirmwareBot::splitChannelText("alice: !ping", 12, sender, sizeof(sender), &body, &body_len));
  assert(strcmp(sender, "alice") == 0);
  assert(body_len == 5);
  assert(strncmp(body, "!ping", body_len) == 0);
  assert(!FirmwareBot::splitChannelText("alice says !ping", 16, sender, sizeof(sender), &body, &body_len));

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

  assert(FirmwareBot::parseCommand("!channels", 9, &command));
  assert(command.id == BOT_COMMAND_CHANNELS);

  assert(FirmwareBot::parseCommand("!commands", 9, &command));
  assert(command.id == BOT_COMMAND_HELP);

  assert(FirmwareBot::parseCommand("!hi", 3, &command));
  assert(command.id == BOT_COMMAND_HELLO);

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

static BotCommandContext make_context() {
  BotCommandContext context;
  memset(&context, 0, sizeof(context));
  strncpy(context.node_name, "cm-bot", sizeof(context.node_name) - 1);
  context.uptime_seconds = 1234;
  context.battery_millivolts = 4180;
  context.storage_used_kb = 12;
  context.storage_total_kb = 128;
  context.observed_messages = 9;
  context.ignored_messages = 2;
  context.eligible_messages = 5;
  context.sent_messages = 4;
  context.send_failures = 1;
  context.random_seed = 0x12345678;
  context.channel_count = 4;
  return context;
}

static BotCommandResult run_command(const char* text, char* output, size_t output_len) {
  BotCommand command;
  assert(FirmwareBot::parseCommand(text, strlen(text), &command));
  BotCommandContext context = make_context();
  return BotCommands::executeCommand(command, context, output, output_len);
}

static void test_command_outputs() {
  char out[BOT_MAX_RESPONSE_LEN + 1];
  BotCommandResult result = run_command("!ping", out, sizeof(out));
  assert(result.code == BOT_COMMAND_RESULT_OK);
  assert(strcmp(out, "Pong!") == 0);
  assert(result.text_len == strlen(out));

  result = run_command("!test", out, sizeof(out));
  assert(result.code == BOT_COMMAND_RESULT_OK);
  assert(strcmp(out, "Bot test OK") == 0);

  result = run_command("!hello", out, sizeof(out));
  assert(result.code == BOT_COMMAND_RESULT_OK);
  assert(strcmp(out, "Hello from cm-bot") == 0);

  result = run_command("!about", out, sizeof(out));
  assert(result.code == BOT_COMMAND_RESULT_OK);
  assert(strstr(out, "Colorado Mesh firmware bot") != NULL);

  result = run_command("!help", out, sizeof(out));
  assert(result.code == BOT_COMMAND_RESULT_OK);
  assert(strstr(out, "!ping") != NULL);
  assert(strstr(out, "!channels") != NULL);

  result = run_command("!channels", out, sizeof(out));
  assert(result.code == BOT_COMMAND_RESULT_OK);
  assert(strcmp(out, "Configured channels: 4. Normal bot replies: DM, #bot, #testing.") == 0);

  result = run_command("!status", out, sizeof(out));
  assert(result.code == BOT_COMMAND_RESULT_OK);
  assert(strstr(out, "cm-bot up 1234s") != NULL);
  assert(strstr(out, "batt 4180mV") != NULL);
  assert(strstr(out, "sent 4 fail 1") != NULL);

  result = run_command("!wat", out, sizeof(out));
  assert(result.code == BOT_COMMAND_RESULT_OK);
  assert(strcmp(out, "Unknown command. Try !help") == 0);
}

static void test_dice_command() {
  char out[BOT_MAX_RESPONSE_LEN + 1];
  BotCommandResult result = run_command("!roll", out, sizeof(out));
  assert(result.code == BOT_COMMAND_RESULT_OK);
  assert(strncmp(out, "Rolled d6: ", 11) == 0);

  result = run_command("!roll d20", out, sizeof(out));
  assert(result.code == BOT_COMMAND_RESULT_OK);
  assert(strncmp(out, "Rolled d20: ", 12) == 0);

  result = run_command("!roll 2d6", out, sizeof(out));
  assert(result.code == BOT_COMMAND_RESULT_OK);
  assert(strncmp(out, "Rolled 2d6: ", 12) == 0);
  assert(strchr(out, '+') != NULL);
  assert(strchr(out, '=') != NULL);

  result = run_command("!roll d7", out, sizeof(out));
  assert(result.code == BOT_COMMAND_RESULT_OK);
  assert(strncmp(out, "Usage: !roll", 12) == 0);

  result = run_command("!roll 0d6", out, sizeof(out));
  assert(result.code == BOT_COMMAND_RESULT_OK);
  assert(strncmp(out, "Usage: !roll", 12) == 0);

  result = run_command("!roll 11d6", out, sizeof(out));
  assert(result.code == BOT_COMMAND_RESULT_OK);
  assert(strncmp(out, "Usage: !roll", 12) == 0);
}

static void test_command_truncation() {
  char out[12];
  BotCommandResult result = run_command("!about", out, sizeof(out));
  assert(result.code == BOT_COMMAND_RESULT_TRUNCATED);
  assert(strcmp(out, "Colorado Me") == 0);
  assert(result.text_len == strlen(out));

  result = run_command("!ping", NULL, 0);
  assert(result.code == BOT_COMMAND_RESULT_NO_SPACE);
  assert(result.text_len == 0);
}

static void test_command_cooldown() {
  BotCommandCooldown cooldowns[2];
  memset(cooldowns, 0, sizeof(cooldowns));
  assert(!FirmwareBot::isCommandOnCooldown(cooldowns, 2, BOT_COMMAND_PING, 1000));
  FirmwareBot::recordCommandCooldown(cooldowns, 2, BOT_COMMAND_PING, 1000, 5000);
  assert(FirmwareBot::isCommandOnCooldown(cooldowns, 2, BOT_COMMAND_PING, 5999));
  assert(!FirmwareBot::isCommandOnCooldown(cooldowns, 2, BOT_COMMAND_PING, 6000));
  assert(!FirmwareBot::isCommandOnCooldown(cooldowns, 2, BOT_COMMAND_STATUS, 2000));

  FirmwareBot::recordCommandCooldown(cooldowns, 2, BOT_COMMAND_STATUS, 0xFFFFFFF0UL, 32);
  assert(FirmwareBot::isCommandOnCooldown(cooldowns, 2, BOT_COMMAND_STATUS, 0xFFFFFFF8UL));
  assert(!FirmwareBot::isCommandOnCooldown(cooldowns, 2, BOT_COMMAND_STATUS, 0x00000010UL));
}

static void test_group_response_cap() {
  assert(BOT_MAX_GROUP_RESPONSE_LEN == BOT_MAX_TEXT_LEN - BOT_GROUP_RESPONSE_PREFIX_RESERVE);
  assert(FirmwareBot::maxResponseLenForChannel(BOT_CHANNEL_DM) == BOT_MAX_RESPONSE_LEN);
  assert(FirmwareBot::maxResponseLenForChannel(BOT_CHANNEL_BOT) == BOT_MAX_GROUP_RESPONSE_LEN);
  assert(FirmwareBot::maxResponseLenForChannel(BOT_CHANNEL_TESTING) == BOT_MAX_GROUP_RESPONSE_LEN);
  assert(BOT_MAX_GROUP_RESPONSE_LEN < BOT_MAX_RESPONSE_LEN);
}

static void test_emergency_forward_short() {
  BotMessage message = make_message("#emergency", "need help at trailhead");
  BotEmergencyForward forward;
  assert(EmergencyForwarder::format(message, forward));
  assert(forward.part_count == 1);
  assert(!forward.truncated);
  assert(strstr(forward.parts[0], "EMERGENCY MESSAGE FROM alice: need help at trailhead") == forward.parts[0]);
  assert(forward.part_lens[0] == strlen(forward.parts[0]));
}

static void test_emergency_forward_loop_prevention() {
  assert(EmergencyForwarder::isForwardedEmergencyText("EMERGENCY MESSAGE FROM alice: need help", 39));
  assert(!EmergencyForwarder::isForwardedEmergencyText("FYI EMERGENCY MESSAGE FROM alice", 32));

  BotMessage message = make_message("#emergency", "EMERGENCY MESSAGE FROM alice: need help");
  BotEmergencyForward forward;
  assert(!EmergencyForwarder::format(message, forward));

  BotMessage public_message = make_message("Public", "!ping");
  assert(!EmergencyForwarder::format(public_message, forward));
}

static void test_emergency_forward_multipart() {
  BotMessage message = make_message("#emergency", "short");
  memset(message.text, 'a', sizeof(message.text));
  message.text[BOT_MAX_TEXT_LEN] = 0;
  message.text_len = BOT_MAX_TEXT_LEN;

  BotEmergencyForward forward;
  assert(EmergencyForwarder::format(message, forward));
  assert(forward.part_count > 1);
  assert(forward.part_count <= BOT_EMERGENCY_MAX_PARTS);
  assert(!forward.truncated);
  for (uint8_t i = 0; i < forward.part_count; i++) {
    assert(strstr(forward.parts[i], "EMERGENCY MESSAGE FROM alice: ") == forward.parts[i]);
    assert(strstr(forward.parts[i], "[") != NULL);
    assert(forward.part_lens[i] <= BOT_MAX_GROUP_RESPONSE_LEN);
  }
}

static void test_emergency_forward_truncation() {
  BotMessage message = make_message("#emergency", "short");
  memset(message.sender_name, 's', sizeof(message.sender_name) - 1);
  message.sender_name[sizeof(message.sender_name) - 1] = 0;
  memset(message.text, 'b', sizeof(message.text));
  message.text[BOT_MAX_TEXT_LEN] = 0;
  message.text_len = BOT_MAX_TEXT_LEN;
  message.text_truncated = true;

  BotEmergencyForward forward;
  assert(EmergencyForwarder::format(message, forward));
  assert(forward.truncated);
  assert(forward.part_count == BOT_EMERGENCY_MAX_PARTS);
  assert(strstr(forward.parts[forward.part_count - 1], "...") != NULL);
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
  test_command_outputs();
  test_dice_command();
  test_command_truncation();
  test_command_cooldown();
  test_group_response_cap();
  test_emergency_forward_short();
  test_emergency_forward_loop_prevention();
  test_emergency_forward_multipart();
  test_emergency_forward_truncation();
  test_fingerprint();
  printf("firmware_bot tests passed\n");
  return 0;
}
