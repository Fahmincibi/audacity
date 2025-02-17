/**********************************************************************

  Audacity: A Digital Audio Editor

  Ruler.h

  Dominic Mazzoni

**********************************************************************/

#ifndef __AUDACITY_RULER__
#define __AUDACITY_RULER__

#include <wx/dc.h>
#include <wx/event.h>
#include <wx/font.h>
#include <wx/panel.h>
#include <wx/window.h>
#include "../Envelope.h"

struct ViewInfo;
class AudacityProject;

class AUDACITY_DLL_API Ruler {
 public:

   enum RulerFormat {
      IntFormat,
      RealFormat,
      TimeFormat,
      LinearDBFormat,
   };

   //
   // Constructor / Destructor
   //

   Ruler();
   ~Ruler();

   //
   // Required Ruler Parameters
   //

   void SetBounds(int left, int top, int right, int bottom);

   // wxHORIZONTAL || wxVERTICAL
   void SetOrientation(int orient);

   // min is the value at (x, y)
   // max is the value at (x+width, y+height)
   // (at the center of the pixel, in both cases)
   void SetRange(double min, double max);

   //
   // Optional Ruler Parameters
   //

   // IntFormat, RealFormat, or TimeFormat
   void SetFormat(RulerFormat format);

   // Specify the name of the units (like "dB") if you
   // want numbers like "1.6" formatted as "1.6 dB".
   void SetUnits(wxString units);

   // Logarithmic
   void SetLog(bool log);

   // Minimum number of pixels between labels
   void SetSpacing(int spacing);

   // If this is true, the edges of the ruler will always
   // receive a label.  If not, the nearest round number is
   // labeled (which may or may not be the edge).
   void SetLabelEdges(bool labelEdges);

   // Makes a vertical ruler hug the left side (instead of right)
   // and a horizontal ruler hug the top (instead of bottom)
   void SetFlip(bool flip);

   // Good defaults are provided, but you can override here
   void SetFonts(const wxFont &minorFont, const wxFont &majorFont);

   // The ruler will not draw text within this (pixel) range.
   // Use this if you have another graphic object obscuring part
   // of the ruler's area.  The values start and end are interpreted
   // relative to the Ruler's local coordinates.
   void OfflimitsPixels(int start, int end);

   //
   // Calculates and returns the maximum size required by the ruler
   //
   void GetMaxSize(wxCoord *width, wxCoord *height);

   //
   // Drawing
   //

   // Note that it will not erase for you...
   void Draw(wxDC& dc);
   void Draw(wxDC& dc, Envelope *speedEnv, long minSpeed, long maxSpeed);

   // So we can have white ticks on black...
   void SetTickColour( const wxColour & colour)
   { mTickColour = colour; mPen.SetColour( colour );}

 private:
   void Invalidate();
   void Update();
   void Update(Envelope *speedEnv, long minSpeed, long maxSpeed);
   void FindTickSizes();
   void FindLinearTickSizes(double UPP);
   wxString LabelString(double d, bool major);
   void Tick(int pos, double d, bool major);

public:
   bool mbTicksOnly;
   bool mbTicksAtExtremes;

private:
   wxColour mTickColour;
   wxPen mPen;
 
   int          mMaxWidth, mMaxHeight;
   int          mLeft, mTop, mRight, mBottom, mLead;
   int          mLength;
   wxDC        *mDC;

   wxFont      *mMinorFont, *mMajorFont;
   bool         mUserFonts;

   double       mMin, mMax;

   double       mMajor;
   double       mMinor;

   int          mDigits;

   int         *mUserBits;
   int         *mBits;
   int          mUserBitLen;

   bool         mValid;

   struct Label {
      int pos;
      int lx, ly;
      wxString text;
   };
   
   int          mNumMajor;
   Label       *mMajorLabels;
   int          mNumMinor;
   Label       *mMinorLabels;   

