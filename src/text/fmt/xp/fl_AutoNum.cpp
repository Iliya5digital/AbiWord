/* -*- mode: C++; tab-width: 4; c-basic-offset: 4; indent-tabs-mode: t; -*- */
/* AbiWord
 * Copyright (C) Luke Jordan, Martin Sevior.
 * BIDI Copyright (c) 2001,2002 Tomas Frydrych, Yaacov Akiba Slama
 * © 2016 Hubert Figuière
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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "fl_AutoNum.h"
#include "fl_Layout.h"
#include "fl_BlockLayout.h"
#include "fp_Run.h"
#include "fp_Line.h"
#include "fv_View.h"
#include "pd_Document.h"
#include "pt_PieceTable.h"
#include "pf_Frag.h"
#include "xap_App.h"
#include "xap_Frame.h"
#include "ut_std_string.h"
#include "ut_assert.h"
#include "ut_debugmsg.h"

#include "ut_string_class.h"

#define CONV_TO_UCS (UT_UCSChar) (unsigned char)

class pf_Frag;

void fl_AutoNum::ItemStorage::deleteNthItem(size_type ndx)
{
    pf_Frag_Strux* pItem = m_vec[ndx];
	m_set.erase(pItem);
    m_vec.erase(m_vec.begin() + ndx);
}

bool fl_AutoNum::ItemStorage::hasItem(const pf_Frag_Strux* pItem) const
{
	return m_set.find(pItem) != m_set.end();
}

void fl_AutoNum::ItemStorage::addItem(pf_Frag_Strux* pItem)
{
    m_vec.push_back(pItem);
    m_set.insert(pItem);
}

void fl_AutoNum::ItemStorage::insertItemAt(pf_Frag_Strux* pItem, size_type idx)
{
    m_vec.insert(m_vec.begin() + idx, pItem);
	m_set.insert(pItem);
}


fl_AutoNum::fl_AutoNum(	UT_uint32 id,
						UT_uint32 parent_id,
						FL_ListType lType,
						UT_uint32 start,
						const gchar * lDelim,
						const gchar * lDecimal,
						PD_Document * pDoc,
						FV_View * pView)
	:	m_pParent(nullptr),
		m_pDoc(pDoc),
		m_pView(pView),
		m_List_Type(lType),
		m_iID(id),
		m_iParentID(parent_id),
		m_iLevel(1),
		m_iStartValue(start),
		m_iAsciiOffset(0),
		m_bUpdatingItems(false),
		m_bDirty(false),
		m_ioffset(0),
		m_bWordMultiStyle(true),
		m_pParentItem(nullptr)
{
	// Set in Block???
	memset(m_pszDelim, 0, 80);
	memset(m_pszDecimal, 0, 80);
	UT_ASSERT(m_pDoc);
	if (lDelim)
		strncpy( m_pszDelim, lDelim, 80);

	if (lDecimal)
		strncpy( m_pszDecimal, lDecimal, 80);
	
	if(m_iParentID != 0)
	{
		_setParent(m_pDoc->getListByID(parent_id));
	}
}

// Because this is called indirectly from the constructor and
// all we need is a const ref, we don't use a fl_AutoNumConstPtr
bool fl_AutoNum::checkReference(const fl_AutoNum & pAuto) const
{
	if (&pAuto == m_pParent.get()) {
		return false;
	}
	if (m_pParent) {
		return m_pParent->checkReference(pAuto);
	}
	return true;
}

void fl_AutoNum::addItem(pf_Frag_Strux* pItem)
{
	if(m_items.hasItem(pItem)) {
		m_items.addItem(pItem);
		fixListOrder();
	}
	m_bDirty = true;
}

/*!
 * Check that this list is not a member of another list.
 */
void fl_AutoNum::fixHierarchy(void)
{
	fl_AutoNumPtr pParent;
	const char * pszParentID =nullptr;
#if 1
	UT_uint32 docParentID = 0;
	if (!m_items.empty())
	{
		pf_Frag_Strux* sdh = m_items.front();
		XAP_Frame * pFrame = XAP_App::getApp()->getLastFocussedFrame();
		FV_View* pView = nullptr;
		if (pFrame)
			pView = static_cast<FV_View *>(pFrame->getCurrentView());

		bool bFound = m_pDoc->getAttributeFromSDH(sdh,
												  pView ? pView->isShowRevisions() : true,
												  pView ? pView->getRevisionLevel(): PD_MAX_REVISION,
												  PT_PARENTID_ATTRIBUTE_NAME,&pszParentID);
		if(bFound)
		{
			docParentID= atoi(pszParentID);
		}
	}
	if((m_iID != 0) && (docParentID != 0) &&  (docParentID != m_iParentID) && (docParentID != m_iID))
	{
		pParent = m_pDoc->getListByID(docParentID);
		UT_ASSERT(this != pParent.get());
		if (pParent)	{
			m_iParentID = docParentID;
			m_bDirty = true;
		}
	}
#endif
	if (m_iParentID != 0) {
		pParent = m_pDoc->getListByID(m_iParentID);
	} else {
		// TODO Add error checking?
		pParent.reset();
	}

	if(pParent != m_pParent)
	{
		_setParent(pParent);
	}
	UT_uint32 oldlevel = m_iLevel;
	if (m_pParent)
		m_iLevel = m_pParent->getLevel() + 1;
	else
		m_iLevel = 1;
	if(oldlevel != m_iLevel)
	{
		m_bDirty = true;
	}
}

