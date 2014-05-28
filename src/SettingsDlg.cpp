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

#include "SettingsDlg.h"

#include <wx/image.h>
#include <wx/notebook.h>
#include <wx/fontmap.h>
#include <wx/spinctrl.h>

#include "EnvVarsPanel.h"

#include "UpdaterThread.h"
#include "eSettings.h"
#include "AppVersion.h"

#ifdef __WXMSW__
	#include "eDocumentPath.h"
	#include "ShellRunner.h"
	#include "Env.h"
#endif


inline bool encoding_allows_bom(wxFontEncoding enc) {
	return (enc == wxFONTENCODING_UTF7 || enc == wxFONTENCODING_UTF8 || enc == wxFONTENCODING_UTF16LE ||
		enc == wxFONTENCODING_UTF16BE || enc == wxFONTENCODING_UTF32LE || enc == wxFONTENCODING_UTF32BE);
}

// Ctrl ids
enum {
	CTRL_LOADPIC = 100,
	CTRL_AUTOPAIR,
	CTRL_AUTOWRAP,
	CTRL_KEEPSTATE,
	CTRL_CHECKCHANGE,
	CTRL_SHOWMARGIN,
	CTRL_WRAPMARGIN,
	CTRL_MARGINSPIN,
	CTRL_ATOMICSAVE,
	CTRL_LASTTAB,
	CTRL_HIGHLIGHTVARIABLES,
	CTRL_HIGHLIGHTHTML,
	CTRL_LINEENDING,
	CTRL_ENCODING,
	CTRL_BOM,
	CTRL_CYGWIN_ACTION, // Only added on Windows
	CTRL_CHECK_FOR_UPDATES,
	CTRL_AUTOUPDATE,
};

BEGIN_EVENT_TABLE(SettingsDlg, wxDialog)
	EVT_BUTTON(wxID_OK, SettingsDlg::OnButtonOk)
	EVT_BUTTON(CTRL_LOADPIC, SettingsDlg::OnButtonLoadPic)
	EVT_BUTTON(CTRL_CYGWIN_ACTION, SettingsDlg::OnButtonCygwinAction)
	EVT_BUTTON(CTRL_CHECK_FOR_UPDATES, SettingsDlg::OnButtonCheckForUpdates)
	EVT_CHECKBOX(CTRL_AUTOPAIR, SettingsDlg::OnCheckAutoPair)
	EVT_CHECKBOX(CTRL_AUTOWRAP, SettingsDlg::OnCheckAutoWrap)
	EVT_CHECKBOX(CTRL_KEEPSTATE, SettingsDlg::OnCheckKeepState)
	EVT_CHECKBOX(CTRL_CHECKCHANGE, SettingsDlg::OnCheckCheckChange)
	EVT_CHECKBOX(CTRL_SHOWMARGIN, SettingsDlg::OnCheckShowMargin)
	EVT_CHECKBOX(CTRL_WRAPMARGIN, SettingsDlg::OnCheckWrapMargin)
	EVT_CHECKBOX(CTRL_AUTOUPDATE, SettingsDlg::OnCheckCheckForUpdates)
	EVT_CHECKBOX(CTRL_ATOMICSAVE, SettingsDlg::OnCheckAtomicSave)
	EVT_CHECKBOX(CTRL_LASTTAB, SettingsDlg::OnCheckLastTab)
	EVT_CHECKBOX(CTRL_HIGHLIGHTVARIABLES, SettingsDlg::OnCheckHighlightVariables)
	EVT_CHECKBOX(CTRL_HIGHLIGHTHTML, SettingsDlg::OnCheckHighlightHtml)
	EVT_SPINCTRL(CTRL_MARGINSPIN, SettingsDlg::OnMarginSpin)
	EVT_COMBOBOX(CTRL_LINEENDING, SettingsDlg::OnComboEol)
	EVT_COMBOBOX(CTRL_ENCODING, SettingsDlg::OnComboEncoding)
	EVT_CHECKBOX(CTRL_BOM, SettingsDlg::OnCheckBom)
END_EVENT_TABLE()


