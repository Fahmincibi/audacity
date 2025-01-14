/**********************************************************************

  Audacity: A Digital Audio Editor

  ToolsToolBar.cpp

  Dominic Mazzoni
  Shane T. Mueller
  Leland Lucius
 
  See ToolsToolBar.h for details

*******************************************************************//*!

\class ToolsToolBar
\brief A kind of ToolBar with Tools on it.

  This class, which is a child of Toolbar, creates the
  window containing the tool selection (ibeam, envelope,
  move, zoom), the rewind/play/stop/record/ff buttons, and
  the volume control. The window can be embedded within a
  normal project window, or within a ToolbarFrame that is
  managed by a global ToolBarStub called
  gToolsToolBarStub.

  All of the controls in this window were custom-written for
  Audacity - they are not native controls on any platform -
  however, it is intended that the images could be easily
  replaced to allow "skinning" or just customization to
  match the look and feel of each platform.

\see \ref Themability
*//*******************************************************************/


#include "../Audacity.h"

// For compilers that support precompilation, includes "wx/wx.h".
#include <wx/wxprec.h>

#ifndef WX_PRECOMP
#include <wx/defs.h>
#include <wx/event.h>
#include <wx/intl.h>
#include <wx/sizer.h>
#endif
#include <wx/tooltip.h>

#include "MeterToolBar.h"
#include "ToolsToolBar.h"

#include "../AllThemeResources.h"
#include "../ImageManipulation.h"
#include "../Project.h"
#include "../Theme.h"
#include "../widgets/AButton.h"

IMPLEMENT_CLASS(ToolsToolBar, ToolBar);

// Strings to convert a tool number into a status message
// These MUST be in the same order as the ids above.
const wxChar * MessageOfTool[numTools] = { wxTRANSLATE("Click and drag to select audio"),
   wxTRANSLATE("Click and drag to edit the amplitude envelope"),
   wxTRANSLATE("Click and drag to edit the samples"),
#if defined( __WXMAC__ )
   wxTRANSLATE("Click to Zoom In, Shift-Click to Zoom Out"),
#elif defined( __WXMSW__ )
   wxTRANSLATE("Drag to Zoom Into Region, Right-Click to Zoom Out"),
#elif defined( __WXGTK__ )
   wxTRANSLATE("Left=Zoom In, Right=Zoom Out, Middle=Normal"),
#endif
   wxTRANSLATE("Click and drag to move a track in time"),
   wxT("") // multi-mode tool
};

////////////////////////////////////////////////////////////
/// Methods for ToolsToolBar
////////////////////////////////////////////////////////////

BEGIN_EVENT_TABLE(ToolsToolBar, ToolBar)
   EVT_COMMAND_RANGE(firstTool,
                     lastTool,
                     wxEVT_COMMAND_BUTTON_CLICKED,
                     ToolsToolBar::OnTool)
END_EVENT_TABLE()

//Standard constructor
ToolsToolBar::ToolsToolBar()
: ToolBar(ToolsBarID, _("Tools"))
{
   //Read the following wxASSERTs as documentating a design decision
   wxASSERT( selectTool   == selectTool   - firstTool );
   wxASSERT( envelopeTool == envelopeTool - firstTool );
   wxASSERT( slideTool    == slideTool    - firstTool );
   wxASSERT( zoomTool     == zoomTool     - firstTool );
   wxASSERT( drawTool     == drawTool     - firstTool );
   wxASSERT( multiTool    == multiTool    - firstTool );
}

ToolsToolBar::~ToolsToolBar()
{
   for (int i = 0; i < 5; i++)
      delete mTool[i];
}

void ToolsToolBar::Create(wxWindow * parent)
{
   ToolBar::Create(parent);

   mCurrentTool = selectTool;
   mTool[mCurrentTool]->PushDown();
}

void ToolsToolBar::RegenerateToolsTooltips()
{

// JKC: 
//   Under Win98 Tooltips appear to be buggy, when you have a lot of
//   tooltip messages flying around.  I found that just creating a 
//   twelfth tooltip caused Audacity to crash when it tried to show 
//   any tooltip.
//
//   Win98 does NOT recover from this crash - for any application which is 
//   using tooltips will also crash thereafter...  so you must reboot.
//   Rather weird.  
//
//   Getting windows to process more of its stacked up messages seems
//   to workaround the problem.  The problem is not fully understood though
//   (as of April 2003).
   
   //	Vaughan, October 2003: Now we're crashing on Win2K if 
	// "Quit when closing last window" is unchecked, when we come back 
	// through here, on either of the wxSafeYield calls.
	// James confirms that commenting them out does not cause his original problem 
	// to reappear, so they're commented out now.
	//		wxSafeYield(); //Deal with some queued up messages...

   #if wxUSE_TOOLTIPS
   mTool[selectTool]->SetToolTip(_("Selection Tool"));
   mTool[envelopeTool]->SetToolTip(_("Envelope Tool"));
   mTool[slideTool]->SetToolTip(_("Time Shift Tool"));
   mTool[zoomTool]->SetToolTip(_("Zoom Tool"));
   mTool[drawTool]->SetToolTip(_("Draw Tool"));
   mTool[multiTool]->SetToolTip(_("Multi-Tool Mode"));
   #endif

   //		wxSafeYield();
   return;
}


