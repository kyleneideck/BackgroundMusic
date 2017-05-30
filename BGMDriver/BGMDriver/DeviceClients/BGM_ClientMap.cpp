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
//  BGM_ClientMap.cpp
//  BGMDriver
//
//  Copyright © 2016 Kyle Neideck
//  Copyright © 2017 Andrew Tonner
//

// Self Include
#include "BGM_ClientMap.h"

// Local Includes
#include "BGM_Types.h"

// PublicUtility Includes
#include "CACFDictionary.h"
#include "CAException.h"


#pragma clang assume_nonnull begin

void    BGM_ClientMap::AddClient(BGM_Client inClient)
{
    CAMutex::Locker theShadowMapsLocker(mShadowMapsMutex);
    
    // If this client has been a client in the past (and has a bundle ID), copy its previous audio settings
    auto pastClientItr = inClient.mBundleID.IsValid() ? mPastClientMap.find(inClient.mBundleID) : mPastClientMap.end();
    if(pastClientItr != mPastClientMap.end())
    {
        DebugMsg("BGM_ClientMap::AddClient: Found previous volume %f and pan %d for client %u",
                 pastClientItr->second.mRelativeVolume,
                 pastClientItr->second.mPanPosition,
                 inClient.mClientID);
        inClient.mRelativeVolume = pastClientItr->second.mRelativeVolume;
        inClient.mPanPosition = pastClientItr->second.mPanPosition;
    }
    
    // Add the new client to the shadow maps
    AddClientToShadowMaps(inClient);
    
    // Swap the maps with their shadow maps
    SwapInShadowMaps();
    
    // The shadow maps (which were the main maps until we swapped them) are now missing the new client. Add it again to
    // keep the sets of maps identical.
    AddClientToShadowMaps(inClient);
    
    // Remove the client from the past clients map (if it was in there)
    if(inClient.mBundleID.IsValid())
    {
        mPastClientMap.erase(inClient.mBundleID);
    }
}

void    BGM_ClientMap::AddClientToShadowMaps(BGM_Client inClient)
{
    ThrowIf(mClientMapShadow.count(inClient.mClientID) != 0,
            BGM_InvalidClientException(),
            "BGM_ClientMap::AddClientToShadowMaps: Tried to add client whose client ID was already in use");
    
    // Add to the client ID shadow map
    mClientMapShadow[inClient.mClientID] = inClient;
    
    // Get a reference to the client in the map so we can add it to the pointer maps
    BGM_Client& clientInMap = mClientMapShadow.at(inClient.mClientID);
    
    // Add to the PID shadow map
    mClientMapByPIDShadow[inClient.mProcessID].push_back(&clientInMap);
    
    // Add to the bundle ID shadow map
    if(inClient.mBundleID.IsValid())
    {
        mClientMapByBundleIDShadow[inClient.mBundleID].push_back(&clientInMap);
    }
}

BGM_Client    BGM_ClientMap::RemoveClient(UInt32 inClientID)
{
    CAMutex::Locker theShadowMapsLocker(mShadowMapsMutex);
    
    auto theClientItr = mClientMapShadow.find(inClientID);
    
    // Removing a client that was never added is an error
    ThrowIf(theClientItr == mClientMapShadow.end(),
            BGM_InvalidClientException(),
            "BGM_ClientMap::RemoveClient: Could not find client to be removed");
    
    BGM_Client theClient = theClientItr->second;
    
    // Insert the client into the past clients map
    if(theClient.mBundleID.IsValid())
    {
        mPastClientMap[theClient.mBundleID] = theClient;
    }
    
    // Remove the client from the shadow maps
    mClientMapShadow.erase(theClientItr);
    mClientMapByPIDShadow.erase(theClient.mProcessID);
    if(theClient.mBundleID.IsValid())
    {
        mClientMapByBundleID.erase(theClient.mBundleID);
    }
    
    // Swap the maps with their shadow maps
    SwapInShadowMaps();
    
    // Erase the client again so the maps and their shadow maps are kept identical
    mClientMapShadow.erase(inClientID);
    mClientMapByPIDShadow.erase(theClient.mProcessID);
    if(theClient.mBundleID.IsValid())
    {
        mClientMapByBundleID.erase(theClient.mBundleID);
    }
    
    return theClient;
}