SettingsDlg::SettingsDlg(wxWindow *parent, CatalystWrapper cw, eSettings& settings):
	wxDialog (parent, -1, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxDEFAULT_DIALOG_STYLE|wxRESIZE_BORDER),
	m_settings(settings), m_catalyst(cw), m_ctUserPic(false)
{
	SetTitle (_("Settings"));

	wxBoxSizer* mainSizer = new wxBoxSizer(wxVERTICAL);
	wxNotebook* notebook = new wxNotebook(this, wxID_ANY);

	wxPanel* settingsPage = CreateSettingsPage(notebook);
	notebook->AddPage(settingsPage, _("Settings"));

	wxPanel* encodingPage = CreateEncodingPage(notebook);
	notebook->AddPage(encodingPage, _("Encoding"));

	wxPanel* profilePage = CreateProfilePage(notebook);
	notebook->AddPage(profilePage, _("Profile"));

	m_envPage = new EnvVarsPanel(notebook);
	m_envPage->AddVars(this->m_settings.env);
	notebook->AddPage(m_envPage, _("Environment"));


#ifdef __WXMSW__
	m_unixPage = CreateUnixPage(notebook);
	notebook->AddPage(m_unixPage, _("UNIX"));
	UpdateUnixPage();
#endif

	wxPanel* updatePage = CreateUpdatePage(notebook);
	notebook->AddPage(updatePage, _("Updates"));

	notebook->SetSelection(0);
	mainSizer->Add(notebook, 0, wxEXPAND|wxALL, 5);
	mainSizer->Add(CreateButtonSizer(wxOK|wxCANCEL), 0, wxEXPAND|wxALL, 5);
	SetSizerAndFit(mainSizer);

	// Manually set size hints
	const wxSize size = GetSize();
	SetSizeHints(size.x, size.y, -1, size.y);

	Centre();
}

wxPanel* SettingsDlg::CreateUpdatePage(wxWindow* parent) {
	wxPanel* page = new wxPanel(parent, wxID_ANY);

	wxFlexGridSizer* sizer = new wxFlexGridSizer(4, 2, 0, 0);
	sizer->AddGrowableCol(1); // 2nd column is sizable


	// Last update string
	wxString when = wxT("<unknown>");
	wxLongLong lastup = -1;
	if (m_settings.GetSettingLong(wxT("lastupdatecheck"), lastup)) {
		wxDateTime lastupdated(lastup);
		when = lastupdated.Format();
	}

	// Last update label
	wxStaticText* labelLastUpdate = new wxStaticText(page, wxID_ANY, _("Last Update:"));
	sizer->Add(labelLastUpdate , 0, wxALIGN_CENTER_VERTICAL|wxALL, 5);

	wxStaticText* labelWhen= new wxStaticText(page, wxID_ANY, when);
	sizer->Add(labelWhen, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5);

	// Checkbox
	sizer->AddStretchSpacer();

	m_checkForUpdatesAtStartup = new wxCheckBox(page, CTRL_AUTOUPDATE, _("Check for updates automatically"));
	sizer->Add(m_checkForUpdatesAtStartup, 0, wxALL, 5);


	// Button
	sizer->AddStretchSpacer();

	m_checkForUpdatesButton = new wxButton(page, CTRL_CHECK_FOR_UPDATES, _("Check now"));
	sizer->Add(m_checkForUpdatesButton, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5);

	page->SetSizerAndFit(sizer);


	// Settings defaults.
	bool checkForUpdates = true;
	m_settings.GetSettingBool(wxT("checkForUpdates"), checkForUpdates);
	m_checkForUpdatesAtStartup->SetValue(checkForUpdates);

	return page;
}

