/* Copyright (c) 2025 Oleg Ekhlakov <subspam@mail.ru>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

/*
 * Unit tests for the protobuf decode shim (src/pinba.pb-c.cc). This is the
 * parsing path for untrusted UDP packets, so both the happy path (a valid
 * Pinba::Request round-trips into the flat Pinba__Request C struct) and the
 * rejection of malformed / incomplete input are worth pinning down.
 *
 * Inputs are produced with the generated C++ message (pinba.pb.h) so the test
 * always agrees with the on-the-wire format defined by pinba.proto.
 */

#include <gtest/gtest.h>

#include <cstdint>
#include <string>

#include "pinba.pb-c.h"
#include "pinba.pb.h"

namespace {

/* Build a minimally-valid request with all proto2 `required` fields set. */
Pinba::Request MakeValidRequest() {
  Pinba::Request req;
  req.set_hostname("web01");
  req.set_server_name("api.example.com");
  req.set_script_name("/index.php");
  req.set_request_count(3);
  req.set_document_size(2048);
  req.set_memory_peak(1048576);
  req.set_request_time(0.125f);
  req.set_ru_utime(0.05f);
  req.set_ru_stime(0.02f);
  return req;
}

TEST(PinbaProtobuf, UnpackValidRequest) {
  Pinba::Request req = MakeValidRequest();
  std::string buf;
  ASSERT_TRUE(req.SerializeToString(&buf));

  Pinba__Request *msg =
      pinba__request__unpack(nullptr, buf.size(), reinterpret_cast<const uint8_t *>(buf.data()));
  ASSERT_NE(msg, nullptr);

  EXPECT_STREQ(msg->hostname, "web01");
  EXPECT_STREQ(msg->server_name, "api.example.com");
  EXPECT_STREQ(msg->script_name, "/index.php");
  EXPECT_EQ(msg->request_count, 3u);
  EXPECT_EQ(msg->document_size, 2048u);
  EXPECT_EQ(msg->memory_peak, 1048576u);
  EXPECT_FLOAT_EQ(msg->request_time, 0.125f);
  EXPECT_FLOAT_EQ(msg->ru_utime, 0.05f);
  EXPECT_FLOAT_EQ(msg->ru_stime, 0.02f);
  /* Optional fields absent -> has_* flags false. */
  EXPECT_FALSE(msg->has_status);
  EXPECT_FALSE(msg->has_memory_footprint);
  /* Repeated fields empty. */
  EXPECT_EQ(msg->n_timer_value, 0u);
  EXPECT_EQ(msg->n_dictionary, 0u);
  EXPECT_EQ(msg->n_requests, 0u);

  pinba__request__free_unpacked(msg, nullptr);
}

TEST(PinbaProtobuf, UnpackRepeatedAndOptionalFields) {
  Pinba::Request req = MakeValidRequest();
  req.set_status(200);
  req.set_memory_footprint(4096);
  req.set_schema("http");

  /* One timer with two tag name/value pairs. */
  req.add_timer_hit_count(1);
  req.add_timer_value(0.5f);
  req.add_timer_tag_count(2);
  req.add_timer_tag_name(10);
  req.add_timer_tag_name(11);
  req.add_timer_tag_value(20);
  req.add_timer_tag_value(21);

  req.add_dictionary("db");
  req.add_dictionary("select");

  std::string buf;
  ASSERT_TRUE(req.SerializeToString(&buf));

  Pinba__Request *msg =
      pinba__request__unpack(nullptr, buf.size(), reinterpret_cast<const uint8_t *>(buf.data()));
  ASSERT_NE(msg, nullptr);

  EXPECT_TRUE(msg->has_status);
  EXPECT_EQ(msg->status, 200u);
  EXPECT_TRUE(msg->has_memory_footprint);
  EXPECT_EQ(msg->memory_footprint, 4096u);
  EXPECT_STREQ(msg->schema, "http");

  ASSERT_EQ(msg->n_timer_hit_count, 1u);
  EXPECT_EQ(msg->timer_hit_count[0], 1u);
  ASSERT_EQ(msg->n_timer_value, 1u);
  EXPECT_FLOAT_EQ(msg->timer_value[0], 0.5f);
  ASSERT_EQ(msg->n_timer_tag_name, 2u);
  EXPECT_EQ(msg->timer_tag_name[0], 10u);
  EXPECT_EQ(msg->timer_tag_name[1], 11u);
  ASSERT_EQ(msg->n_timer_tag_value, 2u);
  EXPECT_EQ(msg->timer_tag_value[1], 21u);

  ASSERT_EQ(msg->n_dictionary, 2u);
  /* dictionary is a flat char buffer of fixed-size entries. */
  EXPECT_STREQ(msg->dictionary, "db");

  pinba__request__free_unpacked(msg, nullptr);
}

TEST(PinbaProtobuf, UnpackNestedRequests) {
  Pinba::Request req = MakeValidRequest();
  Pinba::Request *child = req.add_requests();
  child->set_hostname("child-host");
  child->set_server_name("child-srv");
  child->set_script_name("/child.php");
  child->set_request_count(1);
  child->set_document_size(10);
  child->set_memory_peak(20);
  child->set_request_time(0.01f);
  child->set_ru_utime(0.0f);
  child->set_ru_stime(0.0f);

  std::string buf;
  ASSERT_TRUE(req.SerializeToString(&buf));

  Pinba__Request *msg =
      pinba__request__unpack(nullptr, buf.size(), reinterpret_cast<const uint8_t *>(buf.data()));
  ASSERT_NE(msg, nullptr);

  ASSERT_EQ(msg->n_requests, 1u);
  ASSERT_NE(msg->requests, nullptr);
  EXPECT_STREQ(msg->requests[0]->hostname, "child-host");
  EXPECT_EQ(msg->requests[0]->request_count, 1u);

  pinba__request__free_unpacked(msg, nullptr);
}

TEST(PinbaProtobuf, RejectsNullAndEmpty) {
  const uint8_t dummy = 0;
  EXPECT_EQ(pinba__request__unpack(nullptr, 0, &dummy), nullptr);
  EXPECT_EQ(pinba__request__unpack(nullptr, 4, nullptr), nullptr);
}

TEST(PinbaProtobuf, RejectsGarbageBytes) {
  /* Random bytes that do not form a valid wire message. */
  const uint8_t garbage[] = {0xFF, 0xFF, 0xFF, 0xFF, 0x7F, 0x13, 0x37};
  Pinba__Request *msg = pinba__request__unpack(nullptr, sizeof(garbage), garbage);
  EXPECT_EQ(msg, nullptr);
}

TEST(PinbaProtobuf, RejectsMissingRequiredFields) {
  /* A request lacking required fields must be rejected by the parser. */
  Pinba::Request incomplete;
  incomplete.set_hostname("only-hostname-set");
  std::string buf;
  /* SerializePartial bypasses the required-field check on the writing side so we
   * can produce a wire buffer that the reader (ParseFromArray) must reject. */
  ASSERT_TRUE(incomplete.SerializePartialToString(&buf));

  Pinba__Request *msg =
      pinba__request__unpack(nullptr, buf.size(), reinterpret_cast<const uint8_t *>(buf.data()));
  EXPECT_EQ(msg, nullptr);
}

TEST(PinbaProtobuf, InitResetsFields) {
  Pinba__Request msg;  // constructor calls pinba__request__init
  EXPECT_EQ(msg.hostname[0], '\0');
  EXPECT_EQ(msg.request_count, 0u);
  EXPECT_FALSE(msg.has_status);
  EXPECT_EQ(msg.n_timer_value, 0u);
  EXPECT_EQ(msg.timer_value, nullptr);
  EXPECT_EQ(msg.n_requests, 0u);
  EXPECT_EQ(msg.requests, nullptr);
}

}  // namespace