AButton * ToolsToolBar::MakeTool( teBmps eTool, 
   int id, const wxChar *label)
{
   AButton *button = ToolBar::MakeButton(
      bmpRecoloredUpSmall, bmpRecoloredDownSmall, bmpRecoloredHiliteSmall,
      eTool, eTool,
      wxWindowID(id),
      wxDefaultPosition, true,
      theTheme.ImageSize( bmpRecoloredUpSmall ));
   button->SetLabel( label );
   mToolSizer->Add( button );
   return button;
}
                        

void ToolsToolBar::Populate()
{
   MakeButtonBackgroundsSmall();
   mToolSizer = new wxGridSizer( 2, 3, 1, 1 );
   Add( mToolSizer );

   /* Tools */
   mTool[ selectTool   ] = MakeTool( bmpIBeam, selectTool, _("SelectionTool") );
   mTool[ envelopeTool ] = MakeTool( bmpEnvelope, envelopeTool, _("TimeShiftTool") );
   mTool[ drawTool     ] = MakeTool( bmpDraw, drawTool, _("DrawTool") );
   mTool[ zoomTool     ] = MakeTool( bmpZoom, zoomTool, _("ZoomTool") );
   mTool[ slideTool    ] = MakeTool( bmpTimeShift, slideTool, _("SlideTool") );
   mTool[ multiTool    ] = MakeTool( bmpMulti, multiTool, _("MultiTool") );

   RegenerateToolsTooltips();
}

/// Gets the currently active tool
/// In Multi-mode this might not return the multi-tool itself
/// since the active tool may be changed by what you hover over.
int ToolsToolBar::GetCurrentTool()
{
   return mCurrentTool;
}

/// Sets the currently active tool
/// @param tool - The index of the tool to be used.
/// @param show - should we update the button display?
void ToolsToolBar::SetCurrentTool(int tool, bool show)
{
   //In multi-mode the current tool is shown by the 
   //cursor icon.  The buttons are not updated.

   bool leavingMulticlipMode =
      IsDown(multiTool) && show && tool != multiTool;
      
   if (leavingMulticlipMode)
      mTool[multiTool]->PopUp();
      
   if (tool != mCurrentTool || leavingMulticlipMode) {
      if (show)
         mTool[mCurrentTool]->PopUp();
      mCurrentTool=tool;
      if (show)
         mTool[mCurrentTool]->PushDown();
   }
	//JKC: ANSWER-ME: Why is this RedrawAllProjects() line required?
	//msmeyer: I think it isn't, we leave it out for 1.3.1 (beta), and
	// we'll see if anyone complains.
   // RedrawAllProjects();

   //msmeyer: But we instruct the projects to handle the cursor shape again
   if (show)
      RefreshCursorForAllProjects();
}

bool ToolsToolBar::IsDown(int tool)
{
   return mTool[tool]->IsDown();
}

int ToolsToolBar::GetDownTool()
{
   int tool;
   
   for (tool = firstTool; tool <= lastTool; tool++)
      if (IsDown(tool)) 
         return tool;

   return firstTool;  // Should never happen
}

const wxChar * ToolsToolBar::GetMessageForTool( int ToolNumber )
{
   wxASSERT( ToolNumber >= 0 );
   wxASSERT( ToolNumber < numTools );
   return wxGetTranslation(MessageOfTool[ ToolNumber ]);
}


void ToolsToolBar::OnTool(wxCommandEvent & evt)
{
   mCurrentTool = evt.GetId() - firstTool;
   for (int i = 0; i < numTools; i++)
      if (i == mCurrentTool) 
         mTool[i]->PushDown();
      else
         mTool[i]->PopUp();

   RedrawAllProjects();
}

// Indentation settings for Vim and Emacs and unique identifier for Arch, a
// version control system. Please do not modify past this point.
//
// Local Variables:
// c-basic-offset: 3
// indent-tabs-mode: nil
// End:
//
// vim: et sts=3 sw=3
// arch-tag: ebfdc42a-6a03-4826-afa2-937a48c0565b
