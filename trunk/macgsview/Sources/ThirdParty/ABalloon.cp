#include <Balloons.h>
#include <Windows.h>

#include <LCheckBox.h>
#include <LRadioButton.h>
#include <LCheckBoxGroupBox.h>
#include <LPeriodical.h>
#include <LEventDispatcher.h>
#include <LWindow.h>
#include <TArrayIterator.h>
#include <LStream.h>

#include "ABalloon.h"

/*	---------------------------------------------------------------------------------------------
	ABalloon		PowerPlant attachment class for balloon help
	
	Author: James W. Walker <mailto:jwwalker@kagi.com>
	
	Creation date: 2/26/98
	
	Last Modified: 10/25/98
	
	Purpose:	To assign help balloons to panes in a PowerPlant
				window.  You can use different messages depending
				on whether the item is on or off, enabled or disabled.
				
	Usage:		Attach an ABalloon to each pane (usually a control)
				for which you want to provide balloon help.
				Specify a STR# ID and a string index for each state.
				And remember to register ABalloon at startup.
				You must compile with RTTI for proper function.
				If you want a balloon to pop up automatically when
				the mouse is motionless over a pane for a certain time,
				you must call EnableAutoPop.
	
	Overriding:	The most likely methods to override are ComputeTip
				(to provide more appropriate placement of the
				balloon tip) and ComputeStringIndex (if you have
				more than the 4 states I provided for.)
			
	Restrictions:	ABalloon is �1998 by James W. Walker.
				Permission is granted for use of ABalloon
				free of charge, other than acknowledgement of
				James W. Walker in any program using ABalloon
				(perhaps in an About box or in accompanying
				documentation). 
	
	Change history:
		1.0  2/28/98		First release.
		1.1  4/11/98
				� Don�t show balloons for invisible panes
				  (Thanks, Daniel Chiaramello, for the correction)
				� Rolled ABalloonHelper::CreateABalloonHelper into
				  ABalloonHelper::AddABalloon.
				� sBalloonAttachments is now a TArray<ABalloon*>
				  instead of TArray<LAttachment*>.
				� In the HMShowBalloon call, use kHMRegularWindow
				  instead of kHMSaveBitsNoWindow, because IM says
				  that the former should normally be used.
		1.2  6/7/98...6/28/98
				� New feature of automatically showing a balloon when
				  the cursor is motionless over a pane for a certain
				  length of time.  (Thanks to Fabrice for getting me
				  started on this and helping to implement it.)
				� New subclass that automatically shows a balloon even
				  if you have not globally enabled automatic balloons.
				� Added pragmas to get rid of annoying TArray warnings.
				� Made the TArray an ordinary member rather than a
				  static member of ABalloonHelper.
				� Changed the balloon mode to kHMSaveBitsWindow to fix
				  an update problem when a balloon was removed by the
				  appearance of a modal dialog.
		1.3  10/3/98, 10/25/98
				� New function CalcExposedPortFrame used in FindBalloonPane.
				  This is so that a pane inside a scrolling view will
				  show its balloon even if it is only partially visible.
				  Thanks to Rainer Brockerhoff for reporting this problem.
				� Implemented an option to make balloons appear immediately
				  when the control key is down.  This was contributed by
				  Bill Modesitt.
		1.4  11/27/98
				� Added the helper class ABalloonTip, which controls the
				  location of the balloon tip and the variant of the
				  balloon window.
				� Added another special case to provide good tip
				  placement for LCheckBoxGroupBox.
	---------------------------------------------------------------------------------------------
*/
LAttachable*	ABalloon::sLastBalloonedItem = nil;
SInt16			ABalloon::sLastIndex = 0;
ResIDT			ABalloon::sLastStringList = 0;

#if !__option(RTTI)
	#error you must compile with RTTI for this to work right
#endif

#pragma mark ====== ABalloonHelper declaration

#pragma warn_hidevirtual off
template class TArray<ABalloon*>;
#pragma warn_hidevirtual reset

/*	---------------------------------------------------------------------------------------------
	class ABalloonHelper
	
	This periodical is in charge of sending DoBalloon messages to
	appropriate ABalloon attachments.
	
	This is a singleton, created the first time an ABalloon is created.
	---------------------------------------------------------------------------------------------
*/
class ABalloonHelper  : public LPeriodical
{
	friend class ABalloon;
	
public:
	static void			AddABalloon( ABalloon* inABalloon );
	static void			RemoveABalloon( ABalloon* inABalloon );
	
protected:
	
						ABalloonHelper();
					
						~ABalloonHelper();
					
	virtual	void		SpendTime(
							const EventRecord		&inMacEvent);

	virtual ABalloon*	FindBalloonPane( const Point& inGlobalPoint );
	
