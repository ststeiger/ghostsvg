#pragma once

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
			
	Restrictions:	ABalloon is ©1998 by James W. Walker.
				Permission is granted for use of ABalloon
				free of charge, other than acknowledgement of
				James W. Walker in any program using ABalloon
				(perhaps in an About box or in accompanying
				documentation).
	
	Change history:
		1.0  2/28/98		First release.
		1.1  4/11/98
				€ Don¹t show balloons for invisible panes
				  (Thanks, Daniel Chiaramello, for the correction)
				€ Rolled ABalloonHelper::CreateABalloonHelper into
				  ABalloonHelper::AddABalloon.
				€ sBalloonAttachments is now a TArray<ABalloon*>
				  instead of TArray<LAttachment*>.
				€ In the HMShowBalloon call, use kHMRegularWindow
				  instead of kHMSaveBitsNoWindow, because IM says
				  that the former should normally be used.
		1.2  6/7/98...6/28/98
				€ New feature of automatically showing a balloon when
				  the cursor is motionless over a pane for a certain
				  length of time.  (Thanks to Fabrice for getting me
				  started on this and helping to implement it.)
				€ New subclass that automatically shows a balloon even
				  if you have not globally enabled automatic balloons.
				€ Added pragmas to get rid of annoying TArray warnings.
				€ Made the TArray an ordinary member rather than a
				  static member of ABalloonHelper.
				€ Changed the balloon mode to kHMSaveBitsWindow to fix
				  an update problem when a balloon was removed by the
				  appearance of a modal dialog.
		1.3  10/3/98, 10/25/98
				€ New function CalcExposedPortFrame used in FindBalloonPane.
				  This is so that a pane inside a scrolling view will
				  show its balloon even if it is only partially visible.
				  Thanks to Rainer Brockerhoff for reporting this problem.
				€ Implemented an option to make balloons appear immediately
				  when the control key is down.  This was contributed by
				  Bill Modesitt.
		1.4  11/27/98
				€ Added the helper class ABalloonTip, which controls the
				  location of the balloon tip and the variant of the
				  balloon window.
				€ Added another special case to provide good tip
				  placement for LCheckBoxGroupBox.
	---------------------------------------------------------------------------------------------
*/

#include <LAttachment.h>

class ABalloon : public LAttachment
{
	friend class ABalloonHelper;
	
public:
	enum { class_ID = 'ABal' };
	
					ABalloon(
							LStream			*inStream);
					
	virtual			~ABalloon();
					
	bool			GetAutomatic() const { return mAutomatic; }
	void			SetAutomatic( bool inAutoMode )
						{ mAutomatic = inAutoMode; }
	
	static void		EnableAutoPop( void );
	static void		SetAutoPopDelay( UInt32 inDelayTicks );
	static void		DisableAutoPop( void );
	static void		EnableControlKeyPop( void );
	static void		DisableControlKeyPop( void );
	
	static  bool	CalcExposedPortFrame( LPane* inPane, Rect& outFrame );
	
protected:

	virtual void	DoBalloon();
	
	virtual SInt16	ComputeStringIndex( LPane* inPane );
	
	virtual void	ShowBalloon( SInt16 inStringIndex, Rect& inAltRect );
	
	virtual Point	ComputeTip( const Rect& inAltRect );
	
	ResIDT				mStringListID;
	SInt16				mOffDisabledIndex;
	SInt16				mOffEnabledIndex;
	SInt16				mOnDisabledIndex;
	SInt16				mOnEnabledIndex;
	bool				mAutomatic;
	class ABalloonTip*	mTipSetter;
	
	static	LAttachable*	sLastBalloonedItem;
	static	SInt16			sLastIndex;
	static	ResIDT			sLastStringList;
};

class AAutoBalloon : public ABalloon
{
public:
	enum { class_ID = 'AABa' };
	
					AAutoBalloon( LStream *inStream )
						: ABalloon( inStream )
						{ mAutomatic = true; }
};

class ABalloonTip
{
public:
	ABalloonTip( short inVariant );
	ABalloonTip();
	
	virtual Point	ComputeTip( const Rect& inAltRect ) const;
	virtual short	GetVariant() const;

protected:
	short	mVariant;
};

class ABalloonTipForCheckBox : public ABalloonTip
{
public:
	ABalloonTipForCheckBox()
		: ABalloonTip( 5 ) {}
	
	virtual Point	ComputeTip( const Rect& inAltRect ) const;
};

class ABalloonTipForCheckBoxGroup : public ABalloonTip
{
public:
	ABalloonTipForCheckBoxGroup()
		: ABalloonTip( 5 ) {}
	
	virtual Point	ComputeTip( const Rect& inAltRect ) const;
};
