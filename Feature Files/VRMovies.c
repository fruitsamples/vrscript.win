//////////
//
//	File:		VRMovies.c
//
//	Contains:	Support for QuickTime movie playback in VR nodes.
//
//	Written by:	Tim Monroe
//				Some code borrowed from QTVRSamplePlayer by Bryce Wolfson.
//
//	Copyright:	� 1996-1998 by Apple Computer, Inc., all rights reserved.
//
//	Change History (most recent first):
//
//	   <31>	 	03/17/00	rtm		made changes to get things running under CarbonLib
//	   <30>	 	09/10/99	rtm		added code to implement hide regions
//	   <29>	 	06/30/99	rtm		added fix to VRMoov_LoadEmbeddedMovie to call VRMoov_SetOneBalanceAndVolume
//									only for localized sounds
//	   <28>	 	06/29/99	rtm		reworked VRMoov_LoopEmbeddedMovie to support palindrome looping
//	   <27>	 	06/24/99	rtm		major overhaul of geometry handling, using newly-revised code from
//									VRMovies project; removed USE_COPYBITS_TO_BACKBUFFER flag, since we now
//									always use DecompressSequenceFrameS
//	   <26>	 	05/17/99	rtm		reworked geometry handling to support horizontal back buffer in QT 4.0,
//									using code from revised VRMovies project; this code now automatically
//									determines whether the back buffer is oriented horizontally or not and
//									then does the right thing in either case
//	   <25>	 	04/07/99	rtm		revised VRMoov_StartMovie
//	   <24>	 	03/29/99	rtm		added VRMoov_Init and VRMoov_Stop; conditionalized out the support for
//									on-the-fly movie rotation (too many problems reported)
//	   <23>	 	03/29/99	rtm		added VRMoov_StartMovie as a wrapper for StartMovie to handle streamed
//									movies; changed StartMovie to VRMoov_StartMovie passim
//	   <22>	 	01/26/99	rtm		changed two occurrences of gw->portPixMap to GetGWorldPixMap(gw)
//	   <21>		12/03/98	rtm		modified VRMoov_LoadEmbeddedMovie to accept a string as a parameter,
//									which can now be a full URL or a pathname
//	   <20>		09/28/98	rtm		added call to DisposeMovie in VRMoov_PlayTransitionMovie to plug leak
//	   <19>		09/11/98	rtm		added code to use DecompressImage instead of CopyBits when copying
//									an embedded movie frame into the backbuffer; set the compiler flag
//									USE_COPYBITS_TO_BACKBUFFER in VRMovies.h to select the desired call
//	   <18>		05/04/98	rtm		added support for on-the-fly movie rotation
//	   <17>		05/01/98	rtm		added VRMoov_DoIdle call to VRMoov_PlayTransitionMovie, to service
//									scene-wide sounds while a transition movie is playing; also added
//									call to FlushEvents and cleaned up surrounding code
//	   <16>		10/29/97	rtm		modified VRMoov_SetVideoGraphicsMode to use GetMovieIndTrackType
//	   <15>	 	09/18/97	rtm		added parameter to VRMoov_CheckForCompletedMovies
//	   <14>	 	06/24/97	rtm		modified VRMoov_DoIdle to service QuickTime movies with video tracks 
//									in non-frontmost windows
//	   <13>	 	06/19/97	rtm		added support for scene-wide sound-only movies
//	   <12>	 	05/13/97	rtm		added VRMoov_PlayTransitionMovie
//	   <11>	 	04/28/97	rtm		reworked to support sound-only movie files as well
//	   <10>	 	04/28/97	rtm		added VRMoov_PlayMovie; reworked passim to use linked list
//	   <9>	 	03/17/97	rtm		added VRMoov_SetMovieBalanceAndVolume
//	   <8>	 	03/12/97	rtm		copied file from VRMovies project and integrated with VRScript
//	   <7>	 	03/06/97	rtm		started to implement video masking
//	   <6>	 	03/05/97	rtm		added VRMoov_SetChromaColor; added fChromaColor to app data record
//	   <5>	 	03/04/97	rtm		fixed compositing problems at back buffer edges
//	   <4>	 	03/03/97	rtm		added VRMoov_SetVideoGraphicsMode to handle compositing
//									without using an offscreen GWorld
//	   <3>	 	02/27/97	rtm		further development: borrowed some ideas from QTVRSamplePlayer;
//									added VRMoov_SetEmbeddedMovieWidth etc.
//	   <2>	 	12/12/96	rtm		further development: borrowed some ideas from BoxMoov demo 
//	   <1>	 	12/11/96	rtm		first file 
//	   
// This code supports playing QuickTime movies in QTVR panoramas. For movies with video tracks,
// this code draws the QuickTime movie frames into the back buffer, either directly or indirectly
// (via an offscreen GWorld). Direct drawing gives the best performance, but indirect drawing is
// necessary for some visual effects. 
//
//////////

// TO DO:
// + finish video masking by custom 'hide' hot spots (so video goes *behind* such hot spots)
// + verify that everything works okay if *images* are used instead of movies (scaling seems to be a problem)


//////////
//	   
// header files
//	   
//////////

#include "VRMovies.h"
#include "VRScript.h"


//////////
//	   
// constants
//	   
//////////

const RGBColor					kBlackColor = {0x0000, 0x0000, 0x0000};		// black
const RGBColor					kWhiteColor = {0xffff, 0xffff, 0xffff};		// white


//////////
//	   
// global variables
//	   
//////////

MoviePrePrerollCompleteUPP		gMoviePPRollCompleteProc = NULL;			// a routine descriptor for our pre-preroll completion routine


//////////
//
// VRMoov_Init
// Initialize for QuickTime movie playback.
//
//////////

void VRMoov_Init (void)
{
	// allocate a routine descriptor for our pre-preroll completion routine
	if (gMoviePPRollCompleteProc == NULL)
		gMoviePPRollCompleteProc = NewMoviePrePrerollCompleteUPP(VRScript_MoviePrePrerollCompleteProc);
}


//////////
//
// VRMoov_Stop
// Close down for QuickTime movie playback.
//
//////////

void VRMoov_Stop (void)
{
	// deallocate routine descriptor
	if (gMoviePPRollCompleteProc != NULL)
		DisposeMoviePrePrerollCompleteUPP(gMoviePPRollCompleteProc);
}


//////////
//
// VRMoov_StartMovie
// Start a QuickTime movie playing.
//
// This is essentially just a wrapper for StartMovie, to handle preprerolling of streamed movies.
//
//////////

void VRMoov_StartMovie (Movie theMovie)
{	
	PrePrerollMovie(theMovie, 0, GetMoviePreferredRate(theMovie), gMoviePPRollCompleteProc, (void *)0L);
}


//////////
//
// VRMoov_PlayMovie
// Start a QuickTime movie playing or stop one from playing.
//
//////////

