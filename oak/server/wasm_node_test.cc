/*
 * Copyright 2019 The Project Oak Authors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "oak/server/wasm_node.h"

#include <fstream>

#include "gtest/gtest.h"

namespace oak {

namespace {

std::string DataFrom(const std::string& filename) {
  std::ifstream ifs(filename.c_str(), std::ios::in | std::ios::binary);
  EXPECT_TRUE(ifs.is_open()) << "failed to open " << filename;
  std::stringstream ss;
  ss << ifs.rdbuf();
  ifs.close();
  return ss.str();
}

}  // namespace

TEST(WasmNode, MalformedFailure) {
  // No magic.
  ASSERT_EQ(nullptr, WasmNode::Create(nullptr, "test", ""));
  // Wrong magic.
  ASSERT_EQ(nullptr,
            WasmNode::Create(nullptr, "test", std::string("\x00\x61\x73\x6b\x01\x00\x00\x00", 8)));
  // Wrong version.
  ASSERT_EQ(nullptr,
            WasmNode::Create(nullptr, "test", std::string("\x00\x61\x73\x6d\x09\x00\x00\x00", 8)));
  // Right magic+version, no contents.
  ASSERT_EQ(nullptr, WasmNode::Create(nullptr, "test", DataFrom("oak/server/testdata/empty.wasm")));
}

TEST(WasmNode, MinimalSuccess) {
  std::unique_ptr<WasmNode> node =
      WasmNode::Create(nullptr, "test", DataFrom("oak/server/testdata/minimal.wasm"));
  EXPECT_NE(nullptr, node);
}

TEST(WasmNode, MissingExports) {
  ASSERT_EQ(nullptr,
            WasmNode::Create(nullptr, "test", DataFrom("oak/server/testdata/missing.wasm")));
}

TEST(WasmNode, WrongSignature) {
  ASSERT_EQ(nullptr, WasmNode::Create(nullptr, "test", DataFrom("oak/server/testdata/wrong.wasm")));
}

}  // namespace oak
