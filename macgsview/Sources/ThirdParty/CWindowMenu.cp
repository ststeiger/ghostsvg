// ==============================
//		CWindowMenu.cp
//  	by Drew Thaler
//		    (athaler@umich.edu)
// ==============================
//
//	This source code is public domain, and may be used freely.  If
//	you use it and like it, or would like to suggest improvements,
//	drop me a note at <athaler@umich.edu>.  Thanks!
//
//	v1.0 - initial release
//


// ---------------------------------------------------------------------
// Description:
//	
//	CWindowMenu is a PowerPlant attachment that implements a window menu
//	with stacking/tiling/zooming capabilities.  It keeps track only of
//	PowerPlant "regular" windows, and ignores modals, floaters, and
//	non-PowerPlant windows.  The stacking/tiling algorithms are
//	easy-to-use, multiple-monitor savvy, and can be easily overridden.
//
// To use:
//
//	Simply install CWindowMenu as an attachment to your application
//	class, passing in the MenuID of your Windows menu.  (You should
//	create one if you don't already have one.)  Example:
//
//		CWindowMenu		*windowMenu = new CWindowMenu(menu_Windows);
//		myApplication.AddAttachment( windowMenu );
//
//	That's it!  It's just that simple.  If you wish to use the
//	stacking/tiling/zooming features of CWindowMenu, add the
//	appropriate commands to the window menu in Constructor:
//
//	   (name)        (cmd from CWindowMenu.h)	(suggested key
//                                                equivalent)
//		Stack			cmd_Stack
//		Tile			cmd_Tile
//		Tile Vertical	cmd_TileVertical
//		Zoom			cmd_Zoom					cmd-/
//
//	The menu should not contain a trailing separator; CWindowMenu
//	will add one as appropriate.
//
// Other features:
//
//	Many applications have one or two global windows that you
//	wouldn't want to have in the window list, or that you want to keep
//	track of separately.  To exclude windows from being counted in
//	CWindowMenu's list, use the Exclude function.  Following from the
//	example above, after creating the windowMenu, we can do the
//	following:
//
//		windowMenu->Exclude( myFindWindow );
//
//	If an Excluded window is disposed before the application quits, you
//	should call DontExclude in its destructor.
//	
// ---------------------------------------------------------------------

#include "CWindowMenu.h"

#include <LArrayIterator.h>
#include <UDesktop.h>
#include <UDrawingUtils.h>
#include <UWindows.h>

#include <Fonts.h>

#include <math.h>

Boolean	CWindowMenu::sInitialized = false;



const Point		stackMinimumOffset = { 8, 8 };
const Point		screenBorderPadding = { 2, 2 };


#define DebugRect(rect) do { Str255 s1,s2;\
		LString::CopyPStr("\p"#rect,s1); LString::AppendPStr(s1,"\p: ");\
			NumToString(rect.top,s2); LString::AppendPStr(s1,s2);\
			LString::AppendPStr(s1,"\p, ");\
			NumToString(rect.left,s2); LString::AppendPStr(s1,s2);\
			LString::AppendPStr(s1,"\p, ");\
			NumToString(rect.bottom,s2); LString::AppendPStr(s1,s2);\
			LString::AppendPStr(s1,"\p, ");\
			NumToString(rect.right,s2); LString::AppendPStr(s1,s2);\
			DebugStr(s1); } while(0)

#define DebugSize(size) do { Str255 s1,s2;\
		LString::CopyPStr("\p"#size,s1); LString::AppendPStr(s1,"\p: ");\
			NumToString(size.width,s2); LString::AppendPStr(s1,s2);\
			LString::AppendPStr(s1,"\p, ");\
			NumToString(size.height,s2); LString::AppendPStr(s1,s2);\
			DebugStr(s1); } while(0)


// -----------------------------------------------------------------
//	€	Constructors and destructors
// -----------------------------------------------------------------

CWindowMenu::CWindowMenu( ResIDT windowMenu )
{
	// we want to look at all events
	mMessage = msg_AnyMessage;
	
	// initialize some other stuff
	mMenuID = windowMenu;
	mMenuHandle = ::GetMenuHandle(windowMenu);
	ThrowIfResFail_( mMenuHandle );
	
	// count menu items
	if (!sInitialized)
	{
		mItemCountWhenEmpty = ::CountMItems(mMenuHandle);
		sInitialized = true;
	}
}



