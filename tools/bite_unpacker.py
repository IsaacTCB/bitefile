
"""
Bite file unpacker
"""

import struct
import os
from argparse import ArgumentParser, Namespace
from pathlib import Path


def unpack_bite(bite, extract_path: Path):
    header = read_header(bite)
    files = read_file_table(bite, header)

    for file in files:
        dst = extract_path / file["name"]
        try:
            extract_file(bite, file, dst)
        except Exception as exception:
            print("Unable to extract file: ", exception)
            continue  # Keep going...


# ==========================
# Core
# ==========================

def extract_file(bite, file_entry, dst):
    """Extracts a singular file into the destination."""

    # Skip to file data offset
    bite.seek(file_entry["offset"], os.SEEK_SET)

    # Output the file
    os.makedirs(os.path.dirname(dst), exist_ok=True)
    with open(dst, "wb") as out_file:
        remaining_size = file_entry["size"]

        # Stream file reading/writing
        # Only 512MB shall be loaded to RAM at once
        while remaining_size > 0:
            bytes_to_read = min(remaining_size, 512 * 1024 * 1024)
            buffer = bite.read(bytes_to_read)
            remaining_size -= bytes_to_read
            out_file.write(buffer)


def read_struct(fmt, file):
    """Wrapper function for reading bytes of files into real numbers."""

    if ">" not in fmt and "<" not in fmt:
        fmt = "<" + fmt  # Ensure little endian

    size = struct.calcsize(fmt)
    buffer = file.read(size)
    return struct.unpack(fmt, buffer)[0]


def read_string(file):
    """Reads a string from the bite file."""

    length = read_struct("<H", file)

    bytedata = file.read(length)
    string = bytedata.decode('utf-8')
    return string


def read_header(bite):
    """Reads the bite header and converts it into a human-readable
    dictionary."""

    magic = bite.read(4)
    if magic != b'BITE':
        raise Exception("File is not a Bite file!")

    version = read_struct("<H", bite)
    file_table_offset = read_struct("<I", bite)
    file_table_count = read_struct("<I", bite)

    return {
        "magic": magic,
        "version": version,
        "file_table_offset": file_table_offset,
        "file_table_count": file_table_count,
    }


def read_file_entry(bite):
    """Read and parse a single file entry. This is used by
    read_file_table()."""

    offset = read_struct("<I", bite)
    size = read_struct("<I", bite)
    read_struct("<I", bite)  # Skip reserved
    name = read_string(bite)

    return {
        "offset": offset,
        "size": size,
        "name": name,
    }


def read_file_table(bite, header):
    """Reads and parses the file table. Returns the parsed filedata containing
    all file entries"""

    file_table_entries = []

    # Skip to table offset
    bite.seek(header["file_table_offset"], os.SEEK_SET)

    for i in range(header["file_table_count"]):
        entry = read_file_entry(bite)
        file_table_entries.append(entry)

    return file_table_entries


# ==========================
# Helpers
# ==========================

def build_parser():
    """Builds an argparse object containing all relevant cli data"""

    parser = ArgumentParser(
        prog="bite_unpacker",
        description="Bite file extractor",
    )
    parser.add_argument(
        "input",
        help="select Bite for extraction.",
        type=str,
        nargs=1,
        metavar="bite"
    )
    parser.add_argument(
        "-e", "--extract-to",
        help="specify a destination path for extracted files",
        type=str,
        default="",
    )
    return parser


# ==========================
# Entrypoint
# ==========================

def main():
    parser = build_parser()
    args: Namespace = parser.parse_args()

    try:
        bite = open(args.input, "rb")
        unpack_bite(bite, Path(args.extract_to))
    except FileNotFoundError:
        print(f"Unable to open \"{args.input}\"")
        exit(2)
    except Exception as exception:
        print(exception)
        exit(3)


if __name__ == "__main__":
    main()
