//////////
//
//	File:		VR3DTexture.c
//
//	Contains:	Support for adding a QuickTime movie or a picture as a texture on a QD3D object.
//
//	Written by:	Tim Monroe
//				Parts modeled on BoxMoov code by Rick Evans and Robert Dierkes.
//
//	Copyright:	� 1996-1998 by Apple Computer, Inc., all rights reserved.
//
//	Change History (most recent first):
//
//	   <10>	 	03/28/00	rtm		changed ((**myPixMap).rowBytes & 0x3fff) to QTGetPixMapHandleRowBytes(myPixMap)
//	   <9>	 	03/22/00	rtm		made changes to get things running under CarbonLib
//	   <8>	 	04/21/98	rtm		changed all occurrences of gw->portPixMap to GetGWorldPixMap(gw),
//									to prevent crashes reported on Windows
//	   <7>	 	03/09/98	rtm		changed PICT-specific code to use the graphics importer routines;
//									now we can open any image file format supported by QuickTime;
//									incorporated code previously in the file VR3DObjects.c, in the routines
//									VR3DObjects_LoadEmbeddedMovie and VR3DObjects_LoopEmbeddedMovie
//	   <6>	 	01/28/98	rtm		changed NewGWorld calls to QTNewGWorld
//	   <5>	 	01/27/98	rtm		made minor changes to VR3DTexture_Delete
//	   <4>	 	05/02/97	rtm		added fMediaHandler field to textures, for movies with sound
//	   <3>	 	01/14/97	rtm		added support for PICT textures
//	   <2>	 	12/17/96	rtm		modified VR3DTexture_AddToGroup to replace a texture shader
//	   <1>	 	12/16/96	rtm		first file; revised to personal coding style
//	   
//////////

#if QD3D_AVAIL


//////////
//
// header files
//	   
//////////

#include "VR3DTexture.h"


//////////
//
// VR3DTexture_New
// Create a new texture from a QuickTime movie or PICT file.
//
//////////

