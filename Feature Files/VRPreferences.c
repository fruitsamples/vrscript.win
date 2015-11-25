//////////
//
//	File:		VRPreferences.c
//
//	Contains:	Functions for VRScript preferences management.
//
//	Written by:	Tim Monroe
//
//	Copyright:	� 1999 by Apple Computer, Inc., all rights reserved.
//
//	Change History (most recent first):
//
//	   <6>	 	03/17/00		rtm		made changes to get things running under CarbonLib
//	   <5>	 	06/06/99		rtm		added kVRPrefs_PromptUser option
//	   <4>	 	06/05/99		rtm		tweaked VRPrefs_Init so that default values are identical with the
//										hard-coded values of earlier versions of VRScript
//	   <3>	 	06/04/99		rtm		added functions to read and write preferences on Windows and MacOS
//	   <2>	 	06/03/99		rtm		added dialog box handling code
//	   <1>	 	06/02/99		rtm		first file
//	
//	This file defines functions that we use to manage the VRScript preferences. Currently we support
//	preferences that indicate the name and location of the script file associated with a VR movie, but
//	it would be easy to add other preferences as well.
//
//	A word about terminology: on the Macintosh, we store our preferences in a file (typically in the
//	Preferences folder inside the System folder), which we hereafter call the "preferences file". On
//	Windows, we store our preferences in the registry database. Therefore, on Windows, there is really
//	no single preferences file that contains our application preferences. Nonetheless, in the comments
//	below (and in the naming of some of the functions), we shall refer to a preferences file; in general,
//	you can interpret this to mean: the persistent collection of data that stores the application
//	preferences (whether that collection is an actual file or a chunk of data in some other file).
//
//	A good discussion of Mac-based preference handling is contained in Gary Woodcock's article in develop,
//	issue 18. I have occasionally used an idea or two from the source code that accompanies that article,
//	but in general I had to start from scratch, largely because he assumes that all preferences should be
//	stored in resources (a tactic that may be correct for Macintosh applications but which does not easily 
//	translate to Windows).
//
//////////


//////////
//
// header files
//
//////////

#include "VRPreferences.h"


//////////
//
// global variables
//
//////////

VRScriptPrefsHdl				gPreferences;			// a handle to the global preferences record
static UserItemUPP				gUserItemUPP;			// UPP to our user-item drawing procedure

extern ModalFilterUPP			gModalFilterUPP;		// UPP to our custom dialog event filter


///////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Start-up and shut-down utilities.
//
// Use these functions to initialize and deinitialize our preference handling.
//
///////////////////////////////////////////////////////////////////////////////////////////////////////////

//////////
//
// VRPrefs_Init
// Initialize the application's global preferences.
//
//////////

void VRPrefs_Init (void)
{
	Boolean			myPrefsExist = false;
	
	// allocate a record to hold the application preferences information
	gPreferences = (VRScriptPrefsHdl)NewHandleClear(sizeof(VRScriptPrefsRec));
	if (gPreferences == NULL)
		QTFrame_QuitFramework();		// this should never happen; if it does, we are REALLY in trouble...
	
	// look for a preferences file
	myPrefsExist = VRPrefs_PrefsFileExists();
	if (myPrefsExist) {
		// if one exists, open it and read the existing preferences from it
		VRPrefs_ReadPrefsFile();
		(**gPreferences).fPrefsFileExists = true;
		(**gPreferences).fPrefsDataDirty = false;
	} else {
		// if none exists, initialize the application preferences to a default state
		(**gPreferences).fScriptLocType = kVRPrefs_DirOfMovieFile;
		(**gPreferences).fScriptNameType = kVRPrefs_UserSpecifiedName;
		(**gPreferences).fTurboMode = false;
		(**gPreferences).fVerboseMode = false;
		(**gPreferences).fPrefsFileExists = false;
		(**gPreferences).fPrefsDataDirty = false;
		
		if ((**gPreferences).fScriptNameType == kVRPrefs_UserSpecifiedName) {
			// make sure there is a default user-specified name
			BlockMove(kDefaultScriptFileName, &((**gPreferences).fScriptBaseName[1]), strlen(kDefaultScriptFileName));
			(**gPreferences).fScriptBaseName[0] = strlen(kDefaultScriptFileName);
		}
	}

	// allocate a routine descriptor for the user-item drawing procedure
	gUserItemUPP = NewUserItemUPP(VRPrefs_DrawPrefsDialogUserItem);
}


