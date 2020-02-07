//
// Copyright 2019 The Project Oak Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

use abitest_0_frontend::proto::abitest::{ABITestRequest, ABITestResponse};
use assert_matches::assert_matches;
use log::{error, info};
use oak::grpc;

// Constants for Node config names that should match those in the textproto
// config held in ../../client/config.h.
const FRONTEND_CONFIG_NAME: &str = "frontend-config";
const BACKEND_CONFIG_NAME: &str = "backend-config";
const LOG_CONFIG_NAME: &str = "logging-config";
const FRONTEND_ENTRYPOINT_NAME: &str = "frontend_oak_main";

const FRONTEND_MANIFEST: &str = "../module_0/rust/Cargo.toml";
const BACKEND_MANIFEST: &str = "../module_1/rust/Cargo.toml";

const FRONTEND_WASM_NAME: &str = "abitest_0_frontend.wasm";
const BACKEND_WASM_NAME: &str = "abitest_1_backend.wasm";

fn build_wasm() -> std::io::Result<Vec<(String, Vec<u8>)>> {
    let mut frontend = oak_tests::compile_rust_to_wasm(FRONTEND_MANIFEST)?;
    frontend.push("wasm32-unknown-unknown/debug");
    frontend.push(FRONTEND_WASM_NAME);
    let mut backend = oak_tests::compile_rust_to_wasm(BACKEND_MANIFEST)?;
    backend.push("wasm32-unknown-unknown/debug");
    backend.push(BACKEND_WASM_NAME);

    let frontend_wasm = std::fs::read(frontend)?;
    let backend_wasm = std::fs::read(backend)?;

    Ok(vec![
        (FRONTEND_CONFIG_NAME.to_owned(), frontend_wasm),
        (BACKEND_CONFIG_NAME.to_owned(), backend_wasm),
    ])
}

#[test]
fn test_abi() {
    simple_logger::init().unwrap();

    let configuration = oak_tests::test_configuration(
        build_wasm().expect("failed to build wasm modules"),
        LOG_CONFIG_NAME,
        FRONTEND_CONFIG_NAME,
        FRONTEND_ENTRYPOINT_NAME,
    );

    let (runtime, entry_channel) = oak_runtime::Runtime::configure_and_run(configuration)
        .expect("unable to configure runtime with test wasm");

    let result: grpc::Result<ABITestResponse> = oak_tests::grpc_request(
        &entry_channel,
        "/oak.examples.abitest.OakABITestService/RunTests",
        ABITestRequest::new(),
    );
    assert_matches!(result, Ok(_));

    runtime.stop();

    for result in result.unwrap().get_results() {
        info!(
            "[ {} ] {}",
            if result.success { " OK " } else { "FAIL" },
            result.name
        );
        if !result.success {
            error!("Failure details: {}", result.details);
        }
        assert_eq!(true, result.success);
    }
}
