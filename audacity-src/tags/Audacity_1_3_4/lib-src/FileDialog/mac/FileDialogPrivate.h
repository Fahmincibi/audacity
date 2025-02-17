/////////////////////////////////////////////////////////////////////////////
// Name:        filedlg.h
// Purpose:     wxFileDialog class
// Author:      Stefan Csomor
// Modified by: Leland Lucius
// Created:     1998-01-01
// RCS-ID:      $Id: FileDialogPrivate.h,v 1.1 2007-04-30 04:12:52 llucius Exp $
// Copyright:   (c) Stefan Csomor
// Licence:     wxWindows licence
//
// Modified for Audacity to support an additional button on Save dialogs
//
/////////////////////////////////////////////////////////////////////////////

#ifndef _FILEDIALOGMAC_H_
#define _FILEDIALOGMAC_H_

//-------------------------------------------------------------------------
// FileDialog
//-------------------------------------------------------------------------

class WXDLLEXPORT FileDialog: public wxFileDialogBase
{
DECLARE_DYNAMIC_CLASS(FileDialog)
protected:
    long m_dialogStyle;
    wxArrayString m_fileNames;
    wxArrayString m_paths;

    wxString m_buttonlabel;
    fdCallback m_callback;
    void *m_cbdata;

public:
    FileDialog(wxWindow *parent,
               const wxString& message = wxFileSelectorPromptStr,
               const wxString& defaultDir = wxEmptyString,
               const wxString& defaultFile = wxEmptyString,
               const wxString& wildCard = wxFileSelectorDefaultWildcardStr,
               long style = 0,
               const wxPoint& pos = wxDefaultPosition);

    virtual void GetPaths(wxArrayString& paths) const { paths = m_paths; }
    virtual void GetFilenames(wxArrayString& files) const { files = m_fileNames ; }

    virtual int ShowModal();
    
    // not supported for file dialog, RR
    virtual void DoSetSize(int WXUNUSED(x), int WXUNUSED(y),
                           int WXUNUSED(width), int WXUNUSED(height),
                           int WXUNUSED(sizeFlags) = wxSIZE_AUTO) {}

   virtual void EnableButton(wxString label, fdCallback cb, void *cbdata);
   virtual void ClickButton(int index);
};

#endif
