# Programming Oak

This document walks through the basics of programming in Oak.

- [Writing an Oak Node](#writing-an-oak-node)
  - [Per-Node Boilerplate](#per-node-boilerplate)
  - [Generated gRPC service code](#generated-grpc-service-code)
- [Using an Oak Application from a client](#using-an-oak-application-from-a-client)
  - [Connecting to the Oak Manager](#connecting-to-the-oak-manager)
  - [Starting the Oak Application](#starting-the-oak-application)
- [gRPC Request Processing Path](#grpc-request-processing-path)
- [Node Termination](#node-termination)
- [Channels and Handles](#channels-and-handles)
- [Persistent Storage](#persistent-storage)
- [Testing](#testing)
  - [Testing Multi-Node Applications](#testing-multi-node-applications)

## Writing an Oak Node

Oak Applications are built from a collection of inter-connected Oak **Nodes**,
so the first step is to understand how to build a single Oak Node.

Writing software for the Oak system involves a certain amount of boilerplate, so
this section will cover the hows &ndash; and whys &ndash; of this, using code
taken from the various [examples](../examples/). If you're impatient, you could
start by copying the `hello_world` example in particular, and just try modifying
things from there.

### Per-Node Boilerplate

An Oak Node needs to provide a single
[`oak_main()` entrypoint](abi.md#exported-function), which is the point at which
Node execution begins. However, Node authors don't _have_ to implement this
function themselves; for a Node which is triggered by external gRPC requests
(the normal "front door" for an Oak application), there are helper functions in
the Oak SDK that make this easier.

To use these helpers, an Oak Node should implement a `struct` of some kind to
represent the Node itself, and then add the `derive(OakExports)` attribute (from
the [`oak_derive`](https://project-oak.github.io/oak/sdk/oak_derive/index.html)
crate):

<!-- prettier-ignore-start -->
[embedmd]:# (../examples/rustfmt/module/rust/src/lib.rs Rust /.*derive\(OakExports\).*]/ /;$/)
```Rust
#[derive(OakExports)]
struct Node;
```
<!-- prettier-ignore-end -->

If the Node needs per-instance state, this Node `struct` is an ideal place to
store it. For example, the running average example has a `Node` `struct` with a
running sum and count of samples:

<!-- prettier-ignore-start -->
[embedmd]:# (../examples/running_average/module/rust/src/lib.rs Rust /.*derive\(Default, OakExports\).*]/ /^}/)
```Rust
#[derive(Default, OakExports)]
struct Node {
    sum: u64,
    count: u64,
}
```
<!-- prettier-ignore-end -->

Under the covers the
[`derive(OakExports)`](https://project-oak.github.io/oak/sdk/oak_derive/derive.OakExports.html)
macro implements `oak_main` for you, with the following default behaviour:

- Create an instance of your Node `struct` (using the `new()` method from the
  [`OakNode`](https://project-oak.github.io/oak/sdk/oak/grpc/trait.OakNode.html)
  trait described below).
- Look for channel handles corresponding to the
  [default port names](abi.md#pre-defined-channels) for gRPC input and output
  channel halves.
- Pass the Node `struct` and the channel handles to the
  [`event_loop()`](https://project-oak.github.io/oak/sdk/oak/grpc/fn.event_loop.html)
  function from the [`oak::grpc` module](sdk.md#oakgrpc-module).

To make this work, the Node `struct` must implement the
[`oak::grpc::OakNode`](https://project-oak.github.io/oak/sdk/oak/grpc/trait.OakNode.html)
trait. This has two methods:

- A
  [`new()`](https://project-oak.github.io/oak/sdk/oak/grpc/trait.OakNode.html#tymethod.new)
  method to create an instance of the Node, and perform any one-off
  initialization. (A common initialization action is to enable logging for the
  Node, via the [`oak_log` crate](sdk.md#oak_log-crate).)
- An
  [`invoke()`](https://project-oak.github.io/oak/sdk/oak/grpc/trait.OakNode.html#tymethod.invoke)
  method that is called for each newly-arriving gRPC request from the outside
  world.

<!-- prettier-ignore-start -->
[embedmd]:# (../examples/rustfmt/module/rust/src/lib.rs Rust /impl oak::grpc::OakNode/ /^}/)
```Rust
impl oak::grpc::OakNode for Node {
    fn new() -> Self {
        oak_log::init_default();
        Node
    }
    fn invoke(&mut self, method: &str, req: &[u8], writer: grpc::ChannelResponseWriter) {
        dispatch(self, method, req, writer)
    }
}
```
<!-- prettier-ignore-end -->

The `invoke` method can be written manually, but it is usually easier to rely on
code that is auto-generated from a gRPC service definition, described in the
next section.

### Generated gRPC service code

The Oak SDK includes a `proto_rust_grpc` tool (forked from
https://github.com/stepancheg/rust-protobuf) which takes a
[gRPC service definition](https://grpc.io/docs/guides/concepts/) and
autogenerates Rust code for the corresponding Oak Node that implements that
service.

Adding a `build.rs` file to the Node that invokes this tool results in a
generated file appearing in `src/proto/<service>_grpc.rs`.

<!-- prettier-ignore-start -->
[embedmd]:# (../examples/hello_world/module/rust/build.rs Rust /fn main/ /^}/)
```Rust
fn main() {
    protoc_rust_grpc::run(protoc_rust_grpc::Args {
        out_dir: "src/proto",
        input: &["../../proto/hello_world.proto"],
        includes: &["../../proto", "../../third_party"],
        rust_protobuf: true, // also generate protobuf messages, not just services
        ..Default::default()
    })
    .expect("protoc-rust-grpc");
}
```
<!-- prettier-ignore-end -->

The autogenerated code includes two key chunks.

The first is a trait definition that includes a method for each of the methods
in the gRPC service, taking the relevant (auto-generated) request and response
types. The Oak Node implements the gRPC service by implementing this trait.

<!-- prettier-ignore-start -->
[embedmd]:# (../examples/rustfmt/module/rust/src/proto/rustfmt_grpc.rs Rust /pub trait FormatServiceNode/ /^}/)
```Rust
pub trait FormatServiceNode {
    fn format(&mut self, req: super::rustfmt::FormatRequest) -> grpc::Result<super::rustfmt::FormatResponse>;
}
```
<!-- prettier-ignore-end -->

Secondly, the autogenerated code includes a `dispatch()` method which maps a
request (as a method name and encoded request) to an invocation of the relevant
method on the service trait. This `dispatch()` method can then form the entire
implementation of the `OakNode::invoke()` method described in the previous
section.

Taken altogether, this covers all of the boilerplate needed to have a Node act
as a gRPC server:

- The main `oak_main` entrypoint is auto-generated, and invokes
  `oak::grpc::event_loop` with a new Node `struct`.
- This Node `struct` implements `oak::grpc::OakNode` so the `event_loop()`
  method can call back into the Node's `invoke()` method.
- The `invoke()` method typically just forwards on to the `dispatch()` method
  from the per-gRPC-service auto-generated code.
- The auto-generated `dispatch()` method invokes the relevant per-service method
  of the Node, available because the Node `struct` implements the gRPC service
  trait.

## Using an Oak Application from a client

A client that is outside of the Oak ecosystem can use an Oak Application by
interacting with it as a gRPC service. However, this does involve a few
additional complications beyond the normal gRPC boilerplate, which are described
in this section.

### Connecting to the Oak Manager

Oak client code (in C++) first needs to connect to the
[Oak Manager](concepts.md#oak-manager), as a normal gRPC client:

<!-- prettier-ignore-start -->
[embedmd]:# (../examples/abitest/client/abitest.cc C++ /.*Connect to the Oak Manager/ /NewStub.*;/)
```C++
  // Connect to the Oak Manager.
  auto channel =
      grpc::CreateChannel(absl::GetFlag(FLAGS_manager_address), grpc::InsecureChannelCredentials());
  auto manager_stub = oak::Manager::NewStub(channel, grpc::StubOptions());
```
<!-- prettier-ignore-end -->

This sets up a client for the `oak.Manager` service; this service allows the
creation and termination of Oak Applications:

<!-- prettier-ignore-start -->
[embedmd]:# (../oak/proto/manager.proto protobuf /^service Manager/ /^}/)
```protobuf
service Manager {
  // Request the creation of a new Oak Application with the specified configuration.
  //
  // After the Oak Node is created, the client should connect to the returned
  // endpoint via gRPC and perform a direct attestation against the Node itself,
  // to verify that its configuration corresponds to what the client expects.
  rpc CreateApplication(CreateApplicationRequest) returns (CreateApplicationResponse);

  // Request that an Oak Application terminate.
  rpc TerminateApplication(TerminateApplicationRequest) returns (TerminateApplicationResponse);
}
```
<!-- prettier-ignore-end -->

Alternatively, the manager client can also be wrapped up in a
[helper library](../oak/client/manager_client.h), which will make application
configuration (described in the next section) easier:

<!-- prettier-ignore-start -->
[embedmd]:# (../examples/rustfmt/client/rustfmt.cc C++ /.*Connect to the Oak Manager/ /FLAGS_manager_address.*;/)
```C++
  // Connect to the Oak Manager.
  std::unique_ptr<oak::ManagerClient> manager_client =
      absl::make_unique<oak::ManagerClient>(grpc::CreateChannel(
          absl::GetFlag(FLAGS_manager_address), grpc::InsecureChannelCredentials()));
```
<!-- prettier-ignore-end -->

### Starting the Oak Application

To use the `CreateApplication` method, the client needs to specify the
_application configuration_; this describes the Nodes that make up the
application, and their connectivity. However, for the simple case of a one-Node
application the `oak::ManagerClient` helper takes care of this; the client just
need to provide the code that is going to be run in the Node, as a Wasm module:

<!-- prettier-ignore-start -->
[embedmd]:# (../examples/rustfmt/client/rustfmt.cc C++ /.*FLAGS_module/ /}/)
```C++
  std::string module_bytes = oak::utils::read_file(absl::GetFlag(FLAGS_module));
  std::unique_ptr<oak::CreateApplicationResponse> create_application_response =
      manager_client->CreateApplication(module_bytes);
  if (create_application_response == nullptr) {
    LOG(QFATAL) << "Failed to create application";
  }
```
<!-- prettier-ignore-end -->

There's also a variant of `CreateApplication` that allows the client code to
specify a storage provider to connect to, and to control whether logging is
enabled:

<!-- prettier-ignore-start -->
[embedmd]:# (../examples/hello_world/client/hello_world.cc C++ /.*FLAGS_module/ /}/)
```C++
  std::string module_bytes = oak::utils::read_file(absl::GetFlag(FLAGS_module));
  std::unique_ptr<oak::CreateApplicationResponse> create_application_response =
      manager_client->CreateApplication(module_bytes, /* logging= */ true,
                                        absl::GetFlag(FLAGS_storage_address));
  if (create_application_response == nullptr) {
    LOG(QFATAL) << "Failed to create application";
  }
```
<!-- prettier-ignore-end -->

The Oak Manager will launch an [Oak Runtime](concepts.md#oak-vm) inside the
enclave, and this Runtime will check the provided Wasm module(s) and application
configuration. Assuming everything is correct (e.g. the Nodes all have an
`oak_main` entrypoint and only expect to find the Oak
[host functions](abi.md#host-functions)), the Oak Runtime opens up a port of its
own and the `CreateApplication` returns this to the client.

The client can now connect to this separate gRPC service, and send
(Node-specific) gRPC requests to it, over a channel that has end-to-end
encryption into the enclave:

<!-- prettier-ignore-start -->
[embedmd]:# (../examples/rustfmt/client/rustfmt.cc C++ /.*InitializeAssertionAuthorities/ /CreateChannel.*/)
```C++
  oak::ApplicationClient::InitializeAssertionAuthorities();

  // Connect to the newly created Oak Application.
  auto stub = FormatService::NewStub(oak::ApplicationClient::CreateChannel(addr.str()));
```
<!-- prettier-ignore-end -->

## gRPC Request Processing Path

At this point, the client code can interact with the Node code via gRPC. A
typical sequence for this (using the various helpers described in previous
sections) would be as follows:

- The Node code (Wasm code running in a Wasm interpreter, running in an enclave)
  is blocked inside a call to the `oak.wait_on_channels()` host function from
  the `oak::grpc::event_loop` helper function.
  - `event_loop()` was invoked directly from the auto-generated `oak_main()`
    exported function.
- The client C++ code builds a gRPC request and sends it to the Oak Runtime.
  - This connection is end-to-end encrypted using the Asylo key exchange
    mechanism.
- The Oak Runtime receives the message and encapsulates it in a `GrpcRequest`
  wrapper message.
- The Oak Runtime serializes the `GrpcRequest` and writes it to the gRPC-in
  channel for the node.
- This unblocks the Node code, and `oak::grpc::event_loop` reads and
  deserializes the incoming gRPC request. It then calls the Node's `invoke()`
  method with the method name and (serialized) gRPC request.
- The Node's `invoke()` method passes this straight on to the auto-generated
  gRPC service code in `proto::service_name_grpc::dispatch()`.
- The auto-generated `dispatch()` code invokes the relevant method on the
  `Node`.
- The (user-written) code in this method does its work, and returns a response.
- The auto-generated `dispatch()` code encapsulates the response into a
  `GrpcResponse` wrapper message, and serializes into the gRPC output channel.
- The Oak Runtime reads this message from the channel, deserializes it and sends
  the inner response back to the client.
- The client C++ code receives the response.

<!-- From (Google-internal): http://go/sequencediagram/view/5741464478810112 -->
<img src="images/SDKFlow.png" width="850">

## Node Termination

The client can also politely request that the Oak Application terminate, using
the `TerminateApplication` method on the (outer) gRPC service. The Oak Manager
passes this on to the Oak Runtime, which will then notify the running Nodes that
termination has been requested (by returning `ERR_TERMINATED` on any current or
future `oak.wait_on_channels()` invocations).

## Channels and Handles

TODO: explore the primitives available here

## Persistent Storage

TODO: describe use of storage

## Testing

> "Beware of bugs in the above code; I have only proved it correct, not tried
> it." - [Donald Knuth](https://www-cs-faculty.stanford.edu/~knuth/faq.html)

Regardless of how the code for an Oak Application is produced, it's always a
good idea to write tests for any software. For an Oak Node that
[implements a gRPC service](#generated-grpc-service-code) purely internally,
it's straightforward to test the methods of the service just by calling them:

<!-- prettier-ignore-start -->
[embedmd]:# (../examples/hello_world/module/rust/src/tests.rs Rust /^.*Test invoking a Node service method directly.*/ /^}$/)
```Rust
// Test invoking a Node service method directly.
#[test]
fn test_direct_hello_request() {
    let mut node = crate::Node::new();
    let mut req = HelloRequest::new();
    req.set_greeting("world".to_string());
    let result = node.say_hello(req);
    assert_matches!(result, Ok(_));
    assert_eq!("HELLO world!", result.unwrap().reply);
}
```
<!-- prettier-ignore-end -->

The [oak_tests](https://project-oak.github.io/oak/sdk/oak_tests/index.html)
crate also allows the gRPC service methods to be tested via the
[Oak SDK](sdk.md) framework:

<!-- prettier-ignore-start -->
[embedmd]:# (../examples/hello_world/module/rust/src/tests.rs Rust /^.*Test invoking a Node service method via the Oak.*/ /^}$/)
```Rust
// Test invoking a Node service method via the Oak entrypoints.
#[test]
#[serial(node_test)]
fn test_hello_request() {
    oak_tests::grpc_channel_setup_default();
    oak_tests::start_node(oak_tests::DEFAULT_NODE_NAME);

    let mut req = HelloRequest::new();
    req.set_greeting("world".to_string());
    let result: grpc::Result<HelloResponse> =
        oak_tests::inject_grpc_request("/oak.examples.hello_world.HelloWorld/SayHello", req);
    assert_matches!(result, Ok(_));
    assert_eq!("HELLO world!", result.unwrap().reply);

    assert_eq!(OakStatus::ERR_TERMINATED, oak_tests::stop());
}
```
<!-- prettier-ignore-end -->

This has a little bit more boilerplate than testing a method directly:

- The inbound gRPC channel that requests are delivered over has to be explicitly
  set up (`oak_tests::grpc_channel_setup_default`)
- A separate thread running the Node's `oak_main` entrypoint is started
  (`oak_tests::start_node`).
- The injection of the gRPC request has to specify the method name (in
  `oak_tests::inject_grpc_request`).
- The per-Node thread needs to be stopped at the end of the test
  (`oak_tests::stop`).
- Because the Node's `oak_main` entrypoint should only be running in a single
  thread, we use the [serial_test](https://crates.io/crates/serial_test) crate
  to mark that the test should be serialized with any other tests that involve
  the node (`#[serial(node_test)]`).

However, this extra complication does allow the Node to be tested in a way that
is closer to real execution, and (more importantly) allows testing of a Node
that makes use of the functionality of the Oak TCB.

### Testing Multi-Node Applications

It's also possible to test an Oak Application that's built from multiple Nodes,
using the `oak_tests::start()` entrypoint. The first argument to this entrypoint
is an `ApplicationConfiguration` protobuf message, the same as is needed for
normally [starting the Oak Application](#starting-the-oak-application) (except
now in Rust rather than C++).

However, the fields in the application configuration that hold the Wasm bytecode
for the various Nodes are ignored; instead, the second argument to
`oak_tests::start()` is a map from the relevant `WasmContents.name` to a
`fn() -> i32` function pointer (aliased as `oak_tests::NodeMain`) for the main
entrypoint for each Node.

<!-- prettier-ignore-start -->
[embedmd]:# (../examples/abitest/tests/src/tests.rs Rust /^#\[test\]/ /oak_tests::start().*/)
```Rust
#[test]
#[serial(node_test)]
fn test_abi() {
    // Initialize the test logger first, so logging gets redirected to simple_logger.
    // (A subsequent attempt to use the oak_log crate will fail.)
    oak_tests::init_logging();
    let mut entrypoints = HashMap::new();
    let fe: oak_tests::NodeMain = abitest_frontend::main;
    let be: oak_tests::NodeMain = abitest_backend::main;
    entrypoints.insert("frontend-code".to_string(), fe);
    entrypoints.insert("backend-code".to_string(), be);
    oak_tests::start(test_config(), entrypoints);
```
<!-- prettier-ignore-end -->

Because there are multiple Nodes linked into the whole Application under test,
this means that this main entrypoint for the node can't be the (global)
`extern "C" fn oak_main` function [described previously](#per-node-boilerplate).
Instead, we need a little bit of boilerplate in the source code for each of the
Nodes to allow testability:

<!-- prettier-ignore-start -->
[embedmd]:# (../examples/abitest/module/rust/src/lib.rs Rust /^#.*wasm32.*/ /pub fn main().*/)
```Rust
#[cfg(target_arch = "wasm32")]
#[no_mangle]
pub extern "C" fn oak_main(handle: u64) -> i32 {
    match std::panic::catch_unwind(|| main(handle)) {
        Ok(rc) => rc,
        Err(_) => OakStatus::ERR_INTERNAL.value(),
    }
}
pub fn main(_handle: u64) -> i32 {
```
<!-- prettier-ignore-end -->

Breaking this down in detail:

- The `extern "C" fn oak_main` function is only defined if the build target is
  `wasm32`; this avoids there being duplicate copies of this function at link
  time.
- The [`catch_unwind`](https://doc.rust-lang.org/std/panic/fn.catch_unwind.html)
  protection belongs in this Wasm-specific function, as it's particularly needed
  to prevent panics crossing the Wasm FFI boundary. Also, it's useful when
  testing for panics to be fully propagated.
- The core functionality of the node is implemented in a `main()` function, but
  as this function is _not_ `extern "C"` it's namespaced to the Node crate. This
  function is also `pub` so that it's available to the overall test.

The overall test then lives in a crate of its own, which imports the individual
crates for the different Oak Nodes, and uses `oak_tests::start` to assemble
different threads running the per-Node entrypoints.
