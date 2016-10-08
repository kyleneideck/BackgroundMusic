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
# Copyright © 2016 Nick Jacques
#
# Builds and installs BGMApp, BGMDriver and BGMXPCHelper. Requires xcodebuild and Xcode.
#

# Safe mode
set -euo pipefail
IFS=$'\n\t'

# Subshells and function inherit the ERR trap
set -o errtrace

# Go to the project directory.
cd "$( dirname "${BASH_SOURCE[0]}" )"

error_handler() {
    local LAST_COMMAND="${BASH_COMMAND}" LAST_COMMAND_EXIT_STATUS=$?

    # Log the error.
    echo "Failure in ${0} at line ${1}. The last command was (probably)" >> ${LOG_FILE}
    echo "    ${LAST_COMMAND}" >> ${LOG_FILE}
    echo "which exited with status ${LAST_COMMAND_EXIT_STATUS}." >> ${LOG_FILE}
    echo "Error message: ${ERROR_MSG}" >> ${LOG_FILE}
    echo >> ${LOG_FILE}

    # Scrub username from log (and also real name just in case).
    sed -i'tmp' "s/$(whoami)/[username removed]/g" ${LOG_FILE}
    sed -i'tmp' "s/$(id -F)/[name removed]/g" ${LOG_FILE}
    rm "${LOG_FILE}tmp"

    # Print an error message.
    echo "$(tput setaf 9)ERROR$(tput sgr0): Install failed at line $1 with the message:"
    echo
    echo -e "${ERROR_MSG}" >&2
    echo >&2
    echo "Feel free to report this. If you do, you'll probably want to include the" \
         "build_and_install.log file from this directory ($(pwd)). But quickly skim through it" \
         "first to check that it doesn't include any personal information. It shouldn't, but this" \
         "is alpha software so you never know." >&2
    echo >&2
    echo "To try building and installing without this build script, see MANUAL-INSTALL.md." >&2

    # Finish logging debug info if the script fails early.
    if ! [[ -z ${LOG_DEBUG_INFO_TASK_PID:-} ]]; then
        wait ${LOG_DEBUG_INFO_TASK_PID}
    fi
}

# Build for release by default. Use -d for a debug build.
CONFIGURATION=Release
#CONFIGURATION=Debug

# The default is to clean before installing because we want the log file to have roughly the same
# information after every build.
CLEAN=clean

CONTINUE_ON_ERROR=0

# Update .gitignore if you change this.
LOG_FILE=build_and_install.log

# Empty the log file
echo -n > ${LOG_FILE}

COREAUDIOD_PLIST="/System/Library/LaunchDaemons/com.apple.audio.coreaudiod.plist"

# TODO: Should (can?) we use xcodebuild to get these from the Xcode project rather than duplicating
#       them?
APP_PATH="/Applications"
APP_DIR="Background Music.app"
DRIVER_PATH="/Library/Audio/Plug-Ins/HAL"
DRIVER_DIR="Background Music Device.driver"
XPC_HELPER_PATH="$(BGMApp/BGMXPCHelper/safe_install_dir.sh)"
XPC_HELPER_DIR="BGMXPCHelper.xpc"

GENERAL_ERROR_MSG="Internal script error. Probably a bug in this script."
BUILD_FAILED_ERROR_MSG="A build command failed. Probably a compilation error."
BGMAPP_FAILED_TO_START_ERROR_MSG="Background Music (${APP_PATH}/${APP_DIR}) didn't seem to start \
up. It might just be taking a while.

If it didn't install correctly, you'll need to open the Sound control panel in System Preferences \
and change your output device at least once. Your sound probably won't work until you do. (Or you \
restart your computer.)

If you only have one device, you can create a temporary one by opening \
\"/Applications/Utilities/Audio MIDI Setup.app\", clicking the plus button and choosing \"Create \
Multi-Output Device\"."
ERROR_MSG="${GENERAL_ERROR_MSG}"

XCODEBUILD="/usr/bin/xcodebuild"
if ! [[ -x "${XCODEBUILD}" ]]; then
    XCODEBUILD=$(which xcodebuild || true)
fi
# This check is last because it takes 10 seconds or so if it fails.
if ! [[ -x "${XCODEBUILD}" ]]; then
    XCODEBUILD=$(/usr/bin/xcrun --find xcodebuild &2>>${LOG_FILE} || true)
fi

# TODO: Update this when/if Xcode 6 is supported.
RECOMMENDED_MIN_XCODE_VERSION=7

