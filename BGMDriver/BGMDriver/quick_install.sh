#!/bin/bash
# vim: tw=0:

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
# quick_install.sh
# BGMDriver
#
# Copyright © 2016 Kyle Neideck
#
# Installs the HAL plugin to /Library/Audio/Plug-Ins/HAL and restarts coreaudiod to enable it.
# For testing/debugging.
#
# If BGMDevice is set as your default device when you run this, you might have to change your
# default device and change it back to BGMDevice to get programs that use audio working again.
# Or you can just restart those programs.
#

# Safe mode
set -euo pipefail
IFS=$'\n\t'

# The dir containing this script
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
SCRIPT_FILENAME="$( basename "$0" )"
# Use the same filename as this script but change the extension to .conf
CONFIG_FILENAME="${SCRIPT_FILENAME%%.*}.conf"
# This config file just stores the paths to your BGMDriver builds (at least so far)
CONFIG_FILE="${SCRIPT_DIR}/${CONFIG_FILENAME}"

# Sanity check
if [[ ! -f "${SCRIPT_DIR}/${SCRIPT_FILENAME}" ]]; then
    echo "Assertion failed. Could not find this script ($0) in SCRIPT_DIR (${SCRIPT_DIR})." >&2
    exit 1
fi

bold_face() {
    echo $(tput bold)$*$(tput sgr0)
}

usage() {
    echo "Usage: $0 [options]" >&2
    echo -e "\t-u\tUninstall only" >&2
    echo -e "\t-d\tDon't restart coreaudiod" >&2
    echo -e "\t-r\tInstall release build (debug is the default)" >&2
    echo -e "\t-h\tPrint this usage statement" >&2
    exit 1
}

# Read in the config variables. (One var per line, formatted like "VARIABLE_NAME=value")
read_quick_install_conf() {
    # Set config vars to the empty string if they aren't set in the config file
    DRIVER_PATH_RELEASE=
    DRIVER_PATH_DEBUG=

    if [[ -f "${CONFIG_FILE}" ]]; then
        while IFS='' read -r LINE || [[ -n "${LINE}" ]]; do
            # Take the chars before the first "=" to get the var name
            # ("%%" deletes the longest substring match, starting at the end of the string)
            if [[ "DRIVER_PATH_DEBUG" = "${LINE%%=*}" ]]; then
                # Remove the "VARIABLE_NAME=" part to get the value
                DRIVER_PATH_DEBUG="${LINE/DRIVER_PATH_DEBUG=}"
            fi

            if [[ "DRIVER_PATH_RELEASE" = "${LINE%%=*}" ]]; then
                DRIVER_PATH_RELEASE="${LINE/DRIVER_PATH_RELEASE=}"
            fi
        done < "${CONFIG_FILE}"
    else
        touch "${CONFIG_FILE}"
    fi
}

