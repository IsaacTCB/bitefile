// SPDX-License-Identifier: MIT
/*
 * Copyright (c) 2026-present IsaacTCB
 * Licensed under the MIT License
 */

#include <bitefile/bite.h>

#if defined(BITEFILE_LARGE_FILES)
#define _FILE_OFFSET_BITS 64
#endif

#include <assert.h>
#include <limits.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BITE_FILE_MAGIC (1163151682) /* BITE, in ASCII */
#define BITE_FILE_VERSION (1)
#define BITE_PATH_MAX_LENGTH (255)

// Safe file reader wrapper. Calls 'return BITE_ERR_MALFORMED' on fail.
#define BITE_IMPL_READ(dst, file)   do { \
                                        int status = bite__fread(dst, sizeof(*dst), file); \
                                        if (!status) return BITE_ERR_MALFORMED; \
                                    } while (0)

// Error handling stuff. Might not be a good idea to use a shared global for thread safety reasons.
#define BITE_ERROR_MSG_SIZE (256)
static char error_text_buffer[BITE_ERROR_MSG_SIZE];

// Convenience function for printing to the error text buffer.
#define BITE_ERROR_MSG(...) (snprintf(error_text_buffer, BITE_ERROR_MSG_SIZE, __VA_ARGS__))

#if defined(BITEFILE_LARGE_FILES)
typedef int64_t bite__impl_size_t;
typedef int64_t bite__impl_offset_t;
#define BITE_SIZE_MAX (INT64_MAX)
#define BITE_OFFSET_MAX (INT64_MAX)
#define BITE_OFFSET_MIN (INT64_MIN)
#else
typedef int32_t bite__impl_size_t;
typedef int32_t bite__impl_offset_t;
#define BITE_SIZE_MAX (INT32_MAX)
#define BITE_OFFSET_MAX (INT32_MAX)
#define BITE_OFFSET_MIN (INT32_MIN)
#endif