wxPanel* SettingsDlg::CreateSettingsPage(wxWindow* parent) {
	wxPanel* settingsPage = new wxPanel(parent, wxID_ANY);

	wxCheckBox* autoPair = new wxCheckBox(settingsPage, CTRL_AUTOPAIR, _("Auto-pair characters (quotes etc.)"));
	wxCheckBox* autoWrap = new wxCheckBox(settingsPage, CTRL_AUTOWRAP, _("Auto-wrap selections"));
	wxCheckBox* keepState = new wxCheckBox(settingsPage, CTRL_KEEPSTATE, _("Keep state between sessions"));
	wxCheckBox* checkChange = new wxCheckBox(settingsPage, CTRL_CHECKCHANGE, _("Check for external file modifications"));
	wxCheckBox* showMargin = new wxCheckBox(settingsPage, CTRL_SHOWMARGIN, _("Show margin line"));
	m_marginSpin = new wxSpinCtrl(settingsPage, CTRL_MARGINSPIN, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxSP_ARROW_KEYS, 0, 500, 80);
	m_wrapMargin = new wxCheckBox(settingsPage, CTRL_WRAPMARGIN, _("Wrap at margin line"));
	wxCheckBox* atomicSave = new wxCheckBox(settingsPage, CTRL_ATOMICSAVE, _("Atomic save"));
	wxCheckBox* lastTab = new wxCheckBox(settingsPage, CTRL_LASTTAB, _("Go to last active tab on Ctrl-Tab"));
	wxCheckBox* highlightVariables = new wxCheckBox(settingsPage, CTRL_HIGHLIGHTVARIABLES, _("Highlight occurances of a variable when clicked."));
	wxCheckBox* highlightHtml = new wxCheckBox(settingsPage, CTRL_HIGHLIGHTHTML, _("Highlight matching html tags."));

	wxBoxSizer* settingsSizer = new wxBoxSizer(wxVERTICAL);
		settingsSizer->Add(autoPair, 0, wxALL, 5);
		settingsSizer->Add(autoWrap, 0, wxALL, 5);
		settingsSizer->Add(keepState, 0, wxALL, 5);
		settingsSizer->Add(checkChange, 0, wxALL, 5);
		wxBoxSizer* marginSizer = new wxBoxSizer(wxHORIZONTAL);
			marginSizer->Add(showMargin, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5);
			marginSizer->Add(m_marginSpin);
			settingsSizer->Add(marginSizer);
		settingsSizer->Add(m_wrapMargin, 0, wxALL, 5);
		settingsSizer->Add(atomicSave, 0, wxALL, 5);
		settingsSizer->Add(lastTab, 0, wxALL, 5);
		settingsSizer->Add(highlightVariables, 0, wxALL, 5);
		settingsSizer->Add(highlightHtml, 0, wxALL, 5);
	settingsPage->SetSizer(settingsSizer);

	// Settings defaults.
	bool doAutoPair = true;
	bool doAutoWrap = true;
	bool doKeepState = true;
	bool doCheckChange = true;
	bool doShowMargin = false;
	bool doWrapMargin = false;
	bool noAtomicSave = false;
	bool doLastTab = false;
	bool doHighlightVariables = false;
	bool doHighlightHtml = false;
	int marginChars = 80;

	m_settings.GetSettingBool(wxT("autoPair"), doAutoPair);
	m_settings.GetSettingBool(wxT("autoWrap"), doAutoWrap);
	m_settings.GetSettingBool(wxT("keepState"), doKeepState);
	m_settings.GetSettingBool(wxT("checkChange"), doCheckChange);
	m_settings.GetSettingBool(wxT("showMargin"), doShowMargin);
	m_settings.GetSettingBool(wxT("wrapMargin"), doWrapMargin);
	m_settings.GetSettingInt(wxT("marginChars"), marginChars);
	m_settings.GetSettingBool(wxT("disable_atomic_save"), noAtomicSave);
	m_settings.GetSettingBool(wxT("gotoLastTab"), doLastTab);
	m_settings.GetSettingBool(wxT("highlightVariables"), doHighlightVariables);
	m_settings.GetSettingBool(wxT("highlightHtml"), doHighlightHtml);

	// Update ctrls
	autoPair->SetValue(doAutoPair);
	autoWrap->SetValue(doAutoWrap);
	keepState->SetValue(doKeepState);
	checkChange->SetValue(doCheckChange);
	showMargin->SetValue(doShowMargin);
	m_marginSpin->SetValue(marginChars);
	m_wrapMargin->SetValue(doWrapMargin);
	if (!doShowMargin) {
		m_marginSpin->Disable();
		m_wrapMargin->Disable();
	}
	atomicSave->SetValue(!noAtomicSave);
	lastTab->SetValue(doLastTab);
	highlightVariables->SetValue(doHighlightVariables);
	highlightHtml->SetValue(doHighlightHtml);

	return settingsPage;
}

