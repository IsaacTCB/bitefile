// SPDX-License-Identifier: MIT
//
// Copyright (c) 2026-present IsaacTCB
// Licensed under the MIT License

#ifndef BITE_IMPL_H
#define BITE_IMPL_H

#if defined(__cplusplus)
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>

// If you are linking this lib as a dynamic lib,
// then make BITEFILE_USE_SHARED is defined!

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

#ifndef BITE_API
#define BITE_API
#endif

// Bite handles
typedef struct bite_packed bite_packed_t; // Bite packed archive handle
typedef struct bite_file bite_file_t;     // Virtual file handle

// Bite-packed open/close
BITE_API bite_packed_t* bite_packed_open(const char* filepath);   // Opens a bite-packed file.
BITE_API void           bite_packed_close(bite_packed_t* packed); // Closes a bite-packed file

// File operations
BITE_API bite_file_t* bite_fopen(bite_packed_t* packed, const char* filepath);
BITE_API const char*  bite_fname(bite_file_t* file);
BITE_API size_t       bite_fsize(bite_file_t* file);
BITE_API size_t       bite_fread(void* dst, size_t size, bite_file_t* file);
BITE_API size_t       bite_ftell(bite_file_t* file);
BITE_API int          bite_fseek(bite_file_t* file, long int pos, int whence);
BITE_API void         bite_fclose(bite_file_t* file);

// Error handling
BITE_API const char* bite_error_str();

#if defined(__cplusplus)
}
#endif

#endif // BITE_IMPL_H
