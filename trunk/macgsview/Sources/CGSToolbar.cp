/* ============================================================================	*/
/*	CGSToolbar.h							written by Bernd Heller in 2000		*/
/* ----------------------------------------------------------------------------	*/
/*	Instantiates from LView.													*/
/* ============================================================================	*/


/* Copyright (C) 1999-2000, Bernd Heller.  All rights reserved.
  
  This file is part of MacGSView.
  
  This program is distributed with NO WARRANTY OF ANY KIND.  No author
  or distributor accepts any responsibility for the consequences of using it,
  or for whether it serves any particular purpose or works at all, unless he
  or she says so in writing.  Refer to the MacGSView Free Public Licence 
  (the "Licence") for full details.
  
  Every copy of MacGSView must include a copy of the Licence, normally in a 
  plain ASCII text file named LICENCE.  The Licence grants you the right 
  to copy, modify and redistribute MacGSView, but only under certain conditions 
  described in the Licence.  Among other things, the Licence requires that 
  the copyright notice and this notice be preserved on all copies.
*/



#include <LChasingArrows.h>
#include <LProgressBar.h>
#include <LStaticText.h>
#include <LCmdBevelButton.h>

#include "CGSToolbar.h"



// ---------------------------------------------------------------------------
//	€ CGSToolbar
// ---------------------------------------------------------------------------
//	Constructor

CGSToolbar::CGSToolbar()
{
}



// ---------------------------------------------------------------------------
//	€ CGSToolbar
// ---------------------------------------------------------------------------
//	Stream Constructor

CGSToolbar::CGSToolbar(LStream *inStream)
	: LView(inStream)
{
}



// ---------------------------------------------------------------------------
//	€ ~CGSToolbar
// ---------------------------------------------------------------------------
//	Destructor

CGSToolbar::~CGSToolbar()
{
}



// ---------------------------------------------------------------------------
//	€ FinishCreateSelf
// ---------------------------------------------------------------------------

void
CGSToolbar::FinishCreateSelf()
{
	HideChasingArrows();
	HideProgressBar();
	HideStatusText();
	HideStopButton();
}



// ---------------------------------------------------------------------------
//	€ ShowChasingArrows
// ---------------------------------------------------------------------------

void
CGSToolbar::ShowChasingArrows()
{
	LChasingArrows	*arrows = dynamic_cast<LChasingArrows*>
							  (FindPaneByID(kToolbarChasingArrows));
	arrows->Show();
	Draw(nil);
}



// ---------------------------------------------------------------------------
//	€ HideChasingArrows
// ---------------------------------------------------------------------------

void
CGSToolbar::HideChasingArrows()
{
	LChasingArrows	*arrows = dynamic_cast<LChasingArrows*>
							  (FindPaneByID(kToolbarChasingArrows));
	arrows->Hide();
	Draw(nil);
}



// ---------------------------------------------------------------------------
//	€ ShowStopButton
// ---------------------------------------------------------------------------

void
CGSToolbar::ShowStopButton()
{
	LCmdBevelButton	*stopButton = dynamic_cast<LCmdBevelButton*>
								  (FindPaneByID(kToolbarStopButton));
	stopButton->Show();
	Draw(nil);
}



// ---------------------------------------------------------------------------
//	€ HideStopButton
// ---------------------------------------------------------------------------

void
CGSToolbar::HideStopButton()
{
	LCmdBevelButton	*stopButton = dynamic_cast<LCmdBevelButton*>
								  (FindPaneByID(kToolbarStopButton));
	stopButton->Hide();
	Draw(nil);
}



// ---------------------------------------------------------------------------
//	€ ShowProgressBar
// ---------------------------------------------------------------------------

void
CGSToolbar::ShowProgressBar()
{
	LProgressBar	*progressBar = dynamic_cast<LProgressBar*>
								   (FindPaneByID(kToolbarProgressBar));
	progressBar->Show();
	Draw(nil);
}



// ---------------------------------------------------------------------------
//	€ HideProgressBar
// ---------------------------------------------------------------------------

void
CGSToolbar::HideProgressBar()
{
	LProgressBar	*progressBar = dynamic_cast<LProgressBar*>
								   (FindPaneByID(kToolbarProgressBar));
	progressBar->Hide();
	Draw(nil);
}



// ---------------------------------------------------------------------------
//	€ SetProgressMinMax
// ---------------------------------------------------------------------------

void
CGSToolbar::SetProgressMinMax(SInt32 inMin, SInt32 inMax)
{
	LProgressBar	*progressBar = dynamic_cast<LProgressBar*>
								   (FindPaneByID(kToolbarProgressBar));
	
	progressBar->SetMinValue(inMin);
	progressBar->SetMaxValue(inMax);
	progressBar->Draw(nil);
}



// ---------------------------------------------------------------------------
//	€ SetProgress
// ---------------------------------------------------------------------------

void
CGSToolbar::SetProgress(SInt32 inProgress)
{
	LProgressBar	*progressBar = dynamic_cast<LProgressBar*>
								   (FindPaneByID(kToolbarProgressBar));
	
	if (inProgress == -1) {
		// make indeterminate
		progressBar->SetIndeterminateFlag(true, true);
	} else {
		progressBar->SetValue(inProgress);
		progressBar->SetIndeterminateFlag(false, true);
	}
	progressBar->Draw(nil);
}



// ---------------------------------------------------------------------------
//	€ ShowStatusText
// ---------------------------------------------------------------------------

void
CGSToolbar::ShowStatusText()
{
	LStaticText		*statusText = dynamic_cast<LStaticText*>
								  (FindPaneByID(kToolbarStatusText));
	statusText->Show();
	Draw(nil);
}



// ---------------------------------------------------------------------------
//	€ HideStatusText
// ---------------------------------------------------------------------------

void
CGSToolbar::HideStatusText()
{
	LStaticText		*statusText = dynamic_cast<LStaticText*>
								  (FindPaneByID(kToolbarStatusText));
	statusText->Hide();
	Draw(nil);
}



// ---------------------------------------------------------------------------
//	€ SetStatusText
// ---------------------------------------------------------------------------

void
CGSToolbar::SetStatusText(StringPtr inText)
{
	LStaticText		*statusText = dynamic_cast<LStaticText*>
								  (FindPaneByID(kToolbarStatusText));
	statusText->SetDescriptor(inText);
	Draw(nil);
}




