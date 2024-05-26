#define RLBOX_USE_EXCEPTIONS
#define RLBOX_ENABLE_DEBUG_ASSERTIONS
#define RLBOX_SINGLE_THREADED_INVOCATIONS
#include "rlbox_wasm2c_sandbox.hpp"

// NOLINTNEXTLINE
#define TestName "rlbox_wasm2c_sandbox shadow"
// NOLINTNEXTLINE
#define TestType rlbox::rlbox_wasm2c_sandbox

#ifndef GLUE_LIB_WASM2C_PATH
#  error "Missing definition for GLUE_LIB_WASM2C_PATH"
#endif

// NOLINTNEXTLINE
#if defined(_WIN32)
#define CreateSandbox(sandbox) sandbox.create_sandbox(L"" GLUE_LIB_WASM2C_PATH)
#define CreateSandboxFallible(sandbox) sandbox.create_sandbox(L"does_not_exist", false /* infallible */)
#else
#define CreateSandbox(sandbox) sandbox.create_sandbox(GLUE_LIB_WASM2C_PATH)
#define CreateSandboxFallible(sandbox) sandbox.create_sandbox("does_not_exist", false /* infallible */)
#endif
// NOLINTNEXTLINE
#include "test_sandbox_glue.inc.cpp"
#include "test_wasm2c_sandbox_wasmtests.cpp"
