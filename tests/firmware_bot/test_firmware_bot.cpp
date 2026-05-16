#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "BotCommandRegistry.h"
#include "BotCommands.h"
#include "BotPolicy.h"
#include "BotPrefs.h"
#include "EmergencyForwarder.h"
#include "FirmwareBot.h"
#include "KnownBotRegistry.h"
#include "ResponseCoordinator.h"

static void assert_starts_with(const char* value, const char* prefix) {
  assert(value != NULL);
  assert(prefix != NULL);
  assert(strncmp(value, prefix, strlen(prefix)) == 0);
}

static void assert_contains(const char* value, const char* needle) {
  assert(value != NULL);
  assert(needle != NULL);
  assert(strstr(value, needle) != NULL);
}

static unsigned parse_unsigned_suffix(const char* value, size_t offset) {
  assert(value != NULL);
  assert(offset < strlen(value));
  unsigned parsed = 0;
  for (size_t i = offset; value[i] != 0; i++) {
    assert(isdigit((unsigned char)value[i]));
    parsed = parsed * 10 + (unsigned)(value[i] - '0');
  }
  return parsed;
}

static void assert_roll_range(const char* value, const char* prefix, unsigned low, unsigned high) {
  assert_starts_with(value, prefix);
  unsigned rolled = parse_unsigned_suffix(value, strlen(prefix));
  assert(rolled >= low);
  assert(rolled <= high);
}

static void assert_dice_single_range(const char* value, const char* prefix, unsigned high) {
  assert_starts_with(value, prefix);
  unsigned rolled = parse_unsigned_suffix(value, strlen(prefix));
  assert(rolled >= 1);
  assert(rolled <= high);
}

static void test_command_registry() {
  assert(BotCommandRegistry::commandCount() >= 18);
  uint32_t seen_masks = 0;
  bool saw_internal = false;
  for (size_t i = 0; i < BotCommandRegistry::commandCount(); i++) {
    const BotCommandMetadata* command = BotCommandRegistry::commandAt(i);
    assert(command != NULL);
    assert(BotCommandRegistry::findById(command->id) == command);
    assert(command->name != NULL);
    assert(command->name[0] != 0);
    assert(command->summary != NULL);
    assert(command->usage != NULL);
    assert(command->details != NULL);
    const BotCommandMetadata* by_name = BotCommandRegistry::findByName(command->name, strlen(command->name));
    assert(by_name == command);
    if (command->visibility == BOT_COMMAND_VISIBILITY_DISCOVERABLE) {
      assert(command->mask != 0);
      assert((seen_masks & command->mask) == 0);
      seen_masks |= command->mask;
      assert(BotCommandRegistry::isDiscoverable(command->id));
    } else {
      assert(command->mask == 0);
      assert(!BotCommandRegistry::isDiscoverable(command->id));
      assert(command->id != BOT_COMMAND_UNSUPPORTED);
      saw_internal = saw_internal || command->visibility == BOT_COMMAND_VISIBILITY_INTERNAL;
    }
  }
  assert((seen_masks & BOT_COMMAND_MASK_ALL) == BOT_COMMAND_MASK_ALL);
  assert(saw_internal);
  assert(BotCommandRegistry::findByName("ROLL", 4)->id == BOT_COMMAND_ROLL);
  assert(BotCommandRegistry::findByName("dice", 4)->id == BOT_COMMAND_DICE);
  assert(BotCommandRegistry::findByName("commands", 8)->id == BOT_COMMAND_CMD);
  assert(BotCommandRegistry::findByName("channel", 7)->id == BOT_COMMAND_CHANNELS);
  assert(BotCommandRegistry::findByName("t", 1)->id == BOT_COMMAND_TEST);
  assert(BotCommandRegistry::findByName("tracer", 6)->id == BOT_COMMAND_TRACER);
  assert(BotCommandRegistry::findByName("lookup", 6)->id == BOT_COMMAND_PREFIX);
  assert(BotCommandRegistry::findByName("weather", 7) == NULL);
  assert(BotCommandRegistry::findByName("reboot", 6) == NULL);
  assert(BotCommandRegistry::findByName("nodes", 5) == NULL);
  assert(BotCommandRegistry::findByName("nope", 4) == NULL);
  assert(strcmp(BotCommandRegistry::commandName(BOT_COMMAND_ROLL), "roll") == 0);
  assert(BotCommandRegistry::commandMask(BOT_COMMAND_UNKNOWN) == 0);
}

static void test_channel_policy() {
  assert(BotPolicy::classifyChannel(NULL, 0, true) == BOT_CHANNEL_DM);
  assert(BotPolicy::decide(BOT_CHANNEL_DM) == BOT_POLICY_ALLOW_NORMAL);
  assert(BotPolicy::isNormalAllowed(BOT_CHANNEL_DM));
  assert(BotPolicy::isPrefixlessCommandAllowed(BOT_CHANNEL_DM));
  assert(BotPolicy::classifyChannel("Public", 6, false) == BOT_CHANNEL_PUBLIC);
  assert(BotPolicy::classifyChannel("public", 6, false) == BOT_CHANNEL_OTHER);
  assert(BotPolicy::classifyChannel("#Public", 7, false) == BOT_CHANNEL_OTHER);
  assert(BotPolicy::decide(BOT_CHANNEL_PUBLIC) == BOT_POLICY_IGNORE);
  assert(!BotPolicy::isNormalAllowed(BOT_CHANNEL_PUBLIC));
  assert(!BotPolicy::isPrefixlessCommandAllowed(BOT_CHANNEL_PUBLIC));
  assert(BotPolicy::classifyChannel("#bot", 4, false) == BOT_CHANNEL_BOT);
  assert(BotPolicy::decide(BOT_CHANNEL_BOT) == BOT_POLICY_ALLOW_NORMAL);
  assert(BotPolicy::isNormalAllowed(BOT_CHANNEL_BOT));
  assert(BotPolicy::isPrefixlessCommandAllowed(BOT_CHANNEL_BOT));
  assert(BotPolicy::classifyChannel("testing", 7, false) == BOT_CHANNEL_TESTING);
  assert(BotPolicy::decide(BOT_CHANNEL_TESTING) == BOT_POLICY_ALLOW_NORMAL);
  assert(BotPolicy::isNormalAllowed(BOT_CHANNEL_TESTING));
  assert(BotPolicy::isPrefixlessCommandAllowed(BOT_CHANNEL_TESTING));
  assert(BotPolicy::classifyChannel("#emergency", 10, false) == BOT_CHANNEL_EMERGENCY);
  assert(BotPolicy::classifyChannel("emergency", 9, false) == BOT_CHANNEL_OTHER);
  assert(BotPolicy::classifyChannel("#Emergency", 10, false) == BOT_CHANNEL_OTHER);
  assert(BotPolicy::decide(BOT_CHANNEL_EMERGENCY) == BOT_POLICY_EMERGENCY_FORWARD);
  assert(BotPolicy::isEmergency(BOT_CHANNEL_EMERGENCY));
  assert(!BotPolicy::isNormalAllowed(BOT_CHANNEL_EMERGENCY));
  assert(!BotPolicy::isPrefixlessCommandAllowed(BOT_CHANNEL_EMERGENCY));
  assert(BotPolicy::classifyChannel("#botnet", 7, false) == BOT_CHANNEL_OTHER);
  assert(BotPolicy::decide(BOT_CHANNEL_OTHER) == BOT_POLICY_IGNORE);
  assert(!BotPolicy::isPrefixlessCommandAllowed(BOT_CHANNEL_OTHER));
}

static void test_bot_prefs_defaults() {
  BotPrefs prefs;
  BotPrefsCodec::defaults(prefs);
  assert(prefs.enabled);
  assert(prefs.normal_delay_ms == BOT_RESPONSE_DELAY_BASE_MILLIS);
  assert(prefs.normal_jitter_ms == BOT_RESPONSE_DELAY_JITTER_MILLIS);
  assert(prefs.hop_step_ms == BOT_HOP_STEP_MILLIS_DEFAULT);
  assert(prefs.local_advert_interval_ms == BOT_PREFS_DEFAULT_LOCAL_ADVERT_MILLIS);
  assert(prefs.flood_advert_interval_ms == BOT_PREFS_DEFAULT_FLOOD_ADVERT_MILLIS);
  assert(strcmp(prefs.bot_channel, "#bot") == 0);
  assert(strcmp(prefs.testing_channel, "#testing") == 0);
  assert(strcmp(prefs.emergency_channel, "#emergency") == 0);
  assert(strcmp(prefs.public_channel, "Public") == 0);
  assert(BotPrefsCodec::serializedSize() == BOT_PREFS_SERIALIZED_SIZE);
  assert(BotPrefsCodec::commandEnabled(prefs, BOT_COMMAND_PING));
  assert(!BotPrefsCodec::commandEnabled(prefs, BOT_COMMAND_UNKNOWN));
}

static void test_bot_prefs_serialization_round_trip() {
  BotPrefs prefs;
  BotPrefsCodec::defaults(prefs);
  prefs.enabled = false;
  prefs.normal_delay_ms = 2345;
  prefs.normal_jitter_ms = 6789;
  prefs.hop_step_ms = 7500;
  prefs.local_advert_interval_ms = 123456;
  prefs.flood_advert_interval_ms = 654321;
  strncpy(prefs.bot_channel, "#ops", sizeof(prefs.bot_channel) - 1);
  uint8_t key[BOT_SENDER_KEY_PREFIX_LEN] = { 0xA0, 0xB1, 0xC2, 0xD3, 0xE4, 0xF5 };
  assert(BotPrefsCodec::addKnownBot(prefs, key, BOT_KNOWN_BOT_FLAG_SUPPRESS_NORMAL, "alpha"));
  BotPrefsCodec::setCommandEnabled(prefs, BOT_COMMAND_STATUS, false);

  uint8_t blob[BOT_PREFS_SERIALIZED_SIZE];
  assert(BotPrefsCodec::serialize(prefs, blob, sizeof(blob)));
  BotPrefs loaded;
  assert(BotPrefsCodec::deserialize(blob, sizeof(blob), loaded));
  assert(!loaded.enabled);
  assert(loaded.normal_delay_ms == 2345);
  assert(loaded.normal_jitter_ms == 6789);
  assert(loaded.hop_step_ms == 7500);
  assert(loaded.local_advert_interval_ms == 123456);
  assert(loaded.flood_advert_interval_ms == 654321);
  assert(strcmp(loaded.bot_channel, "#ops") == 0);
  assert(!BotPrefsCodec::commandEnabled(loaded, BOT_COMMAND_STATUS));
  assert(BotPrefsCodec::findKnownBot(loaded, key) != NULL);
  assert(strcmp(BotPrefsCodec::findKnownBot(loaded, key)->label, "alpha") == 0);
}

static void test_bot_prefs_rejects_corrupt_and_wrong_version() {
  BotPrefs prefs;
  BotPrefsCodec::defaults(prefs);
  uint8_t blob[BOT_PREFS_SERIALIZED_SIZE];
  assert(BotPrefsCodec::serialize(prefs, blob, sizeof(blob)));

  BotPrefs loaded;
  blob[20] ^= 0x55;
  assert(!BotPrefsCodec::deserialize(blob, sizeof(blob), loaded));
  assert(loaded.enabled);
  assert(strcmp(loaded.bot_channel, "#bot") == 0);

  assert(BotPrefsCodec::serialize(prefs, blob, sizeof(blob)));
  blob[4] = 0x7F;
  assert(!BotPrefsCodec::deserialize(blob, sizeof(blob), loaded));
  assert(strcmp(loaded.public_channel, "Public") == 0);
  assert(!BotPrefsCodec::deserialize(blob, sizeof(blob) - 1, loaded));
}

