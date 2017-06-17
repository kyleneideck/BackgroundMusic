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
# Copyright © 2016, 2017 Kyle Neideck
#
# Removes BGMApp, BGMDriver and BGMXPCHelper from the system.
#

# Halt on errors.
set -e

PATH=/bin:/sbin:/usr/bin:/usr/sbin; export PATH

bold=$(tput bold)
normal=$(tput sgr0)

# Warn if running as root.
if [[ $(id -u) -eq 0 ]]; then
  echo "$(tput setaf 11)WARNING$(tput sgr0): This script is not intended to be run as root. Run" \
       "it normally and it'll sudo when it needs to." >&2
  echo ""
fi

echo "${bold}You are about to uninstall Background Music.${normal}"
echo "Please pause all audio before continuing."
echo ""
read -p "Continue (y/N)? " user_prompt

if [ "$user_prompt" == "y" ] || [ "$user_prompt" == "Y" ]; then
  # Run from the dir containing this script.
  cd "$( dirname "${BASH_SOURCE[0]}" )"

  if [ -f "BGMApp/BGMApp/_uninstall-non-interactive.sh" ]; then
    # Running from the source directory.
    bash "BGMApp/BGMApp/_uninstall-non-interactive.sh"
  elif [ -f "_uninstall-non-interactive.sh" ]; then
    # Probably running from Background Music.app/Contents/Resources.
    bash "_uninstall-non-interactive.sh"
  else
    echo "${bold}ERROR: Could not find _uninstall-non-interactive.sh${normal}" >&2
    exit 1
  fi

  # Invalidate sudo ticket
  sudo -k

  # Open System Preferences and go to Sound > Output.
  osascript -e 'tell application id "com.apple.systempreferences"
                  activate
                  reveal anchor "output" of pane id "com.apple.preference.sound"
                end tell' >/dev/null || true
  echo ""

else
  echo "Uninstall cancelled."
fi


