/* -*- mode: C++; tab-width: 4; c-basic-offset: 4; indent-tabs-mode: t -*- */

/* AbiWord
 * Copyright (C) 1998-2000 AbiSource, Inc.
 * Copyright (C) 2016-2021 Hubert Figuière
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "ut_locale.h"

#include "ut_assert.h"
#include "ut_debugmsg.h"
#include "ut_types.h"
#include "ut_base64.h"
#include "ut_bytebuf.h"
#include "ut_path.h"
#include "ut_misc.h"
#include "ut_string.h"
#include "ut_std_string.h"

#include "fg_GraphicRaster.h"
#include "ie_imp_PasteListener.h"

#include "pd_Document.h"

#include "ie_types.h"
#include "ie_impGraphic.h"
#include "ie_Table.h"
#include "ie_impexp_HTML.h"
#include "ie_imp_XHTML.h"
#include "ut_html.h"

typedef enum section_class
{
	sc_section = 0,
	sc_other
} SectionClass;

static const char * s_section_classes[sc_other] = {
	"Section"
};

#define CSS_MASK_INLINE (1<<0)
#define CSS_MASK_BLOCK  (1<<1)
#define CSS_MASK_IMAGE  (1<<2)
#define CSS_MASK_BODY   (1<<3)

static UT_UTF8String s_parseCSStyle (const UT_UTF8String & style, UT_uint32 css_mask);

/*****************************************************************/
/*****************************************************************/

IE_Imp_XHTML_Sniffer::IE_Imp_XHTML_Sniffer ()
#ifdef XHTML_NAMED_CONSTRUCTORS
	: IE_ImpSniffer("AbiXHTML::XHTML")
#endif
{
  // 
}

// supported suffixes
static IE_SuffixConfidence IE_Imp_XHTML_Sniffer__SuffixConfidence[] = {
	{ "xhtml", 	UT_CONFIDENCE_PERFECT 	},
	{ "html", 	UT_CONFIDENCE_PERFECT	},
	{ "htm", 	UT_CONFIDENCE_PERFECT	},
	{ "", 	UT_CONFIDENCE_ZILCH 	}
};

const IE_SuffixConfidence * IE_Imp_XHTML_Sniffer::getSuffixConfidence ()
{
	return IE_Imp_XHTML_Sniffer__SuffixConfidence;
}

#ifdef XHTML_NAMED_CONSTRUCTORS

// supported mimetypes
static IE_MimeConfidence IE_Imp_XHTML_Sniffer__MimeConfidence[] = {
	{ IE_MIME_MATCH_FULL, 	IE_MIMETYPE_XHTML, 		UT_CONFIDENCE_PERFECT 	}, 
	{ IE_MIME_MATCH_FULL, 	"application/xhtml", 	UT_CONFIDENCE_PERFECT 	}, 
	{ IE_MIME_MATCH_FULL, 	"text/html", 	 	 	UT_CONFIDENCE_PERFECT 	},
	{ IE_MIME_MATCH_BOGUS, 	"", 					UT_CONFIDENCE_ZILCH }
};

const IE_MimeConfidence * IE_Imp_XHTML_Sniffer::getMimeConfidence ()
{
	return IE_Imp_XHTML_Sniffer__MimeConfidence;
}

#endif /* XHTML_NAMED_CONSTRUCTORS */

UT_Confidence_t IE_Imp_XHTML_Sniffer::recognizeContents(const char * szBuf, 
											 UT_uint32 iNumbytes)
{
	UT_uint32 iLinesToRead = 6 ;  // Only examine the first few lines of the file
	UT_uint32 iBytesScanned = 0 ;
	const char *p ;
	const char *magic ;
	p = szBuf ;
	while( iLinesToRead-- )
	{
		magic = "<html" ;
		if ( (iNumbytes - iBytesScanned) < strlen(magic) ) return(UT_CONFIDENCE_ZILCH);
		if ( strncmp(p, magic, strlen(magic)) == 0 ) return(UT_CONFIDENCE_PERFECT);
		magic = "<!DOCTYPE html" ;
		if ( (iNumbytes - iBytesScanned) < strlen(magic) ) return(UT_CONFIDENCE_ZILCH);
		if ( strncmp(p, magic, strlen(magic)) == 0 ) return(UT_CONFIDENCE_PERFECT);

		magic = "<!DOCTYPE HTML" ;
		if ( (iNumbytes - iBytesScanned) < strlen(magic) ) return(UT_CONFIDENCE_ZILCH);
		if ( strncmp(p, magic, strlen(magic)) == 0 ) return(UT_CONFIDENCE_PERFECT);
		/*  Seek to the next newline:  */
		while ( *p != '\n' && *p != '\r' )
		{
			iBytesScanned++ ; p++ ;
			if( iBytesScanned+2 >= iNumbytes ) return(UT_CONFIDENCE_ZILCH);
		}
		/*  Seek past the next newline:  */
		if ( *p == '\n' || *p == '\r' )
		{
			iBytesScanned++ ; p++ ;
			if ( *p == '\n' || *p == '\r' )
			{
				iBytesScanned++ ; p++ ;
			}
		}
	}
	return UT_CONFIDENCE_ZILCH;
}

UT_Error IE_Imp_XHTML_Sniffer::constructImporter(PD_Document * pDocument,
												 IE_Imp ** ppie)
{
	IE_Imp_XHTML * p = new IE_Imp_XHTML(pDocument);
	*ppie = p;
	return UT_OK;
}

bool	IE_Imp_XHTML_Sniffer::getDlgLabels(const char ** pszDesc,
										   const char ** pszSuffixList,
										   IEFileType * ft)
{
	*pszDesc = "HTML (.html, .htm, .xhtml)";
	*pszSuffixList = "*.html; *.htm; *.xhtml";
	*ft = getFileType();
	return true;
}

/*****************************************************************/
/*****************************************************************/

  // hackishly, we append two lists to every document
  // to represent the <ol> and <ul> list types
  // <ol> has an id of 1 and <ul> is given the list id of 2

  static const gchar *ol_atts[] =
  {
    "id", "1",
    "parentid", "0",
    "type", "0",
    "start-value", "1",
    "list-delim", "%L.",
    "list-decimal", ".",
    nullptr, nullptr
  };

  static const gchar *ol_p_atts[] = 
  {
    "level", "1",
    "listid", "1",
    "parentid", "0",
    "props", "list-style:Numbered List; start-value:1; text-indent:-0.3in; field-font: nullptr;",
	/* margin-left is purposefully left out of the props.  It is computed
	   based on the depth of the list, and appended to this list of
	   attributes.
	*/
    "style", "Normal",
    nullptr, nullptr
  };

  static const gchar *ul_atts[] =
  {
    "id", "2",
    "parentid", "0",
    "type", "5",
    "start-value", "0",
    "list-delim", "%L",
    "list-decimal", "NULL",
    nullptr, nullptr
  };

  static const gchar *ul_p_atts[] =
  {
    "level", "1",
    "listid", "2",
    "parentid", "0",
    "props", "list-style:Bullet List; start-value:0; text-indent:-0.3in; field-font: nullptr;",
	/* margin-left is purposefully left out of the props.  It is computed
	   based on the depth of the list, and appended to this list of
	   attributes.
	*/
    "style", "Normal",
    nullptr, nullptr
  };

IE_Imp_XHTML::IE_Imp_XHTML(PD_Document * pDocument) :
	IE_Imp_XML(pDocument,false),
#ifdef USE_IE_IMP_TABLEHELPER
	m_TableHelperStack(new IE_Imp_TableHelperStack()),
#else
	m_TableHelperStack(0),
#endif
	m_listType(L_NONE),
    m_iListID(0),
	m_iNewListID(0),
	m_iNewImage(0),
	m_szBookMarkName(""),
	m_addedPTXSection(false),
	m_iPreCount(0),
	m_bFirstBlock(false),
	m_bInMath(false),
	m_pMathBB(new UT_ByteBuf)
{
}

IE_Imp_XHTML::~IE_Imp_XHTML()
{
#ifdef USE_IE_IMP_TABLEHELPER
	DELETEP(m_TableHelperStack);
#endif
	UT_VECTOR_PURGEALL(UT_UTF8String *,m_divStyles);
}

// to get lists to work:
// 1. append ol and ul type lists (done in constructor)
// 2. a ol or ul tag begins a paragraph with level=1, parentid=0, listid=[1|2]
//    style=[Numbered List|Bullet List]
// 3. append a field of type "list_label"
// 4. insert character run with type "list_label"
// 5. insert a tab

/*****************************************************************/
/*****************************************************************/

// This certainly leaves off lots of tags, but with HTML, this is inevitable - samth

static struct xmlToIdMapping s_Tokens[] =
{
	{ "a",			TT_A			},
	{ "address",	TT_ADDRESS		},
	{ "b",			TT_B			},
	{ "blockquote",	TT_BLOCKQUOTE	},
	{ "body",		TT_BODY			},
	{ "br",			TT_BR			},
	{ "caption",	TT_CAPTION		},
	{ "cite",		TT_CITE			},
	{ "code",		TT_CODE			},
	{ "col",		TT_COL			},
	{ "colgroup",	TT_COLGROUP		},
	{ "dd",			TT_DD			},
	{ "dfn",		TT_DFN			},
	{ "div",		TT_DIV			},
	{ "dl",			TT_DL			},
	{ "dt",			TT_DT			},
	{ "em",			TT_EM			},
	{ "font",		TT_FONT			},
	{ "h1",			TT_H1			},
	{ "h2",			TT_H2			},
	{ "h3",			TT_H3			},
	{ "h4",			TT_H4			},
	{ "h5",			TT_H5			},
	{ "h6",			TT_H6			},
	{ "head",		TT_HEAD			},
	{ "html",		TT_HTML			},
	{ "i",			TT_I			},
	{ "img",		TT_IMG			},
	{ "kbd",		TT_KBD			},
	{ "li",			TT_LI			},
	{ "math",		TT_MATH			},
	{ "meta",		TT_META			},
	{ "ol",			TT_OL			},
	{ "p",			TT_P			},
	{ "pre",		TT_PRE			},
	{ "q",			TT_Q			},
#ifdef XHTML_RUBY_SUPPORTED
	{ "rp",			TT_RP			},
	{ "rt",			TT_RT			},
	{ "ruby",		TT_RUBY			},
#endif
	{ "s",			TT_S			},
	{ "samp",		TT_SAMP			},
	{ "span",		TT_SPAN			},
	{ "strike",		TT_STRIKE		},
	{ "strong",		TT_STRONG		},
	{ "style",		TT_STYLE		},
	{ "sub",		TT_SUB			},
	{ "sup",		TT_SUP			},
	{ "table",		TT_TABLE		},
	{ "tbody",		TT_TBODY		},
	{ "td",			TT_TD			},
	{ "tfoot",		TT_TFOOT		},
	{ "th",			TT_TH			},
	{ "thead",		TT_THEAD		},
	{ "title",		TT_TITLE		},
	{ "tr",			TT_TR			},	
	{ "tt",			TT_TT			},
	{ "u",			TT_U			},
	{ "ul",			TT_UL			},
	{ "var",		TT_VAR			}
};