fl_AutoNum::~fl_AutoNum()
{
}

static bool compareListItems(pf_Frag_Strux* const & p1,  pf_Frag_Strux* const & p2,
                             const PD_Document * pCurDoc)
{
	PT_DocPosition pos1 = pCurDoc->getStruxPosition(p1);
	PT_DocPosition pos2 = pCurDoc->getStruxPosition(p2);
	return (pos1 < pos2);
}

void    fl_AutoNum::fixListOrder(void)
{
	using namespace std::placeholders;
	m_items.sort(std::bind(&compareListItems, _1, _2, m_pDoc));
	m_bDirty = true;
}

void    fl_AutoNum::markAsDirty(void)
{
	m_bDirty = true;
}

void    fl_AutoNum::findAndSetParentItem(void)
{
	if(m_iParentID == 0)
	{
		return;
	}
	else if (!m_pParent)
	{
		_setParent(m_pDoc->getListByID(m_iParentID));
	}
	else
	{
		fl_AutoNumPtr pParent = m_pDoc->getListByID(m_iParentID);
		if (!pParent) {
			_setParent(fl_AutoNumPtr());
		}
	}

//	pCurDoc = m_pDoc;
//   	fixListOrder();
//   	m_pParent->fixListOrder();
//   	m_pParent->update(0);

	if (m_items.empty())
	{
		return;
	}
	pf_Frag_Strux* pCurFirst =  m_items.front();
	if(pCurFirst == nullptr)
		return;
	PT_DocPosition posCur = m_pDoc->getStruxPosition(pCurFirst);
	PT_DocPosition posParent = 0;
	UT_uint32 cnt = m_pDoc->getListsCount();
	UT_ASSERT(cnt!=0);
	UT_uint32 iList;
	fl_AutoNumPtr pClosestAuto;
	PT_DocPosition posClosest = 0;
	pf_Frag_Strux* pClosestItem = nullptr;
	bool bReparent = false;
	if(m_pParent != nullptr)
	{
		UT_uint32 i=0;
		for(i=0; i <m_pParent->getNumLabels(); i++)
		{
			pf_Frag_Strux* pParentItem = m_pParent->getNthBlock(i);
			if(pParentItem != nullptr)
			{
				posParent = m_pDoc->getStruxPosition(pParentItem);
				if( posParent > posClosest && posParent < posCur)
				{
					posClosest = posParent;
					pClosestAuto = m_pParent;
					pClosestItem = pParentItem;
					bReparent = true;
				}
			}
		}
	}
//
// Reparent this list if the first item of the parent is after the first
// item of this list.
//
	if((m_pParent == nullptr) || (posClosest == 0))
	{
		for(iList = 0; iList < cnt; iList++)
		{
			fl_AutoNumPtr pParent = m_pDoc->getNthList(iList);
			UT_uint32 i=0;
			pf_Frag_Strux* pParentItem = pParent->getNthBlock(i);
			posParent=0;
			if(pParentItem != nullptr)
			{
				posParent = m_pDoc->getStruxPosition(pParentItem);
			}
			while(pParentItem != nullptr && (posParent < posCur))
			{
				i++;
				pParentItem = pParent->getNthBlock(i);
				if(pParentItem != nullptr)
				{
					posParent = m_pDoc->getStruxPosition(pParentItem);
				}
			}
			if( i > 0)
			{
				i--;
				pParentItem = pParent->getNthBlock(i);
				posParent = m_pDoc->getStruxPosition(pParentItem);
				if( posParent > posClosest)
				{
					posClosest = posParent;
					pClosestAuto = pParent;
					pClosestItem = pParentItem;
					bReparent = true;
				}
			}
		}
	}
	if(m_pParentItem != pClosestItem)
		m_bDirty = true;
	if(m_pParent != pClosestAuto)
		m_bDirty = true;
	if(bReparent)
	{
		m_pParentItem = pClosestItem;
		if(m_pParent != pClosestAuto)
		{
			_setParent(pClosestAuto);
			_setParentID(m_pParent->getID());
		}
	}
	if(m_pParent != nullptr)
	{
		m_iLevel = m_pParent->getLevel()+ 1;
		//
		// TODO: change all the para attributes in the list to reflect
		// this change of Parent ID and Level.
	}
	else
	{
		//	        m_iParentID = 0;
		m_iLevel = 1;
	}
	if(m_bDirty == true)
		update(0);
}