bool    BGM_ClientMap::GetClientRT(UInt32 inClientID, BGM_Client* outClient) const
{
    CAMutex::Locker theMapsLocker(mMapsMutex);
    return GetClient(mClientMap, inClientID, outClient);
}

bool    BGM_ClientMap::GetClientNonRT(UInt32 inClientID, BGM_Client* outClient) const
{
    CAMutex::Locker theShadowMapsLocker(mShadowMapsMutex);
    return GetClient(mClientMapShadow, inClientID, outClient);
}

//static
bool    BGM_ClientMap::GetClient(const std::map<UInt32, BGM_Client>& inClientMap, UInt32 inClientID, BGM_Client* outClient)
{
    auto theClientItr = inClientMap.find(inClientID);
    
    if(theClientItr != inClientMap.end())
    {
        *outClient = theClientItr->second;
        return true;
    }
    
    return false;
}

std::vector<BGM_Client> BGM_ClientMap::GetClientsByPID(pid_t inPID) const
{
    CAMutex::Locker theShadowMapsLocker(mShadowMapsMutex);
    
    std::vector<BGM_Client> theClients;
    
    auto theMapItr = mClientMapByPIDShadow.find(inPID);
    if(theMapItr != mClientMapByPIDShadow.end())
    {
        // Found clients with the PID, so copy them into the return vector
        for(auto& theClientPtrsItr : theMapItr->second)
        {
            theClients.push_back(*theClientPtrsItr);
        }
    }
    
    return theClients;
}

#pragma mark Music Player

void    BGM_ClientMap::UpdateMusicPlayerFlags(pid_t inMusicPlayerPID)
{
    CAMutex::Locker theShadowMapsLocker(mShadowMapsMutex);
    
    auto theIsMusicPlayerTest = [&] (BGM_Client theClient) {
        return (theClient.mProcessID == inMusicPlayerPID);
    };
    
    UpdateMusicPlayerFlagsInShadowMaps(theIsMusicPlayerTest);
    SwapInShadowMaps();
    UpdateMusicPlayerFlagsInShadowMaps(theIsMusicPlayerTest);
}

void    BGM_ClientMap::UpdateMusicPlayerFlags(CACFString inMusicPlayerBundleID)
{
    CAMutex::Locker theShadowMapsLocker(mShadowMapsMutex);
    
    auto theIsMusicPlayerTest = [&] (BGM_Client theClient) {
        return (theClient.mBundleID.IsValid() && theClient.mBundleID == inMusicPlayerBundleID);
    };
    
    UpdateMusicPlayerFlagsInShadowMaps(theIsMusicPlayerTest);
    SwapInShadowMaps();
    UpdateMusicPlayerFlagsInShadowMaps(theIsMusicPlayerTest);
}

void    BGM_ClientMap::UpdateMusicPlayerFlagsInShadowMaps(std::function<bool(BGM_Client)> inIsMusicPlayerTest)
{
    for(auto& theItr : mClientMapShadow)
    {
        BGM_Client& theClient = theItr.second;
        theClient.mIsMusicPlayer = inIsMusicPlayerTest(theClient);
    }
}

#pragma mark App Volumes

CACFArray   BGM_ClientMap::CopyClientRelativeVolumesAsAppVolumes(CAVolumeCurve inVolumeCurve) const
{
    // Since this is a read-only, non-real-time operation, we can read from the shadow maps to avoid
    // locking the main maps.
    CAMutex::Locker theShadowMapsLocker(mShadowMapsMutex);
    
    CACFArray theAppVolumes(false);
    
    for(auto& theClientEntry : mClientMapShadow)
    {
        CopyClientIntoAppVolumesArray(theClientEntry.second, inVolumeCurve, theAppVolumes);
    }
    
    for(auto& thePastClientEntry : mPastClientMap)
    {
        CopyClientIntoAppVolumesArray(thePastClientEntry.second, inVolumeCurve, theAppVolumes);
    }
    
    return theAppVolumes;
}