usage() {
    echo "Usage: $0 [options]" >&2
    echo -e "\t-n\tDon't clean before building/installing." >&2
    echo -e "\t-d\tDebug build. (Release is the default.)" >&2
    echo -e "\t-c\tContinue on script errors. Might not be safe." >&2
    echo -e "\t-h\tPrint this usage statement." >&2
    exit 1
}

bold_face() {
    echo $(tput bold)$*$(tput sgr0)
}

# Takes a PID and returns 0 if the process is running.
is_alive() {
    kill -0 $1 > /dev/null 2>&1 && return 0 || return 1
}

# Shows a "..." animation until the previous command finishes. Shows an error message and exits the
# script if the command fails. The return value will be the exit status of the command.
#
# Params:
#  - The error message to show if the previous command fails.
#  - An optional timeout in seconds.
show_spinner() {
    set +e
    trap - ERR

    local PREV_COMMAND_PID=$!

    # Get the previous command as a string, with variables resolved. Assumes that if the command has
    # a child process we just want the text of the child process's command. (And that it only has
    # one child.)
    local CHILD_PID=$(pgrep -P ${PREV_COMMAND_PID} | head -n1 || echo ${PREV_COMMAND_PID})
    local PREV_COMMAND_STRING=$(ps -o command= ${CHILD_PID})
    local TIMEOUT=${2:-0}

    exec 3>&1 # Creates an alias so the following subshell can print to stdout.
    DID_TIMEOUT=$(
        I=1
        while (is_alive ${PREV_COMMAND_PID}) && \
            ([[ ${TIMEOUT} -lt 1 ]] || [[ $I -lt ${TIMEOUT} ]])
        do
            printf '.' >&3
            sleep 1
            # Erase after we've printed three dots. (\b is backspace.)
            [[ $((I % 3)) -eq 0 ]] && printf '\b\b\b   \b\b\b' >&3
            ((I++))
        done
        if [[ $I -eq ${TIMEOUT} ]]; then
            kill ${PREV_COMMAND_PID} >> ${LOG_FILE} 2>&1
            echo 1
        else
            echo 0
        fi)
    exec 3<&- # Close the file descriptor.

    wait ${PREV_COMMAND_PID}
    local EXIT_STATUS=$?

    # Clean up the dots.
    printf '\b\b\b   \b\b\b'

    # Print an error message if the command fails.
    # (wait returns 127 if the process has already exited.)
    if [[ ${EXIT_STATUS} -ne 0 ]] && [[ ${EXIT_STATUS} -ne 127 ]]; then
        ERROR_MSG="$1"
        if [[ ${DID_TIMEOUT} -eq 0 ]]; then
            ERROR_MSG+="\n\nFailed command:
                            ${PREV_COMMAND_STRING}"
        fi

        error_handler ${LINENO}

        if [[ ${CONTINUE_ON_ERROR} -eq 0 ]]; then
            exit ${EXIT_STATUS}
        fi
    fi

    if [[ ${CONTINUE_ON_ERROR} -eq 0 ]]; then
        set -e
        trap 'error_handler ${LINENO}' ERR
    fi

    return ${EXIT_STATUS}
}

parse_options() {
    while getopts ":ndch" opt; do
        case $opt in
            n)
                CLEAN=""
                ;;
            d)
                CONFIGURATION="Debug"
                ;;
            c)
                CONTINUE_ON_ERROR=1
                echo "$(tput setaf 11)WARNING$(tput sgr0): Ignoring errors."
                set +e
                trap - ERR
                ;;
            h)
                usage
                ;;
            \?)
                echo "Invalid option: -$OPTARG" >&2
                usage
                ;;
        esac
    done
}

# Checks if $XCODEBUILD is a usable xcodebuild. Exits with an error status if it isn't.
check_xcode() {
    RETURN=0

    # First, check xcodebuild exists on the system an is an executable.
    if ! [[ -x "${XCODEBUILD}" ]] || ! /usr/bin/xcode-select --print-path &>/dev/null || \
        ! pkgutil --pkg-info=com.apple.pkg.CLTools_Executables &>/dev/null; then
        RETURN=1
    fi

    # Check that Xcode is installed, not just the command line tools.
    if ! "${XCODEBUILD}" -version &>/dev/null; then
        ((RETURN+=2))
    fi

    # Check they've already accepted the Xcode license. This code is mostly copied from
    # Homebrew/Library/Homebrew/brew.sh.
    set +e
    trap - ERR

    XCRUN_OUTPUT="$(/usr/bin/xcrun clang 2>&1)"  # Making this a local breaks $?. Not sure why.
    local XCRUN_STATUS="$?"
    if [[ ${RETURN} -eq 0 ]] && \
        [[ "${XCRUN_STATUS}" -ne 0 ]] && \
        ( [[ "${XCRUN_OUTPUT}" = *license* ]] || [[ "${XCRUN_OUTPUT}" = *licence* ]] ); then
        RETURN=4
    fi

    if [[ ${CONTINUE_ON_ERROR} -eq 0 ]]; then
        set -e
        trap 'error_handler ${LINENO}' ERR
    fi

    # Version check.
    local XCODE_MAJOR_VERSION="$(echo ${XCODE_VERSION} | sed 's/\..*$//g')" 
    if [[ ${RETURN} -eq 0 ]] && \
        [[ "${XCODE_MAJOR_VERSION}" -lt ${RECOMMENDED_MIN_XCODE_VERSION} ]]
    then
        RETURN=5
    fi

    exit ${RETURN}
}

