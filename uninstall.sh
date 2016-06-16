#!/bin/bash
# vim: tw=120:

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
# uninstall.sh
#
# Copyright © 2016 Nick Jacques
# Copyright © 2016 Kyle Neideck
#
# Removes BGMApp, BGMDriver and BGMXPCHelper from the system.
#

# TODO: Log commands and their output to uninstall.log, like build_and_install.sh does, rather than just sending
#       everything to /dev/null.

# TODO: Show a custom error message if the script fails, like build_and_install.sh.

# Halt on errors.
set -e

bold=$(tput bold)
normal=$(tput sgr0)

app_path="/Applications/Background Music.app"
driver_path="/Library/Audio/Plug-Ins/HAL/Background Music Device.driver"
xpc_path1="/usr/local/libexec/BGMXPCHelper.xpc"
xpc_path2="/Library/Application Support/Background Music/BGMXPCHelper.xpc"

# Check that files/directories are at most this big before we delete them, just to be safe.
max_size_mb_for_rm=5

file_paths=("${app_path}" "${driver_path}" "${xpc_path1}" "${xpc_path2}")

bgmapp_process_name="Background Music"

launchd_plist_label="com.bearisdriving.BGM.XPCHelper"
launchd_plist="/Library/LaunchDaemons/${launchd_plist_label}.plist"

coreaudiod_plist="/System/Library/LaunchDaemons/com.apple.audio.coreaudiod.plist"

user_group_name="_BGMXPCHelper"

# We move files to this temp directory and then move the directory to the user's trash at the end of the script.
# Unfortunately, this means that if the user tries to use the "put back" feature the files will just go back to the
# temp directory.
trash_dir="$(mktemp -d -t UninstalledBackgroundMusicFiles)/"

# Takes a path to a file or directory and returns false if the file/directory is larger than $max_size_mb_for_rm.
function size_check {
  local size="$(du -sm "$1" 2>/dev/null | awk '{ print $1 }')"
  [[ "${size}" =~ ^[0-9]+$ ]] && [[ "${size}" -le ${max_size_mb_for_rm} ]]
}

clear
echo "${bold}You are about to uninstall Background Music and its components!${normal}"
echo "Please pause all audio before continuing."
echo "You must be able to run 'sudo' commands to continue. (But don't worry if you don't know what that means.)"
echo ""
read -p "Continue (y/N)? " user_prompt

if [ "$user_prompt" == "y" ] || [ "$user_prompt" == "Y" ]; then

  # Ensure that the user can use sudo. (But not if this is a Travis CI build, because then it would fail.)
  if ([[ -z ${TRAVIS:-} ]] || [[ "${TRAVIS}" != true ]]) && ! sudo -v; then
    echo "ERROR: This script must be run by a user with administrator (sudo) privileges." >&2
    exit 1
  fi

  echo ""

  # Try to kill Background Music.app, in case it's running.
  killall "${bgmapp_process_name}" &>/dev/null || true

  # TODO: Use
  #         mdfind kMDItemCFBundleIdentifier = "com.bearisdriving.BGM.App"
  #       to offer alternatives if Background Music.app isn't installed to /Applications. Or we could open it with
  #         open -b "com.bearisdriving.BGM.App" -- delete-yourself
  #       and have Background Music.app delete itself and close when it gets the "delete-yourself" argument. Though
  #       that wouldn't be backwards compatible.

  # Remove the files defined in file_paths
  for path in "${file_paths[@]}"; do
    if [ -e "${path}" ] && size_check "${path}"; then
      echo "Moving \"${path}\" to the trash."
      sudo mv -f "${path}" "${trash_dir}" &>/dev/null
    fi
  done

  echo "Removing Background Music launchd service."
  sudo launchctl list | grep "${launchd_plist_label}" >/dev/null && \
    (sudo launchctl bootout system "${launchd_plist}" &>/dev/null || \
      # Try older versions of the command in case the user has an old version of launchctl.
      sudo launchctl unbootstrap system "${launchd_plist}" &>/dev/null || \
      sudo launchctl unload "${launchd_plist}" >/dev/null) || \
    echo "  Service does not exist."

  echo "Removing Background Music launchd service configuration file."
  if [ -e "${launchd_plist}" ]; then
    sudo mv -f "${launchd_plist}" "${trash_dir}"
  fi

  # Be paranoid about user_group_name because we really don't want to delete every user account.
  if ! [[ -z ${user_group_name} ]] && [[ "${user_group_name}" != "" ]]; then
    echo "Removing Background Music user."
    dscl . -read /Users/"${user_group_name}" &>/dev/null && \
      sudo dscl . -delete /Users/"${user_group_name}" 1>/dev/null || \
      echo "  User does not exist."

    echo "Removing Background Music group."
    dscl . -read /Groups/"${user_group_name}" &>/dev/null && \
      sudo dscl . -delete /Groups/"${user_group_name}" 1>/dev/null || \
      echo "  Group does not exist."
  else
    echo "Warning: could not delete the Background Music user/group due to an internal error in $0."
  fi

  # We're done removing files, so now actually move trash_dir into the trash.
  osascript -e 'tell application "Finder" to move the POSIX file "'"${trash_dir}"'" to trash' >/dev/null

  echo "Restarting Core Audio."
  # The extra or-clauses are fallback versions of the command that restarts coreaudiod. Apparently some of these commands 
  # don't work with older versions of launchctl, so I figure there's no harm in trying a bunch of different ways (which
  # should all work).
  (sudo launchctl kill SIGTERM system/com.apple.audio.coreaudiod &>/dev/null || \
    sudo launchctl kill TERM system/com.apple.audio.coreaudiod &>/dev/null || \
    sudo launchctl kill 15 system/com.apple.audio.coreaudiod &>/dev/null || \
    sudo launchctl kill -15 system/com.apple.audio.coreaudiod &>/dev/null || \
    (sudo launchctl unload "${coreaudiod_plist}" &>/dev/null && \
      sudo launchctl load "${coreaudiod_plist}" &>/dev/null) || \
    sudo killall coreaudiod &>/dev/null) && \
    sleep 5

  # Invalidate sudo ticket
  sudo -k

  # TODO: What if they only have one audio device?
  echo -e "\n${bold}Done! Toggle your sound output device in the Sound control panel to complete the uninstall.${normal}"

  osascript -e 'tell application "System Preferences"
	activate
	reveal anchor "output" of pane "Sound"
  end tell' >/dev/null
  echo ""

else
  echo "Uninstall cancelled."
fi


