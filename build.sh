#!/usr/bin/env bash
set -euo pipefail
project_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
if [[ -n "${ARDUINO_CLI:-}" ]]; then
  cli="$ARDUINO_CLI"
elif command -v arduino-cli >/dev/null 2>&1; then
  cli="$(command -v arduino-cli)"
else
  cli="/Applications/Arduino IDE.app/Contents/Resources/app/lib/backend/resources/arduino-cli"
fi
build_dir="$project_dir/.build"
"$cli" compile --fqbn esp32:esp32:esp32:PartitionScheme=huge_app,FlashFreq=40,FlashMode=dio,UploadSpeed=115200 \
  --build-path "$build_dir" \
  --build-property "compiler.cpp.extra_flags=-DUSER_SETUP_LOADED -include \"$project_dir/TaskPop_Display.h\"" \
  "$project_dir"
if [[ -n "${1:-}" ]]; then
  "$cli" upload --fqbn esp32:esp32:esp32:PartitionScheme=huge_app,FlashFreq=40,FlashMode=dio,UploadSpeed=115200 \
    --port "$1" --input-dir "$build_dir" "$project_dir"
fi