//////////
//
// VRPrefs_Stop
// Close down the application's global preferences.
//
//////////

void VRPrefs_Stop (void)
{
	if (gPreferences != NULL) {
		// if necessary, update the stored preferences data
		if ((**gPreferences).fPrefsDataDirty)
			VRPrefs_UpdatePrefsFile();
		
		// get rid of the global preferences record
		DisposeHandle((Handle)gPreferences);
		gPreferences = NULL;
	}
}


///////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Dialog box utilities.
//
// Use these functions to display the Preferences dialog box and get the user's preferences from it.
//
///////////////////////////////////////////////////////////////////////////////////////////////////////////

//////////
//
// VRPrefs_ShowPrefsDialog
// Display the Preferences dialog box and handle events in it.
//
//////////

void VRPrefs_ShowPrefsDialog (void)
{
	DialogPtr			myDialog = NULL;
	DialogItemIndex		myItem = 0;
	DialogItemType		myItemType;
	Handle				myItemHandle = NULL;
	Rect				myItemRect;
	short				myIndex;
	OSErr				myErr = noErr;
	
	if (gPreferences == NULL)
		return;
		
	// open up the Preferences dialog box
	myDialog = GetNewDialog(kPreferencesDialogID, NULL, (WindowPtr)-1L);
	if (myDialog == NULL)
		return;

	// set the current values into the dialog box
	VRPrefs_FillPrefsDialogFromPrefs(myDialog);
		
	// set the OK and Cancel buttons
	SetDialogDefaultItem(myDialog, kPrefsButtonDone);
	SetDialogCancelItem(myDialog, kPrefsButtonCancel);
	
	// set the drawing procedure for the two user items
	GetDialogItem(myDialog, kPrefsFilePathUserItem, &myItemType, &myItemHandle, &myItemRect);
	SetDialogItem(myDialog, kPrefsFilePathUserItem, myItemType, (Handle)gUserItemUPP, &myItemRect);
	
	GetDialogItem(myDialog, kPrefsFileNameUserItem, &myItemType, &myItemHandle, &myItemRect);
	SetDialogItem(myDialog, kPrefsFileNameUserItem, myItemType, (Handle)gUserItemUPP, &myItemRect);
	
	// show the dialog
	MacShowWindow(GetDialogWindow(myDialog));
	
	// handle dialog box events until the user selects OK or Cancel
	do {
		ModalDialog(gModalFilterUPP, &myItem);
		
		switch (myItem) {
			// the script-file-location set of radio buttons
			case kPrefsRadioPromptUser:
			case kPrefsRadioDirOfMovieFile:
			case kPrefsRadioDirOfApplication:
			case kPrefsRadioUserSpecifiedPath:
				for (myIndex = kPrefsRadioPromptUser; myIndex <= kPrefsRadioUserSpecifiedPath; myIndex++) {
					GetDialogItem(myDialog, myIndex, &myItemType, &myItemHandle, &myItemRect);
					SetControlValue((ControlHandle)myItemHandle, (myIndex == myItem));
				}
				
				// enable script-file-name set of radio buttons, based on kPrefsRadioPromptUser
				for (myIndex = kPrefsRadioFileNamePlusVRS; myIndex <= kPrefsRadioUserSpecifiedName; myIndex++) {
					GetDialogItem(myDialog, myIndex, &myItemType, &myItemHandle, &myItemRect);
					HiliteControl((ControlHandle)myItemHandle, ((myItem == kPrefsRadioPromptUser) ? 255 : 0));
				}
				break;
				
			// the script-file-name set of radio buttons
			case kPrefsRadioFileNamePlusVRS:
			case kPrefsRadioUserSpecifiedName:
				for (myIndex = kPrefsRadioFileNamePlusVRS; myIndex <= kPrefsRadioUserSpecifiedName; myIndex++) {
					GetDialogItem(myDialog, myIndex, &myItemType, &myItemHandle, &myItemRect);
					SetControlValue((ControlHandle)myItemHandle, (myIndex == myItem));
				}
				break;
		}

	} while ((myItem != kPrefsButtonDone) && (myItem != kPrefsButtonCancel));

	// if the user selected OK, then retrieve the values from the dialog box and remember them
	if (myItem == kPrefsButtonDone) {
		// the script-file-location set of radio buttons
		for (myIndex = kPrefsRadioPromptUser; myIndex <= kPrefsRadioUserSpecifiedPath; myIndex++) {
			GetDialogItem(myDialog, myIndex, &myItemType, &myItemHandle, &myItemRect);
			if (GetControlValue((ControlHandle)myItemHandle) == 1) {
				(**gPreferences).fScriptLocType = myIndex - kPrefsRadioPromptUser;
				break;
			}
		}
		
		// the script-file-name set of radio buttons
		for (myIndex = kPrefsRadioFileNamePlusVRS; myIndex <= kPrefsRadioUserSpecifiedName; myIndex++) {
			GetDialogItem(myDialog, myIndex, &myItemType, &myItemHandle, &myItemRect);
			if (GetControlValue((ControlHandle)myItemHandle) == 1) {
				(**gPreferences).fScriptNameType = myIndex - kPrefsRadioFileNamePlusVRS;
				break;
			}
		}

		// the script directory pathname
		GetDialogItem(myDialog, kPrefsTextEditFilePath, &myItemType, &myItemHandle, &myItemRect);
		GetDialogItemText(myItemHandle, (**gPreferences).fScriptPathName);
		
		// the script name
		GetDialogItem(myDialog, kPrefsTextEditFileName, &myItemType, &myItemHandle, &myItemRect);
		GetDialogItemText(myItemHandle, (**gPreferences).fScriptBaseName);
		
		// in theory, we should check that the user actually changed something; but for now
		// we'll just assume that something changed (mostly since it won't cost much to update 
		// the stored preferences when we exit)
		(**gPreferences).fPrefsDataDirty = true;
	}
	
	DisposeDialog(myDialog);
}


