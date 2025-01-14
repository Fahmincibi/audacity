/**********************************************************************

  Audacity: A Digital Audio Editor

  EffectEqualization.h

  Mitch Golden
  Vaughan Johnson (Preview)

***********************************************************************/

#ifndef __AUDACITY_EFFECT_EQUALIZATION__
#define __AUDACITY_EFFECT_EQUALIZATION__
#define NUMBER_OF_BANDS 31
#define NUM_PTS 180

#include <wx/button.h>
#include <wx/panel.h>
#include <wx/dialog.h>
#include <wx/dynarray.h>
#include <wx/intl.h>
#include <wx/stattext.h>
#include <wx/slider.h>
#include <wx/sizer.h>

#if wxUSE_ACCESSIBILITY
#if defined(__WXMSW__)
#include <oleacc.h>
#endif
#include <wx/access.h>
#endif

// Declare window functions

class wxString;
class wxBoxSizer;
class wxChoice;
class wxRadioButton;

#include "Effect.h"
#include "../xml/XMLTagHandler.h"
#include "../widgets/Ruler.h"

class Envelope;
class WaveTrack;
class EqualizationDialog;

//
// One point in a curve
//
class EQPoint
{
public:
   EQPoint( const double f, const double d ) { Freq = f; dB = d; }
   double Freq;
   double dB;
};
WX_DECLARE_OBJARRAY( EQPoint, EQPointArray);

//
// One curve in a list
//
// LLL:  This "really" isn't needed as the EQPointArray could be
//       attached as wxClientData to the wxChoice entries.  I
//       didn't realize this until after the fact and am too
//       lazy to change it.  (But, hollar if you want me to.)
//
class EQCurve
{
public:
   EQCurve( const wxString & name ) { Name = name; }
   EQCurve( const wxChar * name ) { Name = name; }
   wxString Name;
   EQPointArray points;
};
WX_DECLARE_OBJARRAY( EQCurve, EQCurveArray );

class EffectEqualization: public Effect {

public:

   EffectEqualization();
   virtual ~EffectEqualization();

   virtual wxString GetEffectName() {
      return wxString(_("Equalization..."));
   }

   virtual wxString GetEffectAction() {
      return wxString(_("Performing Equalization"));
   }

   virtual bool PromptUser();
   virtual bool TransferParameters( Shuttle & shuttle );

   virtual bool Process();

   // Number of samples in an FFT window
   enum {windowSize=16384};   //MJS - work out the optimum for this at run time?  Have a dialog box for it?

   // Low frequency of the FFT.  20Hz is the
   // low range of human hearing
   enum {loFreqI=20};

private:
   bool ProcessOne(int count, WaveTrack * t,
                   sampleCount start, sampleCount len);

   void Filter(sampleCount len,
               float *buffer);

   float *mFilterFuncR;
   float *mFilterFuncI;
   int mM;
   float mdBMax;
   float mdBMin;

public:
   enum curveType {
     amradio, acoustic,
     nab, lp, aes, deccaffrrmicro, deccaffrr78, riaa,
     col78, deccaffrrlp, emi78, rcavictor1938, rcavictor1947,
     nCurveTypes
   };

   enum {nCurvePoints=28};
   static const float curvex[];
   static const float curvey[][nCurvePoints];
   static const wxChar * curveNames[];

friend class EqualizationDialog;
friend class EqualizationPanel;
};


class EqualizationPanel: public wxPanel
{
public:
   EqualizationPanel( double loFreq, double hiFreq,
               Envelope *env,
               EqualizationDialog *parent,
               float *filterFuncR, float *filterFuncI, long windowSize,
               wxWindowID id,
               const wxPoint& pos = wxDefaultPosition,
               const wxSize& size = wxDefaultSize);
   ~EqualizationPanel();

   void OnMouseEvent(wxMouseEvent & event);
   void OnPaint(wxPaintEvent & event);
   void OnSize (wxSizeEvent & event);

   // We don't need or want to accept focus.
   bool AcceptsFocus() const { return false; }

