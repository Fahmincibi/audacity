/**********************************************************************

  Audacity: A Digital Audio Editor

  LabelTrack.h

  Dominic Mazzoni

**********************************************************************/

#ifndef _LABELTRACK_
#define _LABELTRACK_

#include <wx/brush.h>
#include <wx/pen.h>
#include <wx/dynarray.h>
#include <wx/string.h>

#include "Track.h"

class wxKeyEvent;
class wxTextFile;
class wxWindow;
class TrackList;

class AudacityProject;
class DirManager;

struct LabelStruct {
   double t;
   double t1;
   wxString title;
   int width;
};

WX_DEFINE_ARRAY(LabelStruct *, LabelArray);

class LabelTrack:public VTrack {
   friend class BouncePane;
   friend bool ExportPCM(AudacityProject *project,
               wxString format, bool stereo, wxString fName,
               bool selectionOnly, double t0, double t1);

 public:
    LabelTrack(DirManager * projDirManager);

    virtual ~ LabelTrack();

   void Draw(wxDC & dc, wxRect & r, double h, double pps,
             double sel0, double sel1);

   virtual int GetKind() {
      return Label;
   } virtual double GetMaxLen();

   bool IsLabelSelected();

   virtual VTrack *Duplicate();

   virtual bool Load(wxTextFile * in, DirManager * dirManager);
   virtual bool Save(wxTextFile * out, bool overwrite);

   virtual void Cut(double t0, double t1, VTrack ** dest);
   virtual void Copy(double t0, double t1, VTrack ** dest);
   virtual void Paste(double t, VTrack * src);
   virtual void Clear(double t0, double t1);

   virtual void Silence(double t0, double t1);
   virtual void InsertSilence(double t, double len);

   void MouseDown(int x, int y, wxRect & r, double h, double pps,
                  double *newSel0, double *newSel1);

   void KeyEvent(double sel0, double sel1, wxKeyEvent & event);

   void AddLabel(double sel0, double sel1);

   void Import(wxTextFile & f);
   void Export(wxTextFile & f);

   void Unselect();

   bool IsSelected();

 private:

   int mSelIndex;

   LabelArray mLabels;

   wxBrush mFlagBrush;
   wxBrush mUnselectedBrush;
   wxBrush mSelectedBrush;

   wxPen mFlagPen;
   wxPen mUnselectedPen;
   wxPen mSelectedPen;

   // Used only for a LabelTrack on the clipboard
   double mClipLen;

};

#endif