TextureHdl VR3DTexture_New (char *thePathName, Boolean isTextureMovie)
{
	FSSpec						myFSSpec;
	unsigned long				myPictMapAddr;
	unsigned long 				myPictRowBytes;
	GWorldPtr 					myGWorld;
	PixMapHandle 				myPixMap;
	GDHandle					myOldGD;
	GWorldPtr					myOldGW;
	Rect						myBounds;
	TQ3StoragePixmap			*myStrgPMapPtr;
	TextureHdl					myTexture = NULL;
	GraphicsImportComponent		myImporter = NULL;
	short						myMovieFileRef;
	QDErr						myErr = noErr;

	myTexture = (TextureHdl)NewHandleClear(sizeof(Texture));
	if (myTexture == NULL)
		return(myTexture);
		
	HLock((Handle)myTexture);

	// save the current port
	GetGWorld(&myOldGW, &myOldGD);
		
	FileUtils_MakeFSSpecForPathName(0, 0L, thePathName, &myFSSpec);

	if (isTextureMovie) {
		// open the movie and store its address in our data structure
		myErr = OpenMovieFile(&myFSSpec, &myMovieFileRef, fsRdPerm);
		if (myErr != noErr)
			goto bail;

		myErr = NewMovieFromFile(&(**myTexture).fMovie, myMovieFileRef, NULL, (StringPtr)NULL, newMovieActive, NULL);
		if (myErr != noErr)
			goto bail;

		if (myMovieFileRef != 0)
			CloseMovieFile(myMovieFileRef);
			
		SetMovieMatrix((**myTexture).fMovie, NULL);
		MacSetRect(&myBounds, 0, 0, 128, 128);
		SetMovieBox((**myTexture).fMovie, &myBounds);

		// get the sound media handler for movies with sound
		if (QTUtils_IsMediaTypeInMovie((**myTexture).fMovie, SoundMediaType) || QTUtils_IsMediaTypeInMovie((**myTexture).fMovie, MusicMediaType))
			(**myTexture).fMediaHandler = QTUtils_GetSoundMediaHandler((**myTexture).fMovie);
		
		// create a new offscreen graphics world (into which we will draw the movie)
		myErr = QTNewGWorld(&myGWorld, kOffscreenPixelType, &myBounds, NULL, NULL, kICMTempThenAppMemory);
		if (myErr != noErr) {
			StopMovie((**myTexture).fMovie);
			DisposeMovie((**myTexture).fMovie);
			(**myTexture).fMovie = NULL;
			goto bail;
		}
	} else {
		// get a graphics importer for the image file
		myErr = GetGraphicsImporterForFile(&myFSSpec, &myImporter);
		if (myErr != noErr)
			goto bail;
		
		// determine the natural size of the image
		myErr = GraphicsImportGetNaturalBounds(myImporter, &myBounds);
		if (myErr != noErr)
			goto bail;
			
		// create a new offscreen graphics world (into which we will draw the image)
		myErr = QTNewGWorld(&myGWorld, kOffscreenPixelType, &myBounds, NULL, NULL, kICMTempThenAppMemory);
		if (myErr != noErr)
			goto bail;
	}

	myErr = noErr;

	myPixMap = GetGWorldPixMap(myGWorld);
	if (myPixMap != NULL) {
		LockPixels(myPixMap);
		myPictMapAddr = (unsigned long)GetPixBaseAddr(myPixMap);
		
		// get the offset, in bytes, from one row of pixels to the next
		myPictRowBytes = (unsigned long)QTGetPixMapHandleRowBytes(myPixMap);
		SetGWorld(myGWorld, NULL);

		// create a storage object associated with the new offscreen graphics world
		myStrgPMapPtr = &(**myTexture).fStoragePixmap;
		myStrgPMapPtr->image = Q3MemoryStorage_NewBuffer((void *)myPictMapAddr,
														myPictRowBytes * myBounds.bottom,
														myPictRowBytes * myBounds.bottom);
		myStrgPMapPtr->width		= myBounds.right;
		myStrgPMapPtr->height		= myBounds.bottom;
		myStrgPMapPtr->rowBytes		= myPictRowBytes;
		myStrgPMapPtr->pixelSize	= (**myPixMap).pixelSize;
		myStrgPMapPtr->pixelType	= ((**myPixMap).pixelSize == 16) ? kQ3PixelTypeRGB16 : kQ3PixelTypeRGB32;
		myStrgPMapPtr->bitOrder		= kQ3EndianBig;
		myStrgPMapPtr->byteOrder	= kQ3EndianBig;
	}
	

	if (isTextureMovie) {
		TimeBase	myTimeBase;
		
		// start playing the movie in a loop
		SetMovieGWorld((**myTexture).fMovie, myGWorld, GetGWorldDevice(myGWorld));
		GoToBeginningOfMovie((**myTexture).fMovie);

		// draw the movie first
		MoviesTask((**myTexture).fMovie, 0);

		// throw the movie into loop mode
		myTimeBase = GetMovieTimeBase((**myTexture).fMovie);
		SetTimeBaseFlags(myTimeBase, GetTimeBaseFlags(myTimeBase) | loopTimeBase);

		// start playing the movie
		VRMoov_StartMovie((**myTexture).fMovie);		
	} else {
		// draw the picture into the offscreen graphics world
		GraphicsImportSetGWorld(myImporter, myGWorld, NULL);
		GraphicsImportSetBoundsRect(myImporter, &myBounds);
		GraphicsImportDraw(myImporter);
	}

	(**myTexture).fpGWorld = myGWorld;

bail:
	SetGWorld(myOldGW, myOldGD);
	HUnlock((Handle)myTexture);

	if (myImporter != NULL)
		CloseComponent(myImporter);

	if (myErr != noErr) {
		if (myTexture != NULL)
			DisposeHandle((Handle)myTexture);
		myTexture = NULL;
	}
		
	return(myTexture);
}


//////////
//
// VR3DTexture_AddToGroup
// Create a new texture shader based on the pixmap storage data and add it to the group.
//
//////////

