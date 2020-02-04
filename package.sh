#!/bin/bash
# vim: tw=100:

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
# package.sh
#
# Copyright © 2017-2020 Kyle Neideck
# Copyright © 2016, 2017 Takayama Fumihiko
#
# Builds Background Music and packages it into a .pkg file. Call this script with -d to use the
# debug build configuration.
#
# Based on https://github.com/tekezo/Karabiner-Elements/blob/master/make-package.sh
#

# TODO: Code signing. See `man productbuild`.

PATH="/bin:/sbin:/usr/bin:/usr/sbin:/usr/local/bin"; export PATH

# Sets all dirs in $1 to 755 (rwxr-xr-x) and all files in $1 to 644 (rw-r--r--).
set_permissions() {
    find "$1" -print0 | while read -d $'\0' filepath; do
        filename="${filepath##*/}"

        if [ -d "$filepath" ]; then
            chmod -h 755 "$filepath"
        else
            chmod -h 644 "$filepath"
        fi
    done
}

# --------------------------------------------------

# Use the release configuration and archive by default.
debug_build=NO
bgmapp_build_output_path="archives/BGMApp.xcarchive/Products/Applications"
bgmxpchelper_build_output_path="archives/BGMXPCHelper.xcarchive/Products/usr/local/libexec"
bgmdriver_build_output_path="archives/BGMDriver.xcarchive/Products/Library/Audio/Plug-Ins/HAL"

# Handle the options passed to this script.
while getopts ":d" opt; do
    case $opt in
        d)
            debug_build=YES
            bgmapp_build_output_path="BGMApp/build/Debug"
            bgmdriver_build_output_path="BGMDriver/build/Debug"
            ;;
        \?)
            echo "Invalid option: -$OPTARG" >&2
            exit 1
            ;;
    esac
done

# Build
if [[ $debug_build == YES ]]; then
    # Disable AddressSanitizer so we can distribute debug packages to users reporting bugs without
    # worrying about loading the AddressSanitizer dylib in coreaudiod.
    #
    # TODO: Would debug packages be more useful if they were built with optimization (i.e. using the
    #       DebugOpt configuration instead of Debug)?
    ENABLE_ASAN=NO bash build_and_install.sh -b -d
    build_status=$?
else
    bash build_and_install.sh -a
    build_status=$?
fi

# Exit if the build failed.
if [[ $build_status -ne 0 ]]; then
    exit $build_status
fi

# Read the version string from the build.
version="$(/usr/libexec/PlistBuddy \
    -c "Print CFBundleShortVersionString" \
    "${bgmapp_build_output_path}/Background Music.app/Contents/Info.plist")"

# Everything in out_dir at the end of this script will be released in the Travis CI builds.
out_dir="Background-Music-$version"
rm -rf "$out_dir"
mkdir "$out_dir"

# Put the archives in a zip file. This file is mainly useful because the debug symbols (dSYMs) are
# in it.
echo "Making archives zip"
zip -r "$out_dir/background-music-xcarchives-$version.zip" "archives"

# --------------------------------------------------

echo "Copying Files"

rm -rf "pkgroot"
mkdir -p "pkgroot"

mkdir -p "pkgroot/Library/Audio/Plug-Ins/HAL"
cp -R "${bgmdriver_build_output_path}/Background Music Device.driver" \
      "pkgroot/Library/Audio/Plug-Ins/HAL/"

mkdir -p "pkgroot/Applications"
cp -R "${bgmapp_build_output_path}/Background Music.app" "pkgroot/Applications"

scripts_dir="$(mktemp -d)"
cp "pkg/preinstall" "$scripts_dir"
cp "pkg/postinstall" "$scripts_dir"
cp "BGMApp/BGMXPCHelper/com.bearisdriving.BGM.XPCHelper.plist.template" "$scripts_dir"
cp "BGMApp/BGMXPCHelper/safe_install_dir.sh" "$scripts_dir"
cp "BGMApp/BGMXPCHelper/post_install.sh" "$scripts_dir"
cp -R "${bgmxpchelper_build_output_path}/BGMXPCHelper.xpc" "$scripts_dir"

set_permissions "pkgroot"
chmod 755 "pkgroot/Applications/Background Music.app/Contents/MacOS/Background Music"
chmod 755 "pkgroot/Applications/Background Music.app/Contents/Resources/uninstall.sh"
chmod 755 "pkgroot/Applications/Background Music.app/Contents/Resources/_uninstall-non-interactive.sh"
chmod 755 "pkgroot/Library/Audio/Plug-Ins/HAL/Background Music Device.driver/Contents/MacOS/Background Music Device"

set_permissions "$scripts_dir"
chmod 755 "$scripts_dir/preinstall"
chmod 755 "$scripts_dir/postinstall"
chmod 755 "$scripts_dir/BGMXPCHelper.xpc/Contents/MacOS/BGMXPCHelper"

rm -rf "pkgres"
mkdir -p "pkgres"
cp "Images/FermataIcon.pdf" "pkgres"

sed "s/{{VERSION}}/$version/g" "pkg/Distribution.xml.template" > "pkg/Distribution.xml"

# --------------------------------------------------

# As a security check for releases, we manually build the same package locally, compare it to the
# release built by Travis and then code sign it. (And then remove the code signature on a different
# computer and check that the signed package still matches the one from Travis.) So we include
# "unsigned" in the name to differentiate the two versions.
pkg="$out_dir/BackgroundMusic-$version.unsigned.pkg"
pkg_identifier="com.bearisdriving.BGM"

echo "Creating $pkg"

pkgbuild \
    --root "pkgroot" \
    --component-plist "pkg/pkgbuild.plist" \
    --identifier "$pkg_identifier" \
    --scripts "$scripts_dir" \
    --version "$version" \
    --install-location "/" \
    "$out_dir/Installer.pkg"

productbuild \
    --distribution "pkg/Distribution.xml" \
    --identifier "$pkg_identifier" \
    --resources "pkgres" \
    --package-path "$out_dir" \
    "$pkg"

rm -f "$out_dir/Installer.pkg"
rm -rf "pkgroot"
rm -rf "pkgres"
rm -f "pkg/Distribution.xml"

# Print checksums
echo "MD5 checksum:"
md5 "$pkg"
echo "SHA256 checksum:"
shasum -a 256 "$pkg"

