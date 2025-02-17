/**********************************************************************

  Audacity: A Digital Audio Editor

  Menus.cpp

  Dominic Mazzoni

  This file implements all of the methods that get called when you
  select an item from a menu, along with routines to create the
  menu bar and enable/disable items.  All of the methods here are
  methods of AudacityProject, so for the header file, see Project.h.

**********************************************************************/

#include <wx/wxprec.h>

#ifndef WX_PRECOMP
   #include <wx/app.h>
   #include <wx/menu.h>
   #include <wx/string.h>
#endif

#include <wx/textfile.h>

#include "Audacity.h"
#include "AboutDialog.h"
#include "APalette.h"
#include "Export.h"
#include "FreqWindow.h"
#include "Help.h"
#include "Import.h"
#include "ImportRaw.h"
#include "ImportMIDI.h"
#include "LabelTrack.h"
#include "Mix.h"
#include "NoteTrack.h"
#include "Prefs.h"
#include "Project.h"
#include "Tags.h"
#include "Track.h"
#include "TrackPanel.h"
#include "WaveTrack.h"
#include "effects/Effect.h"
#include "prefs/PrefsDialog.h"

#define AUDACITY_MENUS_ENUM
#include "Menus.h"
#undef AUDACITY_MENUS_ENUM

void AudacityProject::CreateMenuBar()
{
   mMenusDirtyCheck = gMenusDirty;

   mFirstTimeUpdateMenus = true;

   mMenuBar = new wxMenuBar();

   // Note: if you change the titles of any of the menus here,
   // you must also change the title in OnUpdateMenus, below.
   
   wxString pcmFormat =
       gPrefs->Read("/FileFormats/DefaultExportFormat", "WAV");
   mExportString.Printf("&Export as %s...", pcmFormat.c_str());
   mExportSelectionString.Printf("Export &Selection as %s...", pcmFormat.c_str());
   wxString lossyFormat =
       "MP3";
   mExportLossyString.Printf("Export as %s...", lossyFormat.c_str());
   mExportSelectionLossyString.Printf("Export Selection as %s...", lossyFormat.c_str());

   mFileMenu = new wxMenu();
   mFileMenu->Append(NewID, "&New...\tCtrl+N");
   mFileMenu->Append(OpenID, "&Open...\tCtrl+O");
   mFileMenu->Append(CloseID, "&Close\tCtrl+W");
   mFileMenu->Append(SaveID, "&Save Project\tCtrl+S");
   mFileMenu->Append(SaveAsID, "Save Project &As...");
   mFileMenu->AppendSeparator();
   mFileMenu->Append(ExportMixID, mExportString);
   mFileMenu->Append(ExportSelectionID, mExportSelectionString);
   mFileMenu->AppendSeparator();
   mFileMenu->Append(ExportLossyMixID, mExportLossyString);
   mFileMenu->Append(ExportLossySelectionID, mExportSelectionLossyString);
   mFileMenu->AppendSeparator();
   mFileMenu->Append(ExportLabelsID, "Export &Labels...");
   mFileMenu->AppendSeparator();
   mFileMenu->Append(PreferencesID, "&Preferences...\tCtrl+P");
   mFileMenu->AppendSeparator();
   mFileMenu->Append(ExitID, "E&xit");

   mEditMenu = new wxMenu();
   mEditMenu->Append(UndoID, "&Undo\tCtrl+Z");
   mEditMenu->Append(RedoID, "&Redo\tCtrl+R");
   mEditMenu->AppendSeparator();
   mEditMenu->Append(CutID, "Cut\tCtrl+X");
   mEditMenu->Append(CopyID, "Copy\tCtrl+C");
   mEditMenu->Append(PasteID, "Paste\tCtrl+V");
   mEditMenu->AppendSeparator();
   mEditMenu->Append(DeleteID, "&Delete\tCtrl+K");
   mEditMenu->Append(SilenceID, "Silence\tCtrl+L");
   mEditMenu->AppendSeparator();
   mEditMenu->Append(InsertSilenceID, "Insert Silence...");
   mEditMenu->Append(SplitID, "Split\tCtrl+Y");
   mEditMenu->Append(DuplicateID, "Duplicate\tCtrl+D");
   mEditMenu->AppendSeparator();
   mEditMenu->Append(SelectAllID, "&Select All\tCtrl+A");

   mViewMenu = new wxMenu();
   mViewMenu->Append(ZoomInID, "Zoom &In\tCtrl+1");
   mViewMenu->Append(ZoomNormalID, "Zoom &Normal\tCtrl+2");
   mViewMenu->Append(ZoomOutID, "Zoom &Out\tCtrl+3");
   mViewMenu->Append(ZoomFitID, "Fit in &Window\tCtrl+F");
   mViewMenu->AppendSeparator();
   mViewMenu->Append(PlotSpectrumID, "&Plot Spectrum\tCtrl+U");

#ifndef __WXMAC__
   mViewMenu->AppendSeparator();
   mViewMenu->Append(FloatPaletteID, "Float or Unfloat Palette");
#endif

   mProjectMenu = new wxMenu();
   mProjectMenu->Append(ImportID, "&Import Audio...\tCtrl+I");
   mProjectMenu->Append(ImportLabelsID, "Import Labels...");
   mProjectMenu->Append(ImportMIDIID, "Import &MIDI...");
   mProjectMenu->Append(ImportRawID, "Import Raw Data...");
#ifdef USE_ID3LIB
   mProjectMenu->AppendSeparator();
   mProjectMenu->Append(EditID3ID, "Edit ID3 Tags...");   
#endif
   mProjectMenu->AppendSeparator();
   mProjectMenu->Append(QuickMixID, "&Quick Mix");
   mProjectMenu->AppendSeparator();
   mProjectMenu->Append(AlignID, "Align Tracks Together");
   mProjectMenu->Append(AlignZeroID, "Align with Zero");
   mProjectMenu->AppendSeparator();
   mProjectMenu->Append(NewWaveTrackID, "New &Audio Track");
   mProjectMenu->Append(NewLabelTrackID, "New &Label Track");
   mProjectMenu->AppendSeparator();
   mProjectMenu->Append(RemoveTracksID, "&Remove Track(s)");

   /*
      mTrackMenu = new wxMenu();
      mTrackMenu->Append(QuickMixID, "Quick Mix");
      mTrackMenu->AppendSeparator();
      mTrackMenu->Append(WaveDisplayID, "Waveform Display");
      mTrackMenu->Append(SpectrumDisplayID, "Spectrum Display");
      mTrackMenu->AppendSeparator();
      //  mTrackMenu->Append(AutoCorrelateID, "AutoCorrelate");
      mTrackMenu->Append(PitchID, "Pitch Extract");
    */

   mEffectMenu = new wxMenu();

   int numEffects = Effect::GetNumEffects();
   for (int fi = 0; fi < numEffects; fi++)
      mEffectMenu->Append(FirstEffectID + fi,
                          (Effect::GetEffect(fi))->GetEffectName());

#ifdef __WXMAC__
   wxApp::s_macAboutMenuItemId = AboutID;
#endif

   mHelpMenu = new wxMenu();
   mHelpMenu->Append(AboutID, "About Audacity...");

#ifndef __WXMAC__
   mHelpMenu->AppendSeparator();

   mHelpMenu->Append(HelpID, "Online Help...");
   mHelpMenu->Append(HelpIndexID, "Online Help Index...");
   mHelpMenu->Append(HelpSearchID, "Search Online Help...");
   mHelpMenu->AppendSeparator();
   mHelpMenu->Append(SetHelpFileID, "Locate Help File...");
#endif

   mMenuBar->Append(mFileMenu, "&File");
   mMenuBar->Append(mEditMenu, "&Edit");
   mMenuBar->Append(mViewMenu, "&View");
   mMenuBar->Append(mProjectMenu, "&Project");
   //  mMenuBar->Append(mTrackMenu, "&Track");
   mMenuBar->Append(mEffectMenu, "E&ffect");
   mMenuBar->Append(mHelpMenu, "&Help");

   SetMenuBar(mMenuBar);

   mInsertSilenceAmount = 1.0;
}