wxPanel* SettingsDlg::CreateEncodingPage(wxWindow* parent) {
	wxPanel* encodingPage = new wxPanel(parent, wxID_ANY);
	wxStaticText* desc = new wxStaticText(encodingPage, wxID_ANY, _("Set default encoding for new files."));
	wxStaticText* labelLine = new wxStaticText(encodingPage, wxID_ANY, _("Line Endings:"));
	wxStaticText* labelEnc = new wxStaticText(encodingPage, wxID_ANY, _("Encoding:"));
	m_defLine = new wxComboBox(encodingPage, CTRL_LINEENDING, wxEmptyString, wxDefaultPosition, wxDefaultSize, 0, NULL, wxCB_DROPDOWN|wxCB_READONLY);
	m_defEnc = new wxComboBox(encodingPage, CTRL_ENCODING, wxEmptyString, wxDefaultPosition, wxDefaultSize, 0, NULL, wxCB_DROPDOWN|wxCB_READONLY);
	m_defBom = new wxCheckBox(encodingPage, CTRL_BOM, _("Byte Order Marker"));

	wxBoxSizer* encSizer = new wxBoxSizer(wxVERTICAL);
	encSizer->Add(desc, 0, wxALL, 5);
	wxFlexGridSizer* combiSizer = new wxFlexGridSizer(2, 2, 5, 5);
		combiSizer->Add(labelLine);
		combiSizer->Add(m_defLine);
		combiSizer->Add(labelEnc);
		combiSizer->Add(m_defEnc);
		combiSizer->AddStretchSpacer(0);
		combiSizer->Add(m_defBom);
		encSizer->Add(combiSizer, 0, wxALL, 5);
	encodingPage->SetSizer(encSizer);

	UpdateEncoding();
	return encodingPage;
}

wxPanel* SettingsDlg::CreateProfilePage(wxWindow* parent) {
	wxPanel* profilePage = new wxPanel(parent, wxID_ANY);
	wxFlexGridSizer* profileSizer = new wxFlexGridSizer(2, 2, 0, 0);
	profileSizer->AddGrowableCol(1); // col 2 is sizable

	wxStaticText* labelName = new wxStaticText(profilePage, wxID_ANY, _("Name:"));
	profileSizer->Add(labelName, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5);

	cxLOCK_READ(m_catalyst)
		m_ctrlUserName = new wxTextCtrl(profilePage, wxID_ANY, catalyst.GetUserName(0));
		profileSizer->Add(m_ctrlUserName, 1, wxEXPAND|wxALL, 5);

		wxStaticText* labelPic = new wxStaticText(profilePage, wxID_ANY, _("Picture:"));
		profileSizer->Add(labelPic, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5);

		wxBoxSizer* picSizer = new wxBoxSizer(wxHORIZONTAL);
		m_ctrlUserPic = new wxStaticBitmap(profilePage, wxID_ANY, catalyst.GetUserPic(0));
		m_ctrlUserPic->SetBackgroundColour(*wxWHITE);
		picSizer->Add(m_ctrlUserPic, 0);

		wxButton* loadButton = new wxButton(profilePage, CTRL_LOADPIC, _("Load..."));
		picSizer->Add(loadButton, 0, wxALIGN_CENTER_VERTICAL|wxLEFT, 15);

		profileSizer->Add(picSizer, 0, wxALL, 5);
	cxENDLOCK

	profilePage->SetSizerAndFit(profileSizer);
	return profilePage;
}

