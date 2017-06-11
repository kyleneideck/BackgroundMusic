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
# safe_install_dir.sh
# BGMXPCHelper
#
# Copyright © 2016, 2017 Kyle Neideck
#
# Prints the path to a directory the BGMXPCHelper bundle can safely be installed to. Intended to be
# used as the INSTALL_DIR environment variable for xcodebuild commands. For example,
#     xcodebuild -project BGMApp/BGMApp.xcodeproj -target BGMXPCHelper -configuration Debug \
#     DSTROOT="/" INSTALL_DIR="$(BGMApp/BGMXPCHelper/safe_install_dir.sh)" install
#
# Instead of setting INSTALL_DIR, we could just move the installed bundle in post_install.sh, but
# then we would have to leave an unused copy of the bundle in the default installation directory. If
# we didn't, xcodebuild would fail in one of the post-processing steps it does after running
# post_install.sh.
#
# The default installation directory comes from the Daemonomicon
# <https://developer.apple.com/library/mac/technotes/tn2083/_index.html>, which recommends
# installing daemons to /usr/local/libexec. But that isn't safe on many users' systems, because the
# Daemonomicon also recommends that
#
# "[...] daemons be owned by root, have an owning group of wheel, and use permissions 755
# (rwxr-xr-x) for executables and directories, and 644 (rw-r--r--) for files. In addition, every
# directory from your daemon up to the root directory must be owned by root and only writable by the
# owner (or owned by root and sticky). If you don't do this correctly, a non-admin user might be
# able to escalate their privileges by modifying your daemon (or shuffling it aside)."
#
# BGMXPCHelper runs as the unprivileged _BGMXPCHelper user, so I don't think it would be a security
# risk right now, but I could be wrong about that. We could also forget about this and give the
# _BGMXPCHelper privileges of some kind in a later version.
#
# Installing to /usr/local/libexec would be fine on a default OS X install, which doesn't have a
# /usr/local directory, but if the user has Homebrew installed it will have set them as the owner of
# /usr/local (even if it already existed and was owned by root). So if the owner or permissions for
# /usr/local/libexec aren't what we want we try /Library/Application Support instead.
#
# If given a directory as an argument, this script will print "1" if the directory meets the
# recommendation above, or "0" otherwise.
#

PATH=/bin:/sbin:/usr/bin:/usr/sbin; export PATH

# Safe mode.
set -euo pipefail
IFS=$'\n\t'

# Checks that a directory, and its parent directories, are owned by root and are only writable by
# root.
#
# Takes one param, the directory to check. Sets DIR_IS_SAFE=1 if the directory is suitable.
check_dir() {
    # Remember the current directory so we can come back here at the end of the function.
    pushd . > /dev/null

    # Normalize the path and follow symlinks.
    REAL_PATH=$(python -c "import os,sys; print(os.path.realpath(sys.argv[1]))" "$1")
    cd "${REAL_PATH}"

    DIR_IS_SAFE=0

    # While the current directory's owner has UID 0...
    while [[ "$(stat -f '%u' .)" == 0 ]] && \
        # ...and isn't writable by the group or others...
        # (The stat command prints the current directory's permissions in octal, e.g. 755. We add a
        # leading 0 so Bash will interpret it as an octal value.)
        [[ $((0$(stat -f '%Lp' .) & 0022)) -eq 0 ]]; do
        # ...go upwards until we reach the root directory.
        cd ..
        if [[ "${PWD}" == / ]]; then
            DIR_IS_SAFE=1
            break
        fi
    done

    # Go back to the directory we were in before this function was called. (No real reason to do
    # this yet, but it seemed like good practice.)
    popd > /dev/null
}

