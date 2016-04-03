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
# post_install.sh
# BGMXPCHelper
#
# Copyright © 2016 Kyle Neideck
#
# Installs BGMXPCHelper's launchd plist file and "bootstraps" (registers/enables) it with launchd.
#
# When you install BGMXPCHelper with xcodebuild (Or Xcode itself. I can't figure out if that's
# possible) this script runs as the final build phase.
#

# Check the environment variables we need from Xcode.
if [[ -z ${EXECUTABLE_PATH} ]]; then
    echo "Environment variable EXECUTABLE_PATH was not set." >&2
    exit 1
fi
if [[ -z ${INSTALL_DIR} ]]; then
    echo "Environment variable INSTALL_DIR was not set." >&2
    exit 1
fi
if [[ -z ${TARGET_BUILD_DIR} ]]; then
    echo "Environment variable TARGET_BUILD_DIR was not set." >&2
    exit 1
fi
if [[ -z ${UNLOCALIZED_RESOURCES_FOLDER_PATH} ]]; then
    echo "Environment variable UNLOCALIZED_RESOURCES_FOLDER_PATH was not set." >&2
    exit 1
fi

# Safe mode.
set -euo pipefail
IFS=$'\n\t'

RESOURCES_PATH="${TARGET_BUILD_DIR}/${UNLOCALIZED_RESOURCES_FOLDER_PATH}"

# Show a warning if INSTALL_DIR isn't set to a safe installation directory.
if [[ $(bash "${RESOURCES_PATH}/safe_install_dir.sh" "${INSTALL_DIR}") != 1 ]]; then
    echo "$(tput setaf 11)WARNING$(tput sgr0): Installing to \"${INSTALL_DIR}\" may be" \
         "insecure. See safe_install_dir.sh for details." >&2
fi

LAUNCHD_PLIST_INSTALL_PATH=/Library/LaunchDaemons
LAUNCHD_PLIST_FILENAME=com.bearisdriving.BGM.XPCHelper.plist
LAUNCHD_PLIST=${LAUNCHD_PLIST_INSTALL_PATH}/${LAUNCHD_PLIST_FILENAME}

# Copy the plist template into place.
sudo cp "${RESOURCES_PATH}/${LAUNCHD_PLIST_FILENAME}.template" "${LAUNCHD_PLIST}"

# Replace the template variables in the plist.

# First, escape the / characters in the values because we're using / in our sed pattern. The
# solution usually recommended is to use | or # instead of /, but file and directory names can
# contain those characters so I think this solution is safer.
#
# This is using Bash substring replacement to replace every occurrence of "/" with "\/". The general
# form is ${string//substring/replacement}. (${string/substring/replacement} only replaces the first
# match.)
INSTALL_DIR_ESCAPED="${INSTALL_DIR//\//\\/}"
EXECUTABLE_PATH_ESCAPED="${EXECUTABLE_PATH//\//\\/}"

# Replace the variables in-place. They're formatted like "{{TEMPLATE_VARIABLE}}".
sudo sed -i.tmp "s/{{PATH_TO_BGMXPCHELPER}}/${INSTALL_DIR_ESCAPED}/g" "${LAUNCHD_PLIST}"
# EXECUTABLE_PATH is set by Xcode, currently to "BGMXPCHelper.xpc/Contents/MacOS/BGMXPCHelper".
sudo sed -i.tmp "s/{{BGMXPCHELPER_EXECUTABLE_PATH}}/${EXECUTABLE_PATH_ESCAPED}/g" "${LAUNCHD_PLIST}"
# Remove template-only comments.
sudo sed -i.tmp 's/{{#.*#}}//g' "${LAUNCHD_PLIST}"

# Clean up sed's temporary file.
sudo rm -f "${LAUNCHD_PLIST}.tmp"

echo "Installed to ${LAUNCHD_PLIST_FILENAME} to ${LAUNCHD_PLIST_INSTALL_PATH}."

# Unregister the plist. This disables the existing version of BGMXPCHelper. (We silence any output
# because this command fails if the plist isn't already registered. The "|| true" part is so the
# command can fail without our "safe mode" killing the script.)
sudo launchctl bootout system "${LAUNCHD_PLIST}" &> /dev/null || true

# Register the plist with launchd. This enables BGMXPCHelper.
sudo launchctl bootstrap system "${LAUNCHD_PLIST}"

echo "Started the BGMXPCHelper service."


