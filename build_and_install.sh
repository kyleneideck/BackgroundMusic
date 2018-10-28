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
# Copyright © 2016-2018 Kyle Neideck
# Copyright © 2016 Nick Jacques
#
# Builds and installs BGMApp, BGMDriver and BGMXPCHelper. Requires xcodebuild and Xcode.
#
# Don't let the length of this script scare you away from building/installing without it (using
# either Xcode or xcodebuild). 90% of this code is for error handling, logging, user friendliness,
# etc. See MANUAL-INSTALL.md, DEVELOPING.md and BGMDriver/BGMDriver/quick_install.sh.
#

# Safe mode
set -euo pipefail
IFS=$'\n\t'

# Subshells and functions inherit the ERR trap
set -o errtrace

# Go to the project directory.
cd "$( dirname "${BASH_SOURCE[0]}" )"

error_handler() {
    LAST_COMMAND="$3" LAST_COMMAND_EXIT_STATUS="$2"

    # Log the error.
    echo "Failure in $0 at line $1. The last command was (probably)" >> ${LOG_FILE}
    echo "    ${LAST_COMMAND}" >> ${LOG_FILE}
    echo "which exited with status ${LAST_COMMAND_EXIT_STATUS}." >> ${LOG_FILE}
    echo "Error message: ${ERROR_MSG}" >> ${LOG_FILE}
    echo >> ${LOG_FILE}

    # Scrub username from log (and also real name just in case).
    sed -i'tmp' "s/$(whoami)/[username removed]/g" ${LOG_FILE}
    sed -i'tmp' "s/$(id -F)/[name removed]/g" ${LOG_FILE}
    rm "${LOG_FILE}tmp"

    # Print an error message.
    echo "$(tput setaf 9)ERROR$(tput sgr0): Install failed at line $1 with the message:" >&2
    echo -e "${ERROR_MSG}" >&2
    echo >&2
    echo "Feel free to report this. If you do, you'll probably want to include the" \
         "build_and_install.log file from this directory ($(pwd)). But quickly skim through it" \
         "first to check that it doesn't include any personal information. It shouldn't, but this" \
         "is alpha software so you never know." >&2
    echo >&2
    echo "To try building and installing without this build script, see MANUAL-INSTALL.md." >&2
    echo >&2
    echo "You can also try ignoring compiler warnings with: $0 -w" >&2

    echo >&2
    echo "Error details:" >&2
    echo "Line $1. The last command was (probably)" >&2
    echo "    ${LAST_COMMAND}" >&2
    echo "which exited with status ${LAST_COMMAND_EXIT_STATUS}." >&2
    echo >&2

    # Finish logging debug info if the script fails early.
    if ! [[ -z ${LOG_DEBUG_INFO_TASK_PID:-} ]]; then
        wait ${LOG_DEBUG_INFO_TASK_PID}
    fi
}

enable_error_handling() {
    if [[ -z ${CONTINUE_ON_ERROR} ]] || [[ ${CONTINUE_ON_ERROR} -eq 0 ]]; then
        set -e
        # TODO: The version of Bash that ships with OSX only gives you the line number of the
        #       function the error occurred in -- not the line the error occurred on. There are a
        #       few solutions suggested on various websites, but none of them work.
        trap 'error_handler ${LINENO} $? "${BASH_COMMAND}"' ERR
    fi
}

disable_error_handling() {
    set +e
    trap - ERR
}

# Build for release by default. Use -d for a debug build.
CONFIGURATION=Release
#CONFIGURATION=Debug

XCODEBUILD_ACTION="install"

# The default is to clean before installing because we want the log file to have roughly the same
# information after every build.
CLEAN=clean

XCODEBUILD_OPTIONS=""

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
    XCODEBUILD="$(/usr/bin/xcrun --find xcodebuild 2>>${LOG_FILE} || true)"
fi

RECOMMENDED_MIN_XCODE_VERSION=8

