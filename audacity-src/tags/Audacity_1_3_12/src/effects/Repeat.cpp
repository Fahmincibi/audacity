/**********************************************************************

  Audacity: A Digital Audio Editor

  Repeat.cpp

  Dominic Mazzoni
  Vaughan Johnson

*******************************************************************//**

\class EffectRepeat
\brief An Effect.

*//****************************************************************//**

\class RepeatDialog
\brief Dialog used with EffectRepeat

*//*******************************************************************/


#include "../Audacity.h"

#include "Repeat.h"
#include "../ShuttleGui.h"
#include "../WaveTrack.h"
#include "../LabelTrack.h"
#include "../widgets/TimeTextCtrl.h"
#include "../Project.h"

#include <wx/button.h>
#include <wx/defs.h>
#include <wx/intl.h>
#include <wx/msgdlg.h>
#include <wx/sizer.h>
#include <wx/stattext.h>
#include <wx/textctrl.h>
#include <wx/validate.h>
#include <wx/valtext.h>

#include <math.h>


EffectRepeat::EffectRepeat()
{
   repeatCount = 10;
}

wxString EffectRepeat::GetEffectDescription() { 
   // Note: This is useful only after values have been set. 
   return wxString::Format(_("Repeated %d times"), repeatCount);
} 

bool EffectRepeat::PromptUser()
{
   //
   // Figure out the maximum number of times the selection
   // could be repeated without overflowing any track
   //
   int maxCount = -1;
   TrackListOfKindIterator iter(Track::Wave, mTracks);
   WaveTrack *track = (WaveTrack *) iter.First();
   while (track) {
      sampleCount trackLen = 
         (sampleCount)((track->GetEndTime() - track->GetStartTime()) * 
                       track->GetRate());
      sampleCount selectionLen = (sampleCount)((mT1 - mT0) * track->GetRate());
      int availSamples = 2147483647 - trackLen;
      int count = availSamples / selectionLen;
      if (maxCount == -1 || count < maxCount)
         maxCount = count;

      track = (WaveTrack *) iter.Next();
   }
   
   if (maxCount <= 1) {
      wxMessageBox(_("Tracks are too long to repeat the selection."),
                   _("Repeat"), wxOK | wxCENTRE, mParent);
      return false;
   }

   RepeatDialog dlog(this, mParent);
   dlog.repeatCount = repeatCount;
   dlog.selectionTimeSecs = mT1 - mT0;
   dlog.maxCount = maxCount;
   dlog.TransferDataToWindow();

   dlog.CentreOnParent();

   dlog.ShowModal();

  if (dlog.GetReturnCode() == wxID_CANCEL)
      return false;

   repeatCount = dlog.repeatCount;
   if (repeatCount > maxCount)
      repeatCount = maxCount;
   if (repeatCount < 1)
      repeatCount = 1;

   return true;
}

bool EffectRepeat::TransferParameters( Shuttle & shuttle )
{  
   shuttle.TransferInt(wxT("Count"),repeatCount,1);
   return true;
}

bool EffectRepeat::Process()
{
   // Set up mOutputTracks. This effect needs Track::All for grouping
   this->CopyInputTracks(Track::All);

   int nTrack = 0;
   bool bGoodResult = true;
   double maxDestLen = 0.0; // used to change selection to generated bit
   
   TrackListIterator iter(mOutputTracks);

   for (Track *t = iter.First(); t && bGoodResult; t = iter.Next()) {
      if (t->GetKind() == Track::Label)
      {
         if (t->GetSelected() || t->IsSynchroSelected())
         {
            LabelTrack* track = (LabelTrack*)t;

            if (!track->Repeat(mT0, mT1, repeatCount))
            {
               bGoodResult = false;
               break;
            }
         }
      }
      else if (t->GetKind() == Track::Wave && t->GetSelected())
      {
         WaveTrack* track = (WaveTrack*)t;

         sampleCount start = track->TimeToLongSamples(mT0);
         sampleCount end = track->TimeToLongSamples(mT1);
         sampleCount len = (sampleCount)(end - start);
         double tLen = track->LongSamplesToTime(len);
         double tc = mT0 + tLen;

         if (len <= 0)
            continue;

         Track *dest;
         track->Copy(mT0, mT1, &dest);
         for(int j=0; j<repeatCount; j++)
         {
            if (!track->Paste(tc, dest) || 
                  TrackProgress(nTrack, j / repeatCount)) // TrackProgress returns true on Cancel.
            {
               bGoodResult = false;
               break;
            }
            tc += tLen;
         }
         if (tc > maxDestLen)
            maxDestLen = tc;
         delete dest;
         nTrack++;
      }
      else if (t->IsSynchroSelected())
      {
         t->SyncAdjust(mT1, mT1 + (mT1 - mT0) * repeatCount);
      }
   }

   if (bGoodResult)
   {
      // Select the new bits + original bit
	   mT1 = maxDestLen;
   }

   this->ReplaceProcessedTracks(bGoodResult); 
   return bGoodResult;
}