   int          mOrientation;
   int          mSpacing;
   bool         mHasSetSpacing;
   bool         mLabelEdges;
   RulerFormat  mFormat;
   bool         mLog;
   bool         mFlip;
   wxString     mUnits;
};

class AUDACITY_DLL_API RulerPanel : public wxPanel {
   DECLARE_DYNAMIC_CLASS(RulerPanel)

 public:
   RulerPanel(wxWindow* parent, wxWindowID id,
              const wxPoint& pos = wxDefaultPosition,
              const wxSize& size = wxDefaultSize);

   ~RulerPanel();

   void OnErase(wxEraseEvent &evt);
   void OnPaint(wxPaintEvent &evt);
   void OnSize(wxSizeEvent &evt);

   // We don't need or want to accept focus.
   bool AcceptsFocus() const { return false; }

 public:

   Ruler  ruler;

private:
    DECLARE_EVENT_TABLE()
};

// This is an Audacity Specific ruler panel which additionally
// has border, selection markers, play marker.
// Once TrackPanel uses wxSizers, we will derive it from some 
// wxWindow and the GetSize and SetSize functions
// will then be wxWindows functions instead.
class AUDACITY_DLL_API AdornedRulerPanel : public wxPanel
{
public:
   AdornedRulerPanel(wxWindow* parent,
                     wxWindowID id,
                     const wxPoint& pos = wxDefaultPosition,
                     const wxSize& size = wxDefaultSize,
                     ViewInfo *viewinfo = NULL);

   ~AdornedRulerPanel();

public:
   static int GetRulerHeight() { return 28; }
   void SetLeftOffset(int offset){ mLeftOffset = offset; }

   void DrawCursor(double pos);
   void DrawIndicator(double pos, bool rec);
   void DrawSelection();
   void ClearIndicator();

   void SetPlayRegion(double playRegionStart, double playRegionEnd);
   void ClearPlayRegion();
   void GetPlayRegion(double* playRegionStart, double* playRegionEnd);
   
   void SetProject(AudacityProject* project) {mProject = project;};

private:
   void OnErase(wxEraseEvent &evt);
   void OnPaint(wxPaintEvent &evt);
   void OnSize(wxSizeEvent &evt);
   void OnMouseEvents(wxMouseEvent &evt);

   void DoDraw(wxDC *dc);

   void DoDrawBorder(wxDC * dc);
   void DoDrawMarks(wxDC * dc, bool /*text */ );
   void DoDrawCursor(wxDC * dc);
   void DoDrawSelection(wxDC * dc);
   void DoDrawIndicator(wxDC * dc);
   void DoDrawPlayRegion(wxDC *dc);

   double Pos2Time(int p);
   int Time2Pos(double t);

   bool IsWithinMarker(int mousePosX, double markerTime);

   Ruler  ruler;
   ViewInfo *mViewInfo;
   AudacityProject *mProject;

   wxBitmap *mBuffer;

   wxRect mOuter;
   wxRect mInner;

   int mLeftOffset;  // Number of pixels before we hit the 'zero position'.

   double mCurPos;

   int mIndType;     // -1 = No indicator, 0 = Play, 1 = Record
   double mIndPos;

   double mPlayRegionStart;
   double mPlayRegionEnd;
   
   enum MouseEventState {
      mesNone,
      mesDraggingPlayRegionStart,
      mesDraggingPlayRegionEnd,
      mesSelectingPlayRegionClick,
      mesSelectingPlayRegionRange
   };
   
   MouseEventState mMouseEventState;
   int mButtonDownMousePos;

   DECLARE_EVENT_TABLE()
};

#endif //define __AUDACITY_RULER__

// Indentation settings for Vim and Emacs and unique identifier for Arch, a
// version control system. Please do not modify past this point.
//
// Local Variables:
// c-basic-offset: 3
// indent-tabs-mode: nil
// End:
//
// vim: et sts=3 sw=3
// arch-tag: 420f6c32-664f-4614-b922-307d9027432c

