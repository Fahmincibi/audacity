/**********************************************************************

  Audacity: A Digital Audio Editor

  TrackArtist.cpp

  Dominic Mazzoni


*******************************************************************//*!

\class TrackArtist
\brief   This class handles the actual rendering of WaveTracks (both
  waveforms and spectra), NoteTracks, LabelTracks and TimeTracks.

  It's actually a little harder than it looks, because for
  waveforms at least it needs to cache the samples that are
  currently on-screen.


*//*******************************************************************/

#include "Audacity.h"
#include "AudacityApp.h"
#include "TrackArtist.h"
#include "float_cast.h"

#include <math.h>
#include <float.h>

#include <wx/brush.h>
#include <wx/colour.h>
#include <wx/dc.h>
#include <wx/dcmemory.h>
#include <wx/gdicmn.h>
#include <wx/image.h>
#include <wx/pen.h>
#include <wx/log.h>
#include <wx/datetime.h>

#ifdef USE_MIDI
#include "allegro.h"
#include "NoteTrack.h"
#endif // USE_MIDI

#include "AColor.h"
#include "BlockFile.h"
#include "Envelope.h"
#include "Track.h"
#include "WaveTrack.h"
#include "LabelTrack.h"
#include "TimeTrack.h"
#include "Prefs.h"
#include "Sequence.h"
#include "Spectrum.h"
#include "ViewInfo.h"
#include "widgets/Ruler.h"
#include "Theme.h"
#include "AllThemeResources.h"

#undef PROFILE_WAVEFORM
#ifdef PROFILE_WAVEFORM
   #ifdef __WXMSW__
      #include <time.h>
   #else
      #include <sys/time.h>
   #endif
double gWaveformTimeTotal = 0;
int gWaveformTimeCount = 0;
#endif

#ifdef USE_MIDI
const int octaveHeight = 62;
const int blackPos[5] = { 6, 16, 32, 42, 52 };
const int whitePos[7] = { 0, 9, 17, 26, 35, 44, 53 };
const int notePos[12] = { 1, 6, 11, 16, 21, 27,
                        32, 37, 42, 47, 52, 57 };
#endif // USE_MIDI

TrackArtist::TrackArtist()
{
   mInsetLeft   = 0;
   mInsetTop    = 0;
   mInsetRight  = 0;
   mInsetBottom = 0;

   mdBrange = ENV_DB_RANGE;
   mShowClipping = false;
   UpdatePrefs();

   SetColours();
   vruler = new Ruler();

#ifdef EXPERIMENTAL_FFT_Y_GRID
   fftYGridOld=true;
#endif //EXPERIMENTAL_FFT_Y_GRID

#ifdef EXPERIMENTAL_FIND_NOTES
   fftFindNotesOld=false;
#endif
}

TrackArtist::~TrackArtist()
{
   delete vruler;
}

void TrackArtist::SetColours()
{
   theTheme.SetBrushColour( blankBrush,      clrBlank );
   theTheme.SetBrushColour( unselectedBrush, clrUnselected);
   theTheme.SetBrushColour( selectedBrush,   clrSelected);
   theTheme.SetBrushColour( sampleBrush,     clrSample);
   theTheme.SetBrushColour( selsampleBrush,  clrSelSample);
   theTheme.SetBrushColour( dragsampleBrush, clrDragSample);

   theTheme.SetPenColour(   blankPen,        clrBlank);
   theTheme.SetPenColour(   unselectedPen,   clrUnselected);
   theTheme.SetPenColour(   selectedPen,     clrSelected);
   theTheme.SetPenColour(   samplePen,       clrSample);
   theTheme.SetPenColour(   selsamplePen,    clrSelSample);
   theTheme.SetPenColour(   muteSamplePen,   clrMuteSample);
   theTheme.SetPenColour(   odProgressDonePen,   clrProgressDone);
   theTheme.SetPenColour(   odProgressNotYetPen,   clrProgressNotYet);
   theTheme.SetPenColour(   rmsPen,          clrRms);
   theTheme.SetPenColour(   muteRmsPen,      clrMuteRms);
   theTheme.SetPenColour(   shadowPen,       clrShadow);
   theTheme.SetPenColour(   clippedPen,      clrClipped);
   theTheme.SetPenColour(   muteClippedPen,  clrMuteClipped);
}

void TrackArtist::SetInset(int left, int top, int right, int bottom)
{
   mInsetLeft   = left;
   mInsetTop    = top;
   mInsetRight  = right;
   mInsetBottom = bottom;
}

void TrackArtist::DrawTracks(TrackList * tracks,
                             Track * start,
                             wxDC & dc,
                             wxRegion & reg,
                             wxRect & r,
                             wxRect & clip,
                             ViewInfo * viewInfo,
                             bool drawEnvelope,
                             bool drawSamples,
                             bool drawSliders)
{
   wxRect trackRect = r;
   wxRect stereoTrackRect;

   TrackListIterator iter(tracks);
   Track *t;

   bool hasSolo = false;
   for (t = iter.First(); t; t = iter.Next()) {
      if (t->GetSolo()) {
         hasSolo = true;
         break;
      }
   }

#if defined(DEBUG_CLIENT_AREA)
   // Change the +0 to +1 or +2 to see the bounding box
   mInsetLeft = 1+0; mInsetTop = 5+0; mInsetRight = 6+0; mInsetBottom = 2+0;

   // This just show what the passed in rectanges enclose
   dc.SetPen(wxColour(*wxGREEN));
   dc.SetBrush(*wxTRANSPARENT_BRUSH);
   dc.DrawRectangle(r);
   dc.SetPen(wxColour(*wxBLUE));
   dc.SetBrush(*wxTRANSPARENT_BRUSH);
   dc.DrawRectangle(clip);
#endif

   t = iter.StartWith(start);
   while (t) {
      trackRect.y = t->GetY() - viewInfo->vpos;
      trackRect.height = t->GetHeight();

      if (trackRect.y > clip.GetBottom() && !t->GetLinked()) {
         break;
      }

#if defined(DEBUG_CLIENT_AREA)
      // Filled rectangle to show the interior of the client area
      wxRect zr = trackRect;
      zr.x+=1; zr.y+=5; zr.width-=7; zr.height-=7;
      dc.SetPen(*wxCYAN_PEN);
      dc.SetBrush(*wxRED_BRUSH);
      dc.DrawRectangle(zr);
#endif

      stereoTrackRect = trackRect;

      // For various reasons, the code will break if we display one
      // of a stereo pair of tracks but not the other - for example,
      // if you try to edit the envelope of one track when its linked
      // pair is off the screen, then it won't be able to edit the
      // offscreen envelope.  So we compute the rect of the track and
      // its linked partner, and see if any part of that rect is on-screen.
      // If so, we draw both.  Otherwise, we can safely draw neither.

      Track *link = t->GetLink();
      if (link) {
         if (t->GetLinked()) {
            // If we're the first track
            stereoTrackRect.height += link->GetHeight();
         }
         else {
            // We're the second of two
            stereoTrackRect.y -= link->GetHeight();
            stereoTrackRect.height += link->GetHeight();
         }
      }

      if (stereoTrackRect.Intersects(clip) && reg.Contains(stereoTrackRect)) {
         wxRect rr = trackRect;
         rr.x += mInsetLeft;
         rr.y += mInsetTop;
         rr.width -= (mInsetLeft + mInsetRight);
         rr.height -= (mInsetTop + mInsetBottom);
         DrawTrack(t, dc, rr, viewInfo,
                   drawEnvelope, drawSamples, drawSliders, hasSolo);
      }

      t = iter.Next();
   }
}

void TrackArtist::DrawTrack(const Track * t,
                            wxDC & dc,
                            const wxRect & r,
                            const ViewInfo * viewInfo,
                            bool drawEnvelope,
                            bool drawSamples,
                            bool drawSliders,
                            bool hasSolo)
{
   switch (t->GetKind()) {
   case Track::Wave:
   {
      WaveTrack* wt = (WaveTrack*)t;
      for (WaveClipList::compatibility_iterator it=wt->GetClipIterator(); it; it=it->GetNext()) {
         it->GetData()->ClearDisplayRect();
      }

      bool muted = (hasSolo || t->GetMute()) && !t->GetSolo();

      switch (wt->GetDisplay()) {
      case WaveTrack::WaveformDisplay:
         DrawWaveform(wt, dc, r, viewInfo,
                      drawEnvelope, drawSamples, drawSliders, false, muted);
         break;
      case WaveTrack::WaveformDBDisplay:
         DrawWaveform(wt, dc, r, viewInfo,
                      drawEnvelope,  drawSamples, drawSliders, true, muted);
         break;
      case WaveTrack::SpectrumDisplay:
         DrawSpectrum(wt, dc, r, viewInfo, false, false);
         break;
      case WaveTrack::SpectrumLogDisplay:
         DrawSpectrum(wt, dc, r, viewInfo, false, true);
         break;
      case WaveTrack::PitchDisplay:
         DrawSpectrum(wt, dc, r, viewInfo, true, false);
         break;
      }
      break;              // case Wave
   }
   #ifdef USE_MIDI
   case Track::Note:
      DrawNoteTrack((NoteTrack *)t, dc, r, viewInfo);
      break;
   #endif // USE_MIDI
   case Track::Label:
      DrawLabelTrack((LabelTrack *)t, dc, r, viewInfo);
      break;
   case Track::Time:
      DrawTimeTrack((TimeTrack *)t, dc, r, viewInfo);
      break;
   }
}

void TrackArtist::DrawVRuler(Track *t, wxDC * dc, wxRect & r)
{
   int kind = t->GetKind();

   // Label and Time tracks do not have a vruler
   // But give it a beveled area
   if (kind == Track::Label || kind == Track::Time) {
      wxRect bev = r;
      bev.Inflate(-1, -1);
      bev.width += 1;
      AColor::BevelTrackInfo(*dc, true, bev);

      return;
   }

   // All waves have a ruler in the info panel
   // The ruler needs a bevelled surround.
   if (kind == Track::Wave) {
      wxRect bev = r;
      bev.Inflate(-1, -1);
      bev.width += 1;
      AColor::BevelTrackInfo(*dc, true, bev);

      // Pitch doesn't have a ruler
      if (((WaveTrack *)t)->GetDisplay() == WaveTrack::PitchDisplay) {
         return;
      }

      // Right align the ruler
      wxRect rr = r;
      rr.width--;
      if (t->vrulerSize.GetWidth() < r.GetWidth()) {
         int adj = rr.GetWidth() - t->vrulerSize.GetWidth();
         rr.x += adj;
         rr.width -= adj;
      }

      UpdateVRuler(t, rr);

      vruler->Draw(*dc);

      return;
   }

#ifdef USE_MIDI
   // The note track isn't drawing a ruler at all!
   // But it needs to!
   if (kind == Track::Note) {
      UpdateVRuler(t, r);

      dc->SetPen(*wxTRANSPARENT_PEN);
      dc->SetBrush(*wxWHITE_BRUSH);
      wxRect bev = r;
      bev.x++;
      bev.y++;
      bev.width--;
      bev.height--;
      dc->DrawRectangle(bev);

      r.y += 2;
      r.height -= 2;

      int bottomNote = ((NoteTrack *) t)->GetBottomNote();
      int bottom = r.height +
          ((bottomNote / 12) * octaveHeight + notePos[bottomNote % 12]);

      wxPen hilitePen;
      hilitePen.SetColour(120, 120, 120);
      wxBrush blackKeyBrush;
      blackKeyBrush.SetColour(70, 70, 70);

      dc->SetBrush(blackKeyBrush);

      int fontSize = 10;
#ifdef __WXMSW__
      fontSize = 8;
#endif

      wxFont labelFont(fontSize, wxSWISS, wxNORMAL, wxNORMAL);
      dc->SetFont(labelFont);

      for (int octave = 0; octave < 50; octave++) {
         int obottom = bottom - octave * octaveHeight;
         if (obottom < 0)
            break;

         dc->SetPen(*wxBLACK_PEN);
         for (int white = 0; white < 7; white++)
            if (r.y + obottom - whitePos[white] > r.y &&
                r.y + obottom - whitePos[white] < r.y + r.height)
               AColor::Line(*dc, r.x, r.y + obottom - whitePos[white],
                            r.x + r.width,
                            r.y + obottom - whitePos[white]);

         wxRect br = r;
         br.height = 5;
         br.x++;
         br.width = 17;
         for (int black = 0; black < 5; black++) {
            br.y = r.y + obottom - blackPos[black] - 4;
            if (br.y > r.y && br.y + br.height < r.y + r.height) {
               dc->SetPen(hilitePen);
               dc->DrawRectangle(br);
               dc->SetPen(*wxBLACK_PEN);
               AColor::Line(*dc,
                            br.x + 1, br.y + br.height - 1,
                            br.x + br.width - 1, br.y + br.height - 1);
               AColor::Line(*dc,
                            br.x + br.width - 1, br.y + 1,
                            br.x + br.width - 1, br.y + br.height - 1);
            }
         }

         if (octave >= 2 && octave <= 9) {
            wxString s;
            s.Printf(wxT("C%d"), octave - 2);
            long width, height;
            dc->GetTextExtent(s, &width, &height);
            if (r.y + obottom - height + 4 > r.y &&
                r.y + obottom + 4 < r.y + r.height) {
               dc->SetTextForeground(wxColour(60, 60, 255));
               dc->DrawText(s, r.x + r.width - width,
                            r.y + obottom - height + 2);
            }
         }
      }
   }
#endif // USE_MIDI

}