#define TokenTableSize	((sizeof(s_Tokens)/sizeof(s_Tokens[0])))

/*****************************************************************/	
/*****************************************************************/	

#define X_TestParseState(ps)	((m_parseState==(ps)))

#define X_VerifyParseState(ps)	do {  if (!(X_TestParseState(ps)))      \
				{  m_error = UT_IE_BOGUSDOCUMENT;	\
                                   UT_DEBUGMSG(("DOM: unhandled tag: %d (ps: %d)\n", tokenIndex, ps)); \
				   failLine = __LINE__; goto X_Fail; } } while (0)

#define X_CheckDocument(b)		do {  if (!(b))	\
					{  m_error = UT_IE_BOGUSDOCUMENT;	\
					failLine = __LINE__; goto X_Fail; } } while (0)

#define X_CheckError(v)			do {  if (!(v))	\
					  {  m_error = UT_ERROR; \
					     UT_DEBUGMSG(("JOHN: X_CheckError\n")); \
					failLine = __LINE__; goto X_Fail; } } while (0)

#define	X_EatIfAlreadyError()	do {  if (m_error) { failLine = __LINE__; goto X_Fail; } } while (0)

/*****************************************************************/
/*****************************************************************/

static SectionClass s_class_query (const char * class_value)
{
	if (class_value == nullptr)
		return sc_other;
	if (*class_value == 0) return sc_other;

	SectionClass sc = sc_other;

	for (int i = 0; i < static_cast<int>(sc_other); i++)
		if (strcmp (class_value, s_section_classes[i]) == 0)
			{
				sc = static_cast<SectionClass>(i);
				break;
			}
	return sc;
}

static void s_append_font_family (UT_UTF8String & style, const char * face)
{
	unsigned char u;

	while (*face)
		{
			u = static_cast<unsigned char>(*face);
			if (!isspace (static_cast<int>(u))) break;
			++face;
		}
	if (*face == 0) return;

	char quote = ',';
	if ((*face == '"') || (*face == '\''))
		{
			quote = *face++;
		}

	char * value = g_strdup (face);
	if (value == nullptr)
		return;

	char * ptr = value;
	while (*ptr)
		{
			if (*ptr == quote)
				{
					*ptr = 0;
					break;
				}
			++ptr;
		}
	if (quote == ',')
		while (ptr > value)
			{
				--ptr;
				u = static_cast<unsigned char>(*ptr);
				if (!isspace (static_cast<int>(u))) break;
				*ptr = 0;
			}
	if (strlen (value))
		{
			if (style.byteLength ())
				{
					style += "; ";
				}
			style += "font-family:";
			style += value;
		}
	g_free (value);
}

static const char * s_font_size[7] = {
	"8pt",
	"10pt",
	"11pt",
	"12pt",
	"16pt",
	"24pt",
	"32pt"
};

static void s_append_font_size (UT_UTF8String & style, const char * size)
{
	unsigned char u;

	while (*size)
		{
			u = static_cast<unsigned char>(*size);
			if (!isspace (static_cast<int>(u))) break;
			++size;
		}
	if (*size == 0) return;

	if (*size == '+')
		{
			int sz = atoi (size + 1);
			if ((sz < 1) || (sz > 7)) return;
			if (sz > 3) sz = 3;

			if (style.byteLength ())
				{
					style += "; ";
				}
			style += "font-size:";
			style += s_font_size[3+sz];
		}
	else if (*size == '-')
		{
			int sz = atoi (size + 1);
			if ((sz < 1) || (sz > 7)) return;
			if (sz > 3) sz = 3;

			if (style.byteLength ())
				{
					style += "; ";
				}
			style += "font-size:";
			style += s_font_size[3-sz];
		}
	else
		{
			int sz = atoi (size);
			if (sz == 0) return;

			if (style.byteLength ())
				{
					style += "; ";
				}
			style += "font-size:";

			if (sz <= 7)
				{
					style += s_font_size[sz-1];
				}
			else
				{
					std::string pt_size = UT_std_string_sprintf("%2dpt", sz);
					style += pt_size;
				}
		}
}

static void s_append_color (UT_UTF8String & style, const char * color, const char * property)
{
	unsigned char u;

	while (*color)
		{
			u = static_cast<unsigned char>(*color);
			if (!isspace (static_cast<int>(u))) break;
			++color;
		}
	if (*color == 0) return;

	char * value = g_strdup (color);
	if (value == nullptr)
		return;

	int length = 0;

	bool bValid = true;
	bool bHexal = true;

	char * ptr = value;
	if (*ptr == '#') ++ptr;
	while (*ptr)
		{
			char c = *ptr;
			u = static_cast<unsigned char>(c);
			if (isspace (static_cast<int>(u)))
				{
					*ptr = 0;
					break;
				}
			if (!isalnum (static_cast<int>(u)))
				{
					bValid = false;
					break;
				}
			if (bHexal) {
				if (!isxdigit(u)) {
					bHexal = false;
				}
			}
			++ptr;
			++length;
		}
	if (!bValid)
		{
			g_free (value);
			return;
		}
	if (*value == '#')
		if (!bHexal || ((length != 3) && (length != 6)))
			{
				g_free (value);
				return;
			}

	UT_HashColor hash_color;
	UT_UTF8String hex_color;

	if (*value == '#')
		{
			if (length == 3)
				{
					unsigned int rgb;
					if (sscanf (value + 1, "%x", &rgb) == 1)
						{
							unsigned int uir = (rgb & 0x0f00) >> 8;
							unsigned int uig = (rgb & 0x00f0) >> 4;
							unsigned int uib = (rgb & 0x000f);

							unsigned char r = static_cast<unsigned char>(uir|(uir<<4));
							unsigned char g = static_cast<unsigned char>(uig|(uig<<4));
							unsigned char b = static_cast<unsigned char>(uib|(uib<<4));

							hex_color = hash_color.setColor (r, g, b) + 1;
						}
				}
			else hex_color = value + 1;
		}
	else if (bHexal && (length == 6))
		{
			hex_color = value;
		}
	else
		{
			hex_color = hash_color.lookupNamedColor (color) + 1;
		}
	g_free (value);

	if (hex_color.byteLength () == 0) return;

	if (style.byteLength ())
		{
			style += "; ";
		}
	style += property;
	style += ":";
	style += hex_color;
}

/*****************************************************************/
/*****************************************************************/

static bool recognizeXHTML (const char * szBuf, UT_uint32 iNumbytes)
{
	UT_uint32 iLinesToRead = 6 ;  // Only examine the first few lines of the file
	UT_uint32 iBytesScanned = 0 ;
	const char *p ;
	const char *magic ;
	p = szBuf ;
	while( iLinesToRead-- )
	{
		magic = "<?xml ";
		if ( (iNumbytes - iBytesScanned) < strlen(magic) ) return(false);
		if ( strncmp(p, magic, strlen(magic)) == 0 ) return(true);

		magic = "<html xmlns=\"http://www.w3.org/1999/xhtml\" " ;
		if ( (iNumbytes - iBytesScanned) < strlen(magic) ) return(false);
		if ( strncmp(p, magic, strlen(magic)) == 0 ) return(true);

		/*  Seek to the next newline:  */
		while ( *p != '\n' && *p != '\r' )
		{
			iBytesScanned++ ; p++ ;
			if( iBytesScanned+2 >= iNumbytes ) return(false);
		}
		/*  Seek past the next newline:  */
		if ( *p == '\n' || *p == '\r' )
		{
			iBytesScanned++ ; p++ ;
			if ( *p == '\n' || *p == '\r' )
			{
				iBytesScanned++ ; p++ ;
			}
		}
	}

	return false;
}

UT_Error IE_Imp_XHTML::_loadFile(GsfInput * input)
{
	// see bug 10726 for why this is so convoluted, at least
	// until libxml2's HTML4 parser supports embedded namespaces
	bool is_xml = false;
	UT_Error e = UT_ERROR;

	{
		GsfInputMarker input_marker (input);

		gsf_off_t size = gsf_input_remaining (input);
		if (size > 5 /* strlen(magic) */)
			{
				char buf[1024];

				gsf_input_read (input, std::min(size, static_cast<gsf_off_t>(sizeof(buf))), (guint8*)buf);
				
				is_xml = recognizeXHTML (buf, std::min(size, static_cast<gsf_off_t>(sizeof(buf))));
			}
	}

	UT_XML * parser;

	if (is_xml)
		parser = new UT_XML;
	else
		parser = new UT_HTML;
			
	setParser (parser);
	e = IE_Imp_XML::_loadFile(input);
	setParser(nullptr);
	delete parser;

	// m_parseState = _PS_Sec; // no point having another sections the end
 	if (!requireBlock ()) e = UT_IE_BOGUSDOCUMENT;
	return e;
}

