<!-- vim: set tw=120: -->

# Development Overview

The codebase is split into two projects: BGMDriver, a [userspace](https://en.wikipedia.org/wiki/User_space) [Core
Audio](https://developer.apple.com/library/mac/documentation/MusicAudio/Conceptual/CoreAudioOverview/Introduction/Introduction.html)
[HAL](https://developer.apple.com/library/mac/documentation/DeviceDrivers/Conceptual/WritingAudioDrivers/AudioOnMacOSX/AudioOnMacOSX.html#//apple_ref/doc/uid/TP30000730-TPXREF104)
[plugin](https://developer.apple.com/library/prerelease/content/samplecode/AudioDriverExamples/Listings/ReadMe_txt.html)
that publishes the virtual audio device<sup id="a1">[1](#f1)</sup>, and BGMApp, which handles the UI, passing audio from
the virtual device to the real output device and a few other things. The virtual device is usually referred to as
"BGMDevice" in the code.  Any code shared between the two projects is kept in the `SharedSource` dir.

## Summary

From the user's perspective, BGMDevice appears as one input device and one output device, both named "Background Music".
They're shown in `System Preferences > Sound` along with the real audio devices.

When you start BGMApp, it sets BGMDevice as your system's default output device so the system (i.e. Core Audio) will
start sending all<sup id="a2">[2](#f2)</sup> your audio data to BGMDriver. BGMDriver plays that audio on BGMDevice's
input stream, and the user can record it by selecting the Background Music device in QuickTime the same way they'd select
a microphone.

So that you can still hear the audio, BGMApp starts listening to BGMDevice's input stream and playing the audio out of
your real output device. (See [BGMPlayThrough](BGMApp/BGMApp/BGMPlayThrough.h)). 

The auto-pausing and per-app volume features rely on the fact that BGMDriver can also see the audio from each program
(technically each "client") before Core Audio mixes it all together. When you change an app's volume, BGMApp sends the
new volume to BGMDriver, which applies the app volumes by modifying the apps' audio data directly.

To know when to pause your music player, first BGMApp tells BGMDriver which app you've set as your music player. Then
when audio is playing BGMDriver tells BGMApp whether it's coming from your music player or another app. If it's from
another app, BGMApp tells your music player to pause if it's playing as well. When the other program stops playing
audio, BGMDriver tells BGMApp and BGMApp unpauses your music.

## Real-time Constraints

One slightly tricky part of this project is the code that runs with real-time constraints. When [the
HAL](https://developer.apple.com/library/prerelease/content/documentation/DeviceDrivers/Conceptual/WritingAudioDrivers/AudioOnMacOSX/AudioOnMacOSX.html#//apple_ref/doc/uid/TP30000730-TPXREF104)
calls certain functions of ours--the IO functions in BGMDriver and the IOProcs in BGMApp--they have to return within a
certain amount of time, every time. So they can't do things that aren't guaranteed to be fast, even if they almost
always are.

They can't dynamically allocate memory because OS X doesn't specify a maximum time for that to take. They also can't use
algorithms with a fast enough average-case run time if their worst-case is too slow.  In the `BGM_Device` class, they
can't lock the state mutex because a non-real-time function might be holding it and there's no guarantee it'll be
released in time.

If you're interested, have a look at [Real-time Audio Programming 101: Time Waits for
Nothing](http://www.rossbencina.com/code/real-time-audio-programming-101-time-waits-for-nothing).

## BGMDriver

The BGMDriver project is an audio driver for a virtual audio device named "Background Music", which we use to intercept
the audio playing on the user's system. The driver processes the audio data to apply per-app volumes, see if the music
player is playing, etc. and then writes the audio to BGMDevice's input stream. It's essentially a loopback device with a
few extra features.

There are quite a few other open-source projects with drivers that do the same
thing--[Soundflower](https://github.com/mattingalls/Soundflower) is probably the most well known--but as far as I know
all of those drivers were either written as kernel extensions or using AudioHardwarePlugIn, which is now deprecated
because of [issues with the OS X sandbox](https://lists.apple.com/archives/coreaudio-api/2013/Aug/msg00030.html). The
Apple sample code we started from gives us most of the same functionality and uses the latest Core Audio APIs. The other
projects are still definitely worth reading, though. There's a list in [the README](README.md) with a few of them.

BGMDriver is an AudioServerPlugin (see
[CoreAudio/AudioServerPlugIn.h](https://github.com/phracker/MacOSX-SDKs/blob/master/MacOSX10.11.sdk/System/Library/Frameworks/CoreAudio.framework/Versions/A/Headers/AudioServerPlugIn.h))
based on [Apple's SimpleAudio
example](https://developer.apple.com/library/mac/samplecode/AudioDriverExamples/Introduction/Intro.html).  An
AudioServerPlugIn is a Core Audio driver that runs in userspace and is "hosted" by the HAL, which is nice because the
HAL handles a lot of things for us. Only running in userspace means our bugs shouldn't be able to cause a kernel panic
(though we can definitely crash the audio system) and users don't have to restart after installation. It also makes
debugging much less painful and is less insecure. In addition to running in userspace, the plugin also runs in a fairly
restrictive sandbox.

`BGM_PlugInInterface.cpp` is where you'll find the entry point functions that the HAL calls, so it's a good place to
start. Those functions are our implementation of the interface defined by `AudioServerPlugIn.h`. They're mostly
boilerplate and error handling code, and largely unchanged from the sample code. They call `BGM_PlugIn` or `BGM_Device`
(and probably other subclasses of `BGM_Object` in future) to do the actual work.

`BGM_Device` is by far the largest class, and should really be split up, but most of its code is very straightforward.
The simple parts mostly handle audio object properties, which are often static. An audio object is a plugin, device,
stream, control (e.g. volume), etc. and is part of the HAL's object model. Each object is identified by an
`AudioObjectID`. When the HAL asks us for the value of a property (or the size of the value, whether the value can be
set, etc.) `BGM_Device` handles properties relating to our virtual device, its controls and its streams.

The rest of `BGM_Device` mostly handles IO. During each IO cycle, the HAL calls us for each phase of the cycle we
support--reading input, writing output, etc. When the user has our device set as default, we receive the system's audio
during the read-input phase and store it in our ring buffer. Then in the write-output phase (technically, the
`ProcessOutput` and `WriteMix` phases) we process the data and write from our ring buffer to our output stream. By
"process" I just mean that we apply the per-app volume, keep track of whether the audio is audible or not, and other
things like that.

BGMDriver's IO functions have to be real-time safe, which means any functions they call do as well. Some other functions
need to be real-time safe as well because they access data shared with the IO functions and have to do so on a real-time
thread to avoid [priority inversion](https://en.wikipedia.org/wiki/Priority_inversion). Those functions are usually
called using [BGM_TaskQueue](BGMDriver/BGMDriver/BGM_TaskQueue.h), which can dispatch calls to a real-time worker
thread. BGM_TaskQueue can also be used from a real-time thread to asynchronously dispatch calls to functions that aren't
real-time safe.

### Building and Debugging

To test your changes, build `Background Music Device.driver`, either inside Xcode (set the active scheme to "BGMDevice",
go `Product > Build For > Running` and look in the `products` folder) or with something like
```shell
xcodebuild -project BGMDriver/BGMDriver.xcodeproj -target "PublicUtility" -configuration Debug
xcodebuild -project BGMDriver/BGMDriver.xcodeproj -configuration Debug
```
And then run `BGMDriver/BGMDriver/quick_install.sh` to install. Or if you'd rather install manually, copy `Background
Music Device.driver` to `/Library/Audio/Plug-Ins/HAL` and restart coreaudiod.

You might have to delete `BGMDriver/build` first if you're using `xcodebuild` and run into permissions problems.

Before you build, Xcode might show incorrect warnings on the `#pragma clang assume_nonnull` lines for some reason. They
go away after you build and don't seem to cause any problems.

**The following debug instructions stopped working in OS X 10.11 (El Capitan).** [System Integrity
Protection](https://support.apple.com/en-us/HT204899) stops LLDB from attaching to coreaudiod. I don't know of a
workaround except to disable SIP.

To debug in Xcode,
 - edit the BGMDevice scheme and
    - set the build configuration to Debug,
    - set "Executable" to coreaudiod in `/usr/sbin` (you can use `shift+cmd+G` to get there),
    - check "Debug Executable",
    - set the "debug as" user to root,
    - and select "wait for executable to be launched",
 - set BGMDevice as the active scheme,
 - build and install,
 - stop coreaudiod

   ```shell
   sudo launchctl unload /System/Library/LaunchDaemons/com.apple.audio.coreaudiod.plist
   ```
 - run in Xcode,
 - start coreaudiod

   ```shell
   sudo launchctl load /System/Library/LaunchDaemons/com.apple.audio.coreaudiod.plist
   ```

Xcode should attach to the coreaudiod process when it starts running.

Debug logging is to syslog by default. Console.app is probably the most convenient way to read it.

### HALLab

Apple's HALLab tool can be useful for inspecting the driver's properties, notifications, etc. It's in the Audio Tools
for Xcode package, which you can find in the [Apple developer downloads](https://developer.apple.com/downloads/).

## BGMApp

BGMApp is a fairly standard Cocoa status-bar app, for the most part. The UI is simple and mostly built in Interface
Builder.

`awakeFromNib` in [BGMAppDelegate.mm](/BGMApp/BGMApp/BGMAppDelegate.mm) is (more or less) the entry point/main
function. `applicationDidFinishLaunching` gets called next and finishes setting things up.

At launch, BGMApp sets BGMDevice as the system's default device and starts playing the audio from BGMDevice through the
actual output device. Usually that's the device that BGMDevice replaced when we set it as the default device. When
BGMApp closes, it sets that device back as the default device.

BGMApp stores a small amount of state data using [User
Defaults](https://developer.apple.com/library/content/documentation/Cocoa/Conceptual/UserDefaults/AboutPreferenceDomains/AboutPreferenceDomains.html)
-- currently just whether auto-pause is enabled and which music player to pause. Other persistent state (e.g. app
volumes) is managed by BGMDriver.

BGMApp mostly communicates with BGMDriver through HAL notifications, though in some special cases we use XPC instead.
The only other communication between them is BGMDriver sending the system's audio data through BGMDevice's output
stream, which BGMApp receives.

For example, when an app other than the music player starts playing audio, BGMDriver sends out a notification saying
that BGMDevice's `kAudioDeviceCustomPropertyDeviceAudibleState` property has changed.  BGMApp receives the notification
from the HAL and decides whether it should pause the music player.

Our custom notifications are defined/documented in `BGM_Types.h`, which is shared between the two projects.

BGMApp also keeps the output device in sync with BGMDevice. For example, since BGMDevice is set as the default device,
when the user changes their system volume only BGMDevice's volume will change. So BGMApp listens for changes to
BGMDevice's volume and sets the output device's volume to match. 

The only code in BGMApp that has to be real-time safe is in `BGMPlayThrough`'s IOProcs, `InputDeviceIOProc` and
`OutputDeviceIOProc`, which don't do very much. The most complicated part of BGMApp is probably pausing/reducing IO when
no other processes are playing audio, which is also handled in `BGMPlayThrough`.

### BGMXPCHelper

From [main.m](BGMApp/BGMXPCHelper/main.m):

>BGMXPCHelper passes XPC messages between BGMDriver and BGMApp. So far it's only used for synchronization while starting
>IO.

>BGMApp and BGMDriver usually communicate by changing device properties and listening for notifications about those
>changes, which the HAL sends. We use XPC, or plan to use it, for the few cases that notifications don't suit.

### Building

Build and install/run BGMXPCHelper and `Background Music.app` either inside Xcode or with something like
```shell
sudo xcodebuild -project BGMApp/BGMApp.xcodeproj \
                -target BGMXPCHelper \
                DSTROOT="/" \
                INSTALL_PATH="$(BGMApp/BGMXPCHelper/safe_install_dir.sh)" \
                -configuration Debug \
                install
sudo chown -R $(whoami):staff BGMApp/build  # Fix build dir ownership
xcodebuild -project BGMApp/BGMApp.xcodeproj \
           -configuration Debug
open "BGMApp/build/Debug/Background Music.app"
```

You might have to delete `BGMApp/build` first if you're using `xcodebuild` and run into permissions problems.

To test with Address Sanitizer, you might have to set the environment var `ASAN_OPTIONS=detect_odr_violation=0` to work
around [Issue #647](https://github.com/google/sanitizers/issues/647). (In Xcode, go `Product` > `Scheme` > `Edit
Scheme...`, select the Background Music scheme, and add the environment var in Run > Arguments.)

----

<b id="f1">[1]</b> It actually publishes two devices -- the main one and one for UI-related sounds, but you probably
only need to know about the main one. [↩](#a1)

<b id="f2">[2]</b> All, unless you're playing audio through a program that's set to always use a specific device or,
for some reason, doesn't switch to the new default device right away. [↩](#a2)


