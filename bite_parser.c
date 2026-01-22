#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define BITE_FILE_MAGIC (1163151682) /* BITE, in ASCII */
#define BITE_FILE_VERSION (1)

// Safe file reader wrapper. Calls 'return BITE_ERR_MALFORMED' on fail.
#define BITE_IMPL_READ(dst, file)   do { \
                                        int status = bite__fread(dst, sizeof(*dst), file); \
                                        if (!status) return BITE_ERR_MALFORMED; \
                                    } while (0) \

typedef enum {
    BITE_OK = 0,
    BITE_ERR_INVALID, // Not a valid bite file!
    BITE_ERR_INCOMPATIBLE, // Incompatible version
    BITE_ERR_MALFORMED,    // Malformed file format

} bite_status_e;

typedef struct {
    uint32_t magic; // BITE, in ASCII
    uint16_t version;
    uint32_t file_entry_offset;
    uint32_t file_entry_count;
    uint16_t reserved;

} bite_header_t;

typedef struct {
    uint32_t data_offset; // Is 0 for empty files
    uint32_t data_size;
    uint32_t reserved;
    char*    name;

} bite_entry_t;

typedef struct {
    struct {
        bite_entry_t* entries;
        size_t count;
    } file;

    // struct {
    //     char* ptr;
    //     size_t size;
    //     size_t capacity;
    // } pool;

} bite_table_t;

typedef struct {
    FILE* handle;
    bite_header_t header;
    bite_table_t table;

} bite_packed_t;

typedef struct {
    bite_packed_t* packed_ref; // Weak ref, packed is responsible for closing itself
    // bite_file_entry* entry_ref; // Same as above
    size_t pos;

} bite_file_t;

// -------------
// Public
// -------------

bite_packed_t* bite_packed_open(const char* filepath);
void           bite_packed_close(bite_packed_t* packed);

bite_file_t* bite_fopen(bite_packed_t* packed, const char* filepath);
void         bite_fread(void* dst, size_t size, bite_file_t* file);
size_t       bite_ftell(bite_file_t* file);
void         bite_fseek(bite_file_t* file, size_t pos, int whence);
void         bite_fclose(bite_file_t* file);

// -------------
// Private
// --------------

// Raw usage of this function should be avoided unless strictly needed;
// Use BITE_IMPL_READ wrapper instead, as that is safer and a whole lot cleaner.
static int bite__fread(void* out_ptr, size_t size, FILE* file) {
    return fread(out_ptr, size, 1, file) == 1;
}

static bite_status_e bite__header_read(bite_header_t* header, FILE* file) {
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

static bite_status_e bite__entry_read(bite_entry_t* entry, FILE* file) {
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

static bite_status_e bite__table_read(bite_table_t* table, bite_header_t* header, FILE* file);
static void          bite__table_close(bite_table_t* table);

// Allocates data, so this must be freed using bite_table_close()!
static bite_status_e bite__table_read(bite_table_t* table, bite_header_t* header, FILE* file) {
    size_t file_count = header->file_entry_count;

    bite_status_e status;

    table->file.entries = (bite_entry_t*)malloc(sizeof(bite_entry_t) * file_count);
    if (!table->file.entries) return BITE_ERR_INVALID;

    table->file.count = file_count;
    
    // Skip to file entry offset
    long old_pos = ftell(file);
    if (fseek(file, header->file_entry_offset, SEEK_SET) != 0) {
        printf("Unable to seek to skip file entry table.\n");
        return BITE_ERR_MALFORMED;
    }

    for (size_t i = 0; i < file_count; i++) {
        bite_entry_t* entry = table->file.entries + i;
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

static void bite__table_close(bite_table_t* table) {
    if (table->file.entries) {
        for (size_t i = 0; i < table->file.count; i++) {
            bite_entry_t* entry = table->file.entries + i;
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

int main(int argc, char* argv[]) {
    char* bite_filepath = "data.bite";
    if (argc > 1) {
        bite_filepath = argv[1];
    }
    
    bite_packed_t* packed = bite_packed_open(bite_filepath);
    if (!packed) {
        exit(3);
    }

    // Print filenames
    for (size_t i = 0; i < packed->table.file.count; i++) {
        bite_entry_t* entry = packed->table.file.entries + i;
        printf("FILE: %s\n", entry->name);
    }

    bite_packed_close(packed);
    return 0;
}
