#!/usr/bin/env bash

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PROJECT_PATH="$ROOT_DIR/BGMApp/BGMApp.xcodeproj"
SCHEME_NAME="Background Music"
DERIVED_DATA_DIR="$HOME/Library/Developer/Xcode/DerivedData"
STABLE_APP_PATH="$HOME/Applications/Background Music Debug.app"
BUNDLE_ID="com.bearisdriving.BGM.App"
SAFE_AUDIO_ARG="--safe-audio-mode"
IINA_CLI="/Applications/IINA.app/Contents/MacOS/iina-cli"
IINA_SOUND="/System/Library/Sounds/Glass.aiff"
LOG_PREDICATE='process == "Background Music" AND (eventMessage CONTAINS[c] "BGMNeteaseMusic" OR eventMessage CONTAINS[c] "BGMAutoPauseMusic" OR eventMessage CONTAINS[c] "setBGMDeviceAsDefault")'

find_built_app() {
    find "$DERIVED_DATA_DIR" \
        -path '*/Build/Products/Debug/Background Music.app' \
        -type d \
        -print \
        | head -n 1
}

netease_menu_state() {
    osascript <<'APPLESCRIPT'
tell application "System Events"
  tell process "NeteaseMusic"
    tell menu 1 of menu bar item "Controls" of menu bar 1
      repeat with candidateItem in {"Pause", "Play"}
        if exists menu item (contents of candidateItem) then
          return name of menu item (contents of candidateItem)
        end if
      end repeat
    end tell
  end tell
end tell
APPLESCRIPT
}

current_audio_defaults() {
    python3 <<'PY'
import json
import subprocess
import sys

result = subprocess.run(
    ["system_profiler", "-json", "SPAudioDataType"],
    check=True,
    capture_output=True,
    text=True,
)
data = json.loads(result.stdout)
items = data.get("SPAudioDataType", [])
devices = items[0].get("_items", []) if items else []

default_output = None
default_system = None
for device in devices:
    name = device.get("_name")
    if device.get("coreaudio_default_audio_output_device") == "spaudio_yes":
        default_output = name
    if device.get("coreaudio_default_audio_system_device") == "spaudio_yes":
        default_system = name

print(f"default_output={default_output or ''}")
print(f"default_system={default_system or ''}")
PY
}

print_recent_logs() {
    /usr/bin/log show --style syslog --last 2m --predicate "$LOG_PREDICATE"
}

cleanup() {
    killall IINA 2>/dev/null || true
}

trap cleanup EXIT

echo "==> Building Debug app"
xcodebuild -project "$PROJECT_PATH" -scheme "$SCHEME_NAME" -configuration Debug

BUILT_APP="$(find_built_app || true)"
if [[ -z "${BUILT_APP:-}" ]]; then
    echo "Failed to locate built Debug app in DerivedData." >&2
    exit 1
fi

echo "==> Refreshing stable Debug app at: $STABLE_APP_PATH"
mkdir -p "$HOME/Applications"
rm -rf "$STABLE_APP_PATH"
ditto "$BUILT_APP" "$STABLE_APP_PATH"

echo "==> Resetting TCC entries for $BUNDLE_ID"
tccutil reset Accessibility "$BUNDLE_ID" || true
tccutil reset AppleEvents "$BUNDLE_ID" || true

echo "==> Opening Accessibility settings"
open "x-apple.systempreferences:com.apple.preference.security?Privacy_Accessibility"
cat <<EOF

Manual step required:
1. Add and enable this app in Accessibility:
   $STABLE_APP_PATH
2. Ensure Background Music is configured to auto-pause 网易云音乐.
3. Ensure 网易云音乐 is currently playing.
4. Press Enter here when ready.

EOF
read -r

echo "==> Restarting Background Music Debug"
killall "Background Music" 2>/dev/null || true
open -na "$STABLE_APP_PATH" --args "$SAFE_AUDIO_ARG"
sleep 3

echo "==> Checking current audio defaults"
audio_defaults="$(current_audio_defaults)"
echo "$audio_defaults"
default_output="$(printf '%s\n' "$audio_defaults" | sed -n 's/^default_output=//p')"
default_system="$(printf '%s\n' "$audio_defaults" | sed -n 's/^default_system=//p')"

if [[ "$default_output" != "Background Music" && "$default_system" != "Background Music" ]]; then
    echo "Expected Background Music to be selected as an audio default device after launch." >&2
    exit 1
fi

echo "==> Checking 网易云音乐 menu state before trigger"
before_state="$(netease_menu_state)"
echo "Netease state before trigger: $before_state"
if [[ "$before_state" != "Pause" ]]; then
    echo "Expected 网易云音乐 to be playing before test, but menu state is: $before_state" >&2
    exit 1
fi

if [[ ! -x "$IINA_CLI" ]]; then
    echo "IINA CLI not found at $IINA_CLI" >&2
    exit 1
fi

echo "==> Triggering competing audio with IINA"
"$IINA_CLI" --no-stdin --keep-running --mpv-loop-file=inf "$IINA_SOUND" >/tmp/codex-iina.log 2>&1 &
sleep 4

after_state="$(netease_menu_state)"
echo "Netease state after trigger: $after_state"

if [[ "$after_state" == "Play" ]]; then
    echo "PASS: Background Music paused 网易云音乐."
    exit 0
fi

echo "FAIL: 网易云音乐 was not paused."
echo
print_recent_logs
exit 1
