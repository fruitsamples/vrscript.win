//////////
//
//	File:		VR3DObjects.h
//
//	Contains:	QuickDraw 3D support for QuickTime VR movies.
//
//	Written by:	Tim Monroe
//
//	Copyright:	� 1996 by Apple Computer, Inc., all rights reserved.
//
//	Change History (most recent first):
//
//	   <1>	 	12/05/96	rtm		first file
//	   
//////////

#pragma once

#if QD3D_AVAIL

//////////
//	   
// header files
//	   
//////////

#include <QD3D.h>
#include <QD3DAcceleration.h>
#include <QD3DGroup.h>
#include <QD3DStorage.h>
#include <QD3DErrors.h>
#include <QD3DView.h>
#include <QD3DMath.h>
#include <QD3DGeometry.h>
#include <QD3DShader.h>
#include <QD3DRenderer.h>
#include <QD3DLight.h>
#include <QD3DDrawContext.h>
#include <QD3DCamera.h>
#include <QD3DTransform.h>
#include <QD3DStorage.h>
#include <QD3DIO.h>
#include <QD3DSet.h>
#include <QDOffscreen.h>

#ifndef __QTVRUtilities__
#include "QTVRUtilities.h"
#endif

#include "ComApplication.h"

#if TARGET_OS_MAC
#include "MacFramework.h"
#endif

#if TARGET_OS_WIN32
#include "WinFramework.h"
#endif

#include "VRScript.h"


//////////
//	   
// constants
//	   
//////////

#define	kDefaultInterpolation	kQ3InterpolationStyleVertex		// default interpolation style
#define	kDefaultBackfacing		kQ3BackfacingStyleBoth			// default backfacing style
#define	kDefaultFill			kQ3FillStyleFilled				// default fill style
#define	kDefaultRotateRadians	(float)0.1						// default rotation factor
#define	kDefaultScale			(float)1.0						// default scale factor
#define	kDefaultChromaKeyState	true							// default chroma key state for QD3D textures

#define	k3DObjectDistance		6.0								// this controls the point-of-interest
#define	kPicFileHeaderSize		512								// the number of bytes in PICT file header


//////////
//	   
// function prototypes
//	   
//////////

void						VR3DObjects_Init (void);
void						VR3DObjects_Stop (void);
Boolean						VR3DObjects_IsQD3DAvailable (void);
void						VR3DObjects_InitWindowData (WindowObject theWindowObject);
void						VR3DObjects_DumpWindowData (WindowObject theWindowObject);
Boolean						VR3DObjects_DoIdle (WindowObject theWindowObject);
void						VR3DObjects_EnlistBox (WindowObject theWindowObject, UInt32 theEntryID, float theX, float theY, float theZ, float theXSize, float theYSize, float theZSize, UInt32 theOptions);
void						VR3DObjects_EnlistCone (WindowObject theWindowObject, UInt32 theEntryID, float theX, float theY, float theZ, float theMajRad, float theMinRad, float theHeight, UInt32 theOptions);
void						VR3DObjects_EnlistCylinder (WindowObject theWindowObject, UInt32 theEntryID, float theX, float theY, float theZ, float theMajRad, float theMinRad, float theHeight, UInt32 theOptions);
void						VR3DObjects_EnlistEllipsoid (WindowObject theWindowObject, UInt32 theEntryID, float theX, float theY, float theZ, float theMajRad, float theMinRad, float theHeight, UInt32 theOptions);
void						VR3DObjects_EnlistTorus (WindowObject theWindowObject, UInt32 theEntryID, float theX, float theY, float theZ, float theMajRad, float theMinRad, float theHeight, float theRatio, UInt32 theOptions);
void						VR3DObjects_EnlistRectangle (WindowObject theWindowObject, UInt32 theEntryID, float theX, float theY, float theZ,
															float theX1, float theY1, float theZ1, 
															float theX2, float theY2, float theZ2, 
															float theX3, float theY3, float theZ3, 
															float theX4, float theY4, float theZ4, UInt32 theOptions);
void						VR3DObjects_Enlist3DMFFile (WindowObject theWindowObject, UInt32 theEntryID, float theX, float theY, float theZ, UInt32 theOptions, char *thePathName);
TQ3GroupObject				VR3DObjects_CreateDefaultGroup (TQ3GeometryObject theObject);
TQ3ViewObject				VR3DObjects_CreateView (GWorldPtr theGWorld);
TQ3GroupObject				VR3DObjects_CreateLights (void);
TQ3DrawContextObject		VR3DObjects_CreateDrawContext (GWorldPtr theGWorld);
TQ3CameraObject				VR3DObjects_CreateCamera (CGrafPtr thePort);
TQ3AttributeSet				VR3DObjects_GetModelAttributeSet (VRScript3DObjPtr thePointer);
void						VR3DObjects_SetSubdivisionStyle (TQ3GroupObject theGroup, short theNumDivisions);
void						VR3DObjects_SetColor (WindowObject theWindowObject, UInt32 theEntryID, float theRed, float theGreen, float theBlue, UInt32 theOptions);
void						VR3DObjects_SetTransparency (WindowObject theWindowObject, UInt32 theEntryID, float theRed, float theGreen, float theBlue, UInt32 theOptions);
void						VR3DObjects_SetInterpolation (WindowObject theWindowObject, UInt32 theEntryID, UInt32 theStyle, UInt32 theOptions);
void						VR3DObjects_SetBackfacing (WindowObject theWindowObject, UInt32 theEntryID, UInt32 theStyle, UInt32 theOptions);
void						VR3DObjects_SetFill (WindowObject theWindowObject, UInt32 theEntryID, UInt32 theStyle, UInt32 theOptions);
void						VR3DObjects_SetLocation (WindowObject theWindowObject, UInt32 theEntryID, float theX, float theY, float theZ, UInt32 theOptions);
void						VR3DObjects_SetRotation (WindowObject theWindowObject, UInt32 theEntryID, float theX, float theY, float theZ, UInt32 theOptions);
void						VR3DObjects_SetRotationState (WindowObject theWindowObject, UInt32 theEntryID, Boolean theState, UInt32 theOptions);
void						VR3DObjects_SetVisibleState (WindowObject theWindowObject, UInt32 theEntryID, Boolean theState, UInt32 theOptions);
void						VR3DObjects_SetTexture (WindowObject theWindowObject, UInt32 theEntryID, Boolean isMovie, UInt32 theOptions, char *thePathName);
void						VR3DObjects_AnimateModel (VRScript3DObjPtr thePointer);
TQ3Status					VR3DObjects_DrawModel (WindowObject theWindowObject);
TQ3Status					VR3DObjects_SubmitModel (VRScript3DObjPtr thePointer, TQ3ViewObject theView);
TQ3GroupObject				VR3DObjects_GetModelFromFile (char *thePathName);
void						VR3DObjects_PrescreenRoutine (QTVRInstance theInstance, WindowObject theWindowObject);
void						VR3DObjects_SetCamera (WindowObject theWindowObject);
void						VR3DObjects_SetCameraAspectRatio (WindowObject theWindowObject);
void						VR3DObjects_UpdateDrawContext (WindowObject theWindowObject);
void						VR3DObjects_DumpEntryMem (VRScript3DObjPtr theEntry);

#endif	// if QD3D_AVAIL
