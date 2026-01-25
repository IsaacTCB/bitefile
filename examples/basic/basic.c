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
        exit(3);
    }

    // Open text file and print its content.
    bite_file_t* file = bite_fopen(packed, "assets/hello.txt");
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
    }

    // Open the other text file and print its content.
    file = bite_fopen(packed, "assets/other.txt");
    if (file) {
        printf("Found file '%s'!\n", bite_fname(file));

        // Get size using the std way.
        int status = bite_fseek(file, 0, SEEK_END);
        assert(status == 0);
        size_t len = bite_ftell(file);
        status = bite_fseek(file, 0, SEEK_SET);
        assert(status == 0);

        if (len > 0) {
            char* buffer = malloc(len+1);
            len = bite_fread(buffer, len, file);
            buffer[len] = '\0';
            printf("Read %ld bytes\n%s\n", len, buffer);
            free(buffer);
        }

        bite_fclose(file);
    }

    bite_packed_close(packed);
    return 0;
}