CWindowMenu::~CWindowMenu()
{
}



// -----------------------------------------------------------------
//	€	ExecuteSelf
// -----------------------------------------------------------------
//	Called by the LAttachable for all messages that it handles.

void
CWindowMenu::ExecuteSelf( MessageT inMessage, void *ioParam )
{
	mExecuteHost = true;
	switch (inMessage)
	{
			// decide when to rebuild according to the raw event
			
		case msg_Event:
			EventRecord		*event = (EventRecord*) ioParam;
			switch (event->what)
			{
					// rebuild on activate evts
				case activateEvt:
					Rebuild();
					break;
				
					// rebuild on menubar click
				case mouseDown:
					WindowRef	window;
					if (::FindWindow(event->where,&window) == inMenuBar)
						Rebuild();
					break;
				
					// rebuild on window cmd-key
				case keyDown:
					char	theChar = event->message & charCodeMask;
					if ((theChar >= '1' && theChar <= '9') &&
							(event->modifiers&cmdKey))
						Rebuild();
					break;
			}
			break;
			
				// called by ProcessCommandStatus -- we set these values
		case msg_CommandStatus:
			{
				SCommandStatus	*status = (SCommandStatus*)ioParam;
				switch (status->command)
				{
						// is front window zoomable?
					case cmd_Zoom:
						mExecuteHost = false;
						if ( UDesktop::FrontWindowIsModal() )
							*status->enabled = false;
						else
						{
							LWindow *frontWindow =
								UDesktop::FetchTopRegular();
							if (frontWindow)	// added by Bernd Heller
								*status->enabled = frontWindow->HasAttribute(windAttr_Zoomable);
						}
						break;
					
						// is front window stackable/tileable?  (it has to
						// be one that we know about in order for us to be
						// able to stack it)

					case cmd_Stack:
					case cmd_Tile:
					case cmd_TileVertical:
						mExecuteHost = false;
						{
							LWindow	*frontWindow =
								UDesktop::FetchTopRegular();
							ArrayIndexT	index = LArray::index_Bad;
							
							if (frontWindow)
								index = mKnownWindowList.FetchIndexOf(
											&frontWindow );
							
							*status->enabled =
								(! UDesktop::FrontWindowIsModal()) &&
								(frontWindow != nil) &&
								(index != LArray::index_Bad);
						}
						break;
				}
			}
			break;
			
			// listen to selections from the window menu
		
		case cmd_Zoom:
			mExecuteHost = false;
			Zoom();
			break;
		
		case cmd_Stack:
			mExecuteHost = false;
			Stack();
			break;
		
		case cmd_Tile:
			mExecuteHost = false;
			Tile();
			break;
		
		case cmd_TileVertical:
			mExecuteHost = false;
			TileVertical();
			break;
		
		default:
			{
				ResIDT	synthMenu;
				SInt16	synthItem;
				if ( LCommander::IsSyntheticCommand( inMessage,
						synthMenu,synthItem) )
				{
					if (synthMenu == mMenuID &&
						synthItem > mItemCountWhenEmpty)
					{
						SelectIndexedWindow( synthItem -
							mItemCountWhenEmpty - 1 );

						mExecuteHost = false;
					}
				}
			}
			break;
	}
}










#pragma mark --- Menu handling ---


// -----------------------------------------------------------------
//	€	Rebuild
// -----------------------------------------------------------------
//	Called by ExecuteSelf to rebuild the window menu.

