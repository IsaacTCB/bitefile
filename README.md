# bitefile

> THIS IS STILL A WORK-IN-PROGRESS! The Bite format is not fully-finalized.
> Therefore, it might receive compatibility-breaking updates in the spec
> without remorse. I strongly advise not using this for any serious project
> in its current state.

*bitefile* is a small C library for reading files from Bite packed archives,
providing a simple stdio-like interface.

## What are Bite files?

Bite is a data archive format I created for storing multiple files of any
type into one large `.bite` file, akin to
[Doom's WAD files](https://doomwiki.org/wiki/WAD) and
[Godot's PCK format](https://github.com/godotengine/godot/blob/master/core/io/pck_packer.cpp).
In fact, the latter turned out to be a great source of information for
Bite's specification design.

That being said, Bite isn't anything too fancy, really. It contains a basic
header and a file metadata table that points into the uncompressed raw
binary data of each file.

## Usage

The following C code explains the process of reading from Bite packed
archives using *bitefile*:

```c
#include <bitefile/bite.h>

void load_data_from_bite() {
    // Open file packed data archive
    const char* bite_filepath = "data.bite"
    bite_packed_t* packed = bite_packed_open(bite_filepath);
    if (!packed) return;

    // Get a virtual file handle inside 'data.bite'
    const char* filepath = "my_super_cool_file.txt"
    bite_file_t* file = bite_fopen(packed, filepath);

    // Is the file open/found?
    if (file) {
        // Reading data
        char buffer[64];
        bite_fread(buffer, sizeof(buffer), file); // Load the first 64 bytes
        
        // File control stuff
        bite_fseek(file, 0, SEEK_END); // Skip to file end
        size_t pos = bite_ftell(file); // Tell file cursor position
        
        // Close file
        bite_fclose(file);
    }

    // We are done.
    bite_packed_close(packed);
}
```

As you can see, the program opens the Bite packed `data.bite` that contains a
file called `my_super_cool_file.txt`. It then reads the first 64 bytes of
this file into a buffer.

## Packing Bite archives

Inside of `tools/`, you can find a set of Python CLI scripts:

- **bite_packer.py**: Used for creating bite archives.
  - USAGE: `python3 bite_packer.py [path_to_files] -o <output>`

## To-do

These are some of the missing features that I would like to implement/do in the future:

- Per-file compression.
- Ability to specify callback functions for `bite_packed_open()`.
- Ability to list all files/dirs in a directory (like dirent.h, perhaps?)
- Better file searching algorithm.
