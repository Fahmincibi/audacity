/**********************************************************************

  Audacity: A Digital Audio Editor

  Project.h

  Dominic Mazzoni

  In Audacity, the main window you work in is called a project.
  Projects can contain an arbitrary number of tracks of many
  different types, but if a project contains just one or two
  tracks then it can be saved in standard formats like WAV or AIFF.
  This window is the one that contains the menu bar (except on
  the Mac).

**********************************************************************/

#ifndef __AUDACITY_PROJECT__
#define __AUDACITY_PROJECT__

#include "Audacity.h"

#include "DirManager.h"
#include "UndoManager.h"
#include "ViewInfo.h"
#include "TrackPanel.h"
#include "AudioIO.h"
#include "commands/CommandManager.h"
#include "effects/EffectManager.h"
#include "xml/XMLTagHandler.h"
#include "toolbars/SelectionBar.h"
#include "FreqWindow.h"

#include <wx/defs.h>
#include <wx/event.h>
#include <wx/log.h>
#include <wx/dragimag.h>
#include <wx/generic/dragimgg.h>
#include <wx/frame.h>
#include <wx/intl.h>
#include <wx/dcclient.h>

const int AudacityProjectTimerID = 5200;

class wxWindow;
class wxDialog;
class wxBoxSizer;
class wxScrollEvent;
class wxScrollBar;
class wxPanel;

class ToolManager;
class Toolbar;
class ControlToolBar;
class DeviceToolBar;
class EditToolBar;
class MeterToolBar;
class MixerToolBar;
class SelectionToolBar;
class ToolsToolBar;
class TranscriptionToolBar;

class TrackList;
class Tags;
class HistoryWindow;
#ifdef EXPERIMENTAL_LYRICS_WINDOW
   class LyricsWindow;
#endif
#ifdef EXPERIMENTAL_MIXER_BOARD
   class MixerBoard;
   class MixerBoardFrame;
#endif
class Importer;
class AdornedRulerPanel;

class AudacityProject;
class RecordingRecoveryHandler;
class ODLock;

AudacityProject *CreateNewAudacityProject();
AUDACITY_DLL_API AudacityProject *GetActiveProject();
void RedrawAllProjects();
void RefreshCursorForAllProjects();
AUDACITY_DLL_API void CloseAllProjects();

void GetDefaultWindowRect(wxRect *defRect);
void GetNextWindowPlacement(wxRect *nextRect, bool *bMaximized);

WX_DEFINE_ARRAY(AudacityProject *, AProjectArray);

extern AProjectArray gAudacityProjects;


WX_DEFINE_ARRAY(wxMenu *, MenuArray);

enum PlayMode {
   normalPlay,
   oneSecondPlay,
   loopedPlay
};

// XML handler for <import> tag
class ImportXMLTagHandler : public XMLTagHandler 
{
 public:
   ImportXMLTagHandler(AudacityProject* pProject) { mProject = pProject; };

   virtual bool HandleXMLTag(const wxChar *tag, const wxChar **attrs);
   virtual XMLTagHandler *HandleXMLChild(const wxChar *tag) { return NULL; };
   virtual void WriteXML(XMLWriter &xmlFile) { wxASSERT(false); } //vvv todo
 private: 
   AudacityProject* mProject;
};

