<!-- vim: set tw=120: -->

# Manual Uninstall

- Delete `Background Music.app` from `/Applications`.
- Delete `Background Music Device.driver` from `/Library/Audio/Plug-Ins/HAL`.
- Pause apps that are playing audio, if you can.
- Restart `coreaudiod`:<br>
  <sup>(Open `/Applications/Utilities/Terminal.app` and paste the following at the prompt.)</sup>

  ```shell
  sudo launchctl kickstart -kp system/com.apple.audio.coreaudiod
  ```
  or, if that fails

  ```shell
  sudo killall coreaudiod
  ```
- Go to the Sound section in System Preferences and change your default output device at least once. (If you only have
  one device now, either use `Audio MIDI Setup.app` to create a temporary aggregate device, restart any audio apps that
  have stopped working or just restart your system.)

## Optional

- Delete `BGMXPCHelper.xpc` from `/usr/local/libexec` or possibly `/Library/Application Support/Background Music`.
- Unregister BGMXPCHelper.
  - If you're using OS X 10.11:

    ```shell
    sudo launchctl bootout system /Library/LaunchDaemons/com.bearisdriving.BGM.XPCHelper.plist
    ```
  - If you're using an earlier version of OS X:

    ```shell
    sudo launchctl unload /Library/LaunchDaemons/com.bearisdriving.BGM.XPCHelper.plist
    ```
- Delete BGMXPCHelper's launchd.plist.

  ```shell
  sudo rm /Library/LaunchDaemons/com.bearisdriving.BGM.XPCHelper.plist
  ```
- Delete BGMXPCHelper's user and group.

  ```shell
  sudo dscl . -delete /Users/_BGMXPCHelper
  sudo dscl . -delete /Groups/_BGMXPCHelper
  ```


