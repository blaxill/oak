// TODO: Plug in memory allocator and depend on https://doc.rust-lang.org/alloc/ to enable use of
// heap.

#![no_std]
#![feature(lang_items)]
#![feature(alloc_prelude)]
#![feature(alloc_error_handler)]

extern crate alloc;

// TODO: Move to separate crate.
mod asylo_alloc;

use alloc::prelude::v1::*;
use core::alloc::Layout;
use core::panic::PanicInfo;

// TODO: Move to separate crate and expose safe wrappers.
#[link(name = "sgx_trts")]
extern "C" {
    // SGX-enabled abort function that causes an undefined instruction (`UD2`) to be executed, which
    // terminates the enclave execution.
    //
    // The C type of this function is `extern "C" void abort(void) __attribute__(__noreturn__);`
    //
    // See https://github.com/intel/linux-sgx/blob/d166ff0c808e2f78d37eebf1ab614d944437eea3/sdk/trts/linux/trts_pic.S#L565.
    fn abort() -> !;
}

#[global_allocator]
static A: asylo_alloc::System = asylo_alloc::System;

// Define what happens in an Out Of Memory (OOM) condition.
#[alloc_error_handler]
fn alloc_error(_layout: Layout) -> ! {
    unsafe { abort() }
}

// See https://doc.rust-lang.org/nomicon/panic-handler.html.
#[panic_handler]
fn panic(_info: &PanicInfo) -> ! {
    unsafe { abort() }
}

// See https://doc.rust-lang.org/1.2.0/book/no-stdlib.html.
#[lang = "eh_personality"]
pub extern "C" fn eh_personality() {}

/// An exported placeholder function to check that linking against C++ is successful.
/// It just adds "42" to the provided value and returns it to the caller.
#[no_mangle]
pub extern "C" fn add_magic_number(x: i32) -> i32 {
    // Allocate a bunch of elements on the heap in order to exercise the allocator.
    let v: Vec<i32> = (0..10).map(|n| n + 40).collect();
    x + v[2]
}