#ifdef __WXMSW__
wxPanel* SettingsDlg::CreateUnixPage(wxWindow* parent) {
	wxPanel* unixPage = new wxPanel(parent, wxID_ANY);

	wxFlexGridSizer* sizer = new wxFlexGridSizer(4, 2, 0, 0);
	sizer->AddGrowableCol(1); // 2nd column is sizable

	// Is Cygwin intialized?
	wxStaticText* labelCygInit = new wxStaticText(unixPage, wxID_ANY, _("Cygwin initialized?"));
	sizer->Add(labelCygInit, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5);

	m_labelCygInitValue = new wxStaticText(unixPage, wxID_ANY, _(""));
	sizer->Add(m_labelCygInitValue, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5);

	// Bash path
	wxStaticText* labelBashPath = new wxStaticText(unixPage, wxID_ANY, _("Bash path:"));
	sizer->Add(labelBashPath, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5);

	m_labelBashPathValue = new wxStaticText(unixPage, wxID_ANY, 	_(""));
	sizer->Add(m_labelBashPathValue, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5);

	// Cygdrive path
	wxStaticText* labelCygdrive = new wxStaticText(unixPage, wxID_ANY, _("Cygdrive prefix:"));
	sizer->Add(labelCygdrive, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5);

	m_labelCygdriveValue = new wxStaticText(unixPage, wxID_ANY, _(""));
	sizer->Add(m_labelCygdriveValue, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5);

	// Button
	sizer->AddStretchSpacer();

	m_cygwinButton = new wxButton(unixPage, CTRL_CYGWIN_ACTION, _(""));
	sizer->Add(m_cygwinButton, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5);

	unixPage->SetSizerAndFit(sizer);
	return unixPage;
}

void SettingsDlg::UpdateUnixPage() {
	const bool cygwin_initialized = eDocumentPath::IsInitialized();

	m_labelCygInitValue->SetLabel(cygwin_initialized
		? _("Yes") : _("No"));

	m_labelBashPathValue->SetLabel(cygwin_initialized
		? eDocumentPath::CygwinPath() + wxT("\\bin\\bash.exe")
		: _("(Cygwin not initialized)"));

	m_labelCygdriveValue->SetLabel(cygwin_initialized
		? eDocumentPath::CygdrivePrefix()
		: _("(Cygwin not initialized)"));

	m_cygwinButton->SetLabel(cygwin_initialized
		? _("Show version information")
		: _("Initialize now"));

	m_unixPage->Fit();
}
#endif

void SettingsDlg::UpdateEncoding() {
	// Build list of line endings
	m_defLine->Append(_("DOS/Windows (CR LF)"));
	m_defLine->Append(_("Unix (LF)"));
	m_defLine->Append(_("Mac (CR)"));

	// Build list of encodings
	const size_t enc_count = wxFontMapper::GetSupportedEncodingsCount();
	for (size_t enc_id = 0; enc_id < enc_count; ++enc_id) {
		const wxFontEncoding enc = wxFontMapper::GetEncoding(enc_id);
		wxString enc_name = wxFontMapper::GetEncodingDescription(enc);
		if (enc == wxFONTENCODING_DEFAULT) {
			enc_name += wxT(" (") + wxFontMapper::GetEncodingName(wxLocale::GetSystemEncoding()).Lower() + wxT(")");
		}
		m_defEnc->Append(enc_name);
	}

	// Default values
	wxTextFileType eol = wxTextBuffer::typeDefault;
	wxFontEncoding enc = wxFONTENCODING_DEFAULT;
	bool bom = false; // default

	// Get saved settings
	wxString eolStr;
	wxString encStr;
	m_settings.GetSettingString(wxT("formatEol"), eolStr);
	m_settings.GetSettingString(wxT("formatEncoding"), encStr);
	m_settings.GetSettingBool(wxT("formatBom"), bom);
	if (eolStr == wxT("crlf")) eol = wxTextFileType_Dos;
	else if (eolStr == wxT("lf")) eol = wxTextFileType_Unix;
	else if (eolStr == wxT("cr")) eol = wxTextFileType_Mac;
	if (!encStr.empty()) enc = wxFontMapper::GetEncodingFromName(encStr);

	// Set eol ctrl
	if (eol == wxTextFileType_Dos) m_defLine->SetSelection(0);
	else if (eol == wxTextFileType_Unix) m_defLine->SetSelection(1);
	else if (eol == wxTextFileType_Mac) m_defLine->SetSelection(2);

	// Set encoding ctrl
	for(unsigned int i = 0; i < m_defEnc->GetCount(); ++i) {
		if (wxFontMapper::GetEncoding(i) == enc) {
			m_defEnc->SetSelection(i);
			break;
		}
	}

	m_defBom->Enable(encoding_allows_bom(enc));
	m_defBom->SetValue(bom);
}

