// ============================
//		CWindowMenu.h
//		by Drew Thaler
//		athaler@umich.edu
// ============================
//
// See the accompanying .cp file for information about this class.

#pragma once


#include <LAttachment.h>
#include <LArray.h>
#include <LPane.h>
#include <LWindow.h>


#pragma mark -- CWindowMenu --
// -----------------------------------
//	CWindowMenu
// -----------------------------------

class CWindowMenu : public LAttachment
{
public:
	
			CWindowMenu( ResIDT theMenu );
	virtual	~CWindowMenu();
	
	virtual void 		ExecuteSelf( MessageT inMessage, void *ioParam );

	virtual void 		Rebuild();
	virtual void 		SelectIndexedWindow( SInt16 index );

	virtual void		Stack();
	virtual void		Tile();
	virtual void		TileVertical();
	virtual void		Zoom();
	
	
	virtual void		StackWindowsForScreen( Rect &inScreenRect, LArray &windows );

	virtual void		TileWindowsForScreen( Rect &inScreenRect, LArray &windows,
										SDimension16 tileDimensions );
										
	virtual void		CalculateTileDimensions( SInt16 numberOfWindows,
													SDimension16 &outSize );
	
	
	virtual void 		Exclude( LWindow *window );
	virtual void		DontExclude( LWindow *window );
	

	virtual void		BuildScreenList( LArray &outList );
	virtual void		BuildFrontToBackWindowList( LArray &outList );
	virtual void		SubtractExcludedWindows( LArray &ioList );
	virtual GDHandle	ScreenToStackOn( LWindow *window );
	virtual void		ExtractWindowsForScreen( LArray &inList, LArray &outList, GDHandle gdh );
	
	virtual void		GetWindowSizes( LWindow *window, Point topLeft, Rect &inScreenRect,
											SDimension16 &maxSize,
											SDimension16 &stdSize,
											SDimension16 &minSize );
	virtual Rect		GetWindowBorder( LWindow *window );
	virtual void		MoveAndSizeWindow( LWindow *window, Point where,
											SInt16 width, SInt16 height );
	
protected:
	
	ResIDT		mMenuID;
	MenuHandle	mMenuHandle;
	SInt16		mItemCountWhenEmpty;
	
	LArray		mExclusionList;
	LArray		mKnownWindowList;
	
	static Boolean	sInitialized;
};


#pragma mark -- Command definitions --
// ------------------------------------------------
//	€	Command definitions
// ------------------------------------------------
//
//	Use these command numbers in your window menu if you
//	want CWindowMenu to handle stack, tile, tile vertical,
//	and zoom menu items.
//

const CommandT	cmd_Stack			= 65100;
const CommandT	cmd_Tile			= 65101;
const CommandT	cmd_TileVertical	= 65102;
const CommandT	cmd_Zoom			= 65103;


