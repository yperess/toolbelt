#!/bin/sh

# This script opens the given file path in the default system browser.
# It's designed to be run by 'bazel run'.

FILE_TO_OPEN="$1"

if [ -z "$FILE_TO_OPEN" ]; then
  echo "Error: No file path provided."
  exit 1
fi

case "$(uname -s)" in
  # macOS
  Darwin)
    echo "Detected macOS. Using 'open'..."
    # 'open' on macOS typically detaches by default.
    open "$FILE_TO_OPEN"
    ;;
  # Linux
  Linux)
    echo "Detected Linux. Using 'xdg-open' in the background..."
    # --- THIS IS THE MODIFIED LINE ---
    # Run xdg-open in the background (&) and prevent it from being
    # terminated when the shell closes (nohup).
    # Also, redirect stdout and stderr to /dev/null.
    nohup xdg-open "$FILE_TO_OPEN" >/dev/null 2>&1 &
    ;;
  # Windows (Git Bash, Cygwin, MSYS)
  CYGWIN*|MINGW*|MSYS*)
    echo "Detected Windows. Using 'start'..."
    # 'start' on Windows is designed to not wait for the app to close.
    start "" "$FILE_TO_OPEN"
    ;;
  *)
    echo "Unsupported operating system: $(uname -s)"
    exit 1
    ;;
esac

# This message will now appear immediately.
echo "Browser launch command sent. 'bazel run' should now exit."