# Get the path to the build we're installing, from the user or their config, and set DRIVER_PATH to it
get_build_path() {
    if [[ ${USE_RELEASE_BUILD} == true ]] && [[ "${DRIVER_PATH_RELEASE}" != "" ]] && [[ -d "${DRIVER_PATH_RELEASE}" ]]; then
        # The path to the release build was set, so just set it as the build we're installing
        DRIVER_PATH="${DRIVER_PATH_RELEASE}"
    elif [[ ${USE_RELEASE_BUILD} == false ]] && [[ "${DRIVER_PATH_DEBUG}" != "" ]] && [[ -d "${DRIVER_PATH_DEBUG}" ]]; then
        # The path to the debug build was set, so just set it as the build we're installing
        DRIVER_PATH="${DRIVER_PATH_DEBUG}"
    else
        # The build path isn't set yet, so ask the user to enter it
        echo -n "Enter the absolute path to your BGMDriver"
        ([[ ${USE_RELEASE_BUILD} == true ]] && echo -n " release ") || echo -n " debug "
        echo "build. Probably something like"
        echo -n "/Users/$(whoami)/Library/Developer/Xcode/DerivedData/BGM-somerandomchars/Build/Products/"
        ([[ ${USE_RELEASE_BUILD} == true ]] && echo -n "Release") || echo -n "Debug"
        echo "/Background Music Device.driver"
        cd /
        read -e -p ": /" DRIVER_PATH
        cd -

        # Add leading "/", strip trailing "/"
        DRIVER_PATH="/${DRIVER_PATH%/}"

        # Offer to abort if they entered an invalid path
        if [[ ! -d "${DRIVER_PATH}/Contents/MacOS" ]]; then
            if [[ ! -d "${DRIVER_PATH}" ]]; then
                echo "$(bold_face Warning): this path doesn't seem to be a directory. Continue anyway?" >&2
            else
                echo "$(bold_face Warning): this directory doesn't seem to be a bundle. Continue anyway?" >&2
            fi

            read -p "[y/N]: " CONTINUE_ANYWAY

            if [[ "${CONTINUE_ANYWAY}" != "y" ]]; then
                exit 1
            fi
        fi

        # Write the path to the config file
        if [[ ${USE_RELEASE_BUILD} == true ]]; then
            echo "DRIVER_PATH_RELEASE=${DRIVER_PATH}" >> "${CONFIG_FILE}"
        else
            echo "DRIVER_PATH_DEBUG=${DRIVER_PATH}" >> "${CONFIG_FILE}"
        fi
    fi
}

UNINSTALL_ONLY=false
DONT_RESTART_COREAUDIOD=false
USE_RELEASE_BUILD=false

# Parse options

while getopts ":udrh" opt; do
    case $opt in
        u)
            UNINSTALL_ONLY=true
            ;;
        d)
            DONT_RESTART_COREAUDIOD=true
            ;;
        r)
            USE_RELEASE_BUILD=true
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

# Require sudo

sudo -v

# Get the path to the build we're installing

if [[ ${UNINSTALL_ONLY} == false ]]; then
    read_quick_install_conf
    get_build_path
fi

# Remove installed build

HAL_PLUGINS_DIR="/Library/Audio/Plug-Ins/HAL"
INSTALLED_DRIVER_PATH="${HAL_PLUGINS_DIR}/Background Music Device.driver"

if [[ -d "${INSTALLED_DRIVER_PATH}" ]]; then
    echo "$(bold_face Removing old version of the driver)"

    # Sanity check
    INSTALLED_DRIVER_SIZE_KB="$(du -sk "${INSTALLED_DRIVER_PATH}" | cut -f1)"
    if [[ ! "${INSTALLED_DRIVER_SIZE_KB}" =~ ^-?[0-9]+$ ]] || [[ "${INSTALLED_DRIVER_SIZE_KB}" -gt 2048 ]]; then
        echo "$(bold_face Aborting). The driver currently installed in \"${INSTALLED_DRIVER_PATH}\" was much larger than expected (>2MB)." >&2
        exit 1
    fi

    # ("set -x" so the command is echoed)
    (set -x; sudo rm -rf "${INSTALLED_DRIVER_PATH}")
fi

# Install new build

if [[ ${UNINSTALL_ONLY} == false ]]; then
    echo -n "$(bold_face Installing driver) - "
    ([[ ${USE_RELEASE_BUILD} == true ]] && echo -n "release ") || echo -n "debug "
    echo "build. (Edit or delete ${CONFIG_FILENAME} to change the path to your build.)"

    # ("set -x" so the command is echoed)
    (set -x; sudo cp -r "${DRIVER_PATH}" "${HAL_PLUGINS_DIR}")
fi

# Restart coreaudiod to load the installed build

if [[ ${DONT_RESTART_COREAUDIOD} == false ]]; then
    echo "$(bold_face Restarting coreaudiod). If a running app stops playing audio, change your default audio device (and change it back if you want) or open BGMApp."

    # ("set -x" so the command is echoed)
    (set -x; sudo launchctl kill SIGTERM system/com.apple.audio.coreaudiod || sudo killall coreaudiod)
fi


