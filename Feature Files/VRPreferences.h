//////////
//
//	File:		VRPreferences.h
//
//	Contains:	Header file for VRScript preferences management.
//
//	Written by:	Tim Monroe
//
//	Copyright:	� 1999 by Apple Computer, Inc., all rights reserved.
//
//	Change History (most recent first):
//
//	   <1>	 	06/02/99		rtm		first file
//	   
//////////

#pragma once

//////////
//
// header files
//	   
//////////

#include "ComApplication.h"
#include "VRScript.h"

#if TARGET_OS_WIN32
#include <winreg.h>
#endif

#if TARGET_OS_MAC
#include <Folders.h>
#endif


//////////
//
// constants
//
//////////

// preferences dialog item IDs
#define kPreferencesDialogID			2000

#define kPrefsButtonDone				1
#define kPrefsButtonCancel				2
#define kPrefsRadioPromptUser			3
#define kPrefsRadioDirOfMovieFile		4
#define kPrefsRadioDirOfApplication		5
#define kPrefsRadioUserSpecifiedPath	6
#define kPrefsTextEditFilePath			7
 
#define kPrefsRadioFileNamePlusVRS		8
#define kPrefsRadioUserSpecifiedName	9
#define kPrefsTextEditFileName			10

#define kPrefsFilePathUserItem			13
#define kPrefsFileNameUserItem			14


// values for fScriptLocType field of a preferences record
enum {
	kVRPrefs_PromptUser			= (UInt32)0,		// prompt the user for the location and name
	kVRPrefs_DirOfMovieFile		= (UInt32)1,		// script file is in same directory as the movie file
	kVRPrefs_DirOfApplication	= (UInt32)2,		// script file is in same directory as the application file
	kVRPrefs_UserSpecifiedPath	= (UInt32)3			// script file is in a directory specified by an absolute pathname
};

// values for fScriptNameType field of a preferences record
enum {
	kVRPrefs_FileNamePlusTXT	= (UInt32)0,		// script file name is the movie file basename with .txt suffix
	kVRPrefs_UserSpecifiedName	= (UInt32)1			// script file name is specified by a relative pathname
};

#if TARGET_OS_WIN32
// registry database labels
#define kVRPrefs_PrefsKeyName			"Software\\VRScript"
#define kVRPrefs_ScriptLocTypeLabel		"Script directory selector"
#define kVRPrefs_ScriptPathNameLabel	"Script directory pathname"
#define kVRPrefs_ScriptNameTypeLabel	"Script file name selector"
#define kVRPrefs_ScriptBaseNameLabel	"Script file basename"
#endif

#if TARGET_OS_MAC
#define kVRPrefs_PrefsFileName			"VRScript Preferences"
#endif


//////////
//
// data types
//
//////////

typedef struct VRScriptPrefsRec {
	// the desired location of the script file
	UInt16						fScriptLocType;		// the desired location of the script file
	Str255						fScriptPathName;	// the user specified pathname of the script file directory
	
	// the desired name of the script file
	UInt32						fScriptNameType;	// the desired name of the script file
	Str63						fScriptBaseName;	// the user specified basename of the script file
	
	// application execution options (NOT YET IMPLEMENTED)
	Boolean						fTurboMode;			// use turbo execution mode?
	Boolean						fVerboseMode;		// use verbose mode?
	
	// preferences-management info
	Boolean						fPrefsFileExists;	// does a preferences file exist?
	Boolean						fPrefsDataDirty;	// have the stored preferences changed?
	
} VRScriptPrefsRec, *VRScriptPrefsPtr, **VRScriptPrefsHdl;


//////////
//
// function prototypes
//
//////////

void								VRPrefs_Init (void);
void								VRPrefs_Stop (void);

void								VRPrefs_ShowPrefsDialog (void);
static void							VRPrefs_FillPrefsDialogFromPrefs (DialogPtr theDialog);
PASCAL_RTN void						VRPrefs_DrawPrefsDialogUserItem (DialogPtr theDialog, DialogItemIndex theItem);

static Boolean						VRPrefs_PrefsFileExists (void);
//static OSErr						VRPrefs_GetPrefsFileFSSpec (OSType theCreator, OSType theFileType, FSSpec *theFSSpec);
static OSErr						VRPrefs_GetPrefsFileFSSpec (char *theFileName, FSSpecPtr theFSSpec);

static void							VRPrefs_ReadPrefsFile (void);
static void							VRPrefs_UpdatePrefsFile (void);
