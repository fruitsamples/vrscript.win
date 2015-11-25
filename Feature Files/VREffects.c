//////////
//
//	File:		VREffects.c
//
//	Contains:	QuickTime video effects support for QuickTime VR movies.
//
//	Written by:	Tim Monroe
//				Parts modeled on ShowEffect sample code by Dan Crow.
//
//	Copyright:	� 1997-1998 by Apple Computer, Inc., all rights reserved.
//
//	Change History (most recent first):
//
//	   <10>	 	03/22/00	rtm		made changes to get things running under CarbonLib
//	   <9>	 	06/17/99	rtm		added HLock and HUnlock calls to VREffects_RunTransitionEffect
//	   <8>	 	05/06/99	rtm		added support for MakeImageDescriptionForEffect (QT 4.0 and later)
//	   <7>	 	04/24/98	rtm		added VRMoov_DoIdle call to VREffects_RunTransitionEffect, to service
//									scene-wide sounds while a transition is occurring 
//	   <6>	 	02/26/98	rtm		reworked VREffects_SetupTransitionEffect to use QTVRUpdate, not CopyBits;
//									now we work much better on Windows (finally!);
//									added VRScript_CheckForExpiredCommand call to VREffects_RunTransitionEffect
//	   <5>	 	02/10/98	rtm		fixed source rectangles in VREffects_SetupTransitionEffect
//	   <4>	 	02/05/98	rtm		cleaned up GWorld handling; remember, lock those pixmaps!
//	   <3>	 	11/01/97	rtm		added Endian macros to VREffects_MakeEffectDescription
//	   <2>	 	10/29/97	rtm		further work: got node transition effects working on Mac
//	   <1>	 	10/27/97	rtm		first file
//	   
//	This file provides functions to incorporate QuickTime video effects into QuickTime VR movies.
//	QuickTime video effects were introduced with QuickTime 3.0; they support many SMPTE transitions
//	and other special effects. This file defines functions that use QT video effects in these ways:
//
//	* run effects while transitioning between nodes (let's call these "transition effects")
//	* (this space for rent)
//
//////////

//	TO DO:
//	+ implement parameter handling
//	+ currently only one transition per node pair is supported; allow multiple transition effects

//////////
//
// header files
//	   
//////////

#include "VREffects.h"


//////////
//
// VREffects_InitWindowData
// Initialize for QuickTime video effects.
//
//////////

void VREffects_InitWindowData (WindowObject theWindowObject)
{
	ApplicationDataHdl			myAppData;

	myAppData = (ApplicationDataHdl)QTFrame_GetAppDataFromWindowObject(theWindowObject);
	if (myAppData != NULL) {
		(**myAppData).fSourceGWorld = NULL;
		(**myAppData).fTargetGWorld = NULL;
		(**myAppData).fSourceGWDesc = NULL;
		(**myAppData).fTargetGWDesc = NULL;
		(**myAppData).fActiveTransition = NULL;
	}
}


//////////
//
// VREffects_DumpWindowData
// Dump the window-specific data for QuickTime video effects.
//
//////////

void VREffects_DumpWindowData (WindowObject theWindowObject)
{
	ApplicationDataHdl			myAppData;
	
	myAppData = (ApplicationDataHdl)QTFrame_GetAppDataFromWindowObject(theWindowObject);
	if (myAppData != NULL) {

		// dispose of transition effects GWorlds used by this window object
		if ((**myAppData).fSourceGWorld != NULL) {
			DisposeGWorld((**myAppData).fSourceGWorld);
			(**myAppData).fSourceGWorld = NULL;
		}
		
		if ((**myAppData).fTargetGWorld != NULL) {
			DisposeGWorld((**myAppData).fTargetGWorld);
			(**myAppData).fTargetGWorld = NULL;
		}
		
		// dispose of the image descriptions
		if ((**myAppData).fSourceGWDesc != NULL) {
			DisposeHandle((Handle)(**myAppData).fSourceGWDesc);
			(**myAppData).fSourceGWDesc = NULL;
		}
		
		if ((**myAppData).fTargetGWDesc != NULL) {
			DisposeHandle((Handle)(**myAppData).fTargetGWDesc);
			(**myAppData).fTargetGWDesc = NULL;
		}
		
	}
}


//////////
//
// VREffects_DoIdle
// Do any effects-related processing that can or should occur at idle time.
// Returns true if the caller should call QTVRUpdate, false otherwise.
//
//////////