void TrackArtist::UpdateVRuler(Track *t, wxRect & r)
{
   // Label tracks do not have a vruler
   if (t->GetKind() == Track::Label) {
      return;
   }

   // All waves have a ruler in the info panel
   // The ruler needs a bevelled surround.
   if (t->GetKind() == Track::Wave) {
      WaveTrack *wt = (WaveTrack *)t;
      int display = wt->GetDisplay();

      if (display == WaveTrack::WaveformDisplay) {
         // Waveform

         float min, max;
         wt->GetDisplayBounds(&min, &max);

         vruler->SetBounds(r.x, r.y+1, r.x + r.width, r.y + r.height-1);
         vruler->SetOrientation(wxVERTICAL);
         vruler->SetRange(max, min);
         vruler->SetFormat(Ruler::RealFormat);
         vruler->SetUnits(wxT(""));
         vruler->SetLabelEdges(false);
         vruler->SetLog(false);
      }
      else if (display == WaveTrack::WaveformDBDisplay) {
         // Waveform (db)

         vruler->SetUnits(wxT(""));

         float min, max;
         wt->GetDisplayBounds(&min, &max);

         if (max > 0) {
            int top = 0;
            float topval = 0;
            int bot = r.height;
            float botval = -mdBrange;

            if (min < 0) {
               bot = top + (int)((max / (max-min))*(bot-top));
               min = 0;
            }

            if (max > 1) {
               top += (int)((max-1)/(max-min) * (bot-top));
               max = 1;
            }

            if (max < 1)
               topval = -((1-max)*mdBrange);

            if (min > 0) {
               botval = -((1-min)*mdBrange);
            }

            if (topval > botval && bot > top+10) {
               vruler->SetBounds(r.x, r.y+top+1, r.x + r.width, r.y + bot-1);
               vruler->SetOrientation(wxVERTICAL);
               vruler->SetRange(topval, botval);
               vruler->SetFormat(Ruler::LinearDBFormat);
               vruler->SetLabelEdges(true);
               vruler->SetLog(false);
            }
         }
      }
      else if (display == WaveTrack::SpectrumDisplay) {
         // Spectrum

         if (r.height < 60)
            return;

         double rate = wt->GetRate();
         int freq = lrint(rate/2.);

         int maxFreq = GetSpectrumMaxFreq(freq);
#ifdef EXPERIMENTAL_FFT_SKIP_POINTS
         maxFreq/=(mFftSkipPoints+1);
#endif //EXPERIMENTAL_FFT_SKIP_POINTS
         if(maxFreq > freq)
            maxFreq = freq;

         int minFreq = GetSpectrumMinFreq(0);
#ifdef EXPERIMENTAL_FFT_SKIP_POINTS
         minFreq/=(mFftSkipPoints+1);
#endif //EXPERIMENTAL_FFT_SKIP_POINTS
         if(minFreq < 0)
            minFreq = 0;

         /*
            draw the ruler
            we will use Hz if maxFreq is < 2000, otherwise we represent kHz,
            and append to the numbers a "k"
         */
         vruler->SetBounds(r.x, r.y+1, r.x + r.width, r.y + r.height-1);
         vruler->SetOrientation(wxVERTICAL);
         vruler->SetFormat(Ruler::RealFormat);
         vruler->SetLabelEdges(true);
         // use kHz in scale, if appropriate
         if (maxFreq>=2000) {
            vruler->SetRange((maxFreq/1000.), (minFreq/1000.));
            vruler->SetUnits(wxT("k"));
         } else {
            // use Hz
            vruler->SetRange(int(maxFreq), int(minFreq));
            vruler->SetUnits(wxT(""));
         }
         vruler->SetLog(false);
      }
      else if (display == WaveTrack::SpectrumLogDisplay) {
         // SpectrumLog

         if (r.height < 10)
            return;

         double rate = wt->GetRate();
         int freq = lrint(rate/2.);

         int maxFreq = GetSpectrumLogMaxFreq(freq);
#ifdef EXPERIMENTAL_FFT_SKIP_POINTS
         maxFreq/=(mFftSkipPoints+1);
#endif //EXPERIMENTAL_FFT_SKIP_POINTS
         if(maxFreq > freq)
            maxFreq = freq;

         int minFreq = GetSpectrumLogMinFreq(freq/1000.0);
#ifdef EXPERIMENTAL_FFT_SKIP_POINTS
         minFreq/=(mFftSkipPoints+1);
#endif //EXPERIMENTAL_FFT_SKIP_POINTS
         if(minFreq < 1)
            minFreq = 1;

         /*
            draw the ruler
            we will use Hz if maxFreq is < 2000, otherwise we represent kHz,
            and append to the numbers a "k"
         */
         vruler->SetBounds(r.x, r.y+1, r.x + r.width, r.y + r.height-1);
         vruler->SetOrientation(wxVERTICAL);
         vruler->SetFormat(Ruler::IntFormat);
         vruler->SetLabelEdges(true);
         vruler->SetRange(maxFreq, minFreq);
         vruler->SetUnits(wxT(""));
         vruler->SetLog(true);
      }
      else if (display == WaveTrack::PitchDisplay) {
         // Pitch
      }
   }

#ifdef USE_MIDI
   // The note track isn't drawing a ruler at all!
   // But it needs to!
   else if (t->GetKind() == Track::Note) {
      vruler->SetBounds(r.x, r.y+1, r.x + 1, r.y + r.height-1);
      vruler->SetOrientation(wxVERTICAL);
   }
#endif // USE_MIDI

   vruler->GetMaxSize(&t->vrulerSize.x, &t->vrulerSize.y);
}

/// Takes a value between min and max and returns a value between 
/// height and 0 
/// \todo  Should this function move int GuiWaveTrack where it can
/// then use the zoomMin, zoomMax and height values without having 
/// to have them passed in to it??
int GetWaveYPos(float value, float min, float max,
                int height, bool dB, bool outer,
                float dBr, bool clip)
{
   if (dB) {
      if (height == 0) {
         return 0;
      }

      float sign = (value >= 0 ? 1 : -1);

      if (value != 0.) {
         float db = 20.0 * log10(fabs(value));
         value = (db + dBr) / dBr;
         if (!outer) {
            value -= 0.5;
         }
         if (value < 0.0) {
            value = 0.0;
         }
         value *= sign;
      }
   }
   else {
      if (!outer) {
         if (value >= 0.0) {
            value -= 0.5;
         }
         else {
            value += 0.5;
         }
      }
   }

   if (clip) {
      if (value < min) {
         value = min;
      }
      if (value > max) {
         value = max;
      }
   }

   value = (max - value) / (max - min);
   return (int) (value * (height - 1) + 0.5);
}

void TrackArtist::DrawNegativeOffsetTrackArrows(wxDC &dc, const wxRect &r)
{
   // Draws two black arrows on the left side of the track to
   // indicate the user that the track has been time-shifted
   // to the left beyond t=0.0.

   dc.SetPen(*wxBLACK_PEN);
   AColor::Line(dc,
                r.x + 2, r.y + 6,
                r.x + 8, r.y + 6);
   AColor::Line(dc,
                r.x + 2, r.y + 6,
                r.x + 6, r.y + 2);
   AColor::Line(dc,
                r.x + 2, r.y + 6,
                r.x + 6, r.y + 10);
   AColor::Line(dc,
                r.x + 2, r.y + r.height - 8,
                r.x + 8, r.y + r.height - 8);
   AColor::Line(dc,
                r.x + 2, r.y + r.height - 8,
                r.x + 6, r.y + r.height - 4);
   AColor::Line(dc,
                r.x + 2, r.y + r.height - 8,
                r.x + 6, r.y + r.height - 12);
}

void TrackArtist::DrawWaveformBackground(wxDC &dc, const wxRect &r, const double env[],
                                         float zoomMin, float zoomMax, bool dB,
                                         const sampleCount where[],
                                         sampleCount ssel0, sampleCount ssel1,
                                         bool drawEnvelope)
{
   // Visually (one vertical slice of the waveform background, on its side;
   // the "*" is the actual waveform background we're drawing
   //
   //1.0                              0.0                             -1.0
   // |--------------------------------|--------------------------------|
   //      ***************                           ***************
   //      |             |                           |             |
   //    maxtop        maxbot                      mintop        minbot

   int h = r.height;
   int halfHeight = wxMax(h / 2, 1);
   int maxtop, lmaxtop = 0;
   int mintop, lmintop = 0;
   int maxbot, lmaxbot = 0;
   int minbot, lminbot = 0;
   bool sel, lsel = false;
   int x, lx = 0;
   int l, w;

   dc.SetPen(*wxTRANSPARENT_PEN);
   dc.SetBrush(blankBrush);
   dc.DrawRectangle(r);

   for (x = 0; x < r.width; x++) {
      // First we compute the truncated shape of the waveform background.
      // If drawEnvelope is true, then we compute the lower border of the
      // envelope.

      maxtop = GetWaveYPos(env[x], zoomMin, zoomMax,
                               h, dB, true, mdBrange, true);
      maxbot = GetWaveYPos(env[x], zoomMin, zoomMax,
                               h, dB, false, mdBrange, true);

      mintop = GetWaveYPos(-env[x], zoomMin, zoomMax,
                               h, dB, false, mdBrange, true);
      minbot = GetWaveYPos(-env[x], zoomMin, zoomMax,
                               h, dB, true, mdBrange, true);

      // Make sure it's odd so that a that max and min mirror each other
      mintop +=1;
      minbot +=1;

      if (!drawEnvelope || maxbot > mintop) {
         maxbot = halfHeight;
         mintop = halfHeight;
      }

      sel = (ssel0 <= where[x] && where[x + 1] < ssel1);

      if (lmaxtop == maxtop &&
          lmintop == mintop &&
          lmaxbot == maxbot &&
          lminbot == minbot &&
          lsel == sel) {
         continue;
      }

      dc.SetBrush(lsel ? selectedBrush : unselectedBrush);

      l = r.x + lx;
      w = x - lx;
      if (lmaxbot != lmintop - 1) {
         dc.DrawRectangle(l, r.y + lmaxtop, w, lmaxbot - lmaxtop);
         dc.DrawRectangle(l, r.y + lmintop, w, lminbot - lmintop);
      }
      else {
         dc.DrawRectangle(l, r.y + lmaxtop, w, lminbot - lmaxtop);
      }

      lmaxtop = maxtop;
      lmintop = mintop;
      lmaxbot = maxbot;
      lminbot = minbot;
      lsel = sel;
      lx = x;
   }

   dc.SetBrush(lsel ? selectedBrush : unselectedBrush);
   l = r.x + lx;
   w = x - lx;
   if (lmaxbot != lmintop - 1) {
      dc.DrawRectangle(l, r.y + lmaxtop, w, lmaxbot - lmaxtop);
      dc.DrawRectangle(l, r.y + lmintop, w, lminbot - lmintop);
   }
   else {
      dc.DrawRectangle(l, r.y + lmaxtop, w, lminbot - lmaxtop);
   }

   //OK, the display bounds are between min and max, which
   //is spread across r.height.  Draw the line at the proper place.

   if (zoomMin < 0 && zoomMax > 0) {
      int half = (int)((zoomMax / (zoomMax - zoomMin)) * h);
      dc.SetPen(*wxBLACK_PEN);
      AColor::Line(dc, r.x, r.y + half, r.x + r.width, r.y + half);
   }
}