/*!
 * This method recursively calculates a label based on the type of label
 * of the AutoNum Class. This is output to the label string labelStr.
 *
 * insPoint is the position in the string where the new text goes. It starts
 * Pointing to the byte == 0
 * depth is the level of recursion
 * pLayout is a pointer to the Layout item containing the current list item
 */
void    fl_AutoNum::_getLabelstr( UT_UCSChar labelStr[], UT_uint32 * insPoint,
								  UT_uint32 depth, const pf_Frag_Strux* pLayout) const
{
	// Keep these arrays the same length to prevent overflows; see Bug 10580
	char p[100], leftDelim[100], rightDelim[100];
	UT_uint32 i,psz;
	//
	// Don't get the next level if we don't have a list
	//
	if(m_List_Type == NOT_A_LIST)
	{
		*insPoint = 0;
		return;
	}
//	if(depth > 0 && m_List_Type >= BULLETED_LIST)
//	{
//		*insPoint = 0;
//		return;
//	}

	// TODO This is a bit of a hack to split the delim string. It would be
	// TODO nice to clear it up.

	strncpy (p, m_pszDelim, sizeof(p));
	UT_uint32 rTmp;

	i = 0;

	while (((i + 1) < G_N_ELEMENTS(p)) && p[i] && p[i] != '%' && p[i+1] != 'L')
	{
		// FIXME check the bounds to not overflow leftDelim
		// Update: the arrays are now the same length to prevent overflows
		leftDelim[i] = p[i];
		i++;
	}
	if (i >= G_N_ELEMENTS(p) || p[i] == '\0') {
//		UT_ASSERT(UT_NOT_REACHED);
		UT_DEBUGMSG (("Hub: not a delim (SHOULD NOT HAPPEN)!!!\n"));
		*insPoint = 0;
		return;
	}
	leftDelim[i] = '\0';
	i += 2;
	rTmp = i;
	while ((i < G_N_ELEMENTS(p)) && p[i])
	{
		// FIXME check the bounds to not overflow rightDelim
		// Update: the arrays are now the same length to prevent overflows
		rightDelim[i - rTmp] = p[i];
		i++;
	}
	rightDelim[i - rTmp] = '\0';

	if(m_pParent != nullptr  && m_List_Type < BULLETED_LIST)
	{
		m_pParent->_getLabelstr( labelStr, insPoint, depth+1,getParentItem());
		if(*insPoint != 0)
		{
			psz = strlen(m_pszDecimal);
			for(i=0; i<=psz;i++)
			{
				labelStr[(*insPoint)++] = CONV_TO_UCS m_pszDecimal[i];
			}
			(*insPoint)--;
		}
	}

	UT_sint32 place = getPositionInList(pLayout);
	if (place == -1)
	{
		//	       UT_ASSERT(UT_SHOULD_NOT_HAPPEN);
		labelStr[0] = 0;
		(*insPoint) = 0;
		return;
	}
	place += m_iStartValue;

	//	if (depth == 0 )
	if(IS_NUMBERED_LIST_TYPE(m_List_Type))
	{
		psz = strlen(leftDelim);
		char *pSrc = leftDelim;
		char *pLim = leftDelim + psz;
		while (pSrc < pLim)
		{
			UT_UCS4Char ch = g_utf8_get_char_validated(pSrc,pLim-pSrc);
			if (((signed)ch) < 0) ch=UCS_REPLACECHAR;
			labelStr[(*insPoint)++] = ch;
			pSrc = g_utf8_next_char(pSrc);
		}
	}
	switch( m_List_Type)
	{
	case NUMBERED_LIST:
		sprintf(p,"%i",place);
		psz = strlen( p);
		for(i=0; i<psz; i++)
		{
			labelStr[(*insPoint)++] =  CONV_TO_UCS p[i];
		}
		break;

	case UPPERCASE_LIST:
	{
		char * val = dec2ascii(place - 1, 65);
		sprintf(p,"%s",val);
		FREEP(val);
		psz = strlen( p);
		for(i=0; i<psz; i++)
		{
			labelStr[(*insPoint)++] =  CONV_TO_UCS p[i];
		}
		break;
	}

	case LOWERCASE_LIST:
	{
		char * val = dec2ascii(place - 1, 97);
		sprintf(p,"%s",val);
		FREEP(val);
		psz = strlen( p);
		for(i=0; i<psz; i++)
		{
			labelStr[(*insPoint)++] =  CONV_TO_UCS p[i];
		}
		break;
	}

	case UPPERROMAN_LIST:
	{
		char * val = dec2roman(place,false);
		sprintf(p,"%s",val);
		FREEP(val);
		psz = strlen( p);
		for(i=0; i<psz; i++)
		{
			labelStr[(*insPoint)++] =  CONV_TO_UCS p[i];
		}
		break;
	}

	case LOWERROMAN_LIST:
	{
		char * val = dec2roman(place,true);
		sprintf(p,"%s",val);
		FREEP(val);
		psz = strlen( p);
		for(i=0; i<psz; i++)
		{
			labelStr[(*insPoint)++] =  CONV_TO_UCS p[i];
		}
		break;
	}

	case ARABICNUMBERED_LIST:
		sprintf(p,"%i",place);
		psz = strlen( p);
		for(i=0; i<psz; i++)
		{
			labelStr[(*insPoint)++] =  (CONV_TO_UCS p[i]) + 0x0660 - (CONV_TO_UCS '0');
		}
		break;

	case HEBREW_LIST:
		dec2hebrew(labelStr,insPoint,place);
		break;

	case BULLETED_LIST:
		labelStr[(*insPoint)++] =  0x2022;
		break;

	case DASHED_LIST:
		labelStr[(*insPoint)++] =  0x002D;
		break;

	case SQUARE_LIST:
		labelStr[(*insPoint)++] =  0x25A0;
		break;

	case TRIANGLE_LIST:
		labelStr[(*insPoint)++] =  0x25B2;
		break;

	case DIAMOND_LIST:
		labelStr[(*insPoint)++] =  0x2666;
		break;

	case STAR_LIST:
		labelStr[(*insPoint)++] =  0x2733;
		break;

	case IMPLIES_LIST:
		labelStr[(*insPoint)++] =  0x21D2;
		break;

	case TICK_LIST:
		labelStr[(*insPoint)++] =  0x2713;
		break;

	case BOX_LIST:
		labelStr[(*insPoint)++] =  0x2752;
		break;

	case HAND_LIST:
		labelStr[(*insPoint)++] =  0x261E;
		break;

	case HEART_LIST:
		labelStr[(*insPoint)++] =  0x2665;
		break;

	case ARROWHEAD_LIST:
		labelStr[(*insPoint)++] =  0x27A3;
		break;

	default:
		UT_ASSERT_HARMLESS(UT_SHOULD_NOT_HAPPEN);
		break;
	}
	
	if( m_List_Type < BULLETED_LIST &&
	    (g_ascii_strncasecmp(m_pszDecimal,rightDelim,4) != 0 || depth == 0) )
	{
		psz = strlen(rightDelim);
		char *pSrc = rightDelim;
		char *pLim = rightDelim + psz;
		while (pSrc < pLim)
		{
			UT_UCS4Char ch = g_utf8_get_char_validated(pSrc,pLim-pSrc);
			if (((signed)ch) < 0) ch=UCS_REPLACECHAR;
			labelStr[(*insPoint)++] = ch;
			pSrc = g_utf8_next_char(pSrc);
		}
	}
	labelStr[(*insPoint)] = 0;
	return;
}