usage() {
    echo "Usage: $0 [options]" >&2
    echo -e "\t-n            Don't clean before building/installing." >&2
    echo -e "\t-d            Debug build. (Release is the default.)" >&2
    echo -e "\t-b            Build only, don't install." >&2
    echo -e "\t-w            Ignore compiler warnings. (They're treated as errors by default.)" >&2
    echo -e "\t-x [options]  Extra options to pass to xcodebuild." >&2
    echo -e "\t-c            Continue on script errors. Might not be safe." >&2
    echo -e "\t-h            Print this usage statement." >&2
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
    disable_error_handling

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
        if [[ ${DID_TIMEOUT} -ne 0 ]]; then
            ERROR_MSG+="\n\nCommand timed out after ${TIMEOUT} seconds."
        fi

        error_handler ${LINENO} ${EXIT_STATUS} "${PREV_COMMAND_STRING}"

        if [[ ${CONTINUE_ON_ERROR} -eq 0 ]]; then
            exit ${EXIT_STATUS}
        fi
    fi

    enable_error_handling

    return ${EXIT_STATUS}
}

parse_options() {
    while getopts ":ndbwx:ch" opt; do
        case $opt in
            n)
                CLEAN=""
                ;;
            d)
                CONFIGURATION="Debug"
                ;;
            b)
                # Just build; don't install.
                XCODEBUILD_ACTION="build"
                # The dirs xcodebuild will build in.
                # TODO: If these dirs were created by running this script without -b, they'll be
                #       owned by root and xcodebuild will fail.
                APP_PATH="./BGMApp/build"
                DRIVER_PATH="./BGMDriver/build"
                ;;
            w)
                # TODO: What if they also pass their own OTHER_CFLAGS with -x?
                XCODEBUILD_OPTIONS="${XCODEBUILD_OPTIONS} OTHER_CFLAGS=\"-Wno-error\""
                ;;
            x)
                XCODEBUILD_OPTIONS="$OPTARG"
                ;;
            c)
                CONTINUE_ON_ERROR=1
                echo "$(tput setaf 11)WARNING$(tput sgr0): Ignoring errors."
                disable_error_handling
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

# check_xcode return codes
CHECK_XCODE_NO_ERR=0
CHECK_XCODE_ERR_NO_CLTOOLS=1
CHECK_XCODE_ERR_NO_XCODE=2
CHECK_XCODE_ERR_NO_CLTOOLS_OR_XCODE=3
CHECK_XCODE_ERR_LICENSE_NOT_ACCEPTED=4
CHECK_XCODE_ERR_OLD_VERSION=5

# Checks if $XCODEBUILD is a usable xcodebuild. Exits with one of the status codes above.
check_xcode() {
    local EXIT_CODE=${CHECK_XCODE_NO_ERR}

    # First, check xcodebuild exists on the system an is an executable.
    if ! [[ -x "${XCODEBUILD}" ]] || ! /usr/bin/xcode-select --print-path &>/dev/null || \
        ! pkgutil --pkg-info=com.apple.pkg.CLTools_Executables &>/dev/null; then
        EXIT_CODE=${CHECK_XCODE_ERR_NO_CLTOOLS}
    fi

    # Check that Xcode is installed, not just the command line tools.
    if [[ "${XCODE_VERSION}" == "-1" ]]; then
        ((EXIT_CODE+=2))
    fi

    # Check they've already accepted the Xcode license. This code is mostly copied from
    # Homebrew/Library/Homebrew/brew.sh.
    disable_error_handling

    local XCRUN_OUTPUT  # (Declared local before assigning so we can get $?.)
    XCRUN_OUTPUT="$(/usr/bin/xcrun clang 2>&1)"
    local XCRUN_STATUS="$?"
    if [[ ${EXIT_CODE} -eq 0 ]] && \
        [[ "${XCRUN_STATUS}" -ne 0 ]] && \
        ( [[ "${XCRUN_OUTPUT}" = *license* ]] || [[ "${XCRUN_OUTPUT}" = *licence* ]] ); then
        EXIT_CODE=${CHECK_XCODE_ERR_LICENSE_NOT_ACCEPTED}
    fi

    enable_error_handling

    # Version check.
    local XCODE_MAJOR_VERSION="$(echo ${XCODE_VERSION} | sed 's/\..*$//g')" 
    if [[ ${EXIT_CODE} -eq 0 ]] && \
        [[ "${XCODE_MAJOR_VERSION}" -lt ${RECOMMENDED_MIN_XCODE_VERSION} ]]
    then
        EXIT_CODE=${CHECK_XCODE_ERR_OLD_VERSION}
    fi

    exit ${EXIT_CODE}
}

