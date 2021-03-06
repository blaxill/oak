/*
 * Copyright 2018 The Project Oak Authors
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

#include "asylo_oak_manager.h"

#include "absl/memory/memory.h"
#include "asylo/identity/descriptions.h"
#include "asylo/identity/enclave_assertion_authority_config.pb.h"
#include "asylo/util/logging.h"
#include "asylo/util/statusor.h"

namespace oak {

AsyloOakManager::AsyloOakManager(absl::string_view enclave_path)
    : Service(), enclave_path_(enclave_path), application_id_(0) {
  InitializeEnclaveManager();
}

grpc::Status AsyloOakManager::CreateApplication(grpc::ServerContext* context,
                                                const oak::CreateApplicationRequest* request,
                                                oak::CreateApplicationResponse* response) {
  std::string application_id = NewApplicationId();
  LOG(INFO) << "Creating application " << application_id;

  grpc::Status status = CreateEnclave(application_id, request->application_configuration());
  if (!status.ok()) {
    return status;
  }
  asylo::StatusOr<oak::InitializeOutput> result = GetEnclaveOutput(application_id);
  if (!result.ok()) {
    return result.status().ToOtherStatus<grpc::Status>();
  }
  oak::InitializeOutput out = result.ValueOrDie();
  response->set_application_id(application_id);
  response->set_grpc_port(out.grpc_port());
  return grpc::Status::OK;
}

grpc::Status AsyloOakManager::TerminateApplication(grpc::ServerContext* context,
                                                   const oak::TerminateApplicationRequest* request,
                                                   oak::TerminateApplicationResponse* response) {
  LOG(INFO) << "Terminating application with ID " << request->application_id();

  DestroyEnclave(request->application_id());
  return grpc::Status::OK;
}

void AsyloOakManager::InitializeEnclaveManager() {
  LOG(INFO) << "Initializing enclave manager";
  asylo::EnclaveManager::Configure(asylo::EnclaveManagerOptions());
  auto manager_result = asylo::EnclaveManager::Instance();
  if (!manager_result.ok()) {
    LOG(QFATAL) << "Could not initialize enclave manager: " << manager_result.status();
  }
  enclave_manager_ = manager_result.ValueOrDie();
  LOG(INFO) << "Enclave manager initialized";
  LOG(INFO) << "Loading enclave code from " << enclave_path_;
  enclave_loader_ = absl::make_unique<asylo::SimLoader>(enclave_path_,
                                                        /*debug=*/true);
}

grpc::Status AsyloOakManager::CreateEnclave(
    const std::string& application_id,
    const oak::ApplicationConfiguration& application_configuration) {
  LOG(INFO) << "Creating enclave";
  asylo::EnclaveConfig config;
  // Explicitly initialize the null assertion authority in the enclave.
  asylo::EnclaveAssertionAuthorityConfig* authority_config =
      config.add_enclave_assertion_authority_configs();
  asylo::SetNullAssertionDescription(authority_config->mutable_description());
  oak::InitializeInput* initialize_input = config.MutableExtension(oak::initialize_input);
  initialize_input->set_application_id(application_id);
  *initialize_input->mutable_application_configuration() = application_configuration;
  asylo::Status status = enclave_manager_->LoadEnclave(application_id, *enclave_loader_, config);
  if (!status.ok()) {
    LOG(ERROR) << "Could not load enclave " << enclave_path_ << ": " << status;
    return status.ToOtherStatus<grpc::Status>();
  }
  LOG(INFO) << "Enclave created";
  return grpc::Status::OK;
}

asylo::StatusOr<oak::InitializeOutput> AsyloOakManager::GetEnclaveOutput(
    const std::string& node_id) {
  LOG(INFO) << "Initializing enclave " << node_id;
  asylo::EnclaveClient* client = enclave_manager_->GetClient(node_id);
  asylo::EnclaveInput input;
  asylo::EnclaveOutput output;
  asylo::Status status = client->EnterAndRun(input, &output);
  if (!status.ok()) {
    LOG(ERROR) << "EnterAndRun failed: " << status;
    return status;
  }
  LOG(INFO) << "Enclave initialized";
  return output.GetExtension(oak::initialize_output);
}

std::string AsyloOakManager::NewApplicationId() {
  // TODO: Generate UUID.
  std::stringstream id_str;
  id_str << application_id_;
  application_id_ += 1;
  return id_str.str();
}

void AsyloOakManager::DestroyEnclave(const std::string& node_id) {
  LOG(INFO) << "Destroying enclave " << node_id;
  asylo::EnclaveClient* client = enclave_manager_->GetClient(node_id);
  asylo::EnclaveFinal final_input;
  asylo::Status status = enclave_manager_->DestroyEnclave(client, final_input);
  if (!status.ok()) {
    LOG(QFATAL) << "Destroy " << enclave_path_ << " failed: " << status;
  }
  LOG(INFO) << "Enclave destroyed";
}

}  // namespace oak
