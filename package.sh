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
# Call it with -r expanded_package_dir to repackage a package expanded using
#     pkgutil --expand-full original_package.pkg expanded_package_dir
# This is useful after code signing the bundles in the expanded package.
#
# Based on https://github.com/tekezo/Karabiner-Elements/blob/master/make-package.sh
#

# TODO: Code signing. See `man productbuild`.

PATH="/bin:/sbin:/usr/bin:/usr/sbin:/usr/local/bin"; export PATH

# Sets all dirs in $1 to 755 (rwxr-xr-x) and all files in $1 to 644 (rw-r--r--).
set_permissions() {
    find "$1" -print0 | while read -d $'\0' filepath; do
        if [ -d "$filepath" ]; then
            chmod -h 755 "$filepath"
        else
            chmod -h 644 "$filepath"
        fi
    done
}

# --------------------------------------------------

# Use the release configuration and archive by default.
packaging_operation="make_release_package"
bgmapp_build_output_path="archives/BGMApp.xcarchive/Products/Applications"
bgmxpchelper_build_output_path="archives/BGMXPCHelper.xcarchive/Products/usr/local/libexec"
bgmdriver_build_output_path="archives/BGMDriver.xcarchive/Products/Library/Audio/Plug-Ins/HAL"
repackage_dir=""

# Handle the options passed to this script.
while getopts ":dr:h" opt; do
    case $opt in
        d)
            packaging_operation="make_debug_package"
            bgmapp_build_output_path="BGMApp/build/Debug"
            bgmdriver_build_output_path="BGMDriver/build/Debug"
            ;;
        r)
            packaging_operation="repackage"
            repackage_dir="$(realpath "$OPTARG")"
            ;;
        h)
            # Just print out the header from this file.
            awk '/^# package.sh/,/^$/' "$0"
            exit 0
            ;;
        \?)
            echo "Invalid option: -$OPTARG" >&2
            exit 1
            ;;
    esac
done

bgmapp_path="${bgmapp_build_output_path}/Background Music.app"
bgmdriver_path="${bgmdriver_build_output_path}/Background Music Device.driver"
bgmxpchelper_path="${bgmxpchelper_build_output_path}/BGMXPCHelper.xpc"

# Build
if [[ $packaging_operation == "repackage" ]]; then
    # Paths to the bundles in the expanded package that we're repackaging.
    bgmapp_path="${repackage_dir}/Installer.pkg/Payload/Applications/Background Music.app"
    bgmdriver_path="${repackage_dir}/Installer.pkg/Payload/Library/Audio/Plug-Ins/HAL/Background Music Device.driver"
    bgmxpchelper_path="${repackage_dir}/Installer.pkg/Scripts/BGMXPCHelper.xpc"

    # No need to build anything if we're repackaging.
    build_status=0
elif [[ $packaging_operation == "make_debug_package" ]]; then
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
    "${bgmapp_path}/Contents/Info.plist")"

# Everything in out_dir at the end of this script will be released in the Travis CI builds.
out_dir="Background-Music-$version"
rm -rf "$out_dir"
mkdir "$out_dir"

if [[ $packaging_operation == "make_release_package" ]]; then
    # Put the archives in a zip file. This file is mainly useful because the debug symbols (dSYMs)
    # are in it.
    echo "Making archives zip"
    zip -r --quiet "$out_dir/background-music-xcarchives-$version.zip" "archives"
fi

# --------------------------------------------------

echo "Copying Files"

rm -rf "pkgroot"
mkdir -p "pkgroot"

mkdir -p "pkgroot/Library/Audio/Plug-Ins/HAL"
mkdir -p "pkgroot/Applications"
scripts_dir="$(mktemp -d)"

if [[ $packaging_operation == "repackage" ]]; then
    # When repackaging, only set the permissions of the dirs this script creates. The copied
    # dirs/files should already have the right permissions and we don't want to break any code
    # signatures.
    set_permissions "pkgroot"
    set_permissions "$scripts_dir"

    repackage_scripts_dir="${repackage_dir}/Installer.pkg/Scripts"
    cp "${repackage_scripts_dir}/preinstall" "$scripts_dir"
    cp "${repackage_scripts_dir}/postinstall" "$scripts_dir"
    cp "${repackage_scripts_dir}/com.bearisdriving.BGM.XPCHelper.plist.template" "$scripts_dir"
    cp "${repackage_scripts_dir}/safe_install_dir.sh" "$scripts_dir"
    cp "${repackage_scripts_dir}/post_install.sh" "$scripts_dir"
