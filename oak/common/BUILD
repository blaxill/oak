#
# Copyright 2018 The Project Oak Authors
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

load("@rules_cc//cc:defs.bzl", "cc_library", "cc_test")

package(
    licenses = ["notice"],
)

# Mark tests that can be run on the host system (i.e. with a normal
# compiler, not the Asylo toolchain) with 'host' tag.
test_suite(
    name = "host_tests",
    tags = ["host"],
)

cc_library(
    name = "handles",
    srcs = ["handles.cc"],
    hdrs = ["handles.h"],
    visibility = ["//visibility:public"],
    deps = [
        "@com_google_absl//absl/base",
    ],
)

cc_library(
    name = "app_config",
    srcs = ["app_config.cc"],
    hdrs = ["app_config.h"],
    visibility = ["//visibility:public"],
    deps = [
        "//oak/proto:manager_cc_proto",
        "@com_google_absl//absl/memory",
        "@com_google_asylo//asylo/util:logging",
    ],
)

cc_test(
    name = "app_config_test",
    srcs = ["app_config_test.cc"],
    data = [":testdata"],
    tags = ["host"],
    deps = [
        ":app_config",
        "@com_google_absl//absl/memory",
        "@gtest//:gtest_main",
    ],
)

filegroup(
    name = "testdata",
    srcs = glob([
        "testdata/*.textproto",
    ]),
)

cc_library(
    name = "policy",
    srcs = ["policy.cc"],
    hdrs = ["policy.h"],
    visibility = ["//visibility:public"],
    deps = [
        "//oak/proto:policy_cc_proto",
        "@com_google_absl//absl/strings",
    ],
)

cc_library(
    name = "nonce_generator",
    srcs = [],
    hdrs = ["nonce_generator.h"],
    visibility = ["//visibility:public"],
    deps = [
        "@com_google_absl//absl/base",
        "@com_google_absl//absl/strings",
    ],
)
