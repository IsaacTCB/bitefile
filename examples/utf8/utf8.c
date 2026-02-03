// SPDX-License-Identifier: MIT
//
// Copyright (c) 2026-present IsaacTCB
// Licensed under the MIT License

#include <bitefile/bite.h>

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

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

    // Open text file and print its content.
    const char* filepath = "assets/utf8/thïs_filénàme_hãs_special_chars";
    bite_file_t* file = bite_fopen(packed, filepath);
    if (file) {
        printf("Found file '%s'!\n", bite_fname(file));

        // Get size using the Bite way.
        size_t len = bite_fsize(file);

        if (len > 0) {
            char* buffer = malloc(len+1);
            len = bite_fread(buffer, len, file);
            buffer[len] = '\0';
            printf("Read %ld bytes\n%s\n", len, buffer);
            free(buffer);
        }

        bite_fclose(file);
    } else {
        printf("Error: %s\n", bite_error_str());
    }

    bite_packed_close(packed);
    return 0;
}