else
    cp "pkg/preinstall" "$scripts_dir"
    cp "pkg/postinstall" "$scripts_dir"
    cp "BGMApp/BGMXPCHelper/com.bearisdriving.BGM.XPCHelper.plist.template" "$scripts_dir"
    cp "BGMApp/BGMXPCHelper/safe_install_dir.sh" "$scripts_dir"
    cp "BGMApp/BGMXPCHelper/post_install.sh" "$scripts_dir"
fi

# Copy the bundles.
cp -R "$bgmdriver_path" "pkgroot/Library/Audio/Plug-Ins/HAL/"
cp -R "$bgmapp_path" "pkgroot/Applications"
cp -R "$bgmxpchelper_path" "$scripts_dir"

# Set the file/dir permissions.
if [[ $packaging_operation != "repackage" ]]; then
    set_permissions "pkgroot"
    chmod 755 "pkgroot/Applications/Background Music.app/Contents/MacOS/Background Music"
    chmod 755 "pkgroot/Applications/Background Music.app/Contents/Resources/uninstall.sh"
    chmod 755 "pkgroot/Applications/Background Music.app/Contents/Resources/_uninstall-non-interactive.sh"
    chmod 755 "pkgroot/Library/Audio/Plug-Ins/HAL/Background Music Device.driver/Contents/MacOS/Background Music Device"

    set_permissions "$scripts_dir"
    chmod 755 "$scripts_dir/preinstall"
    chmod 755 "$scripts_dir/postinstall"
    chmod 755 "$scripts_dir/BGMXPCHelper.xpc/Contents/MacOS/BGMXPCHelper"
fi

# Copy the package resources.
rm -rf "pkgres"
mkdir -p "pkgres"

if [[ $packaging_operation == "repackage" ]]; then
    cp "${repackage_dir}/Resources/FermataIcon.pdf" "pkgres"
    cp "${repackage_dir}/Distribution" "pkg/Distribution.xml"
else
    cp "Images/FermataIcon.pdf" "pkgres"
    # Populate the Distribution.xml template and copy it. It only has one template variable so far.
    sed "s/{{VERSION}}/$version/g" "pkg/Distribution.xml.template" > "pkg/Distribution.xml"
fi

# --------------------------------------------------

if [[ $packaging_operation == "repackage" ]]; then
    # Include "repackaged" in the filename just to make it clear.
    pkg="$out_dir/BackgroundMusic-$version.repackaged.pkg"
else
    # As a security check for releases, we manually build the same package locally, compare it to
    # the release built by Travis and then code sign it. (And then remove the code signature on a
    # different computer and check that it still matches the one from Travis.) So we include
    # "unsigned" in the name to differentiate the two versions.
    pkg="$out_dir/BackgroundMusic-$version.unsigned.pkg"
fi

pkg_identifier="com.bearisdriving.BGM"

echo "Creating .pkg installer: $pkg"

# Build the "component package". (See `man pkgbuild`.)
pkgbuild \
    --root "pkgroot" \
    --component-plist "pkg/pkgbuild.plist" \
    --identifier "$pkg_identifier" \
    --scripts "$scripts_dir" \
    --version "$version" \
    --install-location "/" \
    "$out_dir/Installer.pkg"

# Build the .pkg installer. This is basically a wrapper around the Installer.pkg we just built that
# adds the background image and the settings in Distribution.xml.
productbuild \
    --distribution "pkg/Distribution.xml" \
    --identifier "$pkg_identifier" \
    --resources "pkgres" \
    --package-path "$out_dir" \
    "$pkg"

# Clean up.
rm -f "$out_dir/Installer.pkg"
rm -rf "pkgroot"
rm -rf "pkgres"
rm -f "pkg/Distribution.xml"

# Print checksums.
echo "Checksums:"
md5 "$pkg"
echo -n "SHA256 "
shasum -a 256 "$pkg"

if [[ $packaging_operation == "make_release_package" ]]; then
    # Print the checksums of the archives zip.
    md5 "$out_dir/background-music-xcarchives-$version.zip"
    echo -n "SHA256 "
    shasum -a 256 "$out_dir/background-music-xcarchives-$version.zip"
fi