/*
 * Determines what fseek/ftell functions to use on 64-bit
 *
 * todo: move this into a separate file
 * and write a proper function for this
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
// If all of those checks failed, then fallback to standard functions.
#if !defined(ftell_64) || !defined(fseek_64)
#define ftell_64 (ftell)
#define fseek_64 (fseek)
#endif

typedef enum {
    BITE_OK = 0,
    BITE_ERR_GENERIC = 1,  // For any kind of error not in this list

    // Specific errors
    BITE_ERR_NOT_BITE = 2, // Not a Bite file!
    BITE_ERR_INCOMPATIBLE, // Incompatible version
    BITE_ERR_MALFORMED,    // Malformed file
    BITE_ERR_BAD_ALLOC,    // Error on allocation
    BITE_ERR_TOO_LARGE,    // File is too large
} bite__status_e;

typedef struct {
    uint32_t magic; // Should spell BITE in ASCII
    uint16_t version;
    uint16_t reserved_1;
    bite__impl_offset_t file_entry_offset;
    uint32_t file_entry_count;
    bite__impl_offset_t file_data_start_offset;
    uint32_t reserved_2;
} bite__header_t;

typedef enum {
    ENTRY_NONE = (0),
    ENTRY_IS_DIR = (1 << 0),
} bite__entry_flags_e;

typedef struct {
    uint32_t flags; // Represented by bite__entry_flags_e
    union {
        struct {
            bite__impl_offset_t data_offset; // Is 0 for empty files
        };
        struct {
            uint32_t dir_sibling_offset;
            uint32_t dir_children_count; 
        };
    };
    bite__impl_size_t data_size;
    uint32_t reserved;
    size_t   name_pool_offset;
    size_t   name_length;
} bite__entry_t;

typedef struct {
    struct {
        bite__entry_t* entries;
        size_t count;
    } file;
    struct {
        char* ptr;
        size_t size;
        size_t capacity;
    } pool;
} bite__table_t;

// Bite packed archive handle
struct bite_packed {
    FILE* handle;
    bite__header_t header;
    bite__table_t table;
};

// Virtual file handle
struct bite_file {
    bite_packed_t* packed_ref; // Weak ref, packed is responsible for closing itself
    bite__entry_t* entry_ref; // Same as above
    bite__impl_offset_t pos;
};

// ===================
// Public
// ===================

static int bite__fread(void* out_ptr, bite__impl_size_t size, FILE* file);
static bite__status_e bite__header_read(bite__header_t* header, FILE* file);
static bite__status_e bite__entry_read(bite__entry_t* entry, FILE* file);
static bite__status_e bite__table_read(bite__table_t* table, bite__header_t* header, FILE* file);
static void bite__table_close(bite__table_t* table);
static bite__entry_t* bite__packed_find_entry(bite_packed_t* packed, const char* filepath);
static bite_file_t* bite__file_open_entry(bite_packed_t* packed_ref, bite__entry_t* entry_ref);

// Open a bite packed archive using the filepath
bite_packed_t* bite_packed_open(const char* filepath) {
    bite_packed_t* packed = NULL;

    FILE* file = fopen(filepath, "rb");
    if (file == NULL) {
        BITE_ERROR_MSG("Unable to open file at \"%s\"\n", filepath);
        return NULL;
    }

    packed = (bite_packed_t*)malloc(sizeof(*packed));
    if (packed == NULL) {
        BITE_ERROR_MSG("Unable to allocate data for \"%s\" handle.", filepath);
        fclose(file);
        return NULL;
    }

    memset(packed, 0, sizeof(*packed));
    packed->handle = file;

    bite__status_e status = bite__header_read(&packed->header, file);
    if (status != BITE_OK) {
        BITE_ERROR_MSG("Unable to parse header. Err: %d\n", status);
        fclose(file);
        free(packed);
        return NULL;
    }

    status = bite__table_read(&packed->table, &packed->header, file);
    if (status != BITE_OK) {
        BITE_ERROR_MSG("Unable to parse file table. Err: %d\n", status);
        fclose(file);
        free(packed);
        return NULL;
    }

    return packed;
}

// Closes a bite packed archive handle.
void bite_packed_close(bite_packed_t* packed) {
    if (!packed) {
        BITE_ERROR_MSG("bite_packed_close(): Packed handle is NULL!");
        return;
    }

    fclose(packed->handle);
    bite__table_close(&packed->table);
    free(packed);
}

// Finds and opens a virtual file inside of the packed file
bite_file_t* bite_fopen(bite_packed_t* packed, const char* filepath) {
    if (!packed) {
        BITE_ERROR_MSG("bite_fopen() -> %s: Packed handle is NULL! Maybe it wasn't properly initialized?", filepath);
        return NULL;
    }

    bite__entry_t* entry = bite__packed_find_entry(packed, filepath);
    if (!entry) return NULL;

    bite_file_t* file = bite__file_open_entry(packed, entry);
    if (!file) {
        BITE_ERROR_MSG("bite_fopen() -> %s: unable to allocate bite_file_t handle.", filepath);
        return NULL;
    }

    return file;
}

// Returns the name of the virtual file
const char* bite_fname(bite_file_t* file) {
    if (!file) {
        BITE_ERROR_MSG("bite_fname(): file handle is NULL");
        return NULL;
    }
    char* ptr = file->packed_ref->table.pool.ptr;
    ptr += file->entry_ref->name_pool_offset;
    return ptr;
}

// Closes a virtual file.
void bite_fclose(bite_file_t* file) {
    if (!file) {
        BITE_ERROR_MSG("bite_fclose(): file handle is NULL");
        return;
    }
    free(file);
}

// Returns the total file size of the virtual file
bite_size_t bite_fsize(bite_file_t* file) {
    if (!file) {
        BITE_ERROR_MSG("bite_fsize(): file handle is NULL");
        return 0;
    } 

    return (bite_size_t)file->entry_ref->data_size;
}

// Reads size of data into the destination buffer from the virtual file
bite_size_t bite_fread(void* dst, bite_size_t size, bite_file_t* file) {
    if (!file) {
        BITE_ERROR_MSG("bite_fread(): file handle is NULL");
        return 0;
    }

    bite__entry_t* entry = file->entry_ref;
    
    // Clamp cursor to size
    if (file->pos > entry->data_size) {
        file->pos = entry->data_size;
    }

    // Limit reading size
    bite__impl_size_t to_read = (bite__impl_size_t)size;
    if (file->pos >= entry->data_size) {
        to_read = 0;
    } else if (file->pos + to_read >= entry->data_size) {
        to_read = entry->data_size - file->pos;
    }

    if (to_read != 0) {
        FILE* handle = file->packed_ref->handle;
        fseek_64(handle, (bite__impl_offset_t)entry->data_offset + file->pos, SEEK_SET);
        file->pos += to_read;

        int result = bite__fread(dst, to_read, handle);
        if (!result) {
            return 0;
        }
    }

    return to_read;
}

// Returns the virtual file's current cursor position
bite_offset_t bite_ftell(bite_file_t* file) {
    if (!file) {
        BITE_ERROR_MSG("bite_ftell(): file handle is NULL");
        return 0;
    }
    return file->pos;
}

// Seeks into a specific point depending on a whence
int bite_fseek(bite_file_t* file, bite_offset_t offset, int whence) {
    if (!file) {
        BITE_ERROR_MSG("bite_fseek(): file handle is NULL");
        return -1;
    }

    if (offset < BITE_OFFSET_MIN && offset > BITE_OFFSET_MAX) {
        BITE_ERROR_MSG("bite_fseek(): offset is too large. Enabling BITEFILE_LARGE_FILES might solve this.");
        return -1;
    }

    bite__impl_offset_t pos = 0;

    switch (whence) {
        case SEEK_SET:
            pos = (bite__impl_offset_t)offset;
            break;
        case SEEK_CUR:
            pos = file->pos + (bite__impl_offset_t)offset;
            break;
        case SEEK_END:
            pos = (bite__impl_offset_t)(file->entry_ref->data_size)
                + (bite__impl_offset_t)offset;
            break;
        default:
            return -1;
    }

    if (pos < 0 || pos > (bite__impl_offset_t)file->entry_ref->data_size) {
        return -1;
    }

    file->pos = pos;
    return 0;
}

// Returns the a string containing the error info
const char* bite_error_str() {
    return error_text_buffer;
}

// =====================
// Private
// =====================

/*
 * Used by bite__packed_find_entry() for selecting
 * filepath segments. It's supposed to be used with
 * one of its helper functions, like bite__path_view_next()
 * and bite__path_view_string().
 */