void AudacityProject::OnUpdateMenus(wxUpdateUIEvent & event)
{
   if (mMenusDirtyCheck != gMenusDirty) {
      /* This no longer crashes, at least not on GTK */
      
      wxString pcmFormat =
       gPrefs->Read("/FileFormats/DefaultExportFormat", "WAV");
      wxString lossyFormat = "MP3";

#ifdef __WXMSW__
      mFileMenu->FindItem(ExportMixID)->SetText(
		      "Export as " + pcmFormat + "...");
      mFileMenu->FindItem(ExportSelectionID)->SetText(
		      "Export Selection as " + pcmFormat + "...");
      mFileMenu->FindItem(ExportLossyMixID)->SetText(
		      "Export as " + lossyFormat + "...");
      mFileMenu->FindItem(ExportLossySelectionID)->SetText(
		      "Export as " + lossyFormat + "...");
#else     
      mFileMenu->FindItem(ExportMixID)->SetName(
		      "Export as " + pcmFormat + "...");
      mFileMenu->FindItem(ExportSelectionID)->SetName(
		      "Export Selection as " + pcmFormat + "...");
      mFileMenu->FindItem(ExportLossyMixID)->SetName(
		      "Export as " + lossyFormat + "...");
      mFileMenu->FindItem(ExportLossySelectionID)->SetName(
		      "Export as " + lossyFormat + "...");
#endif
   }

   // Note that the titles of the menus here are dependent on the
   // titles above.

   mEditMenu->Enable(mEditMenu->FindItem("Undo"),
                     mUndoManager.UndoAvailable());
   mEditMenu->Enable(mEditMenu->FindItem("Redo"),
                     mUndoManager.RedoAvailable());

   bool nonZeroRegionSelected = (mViewInfo.sel1 > mViewInfo.sel0);

   int numTracks = 0;
   int numTracksSelected = 0;
   int numWaveTracks = 0;
   int numWaveTracksSelected = 0;
   int numLabelTracks = 0;

   TrackListIterator iter(mTracks);
   VTrack *t = iter.First();
   while (t) {
      numTracks++;
      if (t->GetKind() == VTrack::Wave)
         numWaveTracks++;
      if (t->GetKind() == VTrack::Label)
         numLabelTracks++;
      if (t->selected) {
         numTracksSelected++;
         if (t->GetKind() == VTrack::Wave)
            numWaveTracksSelected++;
      }
      t = iter.Next();
   }

   mEditMenu->Enable(mEditMenu->FindItem("Paste"),
                     numTracksSelected > 0 && msClipLen > 0.0);

   // Return from this function if nothing's changed since
   // the last time we were here.

   if (!mFirstTimeUpdateMenus &&
       mLastNonZeroRegionSelected==nonZeroRegionSelected &&
       mLastNumTracks==numTracks &&
       mLastNumTracksSelected==numTracksSelected &&
       mLastNumWaveTracks==numWaveTracks &&
       mLastNumWaveTracksSelected==numWaveTracksSelected &&
       mLastNumLabelTracks==numLabelTracks)
      return;

   // Otherwise, save state and then update all of the menus

   mFirstTimeUpdateMenus = false;
   mLastNonZeroRegionSelected = nonZeroRegionSelected;
   mLastNumTracks = numTracks;
   mLastNumTracksSelected = numTracksSelected;
   mLastNumWaveTracks = numWaveTracks;
   mLastNumWaveTracksSelected = numWaveTracksSelected;
   mLastNumLabelTracks = numLabelTracks;

   mFileMenu->Enable(mFileMenu->FindItem(mExportString), numTracks > 0);
   mFileMenu->Enable(mFileMenu->FindItem(mExportSelectionString),
                     numTracksSelected > 0 && nonZeroRegionSelected);
   mFileMenu->Enable(mFileMenu->FindItem(mExportLossyString), numTracks > 0);
   mFileMenu->Enable(mFileMenu->FindItem(mExportSelectionLossyString),
                     numTracksSelected > 0 && nonZeroRegionSelected);
   mFileMenu->Enable(mFileMenu->FindItem("Export Labels..."),
                     numLabelTracks > 0);

   mEditMenu->Enable(mEditMenu->FindItem("Cut"),
                     numTracksSelected > 0 && nonZeroRegionSelected);
   mEditMenu->Enable(mEditMenu->FindItem("Copy"),
                     numTracksSelected > 0 && nonZeroRegionSelected);
   mEditMenu->Enable(mEditMenu->FindItem("Delete"),
                     numTracksSelected > 0 && nonZeroRegionSelected);
   mEditMenu->Enable(mEditMenu->FindItem("Silence"),
                     numTracksSelected > 0 && nonZeroRegionSelected);
   mEditMenu->Enable(mEditMenu->FindItem("Insert Silence..."),
                     numTracksSelected > 0);
   mEditMenu->Enable(mEditMenu->FindItem("Split"),
                     numTracksSelected > 0 && nonZeroRegionSelected);
   mEditMenu->Enable(mEditMenu->FindItem("Duplicate"),
                     numTracksSelected > 0 && nonZeroRegionSelected);
   mEditMenu->Enable(mEditMenu->FindItem("Select All"), numTracks > 0);

   mViewMenu->Enable(mViewMenu->FindItem("Plot Spectrum"),
                     numWaveTracksSelected > 0 && nonZeroRegionSelected);

   mProjectMenu->Enable(mProjectMenu->FindItem("Quick Mix"),
                        numWaveTracksSelected > 1);

   mProjectMenu->Enable(mProjectMenu->FindItem("Align Tracks Together"),
                        numTracksSelected > 1);
   mProjectMenu->Enable(mProjectMenu->FindItem("Align with Zero"),
                        numTracksSelected > 0);
   mProjectMenu->Enable(mProjectMenu->FindItem("Remove Track(s)"),
                        numTracksSelected > 0);

   for (int e = 0; e < Effect::GetNumEffects(); e++) {
      Effect *f = Effect::GetEffect(e);
      mEffectMenu->Enable(mEffectMenu->FindItem(f->GetEffectName()),
                          numWaveTracksSelected > 0
                          && nonZeroRegionSelected);
   }
}

