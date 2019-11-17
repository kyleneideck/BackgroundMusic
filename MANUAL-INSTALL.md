<!-- vim: set tw=120: -->

# Manual Build and Install

- Install the virtual audio device `Background Music Device.driver` to `/Library/Audio/Plug-Ins/HAL`.

  ```shell
  sudo xcodebuild -project BGMDriver/BGMDriver.xcodeproj \
                  -target "PublicUtility" \
                  RUN_CLANG_STATIC_ANALYZER=0 \
                  clean build
  sudo xcodebuild -project BGMDriver/BGMDriver.xcodeproj \
                  -target "Background Music Device" \
                  RUN_CLANG_STATIC_ANALYZER=0 \
                  DSTROOT="/" \
                  clean install
  ```
- Install the XPC helper.

  ```shell
  sudo xcodebuild -project BGMApp/BGMApp.xcodeproj \
                  -target BGMXPCHelper \
                  RUN_CLANG_STATIC_ANALYZER=0 \
                  DSTROOT="/" \
                  INSTALL_PATH="$(BGMApp/BGMXPCHelper/safe_install_dir.sh)" \
                  clean install
  ```
- Install `Background Music.app` to `/Applications` (or wherever).

  ```shell
  sudo xcodebuild -project BGMApp/BGMApp.xcodeproj \
                  -target "Background Music" \
                  RUN_CLANG_STATIC_ANALYZER=0 \
                  DSTROOT="/" \
                  clean install
  ```
- Restart `coreaudiod`: <br>
  (Audio will stop working until the next step, so you might want to pause any running audio apps.)

  ```shell
  sudo launchctl kickstart -kp system/com.apple.audio.coreaudiod
  ```
  or, if that fails

  ```shell
  sudo killall coreaudiod
  ```
- Run `Background Music.app`.