//////////
//
// VRPrefs_FillPrefsDialogFromPrefs
// Set the values of the Preferences dialog box controls based on the existing values in gPreferences.
//
//////////

static void VRPrefs_FillPrefsDialogFromPrefs (DialogPtr theDialog)
{
	DialogItemType		myItemType;
	Handle				myItemHandle = NULL;
	Rect				myItemRect;
	UInt16				myIndex;

	if (gPreferences == NULL)
		return;
		
	if (theDialog == NULL)
		return;
	
	// do some bounds-checking on the radio button values
	if (((**gPreferences).fScriptLocType < kVRPrefs_PromptUser) || ((**gPreferences).fScriptLocType > kVRPrefs_UserSpecifiedPath))
		(**gPreferences).fScriptLocType = kVRPrefs_PromptUser;
	
	if (((**gPreferences).fScriptNameType < kVRPrefs_FileNamePlusTXT) || ((**gPreferences).fScriptNameType > kVRPrefs_UserSpecifiedName))
		(**gPreferences).fScriptNameType = kVRPrefs_FileNamePlusTXT;
	
	// set up the current values of the radio button sets
	for (myIndex = kPrefsRadioPromptUser; myIndex <= kPrefsRadioUserSpecifiedPath; myIndex++) {
		GetDialogItem(theDialog, myIndex, &myItemType, &myItemHandle, &myItemRect);
		SetControlValue((ControlHandle)myItemHandle, (myIndex == (**gPreferences).fScriptLocType + kPrefsRadioPromptUser));
	}
				
	// enable script-file-name set of radio buttons, based on kPrefsRadioPromptUser
	for (myIndex = kPrefsRadioFileNamePlusVRS; myIndex <= kPrefsRadioUserSpecifiedName; myIndex++) {
		GetDialogItem(theDialog, myIndex, &myItemType, &myItemHandle, &myItemRect);
		HiliteControl((ControlHandle)myItemHandle, (((**gPreferences).fScriptLocType + kPrefsRadioPromptUser == kPrefsRadioPromptUser) ? 255 : 0));
	}

	for (myIndex = kPrefsRadioFileNamePlusVRS; myIndex <= kPrefsRadioUserSpecifiedName; myIndex++) {
		GetDialogItem(theDialog, myIndex, &myItemType, &myItemHandle, &myItemRect);
		SetControlValue((ControlHandle)myItemHandle, (myIndex == (**gPreferences).fScriptNameType + kPrefsRadioFileNamePlusVRS));
	}

	// set the script directory pathname
	GetDialogItem(theDialog, kPrefsTextEditFilePath, &myItemType, &myItemHandle, &myItemRect);
	SetDialogItemText(myItemHandle, (**gPreferences).fScriptPathName);
	
	// set the script basename
	GetDialogItem(theDialog, kPrefsTextEditFileName, &myItemType, &myItemHandle, &myItemRect);
	SetDialogItemText(myItemHandle, (**gPreferences).fScriptBaseName);
}