//
// File Menu
//

void AudacityProject::OnNew(wxCommandEvent & event)
{
   CreateNewAudacityProject(gParentWindow);
}

void AudacityProject::OnOpen(wxCommandEvent & event)
{
   wxString path = gPrefs->Read("/DefaultOpenPath",::wxGetCwd());

   wxString fileName = wxFileSelector("Select an audio file...",
                                      path,     // Path
                                      "",       // Name
                                      "",       // Extension
                                      "All files (*.*)|*.*|"
                                      "Audacity projects (*.aup)|*.aup|"
                                      "WAV files (*.wav)|*.wav|"
                                      "AIFF files (*.aif)|*.aif|"
                                      "AU files (*.au)|*.au|"
                                      "IRCAM files (*.snd)|*.snd|"
                                      "MP3 files (*.mp3)|*.mp3",
                                      0,        // Flags
                                      this);    // Parent

   if (fileName != "") {

      path =::wxPathOnly(fileName);
      gPrefs->Write("/DefaultOpenPath", path);

      if (mDirty) {
         AudacityProject *project =
             CreateNewAudacityProject(gParentWindow);
         project->OpenFile(fileName);
      } else
         OpenFile(fileName);
   }
}

void AudacityProject::OnSave(wxCommandEvent & event)
{
   Save();
}

