/* TODO: add the option to disable stdio altogether */

#if defined(BITEFILE_LARGE_FILES)
#define _FILE_OFFSET_BITS 64
#if !defined(_WIN32) && !defined(_POSIX_C_SOURCE)
/* enable POSIX fseeko and ftello */
#define _POSIX_C_SOURCE 200112L
#endif
#endif

#if defined(_WIN32)
/* fixes MSVC compiler warning caused by
 * fopen() being considered """unsafe"""" */
#define _CRT_SECURE_NO_WARNINGS
#endif

#include "bite_stdio.h"

#include <stdio.h>

/*
 * Determines what fseek/ftell functions to use on 64-bit
 */
#if defined(BITEFILE_LARGE_FILES)
#if defined(_WIN32)
#define ftell_64 (_ftelli64)
#define fseek_64 (_fseeki64)
#elif defined(__GNUC__)
#define ftell_64 (ftello)
#define fseek_64 (fseeko)
#endif
#endif
/* If all of those checks failed, then fallback to standard functions. */
#if !defined(ftell_64) || !defined(fseek_64)
#define ftell_64 (ftell)
#define fseek_64 (fseek)
#endif

/* =================== *
 *   stdio callbacks   *
 * =================== */

static bite_size_t stdio_read(void *dst, bite_size_t size, void *userdata) {
    return (bite_size_t)fread(dst, 1, size, (FILE *)userdata);
}

static int stdio_seek(void *userdata, bite_offset_t offset, int whence) {
    return fseek_64((FILE *)userdata, offset, whence);
}

static void *stdio_open(const char *filepath) {
    return (void *)fopen(filepath, "rb");
}

static int stdio_close(void *userdata) {
    return fclose((FILE *)userdata);
}

static bite_offset_t stdio_tell(void *userdata) {
    return (bite_offset_t)ftell_64((FILE *)userdata);
}

int bite__stdio_callbacks(bite_callbacks_t *callbacks) {
    if (!callbacks) {
        return 0;
    }

    *callbacks = (bite_callbacks_t){
        .read = &stdio_read,
        .seek = &stdio_seek,
        .open = &stdio_open,
        .close = &stdio_close,
        .tell = &stdio_tell,
    };

    return 1;
}
