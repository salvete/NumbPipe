#!/usr/bin/env python3
"""NumbPipe blocking I/O test suite (requires ioctl support)."""
import os
import sys
import time
import fcntl
import subprocess
import signal

# ----------------------------------------------------------------------
# ioctl command codes (must match numb_common.h)
# ----------------------------------------------------------------------
_IOC_NONE = 0
_IOC_NRSHIFT = 0
_IOC_TYPESHIFT = 8
_IOC_SIZESHIFT = 16
_IOC_DIRSHIFT = 30

def _IO(magic, nr):
    return (_IOC_NONE << _IOC_DIRSHIFT) | (ord(magic) << _IOC_TYPESHIFT) | \
           (nr << _IOC_NRSHIFT) | (0 << _IOC_SIZESHIFT)

NUMBPIPE_IOC_MAGIC = 'N'
NUMBPIPE_SET_BLOCKING   = _IO('N', 1)
NUMBPIPE_UNSET_BLOCKING = _IO('N', 2)

def set_blocking(fd: int) -> None:
    """Mark current process as blocking for this device."""
    fcntl.ioctl(fd, NUMBPIPE_SET_BLOCKING)

def unset_blocking(fd: int) -> None:
    """Revert to non‑blocking for this process."""
    fcntl.ioctl(fd, NUMBPIPE_UNSET_BLOCKING)

# ----------------------------------------------------------------------
# Low‑level helpers
# ----------------------------------------------------------------------
def drain_nonblock(device: str) -> None:
    """Discard all data using non‑blocking reads."""
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

def read_nonblock_exact(device: str, n: int) -> bytes:
    """Read exactly n bytes in non‑blocking mode, stops on EAGAIN."""
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

# ----------------------------------------------------------------------
# Child process functions (called via sub‑command)
# ----------------------------------------------------------------------
def child_blocking_read(device: str, n: int) -> None:
    """Open device, set blocking, read exactly n bytes, write to stdout."""
    fd = os.open(device, os.O_RDONLY)
    set_blocking(fd)
    data = b""
    while len(data) < n:
        chunk = os.read(fd, n - len(data))
        if not chunk:
            break
        data += chunk
    os.close(fd)
    sys.stdout.buffer.write(data)

def child_blocking_write(device: str, data_bytes: bytes) -> None:
    """Open device, set blocking, write all data."""
    fd = os.open(device, os.O_WRONLY)
    set_blocking(fd)
    written = 0
    while written < len(data_bytes):
        n = os.write(fd, data_bytes[written:])
        if n <= 0:
            break
        written += n
    os.close(fd)

# ----------------------------------------------------------------------
# Test cases
# ----------------------------------------------------------------------
def test_blocking_read_wakeup(device: str) -> None:
    """Test 1: Blocking read – process sleeps until data arrives."""
    drain_nonblock(device)

    # Start a child that will do a blocking read of 5 bytes
    proc = subprocess.Popen(
        [sys.executable, __file__, device, "child_read", "5"],
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
    )

    time.sleep(0.5)  # Let it enter the blocking read

    # Write the expected data
    with open(device, "wb") as f:
        f.write(b"HELLO")

    stdout, stderr = proc.communicate(timeout=2)
    if proc.returncode != 0:
        sys.exit(f"FAIL: child exited with {proc.returncode}, stderr: {stderr.decode()}")
    if stdout != b"HELLO":
        sys.exit(f"FAIL: expected b'HELLO', got {stdout!r}")
    drain_nonblock(device)
    print("Test 1 (blocking read) PASS")