# Expects CHECK_XCODE_TASK_PID to be set and error handling to be disabled. Returns 1 if we need to
# check for Xcode again.
handle_check_xcode_result() {
    if [[ -z ${HANDLED_CHECK_XCODE_RESULT:-} ]]; then
        HANDLED_CHECK_XCODE_RESULT=1
        # Wait for the Xcode checks to finish.
        wait ${CHECK_XCODE_TASK_PID}
        CHECK_XCODE_TASK_STATUS=$?

        # If there was a problem with Xcode/xcodebuild, print the error message and exit.
        if [[ ${CHECK_XCODE_TASK_STATUS} -ne ${CHECK_XCODE_NO_ERR} ]]; then
            handle_check_xcode_failure ${CHECK_XCODE_TASK_STATUS}
            local STATUS=$?
            return ${STATUS}
        fi
    fi

    return 0
}

# Returns 1 if we need to check for Xcode again. Expects error handling to be disabled.
#
# Params:
#  - The exit code of check_xcode.
handle_check_xcode_failure() {
    local CONTINUE=${CONTINUE_ON_ERROR}

    # No command line tools
    if [[ $1 -eq ${CHECK_XCODE_ERR_NO_CLTOOLS} ]] || \
            [[ $1 -eq ${CHECK_XCODE_ERR_NO_CLTOOLS_OR_XCODE} ]]; then
        echo "$(tput setaf 9)ERROR$(tput sgr0): The Xcode Command Line Tools don't seem to be" \
             "installed on your system." >&2
        echo >&2
        echo "If you have Xcode installed, you should be able to install them with" >&2
        echo "    sudo /usr/bin/xcode-select --install" >&2
        echo "If not, you'll need to install Xcode (~9GB), because xcodebuild no longer works" \
             "without it." >&2
        echo >&2
    fi

    # No Xcode (that the command line tools are aware of)
    if [[ $1 -eq ${CHECK_XCODE_ERR_NO_XCODE} ]] || \
            [[ $1 -eq ${CHECK_XCODE_ERR_NO_CLTOOLS_OR_XCODE} ]]; then
        if ! handle_check_xcode_failure_no_xcode $1; then
            return 1
        fi
    fi

    # They need to agree to the Xcode license.
    if [[ $1 -eq ${CHECK_XCODE_ERR_LICENSE_NOT_ACCEPTED} ]]; then
        echo "$(tput setaf 9)ERROR$(tput sgr0): You need to agree to the Xcode license before you" \
             "can build Background Music. Run this command and then try again:" >&2
        echo "    sudo ${XCODEBUILD} -license" >&2
        echo >&2
    fi

    # Xcode version is probably too old.
    if [[ $1 -eq ${CHECK_XCODE_ERR_OLD_VERSION} ]]; then
        echo "$(tput setaf 11)WARNING$(tput sgr0): Your version of Xcode (${XCODE_VERSION}) may" \
             "not be recent enough to build Background Music. If you have a newer version" \
             "installed, you can set the Xcode command line tools to use it with" >&2
        echo "    sudo /usr/bin/xcode-select --switch /the/path/to/your/Xcode.app" >&2
        echo >&2
        CONTINUE=1
    fi

    # Try to find Xcode and print a more useful error message.
    if [[ $1 -eq ${CHECK_XCODE_ERR_NO_CLTOOLS} ]] || \
            [[ $1 -eq ${CHECK_XCODE_ERR_NO_XCODE} ]] || \
            [[ $1 -eq ${CHECK_XCODE_ERR_NO_CLTOOLS_OR_XCODE} ]] || \
            [[ $1 -eq ${CHECK_XCODE_ERR_OLD_VERSION} ]]; then
        echo "Searching for Xcode installations..." >&2
        echo >&2
        XCODE_PATHS=$(mdfind "kMDItemCFBundleIdentifier == 'com.apple.dt.Xcode' || \
                              kMDItemCFBundleIdentifier == 'com.apple.Xcode'")

        if [[ "${XCODE_PATHS}" != "" ]]; then
            echo "It looks like you have Xcode installed to:" >&2
            echo "${XCODE_PATHS}" >&2
        else
            echo "None found." >&2
        fi
    fi

    # Exit with an error status, unless we only printed a warning or were told to continue anyway.
    if [[ ${CONTINUE} -eq 0 ]]; then
        exit $1
    fi

    return 0
}

