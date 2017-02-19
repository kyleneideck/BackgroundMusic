#!/usr/bin/python2.7

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
# travis-skip.py
# BGMAppUITests
#
# Copyright (c) 2017 Kyle Neideck
#
# Skip the UI tests in Travis builds because they aren't supported.
#
# We can't run the tests on Travis because Xcode needs permission to use the Accessibility API
# to control BGMApp. There's no way to set that up programmatically without disabling SIP and
# Travis doesn't support that.
#
# See https://github.com/travis-ci/travis-ci/issues/5819
#
# TODO: Figure out a better way to do this.
#

import xml.etree.ElementTree as ET

SCHEME_FILE = "BGMApp/BGMApp.xcodeproj/xcshareddata/xcschemes/Background Music.xcscheme"
UI_REF_XPATH = ".//BuildableReference[@BlueprintName='BGMAppUITests']/.."

# Parse the Xcode scheme.
tree = ET.parse(SCHEME_FILE)

# Set the TestableReference for the UI tests to skipped.
tree.getroot().findall(UI_REF_XPATH)[0].set("skipped", "YES")

# Save the scheme.
tree.write(SCHEME_FILE)