struct bite__path_view {
    const char* const src;
    const size_t src_size;
    size_t pos;
    size_t size;
};

// Raw usage of this function should be avoided unless strictly needed;
// Use BITE_IMPL_READ wrapper instead, as it is safer and a whole lot cleaner.
static int bite__fread(void* out_ptr, bite__impl_size_t size, FILE* file) {
    return fread(out_ptr, size, 1, file) == 1;
}

/*
 * Turns out strnlen() isn't standard, so we rewrite it.
 * Returns the length of the string 'str', clamped to 'size'.
 */
static size_t bite__strnlen(const char* str, size_t size) {
    size_t sz;
    for (sz = 0; sz < size; sz++) {
        if (str[sz] == '\0') {
            break;
        }
    }
    return sz;
}

static bite__status_e bite__header_read(bite__header_t* header, FILE* file) {
    BITE_IMPL_READ(&(header->magic), file);
    if (header->magic != BITE_FILE_MAGIC) {
        return BITE_ERR_NOT_BITE;
    }

    BITE_IMPL_READ(&(header->version), file);
    if (header->version != BITE_FILE_VERSION) {
        return BITE_ERR_INCOMPATIBLE;
    }

    BITE_IMPL_READ(&(header->reserved_1), file);

    uint64_t temp;
    BITE_IMPL_READ(&temp, file);
    if (temp > BITE_OFFSET_MAX) {
        return BITE_ERR_TOO_LARGE;
    }
    header->file_entry_offset = (bite__impl_offset_t)temp;

    BITE_IMPL_READ(&(header->file_entry_count), file);

    BITE_IMPL_READ(&temp, file);
    if (temp > BITE_OFFSET_MAX) {
        return BITE_ERR_TOO_LARGE;
    }
    header->file_data_start_offset = (bite__impl_offset_t)temp;

    BITE_IMPL_READ(&(header->reserved_2), file);

    return BITE_OK;
}