void TrackArtist::DrawMinMaxRMS(wxDC &dc, const wxRect &r, const double env[],
                                float zoomMin, float zoomMax, bool dB,
                                const float min[], const float max[], const float rms[],
                                const int bl[], bool showProgress, bool muted)
{
   // Display a line representing the
   // min and max of the samples in this region
   int lasth1, h1;
   int lasth2, h2;
   int *r1 = new int[r.width];
   int *r2 = new int[r.width];
   int *clipped;
   int clipcnt = 0;
   int x;

   if (mShowClipping) {
      clipped =  new int[r.width];
   }

   long pixAnimOffset = (long)fabs((double)(wxDateTime::Now().GetTicks() * -10)) +
      wxDateTime::Now().GetMillisecond() / 100; //10 pixels a second
   
   bool drawStripes = true;
   bool drawWaveform = true;

   dc.SetPen(muted ? muteSamplePen : samplePen);
   for (x = 0; x < r.width; x++) {
      int xx = r.x + x;
      double v;

      v = min[x] * env[x];
      if (mShowClipping && v <= -MAX_AUDIO) {
         if (clipcnt == 0 || clipped[clipcnt - 1] != xx) {
            clipped[clipcnt++] = xx;
         }
      }
      h1 = GetWaveYPos(v, zoomMin, zoomMax,
                       r.height, dB, true, mdBrange, true);

      v = max[x] * env[x];
      if (mShowClipping && v >= MAX_AUDIO) {
         if (clipcnt == 0 || clipped[clipcnt - 1] != xx) {
            clipped[clipcnt++] = xx;
         }
      }
      h2 = GetWaveYPos(v, zoomMin, zoomMax,
                       r.height, dB, true, mdBrange, true);

      // JKC: This adjustment to h1 and h2 ensures that the drawn
      // waveform is continuous.
      if (x > 0) {
         if (h1 < lasth2) {
            h1 = lasth2 - 1;
         }
         if (h2 > lasth1) {
            h2 = lasth1 + 1;
         }
      }
      lasth1 = h1;
      lasth2 = h2;

      r1[x] = GetWaveYPos(-rms[x] * env[x], zoomMin, zoomMax,
                          r.height, dB, true, mdBrange, true);
      r2[x] = GetWaveYPos(rms[x] * env[x], zoomMin, zoomMax,
                          r.height, dB, true, mdBrange, true);

      // Make sure the rms isn't larger than the waveform min/max
      if (r1[x] > h1 - 1) {
         r1[x] = h1 - 1;
      }
      if (r2[x] < h2 + 1) {
         r2[x] = h2 + 1;
      }
      if (r2[x] > r1[x]) {
         r2[x] = r1[x];
      }

      if (bl[x] <= -1) {
         if (drawStripes) {
            // TODO:unify with buffer drawing.
            dc.SetPen((bl[x] % 2) ? muteSamplePen : samplePen);
            for (int y = 0; y < r.height / 25 + 1; y++) {
               // we are drawing over the buffer, but I think DrawLine takes care of this.
               AColor::Line(dc,
                            xx,
                            r.y + 25 * y + (x /*+pixAnimOffset*/) % 25,
                            xx, 
                            r.y + 25 * y + (x /*+pixAnimOffset*/) % 25 + 6); //take the min so we don't draw past the edge
            }
         }
         
         // draw a dummy waveform - some kind of sinusoid.  We want to animate it so the user knows it's a dummy.  Use the second's unit of a get time function.
         // Lets use a triangle wave for now since it's easier - I don't want to use sin() or make a wavetable just for this.
         if (drawWaveform) {
            int triX;
            dc.SetPen(samplePen);
            triX = fabs((double)((x + pixAnimOffset) % (2 * r.height)) - r.height) + r.height;
            for (int y = 0; y < r.height; y++) {
               if ((y + triX) % r.height == 0) {
                  dc.DrawPoint(xx, r.y + y);
               }
            }
         }
      }
      else {
         AColor::Line(dc, xx, r.y + h2, xx, r.y + h1);
      }
   }

   dc.SetPen(muted ? muteRmsPen : rmsPen);
   for (int x = 0; x < r.width; x++) {
      int xx = r.x + x;
      if (bl[x] <= -1) {
      }
      else if (r1[x] != r2[x]) {
         AColor::Line(dc, xx, r.y + r2[x], xx, r.y + r1[x]);
      }
   }

   // Draw the clipping lines
   if (clipcnt) {
      dc.SetPen(muted ? muteClippedPen : clippedPen);
      while (--clipcnt >= 0) {
         int xx = clipped[clipcnt];
         AColor::Line(dc, xx, r.y, xx, r.y + r.height);
      }
   }

   if (mShowClipping) {
      delete[] clipped;
   }

   delete [] r1;
   delete [] r2;
}

void TrackArtist::DrawIndividualSamples(wxDC &dc, const wxRect &r,
                                        float zoomMin, float zoomMax, bool dB,
                                        WaveClip *clip,
                                        double t0, double pps, double h,
                                        bool drawSamples, bool showPoints, bool muted)
{
   double rate = clip->GetRate();
   sampleCount s0 = (sampleCount) (t0 * rate + 0.5);
   sampleCount slen = (sampleCount) (r.width * rate / pps + 0.5);
   sampleCount snSamples = clip->GetNumSamples(); 
   
   slen += 4;

   if (s0 > snSamples) {
      return;
   }

   if (s0 + slen > snSamples) {
      slen = snSamples - s0;
   }

   float *buffer = new float[slen];
   clip->GetSamples((samplePtr)buffer, floatSample, s0, slen);

   int *xpos = new int[slen];
   int *ypos = new int[slen];
   int *clipped;
   int clipcnt = 0;
   sampleCount s;

   if (mShowClipping) {
      clipped = new int[slen];
   }

   dc.SetPen(muted ? muteSamplePen : samplePen);
   
   for (s = 0; s < slen; s++) {
      double tt = (s / rate);

      // MB: (s0/rate - t0) is the distance from the left edge of the screen
      //     to the first sample.
      int xx = (int)rint((tt + s0 / rate - t0) * pps);
      
      if (xx < -10000) {
         xx = -10000;
      }
      if (xx > 10000) {
         xx = 10000;
      }
      
      xpos[s] = xx;

      tt = buffer[s] * clip->GetEnvelope()->GetValueAtX(xx + r.x, r, h, pps);
      if (mShowClipping && (tt <= -MAX_AUDIO || tt >= MAX_AUDIO)) {
         clipped[clipcnt++] = xx;
      }
      ypos[s] = GetWaveYPos(tt, zoomMin, zoomMax,
                            r.height, dB, true, mdBrange, false);
      if (ypos[s] < -1) {
         ypos[s] = -1;
      }
      if (ypos[s] > r.height) {
         ypos[s] = r.height;
      }
   }

   // Draw lines
   for (s = 0; s < slen - 1; s++) {
      AColor::Line(dc,
                   r.x + xpos[s], r.y + ypos[s],
                   r.x + xpos[s + 1], r.y + ypos[s + 1]);
   }

   if (showPoints) {
      // Draw points
      int tickSize= drawSamples ? 4 : 3;// Bigger ellipses when draggable.
      wxRect pr;
      pr.width = tickSize;
      pr.height = tickSize;
      //different colour when draggable.
      dc.SetBrush( drawSamples ? dragsampleBrush : sampleBrush);
      for (s = 0; s < slen; s++) {
         if (ypos[s] >= 0 && ypos[s] < r.height) {
            pr.x = r.x + xpos[s] - tickSize/2;
            pr.y = r.y + ypos[s] - tickSize/2;
            dc.DrawEllipse(pr);
         }
      }
   }

   // Draw clipping
   if (clipcnt) {
      dc.SetPen(muted ? muteClippedPen : clippedPen);
      while (--clipcnt >= 0) {
         s = clipped[clipcnt];
         AColor::Line(dc, r.x + s, r.y, r.x + s, r.y + r.height);
      }
   }
   
   if (mShowClipping) {
      delete [] clipped;
   }

   delete[]buffer;
   delete[]xpos;
   delete[]ypos;
}

void TrackArtist::DrawEnvelope(wxDC &dc, const wxRect &r, const double env[],
                               float zoomMin, float zoomMax, bool dB)
{
   int h = r.height;

   dc.SetPen(AColor::envelopePen);

   for (int x = 0; x < r.width; x++) {
      int cenvTop = GetWaveYPos(env[x], zoomMin, zoomMax,
                                h, dB, true, mdBrange, true);

      int cenvBot = GetWaveYPos(-env[x], zoomMin, zoomMax,
                                h, dB, true, mdBrange, true);

      int envTop = GetWaveYPos(env[x], zoomMin, zoomMax,
                               h, dB, true, mdBrange, false);

      int envBot = GetWaveYPos(-env[x], zoomMin, zoomMax,
                               h, dB, true, mdBrange, false);

      // Make the collision at zero actually look solid
      if (cenvBot - cenvTop < 9) {
         int value = (int)((zoomMax / (zoomMax - zoomMin)) * h);
         cenvTop = value - 4;
         cenvBot = value + 4;
      }

      DrawEnvLine(dc, r, x, envTop, cenvTop, true);
      DrawEnvLine(dc, r, x, envBot, cenvBot, false);
   }
}

void TrackArtist::DrawEnvLine(wxDC &dc, const wxRect &r, int x, int y, int cy, bool top)
{
   int xx = r.x + x;
   int yy = r.y + cy;

   if (y < 0) {
      if (x % 4 != 3) {
         AColor::Line(dc, xx, yy, xx, yy + 3);
      }
   }
   else if (y > r.height) {
      if (x % 4 != 3) {
         AColor::Line(dc, xx, yy - 3, xx, yy);
      }
   }
   else {
      if (top) {
         AColor::Line(dc, xx, yy, xx, yy + 3);
      }
      else {
         AColor::Line(dc, xx, yy - 3, xx, yy);
      }
   }
}