bool IE_Imp_XHTML::pasteFromBuffer(PD_DocumentRange * pDocRange,
								   const unsigned char * pData, 
								   UT_uint32 lenData, 
								   const char * szEncoding)
{
	UT_return_val_if_fail(getDoc() == pDocRange->m_pDoc,false);
	UT_return_val_if_fail(pDocRange->m_pos1 == pDocRange->m_pos2,false);
	
	PD_Document * newDoc = new PD_Document();
	newDoc->createRawDocument();
	UT_XML * newXML;

	if (recognizeXHTML ((const char *)pData, lenData))
		newXML = new UT_XML;
	else
		newXML = new UT_HTML (szEncoding);

	IE_Imp_XHTML * p = new IE_Imp_XHTML(newDoc);
	newXML->setListener(p);
	UT_ByteBuf buf (lenData);
	buf.append (pData, lenData);
	UT_Error e = newXML->parse (&buf);
	if(e != UT_OK)
	{
		char * szPrint = new char [lenData+1];
		UT_uint32 i = 0;
		for(i=0; i<lenData;i++)
			{
				szPrint[i] = pData[i];
			}
		szPrint[i] = 0;
		UT_DEBUGMSG(("Error Pasting HTML \n"));
		if(lenData < 10000)
		{
			UT_DEBUGMSG(("Data is %s Length of buffer is %d \n",szPrint,lenData));
		}
		delete p;
		delete newXML;
		delete [] szPrint;
		UNREFP( newDoc);
		return false;
	}
	newDoc->finishRawCreation();
	PT_DocPosition posEnd = 0;
	bool b = newDoc->getBounds(true, posEnd);
	if(!b || posEnd <= 2)
	{
		// import failed.
		char * szPrint = new char [lenData+1];
		UT_uint32 i = 0;
		for(i=0; i<lenData;i++)
			{
				szPrint[i] = pData[i];
			}
		szPrint[i] = 0;
		UT_DEBUGMSG(("Could not paste HTML.... \n"));
		if(lenData < 10000)
		{
			UT_DEBUGMSG(("Data is |%s| Length of buffer is %d \n",szPrint,lenData));
		}
		delete p;
		delete newXML;
		delete [] szPrint;
		UNREFP( newDoc);
		return false;
	}
	//
	// OK Broadcast from the just filled source document into our current
	// doc via the paste listener
	//
	IE_Imp_PasteListener * pPasteListen = new  IE_Imp_PasteListener(getDoc(),pDocRange->m_pos1,newDoc);
	newDoc->tellListener(static_cast<PL_Listener *>(pPasteListen));
	delete pPasteListen;
	delete p;
	delete newXML;
	UNREFP( newDoc);
	return true;
	//	setClipboard (pDocRange->m_pos1);

}

/*****************************************************************/
/*****************************************************************/