void VRMoov_PlayMovie (
				WindowObject theWindowObject,
				UInt32 theNodeID,
				UInt32 theEntryID, 
				float thePanAngle,
				float theTiltAngle,
				float theScale, 
				float theWidth, 
				UInt32 theKeyRed, 
				UInt32 theKeyGreen, 
				UInt32 theKeyBlue, 
				Boolean theUseBuffer,
				Boolean theUseCenter,
				Boolean theUseKey,
				Boolean theUseHide,
				Boolean theUseDir,
				Boolean theRotate,
				float theVolAngle,
				UInt32 theMode,
				UInt32 theOptions,
				char *thePathName)
{
	ApplicationDataHdl		myAppData;
	Boolean					myNeedPlayMovie = false;
	Boolean					myNeedLoadMovie = false;
	VRScriptMoviePtr		myPointer = NULL;
	
	myAppData = (ApplicationDataHdl)QTFrame_GetAppDataFromWindowObject(theWindowObject);
	if (myAppData == NULL)
		return;
	
	// see if this movie is already in our list of playing movies				
	myPointer = (VRScriptMoviePtr)VRScript_GetObjectByEntryID(theWindowObject, kVREntry_QTMovie, theEntryID);
		
	if (myPointer == NULL) {
		// this movie isn't in our list yet, so we'll need to add it to the list
		myNeedLoadMovie = true;
		myNeedPlayMovie = true;
	} else {
		// this movie is already in our list; theOptions determines how we handle this request:
		
		switch (theOptions) {
		
			case kVRMedia_PlayNew:
				// start the movie playing (whether it's already playing or not);
				// multiple movies aren't supported in QTVR 2.0, so we just map to kVRMedia_Continue
				myNeedPlayMovie = true;
				myNeedLoadMovie = true;
				break;
				
			case kVRMedia_Restart:
				// stop the current movie and then restart it; use the existing movie data
				StopMovie(myPointer->fMovie);
				myNeedPlayMovie = true;
				myNeedLoadMovie = false;
				break;
				
			case kVRMedia_ToggleStop:
				// toggle the movie's current play/stop state
				myNeedLoadMovie = false;
				if (!IsMovieDone(myPointer->fMovie)) {
					// stop the movie and get rid of movie list entry
					StopMovie(myPointer->fMovie);
					VRScript_DelistEntry(theWindowObject, (VRScriptGenericPtr)myPointer);
					myNeedPlayMovie = false;
				} else {
					// start the movie; use the existing movie data
					myNeedPlayMovie = true;
				}
				break;
				
			case kVRMedia_TogglePause:
				// toggle the movie's current play/pause state
				myNeedLoadMovie = false;
				if (GetMovieRate(myPointer->fMovie) != (Fixed)0) {
					// stop the movie
					StopMovie(myPointer->fMovie);
					myNeedPlayMovie = false;
				} else {
					// restart the movie; use the existing movie data
					myNeedPlayMovie = false;
					StartMovie(myPointer->fMovie);
				}
				break;
				
			case kVRMedia_Continue:
				// just let the movie already in progress continue
				myNeedPlayMovie = false;
				myNeedLoadMovie = false;
				break;
				
			case kVRMedia_Stop:
				// stop the current movie
				StopMovie(myPointer->fMovie);
				myNeedPlayMovie = false;
				myNeedLoadMovie = false;
				break;
				
			default:
				// unrecognized option; start the movie playing
				myNeedPlayMovie = true;
				myNeedLoadMovie = false;
				break;
		}
	}
	
	if (myNeedLoadMovie) {
		myPointer = VRScript_EnlistMovie (
						theWindowObject,
						theNodeID, 
						theEntryID,
						thePanAngle,
						theTiltAngle,
						theScale, 
						theWidth, 
						theKeyRed, 
						theKeyGreen, 
						theKeyBlue, 
						theUseBuffer,
						theUseCenter,
						theUseKey,
						theUseHide,
						theUseDir,
						theRotate,
						theVolAngle,
						theMode,
						theOptions,
						thePathName);
	}

	if (myNeedPlayMovie)
		if (myPointer != NULL)
			VRMoov_LoadEmbeddedMovie(thePathName, theWindowObject, myPointer);
}


//////////
//
// VRMoov_PlayTransitionMovie
// Play a QuickTime movie as a transition from one node to another.
//
//////////

void VRMoov_PlayTransitionMovie (WindowObject theWindowObject, UInt32 theOptions, char *thePathName)
{
	FSSpec		myFSSpec;
	short		myMovieFileRef = 0;
	Movie		myMovie;
	Rect		myRect;
	GWorldPtr	myGWorld;
	GDHandle	myGDevice;
	long		myCount = 0;
	Boolean		myUserCancelled = false;
	OSErr		myErr = noErr;
	
	if ((theWindowObject == NULL) || (thePathName == NULL))
		goto bail;
		
	FileUtils_MakeFSSpecForPathName(0, 0L, thePathName, &myFSSpec);
	
	// open the movie
	myErr = OpenMovieFile(&myFSSpec, &myMovieFileRef, fsRdPerm);
	if (myErr != noErr)
		goto bail;

	myErr = NewMovieFromFile(&myMovie, myMovieFileRef, NULL, NULL, newMovieActive, NULL);
	if ((myErr != noErr) || (myMovie == NULL))
		goto bail;

	// set the movie GWorld
	GetMovieGWorld((**theWindowObject).fMovie, &myGWorld, &myGDevice);
	SetMovieGWorld(myMovie, myGWorld, myGDevice);
			
	// set the movie box to the size of the QTVR window			
	GetMovieBox((**theWindowObject).fMovie, &myRect);	
	SetMovieBox(myMovie, &myRect);
	
	// start the movie
	GoToBeginningOfMovie(myMovie);
	VRMoov_StartMovie(myMovie);
	
	// play the movie once thru
	while (!IsMovieDone(myMovie) && !myUserCancelled) {
		MoviesTask(myMovie, 0);
#if !TARGET_API_MAC_CARBON
		SystemTask();
#endif		
		// on very long transition movies (or very slow machines), scene-wide sound-only movies need to be serviced
		// (but not every time thru the loop, eh?)
		if (myCount % kDoIdleStep == 0)
			VRMoov_DoIdle(theWindowObject);
			
		myCount++;
		
		// see if the user has cancelled the transition movie, if so allowed
		if (theOptions == kVRMovie_PlayTilClick)
			myUserCancelled = Button();
	}
	
	StopMovie(myMovie);
	DisposeMovie(myMovie);
	
	// button clicks during a transition movie remain in the OS event queue (even if detected by the
	// Button call above); we'd better remove them (by calling FlushEvents) or they may trigger a hot
	// spot in the destination node; note that if theOptions is kVRMovie_PlayTilClick, then the first
	// click stops the movie and gets flushed, but any subsequent clicks remain in the OS event queue
	// and get processed normally....
	FlushEvents(mDownMask + mUpMask, 0);
	
bail:
	if (myMovieFileRef != 0)
		CloseMovieFile(myMovieFileRef);
}