void TrackArtist::DrawWaveform(WaveTrack *track,
                               wxDC & dc,
                               const wxRect & r,
                               const ViewInfo *viewInfo,
                               bool drawEnvelope,
                               bool drawSamples,
                               bool drawSliders,
                               bool dB,
                               bool muted)
{
   // MM: Draw background. We should optimize that a bit more.
   dc.SetPen(*wxTRANSPARENT_PEN);
   dc.SetBrush(blankBrush);
   dc.DrawRectangle(r);

   for (WaveClipList::compatibility_iterator it = track->GetClipIterator(); it; it = it->GetNext())
      DrawClipWaveform(track, it->GetData(), dc, r, viewInfo,
                       drawEnvelope, drawSamples, drawSliders,
                       dB, muted);

   // Update cache for locations, e.g. cutlines and merge points
   track->UpdateLocationsCache();
   
   for (int i = 0; i<track->GetNumCachedLocations(); i++) {
      WaveTrack::Location loc = track->GetCachedLocation(i);
      double x = (loc.pos - viewInfo->h) * viewInfo->zoom;
      if (x >= 0 && x < r.width) {
         dc.SetPen(*wxGREY_PEN);
         AColor::Line(dc, (int) (r.x + x - 1), r.y, (int) (r.x + x - 1), r.y + r.height);
         if (loc.typ == WaveTrack::locationCutLine) {
            dc.SetPen(*wxRED_PEN);
         }
         else {
            dc.SetPen(*wxBLACK_PEN);
         }
         AColor::Line(dc, (int) (r.x + x), r.y, (int) (r.x + x), r.y + r.height);
         dc.SetPen(*wxGREY_PEN);
         AColor::Line(dc, (int) (r.x + x + 1), r.y, (int) (r.x + x + 1), r.y + r.height);
      }
   }
}

void TrackArtist::DrawClipWaveform(WaveTrack *track,
                                   WaveClip *clip,
                                   wxDC & dc,
                                   const wxRect & r,
                                   const ViewInfo *viewInfo,
                                   bool drawEnvelope,
                                   bool drawSamples,
                                   bool drawSliders,
                                   bool dB,
                                   bool muted)
{
#if PROFILE_WAVEFORM
#   ifdef __WXMSW__
   __time64_t tv0, tv1;
   _time64(&tv0);
#   else
   struct timeval tv0, tv1;
   gettimeofday(&tv0, NULL);
#   endif
#endif
   double h = viewInfo->h;          //The horizontal position in seconds
   double pps = viewInfo->zoom;     //points-per-second--the zoom level
   double sel0 = viewInfo->sel0;    //left selection bound
   double sel1 = viewInfo->sel1;    //right selection bound
   double trackLen = clip->GetEndTime() - clip->GetStartTime();
   double tOffset = clip->GetOffset();
   double rate = clip->GetRate();
   double sps = 1./rate;            //seconds-per-sample

   //If the track isn't selected, make the selection empty
   if (!track->GetSelected()) {
      sel0 = sel1 = 0.0;
   }

   //Some bookkeeping time variables:
   double tstep = 1.0 / pps;                  // Seconds per point
   double tpre = h - tOffset;                 // offset corrected time of
                                              //  left edge of display
   double tpost = tpre + (r.width * tstep);   // offset corrected time of
                                              //  right edge of display

   // Determine whether we should show individual samples
   // or draw circular points as well
   bool showIndividualSamples = (pps / rate > 0.5);   //zoomed in a lot
   bool showPoints = (pps / rate > 3.0);              //zoomed in even more

   // Calculate actual selection bounds so that t0 > 0 and t1 < the
   // end of the track 

   double t0 = (tpre >= 0.0 ? tpre : 0.0);
   double t1 = (tpost < trackLen - sps * .99 ? tpost : trackLen - sps * .99);
   if (showIndividualSamples) {
      // adjustment so that the last circular point doesn't appear
      // to be hanging off the end
      t1 += 2. / pps;
   }

   // Make sure t1 (the right bound) is greater than 0
   if (t1 < 0.0) {
      t1 = 0.0;
   }

   // Make sure t1 is greater than t0
   if (t0 > t1) {
      t0 = t1;
   }

   // Calculate sample-based offset-corrected selection 

   // +.99 better centers the selection drag area at single-sample
   // granularity.  Not 1.0 as that would cause 'select whole
   // track' to lose the first sample
   sampleCount ssel0 = wxMax(0, sampleCount((sel0 - tOffset) * rate + .99)); 
   sampleCount ssel1 = wxMax(0, sampleCount((sel1 - tOffset) * rate + .99));

   //trim selection so that it only contains the actual samples
   if (ssel0 != ssel1 && ssel1 > (sampleCount)(0.5 + trackLen * rate)) {
      ssel1 = (sampleCount)(0.5 + trackLen * rate);
   }

   // The variable "mid" will be the rectangle containing the
   // actual waveform, as opposed to any blank area before
   // or after the track.
   wxRect mid = r;

   dc.SetPen(*wxTRANSPARENT_PEN);

   // If the left edge of the track is to the right of the left
   // edge of the display, then there's some blank area to the
   // left of the track.  Reduce the "mid"
   if (tpre < 0) {
      double delta = r.width;
      if (t0 < tpost) {
         delta = (int) ((t0 - tpre) * pps);
      }
      mid.x += (int)delta;
      mid.width -= (int)delta;
   }

   // If the right edge of the track is to the left of the the right
   // edge of the display, then there's some blank area to the right
   // of the track.  Reduce the "mid" rect by the
   // size of the blank area.
   if (tpost > t1) {
      wxRect post = r;
      if (t1 > tpre) {
         post.x += (int) ((t1 - tpre) * pps);
      }
      post.width = r.width - (post.x - r.x);
      mid.width -= post.width;
   }

   // The "mid" rect contains the part of the display actually
   // containing the waveform.  If it's empty, we're done.
   if (mid.width <= 0) {
#if PROFILE_WAVEFORM
#   ifdef __WXMSW__
      _time64(&tv1);
      double elapsed = _difftime64(tv1, tv0);
#   else
      gettimeofday(&tv1, NULL);
      double elapsed =
         (tv1.tv_sec + tv1.tv_usec*0.000001) -
         (tv0.tv_sec + tv0.tv_usec*0.000001);
#   endif
      gWaveformTimeTotal += elapsed;
      gWaveformTimeCount++;
#endif

     return;
   }

   // If we get to this point, the clip is actually visible on the
   // screen, so remember the display rectangle.
   clip->SetDisplayRect(mid);

   // The bounds (controlled by vertical zooming; -1.0...1.0
   // by default)
   float zoomMin, zoomMax;
   track->GetDisplayBounds(&zoomMin, &zoomMax);

   // Arrays containing the shape of the waveform - each array
   // has one value per pixel.
   float *min = new float[mid.width];
   float *max = new float[mid.width];
   float *rms = new float[mid.width];
   sampleCount *where = new sampleCount[mid.width + 1];
   int *bl = new int[mid.width];
   bool isLoadingOD = false;//true if loading on demand block in sequence.

   // The WaveClip class handles the details of computing the shape
   // of the waveform.  The only way GetWaveDisplay will fail is if
   // there's a serious error, like some of the waveform data can't
   // be loaded.  So if the function returns false, we can just exit.
   if (!clip->GetWaveDisplay(min, max, rms, bl, where,
                             mid.width, t0, pps, isLoadingOD)) {
      delete[] min;
      delete[] max;
      delete[] rms;
      delete[] where;
      delete[] bl;
      return;
   }

   // Get the values of the envelope corresponding to each pixel
   // in the display, and use these to compute the height of the
   // track at each pixel

   double *envValues = new double[mid.width];
   clip->GetEnvelope()->GetValues(envValues, mid.width, t0 + tOffset, tstep);

   // Draw the background of the track, outlining the shape of
   // the envelope and using a colored pen for the selected
   // part of the waveform
   DrawWaveformBackground(dc, mid, envValues, zoomMin, zoomMax, dB,
                          where, ssel0, ssel1, drawEnvelope);

   if (!showIndividualSamples) {
      DrawMinMaxRMS(dc, mid, envValues, zoomMin, zoomMax, dB,
                    min, max, rms, bl, isLoadingOD, muted);
   }
   else {
      DrawIndividualSamples(dc, mid, zoomMin, zoomMax, dB,
                            clip, t0, pps, h,
                            drawSamples, showPoints, muted);
   }

   if (drawEnvelope) {
      DrawEnvelope(dc, mid, envValues, zoomMin, zoomMax, dB);
      clip->GetEnvelope()->Draw(dc, r, h, pps, dB, zoomMin, zoomMax);
   }

   delete[] envValues;
   delete[] min;
   delete[] max;
   delete[] rms;
   delete[] where;
   delete[] bl;
   
   // Draw arrows on the left side if the track extends to the left of the
   // beginning of time.  :)
   if (h == 0.0 && tOffset < 0.0) {
      DrawNegativeOffsetTrackArrows(dc, r);
   }

   if (drawSliders) {
      DrawTimeSlider(track, dc, r, viewInfo, true);  // directed right
      DrawTimeSlider(track, dc, r, viewInfo, false); // directed left
   }

   // Draw clip edges
   dc.SetPen(*wxGREY_PEN);
   if (tpre < 0) {
      AColor::Line(dc,
                   mid.x - 1, mid.y,
                   mid.x - 1, mid.y + r.height);
   }
   if (tpost > t1) {
      AColor::Line(dc,
                   mid.x + mid.width, mid.y,
                   mid.x + mid.width, mid.y + r.height);
   }

#if PROFILE_WAVEFORM
#   ifdef __WXMSW__
   _time64(&tv1);
   double elapsed = _difftime64(tv1, tv0);
#   else
   gettimeofday(&tv1, NULL);
   double elapsed =
      (tv1.tv_sec + tv1.tv_usec*0.000001) -
      (tv0.tv_sec + tv0.tv_usec*0.000001);
#   endif
   gWaveformTimeTotal += elapsed;
   gWaveformTimeCount++;
   wxPrintf(wxT("Avg waveform drawing time: %f\n"),
          gWaveformTimeTotal / gWaveformTimeCount);
#endif
}


void TrackArtist::DrawTimeSlider(WaveTrack *track,
                                 wxDC & dc,
                                 const wxRect & r,
                                 const ViewInfo *viewInfo,
                                 bool rightwards)
{
   const int border = 3; // 3 pixels all round.
   const int width = 6; // width of the drag box.
   const int taper = 6; // how much the box tapers by.
   const int barSpacing = 4; // how far apart the bars are.
   const int barWidth = 3;
   const int xFlat = 3;

   //Enough space to draw in?
   if (r.height <= ((taper+border + barSpacing) * 2)) {
      return;
   }
   if (r.width <= (width * 2 + border * 3)) {
      return;
   }

   // The draggable box is tapered towards the direction you drag it.
   int leftTaper  = rightwards ? 0 : 6;
   int rightTaper = rightwards ? 6 : 0;

   int xLeft = rightwards ? (r.x + border - 2)
                          : (r.x + r.width + 1 - (border + width));
   int yTop  = r.y + border;
   int yBot  = r.y + r.height - border - 1;

   AColor::Light(&dc, false);
   AColor::Line(dc, xLeft,         yBot - leftTaper, xLeft,         yTop + leftTaper);
   AColor::Line(dc, xLeft,         yTop + leftTaper, xLeft + xFlat, yTop);
   AColor::Line(dc, xLeft + xFlat, yTop,             xLeft + width, yTop + rightTaper);

   AColor::Dark(&dc, false);
   AColor::Line(dc, xLeft + width,         yTop + rightTaper, xLeft + width,       yBot - rightTaper);
   AColor::Line(dc, xLeft + width,         yBot - rightTaper, xLeft + width-xFlat, yBot);
   AColor::Line(dc, xLeft + width - xFlat, yBot,              xLeft,               yBot - leftTaper);

   int firstBar = yTop + taper + taper / 2;
   int nBars    = (yBot - yTop - taper * 3) / barSpacing + 1;
   xLeft += (width - barWidth + 1) / 2;
   int y;
   int i;

   AColor::Light(&dc, false);
   for (i = 0;i < nBars; i++) {
      y = firstBar + barSpacing * i;
      AColor::Line(dc, xLeft, y, xLeft + barWidth, y);
   }
   AColor::Dark(&dc, false);
   for(i = 0;i < nBars; i++){
      y = firstBar + barSpacing * i + 1;
      AColor::Line(dc, xLeft, y, xLeft + barWidth, y);
   }
}


