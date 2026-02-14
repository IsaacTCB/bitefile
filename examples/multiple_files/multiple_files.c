// SPDX-License-Identifier: MIT
/*
 * Copyright (c) 2026-present IsaacTCB
 * Licensed under the MIT License
 */

#include <bitefile/bite.h>

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#define ARRAY_SIZE(array) (sizeof(array) / sizeof(*array))

int main(int argc, char* argv[]) {
    char* bite_filepath = "data.bite";
    if (argc > 1) {
        bite_filepath = argv[1];
    }

    bite_packed_t* packed = bite_packed_open(bite_filepath);
    if (!packed) {
        printf("Error: %s\n", bite_error_str());
        exit(3);
    }

    // List of filepaths to open.
    const char* paths[] = {
        /*
         * All of these files will get open at the same time.
        */
        "assets/multiple_files/file1.txt",
        "assets/multiple_files/file2.txt",
        "assets/multiple_files/file3.txt",

        /*
         * You can even have the the same file open more than once!
         * States are completely independent from each other.
        */
        "assets/multiple_files/file2.txt",
    };

    bite_file_t* files[ARRAY_SIZE(paths)];

    // Open files
    for (size_t i = 0; i < ARRAY_SIZE(files); i++) {
        files[i] = bite_fopen(packed, paths[i]);
        if (files[i]) {
            printf("Opened \"%s\"\n", paths[i]);
        } else {
            printf("%s\n", bite_error_str());
        }
    }

    // Read and print text from all open files
    for (size_t i = 0; i < ARRAY_SIZE(files); i++) {
        if (files[i] == NULL) {
            continue;
        }
        // Get size using the Bite way.
        bite_size_t len = bite_fsize(files[i]);
        if (len > 0) {
            char* buffer = malloc(len+1);
            len = bite_fread(buffer, len, files[i]);
            buffer[len] = '\0';
            printf("Read %zd bytes from \"%s\"\n%s\n", len, paths[i], buffer);
            free(buffer);
        }
    }

    // Close files
    for (size_t i = 0; i < ARRAY_SIZE(files); i++) {
        if (files[i] == NULL) {
            continue;
        }
        bite_fclose(files[i]);
        printf("Closed \"%s\"\n", paths[i]);
        files[i] = NULL;
    }
}
