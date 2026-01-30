
"""
Bite file unpacker
"""

import struct
import os
from io import BufferedReader
from argparse import ArgumentParser, Namespace
from pathlib import Path


def unpack_bite(bite: BufferedReader, extract_path: Path) -> None:
    header = read_header(bite)
    table = read_table(bite, header)
    extract_tree(bite, table, extract_path)


# ==========================
# Core
# ==========================

class BiteParsingError(Exception):
    def __init__(self, message):
        super.__init__(message)


class ExtractionFailure(Exception):
    def __init__(self, message):
        super.__init__(message)


def extract_tree(
    bite: BufferedReader,
    table: list[dict],
    extract_path: Path
) -> None:
    """
    Extracts all files/dirs from the flattened file table tree
    """

    # For better performance, we use stacks to know what directory
    # we are on, instead of rebuilding an actual full tree and then
    # iterating over it.
    dir_stack: list[int] = []
    dir_nested = Path()

    for i in range(len(table)):
        entry = table[i]

        if len(dir_stack) > 128:
            raise ExtractionFailure("Too many subdirectories! Maybe this .bite is bad?")

        # Are we still on the stack's top directory?
        while dir_stack and dir_stack[-1] < i:
            # If not, pop it and update current nested path.
            dir_stack.pop()
            dir_nested = dir_nested.parent

        try:
            match (entry["type"]):
                case "dir":
                    # This a dir, push it onto our stack
                    dir_nested = dir_nested / entry["name"]
                    dir_stack.append(i + entry["sibling"])

                    # Create directory
                    dst = extract_path / dir_nested
                    os.makedirs(dst, exist_ok=True)
                case "file":
                    # This is a file. Extract it!
                    dst = extract_path / dir_nested / entry["name"]
                    entry["path"] = dir_nested / entry["name"]  # For printing
                    extract_file(bite, entry, dst)
                case _:
                    raise ExtractionFailure("Invalid tree type")

        except ExtractionFailure as exception:
            print(f"Unable to parse index at {i}, reason: {exception}.")


def extract_file(bite: BufferedReader, file_entry, dst) -> None:
    """
    Extracts a singular file into the destination.
    """

    # Skip to file data offset
    bite.seek(file_entry["offset"], os.SEEK_SET)

    # Output the file
    os.makedirs(os.path.dirname(dst), exist_ok=True)
    with open(dst, "wb") as out_file:
        remaining_size = file_entry["size"]

        # Stream file reading/writing
        # Only 1024MB shall be loaded to RAM at once
        while remaining_size > 0:
            bytes_to_read = min(remaining_size, 1024 * 1024 * 1024)
            buffer = bite.read(bytes_to_read)
            remaining_size -= bytes_to_read
            out_file.write(buffer)

        _print(f"Extracted {file_entry["path"]}")


def read_struct(fmt: str, file: BufferedReader) -> int:
    """
    Wrapper function for reading bytes of files into real numbers.
    """

    if ">" not in fmt and "<" not in fmt:
        fmt = "<" + fmt  # Ensure little endian

    size = struct.calcsize(fmt)
    buffer = file.read(size)
    return struct.unpack(fmt, buffer)[0]


def read_string(file: BufferedReader) -> str:
    """
    Reads a string from the bite file.
    """

    length = read_struct("<B", file)

    bytedata = file.read(length)
    string = bytedata.decode('utf-8')
    return string


def read_header(bite: BufferedReader) -> dict:
    """
    Reads the bite header and converts it into a human-readable
    dictionary.
    """

    magic = bite.read(4)
    if magic != b'BITE':
        raise BiteParsingError("File is not a Bite file!")

    version = read_struct("<H", bite)
    read_struct("<H", bite)  # Skip reserved
    file_table_offset = read_struct("<Q", bite)
    file_table_count = read_struct("<I", bite)
    file_data_offset = read_struct("<Q", bite)
    read_struct("<I", bite)  # Skip reserved

    return {
        "magic": magic,
        "version": version,
        "file_table_offset": file_table_offset,
        "file_table_count": file_table_count,
        "file_data_offset": file_data_offset,
    }


def read_file_entry(bite: BufferedReader, flags: int) -> dict:
    """
    Read and parse a single file entry. This is used by
    read_file_table().
    """

    offset = read_struct("<Q", bite)
    size = read_struct("<Q", bite)
    read_struct("<I", bite)  # Skip reserved
    name = read_string(bite)

    return {
        "type": "file",
        "flags": flags,
        "offset": offset,
        "size": size,
        "name": name,
    }


def read_dir_entry(bite: BufferedReader, flags: int) -> dict:
    """
    Read and parse a single dir entry. This is used by
    read_file_table().
    """

    sibling = read_struct("<I", bite)
    children = read_struct("<I", bite)
    total_size = read_struct("<Q", bite)
    read_struct("<I", bite)  # Skip reserved
    name = read_string(bite)

    return {
        "type": "dir",
        "flags": flags,
        "sibling": sibling,
        "children": children,
        "size": total_size,
        "name": name
    }


def read_table(bite: BufferedReader, header: dict) -> list[dict]:
    """
    Reads and parses the file table. Returns the parsed filedata containing
    all file entries
    """

    file_table_entries = []

    # Skip to table offset
    bite.seek(header["file_table_offset"], os.SEEK_SET)

    for _ in range(header["file_table_count"]):
        flags = read_struct("<I", bite)
        if flags & 1:
            entry = read_dir_entry(bite, flags)
        else:
            entry = read_file_entry(bite, flags)

        file_table_entries.append(entry)

    return file_table_entries


# ==========================
# Helpers
# ==========================

def build_parser() -> ArgumentParser:
    """
    Builds an argparse object containing all relevant cli data
    """

    parser = ArgumentParser(
        prog="bite_unpacker",
        description="Bite file extractor",
    )
    parser.add_argument(
        "input",
        help="select Bite for extraction.",
        type=str,
        metavar="bite"
    )
    parser.add_argument(
        "-e", "--extract-to",
        help="specify a destination path for extracted files",
        type=str,
        default="",
    )
    parser.add_argument(
        "-v", "--verbose",
        help="display extra messages",
        action="store_true",
    )
    return parser


def _print(*args):
    """
    Only prints message if VERBOSE is True
    """

    if VERBOSE:
        print(*args)


# ==========================
# Entrypoint
# ==========================

def main():
    """
    Entrypoint
    """

    parser = build_parser()
    args: Namespace = parser.parse_args()

    # Ugly but works
    global VERBOSE
    VERBOSE = args.verbose

    try:
        bite = open(args.input, "rb")
        unpack_bite(bite, Path(args.extract_to))
        _print("Extraction complete!")
    except FileNotFoundError:
        print(f"Unable to open \"{args.input}\"")
        exit(2)
    # except Exception as exception:
    #     print(exception)
    #     exit(3)


if __name__ == "__main__":
    main()