# Used when we can't find a suitable installation directory. Prints an error message and exits.
# (Reaching this point should be very uncommon.)
fail() {
    if [[ $ALLOW_UNSAFE_FALLBACK -eq 1 ]]; then
        CONTINUE_ANYWAY="y"
    else
        echo "$(tput setaf 11)WARNING$(tput sgr0): Installing BGMXPCHelper to its default" \
             "location (${INSTALL_DIR} or, as a backup, ${BACKUP_INSTALL_DIR}) might not be" \
             "secure on this system. It's recommended that each directory from the installation" \
             "directory up to the root directory should be owned by root and not writable by any" \
             "other user. See safe_install_dir.sh for more details." >&2

        read -e -p "Continue anyway? [y/N]" CONTINUE_ANYWAY
    fi

    if [[ "${CONTINUE_ANYWAY}" == "y" ]] || [[ "${CONTINUE_ANYWAY}" == "Y" ]]; then
        echo "${INSTALL_DIR}"
        exit 0
    else
        # Stops xcodebuild if the output is being used to set xcodebuild's INSTALL_DIR variable.
        echo "/dev/null"
        exit 1
    fi
}

ALLOW_UNSAFE_FALLBACK=0

# This script can be given a directory to check as an argument, or the -y option, which tells this
# script to print the default dir if neither of the dirs are safe. The pkg installer uses -y so it
# can install anyway and show the user instructions to fix the permissions, rather than just
# failing.
#
# (This line uses "${1+x}" instead of "$1" because having our "safe mode" enabled makes the script
# fail if you reference an unset variable, even to check whether it's set or not.)
if [[ ! -z "${1+x}" ]]; then
    if [[ "$1" == "-y" ]]; then
        ALLOW_UNSAFE_FALLBACK=1
    else
        # Check the given path exists and is a directory.
        if [[ ! -d "$1" ]]; then echo "$1 is not a directory." >&2; exit 1; fi

        check_dir "$1"
        echo ${DIR_IS_SAFE}
        exit 0
    fi
fi

# These are just for readability and to save keystrokes. If you change them, you'll have to change
# other parts of the code as well.
INSTALL_DIR="/usr/local/libexec"
BACKUP_INSTALL_DIR="/Library/Application Support/Background Music"

# Create the installation directory if it doesn't exist already.
if [[ ! -e /usr/local ]]; then
    sudo mkdir /usr/local
    sudo chown root:wheel /usr/local 
    sudo chmod go-w /usr/local 
fi
if [[ ! -e "${INSTALL_DIR}" ]]; then
    sudo mkdir "${INSTALL_DIR}"
    sudo chown root:wheel "${INSTALL_DIR}" 
    sudo chmod go-w "${INSTALL_DIR}" 
fi

# Check the directory the build was installed to.
check_dir "${INSTALL_DIR}"

if [[ ${DIR_IS_SAFE} -eq 1 ]]; then
    FINAL_INSTALL_DIR="${INSTALL_DIR}"
else
    # Try the backup directory instead.

    # Make sure /Library/Application Support exists.
    if [[ ! -e "/Library/Application Support" ]]; then fail; fi

    # Check /Library/Application Support before adding our directory.
    check_dir "/Library/Application Support"
    if [[ ${DIR_IS_SAFE} -ne 1 ]]; then fail; fi

    # Add a "Background Music" directory to /Library/Application Support.
    # (Check whether it exists first so we don't sudo unless we need to.)
    if [[ ! -e "${BACKUP_INSTALL_DIR}" ]]; then
        sudo mkdir "${BACKUP_INSTALL_DIR}"
        sudo chown root:wheel "${BACKUP_INSTALL_DIR}" 
        sudo chmod go-w "${BACKUP_INSTALL_DIR}" 
    fi

    # Check the backup directory just to be sure.
    check_dir "${BACKUP_INSTALL_DIR}"
    if [[ ${DIR_IS_SAFE} -ne 1 ]]; then fail; fi

    FINAL_INSTALL_DIR="${BACKUP_INSTALL_DIR}"
fi

echo "${FINAL_INSTALL_DIR}"