void AudacityProject::OnSaveAs(wxCommandEvent & event)
{
   SaveAs();
}

void AudacityProject::OnExit(wxCommandEvent & event)
{
   QuitAudacity();
}

void AudacityProject::OnExportLabels(wxCommandEvent & event)
{
   VTrack *t;
   int numLabelTracks = 0;

   TrackListIterator iter(mTracks);

   t = iter.First();
   while (t) {
      if (t->GetKind() == VTrack::Label)
         numLabelTracks++;
      t = iter.Next();
   }

   if (numLabelTracks == 0) {
      wxMessageBox("There are no label tracks to export.");
      return;
   }

   wxString fName = "labels.txt";

   fName = wxFileSelector("Export Labels As:",
                          NULL,
                          fName,
                          "txt",
                          "*.txt", wxSAVE | wxOVERWRITE_PROMPT, this);

   if (fName == "")
      return;

   wxTextFile f(fName);
#ifdef __WXMAC__
   wxFile *temp = new wxFile();
   temp->Create(fName);
   delete temp;
#else
   f.Create();
#endif
   f.Open();
   if (!f.IsOpened()) {
      wxMessageBox("Couldn't write to " + fName);
      return;
   }

   t = iter.First();
   while (t) {
      if (t->GetKind() == VTrack::Label)
         ((LabelTrack *) t)->Export(f);

      t = iter.Next();
   }

#ifdef __WXMAC__
   f.Write(wxTextFileType_Mac);
#else
   f.Write();
#endif
   f.Close();
}

void AudacityProject::OnExportMix(wxCommandEvent & event)
{
   wxString format =
       gPrefs->Read("/FileFormats/DefaultExportFormat", "WAV");   

   ::Export(this, format, false, 0.0, mTracks->GetMaxLen());
}

void AudacityProject::OnExportSelection(wxCommandEvent & event)
{
   wxString format =
       gPrefs->Read("/FileFormats/DefaultExportFormat", "WAV");   

   ::Export(this, format, true, mViewInfo.sel0, mViewInfo.sel1);
}