static bite__status_e bite__entry_read(bite__entry_t* entry, FILE* file) {
    uint64_t temp;

    BITE_IMPL_READ(&(entry->flags), file);
    // Is this a dir entry?
    if (entry->flags & ENTRY_IS_DIR) {
        BITE_IMPL_READ(&(entry->dir_sibling_offset), file);
        BITE_IMPL_READ(&(entry->dir_children_count), file);
    }
    // This is a file entry?
    else {
        BITE_IMPL_READ(&temp, file);
        if (temp > BITE_OFFSET_MAX) {
            return BITE_ERR_TOO_LARGE;
        }
        entry->data_offset = (bite__impl_offset_t)temp;
    }

    BITE_IMPL_READ(&temp, file);
    if (temp > BITE_SIZE_MAX) {
        return BITE_ERR_TOO_LARGE;
    }
    entry->data_size = (bite__impl_size_t)temp;

    BITE_IMPL_READ(&(entry->reserved), file);

    return BITE_OK;
}

// Allocates data, so this must be freed using bite_table_close()!
static bite__status_e bite__table_read(bite__table_t* table, bite__header_t* header, FILE* file) {
    size_t file_count = header->file_entry_count;

    bite__status_e status;

    table->file.entries = (bite__entry_t*)malloc(sizeof(bite__entry_t) * file_count);
    if (!table->file.entries) return BITE_ERR_BAD_ALLOC;

    table->file.count = file_count;

    table->pool.capacity = 32 * file_count; // Allocate 32 bytes per filepath to avoid possible realloc
    table->pool.ptr = (char*)malloc(table->pool.capacity);
    if (!table->pool.ptr) return BITE_ERR_BAD_ALLOC;
    table->pool.size = 0;

    // Skip to file entry offset
    bite__impl_offset_t old_pos = (bite__impl_offset_t)ftell_64(file);
    if (fseek_64(file, header->file_entry_offset, SEEK_SET) != 0) {
        return BITE_ERR_MALFORMED;
    }

    for (size_t i = 0; i < file_count; i++) {
        bite__entry_t* entry = table->file.entries + i;
        status = bite__entry_read(entry, file);

        /*
         * Stored strings are variable length.
         * First comes length (2 bytes), then afterwards
         * is a continuous ASCII/UTF-8 string (not null-terminated)
         */

        // load name
        entry->name_pool_offset = table->pool.size;
        entry->name_length = 0;

        do {
            uint8_t length;
            int success = bite__fread(&length, sizeof(length), file);
            if (success) entry->name_length = length;
        } while (0);

        if (entry->name_length == 0) {
            bite__table_close(table);
            return BITE_ERR_MALFORMED;
        }

        size_t name_size = entry->name_length + 1;
        if (entry->name_pool_offset + name_size >= table->pool.capacity) {

            // Increase pool capacity if not enough
            do {
                table->pool.capacity *= 2;
            } while (
                entry->name_pool_offset + name_size >= table->pool.capacity
            );

            void* new_ptr = realloc(table->pool.ptr, table->pool.capacity);
            if (new_ptr) {
                table->pool.ptr = (char*)new_ptr;
            } else {
                bite__table_close(table);
                return BITE_ERR_BAD_ALLOC;
            }
            //printf("reallocated %zu bytes for name pool\n", table->pool.capacity);
        }

        bite__fread(&table->pool.ptr[entry->name_pool_offset], (bite__impl_size_t)entry->name_length, file);
        table->pool.ptr[entry->name_pool_offset + entry->name_length] = '\0';
        table->pool.size += name_size;

        if (status != BITE_OK) {
            bite__table_close(table);
            return status;
        }
    }

    fseek_64(file, old_pos, SEEK_SET);
    return BITE_OK;
}

static void bite__table_close(bite__table_t* table) {
    if (table->file.entries) {
        free(table->file.entries);
        table->file.entries = NULL;
    }
    table->file.count = 0;

    if (table->pool.ptr) {
        free(table->pool.ptr);
        table->pool.ptr = NULL;
    }
    table->pool.size = 0;
    table->pool.capacity = 0;
}

// Checks whether or not the char is a path separator.
static inline int bite__path_is_separator(char ch) {
    return ch == '/';
}