   void Recalc();

   int M;
   float dBMax;
   float dBMin;
   bool RecalcRequired;

private:

   wxBitmap *mBitmap;
   wxRect mEnvRect;
   EqualizationDialog *mParent;
   int mWidth;
   int mHeight;
   long mWindowSize;
   float *mFilterFuncR;
   float *mFilterFuncI;
   float *mOutr;
   float *mOuti;

   double mLoFreq;
   double mHiFreq;

   Envelope *mEnvelope;

   DECLARE_EVENT_TABLE()
};


// WDR: class declarations

//----------------------------------------------------------------------------
// EqualizationDialog
//----------------------------------------------------------------------------

class EqualizationDialog: public wxDialog, public XMLTagHandler
{
public:
   // constructors and destructors
   EqualizationDialog(EffectEqualization * effect,
               double loFreq, double hiFreq,
               float *filterFuncR, float *filterFuncI, long windowSize,
               wxWindow *parent, wxWindowID id,
               const wxString &title,
               const wxPoint& pos = wxDefaultPosition,
               const wxSize& size = wxDefaultSize,
               long style = wxDEFAULT_DIALOG_STYLE );

   // WDR: method declarations for EqualizationDialog
   virtual bool Validate();
   virtual bool TransferDataToWindow();
   virtual bool TransferDataFromWindow();
   virtual bool CalcFilter();

   void EnvelopeUpdated();
   static const double thirdOct[];
   wxRadioButton *mFaderOrDraw[2];
   wxChoice *mInterpChoice;
   int M;
   float dBMin;
   float dBMax;
   double whens[NUM_PTS];
   double whensFreq[NUM_PTS];
   double whenSliders[NUMBER_OF_BANDS+1];
   int bandsInUse;

private:
   void MakeEqualizationDialog();
   void CreateChoice();
   void LoadDefaultCurves();
   void LoadCurves();
   void SaveCurves();
   void Select(int sel);
   void setCurve(Envelope *env, int currentCurve);
   void setCurve(Envelope *env);
   void GraphicEQ(Envelope *env);
   void spline(double x[], double y[], int n, double y2[]);
   double splint(double x[], double y[], int n, double y2[], double xr);
   void LayoutEQSliders();

   // XMLTagHandler callback methods for loading and saving
   bool HandleXMLTag(const wxChar *tag, const wxChar **attrs);
   XMLTagHandler *HandleXMLChild(const wxChar *tag);
   void WriteXML(XMLWriter &xmlFile);

private:
   // WDR: member variable declarations for EqualizationDialog

   enum
   {
      ID_FILTERPANEL = 10000,
      ID_LENGTH,
      ID_DBMAX,
      ID_DBMIN,
      ID_CURVE,
      ID_SAVEAS,
      ID_DELETE,
      ID_CLEAR,
      ID_PREVIEW,
      drawRadioID,
      sliderRadioID,
      ID_INTERP,
      ID_SLIDER   // needs to come last
   };

private:
   // WDR: handler declarations for EqualizationDialog
   void OnPaint( wxPaintEvent &event );
   void OnSize( wxSizeEvent &event );
   void OnErase( wxEraseEvent &event );
   void OnSlider( wxCommandEvent &event );
   void OnInterp( wxCommandEvent &event );
   void OnSliderM( wxCommandEvent &event );
   void OnSliderDBMAX( wxCommandEvent &event );
   void OnSliderDBMIN( wxCommandEvent &event );
   void OnDrawRadio(wxCommandEvent &event );
   void OnSliderRadio(wxCommandEvent &event );
   void ErrMin(void);
   void OnCurve( wxCommandEvent &event );
   void OnSaveAs( wxCommandEvent &event );
   void OnDelete( wxCommandEvent &event );
   void OnClear( wxCommandEvent &event );
   void OnPreview(wxCommandEvent &event);
   void OnOk( wxCommandEvent &event );
   void OnCancel( wxCommandEvent &event );

private:
   EffectEqualization * m_pEffect;