void
CWindowMenu::Rebuild()
{
		// remove old items from the menu
	
	SInt16	items = ::CountMItems( mMenuHandle );
	while ( items > mItemCountWhenEmpty )
		::DeleteMenuItem( mMenuHandle, items-- );
	
		// build a list of the windows that we have
	
	LArray	windowsToAdd;
	BuildFrontToBackWindowList( windowsToAdd );
	SubtractExcludedWindows( windowsToAdd );
	
		// subtract off known windows
	
	{
		LArrayIterator	iter(mKnownWindowList);
		LWindow			*window;
		while ( iter.Next(&window) )
		{
			ArrayIndexT		arrayIndex =
								windowsToAdd.FetchIndexOf(&window);
			if (arrayIndex)
			{
					// ok, it's there and we know it's there.
					// remove it from the add list

				windowsToAdd.RemoveItemsAt( 1, arrayIndex );
			}
			else
			{
					// the window is no longer around; remove
					// it from our known list.

				mKnownWindowList.RemoveItemsAt(1,iter.GetCurrentIndex());
			}
		}
	}
	
		// if we don't have anything to add, stop here
	
	if ( mKnownWindowList.GetCount() == 0 &&
			windowsToAdd.GetCount() == 0)
		return;
		
		// add a separator line
	
	::AppendMenu( mMenuHandle, "\p(-" );
	items++;
	
		// now add all the items to the menu
	
	Str255	title;
	SInt16	index = 0;
		
		// copy added windows into menu
		
	{
		LArrayIterator	iter(windowsToAdd, LArray::index_Last);
		LWindow			*window;
		while ( iter.Previous(&window) )
			mKnownWindowList.InsertItemsAt(
					1, LArray::index_Last, &window );
	}
	
		// and build the list from our known menus
		
	{
		LArrayIterator	iter(mKnownWindowList);
		LWindow			*window;
		while ( iter.Next(&window) )
		{
			window->GetDescriptor(title);
			::AppendMenu( mMenuHandle, "\p." );
			::SetMenuItemText( mMenuHandle, ++items, title );
			
				// add check mark if window is active
			if (window->IsActive())
				::SetItemMark( mMenuHandle, items, checkMark );
			
				// add menu cmds for windows 1-9
		//	if ( ++index < 10 )
		//		::SetItemCmd( mMenuHandle, items, '0' + index );
		}
	}
	
	// and we're done!
}




// -----------------------------------------------------------------
//	€	SelectIndexedWindow
// -----------------------------------------------------------------
//	Called when user selects a window from the menu.

void
CWindowMenu::SelectIndexedWindow( SInt16 index )
{
	LWindow		*window;
	
	if ( mKnownWindowList.FetchItemAt(index,&window) )
	{
		window->Show();
		UDesktop::SelectDeskWindow( window );
	}
}



// -----------------------------------------------------------------
//	€	Stack
// -----------------------------------------------------------------
//	Called directly by ExecuteSelf to stack the windows.  For each
//	screen, this function builds a list of windows to be stacked on
//	that screen (from ScreenToStackOn -> FindDominantDevice) and
//	then calls StackWindowsForScreen.

void
CWindowMenu::Stack()
{
		// get list of active screens
	LArray	screens;
	BuildScreenList( screens );
	
		// get list of windows in front-to-back order
	LArray	windowList;
	BuildFrontToBackWindowList( windowList );
	SubtractExcludedWindows( windowList );
	
		// walk the list of screens
	LArrayIterator	screenIterator(screens);
	GDHandle		gdh;
	while ( screenIterator.Next(&gdh) )
	{
			// build list of windows to be tiled on this screen
		
		LArray			windowsForThisScreen;
		ExtractWindowsForScreen( windowList, windowsForThisScreen, gdh );
		
		if ( windowsForThisScreen.GetCount() == 0 )
			continue;

			// get the screen's rect
		
		Rect	gdRect = (**gdh).gdRect;
		if ( ::GetMainDevice() == gdh )
			gdRect.top += ::GetMBarHeight();
			
			// and stack
			
		StackWindowsForScreen( gdRect, windowsForThisScreen );
	}
}


// -----------------------------------------------------------------
//	€	Tile
// -----------------------------------------------------------------
//	Called directly by ExecuteSelf to tile the windows.  For each
//	screen, this function builds a list of windows to be tiled on that
//	screen (from ScreenToStackOn -> FindDominantDevice), uses
//	CalculateTileDimensions to determine the dimensions of the tile,
//	and then calls TileWindowsForScreen.

void
CWindowMenu::Tile()
{
		// get list of active screens
	LArray	screens;
	BuildScreenList( screens );
	
		// get list of windows in front-to-back order
	LArray	windowList;
	BuildFrontToBackWindowList( windowList );
	SubtractExcludedWindows( windowList );
	
		// walk the list of screens
	LArrayIterator	screenIterator(screens);
	GDHandle		gdh;
	while ( screenIterator.Next(&gdh) )
	{
			// build list of windows to be tiled on this screen
		
		LArray			windowsForThisScreen;
		ExtractWindowsForScreen( windowList, windowsForThisScreen, gdh );
		
		if ( windowsForThisScreen.GetCount() == 0 )
			continue;
		
			// get the screen's rect
		
		Rect	gdRect = (**gdh).gdRect;
		if ( ::GetMainDevice() == gdh )
			gdRect.top += ::GetMBarHeight();
			
			// calculate the tile dimensions
		
		SDimension16	tileDimensions;
		CalculateTileDimensions( windowsForThisScreen.GetCount(),
									tileDimensions );
		
			// and do the tiling
			
		TileWindowsForScreen( gdRect, windowsForThisScreen,
										tileDimensions );
	}
}