static void test_bot_prefs_validation_and_command_mask() {
  BotPrefs prefs;
  BotPrefsCodec::defaults(prefs);
  prefs.normal_delay_ms = BOT_PREFS_MAX_DELAY_MILLIS + 1;
  prefs.normal_jitter_ms = BOT_PREFS_MAX_DELAY_MILLIS + 1;
  prefs.local_advert_interval_ms = BOT_PREFS_MAX_ADVERT_MILLIS + 1;
  prefs.flood_advert_interval_ms = BOT_PREFS_MAX_ADVERT_MILLIS + 1;
  prefs.max_response_parts = 99;
  prefs.bot_channel[0] = 0;
  prefs.command_mask = 0xFFFFFFFFUL;
  BotPrefsCodec::validate(prefs);
  assert(prefs.normal_delay_ms == BOT_PREFS_MAX_DELAY_MILLIS);
  assert(prefs.normal_jitter_ms == BOT_PREFS_MAX_DELAY_MILLIS);
  assert(prefs.local_advert_interval_ms == BOT_PREFS_MAX_ADVERT_MILLIS);
  assert(prefs.flood_advert_interval_ms == BOT_PREFS_MAX_ADVERT_MILLIS);
  assert(prefs.max_response_parts == BOT_EMERGENCY_MAX_PARTS);
  assert(strcmp(prefs.bot_channel, "#bot") == 0);
  assert((prefs.command_mask & ~BOT_COMMAND_MASK_ALL) == 0);
  assert(BotPrefsCodec::channelNameValid("#ops", false));
  assert(!BotPrefsCodec::channelNameValid("ops", false));
  assert(BotPrefsCodec::channelNameValid("Public", true));
  assert(!BotPrefsCodec::channelNameValid("#Public", true));
  assert(!BotPrefsCodec::channelNameValid("#abcdefghijklmnopqrstuvw", false));

  BotPrefs duplicate_channels;
  BotPrefsCodec::defaults(duplicate_channels);
  strncpy(duplicate_channels.testing_channel, "#bot", sizeof(duplicate_channels.testing_channel) - 1);
  assert(!BotPrefsCodec::channelConfigValid(duplicate_channels));
  BotPrefsCodec::validate(duplicate_channels);
  assert(strcmp(duplicate_channels.testing_channel, "#testing") == 0);

  BotPrefsCodec::setCommandEnabled(prefs, BOT_COMMAND_PING, false);
  assert(!BotPrefsCodec::commandEnabled(prefs, BOT_COMMAND_PING));
  BotPrefsCodec::setCommandEnabled(prefs, BOT_COMMAND_PING, true);
  assert(BotPrefsCodec::commandEnabled(prefs, BOT_COMMAND_PING));
  assert(BotPrefsCodec::commandEnabled(prefs, BOT_COMMAND_CMD));
  assert(BotPrefsCodec::commandEnabled(prefs, BOT_COMMAND_TRACE));
  assert(BotPrefsCodec::commandEnabled(prefs, BOT_COMMAND_PREFIX));
  BotCommandId id = BOT_COMMAND_NONE;
  assert(BotPrefsCodec::commandIdForName("commands", &id));
  assert(id == BOT_COMMAND_CMD);
  assert(BotPrefsCodec::commandIdForName("ROLL", &id));
  assert(id == BOT_COMMAND_ROLL);
  assert(BotPrefsCodec::commandIdForName("t", &id));
  assert(id == BOT_COMMAND_TEST);
  assert(BotPrefsCodec::commandIdForName("ver", &id));
  assert(id == BOT_COMMAND_VERSION);
  assert(BotPrefsCodec::commandIdForName("8ball", &id));
  assert(id == BOT_COMMAND_MAGIC8);
  assert(BotPrefsCodec::commandIdForName("p", &id));
  assert(id == BOT_COMMAND_PATH);
  assert(BotPrefsCodec::commandIdForName("channel", &id));
  assert(id == BOT_COMMAND_CHANNELS);
  assert(BotPrefsCodec::commandIdForName("tracer", &id));
  assert(id == BOT_COMMAND_TRACER);
  assert(!BotPrefsCodec::commandIdForName("weather", &id));
  assert(!BotPrefsCodec::commandIdForName("reboot", &id));
  assert(BotPrefsCodec::commandIdForName("prefix", &id));
  assert(id == BOT_COMMAND_PREFIX);
  assert(BotPrefsCodec::commandIdForName("lookup", &id));
  assert(id == BOT_COMMAND_PREFIX);
  assert(!BotPrefsCodec::commandIdForName("unknown", &id));
  assert(!BotPrefsCodec::commandIdForName("nope", &id));
}

static bool command_allowed_by_runtime_gate(const BotPrefs& prefs, BotCommandId command_id) {
  return command_id == BOT_COMMAND_UNKNOWN || command_id == BOT_COMMAND_UNSUPPORTED || BotPrefsCodec::commandEnabled(prefs, command_id);
}

static void test_unknown_command_runtime_gate() {
  BotPrefs prefs;
  BotPrefsCodec::defaults(prefs);
  assert(command_allowed_by_runtime_gate(prefs, BOT_COMMAND_UNKNOWN));
  assert(command_allowed_by_runtime_gate(prefs, BOT_COMMAND_UNSUPPORTED));
  assert(!BotPrefsCodec::commandEnabled(prefs, BOT_COMMAND_UNKNOWN));
  assert(!BotPrefsCodec::commandEnabled(prefs, BOT_COMMAND_UNSUPPORTED));
  assert(!BotPrefsCodec::commandIdForName("weather", NULL));
  BotPrefsCodec::setCommandEnabled(prefs, BOT_COMMAND_PING, false);
  assert(!command_allowed_by_runtime_gate(prefs, BOT_COMMAND_PING));
}

static void test_bot_prefs_known_bot_helpers() {
  BotPrefs prefs;
  BotPrefsCodec::defaults(prefs);
  uint8_t key1[BOT_SENDER_KEY_PREFIX_LEN];
  uint8_t key2[BOT_SENDER_KEY_PREFIX_LEN];
  char hex[BOT_SENDER_KEY_PREFIX_LEN * 2 + 1];
  assert(BotPrefsCodec::parseKeyPrefixHex("010203040506", key1));
  assert(!BotPrefsCodec::parseKeyPrefixHex("01020304050", key2));
  assert(!BotPrefsCodec::parseKeyPrefixHex("01020304050x", key2));
  BotPrefsCodec::formatKeyPrefixHex(key1, hex, sizeof(hex));
  assert(strcmp(hex, "010203040506") == 0);
  assert(BotPrefsCodec::addKnownBot(prefs, key1, BOT_KNOWN_BOT_FLAG_SUPPRESS_NORMAL, "alpha"));
  assert(BotPrefsCodec::findKnownBot(prefs, key1) != NULL);
  assert(BotPrefsCodec::addKnownBot(prefs, key1, 0, "beta"));
  assert(BotPrefsCodec::findKnownBot(prefs, key1)->flags == 0);
  assert(strcmp(BotPrefsCodec::findKnownBot(prefs, key1)->label, "beta") == 0);
  assert(BotPrefsCodec::removeKnownBot(prefs, key1));
  assert(!BotPrefsCodec::removeKnownBot(prefs, key1));

  for (size_t i = 0; i < BOT_KNOWN_BOT_SLOTS; i++) {
    for (size_t j = 0; j < BOT_SENDER_KEY_PREFIX_LEN; j++) key2[j] = (uint8_t)(i * 10 + j);
    assert(BotPrefsCodec::addKnownBot(prefs, key2, BOT_KNOWN_BOT_FLAG_SUPPRESS_NORMAL, "bot"));
  }
  uint8_t overflow[BOT_SENDER_KEY_PREFIX_LEN] = { 99, 98, 97, 96, 95, 94 };
  assert(!BotPrefsCodec::addKnownBot(prefs, overflow, BOT_KNOWN_BOT_FLAG_SUPPRESS_NORMAL, "full"));
}