void    BGM_ClientMap::CopyClientIntoAppVolumesArray(BGM_Client inClient, CAVolumeCurve inVolumeCurve, CACFArray& ioAppVolumes) const
{
    // Only include clients set to a non-default volume or pan
    if(inClient.mRelativeVolume != 1.0 || inClient.mPanPosition != 0)
    {
        CACFDictionary theAppVolume(false);
        
        theAppVolume.AddSInt32(CFSTR(kBGMAppVolumesKey_ProcessID), inClient.mProcessID);
        theAppVolume.AddString(CFSTR(kBGMAppVolumesKey_BundleID), inClient.mBundleID.CopyCFString());
        // Reverse the volume conversion from SetClientsRelativeVolumes
        theAppVolume.AddSInt32(CFSTR(kBGMAppVolumesKey_RelativeVolume),
                               inVolumeCurve.ConvertScalarToRaw(inClient.mRelativeVolume / 4));
        theAppVolume.AddSInt32(CFSTR(kBGMAppVolumesKey_PanPosition),
                               inClient.mPanPosition);
        
        ioAppVolumes.AppendDictionary(theAppVolume.GetDict());
    }
}

template <typename T>
std::vector<BGM_Client*> * _Nullable GetClientsFromMap(std::map<T, std::vector<BGM_Client*>> & map, T key) {
    auto theClientItr = map.find(key);
    if(theClientItr != map.end()) {
        return &theClientItr->second;
    }
    return nullptr;
}

std::vector<BGM_Client*> * _Nullable BGM_ClientMap::GetClients(pid_t inAppPid) {
    return GetClientsFromMap(mClientMapByPIDShadow, inAppPid);
}

std::vector<BGM_Client*> * _Nullable BGM_ClientMap::GetClients(CACFString inAppBundleID) {
    return GetClientsFromMap(mClientMapByBundleIDShadow, inAppBundleID);
}

void ShowSetRelativeVolumeMessage(pid_t inAppPID, BGM_Client* theClient);
void ShowSetRelativeVolumeMessage(CACFString inAppBundleID, BGM_Client* theClient);

void ShowSetRelativeVolumeMessage(pid_t inAppPID, BGM_Client* theClient) {
    (void)inAppPID;
    (void)theClient;
    DebugMsg("BGM_ClientMap::ShowSetRelativeVolumeMessage: Set volume %f for client %u by pid (%d)",
             theClient->mRelativeVolume,
             theClient->mClientID,
             inAppPID);
}

void ShowSetRelativeVolumeMessage(CACFString inAppBundleID, BGM_Client* theClient) {
    (void)inAppBundleID;
    (void)theClient;
    DebugMsg("BGM_ClientMap::ShowSetRelativeVolumeMessage: Set volume %f for client %u by bundle ID (%s)",
             theClient->mRelativeVolume,
             theClient->mClientID,
             CFStringGetCStringPtr(inAppBundleID.GetCFString(), kCFStringEncodingUTF8));
}

// Template method declarations are running into LLVM bug 23987
// TODO: template these.

//bool BGM_ClientMap::SetClientsRelativeVolume(pid_t inAppPID, Float32 inRelativeVolume) {
//    return SetClientsRelativeVolumeT<pid_t>(inAppPID, inRelativeVolume);
//}

//bool BGM_ClientMap::SetClientsRelativeVolume(CACFString inAppBundleID, Float32 inRelativeVolume) {
//    return SetClientsRelativeVolumeT<CACFString>(inAppBundleID, inRelativeVolume)
//}

//template <typename T>
//bool BGM_ClientMap::SetClientsRelativeVolume(T searchKey, Float32 inRelativeVolume)

bool BGM_ClientMap::SetClientsRelativeVolume(pid_t searchKey, Float32 inRelativeVolume)
{
    bool didChangeVolume = false;
    
    CAMutex::Locker theShadowMapsLocker(mShadowMapsMutex);
    
    auto theSetVolumesInShadowMapsFunc = [&] {
        // Look up the clients for the key and update their volumes
        
        auto theClients = GetClients(searchKey);
        if(theClients != nullptr)
        {
            for(BGM_Client* theClient : *theClients)
            {
                theClient->mRelativeVolume = inRelativeVolume;
                
                ShowSetRelativeVolumeMessage(searchKey, theClient);
                
                didChangeVolume = true;
            }
        }
    };
    
    theSetVolumesInShadowMapsFunc();
    SwapInShadowMaps();
    theSetVolumesInShadowMapsFunc();
    
    return didChangeVolume;
}