void IE_Imp_XHTML::startElement(const gchar *name,
								const gchar **attributes)
{
	const PP_PropertyVector atts = PP_cloneAndDecodeAttributes (attributes);

	int failLine;
	failLine = 0;
	UT_DEBUGMSG(("startElement: %s, parseState: %u, listType: %u\n", name, m_parseState, m_listType));
	UT_ASSERT(m_error == 0);
	X_EatIfAlreadyError();				// xml parser keeps running until buffer consumed
	                                                // this just avoids all the processing if there is an error

	UT_uint16 parentID;

	UT_uint32 tokenIndex;
	tokenIndex = _mapNameToToken (name, s_Tokens, TokenTableSize);

	if(m_bInMath && (tokenIndex != TT_MATH))
	{
		if(!m_pMathBB)
		{
			UT_ASSERT_HARMLESS(m_pMathBB);
			return;
		}

		m_pMathBB->append(reinterpret_cast<const UT_Byte *>("<"), 1);
		m_pMathBB->append(reinterpret_cast<const UT_Byte *>(name), strlen(name)); //build the mathml
		m_pMathBB->append(reinterpret_cast<const UT_Byte *>(">"), 1);
		return;
	}

	switch (tokenIndex)
	{
	case TT_HTML:
	  //UT_DEBUGMSG(("Init %d\n", m_parseState));
		X_VerifyParseState(_PS_Init);
		m_parseState = _PS_StyleSec;
		return;

	case TT_BODY:
	  //UT_DEBUGMSG(("Doc %d\n", m_parseState));
		X_VerifyParseState(_PS_StyleSec);
		m_parseState = _PS_Doc;
		return;

	case TT_DIV:
		{
			/* <div> is a block marker; <span> is an inline marker
			 */
			if (m_parseState == _PS_Block) m_parseState = _PS_Sec;

			/* stack class attr. values if recognized;
			 * NOTE: these are ptrs to static strings - don't alloc/g_free them
			 */
			const std::string & classVal = PP_getAttribute("class", atts);
			SectionClass sc = childOfSection () ? sc_other : s_class_query (classVal.c_str());
			if (sc == sc_other)
				m_divClasses.push_back(nullptr);
			else
				m_divClasses.push_back (s_section_classes[sc]);

			/* <div> elements can specify block-level styles; concatenate and stack...
			 */
			UT_UTF8String * prev = nullptr;
			if (m_divStyles.getItemCount ())
				{
					prev = m_divStyles.getLastItem ();
				}
			UT_UTF8String * style = nullptr;
			if (prev)
				style = new UT_UTF8String(*prev);
			else
				style = new UT_UTF8String;

			if (style)
				{
					const std::string & alignVal = PP_getAttribute("align", atts);
					if (!alignVal.empty())
						{
							if (alignVal == "right") {
								*style += "text-align: right; ";
							} else if (alignVal == "center") {
								*style += "text-align: center; ";
							} else if (alignVal == "left") {
								*style += "text-align: left; ";
							} else if (alignVal == "justify") {
								*style += "text-align: justify; ";
							}
						}
				}

			const std::string & styleVal = PP_getAttribute("style", atts);
			if (style && !styleVal.empty())
				{
					*style += styleVal;
					*style += "; ";
				}

			m_divStyles.push_back (style);

			/* for top-level section <div> elements, create a new document section
			 */
			if (sc != sc_other)
				{
					m_parseState = _PS_Doc;
					X_CheckError(requireSection ()); // TODO: handle this intelligently
				}
		}
		return;

	case TT_Q:
	case TT_SAMP:
	case TT_VAR:
	case TT_KBD:
	case TT_ADDRESS:
	case TT_CITE:
	case TT_EM:
	case TT_I:
		X_CheckError(pushInline ("font-style:italic"));
		return;

	case TT_DFN:
	case TT_STRONG:
	case TT_B:
		X_CheckError(pushInline ("font-weight:bold"));
		return;

	case TT_CODE:
	case TT_TT:
		X_CheckError(pushInline ("font-family:Courier"));
		return;

	case TT_U:
		X_CheckError(pushInline ("text-decoration:underline"));
		return;

	case TT_S://	case TT_STRIKE:
		X_CheckError(pushInline ("text-decoration:line-through"));
		return;

	case TT_SUP:
		X_CheckError(pushInline ("text-position:superscript"));
		return;

	case TT_SUB:
		X_CheckError(pushInline ("text-position:subscript"));
		return;
		
	case TT_FONT:
		UT_DEBUGMSG(("Font tag encountered\n"));
		{
			UT_UTF8String style;

			const std::string & colorVal = PP_getAttribute("color", atts);
			if (!colorVal.empty()) {
				s_append_color (style, colorVal.c_str(), "color");
			}

			const std::string & bgVal = PP_getAttribute("background", atts);
			if (!bgVal.empty()) {
				s_append_color (style, bgVal.c_str(), "bgcolor");
			}

			const std::string & sizeVal = PP_getAttribute("size", atts);
			if (!sizeVal.empty()) {
				s_append_font_size (style, sizeVal.c_str());
			}

			const std::string & faceVal = PP_getAttribute("face", atts);
			if (!faceVal.empty()) {
				s_append_font_family (style, faceVal.c_str());
			}

			// UT_String_sprintf(output, "color:%s; bgcolor: %s; font-family:%s; size:%spt", color.c_str(), bgcolor.c_str(), face.c_str(), size.c_str());
			UT_DEBUGMSG(("Font properties: %s\n", style.utf8_str()));

			X_CheckError(pushInline (style.utf8_str ()));
		}
		return;

	case TT_PRE:
	{
		if (m_parseState == _PS_Block) m_parseState = _PS_Sec;

		const std::string & style = PP_getAttribute("style", atts);
		newBlock ("Plain Text", style.c_str(), nullptr);

		m_iPreCount++;
		m_bWhiteSignificant = true;
		return;
	}

	case TT_H1:
	case TT_H2:
	case TT_H3:
	case TT_BLOCKQUOTE:
		// &...
	case TT_P:
	case TT_H4:
	case TT_H5:
	case TT_H6:
		if (m_listType != L_NONE)
		{
			/* hmm, document/paragraph structure inside list
			 */
			if (_data_CharCount ())
				{
					UT_UCSChar ucs = UCS_LF;
					X_CheckError(appendSpan (&ucs, 1));
					_data_NewBlock ();
				}
		}
		else
		{
			const std::string & styleVal = PP_getAttribute("style", atts);
			const std::string & alignVal = PP_getAttribute("align", atts);

			const std::string & awmlStyleVal = PP_getAttribute("awml:style", atts);

			if (!awmlStyleVal.empty())
				{
					X_CheckError (newBlock (awmlStyleVal.c_str(), styleVal.c_str(), alignVal.c_str()));
				}
			else if (tokenIndex == TT_H1)
				{
					X_CheckError (newBlock ("Heading 1", styleVal.c_str(), alignVal.c_str()));
				}
			else if (tokenIndex == TT_H2)
				{
					X_CheckError (newBlock ("Heading 2", styleVal.c_str(), alignVal.c_str()));
				}
			else if (tokenIndex == TT_H3)
				{
					X_CheckError (newBlock ("Heading 3", styleVal.c_str(), alignVal.c_str()));
				}
			else if (tokenIndex == TT_BLOCKQUOTE)
				{
					X_CheckError (newBlock ("Block Text", styleVal.c_str(), alignVal.c_str()));
				}
			else if (m_bWhiteSignificant)
				{
					X_CheckError (newBlock ("Plain Text", styleVal.c_str(), alignVal.c_str()));
				}
			else
				{
					X_CheckError (newBlock ("Normal", styleVal.c_str(), alignVal.c_str()));
				}
		}
		return;

	case TT_OL:	  
	case TT_UL:
	case TT_DL:
	{
		if(tokenIndex == TT_OL)
	  		m_listType = L_OL;
	  	else 
			m_listType = L_UL;

		if(m_parseState == _PS_Block)
		{
			endElement("li");
			m_parseState = _PS_Sec;
			m_bWasSpace = false;
			/* this sort of tag shuffling can mess up the space tracking */
		}
		else if(m_parseState != _PS_Sec)
		{
			requireSection ();
			m_bWasSpace = false;
		}

		parentID = m_iListID;
		m_utsParents.push(parentID);

		/* new list, increment list depth */
		m_iNewListID++;
		m_iListID = m_iNewListID;

		const gchar** listAtts;
		listAtts = (tokenIndex == TT_OL ? ol_atts : ul_atts);

		std::string szListID, szParentID;
		szListID = UT_std_string_sprintf("%u", m_iNewListID);
		szParentID = UT_std_string_sprintf("%u", parentID);

		const int IDpos = 1;
		const int parentIDpos = 3;

		listAtts[IDpos] = szListID.c_str();
		listAtts[parentIDpos] = szParentID.c_str();

		X_CheckError(getDoc()->appendList (PP_std_copyProps(listAtts)));

		return;
	}
	case TT_LI:
	case TT_DT:
	case TT_DD:
	{
		if (m_parseState == _PS_Block)
			m_parseState =  _PS_Sec;
		X_CheckError (requireSection ());

		if (m_listType != L_NONE)
		{
			UT_uint16 thisID = m_iListID;
			parentID = m_utsParents.top();

			const gchar** listAtts;
			listAtts = (m_listType == L_OL ? ol_p_atts : ul_p_atts);

			/* assign the appropriate list ID, parent ID, and level
			   to this list item's attributes */

			std::string szListID, szParentID, szLevel, szMarginLeft;
			szListID = UT_std_string_sprintf("%u", thisID);
			szParentID = UT_std_string_sprintf("%u", parentID);
			szLevel = UT_std_string_sprintf("%lu", (unsigned long)m_utsParents.size());

			{
				UT_LocaleTransactor t(LC_NUMERIC, "C");
				szMarginLeft = UT_std_string_sprintf(" margin-left: %.2fin",
								  m_utsParents.size() * 0.5);
			}

			const int LevelPos = 1;
			const int IDpos = 3;
			const int parentIDpos = 5;
			const int propsPos = 7;

			std::string props = listAtts[propsPos];
			props += szMarginLeft;

			listAtts[LevelPos] = szLevel.c_str();
			listAtts[IDpos] = szListID.c_str();
			listAtts[parentIDpos] = szParentID.c_str();

			const gchar* temp = static_cast<const gchar*>(listAtts[propsPos]);
			listAtts[propsPos] = props.c_str();

			X_CheckError(appendStrux(PTX_Block, PP_std_copyProps(listAtts)));
			m_parseState = _PS_Block;
			m_bFirstBlock = true;
			listAtts[propsPos] = temp;

			// append a field
			const PP_PropertyVector new_atts = {
				"type", "list_label"
			};
			X_CheckError(appendObject(PTO_Field, new_atts));

			// append the character run
			X_CheckError(appendFmt(new_atts));

			/* warn XML charData() handler of new block, but insert a tab first
			 */
			UT_UCSChar ucs = UCS_TAB;
			X_CheckError(appendSpan (&ucs, 1));
			_data_NewBlock ();
		}
		return;
	}

	case TT_SPAN:
		{
			UT_UTF8String utf8val;

			const std::string & style = PP_getAttribute(static_cast<const gchar *>("style"), atts);
			if (!style.empty())
				{
					utf8val = style;
					utf8val = s_parseCSStyle (utf8val, CSS_MASK_INLINE);
					UT_DEBUGMSG(("CSS->Props (utf8val): [%s]\n",utf8val.utf8_str()));
				}
			X_CheckError(pushInline (utf8val.utf8_str ()));
		}
		return;

	case TT_BR:
	  //UT_DEBUGMSG(("B %d\n", m_parseState));
		if(m_parseState == _PS_Block)
		{
			UT_UCSChar ucs = UCS_LF;
			X_CheckError(appendSpan(&ucs,1));
		}
		return;

	case TT_A:
	{
		const std::string * val = nullptr;
		val = &PP_getAttribute("xlink:href", atts);
		if (val->empty()) {
			val = &PP_getAttribute("href", atts);
		}
		if(!val->empty()) {
			X_CheckError(requireBlock ());
			const PP_PropertyVector new_atts = {
				"xlink:href", *val
			};
			X_CheckError(appendObject(PTO_Hyperlink, new_atts));
		} else {
			val = &PP_getAttribute("id", atts);
			if (val->empty()) {
				val = &PP_getAttribute("name", atts);
			}
			if (!val->empty()) {
				X_CheckError(requireBlock ());

				m_szBookMarkName = *val;

				PP_PropertyVector bm_new_atts = {
					"type", "start",
					"name", *val
				};
				X_CheckError(appendObject(PTO_Bookmark, bm_new_atts));

				if (m_parseState == _PS_Sec) {
					bm_new_atts[1] = "end";
					X_CheckError(appendObject(PTO_Bookmark, bm_new_atts));

					m_szBookMarkName.clear();
				}
			}
		}
		return;
	}

	case TT_IMG:
		{
			const std::string & szSrc    = PP_getAttribute("src", atts);
			const std::string & szStyle  = PP_getAttribute("style",  atts);
			std::string szWidth  = PP_getAttribute("width",  atts);
			std::string szHeight = PP_getAttribute("height", atts);
			const std::string & szTitle  = PP_getAttribute("title", atts);
			const std::string & szAlt    = PP_getAttribute("alt", atts);

			if (szSrc.empty()) {
				break;
			}

		UT_UTF8String sWidth;
		UT_UTF8String sHeight;

		FG_ConstGraphicPtr pfg;

		if (szSrc.compare(0, 5, "data:") == 0) { // data-URL - special case
			pfg = importDataURLImage (szSrc.c_str() + 5);
		} else if (!isClipboard ()) {
			pfg = importImage (szSrc.c_str());
		}

		if (pfg == nullptr) {
            break;
		}

		const UT_ConstByteBufPtr & pBB = pfg->getBuffer();
		X_CheckError(pBB);

		if(!szWidth.empty())
			{
				UT_Dimension units = UT_determineDimension (szWidth.c_str());
				if(units == DIM_PERCENT)
					{
						getDoc()->convertPercentToInches(szWidth.c_str(), sWidth);
						szWidth = sWidth.utf8_str();
					}
			}
		if(!szHeight.empty())
			{
				UT_Dimension units = UT_determineDimension (szHeight.c_str());
				if(units == DIM_PERCENT)
					{
						getDoc()->convertPercentToInches(szWidth.c_str(), sHeight);
						szHeight = sHeight.utf8_str();
					}
			}


		UT_UTF8String utf8val;
		if (!szStyle.empty())
			{
				utf8val = szStyle;
				utf8val = s_parseCSStyle (utf8val, CSS_MASK_IMAGE);
				UT_DEBUGMSG(("CSS->Props (utf8val): [%s]\n",utf8val.utf8_str()));
			}
		if (!szWidth.empty() && (strstr(utf8val.utf8_str(), "width") == nullptr))
			{
				UT_Dimension units = UT_determineDimension (szWidth.c_str(), DIM_PX);
				double d = UT_convertDimensionless (szWidth.c_str());
				float width = static_cast<float>(UT_convertDimensions (d, units, DIM_IN));
				std::string tmp;

				{
					UT_LocaleTransactor t(LC_NUMERIC, "C");
					tmp = UT_std_string_sprintf ("%gin", width);
				}

				if (!tmp.empty ())
				{
					if (utf8val.byteLength ())
						utf8val += "; ";
					utf8val += "width:";
					utf8val += tmp;
				}
			}
		if (!szHeight.empty() && (strstr(utf8val.utf8_str(), "height") == nullptr))
			{
				UT_Dimension units = UT_determineDimension (szHeight.c_str(), DIM_PX);
				double d = UT_convertDimensionless (szHeight.c_str());
				float height = static_cast<float>(UT_convertDimensions (d, units, DIM_IN));
				std::string tmp;

				{
					UT_LocaleTransactor t(LC_NUMERIC, "C");
					tmp = UT_std_string_sprintf ("%gin", height);
				}

				if (!tmp.empty ())
					{
						if (utf8val.byteLength ())
							utf8val += "; ";
						utf8val += "height:";
						utf8val += tmp;
					}
			}
		if ((strstr(utf8val.utf8_str(), "width") == nullptr) ||
			(strstr(utf8val.utf8_str(), "height") == nullptr))
		{
			float width  = static_cast<float>(pfg->getWidth ());
			float height = static_cast<float>(pfg->getHeight ());
			if ((width > 0) && (height > 0))
	   		{
				UT_DEBUGMSG(("missing width or height; reverting to image defaults\n"));
#if 0
				if (strstr(utf8val.utf8_str(), "width") != nullptr)
				{
					float rat = height/width;
					float fwidth = UT_convertToInches(szWidth.c_str());
					height = rat*fwidth;
					std::string tmp;
					{
						UT_LocaleTransactor t(LC_NUMERIC, "C");
						tmp = UT_std_string_sprintf ("%gin", height);
					}
					if (utf8val.byteLength ())
						utf8val += "; ";
					utf8val += "height:";
					utf8val += tmp;
					goto got_string;
				}
#endif
			}
			else
			{
				UT_DEBUGMSG(("missing width or height; setting these to 100x100\n"));
				width  = static_cast<float>(100);
				height = static_cast<float>(100);
			}
			width = width/96.0f;
			height = height/96.0f;
			if(height > 8.0)
				{
					float rat = 8.0f/height;
					width = width * rat;
					height = 8.0;
				}
			std::string tmp;
			{
				UT_LocaleTransactor t(LC_NUMERIC, "C");
				tmp = UT_std_string_sprintf ("width:%gin; height:%gin", width, height);
			}

			utf8val = tmp;
		}
#if 0
		got_string:
#endif
		std::string dataid = UT_std_string_sprintf ("image%u", static_cast<unsigned int>(m_iNewImage++));

		const PP_PropertyVector api_atts = {
			PT_PROPS_ATTRIBUTE_NAME, utf8val.utf8_str(),
			"dataid", dataid,
			"title", szTitle,
			"alt", szAlt
		};
		if (m_parseState == _PS_Sec)
			{
				X_CheckError(requireBlock ());
			}
		xxx_UT_DEBUGMSG(("inserting `%s' as `%s' [%s]\n",szSrc,dataid.c_str(),utf8val.utf8_str()));

		X_CheckError(appendObject (PTO_Image, api_atts));
		X_CheckError(getDoc()->createDataItem (dataid.c_str(), false, pBB,
                                               pfg->getMimeType(), nullptr));

		UT_DEBUGMSG(("insertion successful\n"));
		}
		return;
#ifdef USE_IE_IMP_TABLEHELPER
	case TT_CAPTION:
		{
			UT_DEBUGMSG(("Found a caption \n"));
			m_TableHelperStack->setCaptionOn();
			m_parseState = _PS_Block;
			break;
		}
		break;
	case TT_COLGROUP:
	case TT_COL:
		// TODO
		break;
	case TT_TABLE:
		{
			requireSection();
			m_parseState = _PS_Table;
			const std::string & szStyle = PP_getAttribute("style", atts);

			X_CheckError(m_TableHelperStack->tableStart(getDoc(), szStyle.c_str()));
		}
		break;
	case TT_THEAD:
		{
			m_parseState = _PS_Table;
			const std::string & szStyle = PP_getAttribute("style", atts);

			m_TableHelperStack->theadStart(szStyle.c_str());
		}
		break;
	case TT_TFOOT:
		{
			m_parseState = _PS_Table;
			const std::string & szStyle = PP_getAttribute("style", atts);

			m_TableHelperStack->tfootStart(szStyle.c_str());
		}
		break;
	case TT_TBODY:
		{
			m_parseState = _PS_Table;
			const std::string & szStyle = PP_getAttribute("style", atts);

			m_TableHelperStack->tbodyStart(szStyle.c_str());
		}
		break;
	case TT_TR:
		{
			m_parseState = _PS_Cell;
			const std::string & szStyle = PP_getAttribute("style", atts);

			m_TableHelperStack->trStart (szStyle.c_str());
			UT_DEBUGMSG(("Finished TR process \n"));
		}
		break;
	case TT_TH:
	case TT_TD:
		{
			UT_DEBUGMSG(("Doing TD \n"));
			m_parseState = _PS_Block;
			const std::string & szStyle   = PP_getAttribute("style", atts);
			const std::string & szColSpan = PP_getAttribute("colspan", atts);
			const std::string & szRowSpan = PP_getAttribute("rowspan", atts);

			UT_uint32 colspan = 1;
			UT_uint32 rowspan = 1;
			if (!szColSpan.empty())	{
					int span = atoi(szColSpan.c_str());
					if (span > 1) {
						colspan = static_cast<UT_uint32>(span);
					}
				}
			if (!szRowSpan.empty()) {
					int span = atoi(szRowSpan.c_str());
					if (span > 1) {
						rowspan = static_cast<UT_uint32>(span);
					}
				}
			m_TableHelperStack->tdStart (rowspan, colspan, szStyle.c_str());
		}
		break;
#endif /* USE_IE_IMP_TABLEHELPER */
	case TT_HEAD:
	case TT_STYLE:
		// these tags are ignored for the time being
		return;

	case TT_TITLE:
		{
			X_VerifyParseState(_PS_StyleSec);
			m_parseState = _PS_MetaData;
		}
		return;

	case TT_META:
		{
			if (!isPasting())
				{
					const std::string & szName = PP_getAttribute("name", atts);
					const std::string & szContent = PP_getAttribute("content", atts);

					if (!szName.empty() && !szContent.empty())
						{
							if (0 == g_ascii_strcasecmp(szName.c_str(), "title"))
								getDoc()->setMetaDataProp(PD_META_KEY_TITLE, szContent);
							else if (0 == g_ascii_strcasecmp(szName.c_str(), "author"))
								getDoc()->setMetaDataProp(PD_META_KEY_CREATOR, szContent);
							else if (0 == g_ascii_strcasecmp(szName.c_str(), "keywords"))
								getDoc()->setMetaDataProp(PD_META_KEY_KEYWORDS, szContent);
							else if (0 == g_ascii_strcasecmp(szName.c_str(), "subject"))
								getDoc()->setMetaDataProp(PD_META_KEY_SUBJECT, szContent);
						}
				}
		}
		return;

	case TT_RUBY:
	case TT_RP:
	case TT_RT:
	  //UT_DEBUGMSG(("B %d\n", m_parseState));
		// For now we just want to render any text between these tags
		// Eventually we want to render the <rt> text above the
		// <ruby> text.  The <rp> text will not be rendered but should
		// be retained so it can be exported.
		X_CheckError(requireBlock ());
		X_CheckError(appendFmt(PP_NOPROPS));
		return;

	case TT_MATH:
		X_VerifyParseState(_PS_Block);

		m_pMathBB.reset();
		m_bInMath = true;
		m_pMathBB = UT_ByteBufPtr(new UT_ByteBuf);
		m_pMathBB->append(reinterpret_cast<const UT_Byte *>("<math xmlns='http://www.w3.org/1998/Math/MathML' display='block'>"), 65);
		return;

	case TT_OTHER:
	default:
		UT_DEBUGMSG(("Unknown tag [%s]\n",name));

		//It's imperative that we keep processing after finding an unknown element

		return;
	}
	UT_ASSERT(m_error == 0);

X_Fail:
	return;
}