	static 	ABalloonHelper*	sTheHelper;		// the single object of this class
	
	TArray<ABalloon*>	mBalloonAttachments;	// all ABalloon instances
	
	static	bool	sAutoPop;
	static	UInt32	sAutoPopTicks;
	static	bool	sControlKeyPop;
};

// Define and initialize static variables
ABalloonHelper*			ABalloonHelper::sTheHelper = nil;
bool					ABalloonHelper::sAutoPop = false;
bool					ABalloonHelper::sControlKeyPop = false;
UInt32					ABalloonHelper::sAutoPopTicks = 60;

#pragma mark ====== ABalloonHelper code

void	ABalloonHelper::AddABalloon( ABalloon* inABalloon )
{
	if (sTheHelper == nil)
	{
		sTheHelper = new ABalloonHelper;
	}
	sTheHelper->mBalloonAttachments.InsertItemsAt( 1,
		LArray::index_Last, inABalloon );
}

void	ABalloonHelper::RemoveABalloon( ABalloon* inABalloon )
{
	sTheHelper->mBalloonAttachments.Remove( inABalloon );
	if (sTheHelper->mBalloonAttachments.GetCount() == 0)
	{
		delete sTheHelper;
	}
}

ABalloonHelper::ABalloonHelper()
{
	StartRepeating();
}

ABalloonHelper::~ABalloonHelper()
{
	sTheHelper = nil;
}

/*	---------------------------------------------------------------------------------------------
	ABalloonHelper::SpendTime
	
	This function determines which ABalloon attachment, if any, should
	show its balloon.
	---------------------------------------------------------------------------------------------
*/
void	ABalloonHelper::SpendTime( const EventRecord	&inMacEvent )
{
	static UInt32	sPrevWhen = 0;
	static Point	sPrevWhere;
	static bool		sControlKeyDown = false;
	
	ABalloon* theABalloon = FindBalloonPane( inMacEvent.where );
	
	if (sControlKeyPop)
	{
		// I would have thought that looking at inMacEvent.modifiers
		// would suffice to detect the control key, but it seems to
		// be wrong for mouse-moved events.
		KeyMap	theKeyMap;
		::GetKeys( theKeyMap );
		
		if ( ((theKeyMap[1] & 0x0008) != 0) && (theABalloon != nil) )
		{
			if ( !sControlKeyDown )	// control key was not previously down
			{
				if ( !::HMGetBalloons() )
				{
					::HMSetBalloons(true);
				}
				sControlKeyDown = true;
			}
		}
		else						// control key not down
		{
			if (sControlKeyDown)		// control key was previously down
			{
				::HMSetBalloons(false);
				sControlKeyDown = false;
			}
		}
	}
	
	if (theABalloon)
	{
		if (::HMGetBalloons())	// balloons turned on manually?
		{
			theABalloon->DoBalloon();
		}
		else // balloons aren't on, but we may need to show one automatically
		{
			if ( sAutoPop || theABalloon->GetAutomatic() )
			{
				// If the mouse has moved, reset the timer.
				if ( (sPrevWhere.h != inMacEvent.where.h) ||
				 	 (sPrevWhere.v != inMacEvent.where.v) ||
				 	 (inMacEvent.what != nullEvent)
				 )
				{
					sPrevWhen = inMacEvent.when;
					sPrevWhere = inMacEvent.where;
				}
				
				if ( inMacEvent.when >= sPrevWhen + sAutoPopTicks )
				{
					// Show a balloon, and keep showing it until
					// something happens.
					::HMSetBalloons(true);
					StopRepeating();	// prevent recursion
					theABalloon->DoBalloon();
					EventRecord theEvent;
					do
					{
						// Check for any events, but only dispatch null
						// events.  Note that this does give background
						// applications a little time.
						if (!::EventAvail( everyEvent, &theEvent ))
						{
							LEventDispatcher::GetCurrentEventDispatcher()
								->DispatchEvent(theEvent);
						}
					}
					while (	(theEvent.what == nullEvent) &&
							(sPrevWhere.h == theEvent.where.h) &&
			 	 			(sPrevWhere.v == theEvent.where.v) );
					sPrevWhere.v = 0x7FFF; //it will be a total reset
					::HMSetBalloons(false);
					StartRepeating();
				}
			}
		}
	}
	
}

