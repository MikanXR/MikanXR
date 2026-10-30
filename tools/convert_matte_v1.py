"""Convert occlusion matte sidecars from version 1 to version 2 in place.

Version 1 stored every record at two bits per pixel, about 20 MB per second of
take. Version 2 run-length encodes each record behind an index, which is what
MikanXR reads now, so a version 1 sidecar is ignored until it is converted or
rebuilt. Converting keeps the labels exactly and skips the segmenter.

    python tools/convert_matte_v1.py <take.matte.json> [<take.matte.json> ...] [--verify]

The new files are written beside the old ones and moved over them, the bin first
so that a json in place always has its bin. --verify decodes every converted
record and compares it with the version 1 labels before anything is replaced.

Needs numpy.
"""

import argparse
import json
import os
import sys

import numpy as np

FORMAT_NAME = "mikan-matte-track"
KEEP = 1
ADDED = 2
INDEX_ENTRY_BYTES = 12


def unpack_v1(packed, width, height):
    """One version 1 record (two bits per pixel, least significant first, rows
    padded to a byte) as a flat uint8 array of labels, 3 read as keep."""
    row_bytes = (width + 3) // 4
    rows = np.frombuffer(packed, dtype=np.uint8).reshape(height, row_bytes)
    labels = np.empty((height, row_bytes * 4), dtype=np.uint8)
    for pixel in range(4):
        labels[:, pixel::4] = (rows >> (2 * pixel)) & 3
    labels = labels[:, :width].reshape(-1)
    labels[labels > ADDED] = KEEP
    return labels


def encode_v2(labels):
    """A flat label array as one version 2 record: runs crossing row ends, each a
    first byte of label (bits 7-6), continue (bit 5) and the low 5 bits of
    length - 1, then 7 bits per byte with continue in bit 7."""
    change = np.ones(labels.size, dtype=bool)
    change[1:] = labels[1:] != labels[:-1]
    starts = np.flatnonzero(change)
    lengths_minus_one = np.diff(np.append(starts, labels.size)).astype(np.uint64) - 1
    run_labels = labels[starts].astype(np.uint8)

    # Bytes per run: the first carries 5 bits, each after it 7
    byte_counts = np.ones(starts.size, dtype=np.int64)
    remaining = lengths_minus_one >> np.uint64(5)
    while np.any(remaining):
        byte_counts += remaining > 0
        remaining >>= np.uint64(7)

    run_offsets = np.concatenate(([0], np.cumsum(byte_counts)[:-1]))
    encoded = np.zeros(int(byte_counts.sum()), dtype=np.uint8)

    first = (run_labels << 6) | (lengths_minus_one & np.uint64(0x1F)).astype(np.uint8)
    first[byte_counts > 1] |= 0x20
    encoded[run_offsets] = first

    remaining = lengths_minus_one >> np.uint64(5)
    for level in range(1, int(byte_counts.max())):
        has_byte = byte_counts > level
        value = (remaining[has_byte] & np.uint64(0x7F)).astype(np.uint8)
        value[byte_counts[has_byte] > level + 1] |= 0x80
        encoded[run_offsets[has_byte] + level] = value
        remaining >>= np.uint64(7)

    return encoded.tobytes()


def decode_v2(encoded, pixel_count):
    """One version 2 record as a flat label array, or None when it is malformed."""
    run_labels = []
    run_lengths = []
    position = 0
    total = 0
    while position < len(encoded):
        first = encoded[position]
        position += 1
        label = first >> 6
        if label > ADDED:
            return None
        length_minus_one = first & 0x1F
        more = first & 0x20
        shift = 5
        while more:
            if position >= len(encoded):
                return None
            next_byte = encoded[position]
            position += 1
            length_minus_one |= (next_byte & 0x7F) << shift
            shift += 7
            more = next_byte & 0x80
        total += length_minus_one + 1
        if total > pixel_count:
            return None
        run_labels.append(label)
        run_lengths.append(length_minus_one + 1)
    if total != pixel_count:
        return None
    return np.repeat(np.array(run_labels, dtype=np.uint8), run_lengths)


def convert(json_path, verify):
    with open(json_path, "r", encoding="utf-8") as json_file:
        header = json.load(json_file)

    if header.get("format") != FORMAT_NAME:
        print(f"{json_path}: not a matte track")
        return False
    if header.get("version") != 1:
        print(f"{json_path}: version {header.get('version')}, not 1")
        return False

    width = header["matte_width"]
    height = header["matte_height"]
    record_count = header["record_count"]
    record_bytes = ((width + 3) // 4) * height
    bin_path = os.path.splitext(json_path)[0] + ".bin"
    if os.path.getsize(bin_path) != record_count * record_bytes:
        print(f"{bin_path}: {os.path.getsize(bin_path)} bytes, expected {record_count * record_bytes}")
        return False

    v1 = np.memmap(bin_path, dtype=np.uint8, mode="r", shape=(record_count, record_bytes))
    index = np.zeros(record_count, dtype=[("offset", "<u8"), ("size", "<u4")])
    payload = []
    payload_bytes = record_count * INDEX_ENTRY_BYTES
    last_encoded = None
    for record_index in range(record_count):
        encoded = encode_v2(unpack_v1(v1[record_index].tobytes(), width, height))
        # A frame step holds the last labels over the frames between
        if encoded != last_encoded:
            payload.append(encoded)
            last_offset = payload_bytes
            payload_bytes += len(encoded)
            last_encoded = encoded
        index[record_index] = (last_offset, len(encoded))

    bin_temp_path = bin_path + ".tmp"
    json_temp_path = json_path + ".tmp"
    with open(bin_temp_path, "wb") as bin_file:
        bin_file.write(index.tobytes())
        for encoded in payload:
            bin_file.write(encoded)

    header["version"] = 2
    with open(json_temp_path, "w", encoding="utf-8", newline="\n") as json_file:
        json.dump(header, json_file, indent="\t")

    if verify:
        data = open(bin_temp_path, "rb").read()
        for record_index in range(record_count):
            offset, size = index[record_index]
            decoded = decode_v2(data[int(offset) : int(offset) + int(size)], width * height)
            expected = unpack_v1(v1[record_index].tobytes(), width, height)
            if decoded is None or not np.array_equal(decoded, expected):
                print(f"{json_path}: record {record_index} does not survive the conversion")
                del v1
                os.remove(bin_temp_path)
                os.remove(json_temp_path)
                return False

    del v1
    os.replace(bin_temp_path, bin_path)
    os.replace(json_temp_path, json_path)
    v1_bytes = record_count * record_bytes
    print(f"{json_path}: {record_count} records, {v1_bytes / 1e6:.1f} MB to {payload_bytes / 1e6:.2f} MB"
          + (", verified" if verify else ""))
    return True


def main():
    parser = argparse.ArgumentParser(description="Convert occlusion matte sidecars from version 1 to version 2.")
    parser.add_argument("json_paths", nargs="+", help="<take>.matte.json files to convert in place")
    parser.add_argument("--verify", action="store_true", help="decode every record and compare before replacing")
    args = parser.parse_args()

    ok = True
    for json_path in args.json_paths:
        ok &= convert(json_path, args.verify)
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