void IE_Imp_XHTML::endElement(const gchar *name)
{
	int failLine = 0;
	UT_uint32 uid;

	UT_DEBUGMSG(("endElement: %s, parseState: %u, listType: %u\n", name, m_parseState, m_listType));
	X_EatIfAlreadyError();				// xml parser keeps running until buffer consumed
	
	
	UT_uint32 tokenIndex;
	tokenIndex = _mapNameToToken (name, s_Tokens, TokenTableSize);
	//if(!strcmp(name == "html")) UT_DEBUGMSG(("tokenindex : %d\n", tokenIndex));

	if(m_bInMath && (tokenIndex != TT_MATH))
	{
		UT_return_if_fail(m_pMathBB);
		m_pMathBB->append(reinterpret_cast<const UT_Byte *>("</"), 2);
		m_pMathBB->append(reinterpret_cast<const UT_Byte *>(name), strlen(name)); //build the mathml
		m_pMathBB->append(reinterpret_cast<const UT_Byte *>(">"), 1);
		return;
	}

	switch (tokenIndex)
	{
	case TT_HTML:
		m_parseState = _PS_Init; // irrel. - shouldn't see anything after this
		return;

	case TT_BODY:
		/* add two empty blocks at the end...
		 */
		requireSection ();
		// m_parseState = _PS_Sec; // no point having two sections at the end
		newBlock("Normal", nullptr, nullptr);
		m_parseState = _PS_Sec; // no point having two sections at the end
		newBlock("Normal", nullptr, nullptr);

		m_parseState = _PS_Init;
		return;

	case TT_DIV:
		{
			if (m_parseState == _PS_Block) m_parseState = _PS_Sec;

			m_divClasses.pop_back ();

			if (m_divStyles.getItemCount ())
				{
					UT_UTF8String * prev = nullptr;
					prev = m_divStyles.getLastItem ();
					DELETEP(prev);
				}
			m_divStyles.pop_back ();
		}
		return;

	case TT_OL:
	case TT_UL:
	case TT_DL:
		m_iListID = m_utsParents.top();
		m_utsParents.pop();

		if(m_utsParents.empty()) {
			m_listType = L_NONE;
		}

		return;

	case TT_LI:
	case TT_DT:
	case TT_DD:
		UT_ASSERT_HARMLESS(m_lenCharDataSeen==0);
		m_parseState = _PS_Sec;
		while (_getInlineDepth ()) _popInlineFmt ();
		return;

	case TT_P:
	case TT_H1:
	case TT_H2:
	case TT_H3:
	case TT_H4:
	case TT_H5:
	case TT_H6:
	case TT_BLOCKQUOTE:
		UT_ASSERT_HARMLESS(m_lenCharDataSeen==0);
		if (m_listType == L_NONE)
			{
				m_parseState = _PS_Sec;
				while (_getInlineDepth ()) _popInlineFmt ();
			}
		return;

		/* text formatting
		 */

	case TT_S://	case TT_STRIKE:
	case TT_U:
	case TT_SUP:
	case TT_SUB:
	case TT_FONT:	
	case TT_CODE:
	case TT_TT:
	case TT_DFN:
	case TT_STRONG:
	case TT_B:
	case TT_Q:
	case TT_SAMP:
	case TT_VAR:
	case TT_KBD:
	case TT_ADDRESS:
	case TT_CITE:
	case TT_EM:
	case TT_I:
		UT_ASSERT_HARMLESS(m_lenCharDataSeen==0);
		_popInlineFmt ();
		if (m_parseState == _PS_Block)
			{
				X_CheckError(appendFmt(m_vecInlineFmt));
			}
		return;

	case TT_SPAN:
		UT_ASSERT_HARMLESS(m_lenCharDataSeen==0);
		//UT_DEBUGMSG(("B %d\n", m_parseState));
		X_VerifyParseState(_PS_Block);
		X_CheckDocument(_getInlineDepth()>0);
		_popInlineFmt();
		appendFmt(m_vecInlineFmt);
		return;

	case TT_BR:						// not a container, so we don't pop stack
		UT_ASSERT_HARMLESS(m_lenCharDataSeen==0);
		//UT_DEBUGMSG(("B %d\n", m_parseState));
//		X_VerifyParseState(_PS_Block);
		return;
#ifdef USE_IE_IMP_TABLEHELPER
	case TT_CAPTION:
		{
			UT_DEBUGMSG(("End Caption \n"));
			m_TableHelperStack->setCaptionOff();
			m_parseState = _PS_Table;
			break;
		}
	case TT_COLGROUP:
	case TT_COL:
		// TODO
		break;
	case TT_TABLE:
		{
			m_TableHelperStack->tableEnd ();
			m_parseState = _PS_Sec;
		}
		break;
	case TT_THEAD:
	case TT_TFOOT:
	case TT_TBODY:
		{
			//			m_TableHelperStack->tbodyStart ();
			m_parseState = _PS_Table;
		}
		break;
	case TT_TR:
		{
			m_parseState = _PS_Table;
		// 
			break;
		}
	case TT_TH:
	case TT_TD:
		{
			m_TableHelperStack->tdEnd();
			m_parseState = _PS_Cell;
			break;
		}
#endif /* USE_IE_IMP_TABLEHELPER */
	case TT_HEAD:
	case TT_META:
	case TT_STYLE:
		return;

	case TT_TITLE:
		{
			X_VerifyParseState(_PS_MetaData);
			m_parseState = _PS_StyleSec;

			if (!isPasting ())
				{
					getDoc()->setMetaDataProp(PD_META_KEY_TITLE, m_Title);
					m_Title.clear();
				}
		}
		return;

	case TT_A:
		if(!m_szBookMarkName.empty()) {
			const PP_PropertyVector bm_new_atts = {
				"type", "end",
				"name", m_szBookMarkName
			};
			X_CheckError(appendObject(PTO_Bookmark, bm_new_atts));
			m_szBookMarkName.clear();
		} else if (m_parseState == _PS_Block) {
			/* if (m_parseState == _PS_Sec) then this is an anchor outside
			 * of a block, not a hyperlink (see TT_A in startElement)
			 */
 			X_CheckError(appendObject(PTO_Hyperlink, PP_NOPROPS));
		}
		return;

	case TT_PRE:
		UT_ASSERT_HARMLESS(m_lenCharDataSeen==0);
		if (m_parseState == _PS_Block) m_parseState = _PS_Sec;
		m_iPreCount--;
		m_bWhiteSignificant = (m_iPreCount > 0);
		return;

	case TT_RUBY:
	case TT_RT:
	case TT_RP:
		// For now we just want to render any text between these tags
		// Eventually we want to render the <rt> text above the
		// <ruby> text.  The <rp> text will not be rendered but should
		// be retained so it can be exported.
		UT_ASSERT_HARMLESS(m_lenCharDataSeen==0);
		//UT_DEBUGMSG(("B %d\n", m_parseState));
		X_VerifyParseState(_PS_Block);
		X_CheckDocument(_getInlineDepth()==0);
		//_popInlineFmt();
		X_CheckError(appendFmt(m_vecInlineFmt));
		return;

	case TT_MATH:
	{
		X_VerifyParseState(_PS_Block);

		UT_return_if_fail(m_pMathBB && m_bInMath);
		m_pMathBB->append(reinterpret_cast<const UT_Byte *>("</math>"), 7);

		// Create the data item

		uid = getDoc()->getUID(UT_UniqueId::Math);
		std::string sUID = UT_std_string_sprintf("MathLatex%d",uid);
		X_CheckError(getDoc()->createDataItem(sUID.c_str(), false, m_pMathBB, "", nullptr));

		PP_PropertyVector new_atts = {
			"dataid", sUID
		};

		X_CheckError(appendObject(PTO_Math, new_atts));

		m_bInMath = false;

		return;
	}
	case TT_OTHER:
	default:
	  	UT_DEBUGMSG(("Unknown end tag [%s]\n",name));
		return;
	}
	return;
X_Fail:
	UT_DEBUGMSG (("X_Fail at %d\n", failLine));
	return;
}