//////////
//
// VRPrefs_DrawUserItem
// Draw any user items in the Preferences dialog box.
//
//////////

PASCAL_RTN void VRPrefs_DrawPrefsDialogUserItem (DialogPtr theDialog, DialogItemIndex theItem)
{
	DialogItemType		myItemType;
	Handle				myItemHandle = NULL;
	Rect				myItemRect;

	if (theDialog == NULL)
		return;

	// draw a rectangle around the two user items
	if ((theItem == kPrefsFilePathUserItem) || (theItem == kPrefsFileNameUserItem)) {
		GetDialogItem(theDialog, theItem, &myItemType, &myItemHandle, &myItemRect);
		MacFrameRect(&myItemRect);
	}
}


///////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Preferences file utilities.
//
// Use these functions to open the preferences file, read the data in it, and update that data.
//
///////////////////////////////////////////////////////////////////////////////////////////////////////////

//////////
//
// VRPrefs_PrefsFileExists
// Does a preferences file for our application already exist on this machine?
//
//////////

static Boolean VRPrefs_PrefsFileExists (void)
{
	Boolean		myFileExists = false;

#if TARGET_OS_WIN32
	char		mySubKeyName[] = kVRPrefs_PrefsKeyName;
	HKEY		myKey;
	
	if (RegOpenKeyEx(HKEY_CURRENT_USER, mySubKeyName, 0L, KEY_ALL_ACCESS, &myKey) == ERROR_SUCCESS) {
		RegCloseKey(myKey);
		myFileExists = true;
	}
#endif

#if TARGET_OS_MAC
	FSSpec		myFSSpec;
	OSErr		myErr = noErr;
	
	//myErr = VRPrefs_GetPrefsFileFSSpec(kScriptFileCreator, kPrefsFileType, &myFSSpec);
	myErr = VRPrefs_GetPrefsFileFSSpec(kVRPrefs_PrefsFileName, &myFSSpec);
	myFileExists = (myErr == noErr);
#endif

	return(myFileExists);
}

#if TARGET_OS_MAC
//////////
//
// VRPrefs_GetPrefsFileFSSpec
// Return, through theFSSpec, an FSSpec record for the preferences file for the specified application.
//
// If the returned OSErr is noErr, then the preferences file exists in the location specified by theFSSpec;
// if it's fnfErr, the preferences file does not exist but you could use theFSSpec to create a new file in
// the specified location. If the returned OSErr is neither noErr nor fnfErr, the preferences file does not
// exist and you should not use theFSSpec to create a new preferences file.
//
//////////

