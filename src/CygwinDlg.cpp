/*******************************************************************************
 *
 * Copyright (C) 2009, Alexander Stigsen, e-texteditor.com
 *
 * This software is licensed under the Open Company License as described
 * in the file license.txt, which you should have received as part of this
 * distribution. The terms are also available at http://opencompany.org/license.
 *
 * This software is distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY
 * KIND, either express or implied.
 *
 ******************************************************************************/

#include "CygwinDlg.h"
#include "shlobj.h"
#include <wx/file.h>

#include "Execute.h"
#include "Env.h"
#include "eDocumentPath.h"
#include "IAppPaths.h"
#include "eSettings.h"


class CygwinInstallThread : public wxThread {
public:
  CygwinInstallThread(cxCygwinInstallMode mode, const wxString& appPath);
  virtual void *Entry();
private:
  const cxCygwinInstallMode m_mode;
  const wxString m_appPath;
};


BEGIN_EVENT_TABLE(CygwinDlg, wxDialog)
	EVT_BUTTON(wxID_OK, CygwinDlg::OnButtonOk)
END_EVENT_TABLE()

CygwinDlg::CygwinDlg(wxWindow *parent, cxCygwinDlgMode mode):
	wxDialog (parent, wxID_ANY, wxEmptyString, wxDefaultPosition),
	m_mode(mode)
{
	SetTitle( m_mode == cxCYGWIN_INSTALL ? _("Cygwin not installed!") : _("Update Cygwin"));

	const wxString installMsg = _("e uses the Cygwin package to provide many of the powerful Unix-like commands not usually available on Windows. Without Cygwin, some of e's advanced features will be disabled.\n\nWould you like to install Cygwin now? (If you say no, e will ask you again when you try to use an advanced feature.)");
	const wxString updateMsg = _("To get full benefit of the bundle commands, your cygwin installation needs to be updated.");

	// Create controls
	wxStaticText* msg = new wxStaticText(this, wxID_ANY, (m_mode == cxCYGWIN_INSTALL) ? installMsg : updateMsg);
	msg->Wrap(300);
	wxStaticText* radioTitle = new wxStaticText(this, wxID_ANY, (m_mode == cxCYGWIN_INSTALL) ? _("Install Cygwin:") : _("Update Cygwin"));
	m_autoRadio = new wxRadioButton(this, wxID_ANY, _("Automatic"), wxDefaultPosition, wxDefaultSize, wxRB_GROUP);
	m_manualRadio = new wxRadioButton(this, wxID_ANY, _("Manual"));

	// Create Layout
	wxBoxSizer* mainSizer = new wxBoxSizer(wxVERTICAL);
		mainSizer->Add(msg, 0, wxALL, 5);
		mainSizer->Add(radioTitle, 0, wxLEFT|wxTOP, 5);
		wxBoxSizer* radioSizer = new wxBoxSizer(wxVERTICAL);
			radioSizer->Add(m_autoRadio, 0, wxLEFT, 20);
			radioSizer->Add(m_manualRadio, 0, wxLEFT, 20);
			mainSizer->Add(radioSizer, 0, wxALL, 5);
		mainSizer->Add(CreateButtonSizer(wxOK|wxCANCEL), 0, wxEXPAND|wxALL, 5);

	SetSizerAndFit(mainSizer);
	Centre();
}

void CygwinDlg::OnButtonOk(wxCommandEvent& WXUNUSED(event)) {
	const wxString appPath = GetAppPaths().AppPath();
	const cxCygwinInstallMode install_mode = m_autoRadio->GetValue() ? cxCYGWIN_AUTO : cxCYGWIN_MANUAL;
	new CygwinInstallThread(install_mode, appPath);

	EndModal(wxID_OK);
}

// ---- CygwinInstallThread ------------------------------------------------------------

CygwinInstallThread::CygwinInstallThread(cxCygwinInstallMode mode, const wxString& appPath):
	m_mode(mode),
	m_appPath(appPath)
{
	Create();
    Run();
}

void* CygwinInstallThread::Entry() {
	wxString cygPath = eDocumentPath::GetCygwinDir();
	if (cygPath.empty()) {
		// Get the base drive letter from the Windows system path, assuming C:
		wxString driveLetter = wxT("C");
		TCHAR szPath[MAX_PATH]; szPath[0]='\0';
		if (SUCCEEDED(SHGetFolderPath(NULL, CSIDL_SYSTEM, NULL, 0, szPath)) &&
			szPath[0] != '\0') {
				driveLetter = szPath[0];
		}

		cygPath = driveLetter + wxT(":\\cygwin");

		// Make sure it get eventual proxy settings from IE
		wxFileName::Mkdir(cygPath + wxT("\\etc\\setup\\"), 0777, wxPATH_MKDIR_FULL);
		wxFile file(cygPath + wxT("\\etc\\setup\\last-connection"), wxFile::write);
		if (file.IsOpened()) file.Write(wxT("IE"));

	}
	const wxString supportPath = m_appPath + wxT("Support\\bin");
	const wxString packages = wxT("curl,wget,tidy,perl,python,ruby,file,libiconv");

	const wxString cmd = wxString::Format(wxT("%s\\setup.exe"), supportPath);
	wxString options;
	if (m_mode == cxCYGWIN_AUTO) {
		options = wxString::Format(wxT("--quiet-mode --no-shortcuts --site http://mirrors.dotsrc.org/cygwin/ --packages %s  --root \"%s\""), packages, cygPath);
	}
	else {
		options = wxString::Format(wxT("--packages %s"), packages);
	}

	// Run Setup
	// We have to use ShellExecute to enable Vista UAC elevation
	const wxString verb = wxT("runas");
	SHELLEXECUTEINFO sei;
    ZeroMemory(&sei, sizeof(sei));
    sei.cbSize = sizeof(SHELLEXECUTEINFOW);
    sei.fMask  = SEE_MASK_NOASYNC|SEE_MASK_NOCLOSEPROCESS;
    sei.nShow  = SW_SHOWNORMAL;
	sei.lpVerb = verb.c_str();
    sei.lpFile = cmd.c_str();
	sei.lpParameters = options.c_str();
	if (!::ShellExecuteEx(&sei)) return NULL;

	// Wait for setup to complete
	if (sei.hProcess != 0) {
		::WaitForSingleObject(sei.hProcess, INFINITE);
		::CloseHandle(sei.hProcess);
	}

	// Path may have been changed during install
	cygPath = eDocumentPath::GetCygwinDir();
	if (cygPath.empty()) return NULL;

	// Setup environment
	cxEnv env;
	env.SetToCurrent();
	env.SetEnv(wxT("TM_SUPPORT_PATH"), eDocumentPath::WinPathToCygwin(m_appPath + wxT("Support"))); // needed by post-install

	// Run postinstall
	cxExecute exec(env);
	exec.SetUpdateWindow(false);
	exec.SetShowWindow(false);
	const wxString supportScript = supportPath + wxT("\\cygwin-post-install.sh");
	const wxString postCmd = wxString::Format(wxT("\"%s\\bin\\bash\" --login \"%s\""), cygPath, supportScript);
	const int postexit = exec.Execute(postCmd);
	if (postexit != 0) {
		wxLogDebug(wxT("post-install failed %d"), postexit);
		// return NULL; // don't return. May fail on Vista
	}

	// Mark this update as done
	const wxFileName supportFile(supportScript);
	const wxDateTime updateTime = supportFile.GetModificationTime();
	eGetSettings().SetSettingLong(wxT("cyg_date"), updateTime.GetValue());

	return NULL;
}
