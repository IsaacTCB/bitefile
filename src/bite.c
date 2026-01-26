#include <bitefile/bite.h>

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BITE_FILE_MAGIC (1163151682) /* BITE, in ASCII */
#define BITE_FILE_VERSION (1)

// Safe file reader wrapper. Calls 'return BITE_ERR_MALFORMED' on fail.
#define BITE_IMPL_READ(dst, file)   do { \
                                        int status = bite__fread(dst, sizeof(*dst), file); \
                                        if (!status) return BITE_ERR_MALFORMED; \
                                    } while (0)

#define BITE_ERROR_MSG_SIZE (256)
static char error_text_buffer[BITE_ERROR_MSG_SIZE];
#define BITE_ERROR_MSG(...) (snprintf(error_text_buffer, BITE_ERROR_MSG_SIZE, __VA_ARGS__))

typedef enum {
    BITE_OK = 0,
    BITE_ERR_INVALID,      // Not a valid bite file!
    BITE_ERR_INCOMPATIBLE, // Incompatible version
    BITE_ERR_MALFORMED,    // Malformed file format
} bite__status_e;

typedef struct {
    uint32_t magic; // BITE, in ASCII
    uint16_t version;
    uint16_t reserved_1;
    uint64_t file_entry_offset;
    uint32_t file_entry_count;
    uint64_t file_data_start_offset;
    uint32_t reserved_2;
} bite__header_t;

typedef struct {
    uint32_t flags;
    union {
        struct {
            uint64_t data_offset; // Is 0 for empty files
        };
        struct {
            uint32_t dir_sibling_offset;
            uint32_t dir_children_count; 
        };
    };
    uint64_t data_size;
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
    size_t pos;
};

struct bite__string_view {
    const char* ptr;
    size_t size;
};

// Raw usage of this function should be avoided unless strictly needed;
// Use BITE_IMPL_READ wrapper instead, as it is safer and a whole lot cleaner.
static int bite__fread(void* out_ptr, size_t size, FILE* file) {
    return fread(out_ptr, size, 1, file) == 1;
}

static bite__status_e bite__header_read(bite__header_t* header, FILE* file) {
    BITE_IMPL_READ(&(header->magic), file);
    if (header->magic != BITE_FILE_MAGIC) {
        // printf("%d != %d", header->magic, BITE_FILE_MAGIC);
        return BITE_ERR_INVALID;
    }

    BITE_IMPL_READ(&(header->version), file);
    if (header->version != BITE_FILE_VERSION) {
        return BITE_ERR_INCOMPATIBLE;
    }

    BITE_IMPL_READ(&(header->reserved_1), file);
    BITE_IMPL_READ(&(header->file_entry_offset), file);
    BITE_IMPL_READ(&(header->file_entry_count), file);
    BITE_IMPL_READ(&(header->file_data_start_offset), file);
    BITE_IMPL_READ(&(header->reserved_2), file);

    return BITE_OK;
}

static bite__status_e bite__entry_read(bite__entry_t* entry, FILE* file) {
    BITE_IMPL_READ(&(entry->flags), file);
    // Is this a dir entry?
    if (entry->flags & 1) {
        BITE_IMPL_READ(&(entry->dir_sibling_offset), file);
        BITE_IMPL_READ(&(entry->dir_children_count), file);
    }
    // This is a file entry?
    else {
        BITE_IMPL_READ(&(entry->data_offset), file);
    }
    BITE_IMPL_READ(&(entry->data_size), file);
    BITE_IMPL_READ(&(entry->reserved), file);

    return BITE_OK;
}

static bite__status_e bite__table_read(bite__table_t* table, bite__header_t* header, FILE* file);
static void          bite__table_close(bite__table_t* table);

// Allocates data, so this must be freed using bite_table_close()!
static bite__status_e bite__table_read(bite__table_t* table, bite__header_t* header, FILE* file) {
    size_t file_count = header->file_entry_count;

    bite__status_e status;

    table->file.entries = (bite__entry_t*)malloc(sizeof(bite__entry_t) * file_count);
    if (!table->file.entries) return BITE_ERR_INVALID;

    table->file.count = file_count;

    table->pool.capacity = 32 * file_count; // Allocate 32 bytes per filepath to avoid possible realloc
    table->pool.ptr = (char*)malloc(table->pool.capacity);
    if (!table->pool.ptr) return BITE_ERR_INVALID;
    table->pool.size = 0;
    
    // Skip to file entry offset
    long old_pos = ftell(file);
    if (fseek(file, header->file_entry_offset, SEEK_SET) != 0) {
        return BITE_ERR_MALFORMED;
    }

    for (size_t i = 0; i < file_count; i++) {
        bite__entry_t* entry = table->file.entries + i;
        status = bite__entry_read(entry, file);

        // Stored strings are variable length.
        // First comes length (2 bytes), then afterwards
        // is a continuous ASCII/UTF-8 string (not null-terminated)

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
            return BITE_ERR_INVALID;
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
                return BITE_ERR_INVALID;
            }
            //printf("reallocated %zu bytes for name pool\n", table->pool.capacity);
        }

        bite__fread(&table->pool.ptr[entry->name_pool_offset], entry->name_length, file);
        table->pool.ptr[entry->name_pool_offset + entry->name_length] = '\0';
        table->pool.size += name_size;

        if (status != BITE_OK) {
            bite__table_close(table);
            return status;
        }
    }

    fseek(file, old_pos, SEEK_SET);
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