static OSErr VRPrefs_GetPrefsFileFSSpec (char *theFileName, FSSpecPtr theFSSpec)
{
	short		myVRefNum;
	long		myPrefsDirID;
	long		mySystemDirID;
	Boolean		myHasPrefsDir = false;
	Boolean		myFoundFile = false;
	StringPtr 	myName = QTUtils_ConvertCToPascalString(theFileName);
	OSErr		myErr = paramErr;

	if (theFSSpec == NULL)
		goto bail;
	
	// get the Preferences folder directory ID
	myErr = FindFolder(kOnSystemDisk, kPreferencesFolderType, kCreateFolder, &myVRefNum, &myPrefsDirID);
	myHasPrefsDir = (myErr == noErr);
	
	// get the System folder directory ID
	myErr = FindFolder(kOnSystemDisk, kSystemFolderType, kDontCreateFolder, &myVRefNum, &mySystemDirID);
	if (myErr != noErr)
		goto bail;		// no use in continuing if we cannot find the System folder

	// see if the preferences file exists in the Preferences folder
	if (myHasPrefsDir) {
		myErr = FSMakeFSSpec(myVRefNum, myPrefsDirID, myName, theFSSpec);
		if (myErr == noErr)
			myFoundFile = true;
	}
	
	// if it's not in Preferences folder, see if the preferences file exists in the System folder
	if (!myFoundFile) {
		myErr = FSMakeFSSpec(myVRefNum, mySystemDirID, myName, theFSSpec);
		if (myErr == noErr)
			myFoundFile = true;
	}

	// if we still haven't found a preferences file and if the Preferences folder exists,
	// then that's where we want the caller to create one
	if ((!myFoundFile) && (myHasPrefsDir))
		myErr = FSMakeFSSpec(myVRefNum, myPrefsDirID, myName, theFSSpec);
		
bail:
	free(myName);
	return(myErr);
}
#endif


//////////
//
// VRPrefs_ReadPrefsFile
// Read the data from the preferences file.
//
//////////

static void VRPrefs_ReadPrefsFile (void)
{
#if TARGET_OS_WIN32
	char		mySubKeyName[] = kVRPrefs_PrefsKeyName;
	HKEY		myKey;
	DWORD		myType;
	DWORD		myValue;
	DWORD		mySize;
	char		myBuffer[MAX_PATH];

	if (RegOpenKeyEx(HKEY_CURRENT_USER, mySubKeyName, 0L, KEY_ALL_ACCESS, &myKey) == ERROR_SUCCESS) {
		mySize = sizeof(DWORD);
		if (RegQueryValueEx(myKey, kVRPrefs_ScriptLocTypeLabel, NULL, &myType, (BYTE *)&myValue, &mySize) == ERROR_SUCCESS)
			(**gPreferences).fScriptLocType = (UInt32)myValue;

		mySize = MAX_PATH;
		if (RegQueryValueEx(myKey, kVRPrefs_ScriptPathNameLabel, NULL, &myType, (BYTE *)myBuffer, &mySize) == ERROR_SUCCESS) {
			BlockMove(myBuffer, (void *)&((**gPreferences).fScriptPathName[1]), mySize);
			(**gPreferences).fScriptPathName[0] = mySize;
		}
		
		mySize = sizeof(DWORD);
		if (RegQueryValueEx(myKey, kVRPrefs_ScriptNameTypeLabel, NULL, &myType, (BYTE *)&myValue, &mySize) == ERROR_SUCCESS)
			(**gPreferences).fScriptNameType = (UInt32)myValue;
		
		mySize = MAX_PATH;
		if (RegQueryValueEx(myKey, kVRPrefs_ScriptBaseNameLabel, NULL, &myType, (BYTE *)myBuffer, &mySize) == ERROR_SUCCESS) {
			BlockMove(myBuffer, (void *)&((**gPreferences).fScriptBaseName[1]), mySize);
			(**gPreferences).fScriptBaseName[0] = mySize;
		}
		
		RegCloseKey(myKey);
	}
#endif

#if TARGET_OS_MAC
	FSSpec		myFSSpec;
	short		myRefNum = 0;
	long		mySize = 0;
	OSErr		myErr = noErr;

	// find the location of the preferences file
	myErr = VRPrefs_GetPrefsFileFSSpec(kVRPrefs_PrefsFileName, &myFSSpec);
	if (myErr != noErr)
		goto bail;
		
	// open the file
	myErr = FSpOpenDF(&myFSSpec, fsRdWrPerm, &myRefNum);
	
	if (myErr == noErr)
		myErr = SetFPos(myRefNum, fsFromStart, 0);

	// get the size of the file data
	if (myErr == noErr)
		myErr = GetEOF(myRefNum, &mySize);
		
	// make sure that the file is at least as large as what we're expecting
	if (mySize < GetHandleSize((Handle)gPreferences))
		myErr = paramErr;
	
	HLock((Handle)gPreferences);

	// read the data from the file into the handle
	if (myErr == noErr)
		myErr = FSRead(myRefNum, &mySize, *gPreferences);

bail:
	HUnlock((Handle)gPreferences);
	
	if (myRefNum != 0)		
		FSClose(myRefNum);
#endif
}


