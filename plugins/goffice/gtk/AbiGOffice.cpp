/*
 * Copyright (C) 2004 Luca Padovani <lpadovan@cs.unibo.it>
 * Copyright (C) 2005 Martin Sevior <msevior@physics.unimelb.edu.au>
 * Copyright (C) 2005 Jean Brefort <jean.brefort@normalesup.org>
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
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301
 * USA
 */

#ifdef ABI_PLUGIN_BUILTIN
#define abi_plugin_register abipgn_abigochart_register
#define abi_plugin_unregister abipgn_abigochart_unregister
#define abi_plugin_supports_version abipgn_abigochart_supports_version
#endif

#include "xap_Module.h"
#include "ie_imp_GOChart.h"
#include "ie_imp_GOComponent.h"
#include "xap_App.h"
#include "xap_Frame.h"
#include "ap_Menu_Id.h"
#include "ev_Menu_Actions.h"
#include "ev_EditMethod.h"
#include "xap_Menu_Layouts.h"
#include "ut_stack.h"
#include "AbiGOffice.h"
#include "AbiGOChart.h"
#include "AbiGOComponent.h"

#include <goffice/component/go-component-factory.h>
#include <list>
#include <string>

//
// GOCmdContext interface implementation for AbiGOffice
//
// we must implement it at least for errors reporting

typedef	GObject AbiCmdContext;
typedef GObjectClass AbiCmdContextClass;

static void
abi_error_error (G_GNUC_UNUSED GOCmdContext *cc, GError *error)
{
	fprintf (stderr, "Error: %s\n", error->message);
}

static void
abi_error_info (G_GNUC_UNUSED GOCmdContext *cc, GOErrorInfo *error)
{
	go_error_info_print (error);
}

static char *
abi_get_password (G_GNUC_UNUSED GOCmdContext *cc,
		  G_GNUC_UNUSED char const* filename)
{
	return nullptr;
}

static void
abi_set_sensitive (G_GNUC_UNUSED GOCmdContext *cc,
		   G_GNUC_UNUSED gboolean sensitive)
{
}

static void
abi_progress_set (G_GNUC_UNUSED GOCmdContext *cc, G_GNUC_UNUSED double val)
{
}

static void
abi_progress_message_set (G_GNUC_UNUSED GOCmdContext *cc, G_GNUC_UNUSED gchar const *msg)
{
}

static void
abi_cmd_context_init (GOCmdContextClass *iface, gpointer)
{
	iface->get_password	    = abi_get_password;
	iface->set_sensitive	    = abi_set_sensitive;
	iface->error.error	    = abi_error_error;
	iface->error.error_info	    = abi_error_info;
	iface->progress_set	    = abi_progress_set;
	iface->progress_message_set = abi_progress_message_set;
}

GSF_CLASS_FULL (AbiCmdContext, abi_cmd_context,
		nullptr, nullptr, nullptr, nullptr,
		nullptr, G_TYPE_OBJECT, 0,
		GSF_INTERFACE (abi_cmd_context_init, GO_TYPE_CMD_CONTEXT))

//
// AbiGOffice_addToMenus
// -----------------------
//   Adds "Object" "Gnome Office Chart" "FromFile" options to AbiWord's Main Menu.
//
// FIXME make these translatable strings

static const char * Object_MenuLabelObject = "Object";
static const char * Object_MenuTooltipObject = "Insert Embeddable Object";
static const char* AbiGOChart_MenuLabelInsert = "Gnome Office Chart";
static const char* AbiGOChart_MenuTooltipInsert = "Create a chart";
static const char* AbiGOComponent_MenuLabelInsertFromFile = "From File";
static const char* AbiGOComponent_MenuTooltipInsertFromFile = "Insert the contents of a file";
static const char* AbiGOComponent_MenuLabelCreate = "New";
static const char* AbiGOComponent_MenuTooltipCreate = "Create a new object";

static XAP_Menu_Id newObjectID = 0;
static XAP_Menu_Id InsertGOChartID = 0;
static XAP_Menu_Id InsertGOComponentFromFileID = 0;
static XAP_Menu_Id CreateGOComponentID = 0;