/*	---------------------------------------------------------------------------------------------
	ABalloonHelper::FindBalloonPane
	
	Look for a visible pane in an active window that contains the given
	point in global coordinates and has an attached ABalloon.
	---------------------------------------------------------------------------------------------
*/
ABalloon*	ABalloonHelper::FindBalloonPane( const Point& inGlobalPoint )
{
	ABalloon*	resultABalloon = nil;
	
	WindowPtr	macWindowP;
	::FindWindow( inGlobalPoint, &macWindowP );	// is the point over a window?
	
	if (macWindowP != nil)
	{
		LWindow	*theWindow = LWindow::FetchWindowObject(macWindowP);
		if ( (theWindow != nil) &&
			theWindow->IsActive() &&
			theWindow->IsEnabled() )
		{
			Point	portMouse = inGlobalPoint;
			theWindow->GlobalToPortPoint(portMouse);
			
			// We have an array of all active ABalloons. Of those
			// attached to subpanes of this window, we want to find
			// the deepest one containing the point.
			Rect		theBestFrameSoFar;
			TArrayIterator<ABalloon*>	iterator(mBalloonAttachments);
			ABalloon*	theAttachment;
			
			while (iterator.Next(theAttachment))
			{
				LPane* ownerPane = dynamic_cast<LPane*>(
					theAttachment->GetOwnerHost() );
				Rect	ownerFrame;
				if ( (ownerPane != nil) &&
					ownerPane->IsVisible() &&		// 4/10/98
					(ownerPane->GetMacPort() == macWindowP) &&
					ABalloon::CalcExposedPortFrame(ownerPane, ownerFrame) &&
					::PtInRect( portMouse, &ownerFrame ) )
				{
					// The current frame is completely enclosed in
					// theBestFrameSoFar just in case their union
					// equals theBestFrameSoFar.
					Rect	theUnion;
					::UnionRect( &theBestFrameSoFar, &ownerFrame, &theUnion );
					
					if ( (resultABalloon == nil) ||
						::EqualRect( &theBestFrameSoFar, &theUnion )
					)
					{
						resultABalloon = theAttachment;
						theBestFrameSoFar = ownerFrame;
					}
				}
			} // end of loop over ABalloons
		}
	}
	
	return resultABalloon;
}


#pragma mark ====== ABalloon code

// ABalloon::ABalloon	stream constructor.
ABalloon::ABalloon(
		LStream			*inStream)
	: LAttachment( inStream ), mAutomatic(false)
{
	// I don't want to have to bother filling this out in
	// Constructor every time.
	mMessage = msg_Nothing;
	
	*inStream >> mStringListID;
	*inStream >> mOffDisabledIndex;
	*inStream >> mOffEnabledIndex;
	*inStream >> mOnDisabledIndex;
	*inStream >> mOnEnabledIndex;
	
	if ( (nil != dynamic_cast<LRadioButton*>(mOwnerHost)) ||
		(nil != dynamic_cast<LCheckBox*>(mOwnerHost))
	)
	{
		mTipSetter = new ABalloonTipForCheckBox;
	}
	else if ( nil != dynamic_cast<LCheckBoxGroupBox*>(mOwnerHost) )
	{
		mTipSetter = new ABalloonTipForCheckBoxGroup;
	}
	else
	{
		mTipSetter = new ABalloonTip;
	}
	
	ABalloonHelper::AddABalloon( this );
}

// ABalloon::~ABalloon	destructor.
ABalloon::~ABalloon()
{
	ABalloonHelper::RemoveABalloon( this );
	delete mTipSetter;
}

/*	---------------------------------------------------------------------------------------------
	ABalloon::ShowBalloon
	
	Show a balloon, given a string index and pane frame.
	We must be sure not to display the same balloon that is already
	showing, to avoid flicker.
	---------------------------------------------------------------------------------------------
*/
void	ABalloon::ShowBalloon( SInt16 inStringIndex, Rect& inAltRect )
{
	if (::HMIsBalloon() && (sLastBalloonedItem == mOwnerHost) &&
		(sLastIndex == inStringIndex) && (sLastStringList == mStringListID) )
	{
		// already showing it, nothing to do
	}
	else
	{
		HMMessageRecord		theMessageRec;
		
		theMessageRec.hmmHelpType = khmmString;
		::GetIndString( theMessageRec.u.hmmString, mStringListID,
			inStringIndex );
		
		sLastBalloonedItem = mOwnerHost;
		sLastIndex = inStringIndex;
		sLastStringList = mStringListID;
		
		::HMShowBalloon( &theMessageRec,
			mTipSetter->ComputeTip( inAltRect ),
			&inAltRect, nil, 0,
			mTipSetter->GetVariant(),
			kHMSaveBitsWindow );
	}
}


/*	---------------------------------------------------------------------------------------------
	ABalloon::ComputeTip
	
	Compute the location for the balloon's tip, given the pane's frame.
	---------------------------------------------------------------------------------------------
*/
Point	ABalloon::ComputeTip( const Rect& inAltRect )
{
	Point	theTip = mTipSetter->ComputeTip( inAltRect );
	return theTip;
}