//////////
//
// VRMoov_LoadEmbeddedMovie
// Load the QuickTime movie in the specified file.
// Returns a Boolean to indicate success (true) or failure (false).
//
//////////

Boolean VRMoov_LoadEmbeddedMovie (char *thePathName, WindowObject theWindowObject, VRScriptMoviePtr theEntry)
{
	FSSpec					myFSSpec;
	short					myMovieFileRef = 0;
	Movie					myMovie = NULL;
	GWorldPtr				myGWorld = NULL;
	Rect					myRect;
	ApplicationDataHdl		myAppData = NULL;
	QTVRInstance			myInstance = NULL;
	OSErr					myErr = paramErr;

	//////////
	//
	// validate the window object and application data
	//
	//////////

	myAppData = (ApplicationDataHdl)QTFrame_GetAppDataFromWindowObject(theWindowObject);
	if (myAppData == NULL)
		goto bail;
		
	if (theEntry == NULL)
		goto bail;
		
	myInstance = (**theWindowObject).fInstance;
	if (myInstance == NULL)
		goto bail;
		
	HLock((Handle)myAppData);

	//////////
	//
	// open the movie file and the movie
	//
	//////////
	
	if (URLUtils_IsAbsoluteURL(thePathName)) {
		// we were passed a URL
		myMovie = URLUtils_NewMovieFromURL(thePathName, newMovieActive, NULL);
		if (myMovie == NULL)
			goto bail;
	} else {
		// we were passed a filename; create an FSSpec for the file
		FileUtils_MakeFSSpecForPathName(0, 0L, thePathName, &myFSSpec);
		
		// open the movie
		myErr = OpenMovieFile(&myFSSpec, &myMovieFileRef, fsRdPerm);
		if (myErr != noErr)
			goto bail;

		myErr = NewMovieFromFile(&myMovie, myMovieFileRef, NULL, (StringPtr)NULL, newMovieActive, NULL);
		if (myErr != noErr)
			goto bail;
	}

	//////////
	//
	// get the movie geometry
	//
	//////////

	GetMovieBox(myMovie, &myRect);
	GetMovieMatrix(myMovie, &theEntry->fOrigMovieMatrix);
	MacOffsetRect(&myRect, -myRect.left, -myRect.top);
	SetMovieBox(myMovie, &myRect);
	
	// determine the orientation of the back buffer
 	(**myAppData).fBackBufferIsHoriz = QTVRUtils_IsBackBufferHorizontal((**theWindowObject).fInstance);

	// keep track of the movie and movie rectangle
	theEntry->fMovie = myMovie;
	theEntry->fMovieBox = myRect;
	
	//////////
	//
	// determine what kinds of media are in this movie
	//
	//////////

	// we cannot query a streamed movie for its media types until it's been preprerolled, which we don't want to do now;
	// as a result, we'll do the best we can
	if (QTUtils_IsStreamedMovie(myMovie)) {
		theEntry->fQTMovieHasSound = true;
		theEntry->fQTMovieHasVideo = !EmptyRect(&myRect);
	} else {
		theEntry->fQTMovieHasSound = QTUtils_IsMediaTypeInMovie(myMovie, SoundMediaType) || QTUtils_IsMediaTypeInMovie(myMovie, MusicMediaType);
		theEntry->fQTMovieHasVideo = QTUtils_IsMediaTypeInMovie(myMovie, VideoMediaType);
	}

	//////////
	//
	// do processing for video media
	//
	//////////

	if (theEntry->fQTMovieHasVideo) {
		// get rid of any existing offscreen graphics world
		if (theEntry->fOffscreenGWorld != NULL) {
			UnlockPixels(theEntry->fOffscreenPixMap);
			DisposeGWorld(theEntry->fOffscreenGWorld);
			theEntry->fOffscreenGWorld = NULL;
		}
	
		// clear out any existing custom cover function and reset the video media graphics mode
		// (these may have been modified for direct-screen drawing)
		SetMovieCoverProcs(myMovie, NULL, NULL, 0);
		VRMoov_SetVideoGraphicsMode(myMovie, theEntry, false);
		
		// if necessary, create an offscreen graphics world;
		// this is where we'll image the movie before copying it into the back buffer
		// when we want to do special effects
		if (theEntry->fUseOffscreenGWorld) {
			myErr = NewGWorld(&myGWorld, 0, &myRect, NULL, NULL, 0);
			theEntry->fOffscreenGWorld = myGWorld;
			theEntry->fOffscreenPixMap = GetGWorldPixMap(myGWorld);
			
			// make an image description, which is needed by DecompressSequenceBegin
			LockPixels(theEntry->fOffscreenPixMap);
			MakeImageDescriptionForPixMap(theEntry->fOffscreenPixMap, &(theEntry->fImageDesc));
			UnlockPixels(theEntry->fOffscreenPixMap);
		} else {
			// set the video media graphics mode to drop out the chroma key color in a movie;
			// we also need to install an uncover function that doesn't erase the uncovered region
			if (theEntry->fCompositeMovie) {
				VRMoov_SetVideoGraphicsMode(myMovie, theEntry, true);
				SetMovieCoverProcs(myMovie, NewMovieRgnCoverUPP(VRMoov_CoverProc), NULL, (long)theWindowObject);
			}
		}
		
		// turn on view-changed flag, so hide region gets calculated (if necessary)
		(**myAppData).fViewHasChanged = true;
	}
	
	//////////
	//
	// do processing for sound media
	//
	//////////

	if (theEntry->fQTMovieHasSound) {
		// get the sound media handler
		theEntry->fMediaHandler = QTUtils_GetSoundMediaHandler(myMovie);

		// set initial balance and volume for localized sounds
		if (theEntry->fSoundIsLocalized) {
			VRMoov_SetOneBalanceAndVolume(myMovie, theEntry->fMediaHandler, QTVRGetPanAngle(myInstance), QTVRGetTiltAngle(myInstance), theEntry->fMovieCenter.x, theEntry->fVolAngle);
			(**myAppData).fSoundHasChanged = true;
		}
	}
		
	//////////
	//
	// install the back-buffer imaging procedure
	//
	//////////
	
	if ((**theWindowObject).fInstance != NULL)
		VRScript_InstallBackBufferImagingProc((**theWindowObject).fInstance, theWindowObject);
		
	//////////
	//
	// start the movie playing
	//
	//////////
	
	switch (theEntry->fMode) {
		case kVRPlay_Once:					// play the movie once thru
			GoToBeginningOfMovie(myMovie);
			VRMoov_StartMovie(myMovie);
			break;
		case kVRPlay_Loop:					// start the movie playing in a loop
			VRMoov_LoopEmbeddedMovie(myMovie, false);
			break;
		case kVRPlay_LoopPalindrome:		// start the movie playing in a palindrome loop
			VRMoov_LoopEmbeddedMovie(myMovie, true);
			break;
	}
	
bail:
	// we don't want to edit the embedded movie, so we can close the movie file
	if (myMovieFileRef != 0)
		CloseMovieFile(myMovieFileRef);
		
	HUnlock((Handle)myAppData);

	return(myErr == noErr);
}
	
	
//////////
//
// VRMoov_LoopEmbeddedMovie
// Start the QuickTime movie playing in a loop.
//
//////////

