#include <cstdarg>
#include <cstdint>
#include <cstdlib>
#include <ostream>
#include <new>

/// Represents an in-progress CRC-64 computation.
struct Digest;

/// Opaque type for C for use in FFI (C-compatible shared library)
struct DigestHandle {
  Digest *_0;
};

extern "C" {

/// Creates a new Digest (C-compatible shared library)
DigestHandle *digest_new();

/// Writes data to the Digest (C-compatible shared library)
///
/// # Safety
///
/// Uses unsafe method calls
void digest_write(DigestHandle *handle, const char *data, uintptr_t len);

/// Calculates the CRC-64 checksum from the Digest (C-compatible shared library)
///
/// # Safety
///
/// Uses unsafe method calls
uint64_t digest_sum64(const DigestHandle *handle);

/// Frees the Digest (C-compatible shared library)
///
/// # Safety
///
/// Uses unsafe method calls
void digest_free(DigestHandle *handle);

}  // extern "C"