void TrackArtist::DrawSpectrum(WaveTrack *track,
                               wxDC & dc,
                               const wxRect & r,
                               const ViewInfo *viewInfo,
                               bool autocorrelation,
                               bool logF)
{
   // MM: Draw background. We should optimize that a bit more.
   dc.SetPen(*wxTRANSPARENT_PEN);
   dc.SetBrush(blankBrush);
   dc.DrawRectangle(r);

   if(!viewInfo->bUpdateTrackIndicator && viewInfo->bIsPlaying) {
      // BG: Draw (undecorated) waveform instead of spectrum
      DrawWaveform(track, dc, r, viewInfo, false, false, false, false, false);
      /*
      // BG: uncomment to draw grey instead of spectrum
      dc.SetBrush(unselectedBrush);
      dc.SetPen(unselectedPen);
      dc.DrawRectangle(r);
      */
      return;
   }

   for (WaveClipList::compatibility_iterator it = track->GetClipIterator(); it; it = it->GetNext()) {
      DrawClipSpectrum(track, it->GetData(), dc, r, viewInfo, autocorrelation, logF);
   }
}

float sumFreqValues(float *freq, int x0, float bin0, float bin1)
{
   float value;
   if (int(bin1) == int(bin0)) {
      value = freq[x0+int(bin0)];
   } else {
      float binwidth= bin1 - bin0;
      value = freq[x0 + int(bin0)] * (1.f - bin0 + (int)bin0);

      bin0 = 1 + int (bin0);
      while (bin0 < int(bin1)) {
         value += freq[x0 + int(bin0)];
         bin0 += 1.0;
      }
      value += freq[x0 + int(bin1)] * (bin1 - int(bin1));
      value /= binwidth;
   }
   return value;
}

void TrackArtist::DrawClipSpectrum(WaveTrack *track,
                                   WaveClip *clip,
                                   wxDC & dc,
                                   const wxRect & r,
                                   const ViewInfo *viewInfo,
                                   bool autocorrelation,
                                   bool logF)
{
#if PROFILE_WAVEFORM
#  ifdef __WXMSW__
   __time64_t tv0, tv1;
   _time64(&tv0);
#  else
   struct timeval tv0, tv1;
   gettimeofday(&tv0, NULL);
#  endif
#endif
   double h = viewInfo->h;
   double pps = viewInfo->zoom;
   double sel0 = viewInfo->sel0;
   double sel1 = viewInfo->sel1;

   sampleCount numSamples = clip->GetNumSamples();
   double tOffset = clip->GetOffset();
   double rate = clip->GetRate();
   double sps = 1./rate;

   int range = gPrefs->Read(wxT("/Spectrum/Range"), 80L);
   int gain = gPrefs->Read(wxT("/Spectrum/Gain"), 20L);

   if (!track->GetSelected())
      sel0 = sel1 = 0.0;

   double tpre = h - tOffset;
   double tstep = 1.0 / pps;
   double tpost = tpre + (r.width * tstep);
   double trackLen = clip->GetEndTime() - clip->GetStartTime();

   bool showIndividualSamples = (pps / rate > 0.5);   //zoomed in a lot
   double t0 = (tpre >= 0.0 ? tpre : 0.0);
   double t1 = (tpost < trackLen - sps*.99 ? tpost : trackLen - sps*.99);
   if(showIndividualSamples) t1+=2./pps; // for display consistency
   // with Waveform display

   // Make sure t1 (the right bound) is greater than 0
   if (t1 < 0.0)
      t1 = 0.0;

   // Make sure t1 is greater than t0
   if (t0 > t1)
      t0 = t1;

   sampleCount ssel0 = wxMax(0, sampleCount((sel0 - tOffset) * rate + .99));
   sampleCount ssel1 = wxMax(0, sampleCount((sel1 - tOffset) * rate + .99));

   //trim selection so that it only contains the actual samples
   if (ssel0 != ssel1 && ssel1 > (sampleCount)(0.5+trackLen*rate))
      ssel1 = (sampleCount)(0.5+trackLen*rate);

   // The variable "mid" will be the rectangle containing the
   // actual waveform, as opposed to any blank area before
   // or after the track.
   wxRect mid = r;

   dc.SetPen(*wxTRANSPARENT_PEN);

   // If the left edge of the track is to the right of the left
   // edge of the display, then there's some blank area to the
   // left of the track.  Reduce the "mid"
   // rect by size of the blank area.
   if (tpre < 0) {
      // Fill in the area to the left of the track
      wxRect pre = r;
      if (t0 < tpost)
         pre.width = (int) ((t0 - tpre) * pps);

      // Offset the rectangle containing the waveform by the width
      // of the area we just erased.
      mid.x += pre.width;
      mid.width -= pre.width;
   }

   // If the right edge of the track is to the left of the the right
   // edge of the display, then there's some blank area to the right
   // of the track.  Reduce the "mid" rect by the
   // size of the blank area.
   if (tpost > t1) {
      wxRect post = r;
      if (t1 > tpre)
         post.x += (int) ((t1 - tpre) * pps);
      post.width = r.width - (post.x - r.x);

      // Reduce the rectangle containing the waveform by the width
      // of the area we just erased.
      mid.width -= post.width;
   }

   // The "mid" rect contains the part of the display actually
   // containing the waveform.  If it's empty, we're done.
   if (mid.width <= 0) {
#if PROFILE_WAVEFORM
#   ifdef __WXMSW__
      _time64(&tv1);
      double elapsed = _difftime64(tv1, tv0);
#   else
      gettimeofday(&tv1, NULL);
      double elapsed =
         (tv1.tv_sec + tv1.tv_usec*0.000001) -
         (tv0.tv_sec + tv0.tv_usec*0.000001);
#   endif
      gWaveformTimeTotal += elapsed;
      gWaveformTimeCount++;
#endif
      return;
   }

   // We draw directly to a bit image in memory,
   // and then paint this directly to our offscreen
   // bitmap.  Note that this could be optimized even
   // more, but for now this is not bad.  -dmazzoni
   wxImage *image = new wxImage((int) mid.width, (int) mid.height);
   if (!image)return;
   unsigned char *data = image->GetData();

   int windowSize = GetSpectrumWindowSize();
   int half = windowSize/2;
   float *freq = new float[mid.width * half];
   sampleCount *where = new sampleCount[mid.width+1];

   bool updated = clip->GetSpectrogram(freq, where, mid.width,
                              t0, pps, autocorrelation);
   int ifreq = lrint(rate/2);

   int maxFreq;
   if (!logF)
      maxFreq = GetSpectrumMaxFreq(ifreq);
   else
      maxFreq = GetSpectrumLogMaxFreq(ifreq);
   if(maxFreq > ifreq)
      maxFreq = ifreq;

   int minFreq;
   if (!logF) {
      minFreq = GetSpectrumMinFreq(0);
      if(minFreq < 0)
         minFreq = 0;
   }
   else {
      minFreq = GetSpectrumLogMinFreq(ifreq/1000.0);
      if(minFreq < 1)
         minFreq = ifreq/1000.0;
   }

   bool usePxCache = false;

   if( !updated && clip->mSpecPxCache->valid && (clip->mSpecPxCache->len == mid.height * mid.width) 
#ifdef EXPERIMENTAL_FFT_Y_GRID
   && mFftYGrid==fftYGridOld
#endif //EXPERIMENTAL_FFT_Y_GRID
#ifdef EXPERIMENTAL_FIND_NOTES
   && mFftFindNotes==fftFindNotesOld
   && mFindNotesMinA==findNotesMinAOld
   && mNumberOfMaxima==findNotesNOld
   && mFindNotesQuantize==findNotesQuantizeOld
#endif
   ) {
      usePxCache = true;
   }
   else {
      delete clip->mSpecPxCache;
      clip->mSpecPxCache = new SpecPxCache(mid.width * mid.height);
      usePxCache = false;
      clip->mSpecPxCache->valid = true;
#ifdef EXPERIMENTAL_FIND_NOTES
      fftFindNotesOld=mFftFindNotes;
      findNotesMinAOld=mFindNotesMinA;
      findNotesNOld=mNumberOfMaxima;
      findNotesQuantizeOld=mFindNotesQuantize;
#endif
   }

   int minSamples = int (minFreq * windowSize / rate + 0.5);   // units are fft bins
   int maxSamples = int (maxFreq * windowSize / rate + 0.5);
   float binPerPx = float(maxSamples - minSamples) / float(mid.height);

   int x = 0;
   sampleCount w1 = (sampleCount) ((t0*rate + x *rate *tstep) + .5);
 
   const float 
//      e=exp(1.0f), 
      log2=log(2.0f),
      f=rate/2.0f/half, 
      lmin=log(float(minFreq)),
      lmax=log(float(maxFreq)),
#ifdef EXPERIMENTAL_FFT_SKIP_POINTS
      lmins=log(float(minFreq)/(mFftSkipPoints+1)),
      lmaxs=log(float(maxFreq)/(mFftSkipPoints+1)),
#else //!EXPERIMENTAL_FFT_SKIP_POINTS
      lmins=lmin,
      lmaxs=lmax,
#endif //EXPERIMENTAL_FFT_SKIP_POINTS
      scale=lmax-lmin, 
      scale2=(lmax-lmin)/log2, 
      lmin2=lmin/log2 /*,
      expo=exp(scale)*/ ;

#ifdef EXPERIMENTAL_FFT_Y_GRID
   bool *yGrid;
   yGrid=new bool[mid.height];
   for (int y = 0; y < mid.height; y++) {
      float n =(float(y  )/mid.height*scale2-lmin2)*12; 
      float n2=(float(y+1)/mid.height*scale2-lmin2)*12; 
      float f =float(minFreq)/(mFftSkipPoints+1)*pow(2.0f, n /12.0f+lmin2);
      float f2=float(minFreq)/(mFftSkipPoints+1)*pow(2.0f, n2/12.0f+lmin2);
      n =log(f /440)/log2*12;
      n2=log(f2/440)/log2*12;
      if (floor(n) < floor(n2))
         yGrid[y]=true;
      else
         yGrid[y]=false;
   }
#endif //EXPERIMENTAL_FFT_Y_GRID

#ifdef EXPERIMENTAL_FIND_NOTES
   int maxima[128];
   float maxima0[128], maxima1[128];
   const float 
#ifdef EXPERIMENTAL_FFT_SKIP_POINTS
      f2bin = half/(rate/2.0f/(mFftSkipPoints+1)),
#else //!EXPERIMENTAL_FFT_SKIP_POINTS
      f2bin = half/(rate/2.0f),
#endif //EXPERIMENTAL_FFT_SKIP_POINTS
      bin2f = 1.0f/f2bin,
      minDistance = pow(2.0f, 2.0f/12.0f),
      i0=exp(lmin)/f,
      i1=exp(scale+lmin)/f,
      minColor=0.0f;
   const int maxTableSize=1024;
   int *indexes=new int[maxTableSize];
#endif //EXPERIMENTAL_FIND_NOTES

   while (x < mid.width) 
   {
      sampleCount w0 = w1;
      w1 = (sampleCount) ((t0*rate + (x+1) *rate *tstep) + .5);
      if (!logF)
      {
         for (int yy = 0; yy < mid.height; yy++) {
            bool selflag = (ssel0 <= w0 && w1 < ssel1);
            unsigned char rv, gv, bv;
            float value;

            if(!usePxCache) {
               float bin0 = float (yy) * binPerPx + minSamples;
               float bin1 = float (yy + 1) * binPerPx + minSamples;


               if (int (bin1) == int (bin0))
                  value = freq[half * x + int (bin0)];
               else {
                  float binwidth= bin1 - bin0;
                  value = freq[half * x + int (bin0)] * (1.f - bin0 + (int)bin0);

                  bin0 = 1 + int (bin0);
                  while (bin0 < int (bin1)) {
                     value += freq[half * x + int (bin0)];
                     bin0 += 1.0;
                  }

                  // Do not reference past end of freq array.
                  if (int(bin1) >= half) {
                     bin1 -= 1.0;
                  }

                  value += freq[half * x + int (bin1)] * (bin1 - int (bin1));

                  value /= binwidth;
               }

               if (!autocorrelation) {
                  // Last step converts dB to a 0.0-1.0 range
                  value = (value + range + gain) / (double)range;
               }

               if (value > 1.0)
                  value = float(1.0);
               if (value < 0.0)
                  value = float(0.0);
               clip->mSpecPxCache->values[x * mid.height + yy] = value;
            }
            else
               value = clip->mSpecPxCache->values[x * mid.height + yy];

            GetColorGradient(value, selflag, mIsGrayscale, &rv, &gv, &bv);

            int px = ((mid.height - 1 - yy) * mid.width + x) * 3;
            data[px++] = rv;
            data[px++] = gv;
            data[px] = bv;
         }
      }
      else //logF
      {
         bool selflag = (ssel0 <= w0 && w1 < ssel1);
         unsigned char rv, gv, bv;
         float value;
         int x0=x*half;

#ifdef EXPERIMENTAL_FIND_NOTES
         int maximas=0;
         if (!usePxCache && mFftFindNotes) {
            for (int i = maxTableSize-1; i >= 0; i--)
               indexes[i]=-1;
            
            // Build a table of (most) values, put the index in it.
            for (int i = int(i0); i < int(i1); i++) {
               float freqi=freq[x0+int(i)];
               int value=int((freqi+gain+range)/range*(maxTableSize-1));
               if (value < 0)
                  value=0;
               if (value >= maxTableSize)
                  value=maxTableSize-1;
               indexes[value]=i;
            }
            // Build from the indices an array of maxima.
            for (int i = maxTableSize-1; i >= 0; i--) {
               int index=indexes[i];
               if (index >= 0) {
                  float freqi=freq[x0+index];
                  if (freqi < mFindNotesMinA) 
                     break;

                  bool ok=true;
                  for (int m=0; m < maximas; m++) {
                     // Avoid to store very close maxima.
                     float maxm = maxima[m];
                     if (maxm/index < minDistance && index/maxm < minDistance) {
                        ok=false;
                        break;
                     }
                  }
                  if (ok) {
                     maxima[maximas++] = index;
                     if (maximas >= mNumberOfMaxima)
                        break;
                  }
               }
            }

// The f2pix helper macro converts a frequency into a pixel coordinate.
#define f2pix(f) (log(f)-lmins)/(lmaxs-lmins)*mid.height

            // Possibly quantize the maxima frequencies and create the pixel block limits.
            for (int i=0; i < maximas; i++) {
               int index=maxima[i];
               float f = float(index)*bin2f;
               if (mFindNotesQuantize)
               {  f = exp(int(log(f/440)/log2*12-0.5)/12.0f*log2)*440;
                  maxima[i] = f*f2bin;
               }
               float f0 = exp((log(f/440)/log2*24-1)/24.0f*log2)*440;
               maxima0[i] = f2pix(f0);
               float f1 = exp((log(f/440)/log2*24+1)/24.0f*log2)*440;
               maxima1[i] = f2pix(f1);
            }
         }
         int it=0;
         int oldBin0=-1;
         bool inMaximum = false;
#endif //EXPERIMENTAL_FIND_NOTES

         for (int yy = 0; yy < mid.height; yy++) {
            if(!usePxCache) {
               float h=float(yy)/mid.height; 
               float yy2=exp(h*scale+lmin)/f;
               if (int(yy2)>=half)
                  yy2=half-1;
               if (yy2<0)
                  yy2=0;
               float bin0 = float(yy2);
               float h1=float(yy+1)/mid.height; 
               float yy3=exp(h1*scale+lmin)/f;
               if (int(yy3)>=half)
                  yy3=half-1;
               if (yy3<0)
                  yy3=0;
               float bin1 = float(yy3);

#ifdef EXPERIMENTAL_FIND_NOTES
               if (mFftFindNotes) {
                  if (it < maximas) {
                     float i0=maxima0[it];
                     if (yy >= i0)
                        inMaximum = true;

                     if (inMaximum) {
                        float i1=maxima1[it];
                        if (yy+1 <= i1) {
                           value=sumFreqValues(freq, x0, bin0, bin1);
                           if (value < mFindNotesMinA)
                              value = minColor;
                           else
                              value = (value + gain + range) / (double)range;
                        } else {
                           it++;
                           inMaximum = false;
                           value = minColor;
                        }
                     } else {
                        value = minColor;
                     }
                  } else
                     value = minColor;
               } else
#endif //EXPERIMENTAL_FIND_NOTES
               {
                  value=sumFreqValues(freq, x0, bin0, bin1);
                  if (!autocorrelation) {
                     // Last step converts dB to a 0.0-1.0 range
                     value = (value + gain + range) / (double)range;
                  }
               }
               if (value > 1.0)
                  value = float(1.0);
               if (value < 0.0)
                  value = float(0.0);
               clip->mSpecPxCache->values[x * mid.height + yy] = value;
            }
            else
               value = clip->mSpecPxCache->values[x * mid.height + yy];

            GetColorGradient(value, selflag, mIsGrayscale, &rv, &gv, &bv);

#ifdef EXPERIMENTAL_FFT_Y_GRID
            if (mFftYGrid && yGrid[yy]) {
               rv /= 1.1f;
               gv /= 1.1f;
               bv /= 1.1f;
            }
#endif //EXPERIMENTAL_FFT_Y_GRID

            int px = ((mid.height - 1 - yy) * mid.width + x) * 3;
            data[px++] = rv;
            data[px++] = gv;
            data[px] = bv;
         }
      }
      x++;
   }

   // If we get to this point, the clip is actually visible on the
   // screen, so remember the display rectangle.
   clip->SetDisplayRect(mid);

   wxBitmap converted = wxBitmap(*image);

   wxMemoryDC memDC;

   memDC.SelectObject(converted);

   dc.Blit(mid.x, mid.y, mid.width, mid.height, &memDC, 0, 0, wxCOPY, FALSE);

   delete image;
   delete[] where;
   delete[] freq;
#ifdef EXPERIMENTAL_FFT_Y_GRID
   delete[] yGrid;
#endif //EXPERIMENTAL_FFT_Y_GRID
#ifdef EXPERIMENTAL_FIND_NOTES
   delete[] indexes;
#endif //EXPERIMENTAL_FIND_NOTES
}

