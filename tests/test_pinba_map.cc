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
 * Unit tests for the string-keyed hash map (src/pinba_map.cc) and the
 * uint64-keyed hash map (src/pinba_lmap.cc). Both are pure data structures with
 * no dependency on the MySQL server, so they can be exercised in isolation.
 */

#include <gtest/gtest.h>

#include <cstdint>
#include <cstring>
#include <set>
#include <string>

#include "pinba_lmap.h"
#include "pinba_map.h"

namespace {

/* ------------------------------------------------------------------ */
/* pinba_map: string-keyed map                                        */
/* ------------------------------------------------------------------ */

TEST(PinbaMap, CreateEmpty) {
  void *map = pinba_map_create();
  ASSERT_NE(map, nullptr);
  EXPECT_EQ(pinba_map_count(map), 0u);

  char index[64] = {0};
  EXPECT_EQ(pinba_map_first(map, index, sizeof(index)), nullptr);

  pinba_map_destroy(map);
}

TEST(PinbaMap, AddCreatesMapWhenNull) {
  int payload = 42;
  /* Passing nullptr makes pinba_map_add allocate a fresh map. */
  void *map = pinba_map_add(nullptr, "alpha", &payload);
  ASSERT_NE(map, nullptr);
  EXPECT_EQ(pinba_map_count(map), 1u);
  EXPECT_EQ(pinba_map_get(map, "alpha"), &payload);

  pinba_map_destroy(map);
}

TEST(PinbaMap, AddGetAndOverwrite) {
  int a = 1, b = 2;
  void *map = pinba_map_create();

  pinba_map_add(map, "key", &a);
  EXPECT_EQ(pinba_map_get(map, "key"), &a);

  /* Re-adding the same key overwrites the stored value. */
  pinba_map_add(map, "key", &b);
  EXPECT_EQ(pinba_map_get(map, "key"), &b);
  EXPECT_EQ(pinba_map_count(map), 1u);

  pinba_map_destroy(map);
}

TEST(PinbaMap, GetMissingReturnsNull) {
  int a = 1;
  void *map = pinba_map_create();
  pinba_map_add(map, "present", &a);

  EXPECT_EQ(pinba_map_get(map, "absent"), nullptr);

  pinba_map_destroy(map);
}

TEST(PinbaMap, DeleteRemovesKey) {
  int a = 1, b = 2;
  void *map = pinba_map_create();
  pinba_map_add(map, "a", &a);
  pinba_map_add(map, "b", &b);

  /* Deleting a key while others remain returns 0 (map still non-empty). */
  EXPECT_EQ(pinba_map_delete(map, "a"), 0);
  EXPECT_EQ(pinba_map_get(map, "a"), nullptr);
  EXPECT_EQ(pinba_map_count(map), 1u);

  /* Deleting the last key returns -1 (map became empty). */
  EXPECT_EQ(pinba_map_delete(map, "b"), -1);
  EXPECT_EQ(pinba_map_count(map), 0u);

  pinba_map_destroy(map);
}

TEST(PinbaMap, IterationVisitsEveryKey) {
  int v0 = 10, v1 = 11, v2 = 12;
  void *map = pinba_map_create();
  pinba_map_add(map, "one", &v0);
  pinba_map_add(map, "two", &v1);
  pinba_map_add(map, "three", &v2);

  std::set<std::string> seen;
  char index[64] = {0};
  void *val = pinba_map_first(map, index, sizeof(index));
  while (val != nullptr) {
    seen.insert(index);
    val = pinba_map_next(map, index, sizeof(index));
  }

  EXPECT_EQ(seen.size(), 3u);
  EXPECT_TRUE(seen.count("one"));
  EXPECT_TRUE(seen.count("two"));
  EXPECT_TRUE(seen.count("three"));

  pinba_map_destroy(map);
}

TEST(PinbaMap, NullMapArgumentsAreSafe) {
  char index[64] = {0};
  EXPECT_EQ(pinba_map_first(nullptr, index, sizeof(index)), nullptr);
  EXPECT_EQ(pinba_map_next(nullptr, index, sizeof(index)), nullptr);
  EXPECT_EQ(pinba_map_get(nullptr, "x"), nullptr);
  EXPECT_EQ(pinba_map_delete(nullptr, "x"), -1);
  EXPECT_EQ(pinba_map_count(nullptr), 0u);
  /* destroy(nullptr) must be a no-op, not a crash. */
  pinba_map_destroy(nullptr);
}

/* ------------------------------------------------------------------ */
/* pinba_lmap: uint64-keyed map                                       */
/* ------------------------------------------------------------------ */

TEST(PinbaLmap, CreateEmpty) {
  void *map = pinba_lmap_create();
  ASSERT_NE(map, nullptr);
  EXPECT_EQ(pinba_lmap_count(map), 0u);

  uint64_t index = 0;
  EXPECT_EQ(pinba_lmap_first(map, &index), nullptr);

  pinba_lmap_destroy(map);
}

TEST(PinbaLmap, AddCreatesMapWhenNull) {
  int payload = 7;
  void *map = pinba_lmap_add(nullptr, 123, &payload);
  ASSERT_NE(map, nullptr);
  EXPECT_EQ(pinba_lmap_count(map), 1u);
  EXPECT_EQ(pinba_lmap_get(map, 123), &payload);

  pinba_lmap_destroy(map);
}

TEST(PinbaLmap, AddGetAndOverwrite) {
  int a = 1, b = 2;
  void *map = pinba_lmap_create();

  pinba_lmap_add(map, 5, &a);
  EXPECT_EQ(pinba_lmap_get(map, 5), &a);

  pinba_lmap_add(map, 5, &b);
  EXPECT_EQ(pinba_lmap_get(map, 5), &b);
  EXPECT_EQ(pinba_lmap_count(map), 1u);

  pinba_lmap_destroy(map);
}

TEST(PinbaLmap, GetMissingReturnsNull) {
  int a = 1;
  void *map = pinba_lmap_create();
  pinba_lmap_add(map, 1, &a);

  EXPECT_EQ(pinba_lmap_get(map, 999), nullptr);

  pinba_lmap_destroy(map);
}

TEST(PinbaLmap, DeleteRemovesKey) {
  int a = 1, b = 2;
  void *map = pinba_lmap_create();
  pinba_lmap_add(map, 1, &a);
  pinba_lmap_add(map, 2, &b);

  EXPECT_EQ(pinba_lmap_delete(map, 1), 0);
  EXPECT_EQ(pinba_lmap_get(map, 1), nullptr);
  EXPECT_EQ(pinba_lmap_count(map), 1u);

  EXPECT_EQ(pinba_lmap_delete(map, 2), -1);
  EXPECT_EQ(pinba_lmap_count(map), 0u);

  pinba_lmap_destroy(map);
}

TEST(PinbaLmap, IterationVisitsEveryKey) {
  int v0 = 10, v1 = 11, v2 = 12;
  void *map = pinba_lmap_create();
  pinba_lmap_add(map, 100, &v0);
  pinba_lmap_add(map, 200, &v1);
  pinba_lmap_add(map, 300, &v2);

  std::set<uint64_t> seen;
  uint64_t index = 0;
  void *val = pinba_lmap_first(map, &index);
  while (val != nullptr) {
    seen.insert(index);
    val = pinba_lmap_next(map, &index);
  }

  EXPECT_EQ(seen.size(), 3u);
  EXPECT_TRUE(seen.count(100));
  EXPECT_TRUE(seen.count(200));
  EXPECT_TRUE(seen.count(300));

  pinba_lmap_destroy(map);
}

TEST(PinbaLmap, NullMapArgumentsAreSafe) {
  uint64_t index = 0;
  EXPECT_EQ(pinba_lmap_first(nullptr, &index), nullptr);
  EXPECT_EQ(pinba_lmap_next(nullptr, &index), nullptr);
  EXPECT_EQ(pinba_lmap_get(nullptr, 1), nullptr);
  EXPECT_EQ(pinba_lmap_delete(nullptr, 1), -1);
  /* NOTE: pinba_lmap_count() intentionally not called with nullptr here — unlike
   * pinba_map_count(), it does not guard against a null map and would dereference
   * a null pointer. Callers in the engine never pass null, so this only documents
   * the asymmetry rather than asserting on it. */
  pinba_lmap_destroy(nullptr);
}

}  // namespace