void VRMoov_LoopEmbeddedMovie (Movie theMovie, Boolean isPalindrome)
{
	TimeBase		myTimeBase;
	long 			myFlags = 0L;

	// set the movie's play hints to enhance looping performance
	SetMoviePlayHints(theMovie, hintsLoop, hintsLoop);
	
	// set the looping flag of the movie's time base
	myTimeBase = GetMovieTimeBase(theMovie);
	myFlags = GetTimeBaseFlags(myTimeBase);
	myFlags |= loopTimeBase;
	
	// set or clear the palindrome flag, depending on the specified setting
	if (isPalindrome)
		myFlags |= palindromeLoopTimeBase;
	else
		myFlags &= ~palindromeLoopTimeBase;
		
	SetTimeBaseFlags(myTimeBase, myFlags);

	// start playing the movie
	VRMoov_StartMovie(theMovie);
}
	

//////////
//
// VRMoov_DoIdle
// Do any movie-related processing that can or should occur at idle time.
// Returns true if the caller should call QTVRUpdate, false otherwise.
//
//////////

Boolean VRMoov_DoIdle (WindowObject theWindowObject)
{
	ApplicationDataHdl		myAppData;
	VRScriptMoviePtr		myPointer;
	Boolean					myNeedUpdate = false;

	if ((**theWindowObject).fInstance == NULL)
		return(myNeedUpdate);
		
	myAppData = (ApplicationDataHdl)QTFrame_GetAppDataFromWindowObject(theWindowObject);
	if (myAppData == NULL)
		return(myNeedUpdate);
	
	// walk the linked list and service any embedded sound-only movies
	myPointer = (VRScriptMoviePtr)(**myAppData).fListArray[kVREntry_QTMovie];
	while (myPointer != NULL) {
		if (myPointer->fQTMovieHasSound && !(myPointer->fQTMovieHasVideo)) {
			MoviesTask(myPointer->fMovie, 0L);
		}
		myPointer = myPointer->fNextEntry;
	}
	
	// now service the (single) embedded video movie, if there is one	
	myPointer = VRMoov_GetEmbeddedVideo(theWindowObject);
	if (myPointer != NULL) {
		// trip the back buffer procedure, which internally calls MoviesTask;
		// note that this is necessary *only* if the window containing the embedded movie
		// isn't the frontmost window
		if ((**theWindowObject).fWindow != QTFrame_GetFrontAppWindow())
			myNeedUpdate = true;
	}
	
	return(myNeedUpdate);
}
		
				
//////////
//
// VRMoov_DumpNodeMovies
// Stop playing all movies enlisted for the current node.
//
//////////

void VRMoov_DumpNodeMovies (WindowObject theWindowObject)
{
	VRMoov_DumpSelectedMovies(theWindowObject, kVRSelect_Node);
}


//////////
//
// VRMoov_DumpSceneMovies
// Stop playing all movies enlisted for the current scene.
//
//////////

void VRMoov_DumpSceneMovies (WindowObject theWindowObject)
{
	VRMoov_DumpSelectedMovies(theWindowObject, kVRSelect_Scene);
}


//////////
//
// VRMoov_DumpSelectedMovies
// Stop any existing embedded movies from playing and then clean up.
//
//////////

void VRMoov_DumpSelectedMovies (WindowObject theWindowObject, UInt32 theOptions)
{
	ApplicationDataHdl		myAppData;
	VRScriptMoviePtr		myPointer;
	VRScriptMoviePtr		myNext;

	myAppData = (ApplicationDataHdl)QTFrame_GetAppDataFromWindowObject(theWindowObject);
	if (myAppData == NULL)
		return;

	// walk the movie list and dump any movies movies associated with this node
	myPointer = (VRScriptMoviePtr)(**myAppData).fListArray[kVREntry_QTMovie];
	while (myPointer != NULL) {
		
		myNext = myPointer->fNextEntry;
		if (((myPointer->fNodeID != kVRAnyNode) && (theOptions == kVRSelect_Node)) ||
			((myPointer->fNodeID == kVRAnyNode) && (theOptions == kVRSelect_Scene)))
			VRScript_DelistEntry(theWindowObject, (VRScriptGenericPtr)myPointer);
		
		myPointer = myNext;
	}
	
	// clear the existing back buffer imaging proc
	QTVRSetBackBufferImagingProc((**theWindowObject).fInstance, NULL, 0, NULL, 0);

	// make sure the back buffer is clean
	QTVRRefreshBackBuffer((**theWindowObject).fInstance, 0);

	return;
}
	

//////////
//
// VRMoov_GetEmbeddedVideo
// Return the first embedded QuickTime movie that has a video track.
//
//////////

VRScriptMoviePtr VRMoov_GetEmbeddedVideo (WindowObject theWindowObject)
{
	ApplicationDataHdl		myAppData;
	VRScriptMoviePtr		myPointer = NULL;
	
	myAppData = (ApplicationDataHdl)QTFrame_GetAppDataFromWindowObject(theWindowObject);
	if (myAppData == NULL)
		return(myPointer);
		
	// walk our linked list of movies to find the target movie
	myPointer = (VRScriptMoviePtr)(**myAppData).fListArray[kVREntry_QTMovie];
	while (myPointer != NULL) {
		if (myPointer->fQTMovieHasVideo)
			return(myPointer);
		myPointer = myPointer->fNextEntry;
	}
	
	return(NULL);
}


//////////
//
// VRMoov_GetEmbeddedMovieWidth
// Get the width of the embedded movie.
//
//////////

float VRMoov_GetEmbeddedMovieWidth (WindowObject theWindowObject)
{
	float					myWidth = 0.0;
	VRScriptMoviePtr		myPointer;
	
	myPointer = VRMoov_GetEmbeddedVideo(theWindowObject);
	if (myPointer != NULL)
		myWidth = myPointer->fMovieWidth;
			
	return(myWidth);
}


//////////
//
// VRMoov_SetEmbeddedMovieWidth
// Set the width of the embedded movie.
//
//////////