void IE_Imp_XHTML::charData (const gchar * buffer, int length)
{
#if DEBUG
#if 0

	UT_UTF8String sBuf;
	sBuf.append(buffer,length);
	UT_DEBUGMSG(("IE_Imp_XHTML::charData Text | %s | \n",sBuf.utf8_str()));
#endif
#endif
	if(m_bInMath)
	{
		UT_return_if_fail(m_pMathBB);
		m_pMathBB->append(reinterpret_cast<const UT_Byte *>(buffer), length);
		return; //don't insert mathml character data
	}

	if (m_parseState == _PS_MetaData && !isPasting ())
		{
			m_Title.append(buffer, length);
			return;
		}

	if ((m_parseState == _PS_StyleSec) || (m_parseState == _PS_Init))
		{
			xxx_UT_DEBUGMSG(("IE_Imp_XHTML::charData wrong parseState %d  \n",m_parseState));
			return; // outside body here
		}

	/* No need to insert new blocks if we're just looking at the spaces
	 * between XML elements - unless we're in a <pre> sequence
	 */
	if (!m_bWhiteSignificant && (m_parseState != _PS_Block))
	{
#ifdef XHTML_UCS4
		UT_UCS4String buf(buffer,static_cast<size_t>(length),!m_bWhiteSignificant);
#else
		UT_UCS2String buf(buffer,static_cast<size_t>(length),!m_bWhiteSignificant);
#endif
		if (buf.size () == 0) return; // probably shouldn't happen; not sure
		if ((buf.size () == 1) && (buf[0] == UCS_SPACE)) return;
	}

	int failLine = 0;

	// bool bResetState = (m_parseState != _PS_Block);

	X_CheckError(requireBlock ());

	xxx_UT_DEBUGMSG(("Calling IE_Imp_XML::charData \n"));
	IE_Imp_XML::charData (buffer, length);

	// if (bResetState) m_parseState = _PS_Sec;

	return;

X_Fail:
	UT_DEBUGMSG (("X_Fail at %d\n", failLine));
	return;
}

FG_ConstGraphicPtr IE_Imp_XHTML::importDataURLImage (const gchar * szData)
{
	if (strncmp (szData, "image/", 6))
		{
			UT_DEBUGMSG(("importDataURLImage: URL-embedded data does not appear to be an image...\n"));
			return nullptr;
		}
	const char * b64bufptr = static_cast<const char *>(szData);

	while (*b64bufptr) if (*b64bufptr++ == ',') break;
	size_t b64length = static_cast<size_t>(strlen (b64bufptr));
	if (b64length == 0)
		{
			UT_DEBUGMSG(("importDataURLImage: URL-embedded data has no data?\n"));
			return nullptr;
		}

	size_t binmaxlen = ((b64length >> 2) + 1) * 3;
	size_t binlength = binmaxlen;
	char * binbuffer = static_cast<char *>(g_try_malloc (binmaxlen));
	if (binbuffer == nullptr)
		{
			UT_DEBUGMSG(("importDataURLImage: out of memory\n"));
			return nullptr;
		}
	char * binbufptr = binbuffer;

	if (!UT_UTF8_Base64Decode (binbufptr, binlength, b64bufptr, b64length))
		{
			UT_DEBUGMSG(("importDataURLImage: error decoding Base64 data - I assume that's what it is...\n"));
			FREEP(binbuffer);
			return nullptr;
		}
	binlength = binmaxlen - binlength;

	UT_ByteBufPtr pBB(new UT_ByteBuf);

	pBB->ins(0, reinterpret_cast<const UT_Byte *>(binbuffer), binlength);
	FREEP(binbuffer);

	FG_ConstGraphicPtr pfg;
	if (IE_ImpGraphic::loadGraphic (pBB, IEGFT_Unknown, pfg) != UT_OK || !pfg)
		{
			UT_DEBUGMSG(("unable to construct image importer!\n"));
			return nullptr;
		}
	UT_DEBUGMSG(("image loaded successfully\n"));

	return pfg;
}

FG_ConstGraphicPtr IE_Imp_XHTML::importImage (const gchar * szSrc)
{
	const char * szFile = static_cast<const char *>(szSrc);

	char * relative_file = UT_go_url_resolve_relative(m_szFileName, szFile);
	if(!relative_file)
		return nullptr;

	UT_DEBUGMSG(("found image reference (%s) - loading... \n", relative_file));

	FG_ConstGraphicPtr pfg;
	UT_Error err = IE_ImpGraphic::loadGraphic (relative_file, IEGFT_Unknown, pfg);

	if (err != UT_OK || !pfg)
		{
			UT_DEBUGMSG(("unable to import image\n"));
			g_free(relative_file);
			return nullptr;
		}

	g_free(relative_file);
	UT_DEBUGMSG(("image loaded successfully\n"));

	return pfg;
}

bool IE_Imp_XHTML::pushInline (const char * props)
{
	if (!requireBlock ()) return false;

	const PP_PropertyVector api_atts = {
		PT_PROPS_ATTRIBUTE_NAME, props
	};

	_pushInlineFmt(api_atts);
	return appendFmt(m_vecInlineFmt);
}