def test_blocking_write_wakeup(device: str) -> None:
    """Test 2: Blocking write – process sleeps until buffer has space."""
    drain_nonblock(device)

    # Fill buffer (4096 bytes)
    with open(device, "wb") as f:
        f.write(b"Z" * 4096)

    # Start child that tries to write 10 bytes (will block)
    proc = subprocess.Popen(
        [sys.executable, __file__, device, "child_write", "1234567890"],
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
    )

    time.sleep(0.5)

    # Read 10 bytes to free space
    read_nonblock_exact(device, 10)

    stdout, stderr = proc.communicate(timeout=2)
    if proc.returncode != 0:
        sys.exit(f"FAIL: child write process failed, rc={proc.returncode}, stderr={stderr.decode()}")

    # Verify the child's data ended up in the buffer
    remaining = read_nonblock_exact(device, 4096)
    if b"1234567890" not in remaining[-10:]:
        sys.exit(f"FAIL: written data not found in buffer, got {remaining[-10:]!r}")

    drain_nonblock(device)
    print("Test 2 (blocking write) PASS")


def test_exclusive_wakeup(device: str) -> None:
    """Test 3: Multiple blocking readers – only one woken per write."""
    drain_nonblock(device)

    # Launch two readers asking for 1 byte each
    p1 = subprocess.Popen(
        [sys.executable, __file__, device, "child_read", "1"],
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
    )
    p2 = subprocess.Popen(
        [sys.executable, __file__, device, "child_read", "1"],
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
    )

    time.sleep(0.5)

    # Write 1 byte
    with open(device, "wb") as f:
        f.write(b"X")

    time.sleep(0.5)
    finished = [p for p in [p1, p2] if p.poll() is not None]
    if len(finished) != 1:
        sys.exit(f"FAIL: expected exactly 1 reader woken, got {len(finished)}")

    stdout, _ = finished[0].communicate()
    if stdout != b"X":
        sys.exit(f"FAIL: woken reader got {stdout!r}")

    # Write a second byte to wake the other
    with open(device, "wb") as f:
        f.write(b"Y")

    unfinished = [p for p in [p1, p2] if p.poll() is None]
    for p in unfinished:
        stdout, _ = p.communicate(timeout=2)
        if stdout != b"Y":
            sys.exit(f"FAIL: second reader got {stdout!r}")

    drain_nonblock(device)
    print("Test 3 (exclusive wakeup) PASS")


def test_unset_blocking(device: str) -> None:
    """Test 4: Unset blocking – read returns EAGAIN instead of sleeping."""
    drain_nonblock(device)

    # Open with O_NONBLOCK so we can observe EAGAIN directly
    fd = os.open(device, os.O_RDONLY | os.O_NONBLOCK)
    set_blocking(fd)    # now blocking mode in driver
    unset_blocking(fd)  # back to non‑blocking

    try:
        os.read(fd, 1)  # buffer is empty → should raise BlockingIOError
        os.close(fd)
        sys.exit("FAIL: expected BlockingIOError after unset")
    except BlockingIOError:
        pass

    os.close(fd)
    drain_nonblock(device)
    print("Test 4 (unset blocking) PASS")


# ----------------------------------------------------------------------
# Main dispatcher
# ----------------------------------------------------------------------
def main() -> None:
    if len(sys.argv) < 2:
        print("Usage: blocking_test.py <device> [child_read <n> | child_write <data>]", file=sys.stderr)
        sys.exit(1)

    device = sys.argv[1]

    # Sub‑command mode for child processes
    if len(sys.argv) >= 3:
        cmd = sys.argv[2]
        if cmd == "child_read":
            n = int(sys.argv[3])
            child_blocking_read(device, n)
            return
        elif cmd == "child_write":
            data_bytes = sys.argv[3].encode()
            child_blocking_write(device, data_bytes)
            return
        else:
            print(f"Unknown sub‑command: {cmd}", file=sys.stderr)
            sys.exit(1)

    # Run tests
    print("=== NumbPipe Blocking I/O Tests ===")
    test_blocking_read_wakeup(device)
    test_blocking_write_wakeup(device)
    test_exclusive_wakeup(device)
    test_unset_blocking(device)
    print("\nAll blocking tests passed.")


if __name__ == "__main__":
    main()