void SettingsDlg::OnComboEol(wxCommandEvent& event) {
	wxString eolStr;
	switch(event.GetSelection()) {
		case 0: eolStr = wxT("crlf"); break;
		case 1: eolStr = wxT("lf"); break;
		case 2: eolStr = wxT("cr"); break;
	}

	m_settings.SetSettingString(wxT("formatEol"), eolStr);
}

void SettingsDlg::OnComboEncoding(wxCommandEvent& event) {
	const wxFontEncoding enc = wxFontMapper::GetEncoding(event.GetSelection());
	const wxString encStr = wxFontMapper::GetEncodingName(enc);

	if (enc == wxFONTENCODING_DEFAULT) m_settings.RemoveSetting(wxT("formatEncoding"));
	else m_settings.SetSettingString(wxT("formatEncoding"), encStr);


	// Check if bom ctrl should be enabled
	m_defBom->Enable(encoding_allows_bom(enc));
}

void SettingsDlg::OnCheckBom(wxCommandEvent& event) {
	m_settings.SetSettingBool(wxT("formatBom"), event.IsChecked());
}

void SettingsDlg::OnButtonOk(wxCommandEvent& WXUNUSED(event)) {
	// Check if name & profile pic changed.
	wxString name = m_ctrlUserName->GetLabel();
	cxLOCK_READ(m_catalyst)
		if (name == catalyst.GetUserName(0)) name.clear(); // empty string means no change
	cxENDLOCK

	if (!name.empty() || m_ctUserPic) {
		if (!m_ctUserPic) m_userImage = wxImage(); // make invalid

		cxLOCK_WRITE(m_catalyst)
			catalyst.SetProfile(name, m_userImage);
		cxENDLOCK
	}

	if (m_envPage->VarsChanged()) {
		m_settings.env.clear();
		m_envPage->GetVars(m_settings.env);
	}

	EndModal(wxID_OK);
}

void SettingsDlg::OnButtonCheckForUpdates(wxCommandEvent& WXUNUSED(event)) {
	CheckForUpdates(m_settings, GetAppVersion(), true);
}

void SettingsDlg::OnButtonCygwinAction(wxCommandEvent& WXUNUSED(event)) {
#ifdef __WXMSW__
	if (eDocumentPath::IsInitialized()){
		const wxString command(wxT("uname -a"));

		cxEnv env;
		const vector<char> cmd(command.begin(), command.end());
		const vector<char> input;
		long cmd_err;
		vector<char> output;
		{
			wxBusyCursor wait;
			cmd_err = ShellRunner::RawShell(cmd, input, &output, NULL, env);
		}

		wxString cmd_out;
		if (!output.empty())
			cmd_out = wxString(&*output.begin(), wxConvUTF8, output.size());
		else {
			cmd_out = wxT("Version information could not be retrieved.");
		}

		::wxMessageBox(cmd_out, wxT("Cygwin Version"));
	}
	else {
		eDocumentPath::InitCygwin();
		UpdateUnixPage();
	}
#endif
}