void TrackArtist::InvalidateSpectrumCache(TrackList *tracks)
{
   TrackListOfKindIterator iter(Track::Wave, tracks);
   for (Track *t = iter.First(); t; t = iter.Next()) {
      InvalidateSpectrumCache((WaveTrack *)t);
   }
}

void TrackArtist::InvalidateSpectrumCache(WaveTrack *track)
{
   WaveClipList::compatibility_iterator it;
   for (it = track->GetClipIterator(); it; it = it->GetNext()) {
      it->GetData()->mSpecPxCache->valid = false;
   }
}


#ifdef USE_MIDI
/*
Note: recall that Allegro attributes end in a type identifying letter.

In addition to standard notes, an Allegro_Note can denote a graphic.
A graphic is a note with a loud of zero (for quick testing) and an
attribute named "shapea" set to one of the following atoms:
    line
        from (time, pitch) to (time+dur, y1r), where y1r is an
          attribute
    rectangle
        from (time, pitch) to (time+dur, y1r), where y1r is an
          attribute
    triangle
        coordinates are (time, pitch), (x1r, y1r), (x2r, y2r)
        dur must be the max of x1r-time, x2r-time
    polygon
        coordinates are (time, pitch), (x1r, y1r), (x2r, y2r),
          (x3r, y3r), ... are coordinates (since we cannot represent
          arrays as attribute values, we just generate as many
          attribute names as we need)
        dur must be the max of xNr-time for all N
    oval
        similar to rectangle
        Note: this oval has horizontal and vertical axes only
    text
        drawn at (time, pitch)
        duration should be zero (text is clipped based on time and duration,
          NOT based on actual coordinates)

and optional attributes as follows:
    linecolori is 0x00rrggbb format color for line or text foreground
    fillcolori is 0x00rrggbb format color for fill or text background
    linethicki is line thickness in pixels, 0 for no line
    filll is true to fill rectangle or draw text background (default is false)
    fonta is one of ['roman', 'swiss', 'modern'] (font, otherwise use default)
    weighta may be 'bold' (font) (default is normal)
    sizei is font size (default is 8)
    justifys is a string containing two letters, a horizontal code and a
      vertical code. The horizontal code is as follows:
        l: the coordinate is to the left of the string (default)
        c: the coordinate is at the center of the string
        r: the coordinate is at the right of the string
      The vertical code is as follows:
        t: the coordinate is at the top of the string
        c: the coordinate is at the center of the string
        b: the coordinate is at the bottom of the string
        d: the coordinate is at the baseline of the string (default)
      Thus, -justifys:"lt" places the left top of the string at the point
        given by (pitch, time). The default value is "ld".
*/

/* Declare Static functions */
static char *IsShape(Alg_note_ptr note);
static double LookupRealAttribute(Alg_note_ptr note, Alg_attribute attr, double def);
static long LookupIntAttribute(Alg_note_ptr note, Alg_attribute attr, long def);
static bool LookupLogicalAttribute(Alg_note_ptr note, Alg_attribute attr, bool def);
static const char *LookupStringAttribute(Alg_note_ptr note, Alg_attribute attr, const char *def);
static char *LookupAtomAttribute(Alg_note_ptr note, Alg_attribute attr, char *def);
static int PITCH_TO_Y(double p, int bottom);
static char *LookupAtomAttribute(Alg_note_ptr note, Alg_attribute attr, char *def);
static int PITCH_TO_Y(double p, int bottom);

// returns NULL if note is not a shape,
// returns atom (string) value of note if note is a shape
char *IsShape(Alg_note_ptr note)
{
  Alg_parameters_ptr parameters = note->parameters;
  while (parameters) {
    if (strcmp(parameters->parm.attr_name(), "shapea") == 0) {
      return parameters->parm.a;
    }
    parameters = parameters->next;
  }
  return NULL;
}

// returns value of attr, or default if not found
double LookupRealAttribute(Alg_note_ptr note, Alg_attribute attr, double def)
{
  Alg_parameters_ptr parameters = note->parameters;
  while (parameters) {
    if (parameters->parm.attr_name() == attr + 1 &&
        parameters->parm.attr_type() == 'r') {
      return parameters->parm.r;
    }
    parameters = parameters->next;
  }
  return def;
}

// returns value of attr, or default if not found
long LookupIntAttribute(Alg_note_ptr note, Alg_attribute attr, long def)
{
  Alg_parameters_ptr parameters = note->parameters;
  while (parameters) {
    if (parameters->parm.attr_name() == attr + 1 &&
        parameters->parm.attr_type() == 'i') {
      return parameters->parm.i;
    }
    parameters = parameters->next;
  }
  return def;
}

// returns value of attr, or default if not found
bool LookupLogicalAttribute(Alg_note_ptr note, Alg_attribute attr, bool def)
{
  Alg_parameters_ptr parameters = note->parameters;
  while (parameters) {
    if (parameters->parm.attr_name() == attr + 1 &&
        parameters->parm.attr_type() == 'l') {
      return parameters->parm.l;
    }
    parameters = parameters->next;
  }
  return def;
}