const UT_UCSChar * fl_AutoNum::getLabel(const pf_Frag_Strux* pItem)  const
{
	static UT_UCSChar label[100];
	UT_uint32 insPoint = 0;
	UT_uint32 depth = 0;
	_getLabelstr( label, &insPoint, depth , pItem);
	if(insPoint == 0 )
	{
		return static_cast<const UT_UCSChar *>(nullptr);
	}
	else
	{
		return static_cast<const UT_UCSChar *>(label);
	}
}

UT_uint32 fl_AutoNum::getValue(const pf_Frag_Strux* pItem) const
{
	return getPositionInList(pItem) + m_iStartValue;
}


void fl_AutoNum::setListType(FL_ListType lType)
{
	m_List_Type = lType;
}

bool fl_AutoNum::isDirty() const
{
	return m_bDirty;
}

void fl_AutoNum::setDelim(const gchar * lDelim)
{
	strncpy( m_pszDelim, lDelim, 80);
	m_pszDelim[80] = 0;
	m_bDirty = true;
}

const gchar * fl_AutoNum::getDelim() const
{
	return m_pszDelim;
}


const gchar * fl_AutoNum::getDecimal() const
{
	return m_pszDecimal;
}

void fl_AutoNum::setDecimal(const gchar * lDecimal)
{
	strncpy( m_pszDecimal, lDecimal, 80);
	m_bDirty = true;
}

FL_ListType fl_AutoNum::getType() const
{
	return m_List_Type;
}

void fl_AutoNum::setStartValue(UT_uint32 start)
{
	m_iStartValue = start;
	m_bDirty = true;
	_updateItems(0, nullptr);
}

void fl_AutoNum::setAsciiOffset(UT_uint32 new_asciioffset)
{
	m_iAsciiOffset = static_cast<UT_uint16>(new_asciioffset);
	m_bDirty = true;
}

UT_uint32 fl_AutoNum::getStartValue32() const
{
	return m_iStartValue;
}

void fl_AutoNum::insertFirstItem(pf_Frag_Strux* pItem, pf_Frag_Strux* pLast, bool bDoFix)
{
	if(!m_items.hasItem(pItem))
	{
		m_items.insertItemAt(pItem, 0);
		m_bDirty = true;
	}
	if(bDoFix)
	{
		fixListOrder();
	}
	if (m_pParent)
	{
		m_pParentItem = pLast;
		m_bDirty = true;
	}
	if(m_pDoc->areListUpdatesAllowed() == false)
		return;
	if (getAutoNumFromSdh(pItem).get() == this) {
		_updateItems(0, nullptr);
	}
}