// -----------------------------------------------------------------
//	€	TileVertical
// -----------------------------------------------------------------
//	Called directly by ExecuteSelf to tile the windows vertically.  For
//	each screen, this function builds a list of windows to be tiled on
//	that screen (from ScreenToStackOn -> FindDominantDevice), sets the
//	tile dimensions for vertical tiling, and then calls
//	TileWindowsForScreen.

void
CWindowMenu::TileVertical()
{
		// get list of active screens
	LArray	screens;
	BuildScreenList( screens );
	
		// get list of windows in front-to-back order
	LArray	windowList;
	BuildFrontToBackWindowList( windowList );
	SubtractExcludedWindows( windowList );
	
		// walk the list of screens
	LArrayIterator	screenIterator(screens);
	GDHandle		gdh;
	while ( screenIterator.Next(&gdh) )
	{
			// build list of windows to be tiled on this screen
		
		LArray			windowsForThisScreen;
		ExtractWindowsForScreen( windowList, windowsForThisScreen, gdh );
		
		if ( windowsForThisScreen.GetCount() == 0 )
			continue;

			// get the screen's rect
		
		Rect	gdRect = (**gdh).gdRect;
		if ( ::GetMainDevice() == gdh )
			gdRect.top += ::GetMBarHeight();
			
			// tiling vertically is a special case of tiling, with
			//	exactly one window per column.
			
		SDimension16	tileDimensions;
		tileDimensions.width = windowsForThisScreen.GetCount();
		tileDimensions.height = 1;
		
		TileWindowsForScreen( gdRect, windowsForThisScreen,
										tileDimensions );
	}
}


// -----------------------------------------------------------------
//	€	Zoom
// -----------------------------------------------------------------
//	Called by ExecuteSelf to zoom the front window.

void
CWindowMenu::Zoom()
{
	if ( !UDesktop::FrontWindowIsModal() )
	{
		LWindow	*frontWindow = UDesktop::FetchTopRegular();
		if (frontWindow->HasAttribute(windAttr_Zoomable))
			frontWindow->SendAESetZoom();
	}
}











#pragma mark --- Stacking/Tiling algorithms ---


// -----------------------------------------------------------------
//	€	StackWindowsForScreen
// -----------------------------------------------------------------
//	Input is a screen area to stack windows into, and a front-to-back
//	list of windows to be stacked in this area.  The screen area will
//	already have the menu bar clipped out if necessary.
//
//	Stacking consists of moving/resizing only, with no layer changes. 
//	The windows end up arranged in (loosely) the following fashion,
//	each at standard size (or smaller, as necessary):
//
//      +-----------
//      | +----------
//      | | +---------
//      | | |        ...
//
//	Windows should not be selected/deselected from this procedure, nor
//	should the front-to-back ordering change.  

void
CWindowMenu::StackWindowsForScreen( Rect &inScreenRect,
										LArray &windowList )
{
		// calculate top-left stacking position
	Point	resetPosition;
	resetPosition.h = inScreenRect.left + screenBorderPadding.h;
	resetPosition.v = inScreenRect.top + screenBorderPadding.v;
	
	Point	position = resetPosition;

		// walk the list of windows -- we do it BACK-TO-FRONT because
		//	we proceed from the upper left to the lower right, and the
		//	hindmost window belongs in the upper left.
		
	LArrayIterator	iter( windowList, LArray::index_Last );
	LWindow			*window;
	while ( iter.Previous(&window) )
	{
			// figure out best size
		SDimension16	maxSize, stdSize, minSize;
		GetWindowSizes( window, position, inScreenRect,
							maxSize, stdSize, minSize );
		
		if ( (maxSize.width < minSize.width) ||
			 (maxSize.height < minSize.height) ||
			 (maxSize.width < (stdSize.width/2)) ||
			 (maxSize.height < (stdSize.height/2)) )
		{
			position = resetPosition;
			GetWindowSizes( window, position, inScreenRect,
								maxSize, stdSize, minSize );
		}
		
			// reposition the window
		MoveAndSizeWindow( window, position,
						maxSize.width, maxSize.height );
		
			// and increment our position point by either the minimum
			// offset or by the width of the window border/title bar --
			//	whichever is greater.

		Rect	border = GetWindowBorder(window);
		
		if (border.left < stackMinimumOffset.h)
			border.left = stackMinimumOffset.h;
		
		if (border.top < stackMinimumOffset.v)
			border.top = stackMinimumOffset.v;
			
		position.h += border.left;
		position.v += border.top;
	}
}