TQ3Status VR3DTexture_AddToGroup (TextureHdl theTexture, TQ3GroupObject theGroup)
{
	TQ3Status			myStatus = kQ3Failure;
	TQ3StoragePixmap	myStoragePixmap;
	TQ3TextureObject	myTextureObject;
	
	if (theTexture == NULL || theGroup == NULL)
		return(myStatus);
	
	myStoragePixmap = (**theTexture).fStoragePixmap;
	myTextureObject = Q3PixmapTexture_New(&myStoragePixmap);
	if (myTextureObject != NULL) {

		TQ3ShaderObject		myTextureShader;
		
		myTextureShader = Q3TextureShader_New(myTextureObject);
		Q3Object_Dispose(myTextureObject);
		
		if (myTextureShader != NULL) {
			TQ3GroupPosition	myPosition = NULL;
			
			// look for an existing texture shader in the group
			Q3Group_GetFirstPositionOfType(theGroup, kQ3SurfaceShaderTypeTexture, &myPosition);
			if (myPosition != NULL) {
			
				// there is an existing texture shader; just replace it
				Q3Group_SetPositionObject(theGroup, myPosition, myTextureShader);
			} else {
			
				// there is no existing texture shader; add one to the group
				Q3Group_GetFirstPosition(theGroup, &myPosition);
				Q3Group_AddObjectBefore(theGroup, myPosition, myTextureShader);
			}
			
			Q3Object_Dispose(myTextureShader);
			myStatus = kQ3Success;
		}
	}

	return(myStatus);
}


//////////
//
// VR3DTexture_Delete
// Deallocate the texture.
//
//////////

Boolean	VR3DTexture_Delete (TextureHdl theTexture)
{
	PixMapHandle		myPixMap;

	if (theTexture == NULL)
		return(false);

	HLock((Handle)theTexture);

	if ((**theTexture).fMovie != NULL) {
		StopMovie((**theTexture).fMovie);
		DisposeMovie((**theTexture).fMovie);
		(**theTexture).fMovie = NULL;
	}

	if ((**theTexture).fpGWorld == NULL) {
		myPixMap = GetGWorldPixMap((**theTexture).fpGWorld);
		if (myPixMap != NULL)
			UnlockPixels(myPixMap);
	
		DisposeGWorld((**theTexture).fpGWorld);
		(**theTexture).fpGWorld = NULL;
	}

	if ((**theTexture).fStoragePixmap.image != NULL) {
		Q3Object_Dispose((**theTexture).fStoragePixmap.image);
		(**theTexture).fStoragePixmap.image = NULL;
	}

	HUnlock((Handle)theTexture);
	DisposeHandle((Handle)theTexture);
	
	return(true);
}


//////////
//
// VR3DTexture_NextFrame
// Advance the texture's movie to the next frame.
//
//////////

Boolean VR3DTexture_NextFrame (TextureHdl theTexture)
{
	TQ3StoragePixmap	*myStrgPMapPtr;
	long				mySize;
	TQ3Status			myStatus;
	unsigned long		myPictMapAddr;
	PixMapHandle		myPixMap;

	// if fpGWorld is non-NULL, then the fMovie is non-NULL and the movie needs updating
	if ((**theTexture).fpGWorld == NULL)
		return(false);

	HLock((Handle)theTexture);

	if ((**theTexture).fMovie != NULL)
		MoviesTask((**theTexture).fMovie, 0);	// draw the next movie frame

	myPixMap = GetGWorldPixMap((**theTexture).fpGWorld);
	myStrgPMapPtr = &(**theTexture).fStoragePixmap;
	mySize = myStrgPMapPtr->height * myStrgPMapPtr->rowBytes;
	myPictMapAddr = (unsigned long)GetPixBaseAddr(myPixMap);

	// tell QD3D the buffer changed
	myStatus = Q3MemoryStorage_SetBuffer(myStrgPMapPtr->image, (void *)myPictMapAddr, mySize, mySize);

	HUnlock((Handle)theTexture);
	
	return(myStatus == kQ3Success);
}

#endif	// if QD3D_AVAIL