void VRMoov_SetEmbeddedMovieWidth (WindowObject theWindowObject, float theWidth)
{
	QTVRInstance			myInstance;
	VRScriptMoviePtr		myPointer;
	
	if (theWindowObject == NULL)
		return;
		
	myInstance = (**theWindowObject).fInstance;
	if (myInstance == NULL)
		return;
	
	myPointer = VRMoov_GetEmbeddedVideo(theWindowObject);
	if (myPointer != NULL)
		myPointer->fMovieWidth = theWidth;

	// clear out the existing area of interest
	QTVRRefreshBackBuffer(myInstance, 0);

	// reinstall the back buffer imaging procedure
	VRScript_InstallBackBufferImagingProc(myInstance, theWindowObject);
}


//////////
//
// VRMoov_GetEmbeddedMovieCenter
// Get the center of the embedded movie.
//
//////////

void VRMoov_GetEmbeddedMovieCenter (WindowObject theWindowObject, QTVRFloatPoint *theCenter)
{
	VRScriptMoviePtr		myPointer;
	
	myPointer = VRMoov_GetEmbeddedVideo(theWindowObject);
	if (myPointer == NULL) {
		theCenter->x = 0.0;
		theCenter->y = 0.0;
	} else {
		theCenter->x = myPointer->fMovieCenter.x;
		theCenter->y = myPointer->fMovieCenter.y;
	}		
}


//////////
//
// VRMoov_SetEmbeddedMovieCenter
// Set the center of the embedded movie.
//
//////////

void VRMoov_SetEmbeddedMovieCenter (WindowObject theWindowObject, const QTVRFloatPoint *theCenter)
{
	QTVRInstance			myInstance;
	float					myX, myY;
	VRScriptMoviePtr		myPointer;

	if (theWindowObject == NULL)
		return;
		
	myInstance = (**theWindowObject).fInstance;
	if (myInstance == NULL)
		return;
	
	myX = theCenter->x;
	myY = theCenter->y;
	
	// subject the values passed in to the current view constraints
	QTVRWrapAndConstrain(myInstance, kQTVRPan, myX, &myX);
	QTVRWrapAndConstrain(myInstance, kQTVRTilt, myY, &myY);
			
	myPointer = VRMoov_GetEmbeddedVideo(theWindowObject);
	if (myPointer != NULL) {
		myPointer->fMovieCenter.x = myX;
		myPointer->fMovieCenter.y = myY;
	}
	
	// clear out the existing area of interest
	QTVRRefreshBackBuffer(myInstance, 0);

	// reinstall the back buffer imaging procedure
	VRScript_InstallBackBufferImagingProc(myInstance, theWindowObject);
}


//////////
//
// VRMoov_GetEmbeddedMovieScale
// Get the scale of the embedded movie.
//
//////////

float VRMoov_GetEmbeddedMovieScale (WindowObject theWindowObject)
{
	float					myScale;
	VRScriptMoviePtr		myPointer;
	
	myPointer = VRMoov_GetEmbeddedVideo(theWindowObject);
	if (myPointer == NULL)
		myScale = 0.0;
	else
		myScale = myPointer->fMovieScale;

	return(myScale);
}


//////////
//
// VRMoov_SetEmbeddedMovieScale
// Set the scale factor of the embedded movie.
//
//////////

void VRMoov_SetEmbeddedMovieScale (WindowObject theWindowObject, float theScale)
{
	QTVRInstance			myInstance;
	VRScriptMoviePtr		myPointer;

	if (theWindowObject == NULL)
		return;
		
	myInstance = (**theWindowObject).fInstance;
	if (myInstance == NULL)
		return;
	
	myPointer = VRMoov_GetEmbeddedVideo(theWindowObject);
	if (myPointer != NULL)
		myPointer->fMovieScale = theScale;
	
	// clear out the existing area of interest
	QTVRRefreshBackBuffer(myInstance, 0);

	// reinstall the back buffer imaging procedure
	VRScript_InstallBackBufferImagingProc(myInstance, theWindowObject);
}


//////////
//
// VRMoov_GetEmbeddedMovieRect
// Get the rectangle of the embedded movie.
//
//////////

void VRMoov_GetEmbeddedMovieRect (WindowObject theWindowObject, Rect *theRect)
{
	VRScriptMoviePtr		myPointer;
	
	myPointer = VRMoov_GetEmbeddedVideo(theWindowObject);
	if (myPointer == NULL) {
		theRect->left = 0.0;
		theRect->top = 0.0;
		theRect->right = 0.0;
		theRect->bottom = 0.0;
	} else {
		theRect->left = myPointer->fMovieBox.left;
		theRect->top = myPointer->fMovieBox.top;
		theRect->right = myPointer->fMovieBox.right;
		theRect->bottom = myPointer->fMovieBox.bottom;
	}		
}


//////////
//
// VRMoov_SetEmbeddedMovieRect
// Set the rectangle of the embedded movie.
//
//////////

void VRMoov_SetEmbeddedMovieRect (WindowObject theWindowObject, const Rect *theRect)
{
	QTVRInstance			myInstance;
	VRScriptMoviePtr		myPointer;

	if (theWindowObject == NULL)
		return;
		
	myInstance = (**theWindowObject).fInstance;
	if (myInstance == NULL)
		return;
	
	myPointer = VRMoov_GetEmbeddedVideo(theWindowObject);
	if (myPointer != NULL) {
		myPointer->fMovieBox.right = theRect->right;
		myPointer->fMovieBox.bottom = theRect->bottom;
	}

	// clear out the existing area of interest
	QTVRRefreshBackBuffer(myInstance, 0);

	// reinstall the back buffer imaging procedure
	VRScript_InstallBackBufferImagingProc(myInstance, theWindowObject);
}


//////////
//
// VRMoov_SetAllBalanceAndVolume
// Set the balance and volume attenuation of all embedded QuickTime movies making sound.
//
//////////