// Open a bite packed archive using the filepath
bite_packed_t* bite_packed_open(const char* filepath) {
    bite_packed_t* packed = NULL;

    FILE* file = fopen(filepath, "rb");
    if (file == NULL) {
        BITE_ERROR_MSG("Unable to open file at \"%s\", err: %d\n", filepath, ferror(file));
        return NULL;
    }

    packed = (bite_packed_t*)malloc(sizeof(*packed));
    if (packed == NULL) {
        BITE_ERROR_MSG("Unable to allocate data for \"%s\" handle.", filepath);
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
    fclose(packed->handle);
    bite__table_close(&packed->table);
    free(packed);
}

// Finds the first entry that matches with the given filepath.
// Returns null if no entry was found.
static bite__entry_t* bite__packed_find_entry(bite_packed_t* packed, const char* filepath) {
    struct bite__string_view part;
    part.ptr = filepath;
    part.size = 0;

    if (packed->table.file.count == 0)
        return NULL;

    size_t begin = 0;
    size_t end = packed->table.file.count;
    // size_t iterations = 0; // logging
    
    bite__entry_t* entry = NULL;
    while (*part.ptr != '\0') {
        part.size = 0;

        for (const char* ch = part.ptr; *ch != '\0'; ch++) {
            if (*ch == '/')
                break;
            part.size++;
        }

        int found = 0;
        for (size_t i = begin; i < end; i++) {
            entry = packed->table.file.entries + i;
            const char* name_ptr = packed->table.pool.ptr + entry->name_pool_offset;

            // iterations++; // Log

            if (entry->name_length == part.size && strncmp(part.ptr, name_ptr, part.size) == 0) {
                // Names match
                if (entry->flags & 1) { // Is this a directory?
                    // Enter it and marked it as found
                    begin = i + 1;
                    end = i + entry->dir_sibling_offset + 1;
                    found = 1;
                    break;
                } else { // Is this a file?
                    part.ptr += part.size;
                    while (*part.ptr == '/') part.ptr++;

                    if (*part.ptr == '\0') {
                        //printf("Took %d iterations!\n", (int)iterations);
                        return entry;
                    } else {
                        BITE_ERROR_MSG("%s: treats file as if it were a directory.", filepath);
                        return NULL;
                    }
                }
            } else { // Names aren't the same
                if (entry->flags & 1) { // Is this a directory?
                    // Skip all entries towards its sibling entry.
                    i += entry->dir_sibling_offset;
                }
            }
        }

        // If no match was found, then path is invalid.
        if (!found) {
            entry = NULL;
            break;
        }

        // Go up a filepath part.
        part.ptr += part.size;
        while (*part.ptr == '/') part.ptr++;
    }

    //printf("Took %d iterations to realize this doesn't exist!\n", (int)iterations);
    if (entry && entry->flags & 1)
        BITE_ERROR_MSG("%s: is a directory, not a file!", filepath);
    else
        BITE_ERROR_MSG("%s: not found.", filepath);
    
    return NULL;
}

// Prepares a bite_file_t object.
static bite_file_t* bite__file_open_entry(bite_packed_t* packed_ref, bite__entry_t* entry_ref) {
    bite_file_t* file = (bite_file_t*)malloc(sizeof(*file));
    
    // just in case...
    if (!file) return NULL;

    memset(file, 0, sizeof(*file));
    file->packed_ref = packed_ref;
    file->entry_ref = entry_ref;
    file->pos = 0;
    return file;
}

// Finds and opens a virtual file inside of the packed file
bite_file_t* bite_fopen(bite_packed_t* packed, const char* filepath) {
    if (!packed) {
        BITE_ERROR_MSG("%s: packed handle is NULL! Maybe it wasn't properly initialized?", filepath);
        return NULL;
    }

    bite__entry_t* entry = bite__packed_find_entry(packed, filepath);
    if (!entry) return NULL;

    bite_file_t* file = bite__file_open_entry(packed, entry);
    if (!file) {
        BITE_ERROR_MSG("%s: unable to allocate bite_file_t handle.", filepath);
        return NULL;
    }

    return file;
}

// Returns the name of the virtual file
const char* bite_fname(bite_file_t* file) {
    char* ptr = file->packed_ref->table.pool.ptr;
    ptr += file->entry_ref->name_pool_offset;
    return ptr;
}

// Closes a virtual file.
void bite_fclose(bite_file_t* file) {
    if (!file) return;
    free(file);
}

// Returns the total file size of the virtual file
size_t bite_fsize(bite_file_t* file) {
    return file->entry_ref->data_size;
}

// Reads size of data into the destination buffer from the virtual file
size_t bite_fread(void* dst, size_t size, bite_file_t* file) {
    bite__entry_t* entry = file->entry_ref;
    
    // Clamp cursor to size
    if (file->pos > entry->data_size) {
        file->pos = entry->data_size;
    }

    FILE* handle = file->packed_ref->handle;
    fseek(handle, entry->data_offset + file->pos, SEEK_SET);

    // Limit reading size
    if (file->pos + size >= entry->data_size) {
        size = entry->data_size - file->pos;
    }

    file->pos += size;
    if (size != 0) {
        int result = bite__fread(dst, size, handle);
        if (!result) {
            return 0;
        }
    }

    return size;
}

// Returns the virtual file's current cursor position
size_t bite_ftell(bite_file_t* file) {
    return file->pos;
}

// Seeks into a specific point depending on a whence
int bite_fseek(bite_file_t* file, long offset, int whence) {
    size_t pos = 0;

    switch (whence) {
        case SEEK_SET:
            pos = offset;
            break;
        case SEEK_CUR:
            pos = file->pos + offset;
            break;
        case SEEK_END:
            pos = file->entry_ref->data_size + offset;
            break;
        default:
            return -1;
    }

    if (pos > file->entry_ref->data_size) {
        return -1;
    }

    file->pos = pos;
    return 0;
}

// Returns the a string containing the error info
const char* bite_error_str() {
    return error_text_buffer;
}
