#!/bin/bash

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
#  set-version.sh
#  SharedSource
#
#  Copyright © 2020 Kyle Neideck
#
#  Append the git HEAD short ID to the build version for SNAPSHOT and DEBUG builds. For example,
#  this might change the version string from "0.4.0" to "0.4.0-SNAPSHOT-abc0123".
#
#  Thanks to Václav Slavík for the initial version of this:
#  <http://stackoverflow.com/a/26354117/1091063>.
#
#  TODO: Update CFBundleVersion as well?
#

# If HEAD isn't tagged, or has "SNAPSHOT" or "DEBUG" in the tag name, this is a snapshot build.
# If HEAD is tagged more than once, use the most recent.
TAG=$(/usr/bin/git tag --points-at HEAD --sort='-taggerdate' 2>/dev/null | head -n 1)

if [[ $? -eq 0 ]] && ( [[ "${TAG}" == "" ]] || \
        [[ "${TAG}" =~ .*SNAPSHOT.* ]] || \
        [[ "${TAG}" =~ .*DEBUG.* ]] ); then
    head_short_id=$(/usr/bin/git rev-list HEAD --max-count=1 --abbrev-commit)
    info_plist="${BUILT_PRODUCTS_DIR}/${INFOPLIST_PATH}"

    if [[ "${CONFIGURATION}" != "Release" ]]; then
        build_type="DEBUG"
    else
        build_type="SNAPSHOT"
    fi

    if [[ -f "$info_plist" ]]; then
        current_version=$(/usr/libexec/PlistBuddy -c "Print :CFBundleShortVersionString" "${info_plist}")
        base_version=$(/usr/libexec/PlistBuddy -c "Print :BGMBundleVersionBase" "${info_plist}" 2>/dev/null)

        if [[ $? -ne 0 ]] || [[ "${base_version}" == "" ]]; then
            base_version="${current_version}"
            /usr/libexec/PlistBuddy -c "Add :BGMBundleVersionBase string ${base_version}" "${info_plist}"
        fi

        new_version="${base_version}-${build_type}-${head_short_id}"

        if [[ "${new_version}" != "${current_version}" ]]; then  # Only touch the file if we need to.
            /usr/libexec/PlistBuddy -c "Set :CFBundleShortVersionString ${new_version}" "${info_plist}"
        fi
    fi
fi

