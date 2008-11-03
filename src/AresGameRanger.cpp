/*
Ares, a tactical space combat game.
Copyright (C) 1997, 1999-2001, 2008 Nathan Lamont

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/

/******************************************\
|**| Ares_GameRanger.c
\******************************************/

#pragma mark **INCLUDES**
/******************************************\
|**| #includes
\******************************************/

#pragma mark _system includes_
/* - system
*******************************************/

#pragma mark _third party includes_
/* - third party libraries
*******************************************/

#include "WrapGameRanger.h"

#pragma mark _bp libraries includes_
/* - bp libraries
*******************************************/

#pragma mark _this library includes_
/* - this project
*******************************************/

#include "AresGlobalType.h"
#include "Processor.h"
#include "AresGameRanger.h"

#pragma mark **DEFINITIONS**
/******************************************\
|**| #defines
\******************************************/

/* - definitions
*******************************************/

#pragma mark _macros_
/* - macros
*******************************************/

#pragma mark **TYPEDEFS**
/******************************************\
|**| typedefs
\******************************************/

#pragma mark **EXTERNAL GLOBALS**
/******************************************\
|**| external globals
\******************************************/
extern aresGlobalType           *gAresGlobal;

#pragma mark **PRIVATE GLOBALS**
/******************************************\
|**| private globals
\******************************************/

#pragma mark **PRIVATE PROTOTYPES**
/******************************************\
|**| private function prototypes
\******************************************/

#pragma mark **PRIVATE FUNCTIONS**
/******************************************\
|**| private functions
\******************************************/

#pragma mark **PUBLIC FUNCTIONS**
/******************************************\
|**| public functions
\******************************************/

OSErr AGR_Init( void)
{
    OSErr result = noErr;


    if ( Wrap_UseGameRanger())
    {
        result = Wrap_GRInstallResumeHandler();
        if ( result == noErr) gAresGlobal->useGameRanger = true;
        else gAresGlobal->useGameRanger = false;

    } else
    {
        gAresGlobal->useGameRanger = false;
    }

    return result;

}