void AudacityProject::OnExportLossyMix(wxCommandEvent & event)
{
   // Until Ogg Vorbis is supported, this is hard-coded to MP3
   ::Export(this, "MP3", false, 0.0, mTracks->GetMaxLen());
}

void AudacityProject::OnExportLossySelection(wxCommandEvent & event)
{
   // Until Ogg Vorbis is supported, this is hard-coded to MP3
   ::Export(this, "MP3", true, mViewInfo.sel0, mViewInfo.sel1);
}

void AudacityProject::OnPreferences(wxCommandEvent & event)
{
   PrefsDialog dialog(this /* parent */ );
   dialog.ShowModal();
}

//
// Edit Menu
//

void AudacityProject::Undo(wxCommandEvent & event)
{
   if (!mUndoManager.UndoAvailable()) {
      wxMessageBox("Nothing to undo");
      return;
   }

   TrackList *l = mUndoManager.Undo(&mViewInfo.sel0, &mViewInfo.sel1);
   PopState(l);

   FixScrollbars();
   //UpdateMenus();
   mTrackPanel->Refresh(false);
}

void AudacityProject::Redo(wxCommandEvent & event)
{
   if (!mUndoManager.RedoAvailable()) {
      wxMessageBox("Nothing to redo");
      return;
   }

   TrackList *l = mUndoManager.Redo(&mViewInfo.sel0, &mViewInfo.sel1);
   PopState(l);

   FixScrollbars();
   //UpdateMenus();
   mTrackPanel->Refresh(false);
}

void AudacityProject::Cut(wxCommandEvent & event)
{
   ClearClipboard();

   TrackListIterator iter(mTracks);

   VTrack *n = iter.First();
   VTrack *dest = 0;

   while (n) {
      if (n->selected) {
         n->Cut(mViewInfo.sel0, mViewInfo.sel1, &dest);
         if (dest)
            msClipboard->Add(dest);
      }
      n = iter.Next();
   }

   msClipLen = (mViewInfo.sel1 - mViewInfo.sel0);
   msClipProject = this;

   mViewInfo.sel1 = mViewInfo.sel0;

   PushState();

   FixScrollbars();
   mTrackPanel->Refresh(false);
   //UpdateMenus();
}

void AudacityProject::Copy(wxCommandEvent & event)
{
   ClearClipboard();

   TrackListIterator iter(mTracks);

   VTrack *n = iter.First();
   VTrack *dest = 0;

   while (n) {
      if (n->selected) {
         n->Copy(mViewInfo.sel0, mViewInfo.sel1, &dest);
         if (dest)
            msClipboard->Add(dest);
      }
      n = iter.Next();
   }

   msClipLen = (mViewInfo.sel1 - mViewInfo.sel0);
   msClipProject = this;

   //mViewInfo.sel1 = mViewInfo.sel0;
   //UpdateMenus();

   //  PushState();
   //  Not an undoable operation
}

void AudacityProject::Paste(wxCommandEvent & event)
{
   if (mViewInfo.sel0 != mViewInfo.sel1)
      Clear();

   wxASSERT(mViewInfo.sel0 == mViewInfo.sel1);

   double tsel = mViewInfo.sel0;

   TrackListIterator iter(mTracks);
   TrackListIterator clipIter(msClipboard);

   VTrack *n = iter.First();
   VTrack *c = clipIter.First();

   while (n && c) {
      if (n->selected) {

         if (msClipProject != this && c->GetKind()==VTrack::Wave)
            ((WaveTrack *)c)->Lock();

         n->Paste(tsel, c);

         if (msClipProject != this && c->GetKind()==VTrack::Wave)
            ((WaveTrack *)c)->Unlock();

         c = clipIter.Next();
      }

      n = iter.Next();
   }

   // TODO: What if we clicked past the end of the track?

   mViewInfo.sel0 = tsel;
   mViewInfo.sel1 = tsel + msClipLen;

   PushState();

   FixScrollbars();
   mTrackPanel->Refresh(false);
   //UpdateMenus();
}

void AudacityProject::OnDelete(wxCommandEvent & event)
{
   Clear();
}

void AudacityProject::OnSilence(wxCommandEvent & event)
{
   TrackListIterator iter(mTracks);

   VTrack *n = iter.First();
   VTrack *dest = 0;

   while (n) {
      if (n->selected)
         n->Silence(mViewInfo.sel0, mViewInfo.sel1);

      n = iter.Next();
   }

   PushState();

   mTrackPanel->Refresh(false);
}