pf_Frag_Strux* fl_AutoNum::getParentItem() const
{
	return m_pParentItem;
}


void fl_AutoNum::setParentItem(pf_Frag_Strux* pItem)
{
	m_pParentItem = pItem;
	m_bDirty = true;
}

void fl_AutoNum::insertItem(pf_Frag_Strux* pItem, const pf_Frag_Strux* pPrev, bool bDoFix)
{
	UT_sint32 ndx, i;
	UT_ASSERT(pItem);
	if(m_items.hasItem(pItem)) {
		return;
	}
	m_bDirty = true;
	ndx = m_items.findItem(pPrev) + 1;
	m_items.insertItemAt(pItem, ndx);
	if(bDoFix)
	{
		fixListOrder();
	}
	if(m_pDoc->areListUpdatesAllowed() == false)
		return;

	// scan through all the lists and update parent pointers

	UT_sint32 numlists = m_pDoc->getListsCount();
	for(i=0; i<numlists; i++)
	{
		fl_AutoNumPtr pAuto = m_pDoc->getNthList(i);
		if( pPrev == pAuto->getParentItem())
		{
			pAuto->setParentItem(pItem);
			pAuto->m_bDirty = true;
			if(!pAuto->_updateItems(0, nullptr)) {
				return;
			}
		}
	}

	_updateItems(ndx + 1, nullptr);
}


void fl_AutoNum::prependItem(pf_Frag_Strux* pItem, const pf_Frag_Strux* pNext, bool bDoFix)
{
	UT_sint32 ndx, i;
	UT_ASSERT(pItem);
	pf_Frag_Strux* pPrev = nullptr;
	if(m_items.hasItem(pItem)) {
		return;
	}
	m_bDirty = true;
	ndx = m_items.findItem(pNext);
	if(ndx > 0)
	{
		pPrev = m_items.at(ndx - 1);
	}
	m_items.insertItemAt(pItem, ndx);
	if(bDoFix)
		fixListOrder(); // safety!!
	if(m_pDoc->areListUpdatesAllowed() == false)
		return;
	if(pPrev != nullptr)
	{
		// scan through all the lists and update parent pointers

		UT_sint32 numlists = m_pDoc->getListsCount();
		for(i=0; i<numlists; i++)
		{
			fl_AutoNumPtr pAuto = m_pDoc->getNthList(i);
			if( pPrev == pAuto->getParentItem())
			{
				pAuto->setParentItem(pItem);
				pAuto->m_bDirty = true;
				if(	pAuto->_updateItems(0, nullptr)) {
					return;
				}
			}
		}
	}
	_updateItems(ndx, nullptr);
}

void fl_AutoNum::removeItem(const pf_Frag_Strux* pItem)
{
	UT_sint32 ndx = m_items.findItem(pItem);
	UT_sint32 i;
	//
	// For multi-views we might have already deleted pItem from the
	// fl_AutoNum
	//
	//
	UT_ASSERT(ndx != -1);
	if(ndx < 0 )
	{
		m_bDirty = true;
		_updateItems(0, nullptr);
		return;
	}
	pf_Frag_Strux* ppItem = nullptr;
	if(ndx > 0)
	{
		ppItem =  m_items.at(ndx - 1);
	}
	m_items.deleteNthItem(ndx);
	m_bDirty = true;
	// scan through all the lists and update parent pointers

	UT_sint32 numlists = m_pDoc->getListsCount();
	for(i=0; i<numlists; i++)
	{
		fl_AutoNumPtr pAuto = m_pDoc->getNthList(i);
		if( pItem == pAuto->getParentItem())
		{
			pAuto->setParentItem(ppItem);
			if(ppItem == nullptr)
			{
				UT_uint32 level = pAuto->getLevel();
				if(level > 0)
				{
					level = level - 1;
				}
				else
				{
					UT_ASSERT_HARMLESS(UT_SHOULD_NOT_HAPPEN);
				}
				pAuto->setLevel(level);
				pAuto->_setParent(getParent());
				pAuto->m_bDirty = true;
				pAuto->setParentItem(getParentItem());
			}
			if(m_pDoc->areListUpdatesAllowed() == true) {
				if(!pAuto->_updateItems(0, nullptr)) {
					return;
				}
			}
		}
	}
	_updateItems(ndx, nullptr);
}

UT_uint32 fl_AutoNum::getNumLabels() const
{
	return m_items.size();
}

