<!-- vim: set tw=120: -->

# Manual Uninstall

- Delete `Background Music.app` from `/Applications`.
- Delete `Background Music Device.driver` from `/Library/Audio/Plug-Ins/HAL`.
- Pause apps that are playing audio, if you can.
- Restart `coreaudiod`:
  <sup>(Open `/Applications/Utilities/Terminal.app` and paste the following at the prompt.)</sup>

  ```shell
  sudo launchctl kill SIGTERM system/com.apple.audio.coreaudiod || sudo killall coreaudiod
  ```
- Go to the Sound section in System Preferences and change your default output device at least once. (If you only have
  one device now, either use `Audio MIDI Setup.app` to create a temporary aggregate device, restart any audio apps that
  have stopped working or just restart your system.)

## Optional

- Delete `BGMXPCHelper.xpc` from `/usr/local/libexec` or possibly `/Library/Application Support/Background Music`.
- Unregister BGMXPCHelper, delete its user and group, and delete its launchd.plist:

  ```shell
  sudo launchctl bootout system /Library/LaunchDaemons/com.bearisdriving.BGM.XPCHelper.plist
  sudo rm /Library/LaunchDaemons/com.bearisdriving.BGM.XPCHelper.plist
  sudo dscl . -delete /Users/_BGMXPCHelper
  sudo dscl . -delete /Groups/_BGMXPCHelper
  ```


