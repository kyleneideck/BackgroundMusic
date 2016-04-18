#!/bin/bash -e

bold=$(tput bold)
normal=$(tput sgr0)

app_path="/Applications/Background Music.app"
driver_path="/Library/Audio/Plug-Ins/HAL/Background Music Device.driver"
xpc_path1="/usr/local/libexec/BGMXPCHelper.xpc"
xpc_path2="/Library/Application Support/Background Music/BGMXPCHelper.xpc"

file_paths=("${app_path}" "${driver_path}" "${xpc_path1}" "${xpc_path2}")

launchd_plist="/Library/LaunchDaemons/com.bearisdriving.BGM.XPCHelper.plist"

user_group_name="_BGMXPCHelper"

clear
echo "${bold}You are about to uninstall BackgroundMusic and its components!${normal}"
echo "Please pause all audio before continuing."
echo "You must be able to run 'sudo' commands to continue."
echo ""
read -p "Continue (y/n)? " user_prompt

if [ "$user_prompt" == "y" ]; then

  # Ensure that the user can use sudo
  sudo -v
  is_sudo=$?
  if [[ "$is_sudo" -ne 0 ]]; then
    echo "ERROR: This script must be run by a user with sudo permissions"
    exit 1
  fi

  echo ""

  # Remove the files defined in file_paths
  for path in "${file_paths[@]}"; do
    if [ -e "${path}" ]; then
      echo "Deleting \"${path}\""
      rm -rf "\"${path}\""
    fi
  done

  echo "Removing BackgroundMusic launchd service"
  launchctl list | grep "${launchd_plist}" >/dev/null && sudo launchctl bootout system "${launchd_plist}" || echo "  Service does not exist"

  echo "Removing BackgroundMusic launchd service configuration file"
  if [ -e "${launchd_plist}" ]; then
    sudo rm "${launchd_plist}"
  fi

  echo "Removing BackgroundMusic user"
  dscl . -read /Users/"${user_group_name}" 2>/dev/null && sudo dscl . -delete /Users/"${user_group_name}" || echo "  User does not exist"

  echo "Removing BackgroundMusic group"
  dscl . -read /Groups/"${user_group_name}" 2>/dev/null && sudo dscl . -delete /Groups/"${user_group_name}" || echo "  Group does not exist"

  echo "Restarting CoreAudio"
  sudo launchctl kill SIGTERM system/com.apple.audio.coreaudiod && sleep 5

  echo -e "\n${bold}Done! Toggle your sound output device in the Sound control panel to complete the uninstall.${normal}"
  osascript -e 'tell application "System Preferences"
	activate
	reveal anchor "output" of pane "Sound"
  end tell' >/dev/null
  echo ""

else
  echo "Uninstall cancelled."
fi
