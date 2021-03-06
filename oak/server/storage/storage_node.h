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

#ifndef OAK_SERVER_STORAGE_NODE_H_
#define OAK_SERVER_STORAGE_NODE_H_

#include <string>
#include <thread>

#include "oak/server/node_thread.h"
#include "oak/server/storage/storage_processor.h"

namespace oak {

class StorageNode final : public NodeThread {
 public:
  StorageNode(const std::string& name, const std::string& storage_address);

 private:
  void Run(Handle handle) override;

  StorageProcessor storage_processor_;
};

}  // namespace oak

#endif  // OAK_SERVER_STORAGE_NODE_H_