static void AbiGOffice_addToMenus()
{
    // First we need to get a pointer to the application itself.
    XAP_App *pApp = XAP_App::getApp();
    //
    // Translated Strings
    //
//     const XAP_StringSet * pSS = pApp->getStringSet();
//     AbiMathView_MenuLabelEquation= pSS->getValue(AP_STRING_ID_MENU_LABEL_INSERT_EQUATION);
//     AbiMathView_MenuTooltipEquation = pSS->getValue(AP_STRING_ID_MENU_LABEL_TOOLTIP_INSERT_EQUATION);
//     AbiMathView_MenuLabelFileInsert = pSS->getValue(AP_STRING_ID_MENU_LABEL_INSERT_EQUATION_FILE);
//     AbiMathView_MenuTooltipFileInsert = pSS->getValue(AP_STRING_ID_MENU_LABEL_TOOLTIP_INSERT_EQUATION_FILE);
    
    // Create an EditMethod that will link our method's name with
    // it's callback function.  This is used to link the name to 
    // the callback.
    EV_EditMethod *myEditMethodFile = new EV_EditMethod(
        "AbiGOChart_Create",  // name of callback function
        AbiGOChart_Create,    // callback function itself.
        0,                      // no additional data required.
        ""                      // description -- allegedly never used for anything
    );
   
    // Now we need to get the EditMethod container for the application.
    // This holds a series of Edit Methods and links names to callbacks.
    EV_EditMethodContainer* pEMC = pApp->getEditMethodContainer();
    
    // We have to add our EditMethod to the application's EditMethodList
    // so that the application will know what callback to call when a call
    // to "AbiMathView_FileInsert" is received.
    pEMC->addEditMethod(myEditMethodFile);

    // Now we need to grab an ActionSet.  This is going to be used later
    // on in our for loop.  Take a look near the bottom.
    EV_Menu_ActionSet* pActionSet = pApp->getMenuActionSet();
 
	XAP_Menu_Factory * pFact = pApp->getMenuFactory();

	// Look to see if "Object" has been loaded already..
   bool bObjectExists = true;
   if(newObjectID <= 0)
   {
     // Put it after Insert Picture in the Main menu
     // FIXME put it before!
     bObjectExists = false;
     newObjectID= pFact->addNewMenuBefore("Main",nullptr,AP_MENU_ID_INSERT_DIRECTIONMARKER,EV_MLF_BeginSubMenu);
   }

   UT_DEBUGMSG(("newObjectID %d \n",newObjectID));

    pFact->addNewLabel(nullptr,newObjectID,Object_MenuLabelObject, Object_MenuTooltipObject);

    // Create the Action that will be called.
    EV_Menu_Action* myObjectAction = new EV_Menu_Action(
	newObjectID,          // id that the layout said we could use
	1,                      // yes, we have a sub menu.
	0,                      // no, we don't raise a dialog.
	0,                      // no, we don't have a checkbox.
	0,                      // no radio buttons for me, thank you
	nullptr,                   //  no callback function to call.
	nullptr,                   // don't know/care what this is for
	nullptr                    // don't know/care what this is for
        );

    // Now what we need to do is add this particular action to the ActionSet
    // of the application.  This forms the link between our new ID that we 
    // got for this particular frame with the EditMethod that knows how to 
    // call our callback function.  

    pActionSet->addAction(myObjectAction);

    InsertGOChartID= pFact->addNewMenuAfter("Main",nullptr,newObjectID,EV_MLF_Normal);
   UT_DEBUGMSG(("GOChart ID %d \n",InsertGOChartID));

    pFact->addNewLabel(nullptr, InsertGOChartID,AbiGOChart_MenuLabelInsert, AbiGOChart_MenuTooltipInsert);
	// Create the Action that will be called.
	EV_Menu_Action* myChartAction = new EV_Menu_Action(
	InsertGOChartID,        // id that the layout said we could use
	0,                      // no, we don't have a sub menu.
	1,                      // yes, we raise a dialog.
	0,                      // no, we don't have a checkbox.
	0,                      // no radio buttons for me, thank you
	"AbiGOChart_Create",  // name of callback function to call.
	nullptr,                   // don't know/care what this is for
	nullptr                    // don't know/care what this is for
		);

	// Now what we need to do is add this particular action to the ActionSet
	// of the application.  This forms the link between our new ID that we 
	// got for this particular frame with the EditMethod that knows how to 
	// call our callback function.  

	pActionSet->addAction(myChartAction);

	if (g_slist_length (mime_types) > 0) {
		myEditMethodFile = new EV_EditMethod(
			"AbiGOComponent_FileInsert",  // name of callback function
			AbiGOComponent_FileInsert,    // callback function itself.
			0,                      // no additional data required.
			""                      // description -- allegedly never used for anything
		);
		pEMC->addEditMethod(myEditMethodFile);
	
		myEditMethodFile = new EV_EditMethod(
			"AbiGOComponent_Create",  // name of callback function
			AbiGOComponent_Create,    // callback function itself.
			0,                      // no additional data required.
			""                      // description -- allegedly never used for anything
		);
		pEMC->addEditMethod(myEditMethodFile);	
	
		InsertGOComponentFromFileID= pFact->addNewMenuAfter("Main",nullptr,InsertGOChartID,EV_MLF_Normal);
	   UT_DEBUGMSG(("GOComponentFromFile ID %d \n",InsertGOComponentFromFileID));
	
		pFact->addNewLabel(nullptr, InsertGOComponentFromFileID,AbiGOComponent_MenuLabelInsertFromFile, AbiGOComponent_MenuTooltipInsertFromFile);
	
	
		// Create the Action that will be called.
		EV_Menu_Action* myFileAction = new EV_Menu_Action(
		InsertGOComponentFromFileID,        // id that the layout said we could use
		0,                      // no, we don't have a sub menu.
		1,                      // yes, we raise a dialog.
		0,                      // no, we don't have a checkbox.
		0,                      // no radio buttons for me, thank you
		"AbiGOComponent_FileInsert",  // name of callback function to call.
		nullptr,                   // don't know/care what this is for
		nullptr                    // don't know/care what this is for
			);
	
		// Now what we need to do is add this particular action to the ActionSet
		// of the application.  This forms the link between our new ID that we 
		// got for this particular frame with the EditMethod that knows how to 
		// call our callback function.  
	
		pActionSet->addAction(myFileAction);
	
	
		CreateGOComponentID= pFact->addNewMenuAfter("Main",nullptr,InsertGOComponentFromFileID,EV_MLF_Normal);
	   UT_DEBUGMSG(("CreateGOComponent ID %d \n",CreateGOComponentID));
	
		pFact->addNewLabel(nullptr, CreateGOComponentID,AbiGOComponent_MenuLabelCreate, AbiGOComponent_MenuTooltipCreate);
	
	
		// Create the Action that will be called.
		EV_Menu_Action* myCompAction = new EV_Menu_Action(
		CreateGOComponentID,        // id that the layout said we could use
		0,                      // no, we don't have a sub menu.
		1,                      // yes, we raise a dialog.
		0,                      // no, we don't have a checkbox.
		0,                      // no radio buttons for me, thank you
		"AbiGOComponent_Create",  // name of callback function to call.
		nullptr,                   // don't know/care what this is for
		nullptr                    // don't know/care what this is for
			);
	
		// Now what we need to do is add this particular action to the ActionSet
		// of the application.  This forms the link between our new ID that we 
		// got for this particular frame with the EditMethod that knows how to 
		// call our callback function.  
	
		pActionSet->addAction(myCompAction);
	
		if(!bObjectExists)
		{
			XAP_Menu_Id endObjectID= pFact->addNewMenuAfter("Main",nullptr,CreateGOComponentID,EV_MLF_EndSubMenu);
			UT_DEBUGMSG(("End Object ID %d \n",endObjectID));
			pFact->addNewLabel(nullptr,endObjectID,nullptr,nullptr);
			// Create the Action that will be called.
			EV_Menu_Action* myEndObjectAction = new EV_Menu_Action(
										   endObjectID,          // id that the layout said we could use
				0,                      // no, we don't have a sub menu.
				0,                      // no, we raise a dialog.
				0,                      // no, we don't have a checkbox.
				0,                      // no radio buttons for me, thank you
				nullptr,                   // name of callback function to call.
				nullptr,                   // don't know/care what this is for
				nullptr                    // don't know/care what this is for
				);
			
			
			  pActionSet->addAction(myEndObjectAction);
		}
	}
	else if(!bObjectExists)
	{
		XAP_Menu_Id endObjectID= pFact->addNewMenuAfter("Main",nullptr,InsertGOChartID,EV_MLF_EndSubMenu);
		UT_DEBUGMSG(("End Object ID %d \n",endObjectID));
		pFact->addNewLabel(nullptr,endObjectID,nullptr,nullptr);
		// Create the Action that will be called.
		EV_Menu_Action* myEndObjectAction = new EV_Menu_Action(
									   endObjectID,          // id that the layout said we could use
			0,                      // no, we don't have a sub menu.
			0,                      // no, we raise a dialog.
			0,                      // no, we don't have a checkbox.
			0,                      // no radio buttons for me, thank you
			nullptr,                   // name of callback function to call.
			nullptr,                   // don't know/care what this is for
			nullptr                    // don't know/care what this is for
			);
		
		  pActionSet->addAction(myEndObjectAction);
	}

    // We need to go through and add the menu element to each "frame" 
    // of the application.  We can iterate through the frames by doing
    // XAP_App::getFrameCount() to tell us how many frames there are,
    // then calling XAP_App::getFrame(i) to get the i-th frame.
	int frameCount = pApp->getFrameCount();
	for(int i = 0;i < frameCount;++i)
    {
        // Get the current frame that we're iterating through.
		XAP_Frame* pFrame = pApp->getFrame(i);
		pFrame->rebuildMenus();
    }
}

