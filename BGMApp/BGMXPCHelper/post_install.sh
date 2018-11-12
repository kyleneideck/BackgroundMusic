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
# Copyright © 2016-2018 Kyle Neideck
#
# Installs BGMXPCHelper's launchd plist file and "bootstraps" (registers/enables) it with launchd.
#
# When you install BGMXPCHelper with xcodebuild (or Xcode itself, if that's possible) this script
# runs as the final build phase.
#

PATH=/bin:/sbin:/usr/bin:/usr/sbin; export PATH

# Check we have the paths we need, either in environment variables from Xcode or from the args.
if [[ -z $1 ]]; then
    if [[ -z ${INSTALL_DIR} ]]; then
        echo "Environment variable INSTALL_DIR was not set." >&2
        exit 1
    fi
else
    INSTALL_DIR="$1"
fi

if [[ -z $2 ]]; then
    if [[ -z ${EXECUTABLE_PATH} ]]; then
        echo "Environment variable EXECUTABLE_PATH was not set." >&2
        exit 1
    fi
else
    EXECUTABLE_PATH="$2"
fi

if [[ -z $3 ]]; then
    if [[ -z ${TARGET_BUILD_DIR} ]]; then
        echo "Environment variable TARGET_BUILD_DIR was not set." >&2
        exit 1
    fi

    if [[ -z ${UNLOCALIZED_RESOURCES_FOLDER_PATH} ]]; then
        echo "Environment variable UNLOCALIZED_RESOURCES_FOLDER_PATH was not set." >&2
        exit 1
    fi

    RESOURCES_PATH="${TARGET_BUILD_DIR}/${UNLOCALIZED_RESOURCES_FOLDER_PATH}"
else
    RESOURCES_PATH="$3"
fi

# If DEPLOYMENT_POSTPROCESSING is true, xcodebuild calls this script even if you're just building
# (and not also installing). I'm not sure why, as we have the "run script only when installing"
# option enabled.
if ! [[ -z ${ACTION} ]] && [[ "${ACTION}" != "install" ]]; then
    echo "$0 should only be called during an install. Exiting."
    exit 0
fi

# Safe mode.
set -euo pipefail
IFS=$'\n\t'

# Show a warning if INSTALL_DIR isn't set to a safe installation directory.
if [[ $(bash "${RESOURCES_PATH}/safe_install_dir.sh" "${INSTALL_DIR}") != 1 ]]; then
    echo "$(tput setaf 11)WARNING$(tput sgr0): Installing to \"${INSTALL_DIR}\" may be" \
         "insecure. See safe_install_dir.sh for details." >&2
fi

HELPER_USER=_BGMXPCHelper

LAUNCHD_PLIST_INSTALL_PATH=/Library/LaunchDaemons
LAUNCHD_PLIST_FILENAME=com.bearisdriving.BGM.XPCHelper.plist
LAUNCHD_PLIST="${LAUNCHD_PLIST_INSTALL_PATH}/${LAUNCHD_PLIST_FILENAME}"

# Create an unprivileged user for BGMXPCHelper to run as.