// -----------------------------------------------------------------
//	€	TileWindowsForScreen
// -----------------------------------------------------------------
//	Input is a screen area to stack windows into, a front-to-back
//	list of windows to be tiled in this area, and the dimensions of
//	the tile.  The screen area will already have the menu bar clipped
//	out if necessary.
//
//	Tiling consists of moving/resizing only, with no layer changes.  The
//	windows end up arranged in (loosely) the following fashion, with the
//	actual dimensions of the tiling dependent upon the input
//
//		+----------+ +----------+ +-----------+
//		|          | |          | |           |
//		|    1     | |    2     | |     3     |
//		|          | |          | |           |
//		+----------+ +----------+ +-----------+
//		+----------+ +----------+ +-----------+
//		|          | |          | |           |
//		|    4     | |    5     | |     6     |
//		|          | |          | |           |
//		+----------+ +----------+ +-----------+
//
//	Windows should not be selected/deselected from this procedure, nor
//	should the front-to-back ordering change.  The ordering of the tile
//	is the order of creation, with the oldest window at the top left.

void
CWindowMenu::TileWindowsForScreen( Rect &inScreenRect, LArray &windowList,
									SDimension16 tileDimensions )
{
		// calculate window size for the tile
	
	SDimension16	idealWindowSize;

	idealWindowSize.width = ((inScreenRect.right - inScreenRect.left) -
					 (tileDimensions.width+1) * screenBorderPadding.h)
					/ tileDimensions.width;

	idealWindowSize.height = ((inScreenRect.bottom - inScreenRect.top) -
					  (tileDimensions.height+1) * screenBorderPadding.h)
					/ tileDimensions.height;
	
		// initialize walk data
	
	SPoint16	walk = {0,0};
	
		// Walk the list of windows, OLDEST-TO-NEWEST.  This
		// means that we use mKnownWindowList rather than windowList.
	
	LArrayIterator	iter(mKnownWindowList);
	LWindow			*window;
	while ( iter.Next(&window) )
	{
			// skip it if the window isn't in our list
		
		ArrayIndexT		index = windowList.FetchIndexOf(&window);
		if ( index == LArray::index_Bad )
			continue;
		
		windowList.RemoveItemsAt( 1, index );
		
			// calculate new position of window

		Point	where;
		
		where.h = inScreenRect.left + screenBorderPadding.h +
					walk.h * (screenBorderPadding.h +
					idealWindowSize.width);
		
		where.v = inScreenRect.top + screenBorderPadding.v +
					walk.v * (screenBorderPadding.v +
					idealWindowSize.height);
			
			// calculate new size of window (subtracting off border)
			
		SDimension16	windowSize = idealWindowSize;
		Rect			border = GetWindowBorder(window);
		
		windowSize.width -= (border.left + border.right);
		windowSize.height -= (border.top + border.bottom);
				
			// and do it
			
		MoveAndSizeWindow( window, where,
							windowSize.width, windowSize.height );
		
			// lastly, increment walk ptr
		
		if ( ++walk.h >= tileDimensions.width )
		{
			walk.h = 0;
			walk.v++;
		}
	}
	
}




// -----------------------------------------------------------------
//	€	CalculateTileDimensions
// -----------------------------------------------------------------
//	Given the number of windows to tile, returns the dimensions of
//	the tile that should be generated.  (e.g., for numberOfWindows = 9,
//	outSize.width = 3 and outSize.height = 3)

