/**********************************************************************

  Audacity: A Digital Audio Editor

  Screenshot.cpp
  
  Dominic Mazzoni

*******************************************************************/

#include "Screenshot.h"
#include "commands/ScreenshotCommand.h"
#include "commands/CommandTargets.h"
#include "commands/CommandDirectory.h"
#include <wx/defs.h>
#include <wx/event.h>
#include <wx/frame.h>

#include "ShuttleGui.h"
#include <wx/button.h>
#include <wx/checkbox.h>
#include <wx/dirdlg.h>
#include <wx/image.h>
#include <wx/intl.h>
#include <wx/panel.h>
#include <wx/statusbr.h>
#include <wx/textctrl.h>
#include <wx/timer.h>
#include <wx/tglbtn.h>
#include <wx/window.h>

#include "AudacityApp.h"
#include "Project.h"
#include "Prefs.h"
#include "toolbars/ToolManager.h"

class CommandType;

////////////////////////////////////////////////////////////////////////////////

class ScreenFrame:public wxFrame
{
 public:
   // constructors and destructors
   ScreenFrame(wxWindow *parent, wxWindowID id);
   virtual ~ScreenFrame();

   virtual bool ProcessEvent(wxEvent & e);

 private:
   void Populate();
   void PopulateOrExchange(ShuttleGui &S);

   void OnCloseWindow(wxCloseEvent & e);
   void OnUIUpdate(wxUpdateUIEvent & e);
   void OnDirChoose(wxCommandEvent & e);

   void SizeMainWindow(int w, int h);
   void OnMainWindowSmall(wxCommandEvent & e);
   void OnMainWindowLarge(wxCommandEvent & e);
   void OnToggleBackgroundBlue(wxCommandEvent & e);
   void OnToggleBackgroundWhite(wxCommandEvent & e);

   void DoCapture(wxString captureMode);
   void OnCaptureWindowContents(wxCommandEvent & e);
   void OnCaptureFullWindow(wxCommandEvent & e);
   void OnCaptureWindowPlus(wxCommandEvent & e);
   void OnCaptureFullScreen(wxCommandEvent & e);
   void OnCaptureToolbars(wxCommandEvent & e);
   void OnCaptureSelectionBar(wxCommandEvent & e);
   void OnCaptureTools(wxCommandEvent & e);
   void OnCaptureControl(wxCommandEvent & e);
   void OnCaptureMixer(wxCommandEvent & e);
   void OnCaptureMeter(wxCommandEvent & e);
   void OnCaptureEdit(wxCommandEvent & e);
   void OnCaptureDevice(wxCommandEvent & e);
   void OnCaptureTranscription(wxCommandEvent & e);

   void TimeZoom(double seconds);
   void OnOneSec(wxCommandEvent & e);
   void OnTenSec(wxCommandEvent & e);
   void OnOneMin(wxCommandEvent & e);
   void OnFiveMin(wxCommandEvent & e);
   void OnOneHour(wxCommandEvent & e);

   void SizeTracks(int h);
   void OnShortTracks(wxCommandEvent & e);
   void OnMedTracks(wxCommandEvent & e);
   void OnTallTracks(wxCommandEvent & e);

   void OnCaptureTrackPanel(wxCommandEvent & e);
   void OnCaptureRuler(wxCommandEvent & e);
   void OnCaptureTracks(wxCommandEvent & e);
   void OnCaptureFirstTrack(wxCommandEvent & e);
   void OnCaptureSecondTrack(wxCommandEvent & e);

   ScreenshotCommand *CreateCommand();

   wxCheckBox *mDelayCheckBox;
   wxTextCtrl *mDirectoryTextBox;
   wxToggleButton *mBlue;
   wxToggleButton *mWhite;
   wxStatusBar *mStatus;

   ScreenshotCommand *mCommand;
   CommandExecutionContext mContext;

   DECLARE_EVENT_TABLE()
};

static ScreenFrame *mFrame = NULL;

////////////////////////////////////////////////////////////////////////////////

