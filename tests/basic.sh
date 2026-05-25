#!/bin/bash
# NumbPipe test environment setup and cleanup
# Usage: sudo bash tests/basic.sh

set -eo pipefail

MODULE="numbpipe"
DEVICE="/dev/numb_pipe"
MAJOR=199
MINOR=0

# Cleanup function (called on exit)
cleanup() {
    echo "=== Cleaning up ==="
    rmmod "$MODULE" 2>/dev/null || true
    rm -f "$DEVICE"
}
trap cleanup EXIT

# Remove leftovers
rmmod "$MODULE" 2>/dev/null || true
rm -f "$DEVICE"

# Load module
echo "=== Loading module ==="
insmod "${MODULE}.ko"
dmesg | tail -5

# Create device node
echo "=== Creating device node ==="
mknod "$DEVICE" c "$MAJOR" "$MINOR"
chmod 666 "$DEVICE"
ls -l "$DEVICE"

# Run the test suite
echo "=== Running tests ==="
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
python3 "$SCRIPT_DIR/basic.py" "$DEVICE"
TEST_EXIT=$?

if [ $TEST_EXIT -ne 0 ]; then
    echo "Tests failed with exit code $TEST_EXIT"
    exit $TEST_EXIT
fi

echo "=== All tests completed successfully ==="