static void test_prefs_aware_channel_policy() {
  BotPrefs prefs;
  BotPrefsCodec::defaults(prefs);
  strncpy(prefs.bot_channel, "#ops", sizeof(prefs.bot_channel) - 1);
  strncpy(prefs.testing_channel, "#lab", sizeof(prefs.testing_channel) - 1);
  strncpy(prefs.emergency_channel, "#sos", sizeof(prefs.emergency_channel) - 1);
  strncpy(prefs.public_channel, "Town", sizeof(prefs.public_channel) - 1);
  assert(BotPolicy::classifyChannel("#ops", 4, false, prefs) == BOT_CHANNEL_BOT);
  assert(BotPolicy::classifyChannel("ops", 3, false, prefs) == BOT_CHANNEL_BOT);
  assert(BotPolicy::classifyChannel("#lab", 4, false, prefs) == BOT_CHANNEL_TESTING);
  assert(BotPolicy::classifyChannel("#sos", 4, false, prefs) == BOT_CHANNEL_EMERGENCY);
  assert(BotPolicy::classifyChannel("#SOS", 4, false, prefs) == BOT_CHANNEL_OTHER);
  assert(BotPolicy::classifyChannel("Town", 4, false, prefs) == BOT_CHANNEL_PUBLIC);
  assert(BotPolicy::classifyChannel("#bot", 4, false, prefs) == BOT_CHANNEL_OTHER);
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

static bool parse_channel_command(BotChannelKind channel_kind, const char* text, BotCommand* command) {
  return BotPolicy::isNormalAllowed(channel_kind) &&
         FirmwareBot::parseCommand(text, strlen(text), command, BotPolicy::isPrefixlessCommandAllowed(channel_kind));
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
  assert(!FirmwareBot::parseCommand("ping", 4, &command));
  assert(FirmwareBot::parseCommand("ping", 4, &command, true));
  assert(command.id == BOT_COMMAND_PING);
  assert(command.args_len == 0);
  assert(parse_channel_command(BOT_CHANNEL_BOT, "ping", &command));
  assert(command.id == BOT_COMMAND_PING);
  assert(parse_channel_command(BOT_CHANNEL_TESTING, "ping", &command));
  assert(command.id == BOT_COMMAND_PING);
  assert(parse_channel_command(BOT_CHANNEL_DM, "ping", &command));
  assert(command.id == BOT_COMMAND_PING);
  assert(!parse_channel_command(BOT_CHANNEL_PUBLIC, "ping", &command));
  assert(!parse_channel_command(BOT_CHANNEL_OTHER, "ping", &command));
  assert(!parse_channel_command(BOT_CHANNEL_EMERGENCY, "ping", &command));
  assert(parse_channel_command(BOT_CHANNEL_DM, "!ping", &command));
  assert(command.id == BOT_COMMAND_PING);
  assert(FirmwareBot::parseCommand("status please", 13, &command, true));
  assert(command.id == BOT_COMMAND_STATUS);
  assert(strcmp(command.args, "please") == 0);
  assert(parse_channel_command(BOT_CHANNEL_DM, "status please", &command));
  assert(command.id == BOT_COMMAND_STATUS);
  assert(strcmp(command.args, "please") == 0);
  assert(!parse_channel_command(BOT_CHANNEL_PUBLIC, "status please", &command));
  assert(!parse_channel_command(BOT_CHANNEL_BOT, "random prose", &command));
  assert(!FirmwareBot::parseCommand("random prose", 12, &command, true));
  assert(FirmwareBot::parseCommand("!PING", 5, &command));
  assert(command.id == BOT_COMMAND_PING);
  assert(strcmp(command.name, "ping") == 0);
  assert(command.args_len == 0);

  assert(FirmwareBot::parseCommand("/roll: 2d6", 10, &command));
  assert(command.id == BOT_COMMAND_ROLL);
  assert(strcmp(command.name, "roll") == 0);
  assert(strcmp(command.args, "2d6") == 0);

  assert(FirmwareBot::parseCommand("!help, please", 13, &command));
  assert(command.id == BOT_COMMAND_HELP);
  assert(strcmp(command.args, "please") == 0);

  assert(FirmwareBot::parseCommand("!channels", 9, &command));
  assert(command.id == BOT_COMMAND_CHANNELS);
  assert(FirmwareBot::parseCommand("!channel list", 13, &command));
  assert(command.id == BOT_COMMAND_CHANNELS);
  assert(strcmp(command.args, "list") == 0);

  assert(FirmwareBot::parseCommand("!cmd", 4, &command));
  assert(command.id == BOT_COMMAND_CMD);
  assert(FirmwareBot::parseCommand("!commands", 9, &command));
  assert(command.id == BOT_COMMAND_CMD);

  assert(FirmwareBot::parseCommand("!hi", 3, &command));
  assert(command.id == BOT_COMMAND_HELLO);

  assert(FirmwareBot::parseCommand("t", 1, &command, true));
  assert(command.id == BOT_COMMAND_TEST);
  assert(FirmwareBot::parseCommand("ver", 3, &command, true));
  assert(command.id == BOT_COMMAND_VERSION);
  assert(FirmwareBot::parseCommand("8ball", 5, &command, true));
  assert(command.id == BOT_COMMAND_MAGIC8);
  assert(FirmwareBot::parseCommand("p", 1, &command, true));
  assert(command.id == BOT_COMMAND_PATH);
  assert(FirmwareBot::parseCommand("decode", 6, &command, true));
  assert(command.id == BOT_COMMAND_PATH);
  assert(FirmwareBot::parseCommand("trace", 5, &command, true));
  assert(command.id == BOT_COMMAND_TRACE);
  assert(FirmwareBot::parseCommand("!tracer", 7, &command));
  assert(command.id == BOT_COMMAND_TRACER);
  assert(FirmwareBot::parseCommand("!prefix 01020304", 16, &command));
  assert(command.id == BOT_COMMAND_PREFIX);
  assert(strcmp(command.args, "01020304") == 0);
  assert(FirmwareBot::parseCommand("!lookup 0102", 12, &command));
  assert(command.id == BOT_COMMAND_PREFIX);
  assert(strcmp(command.args, "0102") == 0);
  assert(FirmwareBot::parseCommand("!trace 01,7a,55", 15, &command));
  assert(command.id == BOT_COMMAND_TRACE);
  assert(strcmp(command.args, "01,7a,55") == 0);
  assert(FirmwareBot::parseCommand("!tracer 01,7a,55", 16, &command));
  assert(command.id == BOT_COMMAND_TRACER);
  assert(strcmp(command.args, "01,7a,55") == 0);
  assert(FirmwareBot::parseCommand("!weather", 8, &command));
  assert(command.id == BOT_COMMAND_UNKNOWN);
  assert(FirmwareBot::parseCommand("!reboot now", 11, &command));
  assert(command.id == BOT_COMMAND_UNKNOWN);
  assert(strcmp(command.name, "reboot") == 0);
  assert(strcmp(command.args, "now") == 0);
  assert(!FirmwareBot::parseCommand("weather", 7, &command, true));
  assert(!FirmwareBot::parseCommand("reboot", 6, &command, true));

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

static void test_channel_text_and_response_guards() {
  char sender[BOT_MAX_SENDER_NAME_LEN + 1];
  char out[BOT_MAX_TEXT_LEN + 1];
  size_t written = 0;
  char input[BOT_MAX_TEXT_LEN + 8];
  strcpy(input, "alice: ");
  memset(&input[7], 'x', BOT_MAX_TEXT_LEN);
  input[7 + BOT_MAX_TEXT_LEN] = 0;
  assert(FirmwareBot::normalizeChannelText(input, sender, sizeof(sender), out, sizeof(out), &written) == BOT_WRITE_TRUNCATED);
  assert(strcmp(sender, "alice") == 0);
  assert(written == BOT_MAX_TEXT_LEN);

  BotCommand command;
  char response[BOT_MAX_RESPONSE_LEN + 1];
  assert(FirmwareBot::writeResponseForChannel(BOT_CHANNEL_BOT, true, "trace sent", 10, response, sizeof(response),
                                              &written) == BOT_WRITE_OK);
  assert(strcmp(response, "# trace sent") == 0);
  assert(!FirmwareBot::parseCommand(response, written, &command, true));
  char normalized[BOT_MAX_TEXT_LEN + 1];
  assert(FirmwareBot::normalizeChannelText(response, sender, sizeof(sender), normalized, sizeof(normalized), &written) ==
         BOT_WRITE_OK);
  assert(strcmp(normalized, "# trace sent") == 0);
  assert(!FirmwareBot::parseCommand(normalized, written, &command, true));
  assert(FirmwareBot::writeResponseForChannel(BOT_CHANNEL_BOT, true, "# trace sent", 12, response, sizeof(response),
                                              &written) == BOT_WRITE_OK);
  assert(strcmp(response, "# trace sent") == 0);
  assert(FirmwareBot::normalizeChannelText(response, sender, sizeof(sender), normalized, sizeof(normalized), &written) ==
         BOT_WRITE_OK);
  assert(!FirmwareBot::parseCommand(normalized, written, &command, true));
  assert(FirmwareBot::writeResponseForChannel(BOT_CHANNEL_DM, false, "trace sent", 10, response, sizeof(response),
                                              &written) == BOT_WRITE_OK);
  assert(strcmp(response, "trace sent") == 0);

  strcpy(sender, "stale");
  assert(FirmwareBot::normalizeChannelText("plain text", sender, sizeof(sender), normalized, sizeof(normalized),
                                           &written) == BOT_WRITE_OK);
  assert(sender[0] == 0);
  assert(strcmp(normalized, "plain text") == 0);

  char long_response[BOT_MAX_RESPONSE_LEN + 1];
  memset(long_response, 'p', sizeof(long_response));
  long_response[BOT_MAX_RESPONSE_LEN] = 0;
  assert(FirmwareBot::writeResponseForChannel(BOT_CHANNEL_BOT, true, long_response, BOT_MAX_RESPONSE_LEN, response,
                                              sizeof(response), &written) == BOT_WRITE_TRUNCATED);
  assert(written == BOT_MAX_GROUP_RESPONSE_LEN);
}

static void test_ack_response_format() {
  BotMessage message;
  memset(&message, 0, sizeof(message));
  message.channel_kind = BOT_CHANNEL_BOT;
  strncpy(message.channel_name, "#bot", sizeof(message.channel_name) - 1);
  strncpy(message.sender_name, "alice", sizeof(message.sender_name) - 1);
  message.sender_timestamp = 12345;
  message.received_at_timestamp = 12345;
  strncpy(message.text, "!test hello", sizeof(message.text) - 1);
  message.text_len = strlen(message.text);
  BotCommand command;
  assert(FirmwareBot::parseCommand(message.text, message.text_len, &command));
  char out[BOT_MAX_RESPONSE_LEN + 1];
  size_t written = 0;
  assert(FirmwareBot::writeAckResponse(message, command, out, sizeof(out), &written) == BOT_WRITE_OK);
  assert(strcmp(out, "ack @[alice] #bot | 0h SNR 0.00 | recv 21:25:45 | hello") == 0);
  assert(written == strlen(out));

  message.channel_kind = BOT_CHANNEL_DM;
  message.sender_name[0] = 0;
  message.channel_name[0] = 0;
  assert(FirmwareBot::parseCommand("!t", 2, &command));
  assert(FirmwareBot::writeAckResponse(message, command, out, sizeof(out), &written) == BOT_WRITE_OK);
  assert(strcmp(out, "ack @[unknown] direct | 0h SNR 0.00 | recv 21:25:45") == 0);

  uint8_t path[] = { 0x12, 0x34, 0xab, 0xcd };
  message.channel_kind = BOT_CHANNEL_BOT;
  strncpy(message.channel_name, "#bot", sizeof(message.channel_name) - 1);
  strncpy(message.sender_name, "bob", sizeof(message.sender_name) - 1);
  message.path = path;
  message.path_hash_size = 2;
  message.path_hash_count = 2;
  message.packet_snr_quarters = -5;
  assert(FirmwareBot::writeAckResponse(message, command, out, sizeof(out), &written) == BOT_WRITE_OK);
  assert(strcmp(out, "ack @[bob] #bot | 2h@2B SNR -1.25 | recv 21:25:45") == 0);
  assert(strstr(out, "Bot test OK") == NULL);

  char small[16];
  assert(FirmwareBot::writeAckResponse(message, command, small, sizeof(small), &written) == BOT_WRITE_TRUNCATED);
  assert(strncmp(small, "ack @[bob]", 10) == 0);
}

static void test_bot_advert_marker() {
  char out[32];
  size_t written = 0;
  assert(FirmwareBot::writeBotAdvertName("node", out, sizeof(out), &written) == BOT_WRITE_OK);
  assert(strcmp(out, "node" BOT_ADVERT_MARKER) == 0);
  assert(written == strlen(out));
  assert(FirmwareBot::isBotAdvertName(out, strlen(out)));
  assert(!FirmwareBot::isBotAdvertName("node", 4));
  assert(!FirmwareBot::isBotAdvertName("node [bot]", 10));

  char small[12];
  assert(FirmwareBot::writeBotAdvertName("very-long-node-name", small, sizeof(small), &written) == BOT_WRITE_TRUNCATED);
  assert(FirmwareBot::isBotAdvertName(small, strlen(small)));
  assert(written == sizeof(small) - 1);
}

static BotMessage make_message(const char* channel, const char* text) {
  BotMessage message;
  memset(&message, 0, sizeof(message));
  message.channel_kind = BotPolicy::classifyChannel(channel, strlen(channel), false);
  strncpy(message.channel_name, channel, sizeof(message.channel_name) - 1);
  strncpy(message.sender_name, "alice", sizeof(message.sender_name) - 1);
  for (size_t i = 0; i < sizeof(message.sender_key_prefix); i++) message.sender_key_prefix[i] = (uint8_t)(i + 1);
  message.sender_timestamp = 12345;
  message.received_at_timestamp = 12345;
  strncpy(message.text, text, sizeof(message.text) - 1);
  message.text_len = strlen(message.text);
  return message;
}

static BotCommandContext make_context() {
  BotCommandContext context;
  memset(&context, 0, sizeof(context));
  strncpy(context.node_name, "cm-bot", sizeof(context.node_name) - 1);
  strncpy(context.bot_channel, "#bot", sizeof(context.bot_channel) - 1);
  strncpy(context.testing_channel, "#testing", sizeof(context.testing_channel) - 1);
  strncpy(context.emergency_channel, "#emergency", sizeof(context.emergency_channel) - 1);
  strncpy(context.public_channel, "Public", sizeof(context.public_channel) - 1);
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
  strncpy(context.firmware_version, "v1.test", sizeof(context.firmware_version) - 1);
  strncpy(context.firmware_build_date, "14 May 2026", sizeof(context.firmware_build_date) - 1);
  context.suppressed_responses = 3;
  context.pending_responses = 6;
  context.emergency_forwards = 1;
  context.emergency_forward_failures = 0;
  context.packets_recv = 77;
  context.packets_sent = 44;
  context.packets_recv_errors = 2;
  context.queue_depth = 3;
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
  assert_starts_with(out, "ack @[");
  assert_contains(out, "recv ");
  assert(strstr(out, "Bot test OK") == NULL);

  result = run_command("!hello", out, sizeof(out));
  assert(result.code == BOT_COMMAND_RESULT_OK);
  assert(strcmp(out, "Hello from cm-bot") == 0);

  {
    BotCommand command;
    assert(FirmwareBot::parseCommand("!hello", 6, &command));
    BotCommandContext context = make_context();
    strncpy(context.response_target, "alice", sizeof(context.response_target) - 1);
    result = BotCommands::executeCommand(command, context, out, sizeof(out));
    assert(result.code == BOT_COMMAND_RESULT_OK);
    assert(strcmp(out, "Hello @[alice], from cm-bot") == 0);
  }

  result = run_command("!about", out, sizeof(out));
  assert(result.code == BOT_COMMAND_RESULT_OK);
  assert(strstr(out, "Colorado Mesh firmware bot") != NULL);

  result = run_command("!help", out, sizeof(out));
  assert(result.code == BOT_COMMAND_RESULT_OK);
  assert_starts_with(out, "Commands: help, cmd, ping");
  assert(result.text_len <= BOT_MAX_GROUP_RESPONSE_LEN);
  assert_contains(out, "roll");
  assert_contains(out, "dice");
  assert_contains(out, "trace");
  assert_contains(out, "tracer");
  assert_contains(out, "prefix");
  assert_contains(out, "help <command>");
  assert(strstr(out, "weather") == NULL);

  result = run_command("!cmd", out, sizeof(out));
  assert(result.code == BOT_COMMAND_RESULT_OK);
  assert_starts_with(out, "Commands: test, ping, help, hello, about");
  assert_contains(out, "prefix");
  assert_contains(out, "path");
  assert_contains(out, "dice");
  assert_contains(out, "roll");
  assert_contains(out, "stats");
  assert_contains(out, "time");
  assert_contains(out, "lora");
  assert_contains(out, "id");
  assert_contains(out, "neighbors");
  assert_contains(out, ", ");
  assert(strstr(out, "weather") == NULL);

  result = run_command("!help roll", out, sizeof(out));
  assert(result.code == BOT_COMMAND_RESULT_OK);
  assert_starts_with(out, "roll: Roll a random number between 1 and X");
  assert_contains(out, "Usage: roll [max|low high]");

  result = run_command("!help dice", out, sizeof(out));
  assert(result.code == BOT_COMMAND_RESULT_OK);
  assert_starts_with(out, "dice: Roll dice for tabletop games");
  assert_contains(out, "Usage: dice [NdX|dX|decade]");

  result = run_command("!help trace", out, sizeof(out));
  assert(result.code == BOT_COMMAND_RESULT_OK);
  assert_starts_with(out, "trace: Run link trace");
  assert_contains(out, "Usage: trace [path]");

  result = run_command("!help tracer", out, sizeof(out));
  assert(result.code == BOT_COMMAND_RESULT_OK);
  assert_starts_with(out, "tracer: Run link trace");
  assert_contains(out, "Usage: tracer [path]");

  result = run_command("!help t", out, sizeof(out));
  assert(result.code == BOT_COMMAND_RESULT_OK);
  assert_starts_with(out, "test: Get test response with connection info");

  result = run_command("!help weather", out, sizeof(out));
  assert(result.code == BOT_COMMAND_RESULT_OK);
  assert(strcmp(out, "No help for weather") == 0);

  result = run_command("!help prefix", out, sizeof(out));
  assert(result.code == BOT_COMMAND_RESULT_OK);
  assert_starts_with(out, "prefix: Look up a local contact by public-key prefix");
  assert_contains(out, "Usage: prefix <hex>");

  result = run_command("!prefix 01020304", out, sizeof(out));
  assert(result.code == BOT_COMMAND_RESULT_OK);
  assert(strcmp(out, "Prefix lookup requires local contacts") == 0);

  result = run_command("!lookup 0102", out, sizeof(out));
  assert(result.code == BOT_COMMAND_RESULT_OK);
  assert(strcmp(out, "Prefix lookup requires local contacts") == 0);

  result = run_command("!help nope", out, sizeof(out));
  assert(result.code == BOT_COMMAND_RESULT_OK);
  assert(strcmp(out, "No help for nope") == 0);

  result = run_command("!weather", out, sizeof(out));
  assert(result.code == BOT_COMMAND_RESULT_OK);
  assert(strcmp(out, "Unknown command. Try help") == 0);

  result = run_command("!reboot now", out, sizeof(out));
  assert(result.code == BOT_COMMAND_RESULT_OK);
  assert(strcmp(out, "Unknown command. Try help") == 0);

  result = run_command("!nodes", out, sizeof(out));
  assert(result.code == BOT_COMMAND_RESULT_OK);
  assert(strcmp(out, "Unknown command. Try help") == 0);

  result = run_command("!version", out, sizeof(out));
  assert(result.code == BOT_COMMAND_RESULT_OK);
  assert(strcmp(out, "Firmware v1.test built 14 May 2026") == 0);

  result = run_command("!stats", out, sizeof(out));
  assert(result.code == BOT_COMMAND_RESULT_OK);
  assert(strstr(out, "Bot: 9 seen, 5 ok, 4 sent, 1 fail") != NULL);
  assert(strstr(out, "RF: 77 rx, 44 tx, 2 err, q 3") != NULL);

  result = run_command("!magic8", out, sizeof(out));
  assert(result.code == BOT_COMMAND_RESULT_OK);
  assert(strstr(out, "Magic 8-ball:") == out);

  result = run_command("!path", out, sizeof(out));
  assert(result.code == BOT_COMMAND_RESULT_OK);
  assert(strcmp(out, "Path unavailable") == 0);

  result = run_command("!trace", out, sizeof(out));
  assert(result.code == BOT_COMMAND_RESULT_OK);
  assert(strcmp(out, "Trace needs a known route. Try path first.") == 0);

  result = run_command("!trace zz", out, sizeof(out));
  assert(result.code == BOT_COMMAND_RESULT_OK);
  assert(strcmp(out, "Usage: trace [path]") == 0);

  result = run_command("!trace 01,7a,55", out, sizeof(out));
  assert(result.code == BOT_COMMAND_RESULT_OK);
  assert(strcmp(out, "Trace ready: 3-hop route") == 0);

  result = run_command("!tracer", out, sizeof(out));
  assert(result.code == BOT_COMMAND_RESULT_OK);
  assert(strcmp(out, "Tracer needs a known route. Try path first.") == 0);

  result = run_command("!tracer 1234", out, sizeof(out));
  assert(result.code == BOT_COMMAND_RESULT_OK);
  assert(strcmp(out, "Tracer ready: 1-hop route") == 0);

  result = run_command("!channels", out, sizeof(out));
  assert(result.code == BOT_COMMAND_RESULT_OK);
  assert(strcmp(out, "Channels: bot #bot | testing #testing | emergency #emergency | public Public (4 total)") == 0);

  result = run_command("!status", out, sizeof(out));
  assert(result.code == BOT_COMMAND_RESULT_OK);
  assert(strstr(out, "cm-bot | up 0d 0h 20m") != NULL);
  assert(strstr(out, "batt 4180mV 97%") != NULL);
  assert(strstr(out, "sent 4 fail 1") != NULL);

  result = run_command("!wat", out, sizeof(out));
  assert(result.code == BOT_COMMAND_RESULT_OK);
  assert(strcmp(out, "Unknown command. Try help") == 0);
}

static void test_path_command() {
  char out[BOT_MAX_RESPONSE_LEN + 1];
  BotCommand command;
  assert(FirmwareBot::parseCommand("!path", 5, &command));
  BotCommandContext context = make_context();
  uint8_t path[] = { 0x12, 0x34, 0xab, 0xcd, 0x00, 0x01 };
  context.path = path;
  context.path_len = (uint8_t)(((2 - 1) << 6) | 3);
  context.path_hash_size = 2;
  context.path_hash_count = 3;
  context.path_snr_quarters = 23;
  BotCommandResult result = BotCommands::executeCommand(command, context, out, sizeof(out));
  assert(result.code == BOT_COMMAND_RESULT_OK);
  assert(strcmp(out, "Path 3h@2B SNR 5.75 | 1234 -> abcd -> 0001") == 0);

  strncpy(context.response_target, "alice", sizeof(context.response_target) - 1);
  result = BotCommands::executeCommand(command, context, out, sizeof(out));
  assert(result.code == BOT_COMMAND_RESULT_OK);
  assert(strcmp(out, "Path @[alice] 3h@2B SNR 5.75 | 1234 -> abcd -> 0001") == 0);
  context.response_target[0] = 0;

  assert(FirmwareBot::parseCommand("!path 1234,abcd,0001", 20, &command));
  result = BotCommands::executeCommand(command, context, out, sizeof(out));
  assert(result.code == BOT_COMMAND_RESULT_OK);
  assert(strcmp(out, "Path 3h@2B | 1234 -> abcd -> 0001") == 0);

  assert(FirmwareBot::parseCommand("!path 1234abcd0001", 18, &command));
  result = BotCommands::executeCommand(command, context, out, sizeof(out));
  assert(result.code == BOT_COMMAND_RESULT_OK);
  assert(strcmp(out, "Path 3h@2B | 1234 -> abcd -> 0001") == 0);

  assert(FirmwareBot::parseCommand("!path 12,34", 11, &command));
  result = BotCommands::executeCommand(command, context, out, sizeof(out));
  assert(result.code == BOT_COMMAND_RESULT_OK);
  assert(strcmp(out, "Usage: path [path]") == 0);

  assert(FirmwareBot::parseCommand("!path 12345678,abcdef01", 23, &command));
  result = BotCommands::executeCommand(command, context, out, sizeof(out));
  assert(result.code == BOT_COMMAND_RESULT_OK);
  assert(strcmp(out, "Usage: path [path]") == 0);

  context.path_hash_size = 0;
  result = BotCommands::executeCommand(command, context, out, sizeof(out));
  assert(result.code == BOT_COMMAND_RESULT_OK);
  assert(strcmp(out, "Path 2h@4B | 12345678 -> abcdef01") == 0);
  context.path_hash_size = 2;

  assert(FirmwareBot::parseCommand("!path", 5, &command));
  context.path = path;
  context.path_len = 0;
  context.path_hash_count = 0;
  context.path_hash_size = 2;
  context.path_snr_quarters = 0;
  result = BotCommands::executeCommand(command, context, out, sizeof(out));
  assert(result.code == BOT_COMMAND_RESULT_OK);
  assert(strcmp(out, "Path direct zero-hop SNR 0.00") == 0);

  context.path = NULL;
  result = BotCommands::executeCommand(command, context, out, sizeof(out));
  assert(result.code == BOT_COMMAND_RESULT_OK);
  assert(strcmp(out, "Path direct zero-hop SNR 0.00") == 0);

  strncpy(context.response_target, "alice", sizeof(context.response_target) - 1);
  result = BotCommands::executeCommand(command, context, out, sizeof(out));
  assert(result.code == BOT_COMMAND_RESULT_OK);
  assert(strcmp(out, "Path @[alice] direct zero-hop SNR 0.00") == 0);
  context.response_target[0] = 0;

  assert(FirmwareBot::parseCommand("!tracer", 7, &command));
  result = BotCommands::executeCommand(command, context, out, sizeof(out));
  assert(result.code == BOT_COMMAND_RESULT_OK);
  assert(strcmp(out, "Tracer ready: direct zero-hop route") == 0);

  context.path_hash_size = 0;
  result = BotCommands::executeCommand(command, context, out, sizeof(out));
  assert(result.code == BOT_COMMAND_RESULT_OK);
  assert(strcmp(out, "Tracer needs a known route. Try path first.") == 0);
}

static void test_trace_result_format() {
  char out[BOT_MAX_RESPONSE_LEN + 1];
  uint8_t snrs_1b[] = { 0x14, 0x0F, 0x06 };
  uint8_t hashes_1b[] = { 0x12, 0xab, 0x01 };
  size_t len = BotCommands::formatTraceResult(out, sizeof(out), NULL, 0xabcd1234UL, 1, snrs_1b, hashes_1b, 3, 18);
  assert(len == strlen(out));
  assert(strcmp(out, "Trace abcd1234 3h tail 4.50 | 12@5.00 -> ab@3.75 -> 01@1.50") == 0);

  len = BotCommands::formatTraceResult(out, sizeof(out), "alice", 0xabcd1234UL, 1, snrs_1b, hashes_1b, 3, 18);
  assert(strcmp(out, "Trace @[alice] abcd1234 3h tail 4.50 | 12@5.00 -> ab@3.75 -> 01@1.50") == 0);

  uint8_t snrs_2b[] = { 0x14, (uint8_t)(int8_t)-5 };
  uint8_t hashes_2b[] = { 0x12, 0x34, 0xab, 0xcd };
  len = BotCommands::formatTraceResult(out, sizeof(out), NULL, 0x10000001UL, 2, snrs_2b, hashes_2b, 2, -10);
  assert(strcmp(out, "Trace 10000001 2h tail -2.50 | 1234@5.00 -> abcd@-1.25") == 0);

  len = BotCommands::formatTraceResult(out, sizeof(out), "bob", 0x5UL, 1, NULL, NULL, 0, 8);
  assert(len > 0);
  assert(strcmp(out, "Trace @[bob] 00000005 direct zero-hop tail 2.00") == 0);

  len = BotCommands::formatTraceResult(out, sizeof(out), "", 0x5UL, 1, NULL, NULL, 0, 8);
  assert(strcmp(out, "Trace 00000005 direct zero-hop tail 2.00") == 0);

  char small[16];
  len = BotCommands::formatTraceResult(small, sizeof(small), NULL, 0x12345678UL, 1, snrs_1b, hashes_1b, 3, 18);
  assert(len == strlen(small));
  assert(len <= sizeof(small) - 1);
  assert(strncmp(small, "Trace 12345678", 14) == 0);
}

static void test_dice_command() {
  char out[BOT_MAX_RESPONSE_LEN + 1];
  BotCommandResult result = run_command("!roll", out, sizeof(out));
  assert(result.code == BOT_COMMAND_RESULT_OK);
  assert_roll_range(out, "Roll 1-100: ", 1, 100);

  result = run_command("!roll 20", out, sizeof(out));
  assert(result.code == BOT_COMMAND_RESULT_OK);
  assert_roll_range(out, "Roll 1-20: ", 1, 20);

  result = run_command("!roll 10000", out, sizeof(out));
  assert(result.code == BOT_COMMAND_RESULT_OK);
  assert_roll_range(out, "Roll 1-10000: ", 1, 10000);

  result = run_command("!roll 5 10", out, sizeof(out));
  assert(result.code == BOT_COMMAND_RESULT_OK);
  assert_roll_range(out, "Roll 5-10: ", 5, 10);

  result = run_command("!roll 1 1", out, sizeof(out));
  assert(result.code == BOT_COMMAND_RESULT_OK);
  assert(strcmp(out, "Roll 1-1: 1") == 0);

  result = run_command("!roll 2d6", out, sizeof(out));
  assert(result.code == BOT_COMMAND_RESULT_OK);
  assert(strcmp(out, "Invalid number. Use roll, roll X, or roll low high (max 10000)") == 0);

  result = run_command("!roll 0", out, sizeof(out));
  assert(result.code == BOT_COMMAND_RESULT_OK);
  assert_starts_with(out, "Invalid number.");

  result = run_command("!roll 10 1", out, sizeof(out));
  assert(result.code == BOT_COMMAND_RESULT_OK);
  assert_starts_with(out, "Invalid number.");

  result = run_command("!roll 10001", out, sizeof(out));
  assert(result.code == BOT_COMMAND_RESULT_OK);
  assert_starts_with(out, "Invalid number.");

  result = run_command("!dice", out, sizeof(out));
  assert(result.code == BOT_COMMAND_RESULT_OK);
  assert_dice_single_range(out, "Dice d6: ", 6);

  result = run_command("!dice d20", out, sizeof(out));
  assert(result.code == BOT_COMMAND_RESULT_OK);
  assert_dice_single_range(out, "Dice d20: ", 20);

  result = run_command("!dice 20", out, sizeof(out));
  assert(result.code == BOT_COMMAND_RESULT_OK);
  assert_dice_single_range(out, "Dice d20: ", 20);

  result = run_command("!dice 2d6", out, sizeof(out));
  assert(result.code == BOT_COMMAND_RESULT_OK);
  assert_starts_with(out, "Dice 2d6: [");
  assert(strchr(out, ',') != NULL);
  assert(strchr(out, '=') != NULL);

  result = run_command("!dice d10 d6", out, sizeof(out));
  assert(result.code == BOT_COMMAND_RESULT_OK);
  assert_starts_with(out, "Dice d10: ");
  assert_contains(out, " + d6: ");
  assert_contains(out, " | Total: ");

  result = run_command("!dice decade", out, sizeof(out));
  assert(result.code == BOT_COMMAND_RESULT_OK);
  assert_starts_with(out, "Dice decade: ");

  result = run_command("!dice d1", out, sizeof(out));
  assert(result.code == BOT_COMMAND_RESULT_OK);
  assert_starts_with(out, "Invalid dice type.");

  result = run_command("!dice 0d6", out, sizeof(out));
  assert(result.code == BOT_COMMAND_RESULT_OK);
  assert_starts_with(out, "Invalid dice type.");

  result = run_command("!dice 11d6", out, sizeof(out));
  assert(result.code == BOT_COMMAND_RESULT_OK);
  assert_starts_with(out, "Invalid dice type.");

  result = run_command("!dice 2d1001", out, sizeof(out));
  assert(result.code == BOT_COMMAND_RESULT_OK);
  assert_starts_with(out, "Invalid dice type.");
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

  memset(cooldowns, 0, sizeof(cooldowns));
  FirmwareBot::recordCommandCooldown(cooldowns, 2, BOT_COMMAND_TRACER, 2000, 5000);
  assert(FirmwareBot::isCommandOnCooldown(cooldowns, 2, BOT_COMMAND_TRACE, 6999));
  assert(FirmwareBot::isCommandOnCooldown(cooldowns, 2, BOT_COMMAND_TRACER, 6999));
  assert(!FirmwareBot::isCommandOnCooldown(cooldowns, 2, BOT_COMMAND_TRACE, 7000));
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
  assert(strstr(forward.parts[0], "EMERGENCY MESSAGE FROM @[alice]: need help at trailhead") == forward.parts[0]);
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
    assert(strstr(forward.parts[i], "EMERGENCY MESSAGE FROM @[alice]: ") == forward.parts[i]);
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

static void test_known_bot_registry() {
  BotKnownBotEntry entries[2];
  uint8_t key1[BOT_SENDER_KEY_PREFIX_LEN] = { 1, 2, 3, 4, 5, 6 };
  uint8_t key2[BOT_SENDER_KEY_PREFIX_LEN] = { 7, 8, 9, 10, 11, 12 };
  uint8_t key3[BOT_SENDER_KEY_PREFIX_LEN] = { 13, 14, 15, 16, 17, 18 };
  KnownBotRegistry::clear(entries, 2);

  assert(!KnownBotRegistry::find(entries, 2, key1, BOT_SENDER_KEY_PREFIX_LEN));
  assert(KnownBotRegistry::add(entries, 2, key1, BOT_KNOWN_BOT_FLAG_SUPPRESS_NORMAL, "alpha-bot"));
  const BotKnownBotEntry* entry = KnownBotRegistry::find(entries, 2, key1, BOT_SENDER_KEY_PREFIX_LEN);
  assert(entry != NULL);
  assert(entry->active);
  assert(strcmp(entry->label, "alpha-bot") == 0);
  assert(KnownBotRegistry::canSuppressNormal(entries, 2, key1, BOT_SENDER_KEY_PREFIX_LEN));
  assert(KnownBotRegistry::canSuppressNormal(entries, 2, key1, BOT_MIN_AUTH_SENDER_KEY_PREFIX_LEN));
  assert(!KnownBotRegistry::canSuppressNormal(entries, 2, key1, BOT_MIN_AUTH_SENDER_KEY_PREFIX_LEN - 1));
  assert(!KnownBotRegistry::canSuppressNormal(entries, 2, key2, BOT_SENDER_KEY_PREFIX_LEN));

  assert(KnownBotRegistry::add(entries, 2, key1, 0, "renamed-bot"));
  entry = KnownBotRegistry::find(entries, 2, key1, BOT_SENDER_KEY_PREFIX_LEN);
  assert(entry != NULL);
  assert(strcmp(entry->label, "renamed-bot") == 0);
  assert(!KnownBotRegistry::canSuppressNormal(entries, 2, key1, BOT_SENDER_KEY_PREFIX_LEN));

  assert(KnownBotRegistry::add(entries, 2, key2, BOT_KNOWN_BOT_FLAG_SUPPRESS_NORMAL, "beta"));
  assert(!KnownBotRegistry::add(entries, 2, key3, BOT_KNOWN_BOT_FLAG_SUPPRESS_NORMAL, "gamma"));
  assert(KnownBotRegistry::remove(entries, 2, key1));
  assert(!KnownBotRegistry::find(entries, 2, key1, BOT_SENDER_KEY_PREFIX_LEN));
  assert(KnownBotRegistry::add(entries, 2, key3, BOT_KNOWN_BOT_FLAG_SUPPRESS_NORMAL, NULL));
  assert(KnownBotRegistry::canSuppressNormal(entries, 2, key3, BOT_SENDER_KEY_PREFIX_LEN));
  assert(KnownBotRegistry::remove(entries, 2, key3));
  assert(!KnownBotRegistry::remove(entries, 2, key3));
}

static void test_known_bot_registry_ambiguous_short_prefix() {
  BotKnownBotEntry entries[2];
  uint8_t key1[BOT_SENDER_KEY_PREFIX_LEN] = { 1, 2, 3, 4, 5, 6 };
  uint8_t key2[BOT_SENDER_KEY_PREFIX_LEN] = { 1, 2, 3, 4, 7, 8 };
  KnownBotRegistry::clear(entries, 2);

  assert(KnownBotRegistry::add(entries, 2, key1, BOT_KNOWN_BOT_FLAG_SUPPRESS_NORMAL, "alpha"));
  assert(KnownBotRegistry::add(entries, 2, key2, BOT_KNOWN_BOT_FLAG_SUPPRESS_NORMAL, "beta"));
  assert(!KnownBotRegistry::canSuppressNormal(entries, 2, key1, BOT_MIN_AUTH_SENDER_KEY_PREFIX_LEN));
  assert(KnownBotRegistry::canSuppressNormal(entries, 2, key1, BOT_SENDER_KEY_PREFIX_LEN));
  assert(KnownBotRegistry::canSuppressNormal(entries, 2, key2, BOT_SENDER_KEY_PREFIX_LEN));
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
  b.sender_key_prefix[0] = a.sender_key_prefix[0];
  strncpy(b.sender_name, "bob", sizeof(b.sender_name) - 1);
  assert(fa.value != FirmwareBot::fingerprintFor(b).value);
}

static BotFingerprint final_response_fingerprint_for(const BotMessage& message, const char* text) {
  char response[BOT_MAX_RESPONSE_LEN + 1];
  size_t written = 0;
  assert(FirmwareBot::writeResponseForChannel(message.channel_kind, BotPolicy::isPrefixlessCommandAllowed(message.channel_kind),
                                              text, strlen(text), response, sizeof(response), &written) != BOT_WRITE_NO_SPACE);
  return FirmwareBot::responseFingerprintFor(message, response, written);
}

static void test_response_fingerprint() {
  BotMessage a = make_message("#bot", "!ping");
  BotMessage b = make_message("bot", "!ping");
  BotMessage c = make_message("#testing", "!ping");
  BotFingerprint fa = FirmwareBot::responseFingerprintFor(a, " Pong! ", 7);
  BotFingerprint fb = FirmwareBot::responseFingerprintFor(b, "pong!", 5);
  BotFingerprint fc = FirmwareBot::responseFingerprintFor(c, "pong!", 5);
  assert(fa.value == fb.value);
  assert(fa.value != fc.value);

  a.sender_key_prefix[0] = 99;
  a.sender_timestamp++;
  assert(fa.value == FirmwareBot::responseFingerprintFor(a, "pong!", 5).value);
  assert(fa.value != FirmwareBot::responseFingerprintFor(a, "status", 6).value);

  BotMessage dm = make_message("#bot", "!ping");
  dm.channel_kind = BOT_CHANNEL_DM;
  dm.channel_name[0] = 0;
  dm.sender_key_prefix_len = BOT_SENDER_KEY_PREFIX_LEN;
  BotMessage other_dm = dm;
  other_dm.sender_key_prefix[0] ^= 0x55;
  assert(FirmwareBot::responseFingerprintFor(dm, "pong!", 5).value !=
         FirmwareBot::responseFingerprintFor(other_dm, "pong!", 5).value);
}

static void test_authoritative_suppression_flow() {
  BotKnownBotEntry entries[1];
  uint8_t known_key[BOT_SENDER_KEY_PREFIX_LEN] = { 2, 4, 6, 8, 10, 12 };
  KnownBotRegistry::clear(entries, 1);
  assert(KnownBotRegistry::add(entries, 1, known_key, BOT_KNOWN_BOT_FLAG_SUPPRESS_NORMAL, "known"));

  BotCoordinatorPending pending[1];
  ResponseCoordinator::clear(pending, 1);
  BotMessage dm = make_message("#bot", "!ping");
  dm.channel_kind = BOT_CHANNEL_DM;
  dm.channel_name[0] = 0;
  memcpy(dm.sender_key_prefix, known_key, sizeof(known_key));
  dm.sender_key_prefix_len = BOT_SENDER_KEY_PREFIX_LEN;
  BotFingerprint request = FirmwareBot::fingerprintFor(dm);
  BotFingerprint response = FirmwareBot::responseFingerprintFor(dm, "Pong!", 5);
  BotFingerprint scheduled;
  uint32_t due = 0;

  assert(ResponseCoordinator::schedule(pending, 1, dm, BOT_COMMAND_PING, request, response, 1000, 0,
                                       0x01020304UL, 0, &scheduled, &due) == BOT_COORDINATOR_SCHEDULED);
  assert(KnownBotRegistry::canSuppressNormal(entries, 1, dm.sender_key_prefix, dm.sender_key_prefix_len));
  assert(ResponseCoordinator::suppress(pending, 1, FirmwareBot::responseFingerprintFor(dm, "Pong!", 5)));
  BotCoordinatorReady ready = ResponseCoordinator::poll(pending, 1, 1000);
  assert(ready.result == BOT_COORDINATOR_READY_SUPPRESSED);
  assert(ready.request_fingerprint.value == request.value);

  ResponseCoordinator::clear(pending, 1);
  BotMessage group = make_message("#bot", "cm-bot: Pong!");
  group.sender_key_prefix_len = 0;
  assert(!KnownBotRegistry::canSuppressNormal(entries, 1, group.sender_key_prefix, group.sender_key_prefix_len));
}

static void test_response_coordinator_schedule_poll() {
  BotCoordinatorPending pending[2];
  ResponseCoordinator::clear(pending, 2);
  BotMessage message = make_message("#bot", "!ping");
  BotFingerprint request = FirmwareBot::fingerprintFor(message);
  BotFingerprint response = FirmwareBot::responseFingerprintFor(message, "Pong!", 5);
  BotFingerprint scheduled;
  uint32_t due = 0;
  uint32_t identity_seed = 0xA5A55A5AUL;
  uint8_t queue_depth = 1;
  uint32_t jitter_seed = 17;

  assert(ResponseCoordinator::schedule(pending, 2, message, BOT_COMMAND_PING, request, response, 1000, jitter_seed,
                                       identity_seed, queue_depth, &scheduled, &due) == BOT_COORDINATOR_SCHEDULED);
  assert(scheduled.value == request.value);
  assert(due == 1000 + ResponseCoordinator::responseDelayMillis(message, BOT_COMMAND_PING, request, identity_seed,
                                                                 queue_depth, jitter_seed));
  BotCoordinatorReady ready = ResponseCoordinator::poll(pending, 2, due - 1);
  assert(ready.result == BOT_COORDINATOR_READY_NONE);
  ready = ResponseCoordinator::poll(pending, 2, due);
  assert(ready.result == BOT_COORDINATOR_READY_SEND);
  assert(ready.request_fingerprint.value == request.value);
  assert(ready.response_fingerprint.value == response.value);
  assert(ResponseCoordinator::poll(pending, 2, due).result == BOT_COORDINATOR_READY_NONE);

  assert(ResponseCoordinator::schedule(pending, 2, message, BOT_COMMAND_PING, request, response, 2000, 0,
                                       identity_seed, 0, &scheduled, &due) == BOT_COORDINATOR_SCHEDULED);
  assert(ResponseCoordinator::schedule(pending, 2, message, BOT_COMMAND_PING, request, response, 3000, 0,
                                       identity_seed, 0, &scheduled, &due) == BOT_COORDINATOR_REPLACED);
  ready = ResponseCoordinator::poll(pending, 2, due);
  assert(ready.result == BOT_COORDINATOR_READY_SEND);
  assert(ready.request_fingerprint.value == request.value);
  assert(ready.response_fingerprint.value == response.value);
}

static void test_response_coordinator_distinct_requests_same_response() {
  BotCoordinatorPending pending[2];
  ResponseCoordinator::clear(pending, 2);
  BotMessage first = make_message("#bot", "!ping");
  BotMessage second = first;
  second.sender_timestamp++;
  BotFingerprint first_request = FirmwareBot::fingerprintFor(first);
  BotFingerprint second_request = FirmwareBot::fingerprintFor(second);
  BotFingerprint response = FirmwareBot::responseFingerprintFor(first, "Pong!", 5);
  BotFingerprint scheduled;
  uint32_t first_due = 0;
  uint32_t second_due = 0;
  uint32_t identity_seed = 0x01020304UL;

  assert(first_request.value != second_request.value);
  assert(response.value == FirmwareBot::responseFingerprintFor(second, "Pong!", 5).value);
  assert(ResponseCoordinator::schedule(pending, 2, first, BOT_COMMAND_PING, first_request, response, 1000, 0,
                                       identity_seed, 0, &scheduled, &first_due) == BOT_COORDINATOR_SCHEDULED);
  assert(ResponseCoordinator::schedule(pending, 2, second, BOT_COMMAND_PING, second_request, response, 1000, 0,
                                       identity_seed, 0, &scheduled, &second_due) == BOT_COORDINATOR_SCHEDULED);
  assert(scheduled.value == second_request.value);

  bool first_is_early = first_due <= second_due;
  BotCoordinatorReady ready = ResponseCoordinator::poll(pending, 2, first_is_early ? first_due : second_due);
  assert(ready.result == BOT_COORDINATOR_READY_SEND);
  assert(ready.request_fingerprint.value == (first_is_early ? first_request.value : second_request.value));
  assert(ready.response_fingerprint.value == response.value);
  ready = ResponseCoordinator::poll(pending, 2, first_is_early ? second_due : first_due);
  assert(ready.result == BOT_COORDINATOR_READY_SEND);
  assert(ready.request_fingerprint.value == (first_is_early ? second_request.value : first_request.value));
  assert(ready.response_fingerprint.value == response.value);
}

static void test_response_coordinator_suppression_uses_response_fingerprint() {
  BotCoordinatorPending pending[2];
  ResponseCoordinator::clear(pending, 2);
  BotMessage first = make_message("#bot", "!ping");
  BotMessage second = first;
  second.sender_timestamp++;
  BotFingerprint first_request = FirmwareBot::fingerprintFor(first);
  BotFingerprint second_request = FirmwareBot::fingerprintFor(second);
  BotFingerprint response = FirmwareBot::responseFingerprintFor(first, "Pong!", 5);
  BotFingerprint scheduled;
  uint32_t due = 0;

  assert(ResponseCoordinator::schedule(pending, 2, first, BOT_COMMAND_PING, first_request, response, 1000, 0,
                                       0x0A0B0C0DUL, 0, &scheduled, &due) == BOT_COORDINATOR_SCHEDULED);
  assert(ResponseCoordinator::schedule(pending, 2, second, BOT_COMMAND_PING, second_request, response, 1000, 0,
                                       0x0A0B0C0DUL, 0, &scheduled, &due) == BOT_COORDINATOR_SCHEDULED);
  assert(ResponseCoordinator::suppress(pending, 2, response));
  BotCoordinatorReady ready = ResponseCoordinator::poll(pending, 2, 1000);
  assert(ready.result == BOT_COORDINATOR_READY_SUPPRESSED);
  assert(ready.request_fingerprint.value == first_request.value);
  assert(ready.response_fingerprint.value == response.value);
  ready = ResponseCoordinator::poll(pending, 2, due);
  assert(ready.result == BOT_COORDINATOR_READY_SUPPRESSED);
  assert(ready.request_fingerprint.value == second_request.value);
  assert(ready.response_fingerprint.value == response.value);
  assert(ResponseCoordinator::poll(pending, 2, due).result == BOT_COORDINATOR_READY_NONE);
}

static void test_trace_final_response_fingerprints_use_final_text() {
  BotMessage message = make_message("#bot", "trace");
  BotFingerprint sent = final_response_fingerprint_for(message, "Trace sent on 2-hop route");
  BotFingerprint result_a = final_response_fingerprint_for(message, "Trace 12345678 2h tail 1.25 | 1111@1.00 -> 2222@1.25");
  BotFingerprint result_b = final_response_fingerprint_for(message, "Trace 12345678 2h tail 1.25 | aaaa@1.00 -> bbbb@1.25");
  BotFingerprint timeout = final_response_fingerprint_for(message, "Trace 12345678 timed out, no reply on 2-hop route");
  assert(sent.value != result_a.value);
  assert(result_a.value != result_b.value);
  assert(timeout.value != sent.value);
  assert(timeout.value != result_a.value);
}

static void test_response_coordinator_distinct_path_outputs() {
  BotCoordinatorPending pending[2];
  ResponseCoordinator::clear(pending, 2);
  BotMessage first = make_message("#bot", "path");
  BotMessage second = first;
  second.sender_timestamp++;
  uint8_t path_a[] = { 0x12, 0x34, 0x56, 0x78 };
  uint8_t path_b[] = { 0xab, 0xcd, 0xef, 0x01 };
  first.path = path_a;
  first.path_hash_size = 2;
  first.path_hash_count = 2;
  second.path = path_b;
  second.path_hash_size = 2;
  second.path_hash_count = 2;
  BotFingerprint first_request = FirmwareBot::fingerprintFor(first);
  BotFingerprint second_request = FirmwareBot::fingerprintFor(second);
  const char* first_path_response = "Path 2h@2B SNR 0.00 | 1234 -> 5678";
  const char* second_path_response = "Path 2h@2B SNR 0.00 | abcd -> ef01";
  BotFingerprint first_response = FirmwareBot::responseFingerprintFor(first, first_path_response, strlen(first_path_response));
  BotFingerprint second_response = FirmwareBot::responseFingerprintFor(second, second_path_response, strlen(second_path_response));
  BotFingerprint scheduled;
  uint32_t first_due = 0;
  uint32_t second_due = 0;
  uint32_t identity_seed = 0x01020304UL;

  assert(first_request.value != second_request.value);
  assert(first_response.value != second_response.value);
  assert(ResponseCoordinator::schedule(pending, 2, first, BOT_COMMAND_PATH, first_request, first_response, 1000, 0,
                                       identity_seed, 0, &scheduled, &first_due) == BOT_COORDINATOR_SCHEDULED);
  assert(ResponseCoordinator::schedule(pending, 2, second, BOT_COMMAND_PATH, second_request, second_response, 1000, 0,
                                       identity_seed, 0, &scheduled, &second_due) == BOT_COORDINATOR_SCHEDULED);
  assert(ResponseCoordinator::suppress(pending, 2, first_response));
  BotCoordinatorReady ready = ResponseCoordinator::poll(pending, 2, 1000);
  assert(ready.result == BOT_COORDINATOR_READY_SUPPRESSED);
  assert(ready.response_fingerprint.value == first_response.value);
  ready = ResponseCoordinator::poll(pending, 2, first_due > second_due ? first_due : second_due);
  assert(ready.result == BOT_COORDINATOR_READY_SEND);
  assert(ready.request_fingerprint.value == second_request.value);
  assert(ready.response_fingerprint.value == second_response.value);
}

static void test_response_coordinator_group_observed_response_suppresses() {
  BotCoordinatorPending pending[1];
  ResponseCoordinator::clear(pending, 1);
  BotMessage request_message = make_message("#bot", "ping");
  BotMessage observed_response = make_message("#bot", "Pong!");
  BotFingerprint request = FirmwareBot::fingerprintFor(request_message);
  BotFingerprint response = final_response_fingerprint_for(request_message, "Pong!");
  BotFingerprint observed = FirmwareBot::responseFingerprintFor(observed_response, observed_response.text,
                                                               observed_response.text_len);
  BotFingerprint scheduled;
  uint32_t due = 0;

  assert(response.value == observed.value);
  assert(ResponseCoordinator::schedule(pending, 1, request_message, BOT_COMMAND_PING, request, response, 1000, 0,
                                       0x01020304UL, 0, &scheduled, &due) == BOT_COORDINATOR_SCHEDULED);
  assert(ResponseCoordinator::suppress(pending, 1, observed));
  BotCoordinatorReady ready = ResponseCoordinator::poll(pending, 1, 1000);
  assert(ready.result == BOT_COORDINATOR_READY_SUPPRESSED);
  assert(ready.response_fingerprint.value == response.value);
}

static void test_response_coordinator_group_guarded_path_output_suppresses() {
  BotCoordinatorPending pending[1];
  ResponseCoordinator::clear(pending, 1);
  BotMessage request_message = make_message("#bot", "path");
  BotFingerprint request = FirmwareBot::fingerprintFor(request_message);
  const char* path_text = "Path 2h@2B SNR 0.00 | 1234 -> 5678";
  char final_response[BOT_MAX_RESPONSE_LEN + 1];
  size_t final_response_len = 0;
  assert(FirmwareBot::writeResponseForChannel(request_message.channel_kind,
                                              BotPolicy::isPrefixlessCommandAllowed(request_message.channel_kind),
                                              path_text, strlen(path_text), final_response, sizeof(final_response),
                                              &final_response_len) == BOT_WRITE_OK);
  assert(strcmp(final_response, "# Path 2h@2B SNR 0.00 | 1234 -> 5678") == 0);
  BotCommand command;
  assert(!FirmwareBot::parseCommand(final_response, final_response_len, &command, true));
  BotFingerprint response = FirmwareBot::responseFingerprintFor(request_message, final_response, final_response_len);
  BotMessage observed_response = make_message("#bot", final_response);
  BotFingerprint observed = FirmwareBot::responseFingerprintFor(observed_response, observed_response.text,
                                                               observed_response.text_len);
  BotFingerprint scheduled;
  uint32_t due = 0;

  assert(response.value == observed.value);
  assert(ResponseCoordinator::schedule(pending, 1, request_message, BOT_COMMAND_PATH, request, response, 1000, 0,
                                       0x01020304UL, 0, &scheduled, &due) == BOT_COORDINATOR_SCHEDULED);
  assert(ResponseCoordinator::suppress(pending, 1, observed));
  BotCoordinatorReady ready = ResponseCoordinator::poll(pending, 1, 1000);
  assert(ready.result == BOT_COORDINATOR_READY_SUPPRESSED);
  assert(ready.response_fingerprint.value == response.value);
}

static void test_response_coordinator_suppress_expire_full() {
  BotCoordinatorPending pending[1];
  ResponseCoordinator::clear(pending, 1);
  BotMessage message = make_message("#bot", "!ping");
  BotMessage second_message = make_message("#bot", "!test");
  BotFingerprint first_request = FirmwareBot::fingerprintFor(message);
  BotFingerprint second_request = FirmwareBot::fingerprintFor(second_message);
  BotFingerprint first_response = FirmwareBot::responseFingerprintFor(message, "Pong!", 5);
  const char* ack_response = "ack @[alice] | direct | Received at: 21:25:45";
  BotFingerprint second_response = FirmwareBot::responseFingerprintFor(second_message, ack_response, strlen(ack_response));
  BotFingerprint scheduled;
  uint32_t due = 0;
  uint32_t identity_seed = 0x11223344UL;

  assert(ResponseCoordinator::schedule(pending, 1, message, BOT_COMMAND_PING, first_request, first_response, 1000, 0,
                                       identity_seed, 0, &scheduled, &due) == BOT_COORDINATOR_SCHEDULED);
  assert(ResponseCoordinator::schedule(pending, 1, second_message, BOT_COMMAND_TEST, second_request, second_response, 1000,
                                       0, identity_seed, 0, &scheduled, &due) == BOT_COORDINATOR_NO_SPACE);
  assert(ResponseCoordinator::suppress(pending, 1, first_response));
  BotCoordinatorReady ready = ResponseCoordinator::poll(pending, 1, 1000);
  assert(ready.result == BOT_COORDINATOR_READY_SUPPRESSED);
  assert(ready.request_fingerprint.value == first_request.value);
  assert(ready.response_fingerprint.value == first_response.value);
  assert(!ResponseCoordinator::suppress(pending, 1, first_response));

  assert(ResponseCoordinator::schedule(pending, 1, message, BOT_COMMAND_PING, first_request, first_response, 1000, 0,
                                       identity_seed, 0, &scheduled, &due) == BOT_COORDINATOR_SCHEDULED);
  assert(ResponseCoordinator::schedule(pending, 1, second_message, BOT_COMMAND_TEST, second_request, second_response,
                                       1000 + BOT_RESPONSE_PENDING_TTL_MILLIS, 0, identity_seed, 0, &scheduled,
                                       &due) == BOT_COORDINATOR_NO_SPACE);
  ready = ResponseCoordinator::poll(pending, 1, 1000 + BOT_RESPONSE_PENDING_TTL_MILLIS);
  assert(ready.result == BOT_COORDINATOR_READY_EXPIRED);
  assert(ready.request_fingerprint.value == first_request.value);
  assert(ready.response_fingerprint.value == first_response.value);
  assert(ResponseCoordinator::schedule(pending, 1, second_message, BOT_COMMAND_TEST, second_request, second_response,
                                       1000 + BOT_RESPONSE_PENDING_TTL_MILLIS, 0, identity_seed, 0, &scheduled,
                                       &due) == BOT_COORDINATOR_SCHEDULED);

  ResponseCoordinator::clear(pending, 1);
  assert(ResponseCoordinator::schedule(pending, 1, message, BOT_COMMAND_PING, first_request, first_response,
                                       0xFFFFFF00UL, 0, identity_seed, 0, &scheduled, &due) == BOT_COORDINATOR_SCHEDULED);
  ready = ResponseCoordinator::poll(pending, 1, 0xFFFFFF00UL);
  assert(ready.result == BOT_COORDINATOR_READY_NONE);
  ready = ResponseCoordinator::poll(pending, 1, due);
  assert(ready.result == BOT_COORDINATOR_READY_SEND);
  assert(ready.request_fingerprint.value == first_request.value);
  assert(ready.response_fingerprint.value == first_response.value);
}

static void test_response_coordinator_cancel_by_request() {
  BotCoordinatorPending pending[2];
  ResponseCoordinator::clear(pending, 2);
  BotMessage first = make_message("#bot", "!ping");
  BotMessage second = first;
  second.sender_timestamp++;
  BotFingerprint first_request = FirmwareBot::fingerprintFor(first);
  BotFingerprint second_request = FirmwareBot::fingerprintFor(second);
  BotFingerprint response = FirmwareBot::responseFingerprintFor(first, "Pong!", 5);
  BotFingerprint scheduled;
  uint32_t first_due = 0;
  uint32_t second_due = 0;

  assert(ResponseCoordinator::schedule(pending, 2, first, BOT_COMMAND_PING, first_request, response, 1000, 0,
                                       0x01020304UL, 0, &scheduled, &first_due) == BOT_COORDINATOR_SCHEDULED);
  assert(ResponseCoordinator::schedule(pending, 2, second, BOT_COMMAND_PING, second_request, response, 1000, 0,
                                       0x01020304UL, 0, &scheduled, &second_due) == BOT_COORDINATOR_SCHEDULED);
  assert(ResponseCoordinator::cancel(pending, 2, first_request));
  assert(!ResponseCoordinator::cancel(pending, 2, first_request));
  BotCoordinatorReady ready = ResponseCoordinator::poll(pending, 2, first_due > second_due ? first_due : second_due);
  assert(ready.result == BOT_COORDINATOR_READY_SEND);
  assert(ready.request_fingerprint.value == second_request.value);
  assert(ready.response_fingerprint.value == response.value);
}

static void test_response_coordinator_schedule_preserves_suppression() {
  BotCoordinatorPending pending[1];
  ResponseCoordinator::clear(pending, 1);
  BotMessage message = make_message("#bot", "!ping");
  BotFingerprint request = FirmwareBot::fingerprintFor(message);
  BotFingerprint response = FirmwareBot::responseFingerprintFor(message, "Pong!", 5);
  BotFingerprint scheduled;
  uint32_t due = 0;

  assert(ResponseCoordinator::schedule(pending, 1, message, BOT_COMMAND_PING, request, response, 1000, 0,
                                       0x01020304UL, 0, &scheduled, &due) == BOT_COORDINATOR_SCHEDULED);
  assert(ResponseCoordinator::suppress(pending, 1, response));
  assert(pending[0].suppressed);
  assert(ResponseCoordinator::schedule(pending, 1, message, BOT_COMMAND_PING, request, response, 1500, 0,
                                       0x01020304UL, 0, &scheduled, &due) == BOT_COORDINATOR_REPLACED);
  assert(pending[0].suppressed);
  BotCoordinatorReady ready = ResponseCoordinator::poll(pending, 1, 1500);
  assert(ready.result == BOT_COORDINATOR_READY_SUPPRESSED);
}

static void test_response_coordinator_delay_biases() {
  BotMessage message = make_message("#bot", "!ping");
  BotFingerprint request = { 0x0123456789ABCDEFULL };
  uint32_t first = ResponseCoordinator::responseDelayMillis(message, BOT_COMMAND_PING, request, 0x01020304UL, 0, 0);
  uint32_t second = ResponseCoordinator::responseDelayMillis(message, BOT_COMMAND_PING, request, 0x05060708UL, 0, 0);
  uint32_t roll = ResponseCoordinator::responseDelayMillis(message, BOT_COMMAND_ROLL, request, 0x01020304UL, 0, 0);
  uint32_t dice = ResponseCoordinator::responseDelayMillis(message, BOT_COMMAND_DICE, request, 0x01020304UL, 0, 0);
  uint32_t trace = ResponseCoordinator::responseDelayMillis(message, BOT_COMMAND_TRACE, request, 0x01020304UL, 0, 0);
  uint32_t tracer = ResponseCoordinator::responseDelayMillis(message, BOT_COMMAND_TRACER, request, 0x01020304UL, 0, 0);
  assert(first != second);
  assert(roll == first);
  assert(dice == first);
  assert(trace == first);
  assert(tracer == first);
  assert(ResponseCoordinator::responseDelayMillis(message, BOT_COMMAND_PING, request, 0x01020304UL, 2,
                                                  BOT_RESPONSE_DELAY_JITTER_MILLIS + 17) == first + 300 + 17);
}

static void test_response_coordinator_hop_count_ranking() {
  BotMessage near = make_message("#bot", "!ping");
  near.path_hash_count = 0;
  BotMessage one_hop = near;
  one_hop.path_hash_count = 1;
  BotMessage two_hop = near;
  two_hop.path_hash_count = 2;
  BotFingerprint request = { 0x0123456789ABCDEFULL };
  uint32_t base_delay = BOT_RESPONSE_DELAY_BASE_MILLIS;
  uint32_t jitter = BOT_RESPONSE_DELAY_JITTER_MILLIS;
  uint16_t hop_step = 1500;
  uint32_t d0 = ResponseCoordinator::responseDelayMillis(near, BOT_COMMAND_PING, request, 0x01020304UL, 0, 0,
                                                          base_delay, jitter, hop_step);
  uint32_t d1 = ResponseCoordinator::responseDelayMillis(one_hop, BOT_COMMAND_PING, request, 0x01020304UL, 0, 0,
                                                          base_delay, jitter, hop_step);
  uint32_t d2 = ResponseCoordinator::responseDelayMillis(two_hop, BOT_COMMAND_PING, request, 0x01020304UL, 0, 0,
                                                          base_delay, jitter, hop_step);
  assert(d1 == d0 + hop_step);
  assert(d2 == d0 + (uint32_t)hop_step * 2);

  uint32_t alt_step = 2000;
  uint32_t d0_alt = ResponseCoordinator::responseDelayMillis(near, BOT_COMMAND_PING, request, 0x01020304UL, 0, 0,
                                                              base_delay, jitter, alt_step);
  uint32_t d1_alt = ResponseCoordinator::responseDelayMillis(one_hop, BOT_COMMAND_PING, request, 0x01020304UL, 0, 0,
                                                              base_delay, jitter, alt_step);
  assert(d0_alt == d0);
  assert(d1_alt == d0 + alt_step);

  BotMessage dm = make_message("dm", "!ping");
  dm.channel_kind = BOT_CHANNEL_DM;
  dm.path_hash_count = 3;
  uint32_t dm_delay = ResponseCoordinator::responseDelayMillis(dm, BOT_COMMAND_PING, request, 0x01020304UL, 0, 0,
                                                                base_delay, jitter, hop_step);
  BotMessage dm_zero = dm;
  dm_zero.path_hash_count = 0;
  uint32_t dm_zero_delay = ResponseCoordinator::responseDelayMillis(dm_zero, BOT_COMMAND_PING, request, 0x01020304UL,
                                                                     0, 0, base_delay, jitter, hop_step);
  assert(dm_delay == dm_zero_delay);
}

static void test_response_coordinator_hop_bias_cap() {
  BotMessage near = make_message("#bot", "!ping");
  near.path_hash_count = 0;
  BotMessage many_hops = near;
  many_hops.path_hash_count = 10;
  BotFingerprint request = { 0x0123456789ABCDEFULL };
  uint32_t base_delay = BOT_RESPONSE_DELAY_BASE_MILLIS;
  uint32_t jitter = BOT_RESPONSE_DELAY_JITTER_MILLIS;
  uint16_t hop_step = 1500;
  uint32_t d0 = ResponseCoordinator::responseDelayMillis(near, BOT_COMMAND_PING, request, 0x01020304UL, 0, 0,
                                                          base_delay, jitter, hop_step);
  uint32_t dmany = ResponseCoordinator::responseDelayMillis(many_hops, BOT_COMMAND_PING, request, 0x01020304UL, 0, 0,
                                                             base_delay, jitter, hop_step);
  assert(dmany == d0 + BOT_HOP_BIAS_MAX_MILLIS);
  assert((uint32_t)many_hops.path_hash_count * hop_step > BOT_HOP_BIAS_MAX_MILLIS);
  assert(dmany + BOT_RESPONSE_DELAY_JITTER_MILLIS < BOT_RESPONSE_PENDING_TTL_MILLIS);
}

static void test_response_coordinator_same_hop_tiebreak() {
  BotMessage message = make_message("#bot", "!ping");
  message.path_hash_count = 2;
  BotFingerprint request = { 0x0123456789ABCDEFULL };
  uint16_t hop_step = 1500;
  uint32_t bot_a = ResponseCoordinator::responseDelayMillis(message, BOT_COMMAND_PING, request, 0x01020304UL, 0, 0,
                                                             BOT_RESPONSE_DELAY_BASE_MILLIS,
                                                             BOT_RESPONSE_DELAY_JITTER_MILLIS, hop_step);
  uint32_t bot_b = ResponseCoordinator::responseDelayMillis(message, BOT_COMMAND_PING, request, 0xFEDCBA98UL, 0, 0,
                                                             BOT_RESPONSE_DELAY_BASE_MILLIS,
                                                             BOT_RESPONSE_DELAY_JITTER_MILLIS, hop_step);
  assert(bot_a != bot_b);
  uint32_t spread = bot_a > bot_b ? bot_a - bot_b : bot_b - bot_a;
  assert(spread < 900);
}

static void test_response_coordinator_recent_responses() {
  BotCoordinatorRecent recent[1];
  ResponseCoordinator::clearRecent(recent, 1);
  BotFingerprint response = { 0xFEDCBA9876543210ULL };
  BotFingerprint zero = { 0 };

  assert(!ResponseCoordinator::recentlySent(recent, 1, response, 1000));
  ResponseCoordinator::recordRecent(recent, 1, zero, 1000);
  assert(!ResponseCoordinator::recentlySent(recent, 1, response, 1000));
  ResponseCoordinator::recordRecent(recent, 1, response, 1000);
  assert(ResponseCoordinator::recentlySent(recent, 1, response, 1000));
  assert(ResponseCoordinator::recentlySent(recent, 1, response, 1000 + BOT_RESPONSE_RECENT_TTL_MILLIS - 1));
  assert(!ResponseCoordinator::recentlySent(recent, 1, response, 1000 + BOT_RESPONSE_RECENT_TTL_MILLIS));
}

static void test_response_coordinator_rejects_non_normal() {
  BotCoordinatorPending pending[1];
  ResponseCoordinator::clear(pending, 1);
  BotMessage bot_message = make_message("#bot", "!ping");
  BotMessage public_message = make_message("Public", "!ping");
  BotMessage emergency_message = make_message("#emergency", "need help");
  BotFingerprint request = FirmwareBot::fingerprintFor(bot_message);
  BotFingerprint response = FirmwareBot::responseFingerprintFor(bot_message, "Pong!", 5);
  BotFingerprint zero = { 0 };
  BotFingerprint scheduled;
  uint32_t due = 99;

  assert(ResponseCoordinator::schedule(pending, 1, public_message, BOT_COMMAND_PING, request, response, 1000, 0,
                                       0, 0, &scheduled, &due) == BOT_COORDINATOR_NOT_NORMAL);
  assert(scheduled.value == 0);
  assert(due == 0);
  assert(ResponseCoordinator::schedule(pending, 1, emergency_message, BOT_COMMAND_PING, request, response, 1000, 0,
                                       0, 0, &scheduled, &due) == BOT_COORDINATOR_NOT_NORMAL);
  assert(ResponseCoordinator::schedule(pending, 1, bot_message, BOT_COMMAND_PING, zero, response, 1000, 0,
                                       0, 0, &scheduled, &due) == BOT_COORDINATOR_NOT_NORMAL);
  assert(ResponseCoordinator::schedule(pending, 1, bot_message, BOT_COMMAND_PING, request, zero, 1000, 0,
                                       0, 0, &scheduled, &due) == BOT_COORDINATOR_NOT_NORMAL);
}

int main() {
  test_command_registry();
  test_channel_policy();
  test_bot_prefs_defaults();
  test_bot_prefs_serialization_round_trip();
  test_bot_prefs_rejects_corrupt_and_wrong_version();
  test_bot_prefs_validation_and_command_mask();
  test_unknown_command_runtime_gate();
  test_bot_prefs_known_bot_helpers();
  test_prefs_aware_channel_policy();
  test_normalize_text();
  test_normalize_truncation();
  test_parse_command();
  test_response_write();
  test_channel_text_and_response_guards();
  test_ack_response_format();
  test_bot_advert_marker();
  test_command_outputs();
  test_path_command();
  test_trace_result_format();
  test_dice_command();
  test_command_truncation();
  test_command_cooldown();
  test_group_response_cap();
  test_emergency_forward_short();
  test_emergency_forward_loop_prevention();
  test_emergency_forward_multipart();
  test_emergency_forward_truncation();
  test_known_bot_registry();
  test_known_bot_registry_ambiguous_short_prefix();
  test_fingerprint();
  test_response_fingerprint();
  test_authoritative_suppression_flow();
  test_response_coordinator_schedule_poll();
  test_response_coordinator_distinct_requests_same_response();
  test_response_coordinator_suppression_uses_response_fingerprint();
  test_trace_final_response_fingerprints_use_final_text();
  test_response_coordinator_distinct_path_outputs();
  test_response_coordinator_schedule_preserves_suppression();
  test_response_coordinator_group_observed_response_suppresses();
  test_response_coordinator_group_guarded_path_output_suppresses();
  test_response_coordinator_suppress_expire_full();
  test_response_coordinator_cancel_by_request();
  test_response_coordinator_delay_biases();
  test_response_coordinator_hop_count_ranking();
  test_response_coordinator_hop_bias_cap();
  test_response_coordinator_same_hop_tiebreak();
  test_response_coordinator_recent_responses();
  test_response_coordinator_rejects_non_normal();
  printf("firmware_bot tests passed\n");
  return 0;
}
