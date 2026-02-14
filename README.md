# bitefile

> THIS IS STILL A WORK-IN-PROGRESS! The Bite format is not fully-finalized.
> Therefore, it might receive compatibility-breaking updates in the spec
> without remorse. Use this at your own risk!

*bitefile* is a small C library for reading files from Bite packed archives,
providing a simple stdio-like interface.

This is a project that I developed for fun, so it shouldn't be treated as
a serious and professional-grade way of reading and storing data.

## What are Bite files?

Bite is a data archive format I created for storing multiple files of any
type into one large `.bite` file, akin to
[Doom's WAD files](https://doomwiki.org/wiki/WAD) and
[Godot's PCK format](https://github.com/godotengine/godot/blob/master/core/io/pck_packer.cpp).
In fact, the latter was a great source of reference for the implementation.

It was designed as a way to store/load assets for small game engines, where
file loading and processing speeds are crucial. No compression algorithms
have been employed (although I'm open to the idea of implementing an opt-in
per-file compression solution).

Bite internally stores data offset and sizes using 64-bit values, meaning
that it can store files up to 16 EiB in theory, but in practice you will
run into issues when opening bite files that are larger than 2 GiB, since
*bitefile* currently relies on the standard 32-bit stdio file operations.

Paths are internally stored
as a flattened tree hierarchy that represents each directory as a branch
which may or not contain files or other directories. This approach greatly
reduces the amount of `strncmp` calls needed to identify where a specific
file entry is located given a path.

That being said, Bite isn't anything too fancy, really. It contains a basic
header and a file metadata table that points into the uncompressed raw
binary data of each file. As of now, no formal specification for this format
has been written, but you can take a look inside the python scripts
(located in `tools/`!) for a general understanding of its structure.

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

### Examples

Inside `examples/`, you can find a set of demo projects showcasing
some of the features of `bitefile`. They get compiled by default when
building the CMake project as top-level.

## Packing/Unpacking Bite archives

Inside `tools/`, you'll find a set of useful Python CLI scripts:

- **bite_packer.py**: Used for **creating** bite archives.
  - USAGE: `python3 bite_packer.py -r <path_to_folder/files...> -o <output>`

- **bite_unpacker.py**: Used for **extracting** bite archives.
  - USAGE: `python3 bite_unpacker.py <input> -e [path_to_destination]`

> These scripts were primarily designed for integration with automated
> build systems in mind, though manual usage is also permitted.
> You can pass `-h` to view the list of all accepted actions and options.

## To-do

These are some of the missing features that I would like to implement/do in
the future:

- Ability to specify callback functions for `bite_packed_open()`.
- Add support for 64-bit fseek/ftell extensions.
- API for listing all files/dirs in a directory (like dirent.h, perhaps?)
- Allow interaction with multiple packed files using a single packed_file_t* handle.
- Per-file compression.
- CRC Checksum system?