void AudacityProject::OnDuplicate(wxCommandEvent & event)
{
   TrackListIterator iter(mTracks);

   VTrack *n = iter.First();
   VTrack *dest = 0;

   TrackList newTracks;

   while (n) {
      if (n->selected) {
         n->Copy(mViewInfo.sel0, mViewInfo.sel1, &dest);
         if (dest) {
            dest->name = n->name;
            dest->linked = n->linked;

            // Matt Brubeck's fix
            // Use max( offset, sel0 ) as new offset
            double newOffset = n->tOffset;
            if (mViewInfo.sel0 > newOffset)
               newOffset = mViewInfo.sel0;
            dest->tOffset = newOffset;

            newTracks.Add(dest);
         }
      }
      n = iter.Next();
   }

   TrackListIterator nIter(&newTracks);
   n = nIter.First();
   while (n) {
      mTracks->Add(n);
      n = nIter.Next();
   }

   PushState();

   FixScrollbars();
   mTrackPanel->Refresh(false);
}

void AudacityProject::OnSplit(wxCommandEvent & event)
{
   TrackListIterator iter(mTracks);

   VTrack *n = iter.First();
   VTrack *dest = 0;

   TrackList newTracks;

   while (n) {
      if (n->selected) {
         double sel0 = mViewInfo.sel0;
         double sel1 = mViewInfo.sel1;
         
         n->Copy(mViewInfo.sel0, mViewInfo.sel1, &dest);
         if (dest) {
            dest->linked = n->linked;
            dest->tOffset = wxMax(sel0, n->tOffset);

            if (sel1 >= n->GetMaxLen())
               n->Clear(sel0, sel1);
            else if (sel0 <= n->tOffset) {
               n->Clear(sel0, sel1);
               n->tOffset = sel1;
            }
            else
               n->Silence(sel0, sel1);

            newTracks.Add(dest);
         }
      }
      n = iter.Next();
   }

   TrackListIterator nIter(&newTracks);
   n = nIter.First();
   while (n) {
      mTracks->Add(n);
      n = nIter.Next();
   }

   PushState();

   FixScrollbars();
   mTrackPanel->Refresh(false);
}

void AudacityProject::OnInsertSilence(wxCommandEvent & event)
{
   wxString amountStr =
       wxGetTextFromUser("Number of seconds of silence to insert:",
                         "Insert Silence",
                         wxString::Format("%lf", mInsertSilenceAmount),
                         this, -1, -1, TRUE);

   if (amountStr == "")
      return;
   if (!amountStr.ToDouble(&mInsertSilenceAmount)) {
      wxMessageBox("Bad number of seconds.");
      return;
   }
   if (mInsertSilenceAmount <= 0.0) {
      wxMessageBox("Bad number of seconds.");
      return;
   }

   if (mViewInfo.sel0 != mViewInfo.sel1)
      Clear();

   wxASSERT(mViewInfo.sel0 == mViewInfo.sel1);

   TrackListIterator iter(mTracks);

   VTrack *n = iter.First();
   VTrack *dest = 0;

   while (n) {
      if (n->selected)
         n->InsertSilence(mViewInfo.sel0, mInsertSilenceAmount);

      n = iter.Next();
   }

   PushState();

   FixScrollbars();
   mTrackPanel->Refresh(false);
}

void AudacityProject::OnSelectAll(wxCommandEvent & event)
{
   TrackListIterator iter(mTracks);

   VTrack *t = iter.First();
   while (t) {
      t->selected = true;
      t = iter.Next();
   }
   mViewInfo.sel0 = mTracks->GetMinOffset();
   mViewInfo.sel1 = mTracks->GetMaxLen();

   mTrackPanel->Refresh(false);
   //UpdateMenus();
}

//
// View Menu
//

void AudacityProject::OnZoomIn(wxCommandEvent & event)
{
   mViewInfo.zoom *= 2.0;
   if (mViewInfo.zoom > 6000000)
      mViewInfo.zoom = 6000000;
   FixScrollbars();
   mTrackPanel->Refresh(false);
}

void AudacityProject::OnZoomOut(wxCommandEvent & event)
{
   mViewInfo.zoom /= 2.0;
   FixScrollbars();
   mTrackPanel->Refresh(false);
}

void AudacityProject::OnZoomNormal(wxCommandEvent & event)
{
   mViewInfo.zoom = 44100.0 / 512.0;
   FixScrollbars();
   mTrackPanel->Refresh(false);
}

