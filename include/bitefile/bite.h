// SPDX-License-Identifier: MIT
/*
 * Copyright (c) 2026-present IsaacTCB
 * Licensed under the MIT License
 */

#ifndef BITE_IMPL_H
#define BITE_IMPL_H

#if defined(__cplusplus)
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>

/*
 * If you are linking this lib as a dynamic lib,
 * then make BITEFILE_USE_SHARED is defined!
 */
#if defined(_WIN32)
    // Building as dynamic lib on Windows
    #if defined(BITEFILE_BUILD_LIB)
        #define BITE_API __declspec(dllexport)
    // Linking as dynamic lib on Windows
    #elif defined(BITEFILE_USE_SHARED)
        #define BITE_API __declspec(dllimport)
    #endif
#elif defined(__GNUC__)
    // Building as shared object on Unix systems
    #if defined(BITEFILE_BUILD_LIB)
        #define BITE_API __attribute__((visibility("default")))
    #endif
#endif

#if !defined(BITE_API)
#define BITE_API
#endif

/*
 * TYPEDEFS
 */

/**
 * @typedef bite_packed
 * @brief An opaque type used to represent packed archives.
 */
typedef struct bite_packed bite_packed_t;

/**
 * @typedef bite_file
 * @brief An opaque type used to represent file descriptors
 * from bite_packed_t.
 */
typedef struct bite_file bite_file_t;

/**
 * @typedef int64_t
 * @brief Represents the file position.
 */
typedef int64_t bite_offset_t;

/**
 * @typedef uint64_t
 * @brief Represents the size amount.
 */
typedef uint64_t bite_size_t;

/*
 * FUNCTIONS
 */

/**
 * @brief Opens a bite packed archive file.
 *
 * @param filepath Length must be < 255.
 * @return `bite_packed_t*` handle or `NULL` on failure.
 */
BITE_API bite_packed_t* bite_packed_open(const char* filepath);

/**
 * @brief Closes a bite packed archive file.
 *
 * This action will NOT free bite_file_t* handles
 * associated with the bite_packed. Make sure you
 * properly free them before calling this!
 *
 * @param packed The bite packed handle.
 */
BITE_API void bite_packed_close(bite_packed_t* packed);

/**
 * @brief Opens a file handle from the bite packed.
 *
 * Whenever you are done with this handle, you
 * should free it with bite_fclose().
 *
 * @param packed   The bite packed handle.
 * @param filepath Length must be < 255.
 * @return `bite_file_t*` handle or `NULL` on failure.
 */
BITE_API bite_file_t* bite_fopen(const bite_packed_t* packed, const char* filepath);

/**
 * @brief Get the filename from an open file handle.
 *
 * @param file A valid bite_file_t* handle
 * @return The filename string
 */
BITE_API const char* bite_fname(bite_file_t* file);

/**
 * @brief Returns the total file size from an open file handle
 *
 * @param file A valid bite_file_t* handle
 * @return A bite_size_t with the file size.
 */
BITE_API bite_size_t bite_fsize(const bite_file_t* file);

/**
 * @brief Reads no more than `size` bytes into `dst` from the given file handle.
 *
 * @param dst  The destination buffer
 * @param size The amount of bytes to be read.
 * @param file A valid bite_file_t* handle
 * @return The amount of bytes read. Might be less than `size` if EOF was reached.
 */
BITE_API bite_size_t bite_fread(void* dst, bite_size_t size, bite_file_t* file);

/**
 * @brief Returns the current file cursor offset from the given file handle.
 *
 * @param file A valid bite_file_t* handle
 * @return The current file offset.
 */
BITE_API bite_offset_t bite_ftell(const bite_file_t* file);

/**
 * @brief Set the file cursor offset based on an offset and a whence/origin.
 *
 * @param file   A valid bite_file_t* handle
 * @param pos    The offset
 * @param whence Can be `SEEK_SET`, `SEEK_END` or `SEEK_CUR`.
 * @return Returns non-zero on fail.
 */
BITE_API int bite_fseek(bite_file_t* file, bite_offset_t pos, int whence);

/**
 * @brief Duplicates a bite file handle under a new file descriptor.
 *
 * Allows for safer multi-threaded operations without having to manually
 * reopen a new bite packed handle.
 *
 * @param file A valid bite_file_t* handle
 * @return A new bite_file_t* handle.
 */
BITE_API bite_file_t* bite_fdup(const bite_file_t* file);

/**
 * @brief Closes a bite file handle.
 *
 * @param file A valid bite_file_t* handle
 */
BITE_API void bite_fclose(bite_file_t* file);

/**
 * @brief Get error string for logging.
 *
 * @return A pointer to the error string buffer.
 */
BITE_API const char* bite_error_str();

#if defined(__cplusplus)
}
#endif

#endif // BITE_IMPL_H
