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
# Copyright © 2016-2022, 2024 Kyle Neideck
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

# Output locations for installing. These are overwritten later if building or archiving.
#
# TODO: Should (can?) we use xcodebuild to get these from the Xcode project rather than duplicating
#       them?
APP_PATH="/Applications"
APP_DIR="Background Music.app"
DRIVER_PATH="/Library/Audio/Plug-Ins/HAL"
DRIVER_DIR="Background Music Device.driver"
# XPC_HELPER_OUTPUT_PATH is set below because it depends on the system (when installing).
XPC_HELPER_DIR="BGMXPCHelper.xpc"

# The root output directory when archiving.
ARCHIVES_DIR="archives"

GENERAL_ERROR_MSG="Internal script error. Probably a bug in this script."
BUILD_FAILED_ERROR_MSG="A build command failed. Probably a compilation error."
BGMAPP_FAILED_TO_START_ERROR_MSG="Background Music (${APP_PATH}/${APP_DIR}) didn't seem to start \
up. It might just be taking a while.

If it didn't install correctly, you'll need to open the Sound control panel in System Settings \
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
    echo -e "\t-a            Build and archive, don't install. See Xcode docs for info about archiving." >&2
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

log_info() {
    echo "$*" | tee -a ${LOG_FILE}
}

capture_command_with_timeout() {
    local timeout_seconds="$1"
    local output_var_name="$2"
    shift 2

    local had_errexit=0
    [[ $- == *e* ]] && had_errexit=1
    set +e

    local tmpfile
    tmpfile="$(mktemp -t bgm-timeout.XXXXXX)"
    "$@" >"${tmpfile}" 2>&1 &
    local command_pid=$!
    local exit_status=0
    local timed_out=0
    local waited=0

    while kill -0 "${command_pid}" > /dev/null 2>&1; do
        if [[ ${waited} -ge ${timeout_seconds} ]]; then
            timed_out=1
            kill "${command_pid}" > /dev/null 2>&1 || true
            sleep 1
            kill -9 "${command_pid}" > /dev/null 2>&1 || true
            break
        fi

        sleep 1
        waited=$((waited + 1))
    done

    if [[ ${timed_out} -eq 0 ]]; then
        wait "${command_pid}"
        exit_status=$?
    else
        wait "${command_pid}" > /dev/null 2>&1 || true
        exit_status=124
    fi

    local captured_output
    captured_output="$(cat "${tmpfile}")"
    rm -f "${tmpfile}"

    printf -v "${output_var_name}" '%s' "${captured_output}"

    if [[ ${had_errexit} -eq 1 ]]; then
        set -e
    fi

    return "${exit_status}"
}

codesign_details() {
    /usr/bin/codesign -dv --verbose=2 "$1" 2>&1
}

verify_installed_bundle_for_hal_restart() {
    local bundle_path="$1"
    local bundle_label="$2"
    local verify_output=""
    local details_output=""

    if ! verify_output="$(/usr/bin/codesign --verify --strict --verbose=2 "${bundle_path}" 2>&1)"; then
        log_info "codesign verification failed for ${bundle_label}: ${verify_output}"
        return 1
    fi

    if ! details_output="$(codesign_details "${bundle_path}")"; then
        log_info "Couldn't inspect ${bundle_label} signature details: ${details_output}"
        return 1
    fi

    if printf '%s\n' "${details_output}" | grep -q 'Signature=adhoc'; then
        log_info "${bundle_label} is still ad hoc signed, which Tahoe may refuse during HAL reload."
        return 1
    fi

    if ! printf '%s\n' "${details_output}" | grep -q 'TeamIdentifier=' || \
       printf '%s\n' "${details_output}" | grep -q 'TeamIdentifier=not set'; then
        log_info "${bundle_label} does not have a TeamIdentifier. Refusing unsafe HAL reload."
        return 1
    fi

    return 0
}

