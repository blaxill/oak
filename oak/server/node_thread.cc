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

#include "oak/server/node_thread.h"

#include "asylo/util/logging.h"

namespace oak {

NodeThread::~NodeThread() { Stop(); }

void NodeThread::Start(Handle handle) {
  if (thread_.joinable()) {
    LOG(ERROR) << "Attempt to Start() an already-running NodeThread";
    return;
  }
  if (termination_pending_.load()) {
    LOG(ERROR) << "Attempt to Start() an already-terminated NodeThread";
    return;
  }

  LOG(INFO) << "Executing new {" << name_ << "} node thread with handle " << handle;
  thread_ = std::thread(&oak::NodeThread::Run, this, handle);
  LOG(INFO) << "Started {" << name_ << "} node thread";
}

void NodeThread::Stop() {
  termination_pending_ = true;
  if (thread_.joinable()) {
    LOG(INFO) << "Waiting for completion of {" << name_ << "} node thread";
    thread_.join();
    LOG(INFO) << "Completed {" << name_ << "} node thread";
  }
}

}  // namespace oak
