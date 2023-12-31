/* -*- mode: C++; tab-width: 4; c-basic-offset: 4; indent-tabs-mode:t; -*- */
/* AbiWord
 * Copyright (C) 1998 AbiSource, Inc.
 * Copyritht (C) 2003-2021 Hubert Figuière
 * 
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  
 * 02110-1301 USA.
 */

#include <string.h>

#include "ut_assert.h"
#include "ut_debugmsg.h"

#include "xap_CocoaDialog_Utilities.h"

#include "xap_App.h"
#include "xap_CocoaApp.h"
#include "xap_CocoaFrame.h"

#include "ap_Strings.h"

#include "ap_CocoaDialog_PageSetup.h"

/*********************************************************************************/

XAP_Dialog * AP_CocoaDialog_PageSetup::static_constructor(XAP_DialogFactory * pFactory, XAP_Dialog_Id dlgid)
{
	AP_CocoaDialog_PageSetup * p = new AP_CocoaDialog_PageSetup(pFactory, dlgid);
	return p;
}

AP_CocoaDialog_PageSetup::AP_CocoaDialog_PageSetup (XAP_DialogFactory * pDlgFactory, XAP_Dialog_Id dlgid) :
	AP_Dialog_PageSetup(pDlgFactory, dlgid)
{
	// 
}

AP_CocoaDialog_PageSetup::~AP_CocoaDialog_PageSetup(void)
{
	// 
}

void AP_CocoaDialog_PageSetup::runModal(XAP_Frame * pFrame)
{
	m_pFrame = pFrame;

	m_ctrl = [[AP_CocoaDialog_PageSetup_Controller alloc] initWithXAP:this];

	NSPrintInfo * printInfo = [NSPrintInfo sharedPrintInfo];

	[printInfo setOrientation:NSPaperOrientationPortrait];

	fp_PageSize fp = getPageSize();

	NSSize paperSize;
	paperSize.width = (CGFloat)fp.Width(DIM_PT);
	paperSize.height = (CGFloat)fp.Height(DIM_PT);

	[printInfo setPaperSize:paperSize];

	bool bPortrait = (getPageOrientation() == PORTRAIT);

	[printInfo setOrientation:(bPortrait ? NSPaperOrientationPortrait : NSPaperOrientationLandscape)];

	NSPageLayout * pageLayout = [NSPageLayout pageLayout];

	[pageLayout addAccessoryController:m_ctrl];

	while (true) {
		if ([pageLayout runModalWithPrintInfo:printInfo] != NSModalResponseOK) {
			setAnswer(a_CANCEL);
			break;
		}
		if (_validate(m_ctrl, printInfo)) {
			setAnswer(a_OK);
			break;
		}
	}
	[m_ctrl release];

	m_pFrame = 0;
}

bool AP_CocoaDialog_PageSetup::_validate(AP_CocoaDialog_PageSetup_Controller * /*ctrl*/, NSPrintInfo * printInfo)
{
	NSRect bounds = [printInfo imageablePageBounds];

	/* Printable page rectangle (inches)
	 */
#if 0
	CGFloat boundsOriginX = UT_convertDimensions(bounds.origin.x, DIM_PT, DIM_IN);
	CGFloat boundsOriginY = UT_convertDimensions(bounds.origin.y, DIM_PT, DIM_IN);
#endif

	CGFloat boundedWidth  = UT_convertDimensions(bounds.size.width,  DIM_PT, DIM_IN);
	CGFloat boundedHeight = UT_convertDimensions(bounds.size.height, DIM_PT, DIM_IN);

	if ((boundedWidth < 1.0) || (boundedHeight < 1.0))
	{
		/* Er. How do we handle ultra-small page sizes? For now, pop up: "The margins selected are too large to fit on the page."
		 */
		m_pFrame->showMessageBox(AP_STRING_ID_DLG_PageSetup_ErrBigMargins, XAP_Dialog_MessageBox::b_O, XAP_Dialog_MessageBox::a_OK);
		return false;
	}

	NSSize paperSize = [printInfo paperSize];
#if 0
	/* Paper size (inches)
	 */
	CGFloat width  = UT_convertDimensions(paperSize.width, DIM_PT, DIM_IN);
	CGFloat height = UT_convertDimensions(paperSize.height, DIM_PT, DIM_IN);

	/* Minimum margin sizes (inches)
	 */
	CGFloat top = height - boundsOriginY - boundedHeight;
	CGFloat bottom = boundsOriginY;
	CGFloat left = boundsOriginX;
	CGFloat right = width  - boundsOriginX - boundedWidth;
#endif
	/* Get dialog controller to update base class with margin settings
	 */
	[m_ctrl fetchData];

	/* Update base class with other settings
	 */
	bool bPortrait = ([printInfo orientation] == NSPaperOrientationPortrait);

	setPageOrientation(bPortrait ? PORTRAIT : LANDSCAPE);

	fp_PageSize fp(paperSize.width, paperSize.height, DIM_PT);

	setPageSize(fp);

	/* ...
	 */

	/* The window will only close (on an OK click) if the margins fit inside the paper size.
	 */
	bool bValid = true;

	if (!validatePageSettings()) {
		/* "The margins selected are too large to fit on the page."
		 */
		m_pFrame->showMessageBox(AP_STRING_ID_DLG_PageSetup_ErrBigMargins, XAP_Dialog_MessageBox::b_O, XAP_Dialog_MessageBox::a_OK);
		bValid = false;
	}
	return bValid;
}