bool BGM_ClientMap::SetClientsRelativeVolume(CACFString searchKey, Float32 inRelativeVolume)
{
    bool didChangeVolume = false;
    
    CAMutex::Locker theShadowMapsLocker(mShadowMapsMutex);
    
    auto theSetVolumesInShadowMapsFunc = [&] {
        // Look up the clients for the key and update their volumes
        
        auto theClients = GetClients(searchKey);
        if(theClients != nullptr)
        {
            for(BGM_Client* theClient : *theClients)
            {
                theClient->mRelativeVolume = inRelativeVolume;
                
                ShowSetRelativeVolumeMessage(searchKey, theClient);
                
                didChangeVolume = true;
            }
        }
    };
    
    theSetVolumesInShadowMapsFunc();
    SwapInShadowMaps();
    theSetVolumesInShadowMapsFunc();
    
    return didChangeVolume;
}

bool BGM_ClientMap::SetClientsPanPosition(pid_t searchKey, SInt32 inPanPosition)
{
    bool didChangePanPosition = false;
    
    CAMutex::Locker theShadowMapsLocker(mShadowMapsMutex);
    
    auto theSetPansInShadowMapsFunc = [&] {
        // Look up the clients for the key and update their pan positions
        auto theClients = GetClients(searchKey);
        if(theClients != nullptr) {
            for(auto theClient: *theClients) {
                theClient->mPanPosition = inPanPosition;
                didChangePanPosition = true;
            }
        }
    };
    
    theSetPansInShadowMapsFunc();
    SwapInShadowMaps();
    theSetPansInShadowMapsFunc();
    
    return didChangePanPosition;
}

bool BGM_ClientMap::SetClientsPanPosition(CACFString searchKey, SInt32 inPanPosition)
{
    bool didChangePanPosition = false;
    
    CAMutex::Locker theShadowMapsLocker(mShadowMapsMutex);
    
    auto theSetPansInShadowMapsFunc = [&] {
        // Look up the clients for the key and update their pan positions
        auto theClients = GetClients(searchKey);
        if(theClients != nullptr) {
            for(auto theClient: *theClients) {
                theClient->mPanPosition = inPanPosition;
                didChangePanPosition = true;
            }
        }
    };
    
    theSetPansInShadowMapsFunc();
    SwapInShadowMaps();
    theSetPansInShadowMapsFunc();
    
    return didChangePanPosition;
}

void    BGM_ClientMap::UpdateClientIOStateNonRT(UInt32 inClientID, bool inDoingIO)
{
    CAMutex::Locker theShadowMapsLocker(mShadowMapsMutex);
    
    mClientMapShadow[inClientID].mDoingIO = inDoingIO;
    SwapInShadowMaps();
    mClientMapShadow[inClientID].mDoingIO = inDoingIO;
}

void    BGM_ClientMap::SwapInShadowMaps()
{
    mTaskQueue->QueueSync_SwapClientShadowMaps(this);
}

void    BGM_ClientMap::SwapInShadowMapsRT()
{
#if DEBUG
    // This method should only be called by the realtime worker thread in BGM_TaskQueue. The only safe way to call it is on a realtime
    // thread while a non-realtime thread is holding the shadow maps mutex. (These assertions assume that the realtime worker thread is
    // the only thread we'll call this on, but we could decide to change that at some point.)
    mTaskQueue->AssertCurrentThreadIsRTWorkerThread("BGM_ClientMap::SwapInShadowMapsRT");
    
    Assert(!mShadowMapsMutex.IsFree(), "Can't swap in the shadow maps while the shadow maps mutex is free");
    Assert(!mShadowMapsMutex.IsOwnedByCurrentThread(), "The shadow maps mutex should not be held by a realtime thread");
#endif
    
    CAMutex::Locker theMapsLocker(mMapsMutex);
    
    mClientMap.swap(mClientMapShadow);
    mClientMapByPID.swap(mClientMapByPIDShadow);
    mClientMapByBundleID.swap(mClientMapByBundleIDShadow);
}

#pragma clang assume_nonnull end