static void
AbiGOffice_removeFromMenus ()
{
	// First we need to get a pointer to the application itself.
	XAP_App *pApp = XAP_App::getApp();
	XAP_Menu_Factory * pFact = pApp->getMenuFactory();

	// remove the edit method
	EV_EditMethodContainer* pEMC = pApp->getEditMethodContainer() ;
	EV_EditMethod * pEM = ev_EditMethod_lookup ( "AbiGOChart_Create" ) ;
	pEMC->removeEditMethod ( pEM ) ;
	DELETEP( pEM ) ;
	pFact->removeMenuItem("Main",nullptr,InsertGOChartID);
	if (g_slist_length (mime_types) > 0) {
		pEM = ev_EditMethod_lookup ( "AbiGOComponent_FileInsert" ) ;
		pEMC->removeEditMethod ( pEM ) ;
		DELETEP( pEM ) ;
		pEM = ev_EditMethod_lookup ( "AbiGOComponent_Create" ) ;
		pEMC->removeEditMethod ( pEM ) ;
		DELETEP( pEM ) ;
		pFact->removeMenuItem("Main",nullptr,InsertGOComponentFromFileID);
		pFact->removeMenuItem("Main",nullptr,CreateGOComponentID);
	}
	pFact->removeMenuItem("Main",nullptr,newObjectID);
	
	int frameCount = pApp->getFrameCount();
	for(int i = 0;i < frameCount;++i)
	{
	  // Get the current frame that we're iterating through.
	  XAP_Frame* pFrame = pApp->getFrame(i);
	  pFrame->rebuildMenus();
	}
}