class AUDACITY_DLL_API AudacityProject:  public wxFrame,
                                     public TrackPanelListener,
                                     public SelectionBarListener,
                                     public XMLTagHandler,
                                     public AudioIOListener
{
 public:

   // Constructor and Destructor

   AudacityProject(wxWindow * parent, wxWindowID id,
                   const wxPoint & pos, const wxSize & size);

   virtual ~ AudacityProject();

   TrackList *GetTracks() { return mTracks; };
   UndoManager *GetUndoManager() { return &mUndoManager; }

   sampleFormat GetDefaultFormat() { return mDefaultFormat; }

   double GetRate() { return mRate; }
   double GetZoom() { return mViewInfo.zoom; }
   double GetSel0() { return mViewInfo.sel0; }
   double GetSel1() { return mViewInfo.sel1; }

   Track *GetFirstVisible();
   void UpdateFirstVisible();

   void GetPlayRegion(double* playRegionStart, double *playRegionEnd);
   bool IsPlayRegionLocked() { return mLockPlayRegion; }
   
   void SetSel0(double);        //Added by STM 
   void SetSel1(double);        //Added by STM 


   bool Clipboard() { return msClipLen > 0.0; }

   wxString GetName();
   DirManager *GetDirManager();
   TrackFactory *GetTrackFactory();
   AdornedRulerPanel *GetRulerPanel();
   Tags *GetTags();
   int GetAudioIOToken();
   void SetAudioIOToken(int token);

   bool IsActive();

   // File I/O

   static wxArrayString ShowOpenDialog(wxString extra = wxEmptyString);
   static void OpenFiles(AudacityProject *proj);
   void OpenFile(wxString fileName, bool addtohistory = true);
   bool WarnOfLegacyFile( );

   // If pNewTrackList is passed in non-NULL, it gets filled with the pointers to new tracks.
   void Import(wxString fileName, WaveTrackArray *pTrackArray = NULL); 

   void AddImportedTracks(wxString fileName,
                          Track **newTracks, int numTracks);
   bool Save(bool overwrite = true, bool fromSaveAs = false, bool bWantSaveCompressed = false);
   bool SaveAs(bool bWantSaveCompressed = false);
   #ifdef USE_LIBVORBIS
      bool SaveCompressedWaveTracks(const wxString strProjectPathName); // full path for aup except extension
   #endif
   void Clear();

   wxString GetFileName() { return mFileName; }
   bool GetDirty() { return mDirty; }
   void SetProjectTitle();

   bool GetIsEmpty() { return mTracks->IsEmpty(); }

   bool GetTracksFitVerticallyZoomed() { return mTracksFitVerticallyZoomed; } //lda
   void SetTracksFitVerticallyZoomed(bool flag) { mTracksFitVerticallyZoomed = flag; } //lda

   bool GetShowId3Dialog() { return mShowId3Dialog; } //lda
   void SetShowId3Dialog(bool flag) { mShowId3Dialog = flag; } //lda

   bool GetCleanSpeechMode() { return mCleanSpeechMode; } //lda
   void SetCleanSpeechMode(bool flag) { mCleanSpeechMode = flag; } //lda

   bool GetNormalizeOnLoad() { return mNormalizeOnLoad; } //lda
   void SetNormalizeOnLoad(bool flag) { mNormalizeOnLoad = flag; } //lda

#include "Menus.h"

   CommandManager *GetCommandManager() { return &mCommandManager; }

   void RebuildMenuBar();
   void RebuildOtherMenus();
   void MayStartMonitoring();

 public:

   // Message Handlers

   void OnMenuEvent(wxMenuEvent & event);
   void OnMenu(wxCommandEvent & event);
   void OnUpdateUI(wxUpdateUIEvent & event);

   void OnActivate(wxActivateEvent & event);
   void OnMouseEvent(wxMouseEvent & event);
   void OnIconize(wxIconizeEvent &event);
   void OnSize(wxSizeEvent & event);
   void OnScroll(wxScrollEvent & event);
   void OnCloseWindow(wxCloseEvent & event);
   void OnTimer(wxTimerEvent & event);
   void OnToolBarUpdate(wxCommandEvent & event);
   void OnOpenAudioFile(wxCommandEvent & event);
   void OnCaptureKeyboard(wxCommandEvent & event);
   void OnReleaseKeyboard(wxCommandEvent & event);
   void OnODTaskUpdate(wxCommandEvent & event);
   void OnODTaskComplete(wxCommandEvent & event);
   void OnTrackListUpdated(wxCommandEvent & event);
   bool HandleKeyDown(wxKeyEvent & event);
   bool HandleChar(wxKeyEvent & event);
   bool HandleKeyUp(wxKeyEvent & event);

   void HandleResize();
   void UpdateLayout();

   // Other commands

   static void DeleteClipboard();
   static void DeleteAllProjectsDeleteLock();

   void UpdateMenus();
   void UpdatePrefs();
   void UpdatePrefsVariables();
   void RedrawProject(const bool bForceWaveTracks = false);
   void RefreshCursor();
   void SelectNone();
   void SelectAllIfNone();
   void Zoom(double level);
   void Rewind(bool shift);
   void SkipEnd(bool shift);
   void SetStop(bool bStopped);
   void EditByLabel( WaveTrack::EditFunction action ); 
   void EditClipboardByLabel( WaveTrack::EditDestFunction action );
   bool IsSticky();
   bool GetStickyFlag() { return mStickyFlag; };
   void SetStickyFlag(bool flag) { mStickyFlag = flag; };

   // Snap To

   void SetSnapTo(bool state);
   bool GetSnapTo();

   // Scrollbars

   void OnScrollLeft();
   void OnScrollRight();

   void OnScrollLeftButton(wxScrollEvent & event);
   void OnScrollRightButton(wxScrollEvent & event);

   void FinishAutoScroll();
   void FixScrollbars();

   // TrackPanel callback methods

   virtual wxSize TP_GetTracksUsableArea();
   virtual void TP_DisplayStatusMessage(wxString msg);
   virtual void TP_DisplaySelection();
   virtual int TP_GetCurrentTool();
   virtual void TP_OnPlayKey();
   virtual void TP_PushState(wxString longDesc, wxString shortDesc,
                             bool consolidate);
   virtual void TP_ModifyState();
   virtual void TP_RedrawScrollbars();
   virtual void TP_ScrollLeft();
   virtual void TP_ScrollRight();
   virtual void TP_ScrollWindow(double scrollto);
   virtual void TP_ScrollUpDown(int delta);
   virtual void TP_HandleResize();
   virtual ControlToolBar * TP_GetControlToolBar();
   virtual ToolsToolBar * TP_GetToolsToolBar();

   // ToolBar

   ControlToolBar *GetControlToolBar();
   DeviceToolBar *GetDeviceToolBar();
   EditToolBar *GetEditToolBar();
   MeterToolBar *GetMeterToolBar();
   MixerToolBar *GetMixerToolBar();
   SelectionBar *GetSelectionBar();
   ToolsToolBar *GetToolsToolBar();
   TranscriptionToolBar *GetTranscriptionToolBar();

   #ifdef EXPERIMENTAL_LYRICS_WINDOW
      LyricsWindow* GetLyricsWindow() { return mLyricsWindow; };
   #endif
   #ifdef EXPERIMENTAL_MIXER_BOARD
      MixerBoard* GetMixerBoard() { return mMixerBoard; };
   #endif

 public:

   // SelectionBar callback methods

   virtual void AS_SetRate(double rate);
   virtual void AS_ModifySelection(double &start, double &end);
   virtual void AS_SetSnapTo(bool state);
   virtual bool AS_GetSnapTo();

   void SetStateTo(unsigned int n);

   // XMLTagHandler callback methods

   virtual bool HandleXMLTag(const wxChar *tag, const wxChar **attrs);
   virtual XMLTagHandler *HandleXMLChild(const wxChar *tag);
   virtual void WriteXML(XMLWriter &xmlFile);

   void WriteXMLHeader(XMLWriter &xmlFile);

   PlayMode mLastPlayMode;
   ViewInfo mViewInfo;

   wxWindow *HasKeyboardCapture();
   void CaptureKeyboard(wxWindow *h);
   void ReleaseKeyboard(wxWindow *h);
   
   // Audio IO callback methods
   virtual void OnAudioIORate(int rate);
   virtual void OnAudioIOStartRecording();
   virtual void OnAudioIOStopRecording();
   virtual void OnAudioIONewBlockFiles(const wxString& blockFileLog);

   // Command Handling
   bool TryToMakeActionAllowed( wxUint32 & flags, wxUint32 flagsRqd, wxUint32 mask );

   ///Prevents delete from external thread - for e.g. use of GetActiveProject 
   static void AllProjectsDeleteLock();
   static void AllProjectsDeleteUnlock();
   
   void PushState(wxString desc, wxString shortDesc,
                  bool consolidate = false);

   FreqWindow *mFreqWindow;

 private:

   void ClearClipboard();
   void InitialState();
   void ModifyState();
   void PopState(TrackList * l);
   
   #ifdef EXPERIMENTAL_LYRICS_WINDOW
      void UpdateLyrics();
   #endif
   #ifdef EXPERIMENTAL_MIXER_BOARD
      void UpdateMixerBoard();
   #endif
   
   void GetRegionsByLabel( Regions &regions );
   
   void AutoSaveIfNeeded();
   void AutoSave();
   static bool IsAutoSaveEnabled();
   void DeleteCurrentAutoSaveFile();
   
   static bool GetCacheBlockFiles();

   // The project's name and file info
   wxString mFileName;
   DirManager *mDirManager; // MM: DirManager now created dynamically

   double mRate;
   sampleFormat mDefaultFormat;

   // Recent files
   wxMenu *mRecentFilesMenu;

   // Tags (artist name, song properties, MP3 ID3 info, etc.)
   Tags *mTags;

   // List of tracks and display info
   TrackList *mTracks;

   bool mSnapTo;

   TrackList *mLastSavedTracks;

   // Clipboard (static because it is shared by all projects)
   static TrackList *msClipboard;
   static AudacityProject *msClipProject;
   static double msClipLen;

   //shared by all projects
   static ODLock *msAllProjectDeleteMutex;

   // History/Undo manager
   UndoManager mUndoManager;
   bool mDirty;

   // Commands

   CommandManager mCommandManager;

   wxUint32 mLastFlags;

   // see AudacityProject::OnUpdateUI() for explanation of next two
   bool mInIdle;
   wxUint32 mTextClipFlag;

   // Window elements

   wxTimer *mTimer;
   long mLastStatusUpdateTime;

   wxStatusBar *mStatusBar;

   AdornedRulerPanel *mRuler;
   TrackPanel *mTrackPanel;
   TrackFactory *mTrackFactory;
   wxPanel * mMainPanel;
   wxScrollBar *mHsbar;
   wxScrollBar *mVsbar;
   bool mAutoScrolling;
   bool mActive;
   bool mIconized;

   HistoryWindow *mHistoryWindow;
   #ifdef EXPERIMENTAL_LYRICS_WINDOW
      LyricsWindow* mLyricsWindow;
   #endif
   #ifdef EXPERIMENTAL_MIXER_BOARD
      MixerBoard* mMixerBoard;
      MixerBoardFrame* mMixerBoardFrame;
   #endif


 public:
   ToolManager *mToolManager;
   bool mShowSplashScreen;
   wxString mHelpPref;

 private:
   int  mAudioIOToken;

   bool mIsDeleting;
   bool mTracksFitVerticallyZoomed;  //lda
   bool mNormalizeOnLoad;  //lda
   bool mCleanSpeechMode;  //lda
   bool mShowId3Dialog; //lda
   bool mEmptyCanBeDirty;
   bool mSelectAllOnNone;
   
   bool mStickyFlag;

   bool mLockPlayRegion;

   // See AudacityProject::OnActivate() for an explanation of this.
   wxWindow *mLastFocusedWindow;

   wxWindow *mKeyboardCaptured;

   ImportXMLTagHandler* mImportXMLTagHandler;

   // Last auto-save file name and path (empty if none)
   wxString mAutoSaveFileName;
   
   // When the last auto-save took place (as returned wx wxGetLocalTime)
   long mLastAutoSaveTime;
   
   // Are we currently auto-saving or not?
   bool mAutoSaving;

   // Has this project been recovered from an auto-saved version
   bool mIsRecovered;
   
   // The auto-save data dir the project has been recovered from
   wxString mRecoveryAutoSaveDataDir;
   
   // The handler that handles recovery of <recordingrecovery> tags
   RecordingRecoveryHandler* mRecordingRecoveryHandler;

   // Dependencies have been imported and a warning should be shown on save
   bool mImportedDependencies;


   bool mWantSaveCompressed;
   wxArrayString mStrOtherNamesArray; // used to make sure compressed file names are unique
   
   // Last effect applied to this project
   Effect *mLastEffect;
   int mLastEffectType;
   wxString mLastEffectDesc;

 private:

   // The screenshot class needs to access internals
   friend class ScreenshotCommand;

 public:
    DECLARE_EVENT_TABLE()
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
// arch-tag: bf2e4288-d3d3-411e-b8af-1e8d12814c70

