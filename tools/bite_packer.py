"""
Wad-like file packer
"""

import argparse
import struct
from pathlib import Path


# ==========================
# Core
# ==========================

def input_validate(in_paths: list[str]):
    """Checks file input to see if they are actual valid files or not"""

    for path in in_paths:
        print(path)


def process_file(in_path: Path):
    """Opens a file and returns its data to be stored."""

    file = open(in_path.as_posix(), "rb")
    bytes = file.read()
    return bytes


def write_header(
        out_file,
        header_version=1,
        file_entries_offset=0,
        file_entries_count=0
):
    """Writes the wad-like header into the output file.
    When called without any arguments (except for the file handle),
    it will print a stub header.
    """

    # magic (4 bytes, ascii)
    magic = b"BITE"
    out_file.write(magic)

    # version (2 bytes)
    write_packed(out_file, "<H", header_version)

    # offset (4 bytes)
    write_packed(out_file, "<I", file_entries_offset)

    # number of files (4 bytes)
    write_packed(out_file, "<I", file_entries_count)


def write_packed(
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
        write_packed(out_file, "b", 0)


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

            # Check file stem to see if it's in snake_case using RegEx.
            # regex_match = re.fullmatch(
            #     "[a-z]+(_[a-z]+)*", Path(in_path).stem()
            # )
            # if regex_match is None:
            #     print(f"Warning: {in_path.as_posix()} is not in snake_case.")
            #     print("Please consider renaming it.")

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
            # Write offset + data size (4 bytes + 4 bytes)
            write_packed(out, "<II", file_entry["offset"], file_entry["size"])

            # Reserved data (4 bytes)
            write_packed(out, "<I", 0)

            # Filename length + data (4 bytes + variable length)
            encoded_name = file_entry["name"].encode('utf-8')
            write_packed(out, "<H", len(encoded_name))
            out.write(encoded_name)

        # Update header w/ new info
        out.seek(0, 0)
        write_header(
            out,
            file_entries_count=len(file_metadata_entries),
            file_entries_offset=file_metadata_offset,
        )


if __name__ == "__main__":
    main()