void VRMoov_SetAllBalanceAndVolume (WindowObject theWindowObject, float thePan, float theTilt)
{
	ApplicationDataHdl		myAppData;
	VRScriptMoviePtr		myPointer;
#if QD3D_AVAIL
	VRScript3DObjPtr		my3DObjPtr;
#endif	
	
	myAppData = (ApplicationDataHdl)QTFrame_GetAppDataFromWindowObject(theWindowObject);
	if (myAppData == NULL)
		return;

	// walk our linked list of embedded QuickTime movies and set the balance and volume of any localized sounds
	myPointer = (VRScriptMoviePtr)(**myAppData).fListArray[kVREntry_QTMovie];
	while (myPointer != NULL) {
		if (myPointer->fQTMovieHasSound)
			if (myPointer->fSoundIsLocalized)
				VRMoov_SetOneBalanceAndVolume(myPointer->fMovie, myPointer->fMediaHandler, thePan, theTilt, myPointer->fMovieCenter.x, myPointer->fVolAngle);
		myPointer = myPointer->fNextEntry;
	}
	
#if QD3D_AVAIL
	// walk our linked list of 3D objects and set the balance and volume of any texture-mapped movies
	my3DObjPtr = (VRScript3DObjPtr)(**myAppData).fListArray[kVREntry_QD3DObject];
	while (my3DObjPtr != NULL) {
		if (my3DObjPtr->fTexture != NULL)
			if (my3DObjPtr->fTextureIsMovie)
				if ((**my3DObjPtr->fTexture).fMediaHandler != NULL)
					VRMoov_SetOneBalanceAndVolume((**my3DObjPtr->fTexture).fMovie, (**my3DObjPtr->fTexture).fMediaHandler, thePan, theTilt, QTVRUtils_Point3DToPanAngle(-1.0 * my3DObjPtr->fGroupCenter.x, my3DObjPtr->fGroupCenter.y, -1.0 * my3DObjPtr->fGroupCenter.z), QTVRUtils_DegreesToRadians(kVR_TextureMovieVolAngle));
		my3DObjPtr = my3DObjPtr->fNextEntry;
	}
#endif	
}
	
	
//////////
//
// VRMoov_SetOneBalanceAndVolume
// Set the balance and volume attenuation of an embedded QuickTime movie.
//
//////////

void VRMoov_SetOneBalanceAndVolume (Movie theMovie, MediaHandler theMediaHandler, float thePan, float theTilt, float theMoviePan, float theVolAngle)
{
#pragma unused(theTilt)

	short			myValue;
	float			myPanDelta;
	float			myCosDelta;			// cosine of pan angle delta from movie center
	float			myCosLimit;			// cosine of attenuation cone limit angle
	
	myPanDelta = thePan - theMoviePan;

	// ***set the balance
	myValue = kQTMaxSoundVolume * sin(myPanDelta);
	MediaSetSoundBalance(theMediaHandler, myValue);
	
	// ***set the volume
	myCosDelta = cos(myPanDelta);
	myCosLimit = cos(theVolAngle);

	if (myCosDelta >= myCosLimit)
		// inside cone of attenuation, volume scales from 1.0 (at center) to 0.0 (at cone edge)
		myValue = kQTMaxSoundVolume * ((myCosDelta - myCosLimit) / (1 - myCosLimit));
	else
		// outside cone of attenuation, volume is 0.0;
		myValue = kNoVolume;
	
	if (myValue != GetMovieVolume(theMovie))
		SetMovieVolume(theMovie, myValue);
}


//////////
//
// VRMoov_CalcImagingMatrix
// Calculate the movie matrix required to draw the embedded movie into the specified rectangle.
//
//////////

OSErr VRMoov_CalcImagingMatrix (WindowObject theWindowObject, Rect *theBBufRect)
{
	ApplicationDataHdl		myAppData = NULL;
	VRScriptMoviePtr		myPointer = NULL;
	Rect					myDestRect = *theBBufRect;

	myAppData = (ApplicationDataHdl)QTFrame_GetAppDataFromWindowObject(theWindowObject);
	if (myAppData == NULL) 
		return(paramErr);
		
	myPointer = VRMoov_GetEmbeddedVideo(theWindowObject);
	if (myPointer == NULL) 
		return(paramErr);

	// reset the current movie matrix with the original movie matrix of the embedded movie;
	// we need to preserve that matrix in our calculations below if we are not drawing the
	// movie into an offscreen GWorld
	myPointer->fMovieMatrix = myPointer->fOrigMovieMatrix;
	
	// in general, it's easiest to construct the desired matrix by first doing the scaling
	// and then doing the rotation and translation (if necessary); so we need to swap the
	// right and bottom edges of the back buffer rectangle before doing the scaling, if a
	// rotation will also be necessary
	if (!(**myAppData).fBackBufferIsHoriz) {
		myDestRect.bottom = theBBufRect->right;
		myDestRect.right = theBBufRect->bottom;
	}
	
	// set up the scaling matrix
	// (MapMatrix concatenates the new matrix to the existing matrix, whereas RectMatrix first
	// sets the existing matrix to the identity matrix)
	if (myPointer->fUseOffscreenGWorld)
		RectMatrix(&myPointer->fMovieMatrix, &myPointer->fMovieBox, &myDestRect);
	else
		MapMatrix(&myPointer->fMovieMatrix, &myPointer->fMovieBox, &myDestRect);

	// add a rotation and translation, if necessary (and if requested to do so)
	if (!(**myAppData).fBackBufferIsHoriz)
		if (myPointer->fDoRotateMovie) {
			RotateMatrix(&myPointer->fMovieMatrix, Long2Fix(-90), 0, 0);
			TranslateMatrix(&myPointer->fMovieMatrix, 0, Long2Fix(RECT_HEIGHT(*theBBufRect)));
		}

	return(noErr);
}


//////////
//
// VRMoov_SetupDecompSeq
// Set up the decompression sequence for DecompressSequenceFrameS.
//
// This needs to be called whenever either the rectangle or the GWorld of the back buffer changes.
//
//////////

OSErr VRMoov_SetupDecompSeq (VRScriptMoviePtr theEntry, GWorldPtr theDestGWorld)
{
	short					myMode = srcCopy;
	OSErr					myErr = noErr;
	
	if (theEntry == NULL) 
		return(paramErr);

	// make sure we don't have a decompression sequence already open
	VRMoov_RemoveDecompSeq(theEntry);
	
	// set up the transfer mode
	if (theEntry->fCompositeMovie)
		myMode = srcCopy | transparent;
		
	// set up the image decompression sequence
	myErr = DecompressSequenceBegin(	&theEntry->fImageSequence,
										theEntry->fImageDesc,
										(CGrafPtr)theDestGWorld,
										NULL,
										NULL,							// entire source image
										&theEntry->fMovieMatrix,
				   						myMode,
										NULL,							// no mask
										0,
										(**(theEntry->fImageDesc)).spatialQuality,
										NULL);
									
	return(myErr);
}


//////////
//
// VRMoov_RemoveDecompSeq
// Remove the decompression sequence for DecompressSequenceFrameS.
//
//////////

OSErr VRMoov_RemoveDecompSeq (VRScriptMoviePtr theEntry)
{
	OSErr					myErr = paramErr;
	
	if (theEntry != NULL) 
		if (theEntry->fImageSequence != 0) {
			myErr = CDSequenceEnd(theEntry->fImageSequence);
			theEntry->fImageSequence = 0;
		}
	
	return(myErr);
}


//////////
//
// VRMoov_BackBufferImagingProc
// The back buffer imaging procedure:
//	* get a frame of movie and image it into the back buffer
//	* also, do any additional compositing that might be desired
// This function is called by VRScript_BackBufferImagingProc.
//
//////////