void
CWindowMenu::CalculateTileDimensions( SInt16 numberOfWindows, SDimension16
&outSize )
{
	outSize.width = sqrtf( numberOfWindows );
	outSize.height = ceil( (float)numberOfWindows /
							(float)outSize.width );
	
	// if there's going to be a lot of overlap, the horizontal dimension
	//	should be dominant, to reduce overlap on title bars.
	
	if ( outSize.height > 3 && outSize.height > outSize.width )
	{
		SInt16	temp = outSize.height;
		outSize.height = outSize.width;
		outSize.width = temp;
	}
}









#pragma mark --- Exclusion ---


// -----------------------------------------------------------------
//	€	Exclude
// -----------------------------------------------------------------
//	Many applications have special-purpose global windows that you don't
//	wish to show up in the window list; this function allows you to
//	exclude those windows from CWindowMenu's list.

void
CWindowMenu::Exclude( LWindow *windowToExclude )
{
	mExclusionList.InsertItemsAt( 1, LArray::index_Last,
			&windowToExclude );
}


// -----------------------------------------------------------------
//	€	DontExclude
// -----------------------------------------------------------------
//	Effectively undoes a previous call to Exclude.  Does nothing if
//	the window has not been excluded.

void
CWindowMenu::DontExclude( LWindow *windowNotToExclude )
{
	ArrayIndexT		index = mExclusionList.FetchIndexOf(
								&windowNotToExclude );
	if (index) mExclusionList.RemoveItemsAt( 1, index );
}

















#pragma mark --- Utility functions ---


// -----------------------------------------------------------------
//	€	BuildScreenList
// -----------------------------------------------------------------
//	Walks the list of GDevices and builds a list of active screens.
//	Used internally.

void
CWindowMenu::BuildScreenList( LArray &outList )
{
		// clear the old contents of outList
	outList.RemoveItemsAt( LArray::index_First, LArray::index_Last );
		
		// walk the Toolbox device list
	GDHandle	gdh = ::GetDeviceList();
	while (gdh)
	{
		if ( UDrawingUtils::IsActiveScreenDevice(gdh) )
			outList.InsertItemsAt( 1, LArray::index_Last, &gdh );
		
		gdh = (GDHandle) (*gdh)->gdNextGD;
	}
}


// -----------------------------------------------------------------
//	€	BuildFrontToBackWindowList
// -----------------------------------------------------------------
//	Walks the list of windows and returns a list containing each of them
//	front-to-back.  Used internally.

void
CWindowMenu::BuildFrontToBackWindowList( LArray &outList )
{
		// clear the old contents of outList
	outList.RemoveItemsAt( LArray::index_First, LArray::index_Last );
		
		// walk the Toolbox windowlist
	LWindow	*window = UDesktop::FetchTopRegular();
	while (window)
	{
		outList.InsertItemsAt( 1, LArray::index_Last, &window );
		
			// notice that this loop only gets PowerPlant windows
		WindowRef	macWindow = window->GetMacPort();
		do
		{
			WindowPeek	peek = (WindowPeek) macWindow;
			macWindow = (WindowRef) peek->nextWindow;
			if (!macWindow) return;
			window = LWindow::FetchWindowObject( macWindow );
			
		} while (window == nil);
	}
}






// -----------------------------------------------------------------
//	€	SubtractExcludedWindows
// -----------------------------------------------------------------
//	Takes the given list of windows and subtracts out any excluded
//	windows. Used internally.

void
CWindowMenu::SubtractExcludedWindows( LArray &ioList )
{
	LArrayIterator	iter(mExclusionList);
	LWindow			*window;
	while ( iter.Next(&window) )
	{
		ArrayIndexT	arrayIndex = ioList.FetchIndexOf(&window);
		if (arrayIndex)
			ioList.RemoveItemsAt( 1, arrayIndex );
	}
}


// -----------------------------------------------------------------
//	€	ScreenToStackOn
// -----------------------------------------------------------------
//	Finds the screen that the window should be stacked/tiled upon.

GDHandle
CWindowMenu::ScreenToStackOn( LWindow *window )
{
	Rect		structRect = UWindows::GetWindowStructureRect(
								window->GetMacPort() );
	GDHandle	gdh = UWindows::FindDominantDevice( structRect );
	
	return (gdh==nil) ? ::GetMainDevice() : gdh;
}




// -----------------------------------------------------------------
//	€	ExtractWindowsForScreen
// -----------------------------------------------------------------
//	Extracts windows whose ScreenToStackOn is the same as gdh, moving
//	them from inList to outList.  Other windows in inList remain
//	unchanged.  Used internally.

