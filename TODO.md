<!-- vim: set tw=120: -->

# TODOs for BGMApp and BGMDriver

There are also lots of other TODOs commented around the code.

## Fairly quick

- Add nullability qualifiers to files without them. Use `assume_nonnull` pragmas everywhere.

- Keyboard shortcuts for the system volume boost and the frontmost app's volume

- Should we hide music players in the preferences menu if Launch Services says they aren't installed? (And they aren't
  running or otherwise obviously present on the system.) If so, should we have a "show all" menu item?

- System-wide volume boost

- Listen for notifications when the output device is removed/disabled and revert to the previous output device

- User-friendly installation process and binaries. A .dmg with `Background Music.app` and `Background Music
  Device.driver` should be fine. Include links to `/Applications` and `/Library/Audio/Plug-Ins/HAL` with instructions to
  drag-and-drop. BGMApp will need to restart coreaudiod on the first run so it can use BGMDevice. (See `launchctl`
  commands in README.) I'm not sure how much work it is to make builds reproducible with Xcode, but it would be worth
  looking into.

## Less quick

- Recording system/application audio. You can already record system audio by selecting BGMDevice as the input device in
  QuickTime Player but that isn't obvious.

- Persist some of BGMDriver's state (app volumes, music player, etc.) using `WriteToStorage` and `CopyFromStorage` from
  AudioServerPlugIn.h. Right now it's lost when you restart.  CACFPreferences.h looks like it might be appropriate for
  this.

- So we don't increase clipping, we should only increase an app's relative volume in the driver if the output device is
  already at full volume. My first thought is to set the volume of the output device to the highest app volume and
  reduce the other app's volumes in the driver so they sound the same. Matching the output device's volume curve might
  be a little tricky.

- More tests. Integration or performance tests would be nice.

- Support for more music players

- Support for custom music players. Probably easiest to ask the user for AppleScript snippets. It could also have an
  option to simulate pressing the play/pause key on the keyboard, which would be less hassle for users.

- Allow the user to select multiple music players.

- Start at login option/setting

- App volumes are only stored with the app's bundle ID and pid, so volumes for apps without bundle IDs are forgotten
  when the apps are closed.

- `-Wprofile-instr-out-of-date` is disabled in BGMApp because I think we're running into 
  <https://llvm.org/bugs/show\_bug.cgi?id=24996>

- Support for devices with more than two output channels

- Split `BGM_Device.cpp` into smaller classes

- MPlayer OSX Extended's HAL client isn't the same process as the one we get from `NSRunningApplications` and it gives
  the HAL a different bundle ID. I think it would work (in most cases like that) to also send any child processes'
  pids/bundle IDs with the app volumes.

- Advanced preferences menu options:
    - Uninstall
    - Restart driver/coreaudiod
    - Size of the IO buffers on the output device, to trade off latency for CPU. BGMDriver will have to be set to match
      the output device. I don't think its IO buffer size is settable yet.

- Should we hide the BGM device when BGMApp isn't running? This would fix the problem of our device being left as the
  default device if BGMApp doesn't shutdown properly (because of a crash, hard reset, etc.), which stops the system from
  playing audio. The problem with that is the Background Music device can still be used without BGMApp, to record
  system/apps' audio, so ideally the BGM device would be able to just unset itself as the default device when BGMApp
  isn't running.  For now, I think we should just have `kAudioDevicePropertyDeviceCanBeDefaultDevice` become false.

- Fault-tolerance:
    - BGMApp should catch any uncaught `CAException` from `BGMPlayThrough` and try to restart
      playthrough. (In release builds only.)
    - ...

- Only allow one instance of BGMApp to be running. Show a warning if the user tries to start another one. It would be
  nice if the warning offered to kill the other instances and start a new one.

- When BGMApp changes the default device to BGMDevice, some apps seem to keep using the previous default device if they
  were running IO at the time. Not sure if we can do anything about this.

- Figure out how to test BGMDriver with Address Sanitizer enabled. It isn't working because it makes coreaudiod try to
  read files outside of its sandbox and the system kills it.

- Proper crash reporting.
    - CrashReporter doesn't show a GUI if BGMApp crashes because it has `LSUIElement` set in its `Info.plist`.
    - We can't always symbolicate users' crash logs (from CrashReporter) because we don't have their debug symbols.

- Test Background Music on a system running OS X on a case-sensitive file system.
    - In [#64](https://github.com/kyleneideck/BackgroundMusic/issues/64), BGMDriver failed to compile and the error
      suggests it was a case-sensitivity problem. That should be fixed now, but I haven't looked for any runtime bugs.
    - As a partial solution, Travis CI now builds Background Music in a case-sensitive disk image.

- BGMApp and BGMXPCHelper should be sandboxed. (BGMDriver already is because it runs in the `coreaudiod` system
  process.)