bool IE_Imp_XHTML::newBlock (const char * style_name, const char * css_style, const char * align)
{
	if (!requireSection ()) 
		{
			UT_return_val_if_fail(0,false);
		}


	UT_UTF8String * div_style = nullptr;
	if (m_divStyles.getItemCount ())
		div_style = m_divStyles.getLastItem ();

	UT_UTF8String style;
	if (div_style)
		style = *div_style;
	if (align)
		{
			if (!strcmp (align, "right"))
				style += "text-align: right; ";
			else if (!strcmp (align, "center"))
				style += "text-align: center; ";
			else if (!strcmp (align, "left"))
				style += "text-align: left; ";
			else if (!strcmp (align, "justify"))
				style += "text-align: justify; ";
		}
	if (css_style)
		style += css_style;

	UT_UTF8String utf8val = s_parseCSStyle (style, CSS_MASK_BLOCK);
	UT_DEBUGMSG(("CSS->Props (utf8val): [%s]\n",utf8val.utf8_str()));

	PP_PropertyVector api_atts = {
		PT_STYLE_ATTRIBUTE_NAME, style_name
	};

	if (utf8val.byteLength()) {
			api_atts.push_back(PT_PROPS_ATTRIBUTE_NAME);
			api_atts.push_back(utf8val.utf8_str());
	}
	if (!appendStrux (PTX_Block, api_atts))	{
			UT_return_val_if_fail(0,false);
	}
	m_bFirstBlock = true;
	m_parseState = _PS_Block;

	_data_NewBlock (); // warn XML charData() handler that a new block is beginning

	while (_getInlineDepth())
		_popInlineFmt ();

	utf8val = s_parseCSStyle (style, CSS_MASK_INLINE);
	UT_DEBUGMSG(("CSS->Props (utf8val): [%s]\n",utf8val.utf8_str()));

	return pushInline (utf8val.utf8_str ());
}

/* forces document into block state; returns false on failure
 */
bool IE_Imp_XHTML::requireBlock ()
{
	if (m_parseState == _PS_Block) return true;

	return m_bWhiteSignificant ? newBlock("Plain Text", nullptr, nullptr) : newBlock("Normal", nullptr, nullptr);
}

/* forces document into section state; returns false on failure
 */
bool IE_Imp_XHTML::requireSection ()
{
	if (m_parseState == _PS_Sec) return true;

	if (!appendStrux (PTX_Section, PP_NOPROPS))
		{
			UT_return_val_if_fail(0,false);
		}
	m_parseState = _PS_Sec;
	m_bFirstBlock = false;
	m_addedPTXSection = true;
	return true;
}

bool IE_Imp_XHTML::appendStrux(PTStruxType pts, const PP_PropertyVector & attributes)
{
	UT_DEBUGMSG(("XHTML Import - appendStruxStrux type %d document %p \n", pts, (void*)getDoc()));
	if(pts == PTX_Section)
	{
		m_bFirstBlock = false;
		m_addedPTXSection = true;
	}
	else if(pts == PTX_Block)
	{
		m_bFirstBlock = true;
	}
	if(!bInTable())
		{
			return getDoc()->appendStrux(pts, attributes);
		}
	else
		{
			return m_TableHelperStack->Block(pts, attributes);
		}
	return true;
}

bool IE_Imp_XHTML::appendFmt(const PP_PropertyVector & attributes)
{
	if(!m_addedPTXSection)
		{
			appendStrux(PTX_Section, PP_NOPROPS);
		}
	if(!m_bFirstBlock)
		{
			appendStrux(PTX_Block, PP_NOPROPS);
		}
	if(!bInTable())
		{
			return getDoc()->appendFmt(attributes);
		}
	else
		{
			return m_TableHelperStack->InlineFormat(attributes);
		}
	return true;
}

bool IE_Imp_XHTML::appendSpan(const UT_UCSChar * p, UT_uint32 length)
{
	if(!m_addedPTXSection)
		{
			appendStrux(PTX_Section, PP_NOPROPS);
		}
	if(!m_bFirstBlock)
		{
			appendStrux(PTX_Block, PP_NOPROPS);
		}

	if(!bInTable())
		{
			return getDoc()->appendSpan(p,length);
		}
	else
		{
			UT_DEBUGMSG(("Doing Inline length %d \n",length));
			return m_TableHelperStack->Inline(p, static_cast<UT_sint32>(length));
		}
	return true;
}


bool IE_Imp_XHTML::appendObject(PTObjectType pto, const PP_PropertyVector & attributes)
{
	if(!m_addedPTXSection)
		{
			appendStrux(PTX_Section, PP_NOPROPS);
		}
	if(!m_bFirstBlock)
		{
			appendStrux(PTX_Block, PP_NOPROPS);
		}
	if(!bInTable())
		{
			return getDoc()->appendObject(pto,attributes);
		}
	else
		{
			return m_TableHelperStack->Object(pto, attributes);
		}
	return true;
}



/*!
 * Returns true if we're in a table
 */
bool IE_Imp_XHTML::bInTable(void)
{
	return !(m_TableHelperStack->top() == nullptr);
}

/* returns true if child of a <div> with a recognized "class" attribute
 */
bool IE_Imp_XHTML::childOfSection ()
{
	bool bChild = false;
	UT_uint32 count = m_divClasses.getItemCount ();

	for (UT_uint32 i = 0; i < count; i++)
		if (m_divClasses[i])
			{
				bChild = true;
				break;
			}
	return bChild;
}

static void s_pass_whitespace (const char *& csstr)
{
	while (*csstr)
	{
		unsigned char u = static_cast<unsigned char>(*csstr);
		if (u & 0x80)
		{
			UT_UTF8Stringbuf::UCS4Char ucs4 = UT_UTF8Stringbuf::charCode (csstr);
#ifdef XHTML_UCS4
			if (UT_UCS4_isspace (ucs4))
#else
			if ((ucs4 & 0x7fff) || UT_UCS_isspace (static_cast<UT_UCSChar>(ucs4 & 0xffff)))
#endif
			{
				while (static_cast<unsigned char>(*++csstr) & 0x80) { }
				continue;
			}
		}
		else if (isspace (static_cast<int>(u)))
		{
			csstr++;
			continue;
		}
		break;
	}
}

static const char * s_pass_name (const char *& csstr)
{
	const char * name_end = csstr;

	while (*csstr)
	{
		unsigned char u = static_cast<unsigned char>(*csstr);
		if (u & 0x80)
		{
			UT_UTF8Stringbuf::UCS4Char ucs4 = UT_UTF8Stringbuf::charCode (csstr);
#ifndef XHTML_UCS4
			if ((ucs4 & 0x7fff) == 0)
#endif
#ifdef XHTML_UCS4
				if (UT_UCS4_isspace (ucs4))
#else
				if (UT_UCS_isspace (static_cast<UT_UCSChar>(ucs4 & 0xffff)))
#endif
				{
					name_end = csstr;
					break;
				}
			while (static_cast<unsigned char>(*++csstr) & 0x80) { }
			continue;
		}
		else if ((isspace (static_cast<int>(u))) || (*csstr == ':'))
		{
			name_end = csstr;
			break;
		}
		csstr++;
	}
	return name_end;
}

static const char * s_pass_value (const char *& csstr)
{
	const char * value_end = csstr;

	bool bQuoted = false;
	while (*csstr)
	{
		bool bSpace = false;
		unsigned char u = static_cast<unsigned char>(*csstr);
		if (u & 0x80)
		{
			UT_UTF8Stringbuf::UCS4Char ucs4 = UT_UTF8Stringbuf::charCode (csstr);
#ifdef XHTML_UCS4
			if (!bQuoted)
				if (UT_UCS4_isspace (ucs4))
#else
			if (!bQuoted && ((ucs4 & 0x7fff) == 0))
				if (UT_UCS_isspace (static_cast<UT_UCSChar>(ucs4 & 0xffff)))
#endif
				{
					bSpace = true;
					break;
				}
			while (static_cast<unsigned char>(*++csstr) & 0x80) { }
			if (!bSpace) value_end = csstr;
			continue;
		}
		else if ((*csstr == '\'') || (*csstr == '"'))
		{
			bQuoted = (bQuoted ? false : true);
		}
		else if (*csstr == ';')
		{
			if (!bQuoted)
			{
				csstr++;
				break;
			}
		}
		else if (!bQuoted && isspace (static_cast<int>(u))) bSpace = true;

		csstr++;
		if (!bSpace) value_end = csstr;
	}
	return value_end;
}

static bool s_pass_number (char *& ptr, bool & bIsPercent)
{
	while (*ptr)
		{
			if (*ptr != ' ') break;
			ptr++;
		}
	unsigned char u = static_cast<unsigned char>(*ptr);
	if (!isdigit (static_cast<int>(u))) return false;

	while (*ptr)
		{
			u = static_cast<unsigned char>(*ptr);
			if (!isdigit (static_cast<int>(u))) break;
			ptr++;
		}
	if (*ptr == '%')
		{
			bIsPercent = true;
			*ptr = ' ';
		}
	else if ((*ptr == ' ') || (*ptr == 0))
		{
			bIsPercent = false;
		}
	else return false;

	return true;
}

static unsigned char s_rgb_number (float f, bool bIsPercent)
{
	if (f < 0) return 0;

	if (bIsPercent) f *= 2.55f;

	if (f > 254.5) return 0xff;

	return (unsigned char) ((int) (f + 0.5));
}

