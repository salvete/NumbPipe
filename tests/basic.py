import os
import sys


def drain(device: str) -> None:
    """Read and discard all available data (nonblocking)."""
    fd = os.open(device, os.O_RDONLY | os.O_NONBLOCK)
    try:
        while True:
            data = os.read(fd, 4096)
            if not data:
                break
    except BlockingIOError:
        pass
    finally:
        os.close(fd)


def read_exact(device: str, n: int) -> bytes:
    """Read exactly n bytes (nonblocking), stops on EAGAIN."""
    fd = os.open(device, os.O_RDONLY | os.O_NONBLOCK)
    try:
        data = b""
        while len(data) < n:
            try:
                chunk = os.read(fd, n - len(data))
                if not chunk:
                    break
                data += chunk
            except BlockingIOError:
                break
        return data
    finally:
        os.close(fd)


def write_all(device: str, data: bytes) -> int:
    """
    Write data to device, handling partial writes.
    Stops when all data is written or write returns EAGAIN (buffer full).
    Returns number of bytes written.
    """
    fd = os.open(device, os.O_WRONLY)
    written = 0
    try:
        while written < len(data):
            try:
                n = os.write(fd, data[written:])
                if n == 0:
                    break
                written += n
            except BlockingIOError:
                # No more space, stop here
                break
    finally:
        os.close(fd)
    return written


def test_basic_read_write(device: str) -> None:
    """Test 1: basic read/write consistency."""
    drain(device)

    test_str = b"Hello NumbPipe!\n"
    written = write_all(device, test_str)
    if written != len(test_str):
        sys.exit(f"FAIL Test 1: could not write all data, wrote {written}")

    result = read_exact(device, len(test_str))
    if result != test_str:
        sys.exit(f"FAIL Test 1: expected {test_str!r}, got {result!r}")

    drain(device)
    print("Test 1 PASS")


def test_boundary(device: str) -> None:
    """Test 2: exact buffer capacity (4096 bytes)."""
    drain(device)

    data = b"A" * 4096
    written = write_all(device, data)
    if written != 4096:
        sys.exit(f"FAIL Test 2: expected to write 4096, wrote {written}")

    result = read_exact(device, 4096)
    if len(result) != 4096 or result != data:
        sys.exit(f"FAIL Test 2: content mismatch")

    drain(device)
    print("Test 2 PASS")


def test_overflow(device: str) -> None:
    """Test 3: overflow handling – write 5000 bytes, expect only 4096 readable."""
    drain(device)

    data = b"B" * 5000
    written = write_all(device, data)
    # We expect the driver to accept only 4096 bytes, then return EAGAIN
    if written != 4096:
        sys.exit(f"FAIL Test 3: expected to write 4096 bytes, but wrote {written}")

    result = read_exact(device, 4096)
    if len(result) != 4096:
        sys.exit(f"FAIL Test 3: expected 4096 bytes, got {len(result)}")

    expected = b"B" * 4096
    if result != expected:
        sys.exit(f"FAIL Test 3: content mismatch")

    drain(device)
    print("Test 3 PASS")


def main() -> None:
    if len(sys.argv) != 2:
        print("Usage: basic.py <device>", file=sys.stderr)
        sys.exit(1)

    device = sys.argv[1]

    test_basic_read_write(device)
    test_boundary(device)
    test_overflow(device)

    print("All tests passed.")


if __name__ == "__main__":
    main()