void OpenScreenshotTools()
{
   if (!mFrame) {
      mFrame = new ScreenFrame(NULL, -1);
   }
   mFrame->Show();
   mFrame->Raise();
}

void CloseScreenshotTools()
{
   if (mFrame) {
      mFrame->Destroy();
      mFrame = NULL;
   }
}

////////////////////////////////////////////////////////////////////////////////

class ScreenFrameTimer:public wxTimer
{
 public:
   ScreenFrameTimer(ScreenFrame *frame,
                    wxEvent & e)
   {
      screenFrame = frame;
      evt = e.Clone();
   }

   virtual void Notify()
   {
      evt->SetEventObject(NULL);
      screenFrame->ProcessEvent(*evt);
      delete evt;
      delete this;
   }

 private:
   ScreenFrame *screenFrame;
   wxEvent *evt;
};

////////////////////////////////////////////////////////////////////////////////

enum
{
   IdMainWindowSmall = 19200,
   IdMainWindowLarge,

   IdDirectory,
   IdDirChoose,

   IdOneSec,
   IdTenSec,
   IdOneMin,
   IdFiveMin,
   IdOneHour,

   IdShortTracks,
   IdMedTracks,
   IdTallTracks,

   IdDelayCheckBox,

   IdCaptureToolbars,
   IdCaptureSelectionBar,
   IdCaptureTools,
   IdCaptureControl,
   IdCaptureMixer,
   IdCaptureMeter,
   IdCaptureEdit,
   IdCaptureDevice,
   IdCaptureTranscription,

   IdCaptureTrackPanel,
   IdCaptureRuler,
   IdCaptureTracks,
   IdCaptureFirstTrack,
   IdCaptureSecondTrack,

   IdToggleBackgroundBlue,
   IdToggleBackgroundWhite,

   // Put all events that might need delay below:
   IdAllDelayedEvents,

   IdCaptureWindowContents,
   IdCaptureFullWindow,
   IdCaptureWindowPlus,
   IdCaptureFullScreen,

   IdLastDelayedEvent,
};

BEGIN_EVENT_TABLE(ScreenFrame, wxFrame)
   EVT_CLOSE(ScreenFrame::OnCloseWindow)

   EVT_UPDATE_UI(IdCaptureFullScreen,   ScreenFrame::OnUIUpdate)

   EVT_BUTTON(IdMainWindowSmall,        ScreenFrame::OnMainWindowSmall)
   EVT_BUTTON(IdMainWindowLarge,        ScreenFrame::OnMainWindowLarge)
   EVT_TOGGLEBUTTON(IdToggleBackgroundBlue,   ScreenFrame::OnToggleBackgroundBlue)
   EVT_TOGGLEBUTTON(IdToggleBackgroundWhite,  ScreenFrame::OnToggleBackgroundWhite)
   EVT_BUTTON(IdCaptureWindowContents,  ScreenFrame::OnCaptureWindowContents)
   EVT_BUTTON(IdCaptureFullWindow,      ScreenFrame::OnCaptureFullWindow)
   EVT_BUTTON(IdCaptureWindowPlus,      ScreenFrame::OnCaptureWindowPlus)
   EVT_BUTTON(IdCaptureFullScreen,      ScreenFrame::OnCaptureFullScreen)

   EVT_BUTTON(IdCaptureToolbars,        ScreenFrame::OnCaptureToolbars)
   EVT_BUTTON(IdCaptureSelectionBar,    ScreenFrame::OnCaptureSelectionBar)
   EVT_BUTTON(IdCaptureTools,           ScreenFrame::OnCaptureTools)
   EVT_BUTTON(IdCaptureControl,         ScreenFrame::OnCaptureControl)
   EVT_BUTTON(IdCaptureMixer,           ScreenFrame::OnCaptureMixer)
   EVT_BUTTON(IdCaptureMeter,           ScreenFrame::OnCaptureMeter)
   EVT_BUTTON(IdCaptureEdit,            ScreenFrame::OnCaptureEdit)
   EVT_BUTTON(IdCaptureDevice,          ScreenFrame::OnCaptureDevice)
   EVT_BUTTON(IdCaptureTranscription,   ScreenFrame::OnCaptureTranscription)

   EVT_BUTTON(IdOneSec,                 ScreenFrame::OnOneSec)
   EVT_BUTTON(IdTenSec,                 ScreenFrame::OnTenSec)
   EVT_BUTTON(IdOneMin,                 ScreenFrame::OnOneMin)
   EVT_BUTTON(IdFiveMin,                ScreenFrame::OnFiveMin)
   EVT_BUTTON(IdOneHour,                ScreenFrame::OnOneHour)

   EVT_BUTTON(IdShortTracks,            ScreenFrame::OnShortTracks)
   EVT_BUTTON(IdMedTracks,              ScreenFrame::OnMedTracks)
   EVT_BUTTON(IdTallTracks,             ScreenFrame::OnTallTracks)

   EVT_BUTTON(IdCaptureTrackPanel,      ScreenFrame::OnCaptureTrackPanel)
   EVT_BUTTON(IdCaptureRuler,           ScreenFrame::OnCaptureRuler)
   EVT_BUTTON(IdCaptureTracks,          ScreenFrame::OnCaptureTracks)
   EVT_BUTTON(IdCaptureFirstTrack,      ScreenFrame::OnCaptureFirstTrack)
   EVT_BUTTON(IdCaptureSecondTrack,     ScreenFrame::OnCaptureSecondTrack)

   EVT_BUTTON(IdDirChoose,              ScreenFrame::OnDirChoose)