PASCAL_RTN OSErr VRMoov_BackBufferImagingProc (QTVRInstance theInstance, Rect *theRect, UInt16 theAreaIndex, UInt32 theFlagsIn, UInt32 *theFlagsOut, SInt32 theRefCon)
{
#pragma unused(theAreaIndex)

	WindowObject			myWindowObject = (WindowObject)theRefCon;
	ApplicationDataHdl		myAppData = NULL;
	Movie					myMovie = NULL;
	Boolean					myIsDrawing = theFlagsIn & kQTVRBackBufferRectVisible;
	VRScriptMoviePtr		myPointer = NULL;
	GWorldPtr				myBBufGWorld, myMovGWorld;
	GDHandle				myBBufGDevice, myMovGDevice;
	Rect					myRect;
	OSErr					myErr = paramErr;
	
	//////////
	//
	// initialize; make sure that we've got the data we need to continue
	//
	//////////

	// assume we're not going to draw anything
	*theFlagsOut = 0;
	
	if ((theInstance == NULL) || (myWindowObject == NULL)) 
		return(paramErr);

	myAppData = (ApplicationDataHdl)QTFrame_GetAppDataFromWindowObject(myWindowObject);
	if (myAppData == NULL) 
		return(paramErr);

	myPointer = VRMoov_GetEmbeddedVideo(myWindowObject);
	if (myPointer == NULL)
		return(paramErr);
		
	HLock((Handle)myAppData);

	myMovie = myPointer->fMovie;
	if ((myMovie == NULL) || (IsMovieDone(myMovie))) {
		// we don't have an embedded movie or it's done playing, so remove this back-buffer imaging procedure
		QTVRSetBackBufferImagingProc((**myWindowObject).fInstance, NULL, 0, NULL, 0);
		goto bail;
	}
	
	//////////
	//
	// make sure that the movie GWorld is set correctly;
	// note that we call SetMovieGWorld only if we have to (for performance reasons)
	//
	//////////
	
	// get the current graphics world
	// (on entry, the current graphics world is [usually] set to the back buffer)
	GetGWorld(&myBBufGWorld, &myBBufGDevice);

	// get the embedded movie's graphics world
	GetMovieGWorld(myMovie, &myMovGWorld, &myMovGDevice);

	if (myPointer->fUseOffscreenGWorld) {
		// we're using an offscreen graphics world, so set movie's GWorld to be that offscreen graphics world
		if (myMovGWorld != myPointer->fOffscreenGWorld)
			SetMovieGWorld(myMovie, myPointer->fOffscreenGWorld, GetGWorldDevice(myPointer->fOffscreenGWorld));			
	} else {
		// we're not using an offscreen graphics world, so set movie GWorld to be the back buffer;
		if ((myMovGWorld != myBBufGWorld) || (myMovGDevice != myBBufGDevice))
			SetMovieGWorld(myMovie, myBBufGWorld, myBBufGDevice);
	}

	//////////
	//
	// make sure the movie rectangle and movie matrix are set correctly
	//
	//////////
	
	if (myIsDrawing) {
		// if we weren't previously visible, make sure we are now
		GetMovieBox(myMovie, &myRect);
		if (EmptyRect(&myRect))
			SetMovieBox(myMovie, &myPointer->fMovieBox);
		
		// when no offscreen GWorld is being used...
		if (!myPointer->fUseOffscreenGWorld) {
			
			// ...make sure the movie matrix is set correctly...
			if (!MacEqualRect(theRect, &myPointer->fPrevBBufRect)) {
				VRMoov_CalcImagingMatrix(myWindowObject, theRect);
				SetMovieMatrix(myMovie, &myPointer->fMovieMatrix);
			}
			
			// ...and, if we are compositing, force QuickTime to draw a movie frame when MoviesTask is called
			// (since the previous frame was automatically erased from the back buffer by QuickTime VR)
			if (myPointer->fCompositeMovie)
				UpdateMovie(myMovie);
		}
		
	} else {
		// if we're not visible, set the movie rectangle to an empty rectangle
		// so we're not wasting time trying to draw a movie frame
		MacSetRect(&myRect, 0, 0, 0, 0);
		SetMovieBox(myMovie, &myRect);
	}

	//////////
	//
	// draw a new movie frame into the movie's graphics world (and play movie sound)
	//
	//////////
	
	MoviesTask(myMovie, 0);
	
	// if we got here, everything is okay so far
	myErr = noErr;

	//////////
	//
	// perform any additional compositing
	//
	//////////
	
	// that is, draw, using the current chroma key color, anything to be dropped out of the image;
	// note that this technique works *only* if we're using an offscreen graphics world
	if (myPointer->fUseOffscreenGWorld && myPointer->fCompositeMovie  && myPointer->fUseHideRegion && myIsDrawing) {
		RGBColor	myColor;
	
		// recalculate the hide region, but only if the view has changed
		if ((**myAppData).fViewHasChanged) {
			Handle		myHotSpotList = NewHandleClear(4);
			UInt32		myHotSpotCount = 0;
			UInt32		myHotSpotID = 0;
			OSType		myHotSpotType;
			RgnHandle	myHotSpotRgn = NULL;
		
			// dispose of any existing hide region
			if (myPointer->fHideRegion != NULL)
				DisposeRgn(myPointer->fHideRegion);
				
			// create a new, empty hide region
			myPointer->fHideRegion = NewRgn();
				
			// get a list of all visible hot spots; here, myHotSpotList is resized (if necessary) to hold
			// a list of the IDs of all visible hot spots
			myHotSpotCount = QTVRGetVisibleHotSpots((**myWindowObject).fInstance, myHotSpotList);
			if (myHotSpotCount > 0) {
				UInt32	*myHotSpotIDPtr = NULL;
				
				HLock(myHotSpotList);
				myHotSpotIDPtr = (UInt32 *)*myHotSpotList;
				while (myHotSpotCount > 0) {
					// get the next hot spot ID in the list
					myHotSpotID = *myHotSpotIDPtr;
					
					// get the type of the hot spot
					QTVRGetHotSpotType((**myWindowObject).fInstance, myHotSpotID, &myHotSpotType);
					if (myHotSpotType == kVRMoov_HideHotSpotType) {
						myHotSpotRgn = NewRgn();
						myErr = QTVRGetHotSpotRegion((**myWindowObject).fInstance, myHotSpotID, myHotSpotRgn);
						if (myErr == noErr) {
							// merge the existing hide region and the region of this hot spot
							MacUnionRgn(myPointer->fHideRegion, myHotSpotRgn, myPointer->fHideRegion);
						}
						
						DisposeRgn(myHotSpotRgn);
					}
				
					myHotSpotIDPtr++;
					myHotSpotCount--;
				}
				
				HUnlock(myHotSpotList);
			}
			
			if (myHotSpotList != NULL)
				DisposeHandle(myHotSpotList);
		}
		
		// since we're using an offscreen graphics world, make sure we draw there
		SetGWorld(myPointer->fOffscreenGWorld, GetGWorldDevice(myPointer->fOffscreenGWorld));

		// set up compositing environment
		GetForeColor(&myColor);
		RGBForeColor(&myPointer->fChromaColor);
				
		// do the drawing
		if (myPointer->fHideRegion != NULL)
			MacPaintRgn(myPointer->fHideRegion);
			
		// restore original drawing environment
		RGBForeColor(&myColor);
		
		// restore original graphics world
		SetGWorld(myBBufGWorld, myBBufGDevice);
	}

	//////////
	//
	// if we're using an offscreen graphics world, copy it into the back buffer
	//
	//////////
	
	if (myIsDrawing) {
	
		if (myPointer->fUseOffscreenGWorld) {
			PixMapHandle	myPixMap;
			
			// if anything relevant to DecompressSequenceFrameS has changed, reset the decompression sequence
			if ((myBBufGWorld != myPointer->fPrevBBufGWorld) || !(MacEqualRect(theRect, &myPointer->fPrevBBufRect))) {
				VRMoov_CalcImagingMatrix(myWindowObject, theRect);
				VRMoov_SetupDecompSeq(myPointer, myBBufGWorld);
			}
			
			myPixMap = GetGWorldPixMap(myPointer->fOffscreenGWorld);
			LockPixels(myPixMap);
			
			// set the chroma key color, if necessary
			if (myPointer->fCompositeMovie)
				RGBBackColor(&myPointer->fChromaColor);
			
			// copy the image from the offscreen graphics world into the back buffer
			myErr = DecompressSequenceFrameS(	myPointer->fImageSequence,
#if TARGET_CPU_68K
												StripAddress(GetPixBaseAddr(myPixMap)),
#else
												GetPixBaseAddr(myPixMap),
#endif
												(**(myPointer->fImageDesc)).dataSize,
												0,
												NULL,
												NULL);

			// reset the chroma key color;
			// we need to do this because the buffer we just drew into might NOT actually
			// be the real back buffer (see Virtual Reality Programming With QuickTime VR, p. 1-154);
			// the copy between the intermediate buffer and the back buffer respects the current back color.
			if (myPointer->fCompositeMovie)
				RGBBackColor(&kWhiteColor);
				
			UnlockPixels(myPixMap);
		}
	}
	
	//////////
	//
	// finish up
	//
	//////////
	
	// keep track of the GWorld and rectangle passed to us this time
	myPointer->fPrevBBufGWorld = myBBufGWorld;
	myPointer->fPrevBBufRect = *theRect;
	
	// if we drew something, tell QuickTime VR
	if (myIsDrawing)
		*theFlagsOut = kQTVRBackBufferFlagDidDraw;
	
bail:	
	HUnlock((Handle)myAppData);
	return(myErr);
}