void AudacityProject::OnZoomFit(wxCommandEvent & event)
{
   ZoomFit();
}

void AudacityProject::OnPlotSpectrum(wxCommandEvent & event)
{
   int selcount = 0;
   WaveTrack *selt;
   TrackListIterator iter(mTracks);
   VTrack *t = iter.First();
   while (t) {
      if (t->selected)
         selcount++;
      if (t->GetKind() == VTrack::Wave)
         selt = (WaveTrack *) t;
      t = iter.Next();
   }
   if (selcount != 1) {
      wxMessageBox("Please select a single track first.\n");
      return;
   }

   sampleCount s0 = (sampleCount) ((mViewInfo.sel0 - selt->tOffset)
                                   * selt->rate);
   sampleCount s1 = (sampleCount) ((mViewInfo.sel1 - selt->tOffset)
                                   * selt->rate);
   sampleCount slen = s1 - s0;

   if (slen > 1048576)
      slen = 1048576;

   float *data = new float[slen];
   sampleType *data_sample = new sampleType[slen];

   if (s0 >= selt->numSamples || s0 + slen > selt->numSamples) {
      wxMessageBox("Not enough samples selected.\n");
      delete[]data;
      delete[]data_sample;
      return;
   }

   selt->Get(data_sample, s0, slen);

   for (sampleCount i = 0; i < slen; i++)
      data[i] = data_sample[i] / 32767.;

   gFreqWindow->Plot(slen, data, selt->rate);
   gFreqWindow->Show(true);
   gFreqWindow->Raise();

   delete[]data;
   delete[]data_sample;
}

void AudacityProject::OnFloatPalette(wxCommandEvent & event)
{
   if (gWindowedPalette)
      HideWindowedPalette();
   else
      ShowWindowedPalette();
}

//
// Project Menu
//

void AudacityProject::OnImport(wxCommandEvent & event)
{
   wxString path = gPrefs->Read("/DefaultOpenPath",::wxGetCwd());

   wxString fileName = wxFileSelector("Select an audio file...",
                                      path,     // Path
                                      "",       // Name
                                      "",       // Extension
                                      "All files (*.*)|*.*|"
                                      "WAV files (*.wav)|*.wav|"
                                      "AIFF files (*.aif)|*.aif|"
                                      "AU files (*.au)|*.au|"
                                      "IRCAM files (*.snd)|*.snd|"
                                      "MP3 files (*.mp3)|*.mp3|"
                                      "Ogg Vorbis files (*.ogg)|*.ogg",
                                      0,        // Flags
                                      this);    // Parent

   if (fileName != "") {
      path =::wxPathOnly(fileName);
      gPrefs->Write("/DefaultOpenPath", path);

      Import(fileName);
   }
}

void AudacityProject::OnImportLabels(wxCommandEvent & event)
{
   wxString path = gPrefs->Read("/DefaultOpenPath",::wxGetCwd());

   wxString fileName =
       wxFileSelector("Select a text file containing labels...",
                      path,     // Path
                      "",       // Name
                      ".txt",   // Extension
                      "Text files (*.txt)|*.txt|" "All files (*.*)|*.*",
                      0,        // Flags
                      this);    // Parent

   if (fileName != "") {
      path =::wxPathOnly(fileName);
      gPrefs->Write("/DefaultOpenPath", path);

      wxTextFile f;

      f.Open(fileName);
      if (!f.IsOpened()) {
         wxMessageBox("Couldn't open " + fileName);
         return;
      }

      LabelTrack *newTrack = new LabelTrack(&mDirManager);

      newTrack->Import(f);

      SelectNone();
      mTracks->Add(newTrack);
      newTrack->selected = true;

      PushState();

      FixScrollbars();
      mTrackPanel->Refresh(false);
   }
}

void AudacityProject::OnImportMIDI(wxCommandEvent & event)
{
   wxString path = gPrefs->Read("/DefaultOpenPath",::wxGetCwd());

   wxString fileName = wxFileSelector("Select a MIDI file...",
                                      path,     // Path
                                      "",       // Name
                                      ".mid",   // Extension
                                      "MIDI files (*.mid)|*.mid|"
                                      "All files (*.*)|*.*",
                                      0,        // Flags
                                      this);    // Parent

   if (fileName != "") {
      path =::wxPathOnly(fileName);
      gPrefs->Write("/DefaultOpenPath", path);

      NoteTrack *newTrack = new NoteTrack(&mDirManager);

      if (::ImportMIDI(fileName, newTrack)) {

         SelectNone();
         mTracks->Add(newTrack);
         newTrack->selected = true;

         PushState();

         FixScrollbars();
         mTrackPanel->Refresh(false);
      }
   }
}