END_EVENT_TABLE();

// Must not be called before CreateStatusBar!
ScreenshotCommand *ScreenFrame::CreateCommand()
{
   wxASSERT(mStatus != NULL);
   CommandOutputTarget *output =
      new CommandOutputTarget(new NullProgressTarget(),
                              new StatusBarTarget(*mStatus),
                              new MessageBoxTarget());
   CommandType *type = CommandDirectory::Get()->LookUp(wxT("Screenshot"));
   wxASSERT_MSG(type != NULL, wxT("Screenshot command doesn't exist!"));
   return new ScreenshotCommand(*type, output, this);
}

ScreenFrame::ScreenFrame(wxWindow * parent, wxWindowID id)
:  wxFrame(parent, id, _("Screen Capture Frame"),
           wxDefaultPosition, wxDefaultSize,
#if !defined(__WXMSW__)
           wxFRAME_TOOL_WINDOW|
#else
           wxSTAY_ON_TOP|
#endif
           wxSYSTEM_MENU|wxCAPTION|wxCLOSE_BOX),
   mContext(&wxGetApp(), GetActiveProject())
{
   mDelayCheckBox = NULL;
   mDirectoryTextBox = NULL;

   mStatus = CreateStatusBar();
   mCommand = CreateCommand();

   Populate();

   // Reset the toolbars to a known state
   mContext.proj->mToolManager->Reset();
}

ScreenFrame::~ScreenFrame()
{
   delete mCommand;
}

void ScreenFrame::Populate()
{
   ShuttleGui S(this, eIsCreating);
   PopulateOrExchange(S);
}