# Expects CHECK_XCODE_TASK_PID to be set.
handle_check_xcode_result() {
    if [[ -z ${HANDLED_CHECK_XCODE_RESULT:-} ]]; then
        HANDLED_CHECK_XCODE_RESULT=1
        # Wait for the Xcode checks to finish.
        set +e
        trap - ERR
        wait ${CHECK_XCODE_TASK_PID}
        CHECK_XCODE_TASK_STATUS=$?
        if [[ ${CONTINUE_ON_ERROR} -eq 0 ]]; then
            set -e
            trap 'error_handler ${LINENO}' ERR
        fi

        # If there was a problem with Xcode/xcodebuild, print the error message and exit.
        if [[ ${CHECK_XCODE_TASK_STATUS} -ne 0 ]]; then
            handle_check_xcode_failure ${CHECK_XCODE_TASK_STATUS}
        fi
    fi
}

handle_check_xcode_failure() {
    # No command line tools
    if [[ $1 -eq 1 ]] || [[ $1 -eq 3 ]]; then
        echo "$(tput setaf 9)ERROR$(tput sgr0): The Xcode Command Line Tools don't seem to be" \
             "installed on your system." >&2
        echo >&2
        echo "If you have Xcode installed, you should be able to install them with" >&2
        echo "    xcode-select --install" >&2
        echo "If not, you'll need to install Xcode (~9GB), because xcodebuild no longer works" \
             "without it." >&2
        echo >&2
    fi

    # No Xcode
    if [[ $1 -eq 2 ]] || [[ $1 -eq 3 ]]; then
        echo "$(tput setaf 9)ERROR$(tput sgr0): Unfortunately, Xcode (~9GB) is required to build" \
             "Background Music, but ${XCODEBUILD} doesn't appear to be usable. You may need to" \
             "tell the Xcode command line tools where your Xcode is installed to with" >&2
        echo "    xcode-select --switch /the/path/to/your/Xcode.app" >&2
        echo >&2
        echo "Output from ${XCODEBUILD}:" >&2

        "${XCODEBUILD}" -version >&2 || true

        echo >&2
    fi

    # Need to agree to the Xcode license
    if [[ $1 -eq 4 ]]; then
        echo "$(tput setaf 9)ERROR$(tput sgr0): You need to agree to the Xcode license before you" \
             "can build Background Music. Run this command and then try again:" >&2
        echo "    sudo xcodebuild -license" >&2
    fi

    if [[ $1 -eq 5 ]]; then
        # Xcode version is probably too old.
        echo "$(tput setaf 11)WARNING$(tput sgr0): Your version of Xcode (${XCODE_VERSION}) may" \
             "not be recent enough to build Background Music." >&2
    fi

    # Try to find Xcode and print a more useful error message.
    if [[ $1 -lt 4 ]]; then
        # Disable error handlers
        set +e; trap - ERR

        echo "Looking for Xcode..." >&2
        XCODE_PATHS=$(mdfind "kMDItemCFBundleIdentifier == 'com.apple.dt.Xcode' || \
                              kMDItemCFBundleIdentifier == 'com.apple.Xcode'")

        if [[ "${XCODE_PATHS}" != "" ]]; then
            echo "It looks like you have Xcode installed to" >&2
            echo "${XCODE_PATHS}" >&2
        else
            echo "Not found." >&2
        fi
    fi

    # Exit with an error status, unless we only printed a warning or were told to continue anyway.
    if [[ $1 -ne 5 ]] && [[ ${CONTINUE_ON_ERROR} -eq 0 ]]; then
        exit "$1"
    fi
}

