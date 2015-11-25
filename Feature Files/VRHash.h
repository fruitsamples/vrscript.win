//////////
//
//	File:		VRHash.h
//
//	Contains:	Header file for hash table management.
//
//	Written by:	Tim Monroe
//
//	Copyright:	� 1998 by Apple Computer, Inc., all rights reserved.
//
//	Change History (most recent first):
//
//	   <1>	 	11/28/98		rtm		first file (Happy Thanksgiving!)
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


//////////
//
// constants
//	   
//////////

// general parameters for our hash table

#define kNumEntriesInTable		127			// the number of bucket pointers in the hash table

// command codes for each supported command word
// currently there are 122 command codes
enum {
	kInvalidCommand = (UInt32)0,
	kSetVerboseState,
	kOpenQTVRMovieFile,
	kReplaceMainMovie,
	kSetCurrentDirectory,
	kSetBarState,
	kSetButtonState,
	kSetResizeState,
	kSetWindowSize,
	kSetMaxWindowSize,
	kReplaceCursor,
	kSetHotSpotIDCursors,
	kSetHotSpotTypeCursors,
	kGoToNodeID,
	kShowDefaultView,
	kOpenResourceFile,
	kSetCorrection,
	kSetQuality,
	kSetSwingSpeed,
	kSetSwingDirection,	
	kSetSwingState,
	kSetPanAngle,
	kSetTiltAngle,
	kSetPanTiltZoom,
	kSetFieldOfView,
	kSetViewCenter,
	kSetPanLimits,
	kSetTiltLimits,
	kSetZoomLimits,
	kSetHotSpotState,	
	kSetTranslateState,
	kSetClickRadius,
	kSetClickTimeout,
	kSetPanTiltSpeed,
	kSetZoomSpeed,
	kSetMouseScale,
	kSetFrameRate,
	kSetViewRate,
	kSetViewTime,
	kSetViewState,	
	kSetAnimationState,
	kSetControlState,
	kSetFrameAnimState,
	kSetViewAnimState,
	kSetQTVRVisState,
	kSetCachePrefs,
	kSetMovieVolume,
	kSetTrackVolume,
	kSetSoundVolume,
	kSetSoundBalance,	
	kPlaySceneSound,
	kPlaySceneQTMidi,
	kPlayNodeQTMidi,
	kPlayNodeSound,
	kPlayNode3DSound,
	kHotSpotQTMidi,
	kHotSpotSound,
	kHotSpot3DSound,
	kHotSpotMovie,
	kTriggerHotSpot,	
	kPlayQTMidi,
	kPlaySndResource,
	kPlaySoundFile,
	kPlay3DSndResource,
	kPlay3DSndResourceAngle,
	kShowPicture,
	kShowNodePicture,
	kAtTime,
	kAtAppLaunch,
	kAtAppQuit,	
	kAtMouseOverHSID,
	kAtMouseOverHSType,
	kAtClickHSID,
	kAtClickHSType,
	kAtClickCustomButton,
	kAtClickSprite,
	kAtTriggerWiredAction,
	kAtNodeEntry,
	kAtNodeExit,
	kAtPanAngle,
	kAtTiltAngle,	
	kAtZoomAngle,
	kDoBoth,
	kDoNothing,
	kPlayMovie,
	kPlayTransMovie,
	kPlayTransEffect,
	kMoveScreen,
	kBeep,
	kProcessScript,
	kCreateBox,	
	kCreateCone,
	kCreateCylinder,
	kCreateEllipsoid,
	kCreateTorus,
	kCreateRectangle,
	kOpen3DMFFile,
	kSet3DObjLocation,
	kSet3DObjColor,
	kSet3DObjTransp,
	kSet3DObjInterp,	
	kSet3DObjBackface,
	kSet3DObjFill,
	kSet3DObjRotation,
	kSet3DObjRotState,
	kSet3DObjVisState,
	kSet3DObjTexture,
	kDestroy3DObject,
	kSet3DSndLocation,
	kSetVariable,
	kIf,
	kSetSpriteVisState,
	kSetSpriteLayer,
	kSetSpriteGraphicsMode,
	kSetSpriteImageIndex,
	kSetSpriteMatrix,
	kSetSpriteLocation,
	kSetTrackState,
	kSetTrackLayer,
	kSetMovieTime,
	kSetMovieRate,	
	kSetMovieTimeScale
};


//////////
//
// data types
//	   
//////////

// a "bucket";
// our hash table is an array of bucket pointers
typedef struct VRScriptHash {
	UInt32						fCommandCode;		// the command code for the command word
	char						*fCommandWord;		// the command word
	struct VRScriptHash			*fNextEntry;		// the next entry in the list
} VRScriptHash, *VRScriptHashPtr;


//////////
//
// function prototypes
//	   
//////////

void								VRHash_Init (void);
void								VRHash_Stop (void);
UInt32								VRHash_HashCommandWord (char *theCommandWord);
void								VRHash_PutCommandIntoTable (char *theCommandWord, UInt32 theCommandCode);
UInt32								VRHash_GetCommandCode (char *theCommandWord);