void ScreenFrame::PopulateOrExchange(ShuttleGui & S)
{
   wxPanel *p = S.StartPanel();
   {
      S.SetBorder(3);

      S.StartStatic(_("Choose location to save files"));
      {
         S.StartMultiColumn(3, wxEXPAND);
         {
            S.SetStretchyCol(1);

            wxString dir =
               gPrefs->Read(wxT("/ScreenshotPath"),
                            wxFileName::GetHomeDir());
            mDirectoryTextBox =
               S.Id(IdDirectory).AddTextBox(_("Save images to:"),
                                            dir, 30);
            S.Id(IdDirChoose).AddButton(_("Choose..."));
         }
         S.EndMultiColumn();
      }
      S.EndStatic();

      S.StartStatic(_("Capture entire window or screen"));
      {
         S.StartHorizontalLay();
         {
            S.Id(IdMainWindowSmall).AddButton(_("Resize Small"));
            S.Id(IdMainWindowLarge).AddButton(_("Resize Large"));
            mBlue = new wxToggleButton(p,
                                       IdToggleBackgroundBlue,
                                       _("Blue Bkgnd"));
            S.AddWindow(mBlue);
            mWhite = new wxToggleButton(p,
                                        IdToggleBackgroundWhite,
                                        _("White Bkgnd"));
            S.AddWindow(mWhite);
         }
         S.EndHorizontalLay();

         S.StartHorizontalLay();
         {
            S.Id(IdCaptureWindowContents).AddButton(_("Capture Window Only"));
            S.Id(IdCaptureFullWindow).AddButton(_("Capture Full Window"));
            S.Id(IdCaptureWindowPlus).AddButton(_("Capture Window Plus"));
         }
         S.EndHorizontalLay();

         S.StartHorizontalLay();
         {
            S.Id(IdCaptureFullScreen).AddButton(_("Capture Full Screen"));
         }
         S.EndHorizontalLay();

         S.StartHorizontalLay();
         {
            mDelayCheckBox = S.Id(IdDelayCheckBox).AddCheckBox
               (_("Wait 5 seconds and capture frontmost window/dialog"),
                _("false"));
         }
         S.EndHorizontalLay();
      }
      S.EndStatic();

      S.StartStatic(_("Capture part of a project window"));
      {
         S.StartHorizontalLay();
         {
            S.Id(IdCaptureToolbars).AddButton(_("All Toolbars"));
            S.Id(IdCaptureSelectionBar).AddButton(_("SelectionBar"));
            S.Id(IdCaptureTools).AddButton(_("Tools"));
            S.Id(IdCaptureControl).AddButton(_("Control"));
         }
         S.EndHorizontalLay();

         S.StartHorizontalLay();
         {
            S.Id(IdCaptureMixer).AddButton(_("Mixer"));
            S.Id(IdCaptureMeter).AddButton(_("Meter"));
            S.Id(IdCaptureEdit).AddButton(_("Edit"));
            S.Id(IdCaptureDevice).AddButton(_("Device"));
            S.Id(IdCaptureTranscription).AddButton(_("Transcription"));
         }
         S.EndHorizontalLay();

         S.StartHorizontalLay();
         {
            S.Id(IdCaptureTrackPanel).AddButton(_("Track Panel"));
            S.Id(IdCaptureRuler).AddButton(_("Ruler"));
            S.Id(IdCaptureTracks).AddButton(_("Tracks"));
            S.Id(IdCaptureFirstTrack).AddButton(_("First Track"));
            S.Id(IdCaptureSecondTrack).AddButton(_("Second Track"));
         }
         S.EndHorizontalLay();
      }
      S.EndStatic();

      S.StartStatic(_("Scale"));
      {
         S.StartHorizontalLay();
         {
            S.Id(IdOneSec).AddButton(_("One Sec"));
            S.Id(IdTenSec).AddButton(_("Ten Sec"));
            S.Id(IdOneMin).AddButton(_("One Min"));
            S.Id(IdFiveMin).AddButton(_("Five Min"));
            S.Id(IdOneHour).AddButton(_("One Hour"));
         }
         S.EndHorizontalLay();

         S.StartHorizontalLay();
         {
            S.Id(IdShortTracks).AddButton(_("Short Tracks"));
            S.Id(IdMedTracks).AddButton(_("Medium Tracks"));
            S.Id(IdTallTracks).AddButton(_("Tall Tracks"));
         }
         S.EndHorizontalLay();
      }
      S.EndStatic();
   }
   S.EndPanel();

   Layout();
   Fit();
   SetMinSize(GetSize());

   int top = 0;
#ifdef __WXMAC__
   // Allow for Mac menu bar
   top += 20;
#endif

   int width, height;
   GetSize(&width, &height);
   int displayWidth, displayHeight;
   wxDisplaySize(&displayWidth, &displayHeight);

   if (width > 100) {
      Move(displayWidth - width - 16, top + 16);
   }
   else {
      CentreOnParent(); 
   }

   SetIcon(mContext.proj->GetIcon());
}