# Returns 1 if we need to check for Xcode again. Expects error handling to be disabled.
#
# Params:
#  - The exit code of check_xcode.
handle_check_xcode_failure_no_xcode() {
    # The problem could be that they have Xcode installed, but the command line tools can't find
    # it.

    # Just check for Xcode in the default location at first, since searching with mdfind can
    # take a while.
    if [[ $1 -eq ${CHECK_XCODE_ERR_NO_XCODE} ]] && [[ -d /Applications/Xcode.app ]]; then
        echo "It looks like you have Xcode installed to /Applications/Xcode.app, but the" \
             "Xcode command line tools aren't set to use it. Try" >&2
        echo "    sudo /usr/bin/xcode-select --switch /Applications/Xcode.app" >&2
        echo "and then run this installer again." >&2
    else
        echo "$(tput setaf 9)ERROR$(tput sgr0): Unfortunately, Xcode (~9GB) is required to" \
             "build Background Music, but ${XCODEBUILD} doesn't appear to be usable. If you" \
             "do have Xcode installed, try telling the Xcode command line tools where to find" \
             "it with" >&2
        echo "    sudo /usr/bin/xcode-select --switch /the/path/to/your/Xcode.app" >&2
        echo "and then running this installer again." >&2
    fi
    echo >&2

    # Explain how to revert the command we suggested.
    local DEV_DIR_PATH  # (Declared local before assigning so we can get $?.)
    DEV_DIR_PATH=$(/usr/bin/xcode-select --print-path 2>/dev/null)
    local XCSELECT_STATUS="$?"
    if [[ "${XCSELECT_STATUS}" -eq 0 ]] && [[ "${DEV_DIR_PATH}" != "" ]]; then
        echo "If you want to change it back afterwards for some reason, use" >&2
        echo "    sudo /usr/bin/xcode-select --switch ${DEV_DIR_PATH}" >&2
    fi
    echo >&2

    # Print the error message from xcodebuild.
    local OUTPUT_MSG="Output from ${XCODEBUILD}: ------------------------------------------"
    echo "${OUTPUT_MSG}" >&2
    "${XCODEBUILD}" -version >&2 || true
    echo "${OUTPUT_MSG}" | tr '[:print:]' - >&2
    echo >&2

    # Offer to switch if it will probably work.
    if [[ -d /Applications/Xcode.app ]]; then
        local SWITCH_CMD="sudo xcode-select --switch /Applications/Xcode.app"
        read -p "Try \"$(bold_face ${SWITCH_CMD})\" now (y/N)? " TRY_SWITCH
        if [[ "${TRY_SWITCH}" == "y" ]] || [[ "${TRY_SWITCH}" == "Y" ]]; then
            echo
            echo "+ ${SWITCH_CMD}"
            eval ${SWITCH_CMD}
            local XCS_STATUS=$?
            if [[ ${XCS_STATUS} -eq 0 ]]; then
                # Return that we should call check_xcode again to see if it worked.
                echo
                echo Seemed to work. Trying to start installation again...
                echo
                return 1
            else
                echo >&2
                echo "\"$(bold_face ${SWITCH_CMD})\" failed with exit status ${XCS_STATUS}." >&2
                exit ${XCS_STATUS}
            fi
        fi
        echo
    fi

    return 0
}

