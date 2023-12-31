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
// *** THIS FILE DEFINES THE BINDINGS TO HANG OFF THE DeadOgonek PREFIX KEY IN   ***
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
#include "ap_LB_DeadOgonek.h"

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

	// Latin-[24] characters
	{0x41, /* A      */ { "insertOgonekData",		"",		"",		""		}},
	{0x45, /* E      */ { "insertOgonekData",		"",		"",		""		}},
	{0x49, /* I      */ { "insertOgonekData",		"",		"",		""		}},
	{0x55, /* U      */ { "insertOgonekData",		"",		"",		""		}},
	{0x61, /* a      */ { "insertOgonekData",		"",		"",		""		}},
	{0x65, /* e      */ { "insertOgonekData",		"",		"",		""		}},
	{0x69, /* i      */ { "insertOgonekData",		"",		"",		""		}},
	{0x75, /* u      */ { "insertOgonekData",		"",		"",		""		}},
};


/*****************************************************************
******************************************************************
** put it all together and load the default bindings.
******************************************************************
*****************************************************************/

bool ap_LoadBindings_DeadOgonek(AP_BindingSet * pThis,
								   EV_EditBindingMap * pebm)
{
	pThis->_loadChar(pebm,s_CharTable,G_N_ELEMENTS(s_CharTable),nullptr,0);
	
	return true;
}