UT_sint32 fl_AutoNum::getPositionInList(const pf_Frag_Strux* pItem) const
{
	pf_Frag_Strux* pTmp;
	UT_uint32 ndx = 0;
	auto count = m_items.size();
	bool bOnLevel = true;
	bool bFirstItem = false;

	for (UT_uint32 i = 0; i < count; i++)
	{
		pTmp = m_items.at(i);
		fl_AutoNumConstPtr pAuto = getAutoNumFromSdh(pItem);
		bOnLevel = (pAuto.get() == this);
		bFirstItem = (pTmp == m_items.front());
		if (pTmp == pItem)
		{
			if (m_bWordMultiStyle && !bOnLevel && !bFirstItem)
				ndx--;
			return ndx;
		}
		else if (!m_bWordMultiStyle || bOnLevel || bFirstItem)
		{
			ndx++;
		}
	}

	return -1;
	// return m_items.findItem(pItem);
}

fl_AutoNumConstPtr fl_AutoNum::getAutoNumFromSdh(const pf_Frag_Strux* sdh) const
{
	UT_sint32 i;
	fl_AutoNumConstPtr pAuto;

	if (m_pDoc->areListUpdatesAllowed() == false)
	{
		if (!isItem(sdh)) {
			return fl_AutoNumConstPtr();
		}

		return shared_from_this();
	}

	UT_sint32 numLists = m_pDoc->getListsCount();
	for (i = 0; i < numLists; ++i)
	{
		pAuto = m_pDoc->getNthList(i);
		if (pAuto->isItem(sdh))
			break;
	}

	if (i >= numLists)
		return fl_AutoNumConstPtr();

	return pAuto;
}

pf_Frag_Strux* fl_AutoNum::getLastItemInHeiracy(void) const
{
       fl_AutoNumConstPtr pAuto = shared_from_this();
       pf_Frag_Strux*  pLastItem = nullptr;
       bool bLoop = true;
       fl_AutoNumPtr pNext;
       UT_uint32 numLists = m_pDoc->getListsCount();
       UT_uint32 i=0;
       while(bLoop)
       {
	     pLastItem = pAuto->getLastItem();
	     for(i=0; i<numLists; i++)
	     {
	          pNext = m_pDoc->getNthList(i);
		  if(pNext->isItem(pLastItem) && pNext->getLevel() > pAuto->getLevel())
		  {
		       pAuto = pNext;
		       break;
		  }
	     }
	     if(i >= numLists)
	     {
	           bLoop = false;
	     }
       }
       return pLastItem;
}

bool fl_AutoNum::isItem(const pf_Frag_Strux* pItem) const
{
	return m_items.hasItem(pItem);
}

bool fl_AutoNum::isEmpty() const
{
	return m_items.empty();
}

pf_Frag_Strux* fl_AutoNum::getFirstItem() const
{
	return (!m_items.empty() ? m_items.front() : nullptr);
}


pf_Frag_Strux* fl_AutoNum::getLastItem() const
{
	if (m_items.empty()) {
		return nullptr;
	}
	return m_items.back();
}

bool fl_AutoNum::doesItemHaveLabel(const fl_BlockLayout * pItem) const
{
	fp_Run * pRun = pItem->getFirstRun();
	bool bStop = false;
	while(bStop == false)
	{
		if(pRun->getType() == FPRUN_FIELD)
		{
			fp_FieldRun * pFRun = static_cast<fp_FieldRun *>(pRun);
			if(pFRun->getFieldType() == FPFIELD_list_label)
			{
				bStop = true;
				return true;
			}
		}
		pRun = pRun->getNextRun();
		if(pRun == nullptr)
		{
			bStop = true;
			return false;
		}
	}
	return false;
}


bool fl_AutoNum::isLastOnLevel(const pf_Frag_Strux* pItem) const
{
	if (m_items.empty()) {
		return false;
	}
	return m_items.back() == pItem;
}

fl_AutoNumPtr fl_AutoNum::getActiveParent(void) const
{
	fl_AutoNumPtr pAutoNum = m_pParent;

	while (pAutoNum && pAutoNum->isEmpty())
		pAutoNum = pAutoNum->getParent();

	return pAutoNum;
}

/*!
 * This method returns true if the requested ID is somewhere in this
 * List heiracy.
 */
bool fl_AutoNum::isIDSomeWhere(UT_uint32 ID) const
{
	fl_AutoNumConstPtr pAuto = shared_from_this();
	while (pAuto) {
		if(pAuto->getID() == ID)
		{
			return true;
		}
		pAuto = pAuto->getParent();
	}
	return false;
}

void fl_AutoNum::_setParent(const fl_AutoNumPtr & pParent)
{
	if (pParent.get() == this)
	{
		m_pParent = nullptr;
		m_iParentID = 0;
		m_bDirty = true;
		return;
	}
	if(pParent != m_pParent)
	{
		char szParent[13];
		m_pParent = pParent;
		if (m_pParent) {
			if (!pParent->checkReference(*this)) {
				m_pParent = nullptr;
				m_iParentID = 0;
				m_bDirty = true;
				return;
			}
			m_iParentID  = pParent->getID();
		}
		else
		{
			m_iParentID  = 0;
		}

		sprintf(szParent,"%d",m_iParentID);
		m_bDirty = true;
		for (ItemStorage::size_type i = 0; i < m_items.size(); i++)
		{
			pf_Frag_Strux* sdh = m_items.at(i);
			m_pDoc->changeStruxForLists(sdh, static_cast<const char *>(szParent));
		}
	}
}