static GR_GOChartManager  *pGOChartManager = nullptr;
static GR_GOComponentManager  *pGOComponentManager = nullptr;

static IE_Imp_Object_Sniffer  * m_impSniffer = nullptr;
static IE_Imp_Component_Sniffer  * m_impCSniffer = nullptr;
static GOCmdContext *cc;

GOCmdContext *
abi_goffice_get_cmd_context (void)
{
	return cc;
}


// -----------------------------------------------------------------------
//
//      Register the component manager for each mime type to stay compatible
//		Should be inactive by default in the future, and using an environment
//		variable to activate (ABIGOCOMPONENT_COMPATIBLE or so).
//
// -----------------------------------------------------------------------

std::list<std::string> uids;

static void
register_mime_cb (char const *mime_type, XAP_App * pApp)
{
	if (!strcmp (mime_type, "application/mathml+xml"))
		return;
	std::string uid = std::string("GOComponent//") + mime_type;
	uids.push_front (uid);
	pApp->registerEmbeddable(pGOComponentManager, uid.c_str());
	if (go_components_support_clipboard (mime_type))
		pApp->addClipboardFmt (mime_type);
}

ABI_PLUGIN_DECLARE(GOffice)

// -----------------------------------------------------------------------
//
//      Abiword Plugin Interface 
//
// -----------------------------------------------------------------------

