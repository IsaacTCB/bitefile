"""

Bite file packer

Copyright (c) 2026-present IsaacTCB
Licensed under the MIT License

"""

import argparse
import struct
import os
from io import BufferedWriter, BufferedReader
from pathlib import Path


def pack_bite(
    bite: BufferedWriter,
    paths: list[Path],
    relative_to: Path
):
    write_header(bite)  # Write a placeholder bite header

    file_tree = build_file_tree(paths, relative_to)
    if VERBOSE:
        print_file_tree(file_tree)

    table = pack_tree(bite, file_tree)

    # Write table list
    file_data_offset = 0
    write_padding(bite)
    file_table_offset = bite.tell()
    write_table(bite, table)

    # Patch bite header w/ new info
    write_header(
        bite,
        opts={
            "file_table_offset": file_table_offset,
            "file_table_count": len(table),
            "file_data_offset": file_data_offset,
        }
    )


# ==========================
# Core
# ==========================

class UserInputError(Exception):
    def __init__(self, message):
        super.__init__(message)


class DirTree:
    """
    This class represents the data structure of whole file tree hierarchy from
    a directory.

    Given a list of relative paths, you may easily build a valid FileTree
    instance using build_file_tree().
    """

    def __init__(self):
        self.dirs = {}  # A dictionary map containing DirTree nodes.
        self.files = set()  # A list of individual file names.


def write_header(
    bite: BufferedWriter,
    header_version: int = 1,
    opts: dict = {
        "file_table_offset": 0,
        "file_table_count": 0,
        "file_data_offset": 0,
    },
) -> None:
    """
    Writes the bite header into the output file.

    When called without any arguments (except for the file handle),
    it will print a stub header.
    """

    bite.seek(0, os.SEEK_SET)

    # Magic (4 bytes, ascii)
    magic = b"BITE"
    bite.write(magic)

    # Version (2 bytes)
    write_struct(bite, "<H", header_version)

    # Reserved (2 bytes)
    write_struct(bite, "<H", 0)

    # File table offset
    write_struct(bite, "<Q", opts["file_table_offset"])

    # File entry count (4 bytes)
    write_struct(bite, "<I", opts["file_table_count"])

    # File data start offset (8 bytes)
    write_struct(bite, "<Q", opts["file_data_offset"])

    # Reserved (4 bytes)
    write_struct(bite, "<I", 0)


def pack_tree(
    bite: BufferedWriter,
    root: DirTree,
    relative_path: Path = Path()
) -> list[dict]:
    """
    Packs every containing file inside a root DirTree recursively,
    and returns their table entries alongside every dir entry.
    """

    table_entries = []

    if relative_path != Path():
        table_entries.append({
            "type": "dir",
            "name": relative_path.name,
            "children": 0,
            "sibling": 0,
            "size": 0,  # @todo: calculate this.
        })

    children = 0
    sibling = 0
    for d in root.dirs:
        entries = pack_tree(bite, root.dirs[d], relative_path / d)
        table_entries += entries
        sibling += 1 + entries[0]["sibling"]
        children += 1

    for f in root.files:
        write_padding(bite)
        entry = pack_file(bite, f)
        table_entries.append((entry))
        sibling += 1
        children += 1

    if relative_path != Path():
        table_entries[0]["children"] = children
        table_entries[0]["sibling"] = sibling

        # Calculate directory filesize
        size = 0
        for entry in table_entries:
            if entry["type"] == "file":
                size += entry["size"]
        table_entries[0]["size"] = size

    return table_entries


def write_table_entry_dir(bite: BufferedWriter, dir_entry: dict) -> None:
    """
    Writes a single directory entry into a file from its
    current position.
    """

    # Write flags (4 bytes)
    write_struct(bite, "<I", 1)

    # Write sibling distance (4 bytes)
    write_struct(bite, "<I", dir_entry["sibling"])

    # Write children count (4 bytes)
    write_struct(bite, "<I", dir_entry["children"])

    # Write total dir size (8 bytes, not calculated yet)
    write_struct(bite, "<Q", dir_entry["size"])

    # Reserved (4 bytes)
    write_struct(bite, "<I", 0)

    # Name (1 byte + N bytes)
    write_string(bite, dir_entry["name"])


def write_table_entry_file(bite: BufferedWriter, file_entry: dict) -> None:
    """
    Writes a single file table entry, containing offsets,
    sizes and more.
    """

    # Write flags (4 bytes)
    write_struct(bite, "<I", 0)

    # Write offset (8 bytes)
    write_struct(bite, "<Q", file_entry["offset"])

    # Data size (8 bytes)
    write_struct(bite, "<Q", file_entry["size"])

    # Reserved data (4 bytes)
    write_struct(bite, "<I", 0)

    # Name (1 byte + N bytes)
    write_string(bite, file_entry["name"])


def write_table(bite: BufferedWriter, table_entries: list[dict]) -> None:
    """
    Writes the entire table list into the packed file.
    """

    for entry in table_entries:
        match entry["type"]:
            case "dir":
                write_table_entry_dir(bite, entry)
            case "file":
                write_table_entry_file(bite, entry)
            case _:
                raise Exception(f"Invalid file table type \"{entry["type"]}\"")