   double mLoFreq;
   double mHiFreq;
   float *mFilterFuncR;
   float *mFilterFuncI;
   long mWindowSize;
   bool mDirty;
   wxSlider * m_sliders[NUMBER_OF_BANDS];
   int m_sliders_old[NUMBER_OF_BANDS];
   double m_EQVals[NUMBER_OF_BANDS+1];

   EqualizationPanel *mPanel;
   Envelope *mEnvelope;
   wxBoxSizer *mCurveSizer;
   wxChoice *mCurve;
   wxButton *mDelete;
   wxButton *mSaveAs;
   wxStaticText *mMText;
   wxStaticText *octText;
   wxSlider *MSlider;
   wxSlider *dBMinSlider;
   wxSlider *dBMaxSlider;
   RulerPanel *dBRuler;
   RulerPanel *freqRuler;
   wxBoxSizer *szrC;
   wxBoxSizer *szrG;
   wxBoxSizer *szrV;
   wxBoxSizer *szr3;
   wxBoxSizer *szr4;
   wxBoxSizer *szr2;
   wxFlexGridSizer *szr1;
   wxSize size;

   EQCurveArray mCurves;

private:
   DECLARE_EVENT_TABLE()

};

#if wxUSE_ACCESSIBILITY

class SliderAx: public wxWindowAccessible
{
public:
   SliderAx(wxWindow * window, wxString fmt);

   virtual ~ SliderAx();

   // Retrieves the address of an IDispatch interface for the specified child.
   // All objects must support this property.
   virtual wxAccStatus GetChild( int childId, wxAccessible** child );

   // Gets the number of children.
   virtual wxAccStatus GetChildCount(int* childCount);

   // Gets the default action for this object (0) or > 0 (the action for a child).
   // Return wxACC_OK even if there is no action. actionName is the action, or the empty
   // string if there is no action.
   // The retrieved string describes the action that is performed on an object,
   // not what the object does as a result. For example, a toolbar button that prints
   // a document has a default action of "Press" rather than "Prints the current document."
   virtual wxAccStatus GetDefaultAction( int childId, wxString *actionName );

   // Returns the description for this object or a child.
   virtual wxAccStatus GetDescription( int childId, wxString *description );

   // Gets the window with the keyboard focus.
   // If childId is 0 and child is NULL, no object in
   // this subhierarchy has the focus.
   // If this object has the focus, child should be 'this'.
   virtual wxAccStatus GetFocus( int *childId, wxAccessible **child );

   // Returns help text for this object or a child, similar to tooltip text.
   virtual wxAccStatus GetHelpText( int childId, wxString *helpText );

   // Returns the keyboard shortcut for this object or child.
   // Return e.g. ALT+K
   virtual wxAccStatus GetKeyboardShortcut( int childId, wxString *shortcut );

   // Returns the rectangle for this object (id = 0) or a child element (id > 0).
   // rect is in screen coordinates.
   virtual wxAccStatus GetLocation( wxRect& rect, int elementId );

   // Gets the name of the specified object.
   virtual wxAccStatus GetName( int childId, wxString *name );

   // Returns a role constant.
   virtual wxAccStatus GetRole( int childId, wxAccRole *role );

   // Gets a variant representing the selected children
   // of this object.
   // Acceptable values:
   // - a null variant (IsNull() returns TRUE)
   // - a list variant (GetType() == wxT("list"))
   // - an integer representing the selected child element,
   //   or 0 if this object is selected (GetType() == wxT("long"))
   // - a "void*" pointer to a wxAccessible child object
   virtual wxAccStatus GetSelections( wxVariant *selections );

   // Returns a state constant.
   virtual wxAccStatus GetState(int childId, long* state);

   // Returns a localized string representing the value for the object
   // or child.
   virtual wxAccStatus GetValue(int childId, wxString* strValue);

private:
   wxWindow *mParent;
   wxString mFmt;
};

#endif // wxUSE_ACCESSIBILITY

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
// arch-tag: 309f263d-748c-4dc0-9e68-9e86732890bb

