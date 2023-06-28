/* -*- mode: C++; tab-width: 4; c-basic-offset: 4; -*- */

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

#include "ie_imp_DocBook.h"
#include "ie_exp_DocBook.h"
#include "xap_Module.h"

#ifdef ABI_PLUGIN_BUILTIN
#define abi_plugin_register abipgn_docbook_register
#define abi_plugin_unregister abipgn_docbook_unregister
#define abi_plugin_supports_version abipgn_docbook_supports_version
// dll exports break static linking
#define ABI_BUILTIN_FAR_CALL extern "C"
#else
#define ABI_BUILTIN_FAR_CALL ABI_FAR_CALL
ABI_PLUGIN_DECLARE("DocBook")
#endif

#define PLUGIN_NAME "AbiDocBook::DocBook"

// we use a reference-counted sniffer
static IE_Imp_DocBook_Sniffer * m_impSniffer = nullptr;
static IE_Exp_DocBook_Sniffer * m_expSniffer = nullptr;

ABI_BUILTIN_FAR_CALL
int abi_plugin_register (XAP_ModuleInfo * mi)
{
	if (!m_impSniffer)
	{
		m_impSniffer = new IE_Imp_DocBook_Sniffer (PLUGIN_NAME);
	}

	if (!m_expSniffer)
	{
		m_expSniffer = new IE_Exp_DocBook_Sniffer (PLUGIN_NAME);
	}

	IE_Imp::registerImporter (m_impSniffer);
	IE_Exp::registerExporter (m_expSniffer);

	mi->name = "DocBook Importer/Exporter";
	mi->desc = "Import/Export DocBook Documents";
	mi->version = ABI_VERSION_STRING;
	mi->author = "Abi the Ant, and Nicolas Mercier <linux@infobi.com> / Infobi";
	mi->usage = "No Usage";

	return 1;
}

ABI_BUILTIN_FAR_CALL
int abi_plugin_unregister (XAP_ModuleInfo * mi)
{
	mi->name = nullptr;
	mi->desc = nullptr;
	mi->version = nullptr;
	mi->author = nullptr;
	mi->usage = nullptr;

	UT_ASSERT (m_impSniffer);
	UT_ASSERT (m_expSniffer);

	IE_Imp::unregisterImporter (m_impSniffer);
	delete m_impSniffer;
	m_impSniffer = nullptr;

	IE_Exp::unregisterExporter (m_expSniffer);
	delete m_expSniffer;
	m_expSniffer = nullptr;

	return 1;
}

ABI_BUILTIN_FAR_CALL
int abi_plugin_supports_version (UT_uint32 /*major*/, UT_uint32 /*minor*/, 
								 UT_uint32 /*release*/)
{
  return 1;
}