Boolean VREffects_DoIdle (WindowObject theWindowObject)
{
	ApplicationDataHdl		myAppData;
	Boolean					myNeedUpdate = false;

	myAppData = (ApplicationDataHdl)QTFrame_GetAppDataFromWindowObject(theWindowObject);
	if (myAppData != NULL) {
	
		// nothing needed for transition effects
		
	}
	
	return(myNeedUpdate);
}
		
				
//////////
//
// VREffects_GetTransitionEffect
// Return the first transition effect enlisted for the specified fromNode/toNode pair.
//
//////////

VRScriptTransitionPtr VREffects_GetTransitionEffect (WindowObject theWindowObject, UInt32 fromNodeID, UInt32 toNodeID)
{
	ApplicationDataHdl		myAppData = NULL;
	VRScriptTransitionPtr	myPointer = NULL;
	VRScriptTransitionPtr	myNext = NULL;
	
	myAppData = (ApplicationDataHdl)QTFrame_GetAppDataFromWindowObject(theWindowObject);
	if (myAppData == NULL)
		return(myPointer);
		
	// walk the transition effects list and see if any apply to this node
	myPointer = (VRScriptTransitionPtr)(**myAppData).fListArray[kVREntry_TransitionEffect];
	while (myPointer != NULL) {
		myNext = myPointer->fNextEntry;
		
		if ((myPointer->fFromNodeID == fromNodeID) || (myPointer->fFromNodeID == kVRAnyNode))
			if ((myPointer->fToNodeID == toNodeID) || (myPointer->fToNodeID == kVRAnyNode))
				return(myPointer);
	
		myPointer = myNext;
	}
	
	return(myPointer);
}


//////////
//
// VREffects_MakeEffectDescription
// Create an effect description.
//
// The effect description specifies which video effect is desired and the parameters for that effect.
// It also describes the source(s) for the effect. An effect description is simply an atom container
// that holds atoms with the appropriate information.
//
// Note that because we are creating an atom container, we must pass big-endian data (hence the calls
// to EndianU32_NtoB).
//
// Currently we support up to two sources.
//
//////////

QTAtomContainer VREffects_MakeEffectDescription (OSType theEffectType, long theEffectNum, OSType theSourceName1, OSType theSourceName2)
{
	QTAtomContainer		mySample = NULL;
	OSType				myType;
	OSErr				myErr = noErr;

	// create a new, empty effect description
	myErr = QTNewAtomContainer(&mySample);
	if (myErr != noErr)
		return(NULL);
	
	// create the effect ID atom: the atom type is kParameterWhatName, and the atom ID is kParameterWhatID
	myType = EndianU32_NtoB(theEffectType);
	myErr = QTInsertChild(mySample, kParentAtomIsContainer, kParameterWhatName, kParameterWhatID, 0, sizeof(myType), &myType, NULL);
	if (myErr != noErr)
		return(NULL);
		
	// add the first source, if it's not kSourceNoneName
	if (theSourceName1 != kSourceNoneName) {
		myType = EndianU32_NtoB(theSourceName1);
		myErr = QTInsertChild(mySample, kParentAtomIsContainer, kEffectSourceName, 1, 0, sizeof(myType), &myType, NULL);
	}
								
	// add the second source, if it's not kSourceNoneName
	if (theSourceName2 != kSourceNoneName) {
		myType = EndianU32_NtoB(theSourceName2);
		myErr = QTInsertChild(mySample, kParentAtomIsContainer, kEffectSourceName, 2, 0, sizeof(myType), &myType, NULL);
	}
	
	// add a parameter for SMPTE effects
	myType = EndianU32_NtoB(theEffectNum);
	myErr = QTInsertChild(mySample, kParentAtomIsContainer, FOUR_CHAR_CODE('wpID'), 1, 0, sizeof(myType), &myType, NULL);
	
	return(mySample);
}


//////////
//
// VREffects_MakeSampleDescription
// Create a description of an effect sample.
//
//////////

