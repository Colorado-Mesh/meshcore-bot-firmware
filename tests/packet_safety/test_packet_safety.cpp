#include <assert.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <SHA256.h>
#include <Packet.h>
#include <Utils.h>

static void test_ciphertext_bounds() {
  uint8_t key[PUB_KEY_SIZE] = {};
  uint8_t source[MAX_PACKET_PAYLOAD] = {};
  uint8_t output[MAX_PACKET_PAYLOAD];
  for (int len = 1; len < MAX_PACKET_PAYLOAD; len++) {
    memset(output, 0xA5, sizeof(output));
    int result = mesh::Utils::decrypt(key, output, source, len);
    if (len % CIPHER_BLOCK_SIZE) {
      assert(result == 0);
      for (uint8_t b : output) assert(b == 0xA5);
    } else {
      assert(result == len);
      for (size_t i = len; i < sizeof(output); i++) assert(output[i] == 0xA5);
    }
  }
  assert(mesh::Utils::decrypt(key, output, source, -1) == 0);
  assert(mesh::Utils::decrypt(key, output, source, 0) == 0);

  // Use the real HMAC implementation, including valid MACs on malformed lengths.
  for (int len = CIPHER_MAC_SIZE + 1; len <= MAX_PACKET_PAYLOAD; len++) {
    SHA256 sha;
    sha.resetHMAC(key, sizeof(key));
    sha.update(source + CIPHER_MAC_SIZE, len - CIPHER_MAC_SIZE);
    sha.finalizeHMAC(key, sizeof(key), source, CIPHER_MAC_SIZE);
    memset(output, 0xA5, sizeof(output));
    int result = mesh::Utils::MACThenDecrypt(key, output, source, len);
    int ciphertext_len = len - CIPHER_MAC_SIZE;
    assert(result == (ciphertext_len % CIPHER_BLOCK_SIZE ? 0 : ciphertext_len));
    size_t written = result;
    for (size_t i = written; i < sizeof(output); i++) assert(output[i] == 0xA5);
  }

  uint8_t plaintext[31];
  memset(plaintext, 0x42, sizeof(plaintext));
  int len = mesh::Utils::encryptThenMAC(key, source, plaintext, sizeof(plaintext));
  assert(len == 34);
  assert(mesh::Utils::MACThenDecrypt(key, output, source, len) == 32);
  assert(memcmp(plaintext, output, sizeof(plaintext)) == 0);
  source[0] ^= 1;
  assert(mesh::Utils::MACThenDecrypt(key, output, source, len) == 0);
}

static void test_path_payload_bounds() {
  for (unsigned encoded = 0; encoded <= 255; encoded++) {
    unsigned count = encoded & 63;
    unsigned width = (encoded >> 6) + 1;
    unsigned bytes = count * width;
    for (size_t len = 0; len <= MAX_PACKET_PAYLOAD; len++) {
      bool expected = width <= 3 && bytes <= MAX_PATH_SIZE && bytes + 2 <= len;
      assert(mesh::Packet::isValidPathPayload(encoded, len) == expected);
    }
  }
  assert(!mesh::Packet::isValidPathPayload(0x3F, 16));
  assert(!mesh::Packet::isValidPathPayload(0xFF, 176));
  assert(mesh::Packet::isValidPathPayload(0x60, 80));
}

void test_contact_response_bounds();

int main() {
  test_ciphertext_bounds();
  test_path_payload_bounds();
  test_contact_response_bounds();
  puts("packet safety tests passed");
}
