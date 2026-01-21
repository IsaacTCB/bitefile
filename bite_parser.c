#include <assert.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define BITE_FILE_MAGIC (1163151682) /* BITE, in ASCII */
#define BITE_FILE_VERSION (1)

// Safe file reader wrapper. Calls 'return BITE_ERR_MALFORMED' on fail.
#define BITE_IMPL_READ(dst, file)   do { \
                                        int8_t status = bite__fread(dst, sizeof(*dst), file); \
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

// Raw usage of this function should be avoided unless strictly needed;
// Use BITE_IMPL_READ wrapper instead, as that is safer and a whole lot cleaner.
static int8_t bite__fread(void* out_ptr, size_t size, FILE* file) {
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
    bite__fread(name, name_length, file);
    name[name_length] = '\0';
    entry->name = name;

    return BITE_OK;
}

static bite_status_e bite__entry_table_read(bite_entry_t** entry_table, size_t entry_count, FILE* file) {
    bite_status_e status;

    bite_entry_t* entries = (bite_entry_t*)malloc(sizeof(bite_entry_t) * entry_count);
    for (size_t i = 0; i < entry_count; i++) {
        bite_entry_t* entry = entries + i;
        status = bite__entry_read(entry, file);
        if (status != BITE_OK) {
            // Free up all string data
            for (size_t j = 0; j < i; j++) {
                entry = entries + j;
                free(entry->name); // This is annoying. Perhaps a memory arena would be more fitting?
            }

            free(entries);
            return status;
        }
    }

    *entry_table = entries;
    return BITE_OK;
}

int main(int argc, char* argv[]) {
    char* bite_filepath = "data.bite";
    if (argc > 1) {
        bite_filepath = argv[1];
    }

    FILE* file = fopen(bite_filepath, "rb");
    if (file == NULL) {
        printf("Unable to open file at \"%s\"\n", bite_filepath);
        exit(2);
    }
    
    // Parse header
    bite_header_t bite_header;
    bite_status_e status = bite__header_read(&bite_header, file);
    if (status != BITE_OK) {
        printf("Unable to parse header. Err: %d\n", status);
        exit(3);
    }

    // Load file entry table
    if (fseek(file, bite_header.file_entry_offset, SEEK_SET) != 0) {
        printf("Unable to seek to skip file entry table.\n");
        exit(3);
    }

    bite_entry_t* entry_table = NULL;
    status = bite__entry_table_read(&entry_table, bite_header.file_entry_count, file);
    if (status != BITE_OK) {
        printf("Unable to parse file entry table. Err: %d\n", status);
        exit(3);
    }

    // Print filenames
    for (size_t i = 0; i < bite_header.file_entry_count; i++) {
        bite_entry_t* entry = entry_table + i;
        printf("FILE: %s\n", entry->name);
    }

    // Free up all data
    for (size_t i = 0; i < bite_header.file_entry_count; i++) {
        bite_entry_t* entry = entry_table + i;
        free(entry->name); // This is annoying. Perhaps a memory arena would be more fitting?
    }
    free(entry_table);
    return 0;
}