bool ScreenFrame::ProcessEvent(wxEvent & e)
{
   int id = e.GetId();

   if (mDelayCheckBox &&
       mDelayCheckBox->GetValue() &&
       e.IsCommandEvent() &&
       e.GetEventType() == wxEVT_COMMAND_BUTTON_CLICKED &&
       id >= IdAllDelayedEvents && id <= IdLastDelayedEvent &&
       e.GetEventObject() != NULL) {
      ScreenFrameTimer *timer = new ScreenFrameTimer(this, e);
      timer->Start(5000, true);
      return true;
   }

   if (e.IsCommandEvent() && e.GetEventObject() == NULL) {
      e.SetEventObject(this);
   }
   return wxFrame::ProcessEvent(e);
}

void ScreenFrame::OnCloseWindow(wxCloseEvent & e)
{
   mFrame = NULL;
   Destroy();
}

void ScreenFrame::OnUIUpdate(wxUpdateUIEvent & e)
{
#ifdef __WXMAC__
   wxTopLevelWindow *top = mCommand->GetFrontWindow(mContext.proj);
   bool needupdate = false;
   bool enable = false;

   if ((!top || top->IsIconized()) && mDirectoryTextBox->IsEnabled()) {
      needupdate = true;
      enable = false;
   }
   else if ((top && !top->IsIconized()) && !mDirectoryTextBox->IsEnabled()) {
      needupdate = true;
      enable = true;
   }

   if (needupdate) {
      for (int i = IdMainWindowSmall; i < IdLastDelayedEvent; i++) {
         wxWindow *w = wxWindow::FindWindowById(i, this);
         if (w) {
            w->Enable(enable);
         }
      }
   }
#endif
}

void ScreenFrame::OnDirChoose(wxCommandEvent & e)
{
   wxString current = mDirectoryTextBox->GetValue();

   wxDirDialog dlog(this, 
                    _("Choose a location to save screenshot images"),
                    current);

   dlog.ShowModal();
   if (dlog.GetPath() != wxT("")) {
      wxFileName tmpDirPath;
      tmpDirPath.AssignDir(dlog.GetPath());
      wxString path = tmpDirPath.GetPath(wxPATH_GET_VOLUME|wxPATH_GET_SEPARATOR);
      mDirectoryTextBox->SetValue(path);
      gPrefs->Write(wxT("/ScreenshotPath"), path);
   }
}

void ScreenFrame::OnToggleBackgroundBlue(wxCommandEvent & e)
{
   mWhite->SetValue(false);
   mCommand->SetParameter(wxT("Background"),
         mBlue->GetValue() ? wxT("Blue") : wxT("None"));
}

void ScreenFrame::OnToggleBackgroundWhite(wxCommandEvent & e)
{
   mBlue->SetValue(false);
   mCommand->SetParameter(wxT("Background"),
         mWhite->GetValue() ? wxT("White") : wxT("None"));
}

void ScreenFrame::SizeMainWindow(int w, int h)
{
   int top = 20;

   mContext.proj->Maximize(false);
   mContext.proj->SetSize(16, 16 + top, w, h);
   mContext.proj->mToolManager->Reset();
}

void ScreenFrame::OnMainWindowSmall(wxCommandEvent & e)
{
   SizeMainWindow(680, 450);
}

void ScreenFrame::OnMainWindowLarge(wxCommandEvent & e)
{
   SizeMainWindow(900, 600);
}

void ScreenFrame::DoCapture(wxString captureMode)
{
   Hide();
   mCommand->SetParameter(wxT("FilePath"), mDirectoryTextBox->GetValue());

   mCommand->SetParameter(wxT("CaptureMode"), captureMode);
   if (!mCommand->Apply(mContext))
      mStatus->SetStatusText(wxT("Capture failed!"));
   Show();
}

void ScreenFrame::OnCaptureWindowContents(wxCommandEvent & e)
{
   DoCapture(wxT("window"));
}

void ScreenFrame::OnCaptureFullWindow(wxCommandEvent & e)
{
   DoCapture(wxT("fullwindow"));
}

void ScreenFrame::OnCaptureWindowPlus(wxCommandEvent & e)
{
   DoCapture(wxT("windowplus"));
}

