/* AbiWord
 * Copyright (C) 1998 AbiSource, Inc.
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


// ********************************************************************************
// ********************************************************************************
// *** THIS FILE DEFINES THE BINDINGS TO HANG OFF THE DeadAcute PREFIX KEY IN   ***
// *** THE DEFAULT BINDINGS TABLE.                                              ***
// ********************************************************************************
// ********************************************************************************

#include "ut_assert.h"
#include "ut_types.h"
#include "ev_EditBits.h"
#include "ev_EditBinding.h"
#include "ev_EditMethod.h"
#include "ev_NamedVirtualKey.h"
#include "ap_LoadBindings.h"
#include "ap_LB_DeadAcute.h"

#define _S		| EV_EMS_SHIFT
#define _C		| EV_EMS_CONTROL
#define _A		| EV_EMS_ALT

/*****************************************************************
******************************************************************
** load bindings for the non-nvk
** (character keys).  note that SHIFT-state is implicit in the
** character value and is not included in the table.  note that
** we do not include the ASCII control characters (\x00 -- \x1f
** and others) since we don't map keystrokes into them.
******************************************************************
*****************************************************************/

static struct ap_bs_Char s_CharTable[] =
{
//	{char, /* desc   */ { none,						_C,		_A,		_A_C	}},
	{0x41, /* A      */ { "insertAcuteData",		"",		"",		""		}},
	{0x45, /* E      */ { "insertAcuteData",		"",		"",		""		}},
	{0x49, /* I      */ { "insertAcuteData",		"",		"",		""		}},
	{0x4f, /* O      */ { "insertAcuteData",		"",		"",		""		}},
	{0x55, /* U      */ { "insertAcuteData",		"",		"",		""		}},
	{0x59, /* Y      */ { "insertAcuteData",		"",		"",		""		}},
	{0x61, /* a      */ { "insertAcuteData",		"",		"",		""		}},
	{0x65, /* e      */ { "insertAcuteData",		"",		"",		""		}},
	{0x69, /* i      */ { "insertAcuteData",		"",		"",		""		}},
	{0x6f, /* o      */ { "insertAcuteData",		"",		"",		""		}},
	{0x75, /* u      */ { "insertAcuteData",		"",		"",		""		}},
	{0x79, /* y      */ { "insertAcuteData",		"",		"",		""		}},

	// Latin-2 characters
	{0x53, /* S      */ { "insertAcuteData",		"",		"",		""		}},
	{0x5a, /* Z      */ { "insertAcuteData",		"",		"",		""		}},
	{0x52, /* R      */ { "insertAcuteData",		"",		"",		""		}},
	{0x4c, /* L      */ { "insertAcuteData",		"",		"",		""		}},
	{0x43, /* C      */ { "insertAcuteData",		"",		"",		""		}},
	{0x4e, /* N      */ { "insertAcuteData",		"",		"",		""		}},

	{0x73, /* s      */ { "insertAcuteData",		"",		"",		""		}},
	{0x7a, /* z      */ { "insertAcuteData",		"",		"",		""		}},
	{0x72, /* r      */ { "insertAcuteData",		"",		"",		""		}},
	{0x6c, /* l      */ { "insertAcuteData",		"",		"",		""		}},
	{0x63, /* c      */ { "insertAcuteData",		"",		"",		""		}},
	{0x6e, /* n      */ { "insertAcuteData",		"",		"",		""		}},
};

/*****************************************************************
******************************************************************
** put it all together and load the default bindings.
******************************************************************
*****************************************************************/

bool ap_LoadBindings_DeadAcute(AP_BindingSet * pThis,
								  EV_EditBindingMap * pebm)
{
	pThis->_loadChar(pebm,s_CharTable,G_N_ELEMENTS(s_CharTable),nullptr,0);

	return true;
}
