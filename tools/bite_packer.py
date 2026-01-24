"""
Bite file packer
"""

import argparse
import struct
from pathlib import Path


# ==========================
# Core
# ==========================

def process_file(in_path: Path):
    """Opens a file and returns its data to be stored."""

    file = open(in_path.as_posix(), "rb")
    bytes = file.read()
    return bytes


def write_header(
        out_file,
        header_version=1,
        file_entries_offset=0,
        file_entries_count=0,
        file_data_start_offset=0
):
    """Writes the bite header into the output file.
    When called without any arguments (except for the file handle),
    it will print a stub header.
    """

    # magic (4 bytes, ascii)
    magic = b"BITE"
    out_file.write(magic)

    # version (2 bytes)
    write_struct(out_file, "<H", header_version)

    # reserved (2 bytes)
    write_struct(out_file, "<H", 0)

    # file table offset & entry count (8+4 bytes)
    write_struct(out_file, "<QI", file_entries_offset, file_entries_count)

    # file data start offset (8 bytes)
    write_struct(out_file, "<Q", file_data_start_offset)

    # reserved (4 bytes)
    write_struct(out_file, "<I", 0)


def write_struct(
        out_file,
        fmt: str,
        *values
):
    """Writes a struct template to output file"""

    if ">" not in fmt and "<" not in fmt:
        fmt = "<" + fmt  # Ensure little endian

    out_file.write(
        struct.pack(fmt, *values)
    )


def write_padding(
        out_file,
        alignment=16,
):
    """Writes empty padding until the file cursor is on a block alignment"""

    pad = alignment - (out_file.tell() % alignment)
    pad = pad % alignment
    for i in range(pad):
        write_struct(out_file, "b", 0)


def parser_build():
    """Builds an argparse object containing all relevant cli data"""

    parser = argparse.ArgumentParser(
        prog="wadlike",
        description="Packs multiple files into one monolith wad-like file.",
    )
    parser.add_argument(
        "input",
        action="extend",
        nargs="+",
        default=[],
    )
    parser.add_argument(
        "-a", "--alignment",
        default=16,
    )
    parser.add_argument(
        "-o", "--output",
        required=True
    )
    return parser


# ==========================
# Entrypoint
# ==========================

def main():
    """
    Entrypoint
    """
    parser = parser_build()
    args = parser.parse_args()

    # Ensure .bite extension for final name
    # "file" -> "file.bite"
    # "file.bite" -> "file.bite"
    out_path: str = args.output
    out_path = out_path.removesuffix(".bite") + ".bite"

    with open(out_path, "wb") as out:
        file_metadata_offset = 0
        file_metadata_entries = []

        write_header(out)  # Write stub header

        for in_path in args.input:
            write_padding(out, args.alignment)

            data_offset = out.tell()
            data_size = 0

            data = process_file(Path(in_path))
            data_size = len(data)

            if data_size > 0:
                out.write(data)
            else:
                # File is empty, there's no allocated data
                data_offset = 0

            entry = {
                "name":   Path(in_path).as_posix(),
                "offset": data_offset,
                "size":   data_size,
            }

            print(entry)
            file_metadata_entries.append(entry)

        # File metadata table
        write_padding(out, args.alignment)
        file_metadata_offset = out.tell()
        for file_entry in file_metadata_entries:
            # file_data_pos = file_entry["offset"]

            # Write offset + data size (8 bytes + 8 bytes)
            write_struct(out, "<Q", file_entry["offset"])
            write_struct(out, "<Q", file_entry["size"])

            # Reserved data (4 bytes)
            write_struct(out, "<I", 0)

            # Filename length + data (1 byte + N bytes)
            encoded_name = file_entry["name"].encode('utf-8')
            write_struct(out, "<B", len(encoded_name))
            out.write(encoded_name)

        # Update header w/ new info
        data_start_offset = 0
        if len(file_metadata_entries) > 0:
            data_start_offset = file_metadata_entries[0]["offset"]
        else:
            data_start_offset = file_metadata_offset

        out.seek(0, 0)
        write_header(
            out,
            file_entries_count=len(file_metadata_entries),
            file_entries_offset=file_metadata_offset,
            file_data_start_offset=data_start_offset,
        )


if __name__ == "__main__":
    main()