void ScreenFrame::OnCaptureFullScreen(wxCommandEvent & e)
{
   DoCapture(wxT("fullscreen"));
}

void ScreenFrame::OnCaptureToolbars(wxCommandEvent & e)
{
   DoCapture(wxT("toolbars"));
}

void ScreenFrame::OnCaptureSelectionBar(wxCommandEvent & e)
{
   DoCapture(wxT("selectionbar"));
}

void ScreenFrame::OnCaptureTools(wxCommandEvent & e)
{
   DoCapture(wxT("tools"));
}

void ScreenFrame::OnCaptureControl(wxCommandEvent & e)
{
   DoCapture(wxT("control"));
}

void ScreenFrame::OnCaptureMixer(wxCommandEvent & e)
{
   DoCapture(wxT("mixer"));
}

void ScreenFrame::OnCaptureMeter(wxCommandEvent & e)
{
   DoCapture(wxT("meter"));
}

void ScreenFrame::OnCaptureEdit(wxCommandEvent & e)
{
   DoCapture(wxT("edit"));
}

void ScreenFrame::OnCaptureDevice(wxCommandEvent & e)
{
   DoCapture(wxT("device"));
}

void ScreenFrame::OnCaptureTranscription(wxCommandEvent & e)
{
   DoCapture(wxT("transcription"));
}

void ScreenFrame::OnCaptureTrackPanel(wxCommandEvent & e)
{
   DoCapture(wxT("trackpanel"));
}

void ScreenFrame::OnCaptureRuler(wxCommandEvent & e)
{
   DoCapture(wxT("ruler"));
}

void ScreenFrame::OnCaptureTracks(wxCommandEvent & e)
{
   DoCapture(wxT("tracks"));
}

void ScreenFrame::OnCaptureFirstTrack(wxCommandEvent & e)
{
   DoCapture(wxT("firsttrack"));
}

void ScreenFrame::OnCaptureSecondTrack(wxCommandEvent & e)
{
   DoCapture(wxT("secondtrack"));
}

void ScreenFrame::TimeZoom(double seconds)
{
   int width, height;
   mContext.proj->GetClientSize(&width, &height);
   mContext.proj->mViewInfo.zoom = (0.75 * width) / seconds;
   mContext.proj->RedrawProject();
}

void ScreenFrame::OnOneSec(wxCommandEvent & e)
{
   TimeZoom(1.0);
}

void ScreenFrame::OnTenSec(wxCommandEvent & e)
{
   TimeZoom(10.0);
}

void ScreenFrame::OnOneMin(wxCommandEvent & e)
{
   TimeZoom(60.0);
}

void ScreenFrame::OnFiveMin(wxCommandEvent & e)
{
   TimeZoom(300.0);
}

void ScreenFrame::OnOneHour(wxCommandEvent & e)
{
   TimeZoom(3600.0);
}

void ScreenFrame::SizeTracks(int h)
{
   TrackListIterator iter(mContext.proj->GetTracks());
   for (Track * t = iter.First(); t; t = iter.Next()) {
      if (t->GetKind() == Track::Wave) {
         if (t->GetLink()) {
            t->SetHeight(h);
         }
         else {
            t->SetHeight(h*2);
         }
      }
   }
   mContext.proj->RedrawProject();
}

void ScreenFrame::OnShortTracks(wxCommandEvent & e)
{
   TrackListIterator iter(mContext.proj->GetTracks());
   for (Track * t = iter.First(); t; t = iter.Next()) {
      if (t->GetKind() == Track::Wave) {
         t->SetHeight(t->GetMinimizedHeight());
      }
   }
   mContext.proj->RedrawProject();
}

void ScreenFrame::OnMedTracks(wxCommandEvent & e)
{
   SizeTracks(60);
}

void ScreenFrame::OnTallTracks(wxCommandEvent & e)
{
   SizeTracks(85);
}

// Indentation settings for Vim and Emacs.
// Please do not modify past this point.
//
// Local Variables:
// c-basic-offset: 3
// indent-tabs-mode: nil
// End:
//
// vim: et sts=3 sw=3