ImageDescriptionHandle VREffects_MakeSampleDescription (OSType theEffectType, short theWidth, short theHeight)
{
	ImageDescriptionHandle		mySampleDesc = NULL;

#if USES_MAKE_IMAGE_DESC_FOR_EFFECT
	OSErr						myErr = noErr;
	
	// create a new sample description
	myErr = MakeImageDescriptionForEffect(theEffectType, &mySampleDesc);
	if (myErr != noErr)
		return(NULL);
#else
	// create a new sample description
	mySampleDesc = (ImageDescriptionHandle)NewHandleClear(sizeof(ImageDescription));
	if (mySampleDesc == NULL)
		return(NULL);
	
	// fill in the fields of the sample description
	(**mySampleDesc).cType = theEffectType;
	(**mySampleDesc).idSize = sizeof(ImageDescription);
	(**mySampleDesc).hRes = 72L << 16;
	(**mySampleDesc).vRes = 72L << 16;
	(**mySampleDesc).dataSize = 0L;
	(**mySampleDesc).frameCount = 1;
	(**mySampleDesc).depth = 0;
	(**mySampleDesc).clutID = -1;
#endif

	(**mySampleDesc).vendor = kAppleManufacturer;
	(**mySampleDesc).temporalQuality = codecNormalQuality;
	(**mySampleDesc).spatialQuality = codecNormalQuality;
	(**mySampleDesc).width = theWidth;
	(**mySampleDesc).height = theHeight;
	
	return(mySampleDesc);
}


//////////
//
// VREffects_SetupTransitionEffect
// Prepare for a transition from the current node to the target node:
// * make a copy of the current screen image
// * set movie and controller GWorlds to an offscreen graphics world
//
//////////

OSErr VREffects_SetupTransitionEffect (WindowObject theWindowObject, UInt32 fromNodeID, UInt32 toNodeID)
{
	ApplicationDataHdl		myAppData = NULL;
	VRScriptTransitionPtr	myPointer = NULL;
	Rect					myRect;
	OSErr					myErr = noErr;

	myAppData = (ApplicationDataHdl)QTFrame_GetAppDataFromWindowObject(theWindowObject);
	if (myAppData == NULL)
		return(paramErr);
		
	if (((**theWindowObject).fMovie == NULL) || ((**theWindowObject).fController == NULL))
		return(paramErr);
		
	// lock the application data handle
	HLock((Handle)myAppData);
	
	// see if any transition effects are enlisted for the current fromNode/toNode pair
	myPointer = VREffects_GetTransitionEffect(theWindowObject, fromNodeID, toNodeID);
	(**myAppData).fActiveTransition = myPointer;
	if (myPointer == NULL)
		goto bail;		// if there is no active transition effect, just bail

	// make sure our slate is clean
	VREffects_DumpWindowData(theWindowObject);
	VREffects_DumpEntryMem(myPointer);

	// get the size of the movie
	GetMovieBox((**theWindowObject).fMovie, &myRect);
	MacOffsetRect(&myRect, -myRect.left, -myRect.top);
	
	// create effect and sample descriptions			
	myPointer->fEffectDesc = VREffects_MakeEffectDescription(myPointer->fEffectType, myPointer->fEffectNum, kSourceOneName, kSourceTwoName);
	myPointer->fSampleDesc = VREffects_MakeSampleDescription(myPointer->fEffectType, myRect.right, myRect.bottom);
	if ((myPointer->fEffectDesc == NULL) || (myPointer->fSampleDesc == NULL))
		goto bail;

	// create an offscreen GWorld that is the size and depth of the movie window;
	// this is the source of the transition effect
	myErr = NewGWorld(&(**myAppData).fSourceGWorld, 0, &myRect, NULL, NULL, 0L);
	if (myErr != noErr)
		goto bail;

	LockPixels(GetGWorldPixMap((**myAppData).fSourceGWorld));	
	
	// create an offscreen GWorld that is the size and depth of the movie window;
	// this is the destination of the transition effect
	myErr = NewGWorld(&(**myAppData).fTargetGWorld, 0, &myRect, NULL, NULL, 0L);
	if (myErr != noErr)
		goto bail;

	LockPixels(GetGWorldPixMap((**myAppData).fTargetGWorld));
	
	// draw the current screen image into the source offscreen GWorld
	SetMovieGWorld((**theWindowObject).fMovie, (CGrafPtr)(**myAppData).fSourceGWorld, NULL);
	QTVRUpdate((**theWindowObject).fInstance, kQTVRCurrentMode);
		
	// set the movie and controller GWorlds to the destination GWorld
	MCSetControllerPort((**theWindowObject).fController, (CGrafPtr)(**myAppData).fTargetGWorld);
	SetMovieGWorld((**theWindowObject).fMovie, (CGrafPtr)(**myAppData).fTargetGWorld, NULL);
	MCMovieChanged((**theWindowObject).fController, (**theWindowObject).fMovie);
	
bail:
	// make sure we've got no transition scheduled if an error occurred
	if (myErr != noErr)
		(**myAppData).fActiveTransition = NULL;

	// unlock the application data handle
	HUnlock((Handle)myAppData);
	
	return(myErr);
}