static void s_props_append (UT_UTF8String & props, UT_uint32 css_mask,
							const char * name, char * value)
{
	UT_HashColor color;
	UT_UTF8String sLineHeight;

	const char * verbatim = nullptr;

	if (css_mask & CSS_MASK_INLINE)
		{
			if (strcmp (name, "font-weight") == 0)
				switch (*value)
					{
					case '1': case '2': case '3': case '4': case '5': case 'n': case 'l':
						if (props.byteLength ()) props += "; ";
						props += name;
						props += ":";
						props += "normal";
						break;
					case '6': case '7': case '8': case '9': case 'b':
						if (props.byteLength ()) props += "; ";
						props += name;
						props += ":";
						props += "bold";
						break;
					default: // inherit
						break;
					}
			else if (strcmp (name, "font-style") == 0)
				{
					if (strcmp (value, "normal") == 0)
						{
							if (props.byteLength ()) props += "; ";
							props += name;
							props += ":";
							props += "normal";
						}
					else if ((strcmp (value, "italic") == 0) ||
							 (strcmp (value, "oblique") == 0))
						{
							if (props.byteLength ()) props += "; ";
							props += name;
							props += ":";
							props += "italic";
						}
					// else inherit
				}
			else if((strcmp (name, "font-size")    == 0))
				{
					UT_Dimension siz_u = UT_determineDimension (value, DIM_none);
					if(siz_u != DIM_none)
					{
						double dval = UT_convertToPoints(value);
						verbatim =  UT_formatDimensionedValue(dval,"pt");
					}
					else
					{
						verbatim = "12pt";
					}
				}
			else if ((strcmp (name, "font-stretch") == 0) ||
					 (strcmp (name, "font-variant") == 0))
				{
					verbatim = value;
				}
			else if (strcmp (name, "font-family") == 0)
				{
					if ((strcmp (value, "serif")      == 0) ||
						(strcmp (value, "sans-serif") == 0) ||
						(strcmp (value, "cursive")    == 0) ||
						(strcmp (value, "fantasy")    == 0) ||
						(strcmp (value, "monospace")  == 0))
						{
							verbatim = value;
						}
					else if ((*value == '\'') || (*value == '"'))
						{
							/* CSS requires font-family names to be quoted, and also allows
							 * a sequence of these to be specified; AbiWord doesn't quote,
							 * and allows only one font-family name.
							 */
							char * value_end = ++value;
							while (*value_end)
								{
									if ((*value_end == '\'') || (*value_end == '"')) break;
									value_end++;
								}
							if (*value_end)
								{
									*value_end = 0;
									verbatim = value;
								}
						}
				}
			else if (strcmp (name, "text-decoration") == 0)
				{
					bool bInherit     = (strstr (value, "inherit")      != nullptr);
					bool bUnderline   = (strstr (value, "underline")    != nullptr);
					bool bLineThrough = (strstr (value, "line-through") != nullptr);
					bool bOverline    = (strstr (value, "overline")     != nullptr);

					if (bUnderline || bLineThrough || bOverline)
						{
							if (props.byteLength ()) props += "; ";
							props += name;
							props += ":";

							if (bUnderline) props += "underline";
							if (bLineThrough)
								{
									if (bUnderline) props += " ";
									props += "line-through";
								}
							if (bOverline)
								{
									if (bUnderline || bLineThrough) props += " ";
									props += "overline";
								}
						}
					else if (!bInherit)
						{
							if (props.byteLength ()) props += "; ";
							props += name;
							props += ":";
							props += "none";
						}
				}
			else if (strcmp (name, "vertical-align") == 0)
				{
					/* AbiWord uses "text-position" for CSS's "vertical-align" in the case
					 * of super-/subscripts.
					 */
					if (strcmp (value, "super") == 0)
						{
							static const char * text_position = "text-position";
							name = text_position;
							verbatim = "superscript";
						}
					if (strcmp (value, "sub") == 0)
						{
							static const char * text_position = "text-position";
							name = text_position;
							verbatim = "subscript";
						}
				}
			else if (strcmp (name, "display") == 0)
				{
					if (strcmp (value, "none") == 0)
						{
							if (props.byteLength ()) props += "; ";
							props += name;
							props += ":";
							props += value;
						}
				}
			else if ((strcmp (name, "color") == 0) || (strcmp (name, "background") == 0))
				{
					/* AbiWord uses rgb hex-sequence w/o the # prefix used by CSS
					 * and uses "bgcolor" instead of background
					 */
					static const char * bgcolor = "bgcolor";

					if (strcmp (name, "background") == 0) name = bgcolor;

					if (*value == '#')
						{
							value++;
							if (strlen (value) == 3)
							{
								unsigned int rgb;
								if (sscanf (value, "%x", &rgb) == 1)
								{
									unsigned int uir = (rgb & 0x0f00) >> 8;
									unsigned int uig = (rgb & 0x00f0) >> 4;
									unsigned int uib = (rgb & 0x000f);

									unsigned char r = static_cast<unsigned char>(uir|(uir<<4));
									unsigned char g = static_cast<unsigned char>(uig|(uig<<4));
									unsigned char b = static_cast<unsigned char>(uib|(uib<<4));

									verbatim = color.setColor (r, g, b);
								}
							}
							else if (strlen (value) == 6) verbatim = value;
						}
					else if (strncmp (value, "rgb(", 4) == 0)
						{
							value += 4;

							char * ptr = value;
							while (*ptr)
								{
									unsigned char u = static_cast<unsigned char>(*ptr);
									if ((isdigit (static_cast<int>(u))) || (*ptr == '%'))
										{
											ptr++;
											continue;
										}
									*ptr++ = ' ';
								}
							bool bValid = true;

							bool b1pc = false;
							bool b2pc = false;
							bool b3pc = false;

							ptr = value;
							if (bValid) bValid = s_pass_number (ptr, b1pc);
							if (bValid) bValid = s_pass_number (ptr, b2pc);
							if (bValid) bValid = s_pass_number (ptr, b3pc);

							if (bValid)
							{
								float fr;
								float fg;
								float fb;

								if (sscanf (value, "%f %f %f", &fr, &fg, &fb) == 3)
								{
									unsigned char r = s_rgb_number (fr, b1pc);
									unsigned char g = s_rgb_number (fg, b2pc);
									unsigned char b = s_rgb_number (fb, b3pc);

									verbatim = color.setColor (r, g, b);
								}
							}
						}
					else
						{
							verbatim = color.lookupNamedColor (value);
						}
				}
			else if (strcmp (name, "text-transform") == 0)
				{
					if ((strcmp (value, "none") == 0) ||
					    (strcmp (value, "capitalize") == 0) ||
					    (strcmp (value, "uppercase") == 0) ||
					    (strcmp (value, "lowercase") == 0))
					{
						verbatim = value;
					}
				}
		}
	if (css_mask & CSS_MASK_BLOCK)
		{
			/* potentially dangerous; TODO: check list-state??
			 */
			if ((strcmp (name, "margin-left")   == 0) ||
				(strcmp (name, "margin-right")  == 0) ||
				(strcmp (name, "margin-top")  == 0) ||
				(strcmp (name, "margin-bottom")  == 0) ||
				(strcmp (name, "text-align")    == 0) ||
				(strcmp (name, "text-indent")   == 0) ||
				(strcmp (name, "orphans")       == 0) ||
				(strcmp (name, "widows")        == 0))
				{
					verbatim = value;
				}
			else if (strcmp (name, "line-height") == 0)
				{
					// examples of handled values: line-height: 1.4,
					// line-height: 14pt, line-height: 140%

					double d = UT_convertDimensionless (value);

					if (d > 0)
					{
						UT_Dimension units = UT_determineDimension (value, DIM_none);

						if (units == DIM_none || units == DIM_PT)
						{
							verbatim = value;
						}
						else if (units == DIM_PERCENT)
						{
							UT_LocaleTransactor t(LC_NUMERIC, "C");
							sLineHeight = UT_UTF8String_sprintf("%.1f", d/100.0);
							verbatim = sLineHeight.utf8_str();
						}
					}
				}
		}
	if (css_mask & CSS_MASK_IMAGE)
		{
			if ((strcmp (name, "width")  == 0) ||
				(strcmp (name, "height") == 0))
				{
					UT_Dimension units = UT_determineDimension (value, DIM_PX);
					double d = UT_convertDimensionless (value);
					float dim = static_cast<float>(UT_convertDimensions (d, units, DIM_IN));
					std::string tmp;

					{
						UT_LocaleTransactor t(LC_NUMERIC, "C");
						tmp = UT_std_string_sprintf ("%gin", dim);
					}

					if (!tmp.empty ())
						{
							if (props.byteLength ()) props += "; ";
							props += name;
							props += ":";
							props += tmp.c_str ();
						}
				}
		}
	if (css_mask & CSS_MASK_BODY)
		{
			if ((strcmp (name, "margin-bottom")    == 0) ||
				(strcmp (name, "margin-top")       == 0) ||
				(strcmp (name, "background-color") == 0))
				{
					verbatim = value;
				}
		}

	if (verbatim)
		{
			if (props.byteLength ()) props += "; ";
			props += name;
			props += ":";
			props += verbatim;
		}
}

static UT_UTF8String s_parseCSStyle (const UT_UTF8String & style, UT_uint32 css_mask)
{
	UT_UTF8String props;

	const char * csstr = style.utf8_str ();
	while (*csstr)
	{
		s_pass_whitespace (csstr);

		const char * name_start = csstr;
		const char * name_end   = s_pass_name (csstr);

		if (*csstr == 0) break; // whatever we have, it's not a "name:value;" pair
		if (name_start == name_end) break; // ?? stray colon?

		s_pass_whitespace (csstr);
		if (*csstr != ':') break; // whatever we have, it's not a "name:value;" pair

		csstr++;
		s_pass_whitespace (csstr);

		if (*csstr == 0) break; // whatever we have, it's not a "name:value;" pair

		const char * value_start = csstr;
		const char * value_end   = s_pass_value (csstr);

		if (value_start == value_end) break; // ?? no value...

		/* unfortunately there's no easy way to turn these two sequences into strings :-(
		 * atm, anyway
		 */
		char * name = static_cast<char *>(g_try_malloc (name_end - name_start + 1));
		if (name)
		{
			strncpy (name, name_start, name_end - name_start);
			name[name_end - name_start] = 0;
		}
		char * value = static_cast<char *>(g_try_malloc (value_end - value_start + 1));
		if (value)
		{
			strncpy (value, value_start, value_end - value_start);
			value[value_end - value_start] = 0;
		}

		if (name && value) s_props_append (props, css_mask, name, value);

		FREEP (name);
		FREEP (value);
	}
	return props;
}
