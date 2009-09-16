//
//  File:       StdPrefsLib.h
//
//  Contains:   Header file for standard preferences library routines.
//
//              Refer to develop Issue 18, "The Right Way to Implement
//              Preferences Files", for additional details on this code.
//
//  Written by: Gary Woodcock
//
//  Copyright:  (C) 1993-94 by Apple Computer, Inc.
//  Change History (most recent first):
//
//                3/3/94    Version 1.0.
//
//  Notes:      This code uses Apple's Universal Interfaces for C.
//
//              Send bug reports to Gary Woodcock at AOL: gwoodcock
//              or Internet: gwoodcock@aol.com.
//

//-----------------------------------------------------------------------
// Includes

#ifndef ANTARES_STD_PREFS_LIB_HPP_
#define ANTARES_STD_PREFS_LIB_HPP_

//#include "CompileFlags.h"

#ifndef forRez

#include <Files.h>

#endif forRez

//-----------------------------------------------------------------------
// Public constants

#ifndef forRez

// Valid version resource ('vers') ID's
enum
{
    kVers1 = 1,
    kVers2
};

//-----------------------------------------------------------------------
// Public interfaces

//-----------------------------------------------------------------------
// NewPreferencesFile creates a new preferences file with the specified
// creator, file type and name in the System Folder (prior to System 7)
// or the Preferences folder (System 7 or later). File names with zero-length
// strings are not permitted. You can optionally specify a custom
// preferences folder of your own with the folderName argument; pass nil
// for this argument if you don't want to use a custom preferences
// folder. If a folder name that is not a zero-length string is provided,
// NewPreferencesFile checks to see if the folder exists; if it doesn't,
// NewPreferencesFile creates the folder. Another option is to provide
// the name of the program (application, extension, etc.) in the ownerName
// argument; if this argument is supplied, a custom application-missing
// message string resource is created for the preferences file.
//
// Errors:
//
//    paramErr    - The fileName or ownerName argument is a zero-length
//                  string.
//    dupFNErr    - A preferences file with this name, creator and file
//                  type already exists.
//
//    Other ResError() or file system codes can be returned, but
//    this should not normally occur.
//-----------------------------------------------------------------------
STUB5(NewPreferencesFile, OSErr(OSType creator, OSType fileType,
    ConstStr31Param fileName, ConstStr31Param folderName,
    ConstStr31Param ownerName), noErr);

//-----------------------------------------------------------------------
// OpenPreferencesFile opens the preferences file with the specified
// creator and file type.  You must have created the file with the
// NewPreferencesFile call before making this call.
//
// Errors:
//
//    paramErr    - The fRefNum argument is nil.
//    fnfErr      - There is no preferences file with the specified
//                  creator and file type; use NewPreferencesFile to
//                  create one.
//
//    Other ResError() or file system codes can be returned, but
//    this should not normally occur.
//-----------------------------------------------------------------------
STUB3(OpenPreferencesFile, OSErr(OSType creator, OSType fileType, short *fRefNum), noErr);

//-----------------------------------------------------------------------
// ClosePreferencesFile closes the currently open preferences file
// for this instance of the standard preferences component.  If there is
// no preferences file currently open, an error is returned.
//
// Errors:
//
//    paramErr    - Bad file reference in fRefNum argument.
//
//    Other ResError() codes can be returned, but this should not
//    normally occur.
//-----------------------------------------------------------------------
STUB1(ClosePreferencesFile, OSErr(short fRefNum), noErr);

//-----------------------------------------------------------------------
// DeletePreferencesFile deletes the preferences file with the specified
// creator and file type.  If the preferences file is open, an error is
// returned; you must make sure any preferences files are closed before
// attempting to delete them.
//
// Errors:
//
//    fBsyErr     - The specified preferences file is currently open,
//                  and must be closed before it can be deleted.
//    fnfErr      - A preferences file with the specified creator and
//                  file type could not be found.
//
//    Other file system codes can be returned, but this should not
//    normally occur.
//-----------------------------------------------------------------------
STUB2(DeletePreferencesFile, OSErr(OSType creator, OSType fileType), noErr);

//-----------------------------------------------------------------------
// DeletePreferencesFolder deletes the preferences folder specified by the
// folderName argument. An attempt will be made to locate the specified
// folder by first searching the Preferences folder and its nested folders
// (if the Preferences folder exists), then by searching the System Folder.
//
// Errors:
//
//    paramErr    - The folderName argument is a zero-length string.
//    dirNFErr    - A folder with the specified name could not be
//                  found.
//
//    Other file system codes can be returned, but this should not
//    normally occur.
//-----------------------------------------------------------------------
STUB1(DeletePreferencesFolder, OSErr(ConstStr31Param folderName), noErr);

//-----------------------------------------------------------------------
// PreferencesFileExists determines whether a preferences file already
// exists or not. If the file specified by the creator and file type
// exists in either the Preferences folder (anywhere) or in the System
// Folder, true is returned; if it doesn't exist, false is returned.
//
// Errors:
//
//    None.
//-----------------------------------------------------------------------
STUB2(PreferencesFileExists, bool(OSType creator, OSType fileType), noErr);