/*	---------------------------------------------------------------------------------------------
	ABalloon::DoBalloon
	
	Compute the pane's local frame and call ShowBalloon.
	---------------------------------------------------------------------------------------------
*/
void	ABalloon::DoBalloon()
{
	LPane*	thePane = dynamic_cast<LPane*>( mOwnerHost );
	if (thePane != nil)
	{
		Rect	theFrame;
		
		thePane->CalcPortFrameRect( theFrame );
		thePane->PortToGlobalPoint( topLeft(theFrame) );
		thePane->PortToGlobalPoint( botRight(theFrame) );
		
		ShowBalloon( ComputeStringIndex(thePane), theFrame );
	}
}

/*	---------------------------------------------------------------------------------------------
	ABalloon::ComputeStringIndex
	
	Examine the state of the pane and return the appropriate string index.
	---------------------------------------------------------------------------------------------
*/
SInt16	ABalloon::ComputeStringIndex( LPane* inPane )
{
	SInt16	whichString;
	if (inPane->GetValue() == 0)
	{
		if (inPane->IsEnabled())
		{
			whichString = mOffEnabledIndex;
		}
		else
		{
			whichString = mOffDisabledIndex;
		}
	}
	else
	{
		if (inPane->IsEnabled())
		{
			whichString = mOnEnabledIndex;
		}
		else
		{
			whichString = mOnDisabledIndex;
		}
	}
	return whichString;
}

/*	---------------------------------------------------------------------------------------------
	ABalloon::CalcExposedPortFrame				[static]
	
	Compute the part of the pane's frame that is actually visible,
	in port coordinates.  Return true if this rectangle is nonempty.
	---------------------------------------------------------------------------------------------
*/
bool	ABalloon::CalcExposedPortFrame( LPane* inPane, Rect& outFrame )
{
	bool	isExposed = inPane->CalcPortFrameRect( outFrame );
	if (isExposed)
	{
		LView*	super = inPane->GetSuperView();
		if (super != nil)
		{
			Rect	revealedRect;
			super->GetRevealedRect( revealedRect );
			isExposed = ::SectRect( &revealedRect, &outFrame, &outFrame );
		}
	}
	return isExposed;
}

/*	---------------------------------------------------------------------------------------------
	ABalloon::EnableAutoPop				[static]
	---------------------------------------------------------------------------------------------
*/
void	ABalloon::EnableAutoPop( void )
{
	ABalloonHelper::sAutoPop = true;
}

/*	---------------------------------------------------------------------------------------------
	ABalloon::DisableAutoPop			[static]
	---------------------------------------------------------------------------------------------
*/
void	ABalloon::DisableAutoPop( void )
{
	ABalloonHelper::sAutoPop = false;
}

/*	---------------------------------------------------------------------------------------------
	ABalloon::SetAutoPopDelay			[static]
	---------------------------------------------------------------------------------------------
*/
void	ABalloon::SetAutoPopDelay( UInt32 inDelayTicks )
{
	ABalloonHelper::sAutoPopTicks = inDelayTicks;
}

/*	---------------------------------------------------------------------------------------------
	ABalloon::EnableControlKeyPop			[static]
	---------------------------------------------------------------------------------------------
*/
void	ABalloon::EnableControlKeyPop( void )
{
	ABalloonHelper::sControlKeyPop = true;
}

/*	---------------------------------------------------------------------------------------------
	ABalloon::DisableControlKeyPop			[static]
	---------------------------------------------------------------------------------------------
*/
void	ABalloon::DisableControlKeyPop( void )
{
	ABalloonHelper::sControlKeyPop = false;
}

#pragma mark ====== ABalloonTip code

ABalloonTip::ABalloonTip()
	: mVariant( 0 )
{
}

ABalloonTip::ABalloonTip( short inVariant )
	: mVariant( inVariant )
{
}

short	ABalloonTip::GetVariant() const
{
	return mVariant;
}

/*	---------------------------------------------------------------------------------------------
	ABalloonTip::ComputeTip
	
	Compute the location for the balloon's tip, given the pane's frame.
	---------------------------------------------------------------------------------------------
*/
Point	ABalloonTip::ComputeTip( const Rect& inAltRect ) const
{
	Point	theTip;
	theTip.v = (inAltRect.top + inAltRect.bottom) / 2;
	theTip.h = inAltRect.right - 4;
	return theTip;
}

Point	ABalloonTipForCheckBox::ComputeTip( const Rect& inAltRect ) const
{
	Point	theTip;
	theTip.v = (inAltRect.top + inAltRect.bottom) / 2;
	theTip.h = inAltRect.left + 6;
	return theTip;
}

Point	ABalloonTipForCheckBoxGroup::ComputeTip( const Rect& inAltRect ) const
{
	Point	theTip;
	theTip.v = inAltRect.top + 10;
	theTip.h = inAltRect.left + 16;
	return theTip;
}