void SettingsDlg::OnButtonLoadPic(wxCommandEvent& WXUNUSED(event)) {
	const wxString filter = wxT("Image files (*.bmp,*.gif.*.ico,*.jpg,*.png)|*.bmp;*.gif;*.png;*.jpg;*.ico");
	wxFileDialog dlg(this, _("Choose an image"), wxT(""), wxT(""), filter, wxFD_OPEN);

	if (dlg.ShowModal() != wxID_OK) return;

	// Load the image
	m_userImage.LoadFile(dlg.GetPath());
	if (!m_userImage.Ok()){
		wxMessageBox(wxT("The selected image could not be opened, sorry."), wxT("Invalid image file"),
			wxOK | wxICON_ERROR, this);
		return;
	}

	// Resize to 48*48
	if (m_userImage.GetWidth() != 48 || m_userImage.GetHeight() != 48) {
		m_userImage.Rescale(48, 48);
	}

	m_ctrlUserPic->SetBitmap(wxBitmap(m_userImage));
	m_ctUserPic = true;
}

void SettingsDlg::OnCheckAutoPair(wxCommandEvent& event) {
	m_settings.SetSettingBool(wxT("autoPair"), event.IsChecked());

	// Notify that the settings have changed
	Dispatcher& dispatcher = m_catalyst.GetDispatcher();
	dispatcher.Notify(wxT("SETTINGS_CHANGED"), NULL, 0);
}

void SettingsDlg::OnCheckAutoWrap(wxCommandEvent& event) {
	m_settings.SetSettingBool(wxT("autoWrap"), event.IsChecked());

	// Notify that the settings have changed
	Dispatcher& dispatcher = m_catalyst.GetDispatcher();
	dispatcher.Notify(wxT("SETTINGS_CHANGED"), NULL, 0);
}

void SettingsDlg::OnCheckShowMargin(wxCommandEvent& event) {
	const bool doShowMargin = event.IsChecked();
	m_settings.SetSettingBool(wxT("showMargin"), doShowMargin);

	m_marginSpin->Enable(doShowMargin);
	m_wrapMargin->Enable(doShowMargin);

	// Notify that the settings have changed
	Dispatcher& dispatcher = m_catalyst.GetDispatcher();
	dispatcher.Notify(wxT("SETTINGS_CHANGED"), NULL, 0);
}

void SettingsDlg::OnCheckWrapMargin(wxCommandEvent& event) {
	const bool doWrapMargin = event.IsChecked();
	m_settings.SetSettingBool(wxT("wrapMargin"), doWrapMargin);

	// Notify that the settings have changed
	Dispatcher& dispatcher = m_catalyst.GetDispatcher();
	dispatcher.Notify(wxT("SETTINGS_CHANGED"), NULL, 0);
}

void SettingsDlg::OnMarginSpin(wxSpinEvent& event) {
	const int marginChars = event.GetPosition();

	m_settings.SetSettingInt(wxT("marginChars"), marginChars);

	// Notify that the settings have changed
	Dispatcher& dispatcher = m_catalyst.GetDispatcher();
	dispatcher.Notify(wxT("SETTINGS_CHANGED"), NULL, 0);
}


void SettingsDlg::OnCheckKeepState(wxCommandEvent& event) {
	m_settings.SetSettingBool(wxT("keepState"), event.IsChecked());
}

void SettingsDlg::OnCheckCheckChange(wxCommandEvent& event) {
	m_settings.SetSettingBool(wxT("checkChange"), event.IsChecked());
}

void SettingsDlg::OnCheckCheckForUpdates(wxCommandEvent& event) {
	m_settings.SetSettingBool(wxT("checkForUpdates"), event.IsChecked());
}

void SettingsDlg::OnCheckAtomicSave(wxCommandEvent& event) {
	m_settings.SetSettingBool(wxT("disable_atomic_save"), !event.IsChecked());
}

void SettingsDlg::OnCheckLastTab(wxCommandEvent& event) {
	m_settings.SetSettingBool(wxT("gotoLastTab"), event.IsChecked());
}

void SettingsDlg::OnCheckHighlightVariables(wxCommandEvent& event) {
	m_settings.SetSettingBool(wxT("highlightVariables"), event.IsChecked());
}


void SettingsDlg::OnCheckHighlightHtml(wxCommandEvent& event) {
	m_settings.SetSettingBool(wxT("highlightHtml"), event.IsChecked());
}