//////////
//
// VRPrefs_UpdatePrefsFile
// Update the preferences file.
//
//////////

static void VRPrefs_UpdatePrefsFile (void)
{
#if TARGET_OS_WIN32
	char		mySubKeyName[] = kVRPrefs_PrefsKeyName;
	HKEY		myKey;
	DWORD		myAction;
	char		*myPathName = QTUtils_ConvertPascalToCString((**gPreferences).fScriptPathName);
	char		*myBaseName = QTUtils_ConvertPascalToCString((**gPreferences).fScriptBaseName);

	if (RegCreateKeyEx(HKEY_CURRENT_USER, mySubKeyName, 0L, NULL, REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, NULL, &myKey, &myAction) == ERROR_SUCCESS) {
		RegSetValueEx(myKey, kVRPrefs_ScriptLocTypeLabel, 0L, REG_DWORD, (BYTE *)&((**gPreferences).fScriptLocType), sizeof(DWORD));
		RegSetValueEx(myKey, kVRPrefs_ScriptPathNameLabel, 0L, REG_SZ, (BYTE *)myPathName, strlen(myPathName));
		RegSetValueEx(myKey, kVRPrefs_ScriptNameTypeLabel, 0L, REG_DWORD, (BYTE *)&((**gPreferences).fScriptNameType), sizeof(DWORD));
		RegSetValueEx(myKey, kVRPrefs_ScriptBaseNameLabel, 0L, REG_SZ, (BYTE *)myBaseName, strlen(myBaseName));
		RegCloseKey(myKey);
	}
	
	free(myPathName);
	free(myBaseName);
#endif

#if TARGET_OS_MAC
	FSSpec		myFSSpec;
	short		myRefNum = 0;
	short		myVolNum;
	long		mySize = 0;
	OSErr		myErr = noErr;
	
	// find the location of the preferences file
	myErr = VRPrefs_GetPrefsFileFSSpec(kVRPrefs_PrefsFileName, &myFSSpec);
	if ((myErr != noErr) && (myErr != fnfErr))
		goto bail;
		
	mySize = GetHandleSize((Handle)gPreferences);
	if (mySize == 0)
		goto bail;

	HLock((Handle)gPreferences);
	
	// if the preferences file doesn't exist yet, then create it
	if (!(**gPreferences).fPrefsFileExists)
		myErr = FSpCreate(&myFSSpec, kScriptFileCreator, kPrefsFileType, smSystemScript);
	
	// open the file
	if (myErr == noErr)
		myErr = FSpOpenDF(&myFSSpec, fsRdWrPerm, &myRefNum);
	
	// position the file mark to the beginning of the file and write the data
	if (myErr == noErr)
		myErr = SetFPos(myRefNum, fsFromStart, 0);

	if (myErr == noErr)
		myErr = FSWrite(myRefNum, &mySize, *gPreferences);

	if (myErr == noErr)
		myErr = SetFPos(myRefNum, fsFromStart, mySize);

	// resize the file to the number of bytes written
	if (myErr == noErr)
		myErr = SetEOF(myRefNum, mySize);
				
	// close the file			 
	if (myErr == noErr)		
		myErr = FSClose(myRefNum);

	// flush the volume
	if (myErr == noErr)		
		myErr = GetVRefNum(myRefNum, &myVolNum);

	if (myErr == noErr)		
		myErr = FlushVol(NULL, myVolNum);
		
bail:
	HUnlock((Handle)gPreferences);
#endif
}