/*
 * Given a properly initialized bite__path_view, find the next valid
 * path segment's position and size, then update their values.
 *
 * To initialize a bite__path_view, you must define 'src' and 'src_size'
 * as the pointer to your string and its length (excluding null-terminator)
 * respectively. Also, 'pos' and 'size' must be 0.
 *
 * Returns 1 if a segment was found, 0 if there are no more segments.
 */
int bite__path_view_next(struct bite__path_view* path) {
    // Jump to next separator
    path->pos += path->size;
    path->size = 0; // Reset size, as that is now unknown.

    size_t i = path->pos;

    // Fail if out-of-bounds
    if (i >= path->src_size)
        return 0;

    // Move out of path separator
    for (; i < path->src_size; i++) {
        if (!bite__path_is_separator(path->src[i]))
            break;
    }

    // Fail if out-of-bounds
    if (i >= path->src_size)
        return 0;

    size_t start = i;

    // Find next separator to determine the size
    for (; i < path->src_size; i++) {
        if (bite__path_is_separator(path->src[i]))
            break;
    }

    path->pos  = start;
    path->size = i - start;
    return 1;
}

/*
 * Finds the first entry that matches with the given filepath.
 * Returns null if no entry was found.
 */
static bite__entry_t* bite__packed_find_entry(bite_packed_t* packed, const char* filepath) {
    assert(packed->table.file.entries > 0);

    const size_t path_length = bite__strnlen(filepath, BITE_PATH_MAX_LENGTH);
    if (path_length >= BITE_PATH_MAX_LENGTH) {
        BITE_ERROR_MSG("filepath is too long (strlen(filepath) >= %d)", BITE_PATH_MAX_LENGTH);
        return NULL;
    }

    bite__entry_t* entry = NULL;
    size_t begin = 0;
    size_t end = packed->table.file.count;

    struct bite__path_view segment = {
        .src      = filepath,
        .src_size = path_length,
        .pos      = 0,
        .size     = 0,
    };

    // For every segment for this path...
    while (bite__path_view_next(&segment)) {
        const char*  segment_name   = segment.src + segment.pos;
        const size_t segment_length = segment.size;

        // Scan current dir tree for any name match
        int found = 0;
        for (size_t i = begin; i < end; i++) {
            entry = packed->table.file.entries + i;
            const char* name_ptr = packed->table.pool.ptr;
            name_ptr += entry->name_pool_offset;

            if (entry->name_length == segment_length &&
                strncmp(segment_name, name_ptr, segment_length) == 0) {
                // Names match
                if (entry->flags & ENTRY_IS_DIR) {
                    // This is a directory.
                    // Enter directory subtree
                    begin = i + 1;
                    end   = i + entry->dir_sibling_offset + 1;
                    found = 1;
                    break;
                } else {
                    // This is a file.
                    // Does the path have any more segments?
                    if (bite__path_view_next(&segment)) {
                        // If so, then filepath is invalid.
                        BITE_ERROR_MSG(
                            "%s: treats file as if it were a directory.",
                            filepath);
                        return NULL;
                    }
                    return entry;
                }
            } else {
                // Names aren't the same
                if (entry->flags & ENTRY_IS_DIR) {
                    i += entry->dir_sibling_offset; // Skip to sibling entry.
                }
            }
        }

        // If no match was found in this subtree, then path is invalid.
        if (!found) {
            entry = NULL;
            break;
        }
    }

    if (entry && entry->flags & ENTRY_IS_DIR)
        BITE_ERROR_MSG("%s: is a directory, not a file!", filepath);
    else
        BITE_ERROR_MSG("%s: not found.", filepath);
    
    return NULL;
}

// Prepares a bite_file_t object.
static bite_file_t* bite__file_open_entry(bite_packed_t* packed_ref, bite__entry_t* entry_ref) {
    bite_file_t* file = (bite_file_t*)malloc(sizeof(*file));
    
    // just in case...
    if (!file)
        return NULL;

    memset(file, 0, sizeof(*file));
    file->packed_ref = packed_ref;
    file->entry_ref = entry_ref;
    file->pos = 0;
    return file;
}