validate_installed_products_before_hal_restart() {
    if [[ "${BGM_ALLOW_UNVERIFIED_HAL_RESTART:-0}" == "1" ]]; then
        log_info "WARNING: Skipping HAL restart signature validation because BGM_ALLOW_UNVERIFIED_HAL_RESTART=1."
        return 0
    fi

    verify_installed_bundle_for_hal_restart "${DRIVER_PATH}/${DRIVER_DIR}" "Background Music Device.driver"
    verify_installed_bundle_for_hal_restart "${XPC_HELPER_OUTPUT_PATH}/${XPC_HELPER_DIR}" "BGMXPCHelper.xpc"
    verify_installed_bundle_for_hal_restart "${APP_PATH}/${APP_DIR}" "Background Music.app"
}

find_browser_audio_session_pids() {
    local current_uid="$(id -u)"

    ps -axo pid=,uid=,args= | \
        awk -v current_uid="${current_uid}" '
            $2 == current_uid {
                cmd = ""
                for (i = 3; i <= NF; ++i) {
                    cmd = cmd $i " "
                }

                if (cmd ~ /audio\.mojom\.AudioService/ ||
                    cmd ~ /com\.apple\.WebKit\.(GPU|WebContent)/) {
                    print $1
                }
            }
        ' | sort -u
}

declare -a SUSPENDED_BROWSER_AUDIO_PIDS=()

