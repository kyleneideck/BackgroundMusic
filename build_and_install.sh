#!/bin/bash
# vim: tw=100:

# This file is part of Background Music.
#
# Background Music is free software: you can redistribute it and/or
# modify it under the terms of the GNU General Public License as
# published by the Free Software Foundation, either version 2 of the
# License, or (at your option) any later version.
#
# Background Music is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with Background Music. If not, see <http://www.gnu.org/licenses/>.

#
# build_and_install.sh
#
# Copyright © 2016 Kyle Neideck
#
# Builds and installs BGMApp, BGMDriver and BGMXPCHelper. Requires xcodebuild.
#

# Safe mode
set -euo pipefail
IFS=$'\n\t'

# General error message
set -o errtrace
general_error() {
    echo "$(tput setaf 1)ERROR$(tput sgr0): Install script failed at line $1. This is probably a" \
         "bug in the script. Feel free to report it." >&2
}
trap 'general_error ${LINENO}' ERR

# Build for release by default.
# TODO: Add an option to use the debug configuration?
CONFIGURATION=Release
#CONFIGURATION=Debug

# Update .gitignore if you change this.
LOG_FILE=build_and_install.log

bold_face() {
    echo $(tput bold)$*$(tput sgr0)
}

# Takes a PID and returns 0 if the process is running.
is_alive() {
    kill -0 $1 > /dev/null 2>&1 && return 0 || return 1
}

# Shows a "..." animation until the previous command finishes. Shows an error message and exits the
# script if the command fails.
#
# Takes an optional timeout in seconds. The return value will be the exit status of the command.
show_spinner() {
    local PREV_COMMAND_PID=$!

    # Get the previous command as a string, with variables resolved. Assumes that if the command has
    # a child process we just want the text of the child process's command. (And that it only has
    # one child.)
    local CHILD_PID=$(pgrep -P ${PREV_COMMAND_PID} | head -n1 || echo ${PREV_COMMAND_PID})
    local PREV_COMMAND_STRING=$(ps -o command= ${CHILD_PID})
    local TIMEOUT=${1:-0}

    (I=1;
    while (is_alive ${PREV_COMMAND_PID}) && ([[ ${TIMEOUT} -lt 1 ]] || [[ $I -lt ${TIMEOUT} ]]); do
        printf '.';
        sleep 1;
        # Erase after we've printed three dots. (\b is backspace.)
        [[ $(($I % 3)) -eq 0 ]] && printf '\b\b\b   \b\b\b';
        I=$(($I + 1));
    done) &

    set +e
    wait ${PREV_COMMAND_PID}
    local EXIT_STATUS=$?
    set -e

    # Clean up the dots.
    printf '\b\b\b'

    # Print an error message if the command fails.
    # (wait returns 127 if the process has already exited.)
    if [[ ${EXIT_STATUS} -ne 0 ]] && [[ ${EXIT_STATUS} -ne 127 ]]; then
        echo "$(tput setaf 1)ERROR$(tput sgr0): Build step failed. See ${LOG_FILE} for details." >&2
        echo "Failed command:" >&2
        echo "    ${PREV_COMMAND_STRING}" >&2

        exit ${EXIT_STATUS}
    fi

    return ${EXIT_STATUS}
}

# Check for xcodebuild.
if [[ "$(which xcodebuild)" == "" ]]; then
    echo "$(tput setaf 1)ERROR$(tput sgr0): Can't find xcodebuild in your \$PATH." >&2
    echo >&2
    echo "If you have Xcode installed, you should be able to install the command line developer" \
         "tools, including xcodebuild, with" >&2
    echo "    xcode-select --install" >&2
    echo "If not, you'll need to install Xcode (~9GB), because xcodebuild no longer works without" \
         "it." >&2

    # Disable error handlers.
    trap - ERR
    set +e 

    # Check for Xcode.
    XCODE_PATH=$(which xcode-select > /dev/null && xcode-select --print-path)
    XCODE_PATH=${XCODE_PATH%/Contents/Developer}

    if [[ "${XCODE_PATH}" == "" ]] && [[ -d /Applications/Xcode.app ]]; then
        XCODE_PATH="/Applications/Xcode.app"
    fi

    if [[ "${XCODE_PATH}" == "" ]] && [[ -d ~/Applications/Xcode.app ]]; then
        XCODE_PATH="~/Applications/Xcode.app"
    fi

    if [[ "${XCODE_PATH}" != "" ]]; then
        echo >&2
        echo "It looks like you have Xcode installed to ${XCODE_PATH}" >&2
    fi

    exit 1
fi

# Go to the project directory.
cd "$( dirname "${BASH_SOURCE[0]}" )"

# BGMDriver

echo "Installing the virtual audio device $(bold_face Background Music Device.driver) to" \
     "$(bold_face /Library/Audio/Plug-Ins/HAL)." \
     | tee ${LOG_FILE}

sudo -v

# Disable the -e shell option here so we can handle the error differently.
(set +e;
sudo xcodebuild -project BGMDriver/BGMDriver.xcodeproj \
                -target "Background Music Device" \
                -configuration ${CONFIGURATION} \
                RUN_CLANG_STATIC_ANALYZER=0 \
                DSTROOT="/" \
                install >> ${LOG_FILE} 2>&1) &

show_spinner

# BGMXPCHelper

INSTALL_DIR=$(BGMApp/BGMXPCHelper/safe_install_dir.sh)

echo "Installing $(bold_face BGMXPCHelper.xpc) to $(bold_face ${INSTALL_DIR})." \
     | tee -a ${LOG_FILE}

(set +e;
sudo xcodebuild -project BGMApp/BGMApp.xcodeproj \
                -target BGMXPCHelper \
                -configuration ${CONFIGURATION} \
                RUN_CLANG_STATIC_ANALYZER=0 \
                DSTROOT="/" \
                INSTALL_PATH="${INSTALL_DIR}" \
                install >> ${LOG_FILE} 2>&1) &

show_spinner

# BGMApp

echo "Installing $(bold_face Background Music.app) to $(bold_face /Applications)." \
     | tee -a ${LOG_FILE}

(set +e;
sudo xcodebuild -project BGMApp/BGMApp.xcodeproj \
                -target "Background Music" \
                -configuration ${CONFIGURATION} \
                RUN_CLANG_STATIC_ANALYZER=0 \
                DSTROOT="/" \
                install >> ${LOG_FILE} 2>&1) &

show_spinner

# Fix Background Music.app owner/group.
# (We have to run xcodebuild as root to install BGMXPCHelper because it installs to directories
# owned by root. But that means the build directory gets created by root, and since BGMApp uses the
# same build directory we have to run xcodebuild as root to install BGMApp as well.)
sudo chown -R $(whoami):admin "/Applications/Background Music.app"

# Restart coreaudiod.

echo "Restarting coreaudiod to load BGMDriver." \
     | tee -a ${LOG_FILE}

sudo launchctl kill SIGTERM system/com.apple.audio.coreaudiod

# Open BGMApp.
# I'd rather not open BGMApp here, or at least ask first, but you have to change your default audio
# device after restarting coreaudiod and this is the easiest way.
echo "Launching Background Music."

open "/Applications/Background Music.app"

# Wait up to 5 seconds for Background Music to start.
(while [[ "$(ps -u $(whoami) -o ucomm= | grep 'Background Music')" == "" ]]; do
    sleep 1;
done) &
show_spinner 5

echo "Done."


