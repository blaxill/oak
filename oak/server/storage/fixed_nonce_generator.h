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

#ifndef OAK_SERVER_STORAGE_FIXED_NONCE_GENERATOR_H_
#define OAK_SERVER_STORAGE_FIXED_NONCE_GENERATOR_H_

#include "asylo/crypto/nonce_generator.h"

namespace oak {

constexpr size_t kAesGcmSivNonceSize = 12;

// Produces fixed nonces using the storage encryption key and datum name to
// allow deterministic encryption of the datum name.
class FixedNonceGenerator : public asylo::NonceGenerator<kAesGcmSivNonceSize> {
 public:
  using AesGcmSivNonce = asylo::UnsafeBytes<kAesGcmSivNonceSize>;
  FixedNonceGenerator() {}

  // Called by asylo::AesGcmSiv::Seal.
  asylo::Status NextNonce(const std::vector<uint8_t>& key_id, AesGcmSivNonce* nonce) override {
    CHECK(nonce != nullptr);
    std::copy(key_id.begin(), key_id.end(), nonce->begin());
    std::copy(datum_name_.begin(), datum_name_.end(), nonce->begin() + key_id.size());

    return asylo::Status::OkStatus();
  }

  // Causes asylo::AesGcmSiv::Seal to encrypt the nonce before use.
  bool uses_key_id() override { return true; }

  // Sets the datum name used to generate the nonce.  Must be called before
  // each invocation of NextNonce or asylo::AesGcmSiv::Seal.
  void set_datum_name(const std::string& datum_name) { datum_name_ = datum_name; }

 private:
  std::string datum_name_;
};

}  // namespace oak

#endif  // OAK_SERVER_STORAGE_FIXED_NONCE_GENERATOR_H_
