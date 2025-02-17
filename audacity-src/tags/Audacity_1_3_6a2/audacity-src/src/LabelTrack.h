/**********************************************************************

  Audacity: A Digital Audio Editor

  LabelTrack.h

  Dominic Mazzoni
  James Crook
  Jun Wan

**********************************************************************/

#ifndef _LABELTRACK_
#define _LABELTRACK_

#include "Track.h"

#include <wx/brush.h>
#include <wx/event.h>
#include <wx/font.h>
#include <wx/pen.h>
#include <wx/dynarray.h>
#include <wx/string.h>
#include <wx/clipbrd.h>


class wxKeyEvent;
class wxMouseEvent;
class wxTextFile;
class wxWindow;
class wxIcon;
class wxBitmap;
class TrackList;

class AudacityProject;
class DirManager;


class LabelStruct 
{
public:
   LabelStruct();
   void DrawLines( wxDC & dc, wxRect & r);
   void DrawGlyphs( wxDC & dc, wxRect & r, int GlyphLeft, int GlyphRight);
   void DrawText( wxDC & dc, wxRect & r);
   void DrawTextBox( wxDC & dc, wxRect & r);
   void DrawHighlight( wxDC & dc, int xPos1, int xPos2, int charHeight);
   void getXPos( wxDC & dc, int * xPos1, int cursorPos);
   
public:
   double t;  /// Time for left hand of label.
   double t1; /// Time for right hand of label.
   wxString title; /// Text of the label.
   int width; /// width of the text in pixels.
   
// Working storage for on-screen layout.
   int x;     /// Pixel position of left hand glyph
   int x1;    /// Pixel position of right hand glyph
   int xText; /// Pixel position of left hand side of text box
   int y;     /// Pixel position of label.

   bool highlighted;              /// if the text is highlighted
   bool changeInitialMouseXPos;   /// flag to change initial mouse X pos 
   bool updated;                  /// flag to tell if the label times were updated
};

WX_DEFINE_ARRAY(LabelStruct *, LabelArray);

const int NUM_GLYPH_CONFIGS = 3;
const int NUM_GLYPH_HIGHLIGHTS = 4;
const int MAX_NUM_ROWS =80;


class LabelTrack:public Track {
   friend class LabelStruct;

 public:
	bool IsGoodLabelCharacter(int keyCode, wxChar charCode);
	bool IsGoodLabelFirstCharacter(int keyCode, wxChar charCode);
   bool IsTextSelected();

	void CreateCustomGlyphs();
   LabelTrack(DirManager * projDirManager);
   LabelTrack(const LabelTrack &orig);

   virtual ~ LabelTrack();
   virtual void SetOffset(double dOffset);

   static void ResetFont();

   void Draw(wxDC & dc, wxRect & r, double h, double pps,
             double sel0, double sel1);

   int getSelectedIndex() const { return mSelIndex; }

   virtual int GetKind() const { return Label; } 

   virtual double GetStartTime();
   virtual double GetEndTime();

   virtual Track *Duplicate() { return new LabelTrack(*this); }

   virtual bool HandleXMLTag(const wxChar *tag, const wxChar **attrs);
   virtual XMLTagHandler *HandleXMLChild(const wxChar *tag);
   virtual void WriteXML(XMLWriter &xmlFile);

#if LEGACY_PROJECT_FILE_SUPPORT
   virtual bool Load(wxTextFile * in, DirManager * dirManager);
   virtual bool Save(wxTextFile * out, bool overwrite);
#endif

   virtual bool Cut  (double t0, double t1, Track ** dest);
   // JKC Do not add the const modifier to Copy(), Clear()
   // or Paste() because then it 
   // is no longer recognised as a virtual function matching the 
   // one in Track.
   virtual bool Copy (double t0, double t1, Track ** dest);// const;
   virtual bool Clear(double t0, double t1);
   virtual bool Paste(double t, Track * src);

   virtual bool Silence(double t0, double t1);
   virtual bool InsertSilence(double t, double len);
   int OverGlyph(int x, int y);
   static wxBitmap & GetGlyph( int i);