void AudacityProject::OnImportRaw(wxCommandEvent & event)
{
   wxString path = gPrefs->Read("/DefaultOpenPath",::wxGetCwd());

   wxString fileName =
       wxFileSelector("Select any uncompressed audio file...",
                      path,     // Path
                      "",       // Name
                      "",       // Extension
                      "All files (*.*)|*.*",
                      0,        // Flags
                      this);    // Parent

   if (fileName != "") {
      path =::wxPathOnly(fileName);
      gPrefs->Write("/DefaultOpenPath", path);

      WaveTrack *left = 0;
      WaveTrack *right = 0;

      if (::ImportRaw(this, fileName, &left, &right, &mDirManager)) {

         SelectNone();

         if (left) {
            mTracks->Add(left);
            left->selected = true;
         }

         if (right) {
            mTracks->Add(right);
            right->selected = true;
         }

         PushState();

         FixScrollbars();
         mTrackPanel->Refresh(false);
      }
   }
}

void AudacityProject::OnEditID3(wxCommandEvent & event)
{
   mTags->ShowEditDialog(this, "Edit ID3 Tags (for MP3 exporting)");
}

void AudacityProject::OnQuickMix(wxCommandEvent & event)
{
   if (::QuickMix(mTracks, &mDirManager)) {

      // After the tracks have been mixed, remove the originals

      TrackListIterator iter(mTracks);
      VTrack *t = iter.First();

      while (t) {
         if (t->selected)
            t = iter.RemoveCurrent();
         else
            t = iter.Next();
      }

      PushState();

      FixScrollbars();
      mTrackPanel->Refresh(false);
   }
}

void AudacityProject::OnAlignZero(wxCommandEvent & event)
{
   TrackListIterator iter(mTracks);
   VTrack *t = iter.First();

   while (t) {
      if (t->selected)
         t->tOffset = 0.0;

      t = iter.Next();
   }

   PushState();

   mTrackPanel->Refresh(false);
   //UpdateMenus();
}

void AudacityProject::OnAlign(wxCommandEvent & event)
{
   double avg = 0.0;
   int num = 0;

   TrackListIterator iter(mTracks);
   VTrack *t = iter.First();

   while (t) {
      if (t->selected) {
         avg += t->tOffset;
         num++;
      }

      t = iter.Next();
   }

   if (num) {
      avg /= num;

      TrackListIterator iter2(mTracks);
      t = iter2.First();

      while (t) {
         if (t->selected)
            t->tOffset = avg;

         t = iter2.Next();
      }
   }

   PushState();

   mTrackPanel->Refresh(false);
   //UpdateMenus();

}

void AudacityProject::OnNewWaveTrack(wxCommandEvent & event)
{
   WaveTrack *t = new WaveTrack(&mDirManager);

   SelectNone();

   mTracks->Add(t);
   t->selected = true;

   PushState();

   FixScrollbars();
   mTrackPanel->Refresh(false);
}

void AudacityProject::OnNewLabelTrack(wxCommandEvent & event)
{
   LabelTrack *t = new LabelTrack(&mDirManager);

   SelectNone();

   mTracks->Add(t);
   t->selected = true;

   PushState();

   FixScrollbars();
   mTrackPanel->Refresh(false);
}

void AudacityProject::OnRemoveTracks(wxCommandEvent & event)
{
   TrackListIterator iter(mTracks);
   VTrack *t = iter.First();

   while (t) {
      if (t->selected)
         t = iter.RemoveCurrent();
      else
         t = iter.Next();
   }

   PushState();

   mTrackPanel->Refresh(false);
   //UpdateMenus();
}

//
// Help Menu
//

void AudacityProject::OnAbout(wxCommandEvent & event)
{
   AboutDialog dlog(this);
   dlog.ShowModal();
}

void AudacityProject::OnHelp(wxCommandEvent & event)
{
   ::ShowHelp(this);
}

void AudacityProject::OnHelpIndex(wxCommandEvent & event)
{
   ::ShowHelpIndex(this);
}

void AudacityProject::OnHelpSearch(wxCommandEvent & event)
{
   ::SearchHelp(this);
}

void AudacityProject::OnSetHelpFile(wxCommandEvent & event)
{
   ::SetHelpFile(this);
}