if [[ "$(dscl . -search /Users RecordName ${HELPER_USER})" == "" ]]; then
    # Find an unused UID and GID. UIDs from 0 to 500 are reserved by OSX.
    HELPER_UID=$(dscl . -list /Users UniqueID | \
        awk '{ a[$2] } END { for (i=501; i in a; i++); print i }')
    HELPER_GID=$(dscl . -list /Groups PrimaryGroupID | \
        awk '{ a[$2] } END { for (i='${HELPER_UID}'; i in a; i++); print i }')

    # Check the UID and GID.
    NUMERIC_ID_REGEX='^[1-9][0-9]*$'

    ([[ "${HELPER_UID}" =~ ${NUMERIC_ID_REGEX} ]] && \
        [[ ${HELPER_UID} -gt 500 ]] && \
        [[ "$(dscl . -search /Users UniqueID ${HELPER_UID})" == "" ]]) || \
        (echo "Internal error. Failed to generate a user ID. HELPER_UID=${HELPER_UID}" >&2; exit 1)

    ([[ "${HELPER_GID}" =~ ${NUMERIC_ID_REGEX} ]] && \
        [[ ${HELPER_GID} -gt 500 ]] && \
        [[ "$(dscl . -search /Groups PrimaryGroupID ${HELPER_GID})" == "" ]]) || \
        (echo "Internal error. Failed to generate a group ID. HELPER_GID=${HELPER_GID}" >&2; exit 1)

    # Create the group.
    sudo dscl . -create /Groups/${HELPER_USER} PrimaryGroupID ${HELPER_GID}
    sudo dscl . -create /Groups/${HELPER_USER} RealName "Background Music XPC Helper Group"
    sudo dscl . -create /Groups/${HELPER_USER} Password '*'

    # Create the user.
    sudo dscl . -create /Users/${HELPER_USER} UniqueID ${HELPER_UID}
    sudo dscl . -create /Users/${HELPER_USER} PrimaryGroupID ${HELPER_GID}
    sudo dscl . -create /Users/${HELPER_USER} RealName "Background Music XPC Helper"
    sudo dscl . -create /Users/${HELPER_USER} Password '*'
    sudo dscl . -create /Users/${HELPER_USER} UserShell /usr/bin/false
    sudo dscl . -create /Users/${HELPER_USER} NFSHomeDirectory /var/empty
    sudo dscl . -delete /Users/${HELPER_USER} AuthenticationAuthority

    # Add the user to the group
    sudo dscl . -append /Groups/${HELPER_USER} GroupMembership ${HELPER_USER}

    # Check the user and group were created.
    [[ "$(dscl . -search /Users RecordName ${HELPER_USER})" != "" ]] || \
        (echo "Internal error. Failed to ${HELPER_USER} user." >&2; exit 1)
    [[ "$(dscl . -search /Groups RecordName ${HELPER_USER})" != "" ]] || \
        (echo "Internal error. Failed to ${HELPER_USER} group." >&2; exit 1)

    echo "Created ${HELPER_USER} user."
fi

# Copy the plist template into place.
sudo cp "${RESOURCES_PATH}/${LAUNCHD_PLIST_FILENAME}.template" "${LAUNCHD_PLIST}"

# Set the plist's owner and permissions. (Probably not necessary, but just in case.)
sudo chown root:wheel "${LAUNCHD_PLIST}"
sudo chmod 0644 "${LAUNCHD_PLIST}"

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
sudo sed -i.tmp "s/{{BGMXPCHELPER_USER_NAME}}/${HELPER_USER}/g" "${LAUNCHD_PLIST}"
sudo sed -i.tmp "s/{{BGMXPCHELPER_GROUP_NAME}}/${HELPER_USER}/g" "${LAUNCHD_PLIST}"

# Remove template-only comments.
sudo sed -i.tmp 's/{{#.*#}}//g' "${LAUNCHD_PLIST}"

# Clean up sed's temporary file.
sudo rm -f "${LAUNCHD_PLIST}.tmp"

echo "Installed ${LAUNCHD_PLIST_FILENAME} to ${LAUNCHD_PLIST_INSTALL_PATH}."

# Unregister the plist. This disables the existing version of BGMXPCHelper.

echo "Unregistering ${LAUNCHD_PLIST}. This will print errors if you don't already have a version" \
     "of it registered, or your system uses an older version of launchctl. They're safe to ignore."
echo "----"

# The fallback versions of this command are for OS X 10.10 and 10.9, respectively. The "|| true"
# part is so the command can fail, which it does if the plist isn't already installed, without -e
# killing the script.
sudo launchctl bootout system "${LAUNCHD_PLIST}" || \
    sudo launchctl unbootstrap system "${LAUNCHD_PLIST}" || \
    sudo launchctl unload "${LAUNCHD_PLIST}" || \
    true

echo "----"

# Register the plist with launchd. This enables BGMXPCHelper.

# The fallback version of this command is for OS X 10.9.
sudo launchctl bootstrap system "${LAUNCHD_PLIST}" || \
    sudo launchctl load "${LAUNCHD_PLIST}"

echo "Started the BGMXPCHelper service."