@implementation AP_CocoaDialog_PageSetup_Controller

- (id)initWithXAP:(AP_CocoaDialog_PageSetup*)owner
{
    if (self = [super initWithNibName:@"ap_CocoaDialog_PageSetup" bundle:NSBundle.mainBundle]) {
        _xap = owner;
    }
    return self;
}

- (void)awakeFromNib
{
	const XAP_StringSet * pSS = XAP_App::getApp()->getStringSet();

	LocalizeControl(_adjustLabel,  pSS, AP_STRING_ID_DLG_PageSetup_Adjust);
	LocalizeControl(_marginBox,    pSS, AP_STRING_ID_DLG_PageSetup_Page);
	LocalizeControl(_percentLabel, pSS, AP_STRING_ID_DLG_PageSetup_Percent);
	LocalizeControl(_scaleBox,     pSS, AP_STRING_ID_DLG_PageSetup_Scale);
	LocalizeControl(_unitLabel,    pSS, AP_STRING_ID_DLG_PageSetup_Units);

	[_unitPopup removeAllItems];
	AppendLocalizedMenuItem(_unitPopup, pSS, XAP_STRING_ID_DLG_Unit_inch, DIM_IN);
	AppendLocalizedMenuItem(_unitPopup, pSS, XAP_STRING_ID_DLG_Unit_cm,   DIM_CM);
	AppendLocalizedMenuItem(_unitPopup, pSS, XAP_STRING_ID_DLG_Unit_mm,   DIM_MM);

	[_icon setImage:[NSImage imageNamed:@"margin"]];

	if (_xap) {
		[_adjustData    setIntValue:(_xap->getPageScale())];
		[_adjustStepper setIntValue:(_xap->getPageScale())];

		_last_margin_unit = _xap->getMarginUnits();

		[_unitPopup selectItemAtIndex:[_unitPopup indexOfItemWithTag:_last_margin_unit]];

		[   _topMargin setDoubleValue:(_xap->getMarginTop())];
		[_bottomMargin setDoubleValue:(_xap->getMarginBottom())];
		[  _leftMargin setDoubleValue:(_xap->getMarginLeft())];
		[ _rightMargin setDoubleValue:(_xap->getMarginRight())];

		[_headerMargin setDoubleValue:(_xap->getMarginHeader())];
		[_footerMargin setDoubleValue:(_xap->getMarginFooter())];
	}
}

- (void)fetchData
{
	_xap->setPageScale([_adjustData intValue]);

	_xap->setMarginUnits(_last_margin_unit);

	_xap->setMarginTop   ([   _topMargin doubleValue]);
	_xap->setMarginBottom([_bottomMargin doubleValue]);
	_xap->setMarginLeft  ([  _leftMargin doubleValue]);
	_xap->setMarginRight ([ _rightMargin doubleValue]);

	_xap->setMarginHeader([_headerMargin doubleValue]);
	_xap->setMarginFooter([_footerMargin doubleValue]);
}

- (IBAction)adjustAction:(id)sender
{
	int percent = [sender intValue];

	percent = (percent < 1) ? 1 : ((percent > 1000) ? 1000 : percent);

	[_adjustData    setIntValue:percent];
	[_adjustStepper setIntValue:percent];
}

- (IBAction)adjustStepperAction:(id)sender
{
	[_adjustData setIntValue:[sender intValue]];
}

- (IBAction)unitAction:(id)sender
{
	UT_UNUSED(sender);
	UT_Dimension mu = (UT_Dimension) [[_unitPopup selectedItem] tag];

	CGFloat top    = [_topMargin doubleValue];
	CGFloat bottom = [_bottomMargin doubleValue];
	CGFloat left   = [_leftMargin doubleValue];
	CGFloat right  = [_rightMargin doubleValue];

	CGFloat header = [_headerMargin doubleValue];
	CGFloat footer = [_footerMargin doubleValue];
	
	top    = UT_convertDimensions(top,    _last_margin_unit, mu);
	bottom = UT_convertDimensions(bottom, _last_margin_unit, mu);
	left   = UT_convertDimensions(left,   _last_margin_unit, mu);
	right  = UT_convertDimensions(right,  _last_margin_unit, mu);

	header = UT_convertDimensions(header, _last_margin_unit, mu);
	footer = UT_convertDimensions(footer, _last_margin_unit, mu);

	_last_margin_unit = mu;

	[   _topMargin setDoubleValue:top   ];
	[_bottomMargin setDoubleValue:bottom];
	[  _leftMargin setDoubleValue:left  ];
	[ _rightMargin setDoubleValue:right ];

	[_headerMargin setDoubleValue:header];
	[_footerMargin setDoubleValue:footer];
}

@end