ABI_FAR_CALL
int abi_plugin_register (XAP_ModuleInfo * mi)
{
    mi->name = "AbiGOffice";
    mi->desc = "The plugin enables Gnome Office Charts and components to be displayed in AbiWord";
    mi->version = ABI_VERSION_STRING;
    mi->author = "Jean Bréfort <jean.brefort@normalesup.org>";
    mi->usage = "No Usage";

    // Add to AbiWord's plugin importers
    m_impSniffer = new IE_Imp_Object_Sniffer();
    IE_Imp::registerImporter (m_impSniffer);    
    m_impCSniffer = new IE_Imp_Component_Sniffer();
    IE_Imp::registerImporter (m_impCSniffer);    

    // Add to AbiWord's plugin listeners
    XAP_App * pApp = XAP_App::getApp();	
    pGOChartManager = new GR_GOChartManager(nullptr);
	pGOChartManager->buildContextualMenu ();
    pApp->registerEmbeddable(pGOChartManager);
 	/* Initialize libgoffice */
	libgoffice_init ();
	cc = GO_CMD_CONTEXT (g_object_new (ABI_CMD_CONTEXT_TYPE, nullptr));
	go_component_set_default_command_context (cc);
	/* Initialize plugins manager */
	go_plugins_init (cc, nullptr, nullptr, nullptr, TRUE, GO_TYPE_PLUGIN_LOADER_MODULE);
	/* Ensure some types are created */
 	GO_TYPE_DATA_SCALAR_STR;
	GO_TYPE_DATA_VECTOR_STR;
 	GO_TYPE_DATA_SCALAR_VAL;
	GO_TYPE_DATA_VECTOR_VAL;
 	GO_TYPE_DATA_MATRIX_VAL;
   // Add to AbiWord's menus.
	mime_types = go_components_get_mime_types ();
	if (mime_types && mime_types->data)
	{
		pGOComponentManager = new GR_GOComponentManager(nullptr);
 	    pApp->registerEmbeddable(pGOComponentManager);
	}
	g_slist_foreach (mime_types, (GFunc) register_mime_cb, pApp);
    AbiGOffice_addToMenus();
	return 1;
}


ABI_FAR_CALL
int abi_plugin_unregister (XAP_ModuleInfo * mi)
{
    mi->name = nullptr;
    mi->desc = nullptr;
    mi->version = nullptr;
    mi->author = nullptr;
    mi->usage = nullptr;

    IE_Imp::unregisterImporter (m_impSniffer);
    delete m_impSniffer;
    m_impSniffer = nullptr;

    IE_Imp::unregisterImporter (m_impCSniffer);
    delete m_impCSniffer;
    m_impCSniffer = nullptr;

    pGOChartManager->removeContextualMenu ();
    XAP_App * pApp = XAP_App::getApp();
    pApp->unRegisterEmbeddable(pGOChartManager->getObjectType());
    DELETEP(pGOChartManager);
	GSList *l = mime_types;
	while (l)
	{
		if (go_components_support_clipboard ((const char*)l->data))
			pApp->deleteClipboardFmt((const char*)l->data);
		l = l->next;
	}
	std::list<std::string>::iterator i, end = uids.end();
	for (i = uids.begin(); i != end; i++)
		pApp->unRegisterEmbeddable((*i).c_str());
	uids.clear();
	if (pGOComponentManager) {
		pApp->unRegisterEmbeddable(pGOComponentManager->getObjectType());
		DELETEP(pGOComponentManager);
	}
    AbiGOffice_removeFromMenus();
	go_component_set_default_command_context (nullptr);
	g_object_unref (cc);
	go_plugins_shutdown();
	libgoffice_shutdown();

    return 1;
}

ABI_FAR_CALL
int abi_plugin_supports_version (G_GNUC_UNUSED UT_uint32 major, G_GNUC_UNUSED UT_uint32 minor, G_GNUC_UNUSED UT_uint32 release)
{
    return 1; 
}

cairo_status_t abi_CairoWrite(UT_ByteBuf * buf, unsigned char * data, unsigned int length)
{
	return (buf->append (static_cast<UT_Byte*>(data), static_cast<UT_uint32>(length)))?
			CAIRO_STATUS_SUCCESS: CAIRO_STATUS_WRITE_ERROR;
}