   void ResetFlags();
   bool OverTextBox(const LabelStruct *pLabel, int x, int y);
   bool CutSelectedText();
   bool CopySelectedText();
   bool PasteSelectedText(double sel0, double sel1);
   bool IsTextClipSupported();
   
   // methods to set flags
   void SetDragXPos(const int d) { mDragXPos = d; };
   void SetInBox(bool inTextBox) { mInBox = inTextBox; };
   void SetResetCursorPos(bool resetFlag) { mResetCursorPos = resetFlag; };
   void SetWrongDragging(bool rightFlag) { mRightDragging = rightFlag; };
   void SetDrawCursor(bool drawCursorFlag) { mDrawCursor = drawCursorFlag; };
   
   bool HandleMouse(const wxMouseEvent & evt, wxRect & r, double h, double pps,
                           double *newSel0, double *newSel1);

   bool CaptureKey(wxKeyEvent & event);
   bool OnKeyDown(double & sel0, double & sel1, wxKeyEvent & event);
   bool OnChar(double & sel0, double & sel1, wxKeyEvent & event);

   void Import(wxTextFile & f);
   void Export(wxTextFile & f);

   void Unselect();

   bool IsSelected() const;

   int GetNumLabels() const;
   const LabelStruct *GetLabel(int index) const;

   //This returns the index of the label we just added.
   int AddLabel(double t, double t1, const wxString &title = wxT(""));
   //And this tells us the index, if there is a label already there.
   int GetLabelIndex(double t, double t1);

   //This deletes the label at given index.
   void DeleteLabel(int index);

   //get current cursor position
   bool CalcCursorX(wxWindow * parent, int * x);
   int getCurrentCursorPosition() const { return mCurrentCursorPos; };
   
   void ShiftLabelsOnClear(double b, double e, WaveTrack *track);
   void ShiftLabelsOnInsert(double length, double pt, WaveTrack *track);
   WaveTrack* GetStickyTrack() { return mStickyTrack; }
   void SetStickyTrack(WaveTrack *track) { mStickyTrack = track; }

 public:
	 void SortLabels();
   //These two are used by a TrackPanel KLUDGE, which is why they are public.
   bool mbHitCenter;
   //The edge variable tells us what state the icon is in.
   //mOldEdge is useful for telling us when there has been a state change.
   int mOldEdge;               
 private:

   int mSelIndex;              /// Keeps track of the currently selected label
   int mMouseOverLabelLeft;    /// Keeps track of which left label the mouse is currently over.
   int mMouseOverLabelRight;   /// Keeps track of which right label the mouse is currently over.
   int mxMouseDisplacement;    /// Displacement of mouse cursor from the centre being dragged.
   LabelArray mLabels;
   WaveTrack *mStickyTrack;

   static int mIconHeight;
   static int mIconWidth;
   static int mTextHeight;
   static bool mbGlyphsReady;
   static wxBitmap mBoundaryGlyphs[NUM_GLYPH_CONFIGS * NUM_GLYPH_HIGHLIGHTS];

   int xUsed[MAX_NUM_ROWS];

   static int mFontHeight;
   int mXPos1;                         /// left X pos of highlighted area
   int mXPos2;                         /// right X pos of highlighted area 
   int mCurrentCursorPos;              /// current cursor position
   int mInitialCursorPos;              /// initial cursor position
   double mMouseXPos;                  /// mouse X pos
   int mDragXPos;                      /// end X pos of dragging
   bool mInBox;                        /// flag to tell if the mouse is in text box
   bool mResetCursorPos;               /// flag to reset cursor position(used in the dragging the glygh) 
   bool mRightDragging;                /// flag to tell if it's a valid dragging
   bool mDrawCursor;                   /// flag to tell if drawing the cursor or not
   
   // Used only for a LabelTrack on the clipboard
   double mClipLen;

   void ComputeLayout(wxRect & r, double h, double pps);
   void ComputeTextPosition(wxRect & r, int index);
   void SetCurrentCursorPosition(wxDC & dc, int xPos);

   void calculateFontHeight(wxDC & dc);
   void RemoveSelectedText();

   bool mIsAdjustingLabel;

   static wxFont msFont;
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
// arch-tag: fa157f82-b858-406d-9d4b-120d89410f11

