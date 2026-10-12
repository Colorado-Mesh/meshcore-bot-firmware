#include <assert.h>
#include <stdint.h>
#include <string.h>

#define MAX_FRAME_SIZE 176
#define PUSH_CODE_LOGIN_SUCCESS 5
#define PUSH_CODE_LOGIN_FAIL 6
#define PUSH_CODE_STATUS_RESPONSE 7
#define PUSH_CODE_TELEMETRY_RESPONSE 8
#define PUSH_CODE_BINARY_RESPONSE 9
#define RESP_SERVER_LOGIN_OK 0

struct ContactInfo { struct { uint8_t pub_key[32]; } id; };
struct TestSerial {
  size_t writes = 0;
  size_t last_len = 0;
  void writeFrame(const uint8_t*, size_t len) {
    assert(len <= MAX_FRAME_SIZE);
    writes++;
    last_len = len;
  }
};
class MyMesh {
public:
  uint8_t out_frame[MAX_FRAME_SIZE + 1] = {};
  uint8_t canary[16];
  uint32_t pending_login = 0, pending_status = 0, pending_telemetry = 0, pending_req = 0;
  TestSerial serial;
  TestSerial* _serial = &serial;
  MyMesh() { memset(canary, 0xA5, sizeof(canary)); }
  void startConnection(const ContactInfo&, uint16_t) {}
  void onContactResponse(const ContactInfo&, const uint8_t*, uint8_t);
};

// The runner extracts this method unchanged from production MyMesh.cpp.
#include "contact_response.inc"

void test_contact_response_bounds() {
  ContactInfo contact = {};
  contact.id.pub_key[0] = 1;
  MyMesh mesh;
  uint8_t data[176] = {};
  mesh.onContactResponse(contact, data, sizeof(data));
  assert(mesh.serial.writes == 0);
  for (uint8_t b : mesh.canary) assert(b == 0xA5);

  uint32_t tag = 123;
  memcpy(data, &tag, sizeof(tag));
  mesh.pending_telemetry = tag;
  mesh.onContactResponse(contact, data, sizeof(data));
  assert(mesh.serial.writes == 0 && mesh.pending_telemetry == tag);
  mesh.onContactResponse(contact, data, 160);
  assert(mesh.serial.writes == 1 && mesh.serial.last_len == 164);
  assert(mesh.pending_telemetry == 0);
  for (uint8_t b : mesh.canary) assert(b == 0xA5);

  for (uint8_t len = 0; len < 4; len++) {
    mesh.onContactResponse(contact, data, len);
    assert(mesh.serial.writes == 1);
  }
  memset(data, 0, sizeof(data));
  mesh.onContactResponse(contact, data, 16);
  assert(mesh.serial.writes == 1);  // no unsolicited tag-zero responses
  memcpy(&mesh.pending_login, contact.id.pub_key, 4);
  mesh.onContactResponse(contact, data, 4);
  assert(mesh.serial.writes == 2 && mesh.out_frame[0] == PUSH_CODE_LOGIN_FAIL);
}