void fl_AutoNum::_setParentID(UT_uint32 iParentID)
{
	m_bDirty = true;
	m_iParentID = iParentID;
}

void fl_AutoNum::update(UT_uint32 start)
{
	xxx_UT_DEBUGMSG(("Updating List %d  There are %d items here \n",m_iID,m_items.getItemCount()));
	if (isUpdating())
		return;
	//_calculateLabelStr(0);
	if(!_updateItems(start, nullptr))
		return;
	pf_Frag_Strux* sdh = getFirstItem();
	UT_return_if_fail(sdh);

	if (m_pParent && !m_pParent->isUpdating())
	{
		UT_uint32 ndx = m_pParent->m_items.findItem(sdh);
		m_pParent->update(ndx + 1);
	}
}

bool fl_AutoNum::_updateItems(UT_sint32 start, const pf_Frag_Strux* notMe)
{
	//	UT_DEBUGMSG(("Entering _updateItems\n"));
	UT_sint32 j;
	UT_return_val_if_fail(m_pDoc,false);
	if(m_pDoc->areListUpdatesAllowed() == true)
	{
		//if(start == 0)
		//	{
		//	pCurDoc = m_pDoc;
		//	fixListOrder();
		//	}
		UT_sint32 numlists = m_pDoc->getListsCount();
		m_bUpdatingItems = true;
		for (ItemStorage::size_type i = start; i < m_items.size(); i++)
		{
			//       	 	UT_DEBUGMSG(("Entering _updateItems for loop\n"));
			pf_Frag_Strux* pTmp = m_items.at(i);
			UT_ASSERT(pTmp);
			m_pDoc->listUpdate(pTmp);

			// scan through all the lists and update child lists if connected to this item

			pf_Frag_Strux* pItem = m_items.at(i);
			for(j=0; j<numlists; j++)
			{
				fl_AutoNumPtr pAuto = m_pDoc->getNthList(j);
				UT_ASSERT_HARMLESS(pAuto);
				if( pAuto && (pItem == pAuto->getParentItem()) && (pItem != notMe))
				{
					if(!pAuto->_updateItems(0, pItem)) {
						return false;
					}
				}
			}
		}
		m_bUpdatingItems = false;
		m_bDirty = false;
	}
	return true;
}

///
/// Returns true if item is contained or immediately adjacent to the list
///
bool fl_AutoNum::isContainedByList(pf_Frag_Strux* pItem) const
{
	pf_Frag_Strux *sdh, *sdh_prev, *sdh_next;
	PT_DocPosition pos_prev,pos_next,pos;
	bool bret;
	if (m_items.empty()) {
		return false;
	}
	sdh = m_items.front();
	bret = m_pDoc->getPrevStruxOfType(sdh,PTX_Block, &sdh_prev);
	if(bret == false)
		sdh_prev = sdh;
	pos_prev = m_pDoc->getStruxPosition(sdh_prev);
	sdh = m_items.back();
	bret = m_pDoc->getNextStruxOfType(sdh,PTX_Block, &sdh_next);
	if(bret == false)
		sdh_next = sdh;
	pos_next = m_pDoc->getStruxPosition(sdh_next);
	pos =  m_pDoc->getStruxPosition(pItem);
	if((pos >= pos_prev) && (pos <= pos_next))
		return true;
	return false;
}


pf_Frag_Strux* fl_AutoNum::getNthBlock( UT_uint32 list_num) const
{
	if(static_cast<ItemStorage::size_type>(list_num) >= m_items.size()) {
		return nullptr;
	}
	return m_items.at(list_num);
}

pf_Frag_Strux* fl_AutoNum::getPrevInList(const pf_Frag_Strux* pItem) const
{
	UT_sint32 itemloc = m_items.findItem(pItem);
	if (itemloc == -1 || itemloc == 0)
		return nullptr;
	return m_items.at(static_cast<UT_uint32>(itemloc) - 1);
}

inline UT_uint32 fl_AutoNum::_getLevelValue(const fl_AutoNumConstPtr & pAutoNum) const
{
	const pf_Frag_Strux* pBlock = getFirstItem();
	fl_AutoNumConstPtr pCurr = shared_from_this();

	while (1)
	{
		if (pAutoNum->isItem(pBlock))
		{
			break;
		}
		else
		{
			pCurr = pCurr->getParent();
			pBlock = pCurr->getFirstItem();
		}
	}

	return pAutoNum->getValue(pBlock);
}

