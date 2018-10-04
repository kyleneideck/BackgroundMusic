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
# Copyright © 2017, 2018 Kyle Neideck
# Copyright © 2016, 2017 Takayama Fumihiko
#
# Build Background Music and package it into a .pkg file and a .zip of the debug symbols (dSYM).
# Call this script with -d to use the debug build configuration.
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

# Use the release configuration by default.
debug_build=NO
build_output_path="build/Release"

# Handle the options passed to this script.
while getopts ":d" opt; do
    case $opt in
        d)
            debug_build=YES
            build_output_path="build/Debug"
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
    bash build_and_install.sh -b
    build_status=$?
fi

# Exit if the build failed.
if [[ $build_status -ne 0 ]]; then
    exit $build_status
fi

# Read the version string from the build.
version="$(/usr/libexec/PlistBuddy \
    -c "Print CFBundleShortVersionString" \
    "BGMApp/${build_output_path}/Background Music.app/Contents/Info.plist")"

# Everything in out_dir at the end of this script will be released in the Travis CI builds.
out_dir="Background-Music-$version"
rm -rf "$out_dir"
mkdir "$out_dir"

if [[ $debug_build == NO ]]; then
    # Separate the debug symbols and the .app bundle.
    echo "Archiving debug symbols"

    dsym_archive="$out_dir/Background Music.dSYM-$version.zip"
    mv "BGMApp/${build_output_path}/Background Music.app/Contents/MacOS/Background Music.dSYM" \
       "Background Music.dSYM"
    zip -r "$dsym_archive" "Background Music.dSYM"
    rm -r "Background Music.dSYM"
fi

# --------------------------------------------------

echo "Copying Files"

rm -rf "pkgroot"
mkdir -p "pkgroot"

mkdir -p "pkgroot/Library/Audio/Plug-Ins/HAL"
cp -R "BGMDriver/${build_output_path}/Background Music Device.driver" \
      "pkgroot/Library/Audio/Plug-Ins/HAL/"

mkdir -p "pkgroot/Applications"
cp -R "BGMApp/${build_output_path}/Background Music.app" "pkgroot/Applications"

scripts_dir="$(mktemp -d)"
cp "pkg/preinstall" "$scripts_dir"
cp "pkg/postinstall" "$scripts_dir"
cp "BGMApp/BGMXPCHelper/com.bearisdriving.BGM.XPCHelper.plist.template" "$scripts_dir"
cp "BGMApp/BGMXPCHelper/safe_install_dir.sh" "$scripts_dir"
cp "BGMApp/BGMXPCHelper/post_install.sh" "$scripts_dir"
cp -R "BGMApp/${build_output_path}/BGMXPCHelper.xpc" "$scripts_dir"

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

pkg="$out_dir/BackgroundMusic-$version.pkg"
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
if [[ $debug_build == YES ]]; then
    echo "MD5 checksum:"
    md5 "$pkg"
    echo "SHA256 checksum:"
    shasum -a 256 "$pkg"
else
    echo "MD5 checksums:"
    md5 {"$pkg","$dsym_archive"}
    echo "SHA256 checksums:"
    shasum -a 256 {"$pkg","$dsym_archive"}
fi


