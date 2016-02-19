#!/bin/bash
# vim: tw=0:

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
# generate_icon_pngs.sh
# Copyright © 2016 Kyle Neideck
#
# Creates the app and status bar icons for BGMApp in Images.xcassets, and DeviceIcon.icns
# for BGMDriver, from FermataIcon.pdf
#
# Requires ImageMagick (for iconizer.sh)
#

# Safe mode
set -euo pipefail
IFS=$'\n\t'

echo Copying FermataIcon.pdf into FermataIcon.imageset for the status bar icon
echo ----

(set -x; cp FermataIcon.pdf ../BGMApp/BGMApp/Images.xcassets/FermataIcon.imageset/)

echo
echo Generating app icon for BGMApp
echo ----

cp ../BGMApp/BGMApp/Images.xcassets/AppIcon.appiconset/Contents.json \
    ../BGMApp/BGMApp/Images.xcassets/AppIcon.appiconset/Contents.json.brb

sh iconizer.sh FermataIcon.pdf ../BGMApp/BGMApp

# Delete unused sizes
cd ../BGMApp/BGMApp/Images.xcassets/AppIcon.appiconset/

rm appicon_114.png appicon_144.png appicon_180.png appicon_80.png appicon_100.png appicon_120.png appicon_152.png appicon_40.png appicon_57.png appicon_72.png appicon_87.png appicon_29.png appicon_50.png appicon_58.png appicon_76.png

mv Contents.json.brb Contents.json

cd - > /dev/null

echo
echo Generating DeviceIcon.icns for BGMDriver
echo ----

set -x

cp -r ../BGMApp/BGMApp/Images.xcassets/AppIcon.appiconset ../BGMDriver/BGMDriver/DeviceIcon.iconset

cd ../BGMDriver/BGMDriver/DeviceIcon.iconset

mv appicon_1024.png icon_512x512@2x.png
mv appicon_512.png icon_512x512.png
cp icon_512x512.png icon_256x256@2x.png
mv appicon_256.png icon_256x256.png
cp icon_256x256.png icon_128x128@2x.png
mv appicon_128.png icon_128x128.png
mv appicon_64.png icon_32x32@2x.png
mv appicon_32.png icon_32x32.png
cp icon_32x32.png icon_16x16@2x.png
mv appicon_16.png icon_16x16.png

cd -

iconutil -c icns -o ../BGMDriver/BGMDriver/DeviceIcon.icns ../BGMDriver/BGMDriver/DeviceIcon.iconset

# Fail if the .icns wasn't created
ls ../BGMDriver/BGMDriver/DeviceIcon.icns

rm -r ../BGMDriver/BGMDriver/DeviceIcon.iconset


