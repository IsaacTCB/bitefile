#ifndef BITE_IMPL_H
#define BITE_IMPL_H

#include <stddef.h>
#include <stdint.h>

typedef enum {
    BITE_OK = 0,
    BITE_ERR_INVALID,      // Not a valid bite file!
    BITE_ERR_INCOMPATIBLE, // Incompatible version
    BITE_ERR_MALFORMED,    // Malformed file format
} bite_status_e;

typedef struct bite_packed bite_packed_t;
typedef struct bite_file bite_file_t;

// Bite-packed open/close
bite_packed_t* bite_packed_open(const char* filepath);   // Opens a bite-packed file.
void           bite_packed_close(bite_packed_t* packed); // Closes a bite-packed file

// File operations
bite_file_t* bite_fopen(bite_packed_t* packed, const char* filepath);
size_t       bite_fsize(bite_file_t* file);
size_t       bite_fread(void* dst, size_t size, bite_file_t* file);
size_t       bite_ftell(bite_file_t* file);
int          bite_fseek(bite_file_t* file, long int pos, int whence);
void         bite_fclose(bite_file_t* file);

#endif // BITE_IMPL_H