//-----------------------------------------------------------------------
// GetPreferencesFileVersion returns the contents of the specified 'vers'
// resource of the currently open preferences file.  You can use this
// information to distinguish between current and older versions of your
// preferences files (say, between the preferences files for version 1.5
// and 1.0 of your application program).  You must have called
// OpenPreferencesFile previously before making this call.  Only versID's
// with a value of kVers1 or kVers2 are allowed.
//
// Errors:
//
//    paramErr    - A value other than kVers1 or kVers2 was passed for the
//                  versID argument, a bad address was passed for the
//                  numVersion argument or the regionCode argument, or the
//                  fRefNum argument contains a bad file reference.
//
//    Other file system, ResError(), or MemError() codes can be returned,
//    but this should not normally occur.
//-----------------------------------------------------------------------
STUB6(GetPreferencesFileVersion, OSErr(short fRefNum, short versID,
    NumVersion *numVersion, short *regionCode,
    ConstStr255Param shortVersionStr, ConstStr255Param longVersionStr), noErr);

//-----------------------------------------------------------------------
// SetPreferencesFileVersion lets you set the contents of the specified
// 'vers' resource of the currently open preferences file.  You must have
// called OpenPreferencesFile previously before making this call.  Only
// versID's with a value of kVers1 or kVers2 are allowed.
//
// Errors:
//
//    paramErr    - A value other than kVers1 or kVers2 was passed for
//                  the versID argument, a bad address was passed for
//                  the numVersion argument, or the fRefNum argument
//                  contains a bad file reference.
//
//    Other file system, ResError(), or MemError() codes can be returned,
//    but this should not normally occur.
//-----------------------------------------------------------------------
STUB6(SetPreferencesFileVersion, OSErr(short fRefNum, short versID,
    NumVersion *numVersion, short regionCode,
    ConstStr255Param shortVersionStr, ConstStr255Param longVersionStr), noErr);

//-----------------------------------------------------------------------
// ReadPreference reads the specified preference resource from the
// currently open preferences file.  If you pass in nil for the address
// of the resourceID argument, or if you pass in 0 for the value of the
// resource ID, then ReadPreferences will find the first resource with
// the specified type in the currently open preferences file.  If you pass
// in 0 for the value of the resource ID, then OpenPreferencesFile will
// return the resourceID corresponding to the preference resource it found.
// You must have called OpenPreferencesFile previously before making this
// call.
//
// Errors:
//
//    paramErr    - The address of the preference argument is nil, or
//                  the fRefNum argument contains a bad file reference.
//    resNotFound - No resource with the specified type and/or ID was
//                  found.
//
//    Other ResError() codes can be returned, but this should not
//    normally occur.
//-----------------------------------------------------------------------
template <typename T>
OSErr ReadPreference(short fRefNum, ResType resourceType, short *resourceID,
        unsigned char* resourceName, TypedHandle<T> *preference) {
    static_cast<void>(fRefNum);
    static_cast<void>(resourceType);
    static_cast<void>(resourceID);
    static_cast<void>(resourceName);
    static_cast<void>(preference);
    return noErr;
}

//-----------------------------------------------------------------------
// WritePreference writes the specified preference resource to the
// currently open preferences file.  If you pass in nil for the address
// of the resourceID argument, or if you pass in 0 for the value of the
// resource ID, then WritePreferences assigns the preference resource a
// resource ID and returns it in the resourceID argument (if its address
// isn't nil).  If a resource with the same type and ID already exists
// in the preferences file, the existing resource is replaced with the
// new one.  You must have called OpenPreferencesFile previously before
// making this call.  WritePreference checks to make sure that there is a
// minimum amount of disk space left before actually writing out the
// preference to the preferences file - if there isn't enough disk space,
// the preferences file will be untouched, and an error will be returned.
// The actual size of this minimum amount is dependent on the allocation
// block size of the volume that the preferences file is located on.
// IMPORTANT NOTE:  It is the caller's responsibility to dispose the
// memory occupied by the preference argument - WritePreference does NOT
// dispose this memory.
//
// Errors:
//
//    paramErr    - The preference argument is nil, or the fRefNum argument
//                  contains a bad file reference.
//    dskFulErr   - The preference can't be written and still maintain a
//                  minimum amount of free space on the disk.
//
//    Other MemError() or ResError() codes can be returned, but this should
//    not normally occur.
//-----------------------------------------------------------------------
template <typename T>
OSErr WritePreference(short fRefNum, ResType resourceType, short *resourceID,
        unsigned char* resourceName, TypedHandle<T> preference) {
    static_cast<void>(fRefNum);
    static_cast<void>(resourceType);
    static_cast<void>(resourceID);
    static_cast<void>(resourceName);
    static_cast<void>(preference);
    return noErr;
}

//-----------------------------------------------------------------------
// DeletePreference deletes the specified preference resource from the
// currently open preferences file.  If you pass in 0 for the resourceID
// argument, then DeletePreference will delete the first resource of the
// specified type in the currently open preferences file.  You must have
// called OpenPreferenceFile previously before making this call.
//
// Errors:
//
//    paramErr    - The fRefNum argument contains a bad file reference.
//    resNotFound - No resource with the specified type and/or ID was
//                  found.
//
//    Other MemError() or ResError() codes can be returned, but this
//    should not normally occur.
//-----------------------------------------------------------------------
STUB3(DeletePreference, OSErr(short fRefNum, ResType resourceType, short resourceID), noErr);

#endif forRez

//-----------------------------------------------------------------------

#endif // ANTARES_STD_PREFS_LIB_HPP_
