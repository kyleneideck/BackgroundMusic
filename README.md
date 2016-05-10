<!-- vim: set tw=120: -->

![](Images/README/FermataIcon.png)

# Background Music
##### OS X audio utility

![](Images/README/Screenshot.png)

- Automatically pauses your music player when other audio starts playing and unpauses it afterwards
- Per-application volume, boost quiet apps
- Record system audio <br>
<br>
- No restart required to install
- Runs entirely in userspace

**Version 0.1.0**, first release. Probably very buggy. Not very polished. Pretty much only tested on one system. (A
MacBook running OS X 10.11 using the built-in audio device.)

**Requires OS X 10.10+**. Might work on 10.9, but I haven't tried it. Currently unable to build in Xcode 6.

## Auto-pause music

Background Music can pause your music player app when other audio starts playing and unpause it afterwards. The idea is
that when I'm listening to music and pause it to watch a video or something I always forget to unpause it afterwards. So
this keeps me from wearing headphones for hours listening to nothing.

So far iTunes, [Spotify](https://www.spotify.com), [VLC](https://www.videolan.org/vlc/) and
[VOX](http://coppertino.com/vox/mac) are supported, but adding support for a music player should only take a few minutes
(see `BGMMusicPlayer.h`). If you don't know how to program, or just don't feel like it, create an issue and I'll try to
add it for you.

## App volumes

Background Music has a volume slider for each app running on the system. I mostly use this to boost quiet apps above
their normal maximum volume.

## Recording system audio

With Background Music running, open QuickTime Player and go `File > New Audio Recording...` (or movie/screen). Then
click the arrow next to the record button that looks like `⌄` and select `Background Music Device` as the input device.

You should be able to record system audio and a microphone together by creating an [aggregate
device](https://support.apple.com/en-us/HT202000) that combines your input device (usually Built-in Input) with
Background Music Device. You can create the aggregate device using the Audio MIDI Setup utility from
`/Applications/Utilities`.

## Install

No binaries yet (working on it) but building should take less than a minute.

<table>
<tr><td>⚠️</td>
<td>Unfortunately, <strong>it won't build if you don't have Xcode installed</strong> because xcodebuild doesn't work on its own anymore. If you don't mind a 4GB download, you can <a href="https://developer.apple.com/xcode/download/">get Xcode here</a>.</td>
</tr></table>

If you're comfortable with it, you can just paste the following at a Terminal prompt.

[//]: # (Uses /bin/bash instead of just bash on the off chance that someone has a non standard Bash in their $PATH, but)
[//]: # (it doesn't do that for Tar or cURL because I'm fairly sure any versions of them should work here. That said,)
[//]: # (build_and_install.sh doesn't call things by absolute paths (yet?) anyway.)
[//]: # ( )
[//]: # (Uses "gzcat - | tar x" instead of "tar xz" because gzcat will also check the file's integrity. (Gzip files)
[//]: # (include a checksum.))
```shell
(set -eo pipefail; URL='https://github.com/kyleneideck/BackgroundMusic/archive/master.tar.gz'; \
    cd $(mktemp -d); echo Downloading $URL to $(pwd); curl -qfL# $URL | gzcat - | tar x && \
    /bin/bash BackgroundMusic-master/build_and_install.sh && rm -rf BackgroundMusic-master)
```

Otherwise, to build and install everything, do the following:

- Clone or [download](https://github.com/kyleneideck/BackgroundMusic/archive/master.zip) the project
- If the project is in a zip, unzip it
- [Go to the folder of the project](https://github.com/0nn0/terminal-mac-cheatsheet/wiki/Terminal-Cheatsheet-for-Mac-(-basics-)#core-commands) on your terminal
- Run the following the command: `/bin/bash build_and_install.sh`.

The script restarts the system audio process (coreaudiod) at the end of the installation, so you might want to pause any apps playing audio.

Additional detailed installation instructions can be found on [the Wiki](https://github.com/kyleneideck/BackgroundMusic/wiki/Installation).

## Uninstall

- Run the `uninstall.sh` script to remove Background Music from your system.
- Go to the Sound section in System Preferences and change your default output device at least once. (If you only have
  one device now, either use `Audio MIDI Setup.app` to create a temporary aggregate device, restart any audio apps that
  have stopped working or just restart your system.)

### Manual Uninstall

Try following the instructions in [`MANUAL-UNINSTALL.md`](MANUAL-UNINSTALL.md) if `uninstall.sh` fails. (You might
consider submitting a bug report, too.)

## Troubleshooting

If Background Music crashes and system audio stops working, open the Sound panel in System Preferences and change your
system's default output device to something other than Background Music Device. If it already is, it might help to
change the default device and then change it back again. Failing that, you might have to uninstall.

## Known issues

- VLC automatically pauses iTunes/Spotify when it starts playing something, but that stops Background Music from
  unpausing your music afterwards. To workaround it, open VLC's preferences, click `Show All`, go `Interface` > `Main
  interfaces` > `macosx` and change `Control external music players` to either `Do nothing` or `Pause and resume
  iTunes/Spotify`.

  Similarly, Skype pauses iTunes during calls. If you want to disable that, uncheck `Pause iTunes during calls` on the
  General tab of Skype's preferences.
- Plugging in or unplugging headphones when Background Music isn't running can silence system audio. To fix it, go to
  the Sound section in System Preferences, click the Output tab and change your default output device to something other
  than Background Music Device. Alternatively, you may Option+Click on the Sound icon in the menu bar to select a different output device.

  This happens when OS X remembers that Background Music Device was your default audio device the last time you last
  used (or didn't use) headphones.
- [A recent Chrome bug](https://bugs.chromium.org/p/chromium/issues/detail?id=557620) can stop Chrome from switching to
  Background Music Device after you open Background Music. Chrome's audio will still play, but Background Music won't be
  aware of it.
- Some apps play notification sounds that are only just long enough to trigger an auto-pause. The only workaround right
  now is to increase the `kPauseDelayMSecs` constant in `BGMApp/BGMAutoPauseMusic.mm`. That will make your music overlap
  the other audio for longer, though, so you don't want to increase it too much. See
  [#5](https://github.com/kyleneideck/BackgroundMusic/issues/5) for details.
- Plenty more. Some are in listed in TODO.md.

## Related projects

- [Core Audio User-Space Driver
  Examples](https://developer.apple.com/library/mac/samplecode/AudioDriverExamples/Introduction/Intro.html)
  The sample code from Apple that BGMDriver is based on.
- [Soundflower](https://github.com/mattingalls/Soundflower) - "MacOS system extension that allows applications to pass
  audio to other applications."
- [WavTap](https://github.com/pje/WavTap) - "globally capture whatever your mac is playing—-as simply as a screenshot"
- [llaudio](https://github.com/mountainstorm/llaudio) - "An old piece of work to reverse engineer the Mac OSX
  user/kernel audio interface. Shows how to read audio straight out of the kernel as you would on Darwin (where most the
  OSX goodness is missing)"
- [mute.fm](http://www.mute.fm), [GitHub](https://github.com/jaredsohn/mutefm) (Windows) - Auto-pause music
- [Jack OS X](http://www.jackosx.com) - "A Jack audio connection kit implementation for Mac OS X"
- [PulseAudio OS X](https://github.com/zonque/PulseAudioOSX) - "PulseAudio for Mac OS X"
- [Sound Pusher](https://github.com/q-p/SoundPusher) - "Virtual audio device, real-time encoder and SPDIF forwarder for
  Mac OS X"
- [eqMac](http://www.bitgapp.com/eqmac/) - "System-wide Audio Equalizer for Mac OSX"
- [Zirkonium](https://code.google.com/archive/p/zirkonium) - "An infrastructure and application for multi-channel sound
  spatialization on MacOS X."

### Non-free

- [Audio Hijack](https://rogueamoeba.com/audiohijack/) - "Capture Audio From Anywhere on Your Mac"
- [Volume Mixer For Mac](http://www.volumemixer-app.com/) - "Application specific volume control for Mac OS Yosemite and
  El Capitan"
- [Sound Siphon](http://staticz.com) - System/app audio recording, per-app volumes, system audio equaliser
- [SoundBunny](https://www.prosofteng.com/soundbunny-mac-volume-control/) - "Control application volume independently."
- [Boom 2](http://www.globaldelight.com/boom/index.php) - "The Best Volume Booster & Equalizer For Mac"

## License

GPLv2 or later
