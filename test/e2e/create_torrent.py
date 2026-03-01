#!/usr/bin/env python3
"""Pure-Python .torrent file creator for aria2 e2e tests.

Usage:
  python3 create_torrent.py --file F --output T [--tracker URL] [--piece-length N]
  python3 create_torrent.py --dir D --output T [--tracker URL] [--piece-length N]
  python3 create_torrent.py --file F --output T --print-info-hash
"""

import argparse
import hashlib
import os
import sys


def bencode(obj):
    """Bencode a Python object (dict, list, int, bytes)."""
    if isinstance(obj, int):
        return b"i" + str(obj).encode("ascii") + b"e"
    if isinstance(obj, bytes):
        return str(len(obj)).encode("ascii") + b":" + obj
    if isinstance(obj, str):
        return bencode(obj.encode("utf-8"))
    if isinstance(obj, list):
        return b"l" + b"".join(bencode(item) for item in obj) + b"e"
    if isinstance(obj, dict):
        items = sorted(obj.items(), key=lambda kv: kv[0].encode("utf-8")
                       if isinstance(kv[0], str) else kv[0])
        encoded = b"d"
        for k, v in items:
            encoded += bencode(k) + bencode(v)
        return encoded + b"e"
    raise TypeError(f"unsupported type: {type(obj)}")


def create_torrent(filepath, tracker, piece_length):
    """Build a single-file .torrent metainfo dict."""
    with open(filepath, "rb") as f:
        data = f.read()

    pieces = b""
    for offset in range(0, len(data), piece_length):
        chunk = data[offset:offset + piece_length]
        pieces += hashlib.sha1(chunk).digest()

    info = {
        "name": os.path.basename(filepath),
        "piece length": piece_length,
        "pieces": pieces,
        "length": len(data),
    }
    torrent = {"info": info}
    if tracker:
        torrent["announce"] = tracker
    return torrent


def create_multifile_torrent(dirpath, tracker, piece_length):
    """Build a multi-file .torrent metainfo dict from a directory."""
    dirpath = os.path.abspath(dirpath)
    dirname = os.path.basename(dirpath)

    # Collect files sorted by path components
    file_list = []
    for root, _dirs, files in os.walk(dirpath):
        for fname in sorted(files):
            full = os.path.join(root, fname)
            relpath = os.path.relpath(full, dirpath)
            path_components = relpath.split(os.sep)
            size = os.path.getsize(full)
            file_list.append((path_components, size, full))
    file_list.sort(key=lambda x: x[0])

    # Build files list for info dict
    files_info = []
    for path_components, size, _full in file_list:
        files_info.append({
            "length": size,
            "path": path_components,
        })

    # Compute pieces by concatenating all files in order
    all_data = b""
    for _path_components, _size, full in file_list:
        with open(full, "rb") as f:
            all_data += f.read()

    pieces = b""
    for offset in range(0, len(all_data), piece_length):
        chunk = all_data[offset:offset + piece_length]
        pieces += hashlib.sha1(chunk).digest()

    info = {
        "name": dirname,
        "piece length": piece_length,
        "pieces": pieces,
        "files": files_info,
    }
    torrent = {"info": info}
    if tracker:
        torrent["announce"] = tracker
    return torrent


def info_hash(torrent):
    """Return the SHA1 hex digest of the bencoded info dict."""
    return hashlib.sha1(bencode(torrent["info"])).hexdigest()


def main():
    parser = argparse.ArgumentParser(
        description="Create a .torrent file for e2e tests")
    parser.add_argument("--file", default=None,
                        help="Input file to create torrent for")
    parser.add_argument("--dir", default=None,
                        help="Input directory for multi-file torrent")
    parser.add_argument("--output", required=True,
                        help="Output .torrent file path")
    parser.add_argument("--tracker", default=None,
                        help="Tracker announce URL")
    parser.add_argument("--piece-length", type=int, default=262144,
                        help="Piece length in bytes (default 256KB)")
    parser.add_argument("--print-info-hash", action="store_true",
                        help="Print SHA1 info hash to stdout")
    args = parser.parse_args()

    if args.dir:
        if not os.path.isdir(args.dir):
            print(f"error: dir not found: {args.dir}", file=sys.stderr)
            sys.exit(1)
        torrent = create_multifile_torrent(
            args.dir, args.tracker, args.piece_length)
    elif args.file:
        if not os.path.isfile(args.file):
            print(f"error: file not found: {args.file}", file=sys.stderr)
            sys.exit(1)
        torrent = create_torrent(
            args.file, args.tracker, args.piece_length)
    else:
        print("error: --file or --dir required", file=sys.stderr)
        sys.exit(1)

    with open(args.output, "wb") as f:
        f.write(bencode(torrent))

    if args.print_info_hash:
        print(info_hash(torrent))


if __name__ == "__main__":
    main()
