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


#ifndef IE_EXP_HRTEXT_H
#define IE_EXP_HRTEXT_H

#include "ie_exp.h"
#include "pl_Listener.h"
class PD_Document;
class s_HRText_Listener;

// The exporter/writer for human readable text

class IE_Exp_HRText_Sniffer : public IE_ExpSniffer
{
	friend class IE_Exp;

public:
	IE_Exp_HRText_Sniffer ();
	virtual ~IE_Exp_HRText_Sniffer () {}

	virtual bool recognizeSuffix (const char * szSuffix) override;
	virtual bool getDlgLabels (const char ** szDesc,
							   const char ** szSuffixList,
							   IEFileType * ft) override;
	virtual UT_Error constructExporter (PD_Document * pDocument,
										IE_Exp ** ppie) override;
};


class IE_Exp_HRText : public IE_Exp
{
public:
	IE_Exp_HRText(PD_Document * pDocument);
	virtual ~IE_Exp_HRText();

protected:
	virtual UT_Error	_writeDocument(void) override;

 private:
	s_HRText_Listener *	m_pListener;
};

#endif /* IE_EXP_HRTEXT_H */