log_debug_info() {
    # Log some environment details, version numbers, etc. This takes a while, so we do it in the
    # background.

    (disable_error_handling
        echo "Background Music Build Log" >> ${LOG_FILE}
        echo "----" >> ${LOG_FILE}
        echo "Build script args: $*" >> ${LOG_FILE}
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

# Cleans the build products and intermediate files for a build scheme.
#
# Params:
#  - The Xcode build scheme to clean, e.g. "Background Music Device".
clean() {
    if [[ "${CLEAN}" != "" ]]; then
        ${SUDO} "${XCODEBUILD}" -scheme "$1" \
                                -configuration ${CONFIGURATION} \
                                BUILD_DIR=./build \
                                ${CLEAN} >> ${LOG_FILE} 2>&1
    fi
}

# Register our handler so we can print a message and clean up if there's an error.
enable_error_handling

parse_options "$@"

# Warn if running as root.
if [[ $(id -u) -eq 0 ]]; then
    echo "$(tput setaf 11)WARNING$(tput sgr0): This script is not intended to be run as root. Run" \
         "it normally and it'll sudo when it needs to." >&2
fi

# Print initial message.
if [[ "${XCODEBUILD_ACTION}" == "install" ]]; then
    XPC_HELPER_PATH="$(BGMApp/BGMXPCHelper/safe_install_dir.sh)"

    echo "$(bold_face About to install Background Music). Please pause all audio, if you can."
    [[ "${CONFIGURATION}" == "Debug" ]] && echo "Debug build."
    echo
    echo "This script will install:"
    echo " - ${APP_PATH}/${APP_DIR}"
    echo " - ${DRIVER_PATH}/${DRIVER_DIR}"
    echo " - ${XPC_HELPER_PATH}/${XPC_HELPER_DIR}"
    echo " - /Library/LaunchDaemons/com.bearisdriving.BGM.XPCHelper.plist"
    echo
elif [[ "${XCODEBUILD_ACTION}" == "build" ]]; then
    XPC_HELPER_PATH="${APP_PATH}"

    echo "$(bold_face Building Background Music...)"
    echo
fi

# Make sure Xcode and the command line tools are installed and recent enough.
# This sets XCODE_VERSION to major.minor, e.g. 8.3, or -1 if Xcode isn't installed.
XCODE_VERSION=$((${XCODEBUILD} -version 2>/dev/null || echo 'V -1') | head -n 1 | awk '{ print $2 }')
check_xcode &
CHECK_XCODE_TASK_PID=$!

if [[ "${XCODEBUILD_ACTION}" == "install" ]]; then
    read -p "Continue (y/N)? " CONTINUE_INSTALLATION

    if [[ "${CONTINUE_INSTALLATION}" != "y" ]] && [[ "${CONTINUE_INSTALLATION}" != "Y" ]]; then
        echo "Installation cancelled."
        exit 0
    fi
fi

# If the check_xcode process has already finished, we can check the result early.
NEED_TO_HANDLE_CHECK_XCODE_RESULT=1
if ! is_alive ${CHECK_XCODE_TASK_PID}; then
    disable_error_handling

    handle_check_xcode_result
    NEED_TO_HANDLE_CHECK_XCODE_RESULT=$?

    enable_error_handling
fi

if [[ "${XCODEBUILD_ACTION}" == "install" ]]; then
    # Update the user's sudo timestamp. (Prompts the user for their password.)
    # Don't call sudo -v if this is a Travis CI build.
    if ([[ -z ${TRAVIS:-} ]] || [[ "${TRAVIS}" != true ]]) && ! sudo -v; then
        echo "$(tput setaf 9)ERROR$(tput sgr0): This script must be run by a user with" \
             "administrator (sudo) privileges." >&2
        exit 1
    fi
    echo
fi

while [[ ${NEED_TO_HANDLE_CHECK_XCODE_RESULT} -ne 0 ]]; do
    disable_error_handling

    handle_check_xcode_result
    NEED_TO_HANDLE_CHECK_XCODE_RESULT=$?

    enable_error_handling
done

log_debug_info $*

if [[ "${XCODEBUILD_ACTION}" == "install" ]]; then
    SUDO="sudo"
    ACTIONING="Installing"
else
    # No need to sudo if we're only building.
    SUDO=""
    ACTIONING="Building"
fi

# Enable AddressSanitizer in debug builds to catch memory bugs. Allow ENABLE_ASAN to be set as an
# environment variable by only setting it here if it isn't already set. (Used by package.sh.)
if [[ "${CONFIGURATION}" == "Debug" ]]; then
    ENABLE_ASAN="${ENABLE_ASAN:-YES}"
else
    ENABLE_ASAN="${ENABLE_ASAN:-NO}"
fi

# Clean all projects. Done separately to workaround what I think is a bug in Xcode 10.0. If you just
# add "clean" to the other xcodebuild commands, they seem to fail because of the DSTROOT="/" arg.
if [[ "${CLEAN}" != "" ]]; then
    # Disable the -e shell option and error trap for build commands so we can handle errors
    # differently.
    (disable_error_handling
        clean "Background Music Device"
        clean "PublicUtility"
        clean "BGMXPCHelper"
        clean "Background Music"
        # Also delete the build dirs as files/dirs left in them can make the install step fail and,
        # if you're using Xcode 10, the commands above will have cleaned the DerivedData dir but not
        # the build dirs. I think this is a separate Xcode bug. See
        # <http://www.openradar.me/40906897>.
        ${SUDO} /bin/rm -rf BGMDriver/build BGMApp/build >> ${LOG_FILE} 2>&1) &

    echo "Cleaning"
    show_spinner "Clean command failed. Try deleting the directories BGMDriver/build and \
BGMApp/build manually and running '$0 -n' to skip the cleaning step."
fi

# BGMDriver

echo "[1/3] ${ACTIONING} the virtual audio device $(bold_face ${DRIVER_DIR}) to" \
         "$(bold_face ${DRIVER_PATH})" \
     | tee -a ${LOG_FILE}

(disable_error_handling
    # Build and install BGMDriver.
    ${SUDO} "${XCODEBUILD}" -scheme "Background Music Device" \
                            -configuration ${CONFIGURATION} \
                            -enableAddressSanitizer ${ENABLE_ASAN} \
                            BUILD_DIR=./build \
                            RUN_CLANG_STATIC_ANALYZER=0 \
                            DSTROOT="/" \
                            ${XCODEBUILD_OPTIONS} \
                            "${XCODEBUILD_ACTION}" >> ${LOG_FILE} 2>&1) &

show_spinner "${BUILD_FAILED_ERROR_MSG}"

# BGMXPCHelper

echo "[2/3] ${ACTIONING} $(bold_face ${XPC_HELPER_DIR}) to $(bold_face ${XPC_HELPER_PATH})" \
    | tee -a ${LOG_FILE}

(disable_error_handling
    ${SUDO} "${XCODEBUILD}" -scheme BGMXPCHelper \
                            -configuration ${CONFIGURATION} \
                            -enableAddressSanitizer ${ENABLE_ASAN} \
                            BUILD_DIR=./build \
                            RUN_CLANG_STATIC_ANALYZER=0 \
                            DSTROOT="/" \
                            INSTALL_PATH="${XPC_HELPER_PATH}" \
                            ${XCODEBUILD_OPTIONS} \
                            "${XCODEBUILD_ACTION}" >> ${LOG_FILE} 2>&1) &

show_spinner "${BUILD_FAILED_ERROR_MSG}"

# BGMApp

echo "[3/3] ${ACTIONING} $(bold_face ${APP_DIR}) to $(bold_face ${APP_PATH})" \
     | tee -a ${LOG_FILE}

(disable_error_handling
    ${SUDO} "${XCODEBUILD}" -scheme "Background Music" \
                            -configuration ${CONFIGURATION} \
                            -enableAddressSanitizer ${ENABLE_ASAN} \
                            BUILD_DIR=./build \
                            RUN_CLANG_STATIC_ANALYZER=0 \
                            DSTROOT="/" \
                            ${XCODEBUILD_OPTIONS} \
                            "${XCODEBUILD_ACTION}" >> ${LOG_FILE} 2>&1) &

show_spinner "${BUILD_FAILED_ERROR_MSG}"

if [[ "${XCODEBUILD_ACTION}" == "install" ]]; then
    # Fix Background Music.app owner/group.
    #
    # We have to run xcodebuild as root to install BGMXPCHelper because it installs to directories
    # owned by root. But that means the build directory gets created by root, and since BGMApp uses
    # the same build directory we have to run xcodebuild as root to install BGMApp as well.
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
    # some of these commands don't work with older versions of launchctl, so I figure there's no
    # harm in trying a bunch of different ways (which should all work).
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

    # Open BGMApp. We have to change the default audio device after restarting coreaudiod and this
    # is the easiest way.
    echo "Launching Background Music."

    ERROR_MSG="${BGMAPP_FAILED_TO_START_ERROR_MSG}"
    open "${APP_PATH}/${APP_DIR}"

    # Ignore script errors from this point.
    disable_error_handling

    # Wait up to 5 seconds for Background Music to start.
    (trap 'exit 1' TERM
        while ! (ps -Ao ucomm= | grep 'Background Music' > /dev/null); do
            sleep 1
        done) &
    show_spinner "${BGMAPP_FAILED_TO_START_ERROR_MSG}" 5
fi

echo "Done."