log_debug_info() {
    # Log some environment details, version numbers, etc. This takes a while, so we do it in the
    # background.
    (set +e; trap - ERR
        echo "Background Music Build Log" >> ${LOG_FILE}
        echo "----" >> ${LOG_FILE}
        echo "System details:" >> ${LOG_FILE}

        sw_vers >> ${LOG_FILE} 2>&1
        # The same as uname -a, except without printing the nodename (for privacy).
        uname -mrsv >> ${LOG_FILE} 2>&1

        /bin/bash --version >> ${LOG_FILE} 2>&1
        /usr/bin/env python --version >> ${LOG_FILE} 2>&1

        echo "On git branch: $(git rev-parse --abbrev-ref HEAD 2>&1)" >> ${LOG_FILE}
        echo "Most recent commit: $(git rev-parse HEAD 2>&1)" \
             "(\"$(git show -s --format=%s HEAD 2>&1)\")" >> ${LOG_FILE}

        echo "Using xcodebuild: ${XCODEBUILD}" >> ${LOG_FILE}
        echo "Using BGMXPCHelper path: ${XPC_HELPER_PATH}" >> ${LOG_FILE}

        xcode-select --version >> ${LOG_FILE} 2>&1
        echo "Xcode path: $(xcode-select --print-path 2>&1)" >> ${LOG_FILE}
        echo "Xcode version:" >> ${LOG_FILE}
        xcodebuild -version >> ${LOG_FILE} 2>&1
        echo "Xcode SDKs:" >> ${LOG_FILE}
        xcodebuild -showsdks >> ${LOG_FILE} 2>&1
        xcrun --version >> ${LOG_FILE} 2>&1
        echo "Clang version:" >> ${LOG_FILE}
        $(/usr/bin/xcrun --find clang 2>&1) --version >> ${LOG_FILE} 2>&1

        echo "launchctl version: $(launchctl version 2>&1)" >> ${LOG_FILE}
        echo "----" >> ${LOG_FILE}) &

    LOG_DEBUG_INFO_TASK_PID=$!
}

# Register our handler so we can print a message and clean up if there's an error.
trap 'error_handler ${LINENO}' ERR

parse_options "$@"

# Warn if running as root.
if [[ $(id -u) -eq 0 ]]; then
    echo "$(tput setaf 11)WARNING$(tput sgr0): This script is not intended to be run as root. Run" \
         "it normally and it'll sudo when it needs to." >&2
fi

# Print initial message.
echo "$(bold_face About to install Background Music). Please pause all audio, if you can."
[[ "${CONFIGURATION}" == "Debug" ]] && echo "Debug build."
echo
echo "This script will install:"
echo " - ${APP_PATH}/${APP_DIR}"
echo " - ${DRIVER_PATH}/${DRIVER_DIR}"
echo " - ${XPC_HELPER_PATH}/${XPC_HELPER_DIR}"
echo " - /Library/LaunchDaemons/com.bearisdriving.BGM.XPCHelper.plist"
echo

# Make sure Xcode and the command line tools are installed and recent enough.
XCODE_VERSION=$(${XCODEBUILD} -version | head -n 1 | awk '{ print $2 }' 2>/dev/null || echo 0)
check_xcode &
CHECK_XCODE_TASK_PID=$!

read -p "Continue (y/N)? " CONTINUE_INSTALLATION

if [[ "${CONTINUE_INSTALLATION}" != "y" ]] && [[ "${CONTINUE_INSTALLATION}" != "Y" ]]; then
    echo "Installation cancelled."
    exit 0
fi

# If the check_xcode process has already finished, we can check the result early.
if ! is_alive ${CHECK_XCODE_TASK_PID}; then
    handle_check_xcode_result
fi

# Update the user's sudo timestamp. (Prompts the user for their password.)
# Don't call sudo -v if this is a Travis CI build.
if ([[ -z ${TRAVIS:-} ]] || [[ "${TRAVIS}" != true ]]) && ! sudo -v; then
    echo "ERROR: This script must be run by a user with administrator (sudo) privileges." >&2
    exit 1
fi

handle_check_xcode_result

log_debug_info

# BGMDriver

echo "[1/3] Installing the virtual audio device $(bold_face ${DRIVER_DIR}) to" \
     "$(bold_face ${DRIVER_PATH})." \
     | tee -a ${LOG_FILE}

# Disable the -e shell option and error trap for build commands so we can handle errors differently.
(set +e; trap - ERR
    # Build Apple's PublicUtility classes as a static library.
    sudo "${XCODEBUILD}" -project BGMDriver/BGMDriver.xcodeproj \
                         -target "PublicUtility" \
                         -configuration ${CONFIGURATION} \
                         RUN_CLANG_STATIC_ANALYZER=0 \
                         ${CLEAN} build >> ${LOG_FILE} 2>&1) &

