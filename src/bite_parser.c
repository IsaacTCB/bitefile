#include "bite_parser.h"

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

typedef struct {
    uint32_t magic; // BITE, in ASCII
    uint16_t version;
    uint32_t file_entry_offset;
    uint32_t file_entry_count;
    uint16_t reserved;
} bite__header_t;

typedef struct {
    uint32_t data_offset; // Is 0 for empty files
    uint32_t data_size;
    uint32_t reserved;
    char*    name;
} bite__entry_t;

typedef struct {
    struct {
        bite__entry_t* entries;
        size_t count;
    } file;
    // struct {
    //     char* ptr;
    //     size_t size;
    //     size_t capacity;
    // } pool;
} bite__table_t;

struct bite_packed {
    FILE* handle;
    bite__header_t header;
    bite__table_t table;
};

struct bite_file {
    bite_packed_t* packed_ref; // Weak ref, packed is responsible for closing itself
    bite__entry_t* entry_ref; // Same as above
    size_t pos;
};

// Raw usage of this function should be avoided unless strictly needed;
// Use BITE_IMPL_READ wrapper instead, as that is safer and a whole lot cleaner.
static int bite__fread(void* out_ptr, size_t size, FILE* file) {
    return fread(out_ptr, size, 1, file) == 1;
}

static bite_status_e bite__header_read(bite__header_t* header, FILE* file) {
    BITE_IMPL_READ(&(header->magic), file);
    if (header->magic != BITE_FILE_MAGIC) {
        // printf("%d != %d", header->magic, BITE_FILE_MAGIC);
        return BITE_ERR_INVALID;
    }

    BITE_IMPL_READ(&(header->version), file);
    if (header->version != BITE_FILE_VERSION) {
        return BITE_ERR_INCOMPATIBLE;
    }

    BITE_IMPL_READ(&(header->file_entry_offset), file);
    BITE_IMPL_READ(&(header->file_entry_count), file);
    BITE_IMPL_READ(&(header->reserved), file);

    return BITE_OK;
}

static bite_status_e bite__entry_read(bite__entry_t* entry, FILE* file) {
    BITE_IMPL_READ(&(entry->data_offset), file);
    BITE_IMPL_READ(&(entry->data_size), file);
    BITE_IMPL_READ(&(entry->reserved), file);

    // Stored strings are variable length.
    // First comes length (2 bytes), then afterwards
    // is a continuous ASCII/UTF-8 string (not null-terminated)
    uint16_t name_length;
    BITE_IMPL_READ(&name_length, file);

    // @todo: use memory arena for storing strings.
    char* name = (char*)malloc(name_length+1);

    do {
        int status = bite__fread(name, name_length, file);
        if (!status) {
            free(name); // Memory arenas would fix this
            return BITE_ERR_MALFORMED;
        }
    } while (0);

    name[name_length] = '\0';
    entry->name = name;

    return BITE_OK;
}

static bite_status_e bite__table_read(bite__table_t* table, bite__header_t* header, FILE* file);
static void          bite__table_close(bite__table_t* table);

// Allocates data, so this must be freed using bite_table_close()!
static bite_status_e bite__table_read(bite__table_t* table, bite__header_t* header, FILE* file) {
    size_t file_count = header->file_entry_count;

    bite_status_e status;

    table->file.entries = (bite__entry_t*)malloc(sizeof(bite__entry_t) * file_count);
    if (!table->file.entries) return BITE_ERR_INVALID;

    table->file.count = file_count;
    
    // Skip to file entry offset
    long old_pos = ftell(file);
    if (fseek(file, header->file_entry_offset, SEEK_SET) != 0) {
        printf("Unable to seek to skip file entry table.\n");
        return BITE_ERR_MALFORMED;
    }

    for (size_t i = 0; i < file_count; i++) {
        bite__entry_t* entry = table->file.entries + i;
        status = bite__entry_read(entry, file);

        if (status != BITE_OK) {
            table->file.count = i; // hacky, but works
            bite__table_close(table);
            return status;
        }
    }

    fseek(file, old_pos, SEEK_SET);
    return BITE_OK;
}

static void bite__table_close(bite__table_t* table) {
    if (table->file.entries) {
        for (size_t i = 0; i < table->file.count; i++) {
            bite__entry_t* entry = table->file.entries + i;
            free(entry->name); // This is annoying. Perhaps a memory arena would be more fitting?
        }

        free(table->file.entries);
        table->file.entries = NULL;
    }

    table->file.count = 0;
}

bite_packed_t* bite_packed_open(const char* filepath) {
    bite_packed_t* packed = NULL;

    FILE* file = fopen(filepath, "rb");
    if (file == NULL) {
        printf("Unable to open file at \"%s\"\n", filepath);
        return NULL;
    }

    packed = (bite_packed_t*)malloc(sizeof(*packed));
    memset(packed, 0, sizeof(*packed));

    packed->handle = file;

    bite_status_e status = bite__header_read(&packed->header, file);
    if (status != BITE_OK) {
        printf("Unable to parse header. Err: %d\n", status);
        fclose(file);
        free(packed);
        return NULL;
    }

    status = bite__table_read(&packed->table, &packed->header, file);
    if (status != BITE_OK) {
        printf("Unable to parse header. Err: %d\n", status);
        fclose(file);
        free(packed);
        return NULL;
    }

    return packed;
}

void bite_packed_close(bite_packed_t* packed) {
    fclose(packed->handle);
    bite__table_close(&packed->table);
    free(packed);
}

// Finds the first entry that matches with the given filepath.
// Returns null if no entry was found.
static bite__entry_t* bite__packed_find_entry(bite_packed_t* packed, const char* filepath) {
    bite__entry_t* entry;

    for (size_t i = 0; i < packed->table.file.count; i++) {
        entry = packed->table.file.entries + i;
        if (strcmp(entry->name, filepath) == 0) {
            return entry;
        }
    }

    return NULL;
}

// Prepares a bite_file_t object.
static bite_file_t* bite__file_open_entry(bite_packed_t* packed_ref, bite__entry_t* entry_ref) {
    bite_file_t* file = (bite_file_t*)malloc(sizeof(*file));
    memset(file, 0, sizeof(*file));

    file->packed_ref = packed_ref;
    file->entry_ref = entry_ref;
    file->pos = 0;

    return file;
}

bite_file_t* bite_fopen(bite_packed_t* packed, const char* filepath) {
    bite__entry_t* entry = bite__packed_find_entry(packed, filepath);
    if (!entry) {
        return NULL;
    }

    bite_file_t* file = bite__file_open_entry(packed, entry);
    return file;
}

const char* bite_fname(bite_file_t* file) {
    return file->entry_ref->name;
}

void bite_fclose(bite_file_t* file) {
    if (!file) return;
    free(file);
}

size_t bite_fsize(bite_file_t* file) {
    return file->entry_ref->data_size;
}

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

size_t bite_ftell(bite_file_t* file) {
    return file->pos;
}

int bite_fseek(bite_file_t* file, long int offset, int whence) {
    size_t pos = 0;

    switch (whence) {
        case SEEK_SET:
            pos = offset;
            break;

        case SEEK_CUR:
            pos = file->pos + offset;
            break;

        case SEEK_END:
            pos = file->entry_ref->data_size - offset;
            break;
    }

    if (pos > file->entry_ref->data_size) {
        return -1;
    }

    file->pos = pos;
    return 0;
}