suspend_browser_audio_clients_before_hal_restart() {
    SUSPENDED_BROWSER_AUDIO_PIDS=()

    while IFS= read -r pid; do
        [[ -n "${pid}" ]] && SUSPENDED_BROWSER_AUDIO_PIDS+=("${pid}")
    done < <(find_browser_audio_session_pids)

    if [[ ${#SUSPENDED_BROWSER_AUDIO_PIDS[@]} -eq 0 ]]; then
        return 0
    fi

    log_info "Temporarily suspending browser audio clients before restarting coreaudiod: ${SUSPENDED_BROWSER_AUDIO_PIDS[*]}"
    kill -STOP "${SUSPENDED_BROWSER_AUDIO_PIDS[@]}" >> ${LOG_FILE} 2>&1 || true
    sleep 1
}

resume_suspended_browser_audio_clients() {
    if [[ ${#SUSPENDED_BROWSER_AUDIO_PIDS[@]} -eq 0 ]]; then
        return 0
    fi

    log_info "Resuming browser audio clients after the guarded coreaudiod restart."
    kill -CONT "${SUSPENDED_BROWSER_AUDIO_PIDS[@]}" >> ${LOG_FILE} 2>&1 || true
    SUSPENDED_BROWSER_AUDIO_PIDS=()
}

wait_for_coreaudiod_restart() {
    local old_pid="$1"
    local old_start_time="$2"
    local timeout_seconds="${3:-20}"
    local waited=0

    while [[ ${waited} -lt ${timeout_seconds} ]]; do
        local new_pid
        new_pid="$(pgrep -x coreaudiod | head -n1 || true)"

        if [[ -n "${new_pid}" ]]; then
            local new_start_time
            new_start_time="$(ps -o lstart= -p "${new_pid}" 2>/dev/null | sed 's/^ *//')"

            if [[ -z "${old_pid}" ]] || [[ "${new_pid}" != "${old_pid}" ]] || \
               [[ -n "${old_start_time}" && "${new_start_time}" != "${old_start_time}" ]]; then
                sleep 2
                return 0
            fi
        fi

        sleep 1
        waited=$((waited + 1))
    done

    return 1
}

restart_coreaudiod_guarded() {
    local old_pid
    old_pid="$(pgrep -x coreaudiod | head -n1 || true)"
    local old_start_time=""
    if [[ -n "${old_pid}" ]]; then
        old_start_time="$(ps -o lstart= -p "${old_pid}" 2>/dev/null | sed 's/^ *//')"
    fi

    suspend_browser_audio_clients_before_hal_restart
    trap 'resume_suspended_browser_audio_clients' RETURN

    if ! (sudo launchctl kickstart -k system/com.apple.audio.coreaudiod &>/dev/null || \
            sudo launchctl kill SIGTERM system/com.apple.audio.coreaudiod &>/dev/null || \
            sudo launchctl kill TERM system/com.apple.audio.coreaudiod &>/dev/null || \
            sudo launchctl kill 15 system/com.apple.audio.coreaudiod &>/dev/null || \
            sudo launchctl kill -15 system/com.apple.audio.coreaudiod &>/dev/null || \
            (sudo launchctl unload "${COREAUDIOD_PLIST}" &>/dev/null && \
                sudo launchctl load "${COREAUDIOD_PLIST}" &>/dev/null) || \
            sudo killall coreaudiod &>/dev/null); then
        return 1
    fi

    if ! wait_for_coreaudiod_restart "${old_pid}" "${old_start_time}" 20; then
        return 1
    fi

    resume_suspended_browser_audio_clients
    trap - RETURN
}

probe_bgm_device_once() {
    local profiler_output=""
    if capture_command_with_timeout 8 profiler_output system_profiler SPAudioDataType; then
        if [[ "${profiler_output}" =~ "Background Music" ]]; then
            return 0
        fi
    else
        log_info "system_profiler timed out while checking the HAL device after restart."
    fi

    local input_device_output=""
    if capture_command_with_timeout 8 input_device_output /usr/bin/xcrun swift pkg/ListInputDevices.swift; then
        [[ "${input_device_output}" =~ "BGMDevice" ]] && return 0
    else
        log_info "AVFoundation device probe timed out while checking the HAL device after restart."
    fi

    return 1
}

wait_for_bgm_device_after_restart() {
    local retries="${1:-5}"

    while [[ ${retries} -gt 0 ]]; do
        if probe_bgm_device_once; then
            return 0
        fi

        retries=$((retries - 1))
        [[ ${retries} -gt 0 ]] && sleep 2
    done

    return 1
}

find_signing_identity() {
    SIGNING_IDENTITY="${SIGNING_IDENTITY:-${BGM_CODESIGN_IDENTITY:-}}"

    if [[ -n ${SIGNING_IDENTITY:-} ]]; then
        return 0
    fi

    local available_identities
    available_identities="$(security find-identity -v -p codesigning 2>/dev/null || true)"

    SIGNING_IDENTITY="$(printf '%s\n' "${available_identities}" | \
        sed -n 's/.*"\(Apple Development:[^"]*\)".*/\1/p' | head -n1)"

    if [[ -z ${SIGNING_IDENTITY} ]]; then
        SIGNING_IDENTITY="$(printf '%s\n' "${available_identities}" | \
            sed -n 's/.*"\(Mac Development:[^"]*\)".*/\1/p' | head -n1)"
    fi

    if [[ -z ${SIGNING_IDENTITY} ]]; then
        SIGNING_IDENTITY="$(printf '%s\n' "${available_identities}" | \
            sed -n 's/.*"\(Developer ID Application:[^"]*\)".*/\1/p' | head -n1)"
    fi

    return 0
}

bgmapp_entitlements_path() {
    if [[ "${CONFIGURATION}" == "Release" ]]; then
        echo "BGMApp/BGMApp/BGMApp.entitlements"
    else
        echo "BGMApp/BGMApp/BGMApp-Debug.entitlements"
    fi
}

resign_installed_products_if_possible() {
    local launchd_plist="/Library/LaunchDaemons/com.bearisdriving.BGM.XPCHelper.plist"
    local current_user="$(id -un)"
    local current_group="$(id -gn)"
    local -a temporarily_user_owned_paths=(
        "${DRIVER_PATH}/${DRIVER_DIR}"
        "${XPC_HELPER_OUTPUT_PATH}/${XPC_HELPER_DIR}"
    )
    local restored_root_ownership=0

    restore_root_owned_products() {
        if [[ ${restored_root_ownership} -ne 0 ]]; then
            return 0
        fi

        echo "Restoring root ownership on the driver and helper." >> ${LOG_FILE}
        sudo chown -RH root:wheel "${temporarily_user_owned_paths[@]}" >> ${LOG_FILE} 2>&1
        restored_root_ownership=1
    }

    find_signing_identity

    if [[ -z ${SIGNING_IDENTITY} ]]; then
        echo "$(tput setaf 11)WARNING$(tput sgr0): Couldn't find an Apple/macOS code signing " \
             "certificate. Tahoe may reject the ad hoc signed app, helper and driver." \
             | tee -a ${LOG_FILE}
        return 0
    fi

    echo "Re-signing installed products with ${SIGNING_IDENTITY}." | tee -a ${LOG_FILE}

    # codesign needs access to the developer certificate's private key, which lives in the current
    # user's login keychain rather than root's keychain. Temporarily hand the installed root-owned
    # bundles to the invoking user, sign them as that user, then restore root ownership before
    # launchd/coreaudiod use them.
    trap restore_root_owned_products RETURN
    sudo chown -R "${current_user}:${current_group}" "${temporarily_user_owned_paths[@]}" \
         >> ${LOG_FILE} 2>&1

    /usr/bin/codesign --force --sign "${SIGNING_IDENTITY}" \
                      --options runtime \
                      "${DRIVER_PATH}/${DRIVER_DIR}" >> ${LOG_FILE} 2>&1

    /usr/bin/codesign --force --sign "${SIGNING_IDENTITY}" \
                      --options runtime \
                      "${XPC_HELPER_OUTPUT_PATH}/${XPC_HELPER_DIR}" \
                      >> ${LOG_FILE} 2>&1

    /usr/bin/codesign --force --sign "${SIGNING_IDENTITY}" \
                      --options runtime \
                      --entitlements "$(bgmapp_entitlements_path)" \
                      "${APP_PATH}/${APP_DIR}" >> ${LOG_FILE} 2>&1

    restore_root_owned_products
    trap - RETURN

    # post_install.sh bootstraps the launchd job before Xcode's final signing step runs. Reload the
    # helper after re-signing so launchd sees the final executable with a Team ID.
    echo "Reloading BGMXPCHelper after re-signing." | tee -a ${LOG_FILE}
    sudo launchctl bootout system "${launchd_plist}" >> ${LOG_FILE} 2>&1 || \
        sudo launchctl unload "${launchd_plist}" >> ${LOG_FILE} 2>&1 || \
        true
    sudo launchctl bootstrap system "${launchd_plist}" >> ${LOG_FILE} 2>&1 || \
        sudo launchctl load "${launchd_plist}" >> ${LOG_FILE} 2>&1
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
    while getopts ":ndabwx:ch" opt; do
        case $opt in
            n)
                CLEAN=""
                ;;
            d)
                CONFIGURATION="Debug"
                ;;
            a)
                # The "archive" action makes a build for distribution. It's the same as the archive
                # option in Xcode. It won't install.
                XCODEBUILD_ACTION="archive"
                # The dirs xcodebuild will put the archives in.
                APP_PATH="$ARCHIVES_DIR/BGMApp.xcarchive"
                XPC_HELPER_OUTPUT_PATH="$ARCHIVES_DIR/BGMXPCHelper.xcarchive"
                DRIVER_PATH="$ARCHIVES_DIR/BGMDriver.xcarchive"
                ;;
            b)
                # Just build; don't install.
                XCODEBUILD_ACTION="build"
                # The dirs xcodebuild will build in.
                # TODO: If these dirs were created by running this script without -b, they'll be
                #       owned by root and xcodebuild will fail.
                APP_PATH="./BGMApp/build"
                XPC_HELPER_OUTPUT_PATH="./BGMApp/build"
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
        (! [[ -e /Library/Developer/CommandLineTools/usr/bin/git ]] && \
            ! pkgutil --pkg-info=com.apple.pkg.CLTools_Executables &>/dev/null); then
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

        echo "On git branch: $(git rev-parse --abbrev-ref HEAD 2>&1)" >> ${LOG_FILE}
        echo "Most recent commit: $(git rev-parse HEAD 2>&1)" \
             "(\"$(git show -s --format=%s HEAD 2>&1)\")" >> ${LOG_FILE}

        echo "Using xcodebuild: ${XCODEBUILD}" >> ${LOG_FILE}
        echo "Using BGMXPCHelper output path: ${XPC_HELPER_OUTPUT_PATH}" >> ${LOG_FILE}

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
    XPC_HELPER_OUTPUT_PATH="$(BGMApp/BGMXPCHelper/safe_install_dir.sh)"

    echo "$(bold_face About to install Background Music). Please pause all audio, if you can."
    [[ "${CONFIGURATION}" == "Debug" ]] && echo "Debug build."
    echo
    echo "This script will install:"
    echo " - ${APP_PATH}/${APP_DIR}"
    echo " - ${DRIVER_PATH}/${DRIVER_DIR}"
    echo " - ${XPC_HELPER_OUTPUT_PATH}/${XPC_HELPER_DIR}"
    echo " - /Library/LaunchDaemons/com.bearisdriving.BGM.XPCHelper.plist"
    echo
elif [[ "${XCODEBUILD_ACTION}" == "archive" ]]; then
    echo "$(bold_face Building and archiving Background Music...)"
    echo
else
    echo "$(bold_face Building Background Music...)"
    echo
fi

# Make sure Xcode and the command line tools are installed and recent enough.
# This sets XCODE_VERSION to major.minor, e.g. 8.3, or -1 if Xcode isn't installed.
get_xcode_version() {
    local XCODE_VERSION_FULL
    # Separated from the line below to workaround an intermittent bug in xcodebuild 16.1.
    XCODE_VERSION_FULL=$(${XCODEBUILD} -version 2>/dev/null || echo 'V -1')
    echo "${XCODE_VERSION_FULL}" | head -n 1 | awk '{ print $2 }'
}
XCODE_VERSION=$(get_xcode_version)
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

# Update the user's sudo timestamp if we're going to need to sudo at some point. This prompts the
# user for their password.
if [[ "${XCODEBUILD_ACTION}" == "install" ]]; then
    if ! sudo -v; then
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

log_debug_info "$@"

# Set some variables that control the compilation commands below.
if [[ "${XCODEBUILD_ACTION}" == "install" ]]; then
    SUDO="sudo"
    ACTIONING="Installing"
    DSTROOT_ARG="DSTROOT=/"
    # Work around an Xcode (15.2) bug where xcodebuild incorrectly detects a dependency cycle if
    # DSTROOT is set to /.
    for v in /Volumes/*; do
        if [[ "$(realpath "$v")" == "/" ]]; then
            DSTROOT_ARG="DSTROOT=$v"
            echo "Set DSTROOT_ARG to ${DSTROOT_ARG} to work around an Xcode bug." >> ${LOG_FILE}
            break
        fi
    done
    echo "DSTROOT_ARG: ${DSTROOT_ARG}." >> ${LOG_FILE}
elif [[ "${XCODEBUILD_ACTION}" == "archive" ]]; then
    SUDO=""
    ACTIONING="Building and archiving"
    DSTROOT_ARG=""
else
    # No need to sudo if we're only building.
    SUDO=""
    ACTIONING="Building"
    DSTROOT_ARG=""
fi

# Enable AddressSanitizer in debug builds to catch memory bugs. Allow ENABLE_ASAN to be set as an
# environment variable by only setting it here if it isn't already set. (Used by package.sh.)
if [[ "${CONFIGURATION}" == "Debug" ]]; then
    ENABLE_ASAN="${ENABLE_ASAN:-YES}"
else
    ENABLE_ASAN="${ENABLE_ASAN:-NO}"
fi

enableUBSanArg() {
    if [[ "${ENABLE_UBSAN+}" != "" ]]; then
        echo "-enableUndefinedBehaviorSanitizer"
        echo "$ENABLE_UBSAN"
    fi
}

# Clean all projects. Done separately to workaround what I think is a bug in Xcode 10.0. If you just
# add "clean" to the other xcodebuild commands, they seem to fail because of the DSTROOT="/" arg.
if [[ "${CLEAN}" != "" ]]; then
    if [[ "${XCODEBUILD_ACTION}" == "archive" ]]; then
        # Delete any previous archives to force Xcode to rebuild them.
        /bin/rm -rf "${ARCHIVES_DIR}" >> ${LOG_FILE} 2>&1
    fi

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

# Prints the -archivePath option if we're archiving (i.e. making a .xcarchive). Does nothing if not.
# Params:
#  - The name for the archive. The .xcarchive extension will be added.
archivePath() {
    if [[ "${XCODEBUILD_ACTION}" == "archive" ]]; then
        echo "-archivePath"
        echo "$ARCHIVES_DIR/$1"
    fi
}

# Prints the INSTALL_OWNER and INSTALL_GROUP arguments to use for the xcodebuild commands.
ownershipArgs() {
    if [[ "${XCODEBUILD_ACTION}" != "install" ]]; then
        # Stop xcodebuild from trying to chown the files in the archive to root when making an
        # archive, so we don't need to use sudo.
        echo "INSTALL_OWNER="
        echo "INSTALL_GROUP="
    fi
}

# BGMDriver

echo "[1/3] ${ACTIONING} the virtual audio device $(bold_face ${DRIVER_DIR}) to" \
         "$(bold_face ${DRIVER_PATH})" \
     | tee -a ${LOG_FILE}

(disable_error_handling
    # Build and, if requested, archive or install BGMDriver.
    ${SUDO} "${XCODEBUILD}" -scheme "Background Music Device" \
                            -configuration ${CONFIGURATION} \
                            -enableAddressSanitizer ${ENABLE_ASAN} \
                            $(enableUBSanArg) \
                            $(archivePath BGMDriver) \
                            BUILD_DIR=./build \
                            RUN_CLANG_STATIC_ANALYZER=0 \
                            $(ownershipArgs) \
                            ${DSTROOT_ARG} \
                            ${XCODEBUILD_OPTIONS} \
                            "${XCODEBUILD_ACTION}" >> ${LOG_FILE} 2>&1) &

show_spinner "${BUILD_FAILED_ERROR_MSG}"

# BGMXPCHelper

echo "[2/3] ${ACTIONING} $(bold_face ${XPC_HELPER_DIR}) to $(bold_face ${XPC_HELPER_OUTPUT_PATH})" \
    | tee -a ${LOG_FILE}

xpcHelperInstallPathArg() {
    if [[ "${XCODEBUILD_ACTION}" == "install" ]]; then
        echo "INSTALL_PATH=${XPC_HELPER_OUTPUT_PATH}"
    fi
}

# The Xcode project file is configured so that xcodebuild will call post_install.sh after it
# finishes building. It calls post_install.sh with the ACTION env var set to "install" when
# XCODEBUILD_ACTION is set to "archive" here. The REAL_ACTION arg in this command is a workaround
# that lets post_install.sh know when we're archiving.
(disable_error_handling
    ${SUDO} "${XCODEBUILD}" -scheme BGMXPCHelper \
                            -configuration ${CONFIGURATION} \
                            -enableAddressSanitizer ${ENABLE_ASAN} \
                            $(enableUBSanArg) \
                            $(archivePath BGMXPCHelper) \
                            BUILD_DIR=./build \
                            RUN_CLANG_STATIC_ANALYZER=0 \
                            $(xpcHelperInstallPathArg) \
                            $(ownershipArgs) \
                            REAL_ACTION="${XCODEBUILD_ACTION}" \
                            ${DSTROOT_ARG} \
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
                            $(enableUBSanArg) \
                            $(archivePath BGMApp) \
                            BUILD_DIR=./build \
                            RUN_CLANG_STATIC_ANALYZER=0 \
                            $(ownershipArgs) \
                            ${DSTROOT_ARG} \
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

    resign_installed_products_if_possible

    ERROR_MSG="Refusing to restart coreaudiod because the installed HAL artifacts failed signature validation. Re-sign them with a Team ID, or set BGM_ALLOW_UNVERIFIED_HAL_RESTART=1 to override at your own risk."
    validate_installed_products_before_hal_restart

    # Restart coreaudiod.

    log_info "Restarting coreaudiod with browser audio clients quiesced to load the virtual audio device."
    ERROR_MSG="Guarded coreaudiod restart failed. Suspended browser audio clients were resumed, but the HAL may still be mid-reload. Close active browser audio playback (especially Chromium audio.mojom.AudioService) and retry."
    restart_coreaudiod_guarded

    ERROR_MSG="Background Music Device didn't appear after the guarded coreaudiod restart. Close active audio playback or reboot before retrying."
    wait_for_bgm_device_after_restart

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

    echo "Done."
elif [[ "${XCODEBUILD_ACTION}" == "archive" ]]; then
    # Copy the dSYMs (debug symbols) into the correct directories in the archives. I haven't been
    # able to figure out why Xcode isn't doing this automatically.
    cp -r "BGMDriver/build/Release/Background Music Device.driver.dSYM" "$DRIVER_PATH/dSYMs"
    cp -r "BGMApp/build/Release/BGMXPCHelper.xpc.dSYM" "$XPC_HELPER_OUTPUT_PATH/dSYMs"
    mv "$APP_PATH/Products/Applications/Background Music.app/Contents/MacOS/Background Music.dSYM" \
       "$APP_PATH/dSYMs"
fi
