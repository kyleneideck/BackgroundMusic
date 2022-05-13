// This file is part of Background Music.
//
// Background Music is free software: you can redistribute it and/or
// modify it under the terms of the GNU General Public License as
// published by the Free Software Foundation, either version 2 of the
// License, or (at your option) any later version.
//
// Background Music is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with Background Music. If not, see <http://www.gnu.org/licenses/>.

//
//  ListInputDevices.swift
//
//  Copyright © 2022 Kyle Neideck
//
//  The postinstall script uses this to check that BGMDevice was installed successfully.
//

import AVFoundation

var devices: Array<AVCaptureDevice>

if #available(macOS 10.15, *) {
    devices = AVCaptureDevice.DiscoverySession(
        deviceTypes: [ .builtInMicrophone, .externalUnknown ],
        mediaType: .audio,
        position: .unspecified
    ).devices
} else {
    devices = AVCaptureDevice.devices(for: .audio)
}

print(devices.map {
    (device: AVCaptureDevice) -> Array<String> in
    [device.uniqueID, device.modelID, device.localizedName]
})