// returns value of attr, or default if not found
const char *LookupStringAttribute(Alg_note_ptr note, Alg_attribute attr, const char *def)
{
  Alg_parameters_ptr parameters = note->parameters;
  while (parameters) {
    if (parameters->parm.attr_name() == attr + 1 &&
        parameters->parm.attr_type() == 's') {
      return parameters->parm.s;
    }
    parameters = parameters->next;
  }
  return def;
}

// returns value of attr, or default if not found
char *LookupAtomAttribute(Alg_note_ptr note, Alg_attribute attr, char *def)
{
  Alg_parameters_ptr parameters = note->parameters;
  while (parameters) {
    if (parameters->parm.attr_name() == attr + 1 &&
        parameters->parm.attr_type() == 'a') {
      return parameters->parm.s;
    }
    parameters = parameters->next;
  }
  return def;
}
#endif // USE_MIDI

#define TIME_TO_X(t) (r.x + (int) (((t) - h) * pps))
#define X_TO_TIME(xx) (((xx) - r.x) / pps + h)

// CLIP(x) changes x to lie between +/- CLIP_MAX due to graphics display problems
//  with very large coordinate values (this happens when you zoom in very far)
//  This will cause incorrect things to be displayed, but at these levels of zoom
//  you will only see a small fraction of the overall shape. Note that rectangles
//  and lines are clipped in a way that preserves correct graphics, so in
//  particular, line plots will be correct at any zoom (limited by floating point
//  precision).
#define CLIP_MAX 16000
#define CLIP(x) { long c = (x); if (c < -CLIP_MAX) c = -CLIP_MAX; \
                  if (c > CLIP_MAX) c = CLIP_MAX; (x) = c; }

#define RED(i) ( unsigned char )( (((i) >> 16) & 0xff) )
#define GREEN(i) ( unsigned char )( (((i) >> 8) & 0xff) )
#define BLUE(i) ( unsigned char )( ((i) & 0xff) )

//#define PITCH_TO_Y(p) (r.y + r.height - int(pitchht * ((p) + 0.5 - pitch0) + 0.5))

#ifdef USE_MIDI
int PITCH_TO_Y(double p, int bottom)
{
   int octave = (((int) (p + 0.5)) / 12);
   int n = ((int) (p + 0.5)) % 12;
   
   return bottom - octave * octaveHeight - notePos[n] - 4;
}

void TrackArtist::DrawNoteTrack(NoteTrack *track,
                                wxDC & dc,
                                const wxRect & r,
                                const ViewInfo *viewInfo)
{
   double h = viewInfo->h;
   double pps = viewInfo->zoom;
   double sel0 = viewInfo->sel0;
   double sel1 = viewInfo->sel1;

   double h1 = X_TO_TIME(r.x + r.width);

   Alg_seq_ptr seq = track->mSeq;
   if (!seq) {
      assert(track->mSerializationBuffer);
      Alg_track_ptr alg_track = seq->unserialize(track->mSerializationBuffer,
            track->mSerializationLength);
      assert(alg_track->get_type() == 's');
      track->mSeq = seq = (Alg_seq_ptr) alg_track;
      free(track->mSerializationBuffer);
      track->mSerializationBuffer = NULL;
   }
   assert(seq);
   int visibleChannels = track->mVisibleChannels;

   if (!track->GetSelected())
      sel0 = sel1 = 0.0;

   int ctrpitch = 60;
   int pitch0;
   int pitchht = 4;

   int numPitches = r.height / pitchht;
   pitch0 = (ctrpitch - numPitches/2);

   int bottomNote = track->GetBottomNote();
   int bottom = r.height +
   ((bottomNote / 12) * octaveHeight + notePos[bottomNote % 12]);

   //214, 214, 214
   dc.SetBrush(blankBrush);
   dc.SetPen(blankPen);
   dc.DrawRectangle(r);

   wxPen blackStripePen;
   blackStripePen.SetColour(192, 192, 192);
   wxBrush blackStripeBrush;
   blackStripeBrush.SetColour(192, 192, 192);

   dc.SetBrush(blackStripeBrush);

   for (int octave = 0; octave < 50; octave++) {
      int obottom = r.y + bottom - octave * octaveHeight;

      if (obottom > r.y && obottom < r.y + r.height) {
         dc.SetPen(*wxBLACK_PEN);
         AColor::Line(dc, r.x, obottom, r.x + r.width, obottom);
      }
      if (obottom - 26 > r.y && obottom - 26 < r.y + r.height) {
         dc.SetPen(blackStripePen);
         AColor::Line(dc, r.x, obottom - 26, r.x + r.width, obottom - 26);
      }

      wxRect br = r;
      br.height = 5;
      for (int black = 0; black < 5; black++) {
         br.y = obottom - blackPos[black] - 4;
         if (br.y > r.y && br.y + br.height < r.y + r.height) {
            dc.SetPen(blackStripePen);
            dc.DrawRectangle(br);
         }
      }
   }

   dc.SetClippingRegion(r);

   // Draw the selection background
   // First, the white keys, as a single rectangle
   wxRect selBG;
   selBG.y = r.y;
   selBG.height = r.height;
   selBG.x = TIME_TO_X(sel0);
   selBG.width = TIME_TO_X(sel1) - TIME_TO_X(sel0);

   wxPen selectedWhiteKeyPen;
   selectedWhiteKeyPen.SetColour(165, 165, 190);
   dc.SetPen(selectedWhiteKeyPen);
   
   wxBrush selectedWhiteKeyBrush;
   selectedWhiteKeyBrush.SetColour(165, 165, 190);
   dc.SetBrush(selectedWhiteKeyBrush);

   dc.DrawRectangle(selBG);

   // Then, the black keys and octave stripes, as smaller rectangles
   wxPen selectedBlackKeyPen;
   selectedBlackKeyPen.SetColour(148, 148, 170);
   wxBrush selectedBlackKeyBrush;
   selectedBlackKeyBrush.SetColour(148, 148, 170);

   dc.SetBrush(selectedBlackKeyBrush);

   for (int octave = 0; octave < 50; octave++) {
      int obottom = selBG.y + bottom - octave * octaveHeight;

      if (obottom > selBG.y && obottom < selBG.y + selBG.height) {
         dc.SetPen(*wxBLACK_PEN);
         AColor::Line(dc, selBG.x, obottom, selBG.x + selBG.width, obottom);
      }
      if (obottom - 26 > selBG.y && obottom - 26 < selBG.y + selBG.height) {
         dc.SetPen(selectedBlackKeyPen);
         AColor::Line(dc, selBG.x, obottom - 26, selBG.x + selBG.width, obottom - 26);
      }

      wxRect bselBG = selBG;
      bselBG.height = 5;
      for (int black = 0; black < 5; black++) {
         bselBG.y = obottom - blackPos[black] - 4;
         if (bselBG.y > selBG.y && bselBG.y + bselBG.height < selBG.y + selBG.height) {
            dc.SetPen(selectedBlackKeyPen);
            dc.DrawRectangle(bselBG);
         }
      }
   }

   // NOTE: it would be better to put this in some global initialization
   // function rather than do lookups every time.
   Alg_attribute line = symbol_table.insert_string("line");
   Alg_attribute rectangle = symbol_table.insert_string("rectangle");
   Alg_attribute triangle = symbol_table.insert_string("triangle");
   Alg_attribute polygon = symbol_table.insert_string("polygon");
   Alg_attribute oval = symbol_table.insert_string("oval");
   Alg_attribute text = symbol_table.insert_string("text");
   Alg_attribute texts = symbol_table.insert_string("texts");
   Alg_attribute x1r = symbol_table.insert_string("x1r");
   Alg_attribute x2r = symbol_table.insert_string("x2r");
   Alg_attribute y1r = symbol_table.insert_string("y1r");
   Alg_attribute y2r = symbol_table.insert_string("y2r");
   Alg_attribute linecolori = symbol_table.insert_string("linecolori");
   Alg_attribute fillcolori = symbol_table.insert_string("fillcolori");
   Alg_attribute linethicki = symbol_table.insert_string("linethicki");
   Alg_attribute filll = symbol_table.insert_string("filll");
   Alg_attribute fonta = symbol_table.insert_string("fonta");
   Alg_attribute roman = symbol_table.insert_string("roman");
   Alg_attribute swiss = symbol_table.insert_string("swiss");
   Alg_attribute modern = symbol_table.insert_string("modern");
   Alg_attribute weighta = symbol_table.insert_string("weighta");
   Alg_attribute bold = symbol_table.insert_string("bold");
   Alg_attribute sizei = symbol_table.insert_string("sizei");
   Alg_attribute justifys = symbol_table.insert_string("justifys");

   // We want to draw in seconds, so we need to convert to seconds
   seq->convert_to_seconds();

   Alg_iterator iterator(seq, false);
   iterator.begin();
   //for every event
   Alg_event_ptr evt;
   printf ("go time\n");
   while (evt = iterator.next()) {
   
      //printf ("one note");

      //if the event is a note
      if (evt->get_type() == 'n') {

         Alg_note_ptr note = (Alg_note_ptr) evt;

         //if the notes channel is visible
         if (visibleChannels & (1 << (evt->chan & 15))) {
            double x = note->time;
            double x1 = note->time + note->dur;
            if (x < h1 && x1 > h) { // omit if outside box
               char *shape = NULL;
               if (note->loud > 0.0 || !(shape = IsShape(note))) {

                  int octave = (((int) (note->pitch + 0.5)) / 12);
                  int n = ((int) (note->pitch + 0.5)) % 12;

                  wxRect nr;
                  nr.y = bottom - octave * octaveHeight - notePos[n] - 4;
                  nr.height = 5;

                  if (nr.y + nr.height >= 0 && nr.y < r.height) {

                     if (nr.y + nr.height > r.height)
                        nr.height = r.height - nr.y;
                     if (nr.y < 0) {
                        nr.height += nr.y;
                        nr.y = 0;
                     }
                     nr.y += r.y;

                     nr.x = r.x + (int) ((note->time - h) * pps);
                     nr.width = (int) (note->dur * pps) + 1;

                     if (nr.x + nr.width >= r.x && nr.x < r.x + r.width) {
                        if (nr.x < r.x) {
                           nr.width -= (r.x - nr.x);
                           nr.x = r.x;
                        }
                        if (nr.x + nr.width > r.x + r.width)
                           nr.width = r.x + r.width - nr.x;

                        AColor::MIDIChannel(&dc, note->chan + 1);

//                      if (note->time + note->dur >= sel0 && note->time <= sel1) {
//                         dc.SetBrush(*wxWHITE_BRUSH);
//                         dc.DrawRectangle(nr);
//                      } else {
                        dc.DrawRectangle(nr);
                        AColor::LightMIDIChannel(&dc, note->chan + 1);
                        AColor::Line(dc, nr.x, nr.y, nr.x + nr.width-2, nr.y);
                        AColor::Line(dc, nr.x, nr.y, nr.x, nr.y + nr.height-2);
                        AColor::DarkMIDIChannel(&dc, note->chan + 1);
                        AColor::Line(dc, nr.x+nr.width-1, nr.y,
                              nr.x+nr.width-1, nr.y+nr.height-1);
                        AColor::Line(dc, nr.x, nr.y+nr.height-1,
                              nr.x+nr.width-1, nr.y+nr.height-1);
//                      }
                     }
                  }

               } else if (shape) {
                  // draw a shape according to attributes
                  // add 0.5 to pitch because pitches are plotted with height = pitchht,
                  // thus, the center is raised by pitchht * 0.5
                  int y = PITCH_TO_Y(note->pitch, bottom);
                  long linecolor = LookupIntAttribute(note, linecolori, -1);
                  long linethick = LookupIntAttribute(note, linethicki, 1);
                  long fillcolor = -1;
                  long fillflag = 0;

                  // set default color to be that of channel
                  AColor::MIDIChannel(&dc, note->chan+1);
                  if (shape != text) {
                     if (linecolor != -1)
                        dc.SetPen(wxPen(wxColour(RED(linecolor), 
                              GREEN(linecolor),
                              BLUE(linecolor)),
                              linethick, wxSOLID));
                  }
                  if (shape != line) {
                     fillcolor = LookupIntAttribute(note, fillcolori, -1);
                     fillflag = LookupLogicalAttribute(note, filll, false);

                     if (fillcolor != -1) 
                        dc.SetBrush(wxBrush(wxColour(RED(fillcolor),
                              GREEN(fillcolor),
                              BLUE(fillcolor)),
                              wxSOLID));
                     if (!fillflag) dc.SetBrush(*wxTRANSPARENT_BRUSH);
                  }
                  int y1 = PITCH_TO_Y(LookupRealAttribute(note, y1r, note->pitch), bottom);
                  if (shape == line) {
                     // extreme zooms caues problems under windows, so we have to do some
                     // clipping before calling display routine
                     if (x < h) { // clip line on left
                        y = int((y + (y1 - y) * (h - x) / (x1 - x)) + 0.5);
                        x = h;
                     }
                     if (x1 > h1) { // clip line on right
                        y1 = int((y + (y1 - y) * (h1 - x) / (x1 - x)) + 0.5);
                        x1 = h1;
                     }
                     AColor::Line(dc, TIME_TO_X(x), y, TIME_TO_X(x1), y1);
                  } else if (shape == rectangle) {
                     if (x < h) { // clip on left, leave 10 pixels to spare
                        x = h - (linethick + 10) / pps;
                     }
                     if (x1 > h1) { // clip on right, leave 10 pixels to spare
                        x1 = h1 + (linethick + 10) / pps;
                     }
                     dc.DrawRectangle(TIME_TO_X(x), y, int((x1 - x) * pps + 0.5), y1 - y + 1);
                  } else if (shape == triangle) {
                     wxPoint points[3];
                     points[0].x = TIME_TO_X(x);
                     CLIP(points[0].x);
                     points[0].y = y;
                     points[1].x = TIME_TO_X(LookupRealAttribute(note, x1r, note->pitch));
                     CLIP(points[1].x);
                     points[1].y = y1;
                     points[2].x = TIME_TO_X(LookupRealAttribute(note, x2r, note->time));
                     CLIP(points[2].x);
                     points[2].y = PITCH_TO_Y(LookupRealAttribute(note, y2r, note->pitch), bottom);
                     dc.DrawPolygon(3, points);
                  } else if (shape == polygon) {
                     wxPoint points[20]; // upper bound of 20 sides
                     points[0].x = TIME_TO_X(x);
                     CLIP(points[0].x);
                     points[0].y = y;
                     points[1].x = TIME_TO_X(LookupRealAttribute(note, x1r, note->time));
                     CLIP(points[1].x);
                     points[1].y = y1;
                     points[2].x = TIME_TO_X(LookupRealAttribute(note, x2r, note->time));
                     CLIP(points[2].x);
                     points[2].y = PITCH_TO_Y(LookupRealAttribute(note, y2r, note->pitch), bottom);
                     int n = 3;
                     while (n < 20) {
                        char name[8];
                        sprintf(name, "x%dr", n);
                        Alg_attribute attr = symbol_table.insert_string(name);
                        double xn = LookupRealAttribute(note, attr, -1000000.0);
                        if (xn == -1000000.0) break;
                        points[n].x = TIME_TO_X(xn);
                        CLIP(points[n].x);
                        sprintf(name, "y%dr", n - 1);
                        attr = symbol_table.insert_string(name);
                        double yn = LookupRealAttribute(note, attr, -1000000.0);
                        if (yn == -1000000.0) break;
                        points[n].y = PITCH_TO_Y(yn, bottom);
                        n++;
                     }
                     dc.DrawPolygon(n, points);
                  } else if (shape == oval) {
                     int ix = TIME_TO_X(x);
                     CLIP(ix);
                     int ix1 = int((x1 - x) * pps + 0.5);
                     if (ix1 > CLIP_MAX * 2) ix1 = CLIP_MAX * 2; // CLIP a width
                     dc.DrawEllipse(ix, y, ix1, y1 - y + 1);
                  } else if (shape == text) {
                     if (linecolor != -1)
                        dc.SetTextForeground(wxColour(RED(linecolor), 
                              GREEN(linecolor),
                              BLUE(linecolor)));
                     // if no color specified, copy color from brush
                     else dc.SetTextForeground(dc.GetBrush().GetColour());

                     // This seems to have no effect, so I commented it out. -RBD
                     //if (fillcolor != -1)
                     //  dc.SetTextBackground(wxColour(RED(fillcolor), 
                     //                                GREEN(fillcolor),
                     //                                BLUE(fillcolor)));
                     //// if no color specified, copy color from brush
                     //else dc.SetTextBackground(dc.GetPen().GetColour());

                     char *font = LookupAtomAttribute(note, fonta, NULL);
                     char *weight = LookupAtomAttribute(note, weighta, NULL);
                     int size = LookupIntAttribute(note, sizei, 8);
                     const char *justify = LookupStringAttribute(note, justifys, "ld");
                     wxFont wxfont;
                     wxfont.SetFamily(font == roman ? wxROMAN : 
                        (font == swiss ? wxSWISS :
                           (font == modern ? wxMODERN : wxDEFAULT)));
                     wxfont.SetStyle(wxNORMAL);
                     wxfont.SetWeight(weight == bold ? wxBOLD : wxNORMAL);
                     wxfont.SetPointSize(size);
                     dc.SetFont(wxfont);

                     // now do justification
                     const char *s = LookupStringAttribute(note, texts, "");
                     #ifdef __WXMAC__
                        long textWidth, textHeight;
                     #else
                        int textWidth, textHeight;
                     #endif
                     dc.GetTextExtent(LAT1CTOWX(s), &textWidth, &textHeight);
                     long hoffset = 0;
                     long voffset = -textHeight; // default should be baseline of text

                     if (strlen(justify) != 2) justify = "ld";

                     if (justify[0] == 'c') hoffset = -(textWidth/2);
                     else if (justify[0] == 'r') hoffset = -textWidth;

                     if (justify[1] == 't') voffset = 0;
                     else if (justify[1] == 'c') voffset = -(textHeight/2);
                     else if (justify[1] == 'b') voffset = -textHeight;
                     if (fillflag) {
                        // It should be possible to do this with background color,
                        // but maybe because of the transfer mode, no background is
                        // drawn. To fix this, just draw a rectangle:
                        dc.SetPen(wxPen(wxColour(RED(fillcolor), 
                              GREEN(fillcolor),
                              BLUE(fillcolor)),
                              1, wxSOLID));
                        dc.DrawRectangle(TIME_TO_X(x) + hoffset, y + voffset,
                              textWidth, textHeight);
                     }
                     dc.DrawText(LAT1CTOWX(s), TIME_TO_X(x) + hoffset, y + voffset);
                  }
               }
            }
         }
      }
   }
   iterator.end();
   dc.DestroyClippingRegion();
}
#endif // USE_MIDI


