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

#ifndef IE_IMP_ISCII_H
#define IE_IMP_ISCII_H

#include "ie_imp_Text.h"

#define ISCII_INV	0xD9	// Invisible character - acts like Unicode ZWJ
#define ISCII_NUKTA	0xE9	// A Nukta following a character can modify its meaning
#define ISCII_ATTR	0xEF	// Attribute - meaning?
#define ISCII_EXT	0xF0	// EXT preceeding a character can modify its meaning

class PD_Document;

// ISCII file stream class

class ImportISCIIStreamFile : public ImportStreamFile
{
public:
	ImportISCIIStreamFile(GsfInput *pFile);
	~ImportISCIIStreamFile() {}
protected:
	virtual bool getRawChar(UT_UCSChar &b) override;
private:
	unsigned char m_cLookAhead;
	bool m_bNeedByte;
};

// The importer/reader for ISCII files.

class IE_Imp_ISCII_Sniffer : public IE_ImpSniffer
{
	friend class IE_Imp;

public:
	IE_Imp_ISCII_Sniffer(const char * name);
	virtual ~IE_Imp_ISCII_Sniffer() {}

	virtual const IE_SuffixConfidence * getSuffixConfidence() override;
	virtual const IE_MimeConfidence * getMimeConfidence() override { return nullptr; }
	virtual UT_Confidence_t recognizeContents (const char * szBuf,
									UT_uint32 iNumbytes) override;
	virtual bool getDlgLabels (const char ** szDesc,
							   const char ** szSuffixList,
							   IEFileType * ft) override;
	virtual UT_Error constructImporter (PD_Document * pDocument,
										IE_Imp ** ppie) override;

};

class IE_Imp_ISCII : public IE_Imp_Text
{
public:
	IE_Imp_ISCII(PD_Document * pDocument);
	virtual ~IE_Imp_ISCII() {};

protected:
	virtual UT_Error	_constructStream(ImportStream *& pStream, GsfInput * fp) override;
};

#endif /* IE_IMP_ISCII_H */