//////////
//
// VREffects_RunTransitionEffect
// Run a transition from the previous node to the current node.
//
//////////

OSErr VREffects_RunTransitionEffect (WindowObject theWindowObject)
{
	ApplicationDataHdl			myAppData = NULL;
	VRScriptTransitionPtr		myPointer = NULL;
	ImageSequenceDataSource		mySrc1 = 0;
	ImageSequenceDataSource		mySrc2 = 0;
	ICMFrameTimeRecord			myFrameTime;
	TimeValue					myFrame;
	PixMapHandle				mySrcPixMap;
	PixMapHandle				myDstPixMap;
	OSErr						myErr = paramErr;

	myAppData = (ApplicationDataHdl)QTFrame_GetAppDataFromWindowObject(theWindowObject);
	if (myAppData == NULL)
		goto bail;
		
	HLock((Handle)myAppData);

	// if there is no active transition effect, just bail
	myPointer = (**myAppData).fActiveTransition;
	if (myPointer == NULL)
		goto bail;
	
	if ((myPointer->fEffectDesc == NULL) || (myPointer->fSampleDesc == NULL))
		goto bail;
		
	// make an effects sequence
	HLock((Handle)myPointer->fEffectDesc);
	
	// prepare the decompression sequence for playback	
	myErr = DecompressSequenceBeginS(
							&(myPointer->fSequenceID),
							myPointer->fSampleDesc,
#if TARGET_CPU_68K
							StripAddress(*((Handle)myPointer->fEffectDesc)),
#else
							*((Handle)myPointer->fEffectDesc),
#endif
							GetHandleSize((Handle)myPointer->fEffectDesc),
							(CGrafPtr)QTFrame_GetPortFromWindowReference((**theWindowObject).fWindow),
							NULL,
							NULL,
							NULL,
							ditherCopy,
							NULL,
							0,
							codecNormalQuality,
							NULL);

	HUnlock((Handle)myPointer->fEffectDesc);
	if (myErr != noErr)
		goto bail;

	// get the pixel maps for the GWorlds (which were already locked in VREffects_SetupTransitionEffect)
	mySrcPixMap = GetGWorldPixMap((**myAppData).fSourceGWorld);
	myDstPixMap = GetGWorldPixMap((**myAppData).fTargetGWorld);
	if ((mySrcPixMap == NULL) || (myDstPixMap == NULL))
		goto bail;

	// make the effect sources
	MakeImageDescriptionForPixMap(mySrcPixMap, &(**myAppData).fSourceGWDesc);
	myErr = CDSequenceNewDataSource(
							myPointer->fSequenceID,
							&mySrc1,
							kSourceOneName,
							1,
							(Handle)(**myAppData).fSourceGWDesc,
							NULL,
							0);
	if (myErr != noErr)
		goto bail;

	CDSequenceSetSourceData(mySrc1, GetPixBaseAddr(mySrcPixMap), (**(**myAppData).fSourceGWDesc).dataSize);

	MakeImageDescriptionForPixMap(myDstPixMap, &(**myAppData).fTargetGWDesc);
	myErr = CDSequenceNewDataSource(
							myPointer->fSequenceID,
							&mySrc2,
							kSourceTwoName,
							1,
							(Handle)(**myAppData).fTargetGWDesc,
							NULL,
							0);
	if (myErr != noErr)
		goto bail;

	CDSequenceSetSourceData(mySrc2, GetPixBaseAddr(myDstPixMap), (**(**myAppData).fTargetGWDesc).dataSize);

	// dispose of any existing time base
	if (myPointer->fTimeBase != NULL)
		DisposeTimeBase(myPointer->fTimeBase);
		
	// create a new time base and associate it with the decompression sequence
	// (this time base MUST be disposed of before the application exits or bad
	// things may happen; we dispose of it when we call VREffects_DumpEntryMem
	// below)
	myPointer->fTimeBase = NewTimeBase();
	myErr = GetMoviesError();
	if (myErr != noErr)
		goto bail;
		
	SetTimeBaseRate(myPointer->fTimeBase, 0);
	myErr = CDSequenceSetTimeBase(myPointer->fSequenceID, myPointer->fTimeBase);
	if (myErr != noErr)
		goto bail;

	HLock((Handle)myPointer->fEffectDesc);
	
	// now run the effect thru from start to finish
	for (myFrame = 1; myFrame <= myPointer->fNumberOfSteps; myFrame++) {

		// set the timebase time to the step of the sequence to be rendered
		SetTimeBaseValue(myPointer->fTimeBase, myFrame, myPointer->fNumberOfSteps);
		
		myFrameTime.value.hi				= 0;
		myFrameTime.value.lo				= myFrame;
		myFrameTime.scale					= myPointer->fNumberOfSteps;
		myFrameTime.base					= 0;
		myFrameTime.duration				= myPointer->fNumberOfSteps;
		myFrameTime.rate					= 0;
		myFrameTime.recordSize				= sizeof(myFrameTime);
		myFrameTime.frameNumber				= 1;
		myFrameTime.flags					= icmFrameTimeHasVirtualStartTimeAndDuration;
		myFrameTime.virtualStartTime.hi		= 0;
		myFrameTime.virtualStartTime.lo		= 0;
		myFrameTime.virtualDuration			= myPointer->fNumberOfSteps;
		
		
		myErr = DecompressSequenceFrameWhen(myPointer->fSequenceID,
#if TARGET_CPU_68K
											StripAddress(*((Handle)myPointer->fEffectDesc)),
#else
											*((Handle)myPointer->fEffectDesc),
#endif
											GetHandleSize((Handle)myPointer->fEffectDesc),
											0,
											0,
											NULL,
											&myFrameTime);
		if (myErr != noErr)
			goto bail;
			
		// on very long transitions (or very slow machines), scene-wide sound-only movies need to be serviced
		// (but not every frame, eh?)
		if (myFrame % kDoIdleStep == 0)
			VRMoov_DoIdle(theWindowObject);
	}

	// see whether the enlisted effect has expired
	VRScript_CheckForExpiredCommand(theWindowObject, (VRScriptGenericPtr)myPointer);
	
	// if we got this far, all is okay
	myErr = noErr;
	
bail:
	if ((myPointer != NULL) && (myPointer->fEffectDesc != NULL))
		HUnlock((Handle)myPointer->fEffectDesc);
	
	// dispose of the effects GWorlds, if necessary
	VREffects_DumpWindowData(theWindowObject);
	
	// dispose of the effect sequence and time base
	VREffects_DumpEntryMem(myPointer);
	
	// reset the movie and controller GWorlds to the on-screen window
	MCSetControllerPort((**theWindowObject).fController, (CGrafPtr)QTFrame_GetPortFromWindowReference((**theWindowObject).fWindow));
	SetMovieGWorld((**theWindowObject).fMovie, (CGrafPtr)QTFrame_GetPortFromWindowReference((**theWindowObject).fWindow), NULL);
	MCMovieChanged((**theWindowObject).fController, (**theWindowObject).fMovie);
	
	// mark this transition as having been run
	(**myAppData).fActiveTransition = NULL;
	
	if (mySrcPixMap != NULL)
		UnlockPixels(mySrcPixMap);
		
	if (myDstPixMap != NULL)
		UnlockPixels(myDstPixMap);
		
	if (myAppData != NULL)
		HUnlock((Handle)myAppData);

	return(myErr);
}


//////////
//
// VREffects_DumpEntryMem
// Release any memory associated with the specified list entry.
//
//////////

void VREffects_DumpEntryMem (VRScriptTransitionPtr theEntry)
{	
	if (theEntry != NULL) {
		// if an effect sequence is currently set up, end it
		if (theEntry->fSequenceID != 0L) {
			CDSequenceEnd(theEntry->fSequenceID);
			theEntry->fSequenceID = 0L;
		}
			
		// if a time base currently exists, dispose of it
		if (theEntry->fTimeBase != NULL) {
			DisposeTimeBase(theEntry->fTimeBase);
			theEntry->fTimeBase = NULL;
		}
		
		// dispose of the effect description
		if (theEntry->fEffectDesc != NULL) {
			QTDisposeAtomContainer(theEntry->fEffectDesc);
			theEntry->fEffectDesc = NULL;
		}
		
		// dispose of the sample description
		if (theEntry->fSampleDesc != NULL) {
			DisposeHandle((Handle)theEntry->fSampleDesc);
			theEntry->fSampleDesc = NULL;
		}
	}
}