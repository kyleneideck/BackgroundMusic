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
//  BGM_ClientTasks.h
//  BGMDriver
//
//  Copyright © 2016 Kyle Neideck
//
//  The interface between the client classes (BGM_Client, BGM_Clients and BGM_ClientMap) and BGM_TaskQueue.
//

#ifndef __BGMDriver__BGM_ClientTasks__
#define __BGMDriver__BGM_ClientTasks__

// Local Includes
#include "BGM_Clients.h"
#include "BGM_ClientMap.h"


// Forward Declarations
class BGM_TaskQueue;


#pragma clang assume_nonnull begin

class BGM_ClientTasks
{
    
    friend class BGM_TaskQueue;
    
private:
    static bool                            StartIONonRT(BGM_Clients* inClients, UInt32 inClientID) { return inClients->StartIONonRT(inClientID); }
    static bool                            StopIONonRT(BGM_Clients* inClients, UInt32 inClientID) { return inClients->StopIONonRT(inClientID); }
    
    static void                            SwapInShadowMapsRT(BGM_ClientMap* inClientMap) { inClientMap->SwapInShadowMapsRT(); }
    
};

#pragma clang assume_nonnull end

#endif /* __BGMDriver__BGM_ClientTasks__ */