def pack_file(bite: BufferedWriter, input_path: Path) -> dict:
    """
    Opens a file and appends its data onto the bite file.
    """

    file_offset = bite.tell()
    total_size = 0

    with open(input_path, "rb") as file:
        _print(f"Packing {input_path}")
        data = process_file(file)
        bite.write(data)
        total_size = len(data)

    return {
        "type": "file",
        "name": input_path.name,
        "offset": file_offset,
        "size": total_size,
    }


def process_file(file: BufferedReader) -> bytes:
    """
    Processes a file and returns the data to be stored.
    """

    bytes = file.read()
    return bytes


# ==========================
# Helpers
# ==========================

def write_struct(bite: BufferedWriter, fmt: str, *values) -> None:
    """
    Writes a struct template to output file.
    """

    if ">" not in fmt and "<" not in fmt:
        fmt = "<" + fmt  # Ensure little endian

    bite.write(
        struct.pack(fmt, *values)
    )


def write_padding(bite: BufferedWriter, alignment: int = 16) -> None:
    """
    Writes empty padding until the file cursor is on a block alignment
    """

    pad = alignment - (bite.tell() % alignment)
    pad = pad % alignment
    for _ in range(pad):
        write_struct(bite, "b", 0)


def write_string(bite: BufferedWriter, string: str) -> None:
    encoded = string.encode('utf-8')
    write_struct(bite, "<B", len(encoded))
    bite.write(encoded)


def parser_build() -> argparse.ArgumentParser:
    """
    Builds an argparse object containing all relevant cli data
    """

    parser = argparse.ArgumentParser(
        prog="bite_packer",
        description="Packs multiple files into one monolithic bite file.",
    )
    parser.add_argument(
        "input",
        help="list of files to be packed",
        metavar="files",
        action="extend",
        type=str,
        nargs="+",
        default=[],
    )
    parser.add_argument(
        "-r", "--recursive",
        help="parse directories and their contents.",
        action="store_true",
    )
    parser.add_argument(
        "-rel", "--relative-to",
        help="set the relative path of packed files.",
        type=str,
        default=str(Path.cwd()),
    )
    # parser.add_argument(
    #     "-a", "--alignment",
    #     type=int,
    #     default=16,
    # )
    parser.add_argument(
        "-o", "--output",
        help="specify target filename",
        type=str,
        required=True
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


def parse_input_paths(args: argparse.Namespace) -> list[Path]:
    """
    Filters and validate paths based on args.
    Throws exceptions if invalid.
    """

    # Convert strings to paths, remove dirs & duplicates
    filtered_paths = []

    input_paths = [Path(path) for path in args.input]
    for path in input_paths:
        # Is this path even valid?
        if not path.exists():
            raise UserInputError(
                f"\"{path}\" does not exist!"
            )
        
        # Paths must be resolved beforehand!
        if ".." in path.parts:
            raise UserInputError(
                f"\"{path}\": All input paths must be resolved beforehand!"
            )

        # For directories, recurse only if that setting is enabled
        if path.is_dir():
            if not args.recursive:
                raise UserInputError(
                    f"\"{path}\" is a directory! "
                    "Use -r or --recursive for recursive selection."
                )

            for sub_path in path.rglob('*'):
                filtered_paths.append(sub_path)

        # Append normal files
        elif path.is_file():
            filtered_paths.append(path)
        else:
            raise UserInputError(
                f"\"{path}\" is an invalid path."
            )

    filtered_paths = list(set(filtered_paths))  # Remove duplicates

    if len(input_paths) == 0:
        raise UserInputError("No files to pack!")

    return filtered_paths


def build_file_tree(paths: list[Path], relative_to: Path) -> DirTree:
    """
    Builds a filesystem tree with the given input paths.

    paths is the list of inputs, may be absolute or not.
    relative_to specifies what path is used for converting absolute paths into relative paths.
    """

    root = DirTree()

    # Sort list by directory and alphabetically
    paths.sort(key=lambda p: (
        p.parts[:-1],
        p.name.lower(),
        p.name,
    ))

    for path in paths:
        p = path
        if p.is_absolute():
            p = p.relative_to(relative_to)

        parts = p.parts
        current_node = root

        for part in parts[:-1]:
            current_node = current_node.dirs.setdefault(part, DirTree())

        if path.is_file():
            current_node.files.add(path)
        else:
            current_node.dirs.setdefault(parts[-1], DirTree())

    return root


def print_file_tree(root: DirTree, indent: int = 0) -> None:
    """
    Prints a directory tree
    """

    tab = "   " * indent

    for d in root.dirs:
        print(tab + d)
        print_file_tree(root.dirs[d], indent+1)

    for f in root.files:
        print(tab + "- " + f.name)


# ==========================
# Entrypoint
# ==========================

def main() -> None:
    """
    Entrypoint
    """

    parser = parser_build()
    args = parser.parse_args()

    # This is ugly, but it works.
    global VERBOSE
    VERBOSE = args.verbose

    # Parse file inputs
    input_paths = []
    try:
        input_paths = parse_input_paths(args)
    except UserInputError as exception:
        print(exception)
        exit(2)

    # Open bite file and write to it!
    bite_path = Path(args.output)
    with open(bite_path, "wb") as bite:
        pack_bite(bite, input_paths, args.relative_to)
        _print(f"Generated \"{bite_path}\" successfully!")


if __name__ == "__main__":
    main()
