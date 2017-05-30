<!-- vim: set tw=120: -->

# Contributing

Firstly, thanks for reading this. Pull requests, bug reports, feature requests, questions, etc. are all very welcome --
including ones from non-developers.

## Issues

You'll probably want to update to the latest version of the code before creating an issue. The easiest way is to just
run the installation command from [README.md](/README.md#install) again. (But `git pull && ./build_and_install.sh` is
faster.)

For bug reports about `build_and_install.sh`, please include your `build_and_install.log`. It should be saved in the
directory `build_and_install.sh` is in.

It might also be helpful to include logs in bug reports about Background Music itself. Those logs go to syslog by
default, so you can use Console.app to read them. (It might help to search for "BGM" or "Background Music".)

You also might not get any log messages at all. Normally (i.e. in release builds) Background Music only logs errors and
warnings. We're still working on adding optional debug-level logging to release builds.

If you feel like being really helpful, you could reproduce your bug with a debug build and include the debug logs, which
are much more detailed. But don't feel obligated to. To install a debug build, use `./build_and_install.sh -d`.

If you make an issue and you're interested in implementing/fixing it yourself, mention that in the issue so we can
confirm you're on the right track, assign the issue to you and so on.

## Code

The code is mostly C++ and Objective-C. But don't worry if you don't know those languages--I don't either. Or Core
Audio, for that matter. Also don't worry if you're not sure your code is right.

No dependencies so far. (Though you're welcome to add some.)

The best place to start is probably [DEVELOPING.md](/DEVELOPING.md), which has an overview of the project and
instructions for building, debugging, etc. It's kind of long, though, and not very interesting, so you might prefer to
go straight into the code. In that case, you'll probably want to start with
[BGMAppDelegate.mm](/BGMApp/BGMApp/BGMAppDelegate.mm).

If you get stuck or have questions about the project, feel free to open an issue. You could also [email
me](mailto:kyle@bearisdriving.com) or try [#backgroundmusic on
Freenode](https://webchat.freenode.net/?channels=backgroundmusic).

If you have questions related to Core Audio, the [Core Audio mailing
list](https://lists.apple.com/archives/coreaudio-api) is very useful. There's also the [Core Audio
Overview](https://developer.apple.com/library/mac/documentation/MusicAudio/Conceptual/CoreAudioOverview/Introduction/Introduction.html)
and the [Core Audio
Glossary](https://developer.apple.com/library/mac/documentation/MusicAudio/Reference/CoreAudioGlossary/Glossary/core_audio_glossary.html).

If you remember to, add a copyright notice with your name to any source files you change substantially. Let us know in
the PR if you've intentionally not added one so we know not to add one for you.