//////////
//
// VRMoov_CoverProc
// The cover function of the embedded movie.
//
//////////

PASCAL_RTN OSErr VRMoov_CoverProc (Movie theMovie, RgnHandle theRegion, SInt32 theRefCon)
{
#pragma unused(theMovie, theRegion, theRefCon)

	return(noErr);
}


//////////
//
// VRMoov_SetVideoGraphicsMode
// Set the video media graphics mode of the embedded movie.
//
//////////

void VRMoov_SetVideoGraphicsMode (Movie theMovie, VRScriptMoviePtr theEntry, Boolean theSetVGM)
{
	Track		myTrack;
	Media		myMedia;

	if (theEntry == NULL)
		return;
		
	myTrack = GetMovieIndTrackType(theMovie, 1, VideoMediaType, movieTrackMediaType | movieTrackEnabledOnly);
	if (myTrack != NULL) {
		myMedia = GetTrackMedia(myTrack);
		if (theSetVGM)
			MediaSetGraphicsMode(GetMediaHandler(myMedia), srcCopy | transparent, &theEntry->fChromaColor);
		else
			MediaSetGraphicsMode(GetMediaHandler(myMedia), srcCopy, &kWhiteColor);
	} 
		
	return;
}


//////////
//
// VRMoov_GetFinishedMovie
// Get the first enlisted movie that is done playing.
//
//////////

VRScriptMoviePtr VRMoov_GetFinishedMovie (WindowObject theWindowObject)
{
	ApplicationDataHdl	myAppData;
	VRScriptMoviePtr	myPointer = NULL;

	myAppData = (ApplicationDataHdl)QTFrame_GetAppDataFromWindowObject(theWindowObject);	
	if (myAppData == NULL)
		return(NULL);
	
	// walk our linked list of movies to find the target movie
	myPointer = (VRScriptMoviePtr)(**myAppData).fListArray[kVREntry_QTMovie];
	while (myPointer != NULL) {
		if (IsMovieDone(myPointer->fMovie))
			return(myPointer);
		
		myPointer = myPointer->fNextEntry;
	}
	
	return(NULL);
}


//////////
//
// VRMoov_CheckForCompletedMovies
// Clean up any movies that are finished playing.
//
//////////

void VRMoov_CheckForCompletedMovies (WindowObject theWindowObject)
{
	VRScriptMoviePtr		myPointer = NULL;
	
	// delist any completed movies for the specified movie window
	if (theWindowObject != NULL)
		while ((myPointer = VRMoov_GetFinishedMovie(theWindowObject)) != NULL)
			VRScript_DelistEntry(theWindowObject, (VRScriptGenericPtr)myPointer);
}


//////////
//
// VRMoov_DumpEntryMem
// Release any memory associated with the specified list entry.
//
//////////

void VRMoov_DumpEntryMem (VRScriptMoviePtr theEntry)
{
	if (theEntry != NULL) {
		
		if (theEntry->fMovie != NULL) {
			StopMovie(theEntry->fMovie);
			DisposeMovie(theEntry->fMovie);
		}
			
		if (theEntry->fQTMovieHasVideo) {
			if (theEntry->fOffscreenPixMap != NULL)
				UnlockPixels(theEntry->fOffscreenPixMap);
				
			if (theEntry->fOffscreenGWorld != NULL)
				DisposeGWorld(theEntry->fOffscreenGWorld);
				
			if (theEntry->fHideRegion != NULL)
				DisposeRgn(theEntry->fHideRegion);
				
			VRMoov_RemoveDecompSeq(theEntry);
			
			if (theEntry->fImageDesc != NULL)
				DisposeHandle((Handle)theEntry->fImageDesc);
		}
		
		theEntry->fMovie = NULL;
		theEntry->fOffscreenGWorld = NULL;
		theEntry->fPrevBBufGWorld = NULL;
		theEntry->fMediaHandler = NULL;
		theEntry->fHideRegion = NULL;
		theEntry->fImageDesc = NULL;
		
		free(theEntry->fPathname);
	}
}