void
CWindowMenu::ExtractWindowsForScreen( LArray &inList, LArray &outList,
										GDHandle gdh )
{
	LArrayIterator	windowIterator(inList);
	LWindow			*window;
	while ( windowIterator.Next(&window) )
	{
		if ( ScreenToStackOn(window) == gdh )
		{
			inList.RemoveItemsAt( 1, windowIterator.GetCurrentIndex() ); 
			outList.InsertItemsAt( 1, LArray::index_Last, &window );
		}
	}
}



// -----------------------------------------------------------------
//	€	GetWindowSizes
// -----------------------------------------------------------------
//	Returns the maximum, standard, and minimum sizes for a given
//	(window, point, rect) combo.  Used internally.

void
CWindowMenu::GetWindowSizes( LWindow *window, Point topLeft,
		Rect &screenRect, SDimension16 &maxSize,
		SDimension16 &stdSize, SDimension16 &minSize )
{
	Rect	border = GetWindowBorder( window );
	
		// get minimum size

	Rect	minMaxRect;
	window->GetMinMaxSize( minMaxRect );
	minSize.width = minMaxRect.left;
	minSize.height = minMaxRect.top;
	
		// get standard size -- can't be bigger than the max size!
	
	Rect			stdRect;
	
	window->CalcStandardBoundsForScreen( screenRect, stdRect );
	stdSize.width = stdRect.right - stdRect.left;
	stdSize.height = stdRect.bottom - stdRect.top;
	
	if (stdSize.width > minMaxRect.right)
		stdSize.width = minMaxRect.right;
	
	if (stdSize.height > minMaxRect.bottom)
		stdSize.height = minMaxRect.bottom;
		
		// calculate maximum size for this screen -- we don't
		//	want to grow bigger than the standard size.
		
	maxSize.width = screenRect.right - topLeft.h -
		(border.left + border.right) - screenBorderPadding.h;
	
	maxSize.height = screenRect.bottom - topLeft.v -
		(border.top + border.bottom) - screenBorderPadding.v;
	
	if (maxSize.width > stdSize.width)
		maxSize.width = stdSize.width;
	
	if (maxSize.height > stdSize.height)
		maxSize.height = stdSize.height;
		
}




// -----------------------------------------------------------------
//	€	GetWindowBorder
// -----------------------------------------------------------------
//	Returns, as a rect, the difference between the content and
//	structure rects of a window.  Thus, border.left contains the
//	thickness of the left border of the window, in pixels.

Rect
CWindowMenu::GetWindowBorder( LWindow *window )
{
	Rect	struc =
				UWindows::GetWindowStructureRect(window->GetMacPort());
	Rect	content =
				UWindows::GetWindowContentRect( window->GetMacPort());
	Rect	border;
	
		// calculate frame border
	border.left = content.left - struc.left;
	border.right = struc.right - content.right;
	border.top = content.top - struc.top;
	border.bottom = struc.bottom - content.bottom;
	
	return border;
}


// -----------------------------------------------------------------
//	€	MoveAndSizeWindow
// -----------------------------------------------------------------
//	Moves and resizes the window, given a new location and desired
//	height and width.  NOTE: the point is the top left of the actual
//	window *frame*, -not- the window content, while the width and height
//	refer to the content width and height.

void
CWindowMenu::MoveAndSizeWindow( LWindow *window, Point where,
								SInt16 width, SInt16 height )
{
		// pin width/height to values within the min/max range, treating
		// max as unsigned just in case one of the max values is -1...
		
	Rect	minMaxRect;
	window->GetMinMaxSize(minMaxRect);

	if (width < minMaxRect.left)
		width = minMaxRect.left;
	else if (width > (UInt16)minMaxRect.right)
		width = (UInt16)minMaxRect.right;
	
	if (height < minMaxRect.top)
		height = minMaxRect.top;
	else if (height > (UInt16)minMaxRect.bottom)
		height = (UInt16)minMaxRect.bottom;
	
		// calculate new frame location
	
	Rect	border = GetWindowBorder(window);
		
	Rect	newFrame;
	newFrame.top = where.v + border.top;
	newFrame.left = where.h + border.left;
	newFrame.bottom = newFrame.top + height;
	newFrame.right = newFrame.left + width;
	
		// and do it
	
	window->DoSetBounds( newFrame );
}



