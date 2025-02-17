/**********************************************************************

  Audacity: A Digital Audio Editor

  ToolManager.h

  Dominic Mazzoni
  Shane T. Mueller
  Leland Lucius

**********************************************************************/

#ifndef __AUDACITY_TOOLMANAGER__
#define __AUDACITY_TOOLMANAGER__

#include <wx/defs.h>
#include <wx/frame.h>
#include <wx/timer.h>

#include "ToolDock.h"
#include "ToolBar.h"

class wxArrayPtrVoid;
class wxBitmap;
class wxCommandEvent;
class wxFrame;
class wxMouseEvent;
class wxPaintEvent;
class wxPoint;
class wxRect;
class wxRegion;
class wxSize;
class wxTimer;
class wxTimerEvent;
class wxWindow;

class AudacityProject;
class ToolFrame;

////////////////////////////////////////////////////////////
/// class ToolManager
////////////////////////////////////////////////////////////

class ToolManager:public wxEvtHandler
{

 public:

   ToolManager( AudacityProject *parent );
   ~ToolManager();

   void LayoutToolBars();

   bool IsDocked( int type );

   bool IsVisible( int type );

   bool DefaultShow(int type);
   void ShowHide( int type );

   void Expose( int type, bool show );

   ToolBar *GetToolBar( int type ) const;

   ToolDock *GetTopDock();
   ToolDock *GetBotDock();

   void Reset();

 private:

   ToolBar *Float( ToolBar *t, wxPoint & pos );

   void OnTimer( wxTimerEvent & event );
   void OnMouse( wxMouseEvent & event );
   void OnGrabber( GrabberEvent & event );

   void OnIndicatorCreate( wxWindowCreateEvent & event );
   void OnIndicatorPaint( wxPaintEvent & event );

   void ReadConfig();
   void WriteConfig();
   void Updated();

   AudacityProject *mParent;

   ToolFrame *mDragWindow;
   ToolDock *mDragDock;
   ToolBar *mDragBar;
   wxPoint mDragOffset;
   int mDragBefore;

   wxPoint mLastPos;
   wxRect mBarPos;

   wxFrame *mIndicator;
   wxRegion *mLeft;
   wxRegion *mDown;
   wxRegion *mCurrent;

   wxTimer mTimer;
   bool mLastState;

#if defined(__WXMAC__)
   bool mTransition;
#endif

   wxArrayPtrVoid mDockedBars;
   ToolDock *mTopDock;
   ToolDock *mBotDock;

   ToolBar *mBars[ ToolBarCount ];

 public:

   DECLARE_CLASS( ToolManager );
   DECLARE_EVENT_TABLE();
};

#endif

// Indentation settings for Vim and Emacs and unique identifier for Arch, a
// version control system. Please do not modify past this point.
//
// Local Variables:
// c-basic-offset: 3
// indent-tabs-mode: nil
// End:
//
// vim: et sts=3 sw=3
// arch-tag: 5a2a21f8-6c9e-45a4-8718-c26cad5cfe65

