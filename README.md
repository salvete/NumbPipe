# NumbPipe

**A character device-based ring buffer pipe with support for blocking and non-blocking I/O.**

[![License: GPL v2 only](https://img.shields.io/badge/License-GPL%20v2%20only-blue.svg)](https://www.gnu.org/licenses/old-licenses/gpl-2.0.en.html)

## Introduction

`NumbPipe` is a Linux kernel module that creates a character device (default: `/dev/numb_pipe`) providing a low-performance pipe backed by a ring buffer in user space.

Unlike standard Unix pipes, `NumbPipe` is exposed through a device file and additionally supports `ioctl` to dynamically toggle between **blocking** and **non-blocking** modes for specific processes. It also exposes internal state via `sysfs`, making it ideal for scenarios that require fine-grained control over I/O behavior.

## Usage Examples

### Basic Read/Write
```bash
# Write data
echo "Hello, NumbPipe!" > /dev/numb_pipe

# Read data
cat /dev/numb_pipe
# Output: Hello, NumbPipe!
```

### Inspect Buffer State
```bash
# View current head and tail positions
cat /sys/class/numb_pipe/numb_pipe0/head
cat /sys/class/numb_pipe/numb_pipe0/tail
```

### Blocking and Non-Blocking Modes
By default, processes use non-blocking I/O on the device. You can use Python with `ioctl` to switch a specific process to blocking mode (the module records which PIDs should block).
```c
import os
import fcntl

# ioctl command codes (must match definitions in numb_common.h)
NUMBPIPE_IOC_MAGIC = ord('N')
NUMBPIPE_SET_BLOCKING   = (0 << 30) | (NUMBPIPE_IOC_MAGIC << 8) | (1 << 0) | (0 << 16)
NUMBPIPE_UNSET_BLOCKING = (0 << 30) | (NUMBPIPE_IOC_MAGIC << 8) | (2 << 0) | (0 << 16)

fd = os.open("/dev/numb_pipe", os.O_RDONLY)

# Enable blocking mode
fcntl.ioctl(fd, NUMBPIPE_SET_BLOCKING)

# read() will now block until data is available
try:
    data = os.read(fd, 1024)  # This call blocks
except KeyboardInterrupt:
    pass

# Switch back to non-blocking mode
fcntl.ioctl(fd, NUMBPIPE_UNSET_BLOCKING)
os.close(fd)
```
See `tests/blocking_test.py` for a complete working example.

## License

This project is licensed under the GNU General Public License v2.0 only. See the SPDX identifiers in the source files for details.