char *  fl_AutoNum::dec2roman(UT_sint32 value, bool lower)
{
	UT_String roman;

	while( value >= 1000 )
	{
		roman += "M";
		value -= 1000;
	}
	if( value >= 900 )
	{
		roman += "CM";
		value -= 900;
	}
	while( value >= 500 )
	{
		roman += "D";
		value -= 500;
	}
	if( value >= 400 )
	{
		roman += "CD";
		value -= 400;
	}
	while( value >= 100 )
	{
		roman += "C";
		value -= 100;
	}
	if( value >= 90 )
	{
		roman += "XC";
		value -= 90;
	}
	while( value >= 50 )
	{
		roman += "L";
		value -= 50;
	}
	if( value >= 40 )
	{
		roman += "XL";
		value -= 40;
	}
	while( value >= 10 )
	{
		roman += "X";
		value -= 10;
	}
	if( value >= 9 )
	{
		roman += "IX";
		value -= 9;
	}
	while( value >= 5 )
	{
		roman += "V";
		value -= 5;
	}
	if( value >= 4 )
	{
		roman += "IV";
		value -= 4;
	}
	while( value > 0 )
	{
		roman += "I";
		value--;
	}

	char * rmn = g_strdup (roman.c_str());

	if (lower == true)
	{
		int len;
		len = roman.size();
		while (--len >= 0)
		{
			UT_sint32 r = static_cast<UT_sint32>(roman[len]);
			if( (r >= (UT_sint32) 'A') && (r <= (UT_sint32) 'Z'))
				r = r + 32;
			rmn[len] = static_cast<char>(r);
		}
	}

	return rmn;
}

char * fl_AutoNum::dec2ascii(UT_sint32 value, UT_uint32 offset)
{
	char ascii[30];
	UT_uint32 ndx, count, i;

	ascii[0] = '\0';
	ndx = abs(value % 26);
	count = abs(value / 26);

	// For now, we do this like Word. A preference would be nice.
	for (i = 0; i <= count; i++)
	{
		ascii[i] = static_cast<char>(ndx + offset);
	}
	ascii[i] = '\0';

	return g_strdup(ascii);
}

void fl_AutoNum::dec2hebrew(UT_UCSChar labelStr[], UT_uint32 * insPoint, UT_sint32 value)
{
	UT_UCSChar gHebrewDigit[22] =
	{
		//   1       2       3       4       5       6       7       8       9
		0x05D0, 0x05D1, 0x05D2, 0x05D3, 0x05D4, 0x05D5, 0x05D6, 0x05D7, 0x05D8,
		//  10      20      30      40      50      60      70      80      90
		0x05D9, 0x05DB, 0x05DC, 0x05DE, 0x05E0, 0x05E1, 0x05E2, 0x05E4, 0x05E6,
		// 100     200     300     400
		0x05E7, 0x05E8, 0x05E9, 0x05EA
	};

	bool outputSep = false;
	UT_UCSChar digit;
	do
 	{
		UT_sint32 n3 = value % 1000;

		if(outputSep)
			labelStr[(*insPoint)++] = 0x0020; // output thousand seperator
		outputSep = ( n3 > 0); // request to output thousand seperator next time.

		// Process digit for 100 - 900
		for(UT_sint32 n1 = 400; n1 > 0; )
		{
			if( n3 >= n1)
			{
				n3 -= n1;
				labelStr[(*insPoint)++] = gHebrewDigit[(n1/100)-1+18];
			} else {
				n1 -= 100;
			} // if
		} // for

		// Process digit for 10 - 90
		UT_sint32 n2;
		if( n3 >= 10 )
		{
			// Special process for 15 and 16
			if(( 15 == n3 ) || (16 == n3)) {
				// Special rule for religious reason...
				// 15 is represented by 9 and 6, not 10 and 5
				// 16 is represented by 9 and 7, not 10 and 6
				n2 = 9;
				digit = gHebrewDigit[ n2 - 1];
			} else {
				n2 = n3 - (n3 % 10);
				digit = gHebrewDigit[(n2/10)-1+9];
			} // if

			n3 -= n2;
			labelStr[(*insPoint)++] = digit;
		} // if

		// Process digit for 1 - 9
		if ( n3 > 0)
		{
			labelStr[(*insPoint)++] = gHebrewDigit[n3-1];
		} // if
		value /= 1000;
	} while (value >= 1);
}

void fl_AutoNum::getAttributes (std::vector<std::string> & v,
								bool bEscapeXML) const
{
	char szID[15], szPid[15], szType[12], szStart[12];

	sprintf(szID, "%i", m_iID);
	v.push_back("id");
	v.push_back(szID);

	if (m_pParent)
		sprintf(szPid, "%i", m_pParent->getID());
	else
		sprintf(szPid, "0");
	v.push_back("parentid");
	v.push_back(szPid);

	sprintf(szType, "%i", m_List_Type);
	v.push_back("type");
	v.push_back(szType);

	sprintf(szStart, "%i", m_iStartValue);
	v.push_back("start-value");
	v.push_back(szStart);

	v.push_back("list-delim");
	std::string s = m_pszDelim;
	if (bEscapeXML)
	{
		s = UT_escapeXML(s);
	}
	v.push_back(s);

	v.push_back("list-decimal");
	s = m_pszDecimal;
	if (bEscapeXML)
	{
		s = UT_escapeXML(s);
	}
	v.push_back(s);
}