//----------------------------------------------------------------------------
// RepeatDialog
//----------------------------------------------------------------------------

const static wxChar *numbers[] =
{
   wxT("0"), wxT("1"), wxT("2"), wxT("3"), wxT("4"),
   wxT("5"), wxT("6"), wxT("7"), wxT("8"), wxT("9")
};

#define ID_REPEAT_TEXT 7000

BEGIN_EVENT_TABLE(RepeatDialog, EffectDialog)
   EVT_TEXT(ID_REPEAT_TEXT, RepeatDialog::OnRepeatTextChange)
   EVT_BUTTON(ID_EFFECT_PREVIEW, RepeatDialog::OnPreview)
END_EVENT_TABLE()

RepeatDialog::RepeatDialog(EffectRepeat *effect,
                           wxWindow * parent)
:  EffectDialog(parent, _("Repeat"), PROCESS_EFFECT),
   mEffect(effect)
{
   Init();
}

void RepeatDialog::PopulateOrExchange(ShuttleGui & S)
{
   wxTextValidator vld(wxFILTER_INCLUDE_CHAR_LIST);
   vld.SetIncludes(wxArrayString(10, numbers));

   S.StartHorizontalLay(wxCENTER, false);
   {
      S.AddTitle(_("by Dominic Mazzoni && Vaughan Johnson"));
   }
   S.EndHorizontalLay();

   S.StartHorizontalLay(wxCENTER, false);
   {
      // Add a little space
   }
   S.EndHorizontalLay();

   S.StartHorizontalLay(wxCENTER, false);
   {
      mRepeatCount = S.Id(ID_REPEAT_TEXT).AddTextBox(_("Number of times to repeat:"),
                                                     wxT(""),
                                                     12);
      mRepeatCount->SetValidator(vld);
   }
   S.EndHorizontalLay();

   S.StartHorizontalLay(wxCENTER, true);
   {
      mTotalTime = S.AddVariableText(_("New selection length: hh:mm:ss"));
   }
   S.EndHorizontalLay();
}

bool RepeatDialog::TransferDataToWindow()
{
   mRepeatCount->ChangeValue(wxString::Format(wxT("%d"), repeatCount));

   DisplayNewTime();

   return true;
}

bool RepeatDialog::TransferDataFromWindow()
{
   long l;
   mRepeatCount->GetValue().ToLong(&l);

   repeatCount = l;
   if (repeatCount < 1)
      repeatCount = 1;
   if (repeatCount > maxCount)
      repeatCount = maxCount;

   return true;
}

void RepeatDialog::DisplayNewTime()
{
   wxString str;

   str = _("New selection length: ");
   TimeTextCtrl tt(this,
                   wxID_ANY,
                   wxT(""),
                   selectionTimeSecs * (repeatCount + 1),
                   mEffect->mProjectRate,
                   wxPoint(10000, 10000),  // create offscreen
                   wxDefaultSize,
                   true);
   tt.SetFormatString(tt.GetBuiltinFormat(_("hh:mm:ss")));
   str += tt.GetTimeString();

   mTotalTime->SetLabel(str);
}

void RepeatDialog::OnRepeatTextChange(wxCommandEvent & event)
{
   TransferDataFromWindow();

   DisplayNewTime();
}

void RepeatDialog::OnPreview(wxCommandEvent &event)
{
   TransferDataFromWindow();

	int oldRepeatCount = mEffect->repeatCount;

   mEffect->repeatCount = repeatCount;

   // LL:  Preview doesn't work...Effect::Preview needs to allow new length
   mEffect->Preview();

   mEffect->repeatCount = oldRepeatCount;
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
// arch-tag: 65c3dad7-00c2-48bf-a253-c973e626b9ac