(set +e; trap - ERR
    # Build and install BGMDriver
    # TODO: Should these use -scheme instead?
    sudo "${XCODEBUILD}" -project BGMDriver/BGMDriver.xcodeproj \
                         -target "Background Music Device" \
                         -configuration ${CONFIGURATION} \
                         RUN_CLANG_STATIC_ANALYZER=0 \
                         DSTROOT="/" \
                         ${CLEAN} install >> ${LOG_FILE} 2>&1) &

show_spinner "${BUILD_FAILED_ERROR_MSG}"

# BGMXPCHelper

echo "[2/3] Installing $(bold_face ${XPC_HELPER_DIR}) to $(bold_face ${XPC_HELPER_PATH})." \
     | tee -a ${LOG_FILE}

(set +e; trap - ERR
    sudo "${XCODEBUILD}" -project BGMApp/BGMApp.xcodeproj \
                         -target BGMXPCHelper \
                         -configuration ${CONFIGURATION} \
                         RUN_CLANG_STATIC_ANALYZER=0 \
                         DSTROOT="/" \
                         INSTALL_PATH="${XPC_HELPER_PATH}" \
                         ${CLEAN} install >> ${LOG_FILE} 2>&1) &

show_spinner "${BUILD_FAILED_ERROR_MSG}"

# BGMApp

echo "[3/3] Installing $(bold_face ${APP_DIR}) to $(bold_face ${APP_PATH})." \
     | tee -a ${LOG_FILE}

(set +e; trap - ERR
    sudo "${XCODEBUILD}" -project BGMApp/BGMApp.xcodeproj \
                         -target "Background Music" \
                         -configuration ${CONFIGURATION} \
                         RUN_CLANG_STATIC_ANALYZER=0 \
                         DSTROOT="/" \
                         ${CLEAN} install >> ${LOG_FILE} 2>&1) &

show_spinner "${BUILD_FAILED_ERROR_MSG}"

# Fix Background Music.app owner/group.
#
# We have to run xcodebuild as root to install BGMXPCHelper because it installs to directories
# owned by root. But that means the build directory gets created by root, and since BGMApp uses the
# same build directory we have to run xcodebuild as root to install BGMApp as well.
#
# TODO: Can't we just chown -R the build dir before we install BGMApp? Then we wouldn't have to
#       install BGMApp as root. (But maybe still handle the unlikely case of APP_PATH not being
#       user-writable.)
sudo chown -R "$(whoami):admin" "${APP_PATH}/${APP_DIR}"

# Fix the build directories' owner/group. This is mainly so the whole source directory can be
# deleted easily after installing.
sudo chown -R "$(whoami):admin" "BGMApp/build" "BGMDriver/build"

# Restart coreaudiod.

echo "Restarting coreaudiod to load the virtual audio device." \
     | tee -a ${LOG_FILE}

# The extra or-clauses are fallback versions of the command that restarts coreaudiod. Apparently
# some of these commands don't work with older versions of launchctl, so I figure there's no harm in
# trying a bunch of different ways (which should all work).
(sudo launchctl kill SIGTERM system/com.apple.audio.coreaudiod &>/dev/null || \
    sudo launchctl kill TERM system/com.apple.audio.coreaudiod &>/dev/null || \
    sudo launchctl kill 15 system/com.apple.audio.coreaudiod &>/dev/null || \
    sudo launchctl kill -15 system/com.apple.audio.coreaudiod &>/dev/null || \
    (sudo launchctl unload "${COREAUDIOD_PLIST}" &>/dev/null && \
        sudo launchctl load "${COREAUDIOD_PLIST}" &>/dev/null) || \
    sudo killall coreaudiod &>/dev/null) && \
    sleep 5

# Invalidate sudo ticket
sudo -k

# Open BGMApp.
#
# I'd rather not open BGMApp here, or at least ask first, but you have to change your default audio
# device after restarting coreaudiod and this is the easiest way.
echo "Launching Background Music."

ERROR_MSG="${BGMAPP_FAILED_TO_START_ERROR_MSG}"
open "${APP_PATH}/${APP_DIR}"

# Ignore script errors from this point.
set +e
trap - ERR

# Wait up to 5 seconds for Background Music to start.
(trap 'exit 1' TERM
    while ! (ps -Ao ucomm= | grep 'Background Music' > /dev/null); do
        sleep 1
    done) &
show_spinner "${BGMAPP_FAILED_TO_START_ERROR_MSG}" 5

echo "Done."