void TrackArtist::DrawLabelTrack(LabelTrack *track,
                                 wxDC & dc,
                                 const wxRect & r,
                                 const ViewInfo *viewInfo)
{
   double sel0 = viewInfo->sel0;
   double sel1 = viewInfo->sel1;
   
   if (!track->GetSelected())
      sel0 = sel1 = 0.0;
   
   track->Draw(dc, r, viewInfo->h, viewInfo->zoom, sel0, sel1);
}

void TrackArtist::DrawTimeTrack(TimeTrack *track,
                                wxDC & dc,
                                const wxRect & r,
                                const ViewInfo *viewInfo)
{
   track->Draw(dc, r, viewInfo->h, viewInfo->zoom);
   wxRect envRect = r;
   envRect.height -= 2;
   track->GetEnvelope()->Draw(dc, envRect, viewInfo->h, viewInfo->zoom, 
               false,0.0,1.0);
}

void TrackArtist::UpdatePrefs()
{
   mdBrange = gPrefs->Read(wxT("/GUI/EnvdBRange"), mdBrange);
   mShowClipping = gPrefs->Read(wxT("/GUI/ShowClipping"), mShowClipping);

   mMaxFreq = gPrefs->Read(wxT("/Spectrum/MaxFreq"), -1);
   mMinFreq = gPrefs->Read(wxT("/Spectrum/MinFreq"), -1);
   mLogMaxFreq = gPrefs->Read(wxT("/SpectrumLog/MaxFreq"), -1);
   mLogMinFreq = gPrefs->Read(wxT("/SpectrumLog/MinFreq"), -1);

   mWindowSize = gPrefs->Read(wxT("/Spectrum/FFTSize"), 256);
   mIsGrayscale = (gPrefs->Read(wxT("/Spectrum/Grayscale"), 0L) != 0);

#ifdef EXPERIMENTAL_FFT_Y_GRID
   mFftYGrid = (gPrefs->Read(wxT("/Spectrum/FFTYGrid"), 0L) != 0);
#endif //EXPERIMENTAL_FFT_Y_GRID

#ifdef EXPERIMENTAL_FIND_NOTES
   mFftFindNotes = (gPrefs->Read(wxT("/Spectrum/FFTFindNotes"), 0L) != 0);
   mFindNotesMinA = gPrefs->Read(wxT("/Spectrum/FindNotesMinA"), -30.0);
   mNumberOfMaxima = gPrefs->Read(wxT("/Spectrum/FindNotesN"), 5L);
   mFindNotesQuantize = (gPrefs->Read(wxT("/Spectrum/FindNotesQuantize"), 0L) != 0);
#endif //EXPERIMENTAL_FIND_NOTES

#ifdef EXPERIMENTAL_FFT_SKIP_POINTS
   mFftSkipPoints = gPrefs->Read(wxT("/Spectrum/FFTSkipPoints"), 0L);
#endif //EXPERIMENTAL_FFT_SKIP_POINTS
}

// Get various preference values
int TrackArtist::GetSpectrumMinFreq(int deffreq)
{
   return mMinFreq < 0 ? deffreq : mMinFreq;
}

int TrackArtist::GetSpectrumMaxFreq(int deffreq)
{
   return mMaxFreq < 0 ? deffreq : mMaxFreq;
}

int TrackArtist::GetSpectrumLogMinFreq(int deffreq)
{
   return mLogMinFreq < 0 ? deffreq : mLogMinFreq;
}

int TrackArtist::GetSpectrumLogMaxFreq(int deffreq)
{
   return mLogMaxFreq < 0 ? deffreq : mLogMaxFreq;
}

int TrackArtist::GetSpectrumWindowSize()
{
   return mWindowSize;
}

#ifdef EXPERIMENTAL_FFT_SKIP_POINTS
int TrackArtist::GetSpectrumFftSkipPoints()
{
   return mFftSkipPoints;
}
#endif

// Set various preference values
void TrackArtist::SetSpectrumMinFreq(int freq)
{
   mMinFreq = freq;
}

void TrackArtist::SetSpectrumMaxFreq(int freq)
{
   mMaxFreq = freq;
}

void TrackArtist::SetSpectrumLogMinFreq(int freq)
{
   mLogMinFreq = freq;
}

void TrackArtist::SetSpectrumLogMaxFreq(int freq)
{
   mLogMaxFreq = freq;
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
// arch-tag: cd156c5b-b867-48eb-9289-03d21bdf14e5

