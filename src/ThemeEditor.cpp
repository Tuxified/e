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

#include "ThemeEditor.h"
#include <wx/grid.h>
#include <wx/fontdlg.h>
#include <wx/colordlg.h>
#include <wx/clipbrd.h>

#include "Strings.h"
#include "ITmThemeHandler.h"
#include "Colours.h"

void SetClipboardText(const wxString& text) {
	if (wxTheClipboard->Open())
	{
		wxTheClipboard->SetData( new wxTextDataObject(text) );
		wxTheClipboard->Close();
	}
}

wxString GetClipboardText() {
	wxString value = wxEmptyString;

	if (wxTheClipboard->Open()) {
		if (wxTheClipboard->IsSupported( wxDF_TEXT )) {
			wxTextDataObject data;
			if (wxTheClipboard->GetData(data)) {
				value = data.GetText();
			}
		}
		wxTheClipboard->Close();
	}

	return value;
}

class ColourCellRenderer : public wxGridCellRenderer {
public:
	ColourCellRenderer(const PListDict& themeDict) : m_themeDict(themeDict) {};
	void Draw(wxGrid& grid, wxGridCellAttr& attr, wxDC& dc, const wxRect& rect, int row, int col, bool isSelected);
	wxSize GetBestSize(wxGrid& grid, wxGridCellAttr& attr, wxDC& dc, int row, int col);
	wxGridCellRenderer* Clone() const;
private:
	const PListDict& m_themeDict;
};


enum {
	CTRL_FONTSELECT,
	CTRL_FONTQUALITY,
	CTRL_THEMELIST,
	CTRL_FGBUTTON,
	CTRL_BGBUTTON,
	CTRL_SELBUTTON,
	CTRL_INVBUTTON,
	CTRL_LINEBUTTON,
	CTRL_CARETBUTTON,
	CTRL_GUTTERBUTTON,
	CTRL_SEARCHBUTTON,
	CTRL_MULTIBUTTON,
	CTRL_BRACKETBUTTON,
	CTRL_NEWSETTING,
	CTRL_DELSETTING,
	CTRL_NEWTHEME,
	CTRL_DELTHEME,

	MENU_COPY_COLOUR,
	MENU_PASTE_COLOUR
};

BEGIN_EVENT_TABLE(ThemeEditor, wxDialog)
	EVT_BUTTON(CTRL_NEWTHEME, ThemeEditor::OnNewTheme)
	EVT_BUTTON(CTRL_DELTHEME, ThemeEditor::OnDelTheme)
	EVT_BUTTON(CTRL_FGBUTTON, ThemeEditor::OnButtonFg)
	EVT_BUTTON(CTRL_BGBUTTON, ThemeEditor::OnButtonBg)
	EVT_BUTTON(CTRL_SELBUTTON, ThemeEditor::OnButtonSel)
	EVT_BUTTON(CTRL_INVBUTTON, ThemeEditor::OnButtonInv)
	EVT_BUTTON(CTRL_LINEBUTTON, ThemeEditor::OnButtonLine)
	//EVT_BUTTON(CTRL_CARETBUTTON, ThemeEditor::OnButtonCaret)
	EVT_BUTTON(CTRL_SEARCHBUTTON, ThemeEditor::OnButtonSearchHL)
	EVT_BUTTON(CTRL_BRACKETBUTTON, ThemeEditor::OnButtonBracketHL)
	EVT_BUTTON(CTRL_GUTTERBUTTON, ThemeEditor::OnButtonGutter)
	EVT_BUTTON(CTRL_MULTIBUTTON, ThemeEditor::OnButtonMulti)
	EVT_BUTTON(CTRL_NEWSETTING, ThemeEditor::OnNewSetting)
	EVT_BUTTON(CTRL_DELSETTING, ThemeEditor::OnDelSetting)
	EVT_BUTTON(CTRL_FONTSELECT, ThemeEditor::OnFontSelect)

	EVT_LISTBOX(CTRL_THEMELIST, ThemeEditor::OnThemeSelected)

	EVT_SIZE(ThemeEditor::OnSize)

	EVT_GRID_SELECT_CELL(ThemeEditor::OnGridSelect)
	EVT_GRID_CELL_LEFT_DCLICK(ThemeEditor::OnGridLeftDClick)
	EVT_GRID_CELL_CHANGE(ThemeEditor::OnGridCellChange)
	EVT_GRID_CELL_RIGHT_CLICK(ThemeEditor::OnGridRightClick)

	EVT_COMBOBOX(CTRL_FONTQUALITY, ThemeEditor::OnFontQuality)

	EVT_MENU(MENU_COPY_COLOUR, ThemeEditor::OnCopyColour)
	EVT_MENU(MENU_PASTE_COLOUR, ThemeEditor::OnPasteColour)

END_EVENT_TABLE()

ThemeEditor::ThemeEditor(wxWindow *parent, ITmThemeHandler& syntaxHandler):
	wxDialog (parent, -1, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxDEFAULT_DIALOG_STYLE|wxRESIZE_BORDER),
	m_syntaxHandler(syntaxHandler), m_plistHandler(m_syntaxHandler.GetPListHandler()),
	m_themeNdx(-1), m_currentRow(-1)
{
	SetTitle (_("Edit Themes"));

	// Create the controls

	// Theme list
	m_themeList = new wxListBox(this, CTRL_THEMELIST);
	m_themeList->SetMinSize(wxSize(50,50)); // ensure resizeability
	m_themePlus = new wxButton(this, CTRL_NEWTHEME, wxT("+"));
	m_themeMinus = new wxButton(this, CTRL_DELTHEME, wxT("-"));

	// Add the themes (they are sorted alphabetically)
	m_plistHandler.GetThemes(m_themes);
	for (unsigned int i = 0; i < m_themes.size(); ++i) {
		m_themeList->Append(m_themes[i].name);
	}

	// General settings
	wxStaticText* fgLabel = new wxStaticText(this, wxID_ANY, _("Foreground:"));
	m_fgButton = new ColourButton(this, CTRL_FGBUTTON);
	wxStaticText* bgLabel = new wxStaticText(this, wxID_ANY, _("Background:"));
	m_bgButton = new ColourButton(this, CTRL_BGBUTTON);
	wxStaticText* selLabel = new wxStaticText(this, wxID_ANY, _("Selection:"));
	m_selButton = new ColourButton(this, CTRL_SELBUTTON);
	wxStaticText* invLabel = new wxStaticText(this, wxID_ANY, _("Invisibles:"));
	m_invButton = new ColourButton(this, CTRL_INVBUTTON);
	wxStaticText* lineLabel = new wxStaticText(this, wxID_ANY, _("Line Highlight:"));
	m_lineButton = new ColourButton(this, CTRL_LINEBUTTON);
	//wxStaticText* caretLabel = new wxStaticText(this, wxID_ANY, _("Caret:"));
	//m_caretButton = new ColourButton(this, CTRL_CARETBUTTON);
	wxStaticText* gutterLabel = new wxStaticText(this, wxID_ANY, _("Gutter:"));
	m_gutterButton = new ColourButton(this, CTRL_GUTTERBUTTON);
	wxStaticText* searchLabel = new wxStaticText(this, wxID_ANY, _("Search Highlight:"));
	m_searchButton = new ColourButton(this, CTRL_SEARCHBUTTON);
	wxStaticText* multiLabel = new wxStaticText(this, wxID_ANY, _("Multiedit Highlight:"));
	m_multiButton = new ColourButton(this, CTRL_MULTIBUTTON);
	wxStaticText* bracketLabel = new wxStaticText(this, wxID_ANY, _("Bracket Highlight:"));
	m_bracketButton = new ColourButton(this, CTRL_BRACKETBUTTON);

	// Set tooltips
	const wxString colorTip = _("Click to change color (shift-click for transparency).");
	m_fgButton->SetToolTip(colorTip);
	m_bgButton->SetToolTip(colorTip);
	m_selButton->SetToolTip(colorTip);
	m_invButton->SetToolTip(colorTip);
	m_lineButton->SetToolTip(colorTip);
	m_gutterButton->SetToolTip(colorTip);
	m_searchButton->SetToolTip(colorTip);
	m_multiButton->SetToolTip(colorTip);
	m_bracketButton->SetToolTip(colorTip);

	// Grid
	m_grid = new wxGrid(this, wxID_ANY);
	m_grid->SetMargins(0,0);
	m_grid->SetRowLabelSize(1);
	m_grid->EnableGridLines(false);
	m_grid->EnableDragColSize(false);
	m_grid->EnableDragRowSize(false);
	m_grid->EnableDragGridSize(false);
	m_grid->CreateGrid(0, 6);
	m_grid->SetSelectionMode(wxGrid::wxGridSelectRows);
	m_grid->SetColFormatBool(3);
	m_grid->SetColFormatBool(4);
	m_grid->SetColFormatBool(5);
	m_grid->SetColLabelValue(0, _("Elements"));
	m_grid->SetColLabelValue(1, _("FG"));
	m_grid->SetColLabelValue(2, _("BG"));
	m_grid->SetColLabelValue(3, _("B"));
	m_grid->SetColLabelValue(4, _("I"));
	m_grid->SetColLabelValue(5, _("U"));
	m_grid->SetMinSize(wxSize(100,100)); // ensure resizeability

	m_gridPlus = new wxButton(this, CTRL_NEWSETTING, wxT("+"));
	m_gridMinus = new wxButton(this, CTRL_DELSETTING, wxT("-"));
	wxStaticText* selectorLabel = new wxStaticText(this, wxID_ANY, _("Selector:"));
	m_selectorCtrl = new FocusTextCtrl(*this, wxID_ANY);

	// Font
	wxStaticText* fontLabel = new wxStaticText(this, wxID_ANY, _("Font:"));
	m_fontDesc = new wxTextCtrl(this, wxID_ANY, wxT("font"), wxDefaultPosition, wxDefaultSize, wxTE_READONLY);
	wxButton *fontSelect = new wxButton(this, CTRL_FONTSELECT, _("Select..."));
#ifdef __WXMSW__
	const wxString choices[] = {_("Default"), _("Not Antialiased"), _("Antialiased"), _("Cleartype")};
	m_qualityCombo = new wxComboBox(this, CTRL_FONTQUALITY, wxT(""), wxDefaultPosition, wxDefaultSize, 4, choices, wxCB_READONLY);
#endif

	// Set the font description
	const wxFont& font = m_syntaxHandler.GetFont();
	const wxString desc = wxString::Format(wxT("%s, %dpt"), font.GetFaceName().c_str(), font.GetPointSize());
	m_fontDesc->SetValue(desc);

#ifdef __WXMSW__
	// We have to extract font quality info out of the native description
	const wxArrayString tokens = wxSplit(font.GetNativeFontInfoDesc(), _T(';'));
	long l = 0;
	if (tokens.size() > 12) tokens[12].ToLong(&l);
	switch (l) {
		case NONANTIALIASED_QUALITY: m_qualityCombo->SetSelection(1); break;
		case ANTIALIASED_QUALITY: m_qualityCombo->SetSelection(2); break;
		case CLEARTYPE_QUALITY: m_qualityCombo->SetSelection(3); break;
		default: m_qualityCombo->SetSelection(0);
	}
#endif

	// Create the layout
	wxSizer* mainSizer = new wxBoxSizer(wxVERTICAL);
		wxSizer* themeSizer = new wxBoxSizer(wxHORIZONTAL);
			wxSizer* themeListSizer = new wxBoxSizer(wxVERTICAL);
				themeListSizer->Add(m_themeList, 1, wxEXPAND|wxALL, 5);
				wxSizer* themeButtonSizer = new wxBoxSizer(wxHORIZONTAL);
					themeButtonSizer->Add(m_themePlus, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5);
					themeButtonSizer->Add(m_themeMinus, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5);
					themeListSizer->Add(themeButtonSizer, 0);
				themeSizer->Add(themeListSizer, 0, wxEXPAND);
			wxStaticBoxSizer* themeSettings = new wxStaticBoxSizer(wxVERTICAL, this, _("Theme Settings"));
				wxGridSizer* generalSizer = new wxGridSizer(3, 3);
					wxSizer* fgSizer = new wxBoxSizer(wxHORIZONTAL);
						fgSizer->Add(fgLabel, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5);
						fgSizer->Add(m_fgButton, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5);
						generalSizer->Add(fgSizer, 0, wxALIGN_RIGHT);
					wxSizer* bgSizer = new wxBoxSizer(wxHORIZONTAL);
						bgSizer->Add(bgLabel, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5);
						bgSizer->Add(m_bgButton, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5);
						generalSizer->Add(bgSizer, 0, wxALIGN_RIGHT);
					wxSizer* lineSizer = new wxBoxSizer(wxHORIZONTAL);
						lineSizer->Add(lineLabel, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5);
						lineSizer->Add(m_lineButton, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5);
						generalSizer->Add(lineSizer, 0, wxALIGN_RIGHT);
					wxSizer* invSizer = new wxBoxSizer(wxHORIZONTAL);
						invSizer->Add(invLabel, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5);
						invSizer->Add(m_invButton, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5);
						generalSizer->Add(invSizer, 0, wxALIGN_RIGHT);
					wxSizer* selSizer = new wxBoxSizer(wxHORIZONTAL);
						selSizer->Add(selLabel, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5);
						selSizer->Add(m_selButton, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5);
						generalSizer->Add(selSizer, 0, wxALIGN_RIGHT);
					wxSizer* searchSizer = new wxBoxSizer(wxHORIZONTAL);
						searchSizer->Add(searchLabel, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5);
						searchSizer->Add(m_searchButton, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5);
						generalSizer->Add(searchSizer, 0, wxALIGN_RIGHT);
					//wxSizer* caretSizer = new wxBoxSizer(wxHORIZONTAL);
					//	caretSizer->Add(caretLabel, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5);
					//	caretSizer->Add(m_caretButton, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5);
					//	generalSizer->Add(caretSizer, 0, wxALIGN_RIGHT);
					wxSizer* gutterSizer = new wxBoxSizer(wxHORIZONTAL);
						gutterSizer->Add(gutterLabel, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5);
						gutterSizer->Add(m_gutterButton, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5);
						generalSizer->Add(gutterSizer, 0, wxALIGN_RIGHT);
					wxSizer* multiSizer = new wxBoxSizer(wxHORIZONTAL);
						multiSizer->Add(multiLabel, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5);
						multiSizer->Add(m_multiButton, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5);
						generalSizer->Add(multiSizer, 0, wxALIGN_RIGHT);
					wxSizer* bracketSizer = new wxBoxSizer(wxHORIZONTAL);
						bracketSizer->Add(bracketLabel, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5);
						bracketSizer->Add(m_bracketButton, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5);
						generalSizer->Add(bracketSizer, 0, wxALIGN_RIGHT);
					themeSettings->Add(generalSizer, 0, wxALIGN_CENTER_HORIZONTAL);

				themeSettings->Add(m_grid, 1, wxEXPAND|wxALL, 5);
				wxSizer* selectorSizer = new wxBoxSizer(wxHORIZONTAL);
					selectorSizer->Add(m_gridPlus, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5);
					selectorSizer->Add(m_gridMinus, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5);
					selectorSizer->Add(selectorLabel, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5);
					selectorSizer->Add(m_selectorCtrl, 1, wxALIGN_CENTER_VERTICAL|wxALL, 5);
					themeSettings->Add(selectorSizer, 0, wxEXPAND);
			themeSizer->Add(themeSettings, 3, wxEXPAND|wxALL, 5);
			mainSizer->Add(themeSizer, 1, wxEXPAND);
		wxSizer* fontSizer = new wxBoxSizer(wxHORIZONTAL);
			fontSizer->Add(fontLabel, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5);
			fontSizer->Add(m_fontDesc, 1, wxALIGN_CENTER_VERTICAL);
#ifdef __WXMSW__
			fontSizer->Add(m_qualityCombo, 0, wxALIGN_CENTER_VERTICAL);
#endif
			fontSizer->Add(fontSelect, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5);
			mainSizer->Add(fontSizer, 0, wxEXPAND);

	// Select current theme
	const wxString& themeName = m_syntaxHandler.GetCurrentThemeName();
	for (unsigned int n = 0; n < m_themes.size(); ++n) {
		if (m_themes[n].name == themeName) {
			m_themeList->SetSelection(n);
			SetTheme(m_themes[n], true);
			break;
		}
	}

	SetSizerAndFit(mainSizer);
	SetSize(400, 600);
	SizeGrid();
	Centre();
	EnableCtrls();
}

ThemeEditor::~ThemeEditor() {
	m_plistHandler.Commit();
}

void ThemeEditor::Clear() {
	m_themeNdx = -1;
	m_currentRow = -1;

	m_fgButton->Clear();
	m_bgButton->Clear();
	m_selButton->Clear();
	m_invButton->Clear();
	m_lineButton->Clear();
	//m_caretButton->Clear();
	m_bracketButton->Clear();
	m_gutterButton->Clear();
	m_searchButton->Clear();
	m_multiButton->Clear();

	// Clear the grid
	if (m_grid->GetNumberRows()) {
		m_grid->DeleteRows(0, m_grid->GetNumberRows());
		m_grid->ForceRefresh();
	}

	EnableCtrls();
}

void ThemeEditor::EnableCtrls() {
	const bool enableAll = (m_themeNdx != -1);

	m_fgButton->Enable(enableAll);
	m_bgButton->Enable(enableAll);
	m_selButton->Enable(enableAll);
	m_invButton->Enable(enableAll);
	m_lineButton->Enable(enableAll);
	//m_caretButton->Enable(enableAll);
	m_bracketButton->Enable(enableAll);
	m_gutterButton->Enable(enableAll);
	m_searchButton->Enable(enableAll);
	m_multiButton->Enable(enableAll);

	m_grid->Enable(enableAll);
	m_gridPlus->Enable(enableAll);

	const bool enableSel = (enableAll && m_currentRow != -1);
	m_gridMinus->Enable(enableSel);
	m_selectorCtrl->Enable(enableSel);
	if (!enableSel) m_selectorCtrl->Clear();
}

void ThemeEditor::SetTheme(const PListHandler::cxItemRef& themeRef, bool init) {
	if (!init && (int)themeRef.ref == m_themeNdx) return;
	m_currentRow = -1;

	const unsigned int ndx = themeRef.ref;
	if (!m_plistHandler.GetTheme(ndx, m_themeDict)) return;
	m_themeNdx = ndx;

	// Clear the grid
	m_grid->BeginBatch();
	if (m_grid->GetNumberRows()) {
		m_grid->DeleteRows(0, m_grid->GetNumberRows());
	}

	// Load Settings
	PListArray settings;
	if (m_themeDict.GetArray("settings", settings) && settings.GetSize()) {
		wxColour colour;
		unsigned int alpha;

		wxColour themeBackground;
		wxColour themeForeground;

		// First entry is the general settings
		PListDict general;
		PListDict genSettings;
		if (settings.GetDict(0, general) && general.GetDict("settings", genSettings)) {
			const char* fg = genSettings.GetString("foreground");
			if (fg && ParseColourAlpha(fg, themeForeground, alpha)) m_fgButton->SetColour(themeForeground, alpha);

			const char* bg = genSettings.GetString("background");
			if (bg && ParseColourAlpha(bg, themeBackground, alpha)) m_bgButton->SetColour(themeBackground, alpha);

			const char* sel = genSettings.GetString("selection");
			if (sel && ParseColourAlpha(sel, colour, alpha)) m_selButton->SetColour(colour, alpha);

			const char* inv = genSettings.GetString("invisibles");
			if (inv && ParseColourAlpha(inv, colour, alpha)) m_invButton->SetColour(colour, alpha);

			const char* line = genSettings.GetString("lineHighlight");
			if (line && ParseColourAlpha(line, colour, alpha)) m_lineButton->SetColour(colour, alpha);

			//const char* caret = genSettings.GetString("caret");
			//if (caret && ParseColourAlpha(caret, colour, alpha)) m_caretButton->SetColour(colour, alpha);

			const char* gutter = genSettings.GetString("gutter");
			if (gutter)	{
				if (ParseColourAlpha(gutter, colour, alpha)) m_gutterButton->SetColour(colour, alpha);
			}
			else m_gutterButton->SetColour(wxColour(192, 192, 255), 0); // default for gutter is Pastel purple

			const char* search = genSettings.GetString("searchHighlight");
			if (search)	{
				if (ParseColourAlpha(search, colour, alpha)) m_searchButton->SetColour(colour, alpha);
			}
			else m_searchButton->SetColour(wxColour(wxT("Yellow")), 0); // default for searchHL is Yellow

			const char* multi = genSettings.GetString("multiEditHighlight");
			if (multi)	{
				if (ParseColourAlpha(multi, colour, alpha)) m_multiButton->SetColour(colour, alpha);
			}
			else m_multiButton->SetColour(wxColour(225, 225, 225), 0); // default for multiEdit is Grey

			const char* bracket = genSettings.GetString("bracketHighlight");
			if (bracket) {
				if (ParseColourAlpha(bracket, colour, alpha)) m_bracketButton->SetColour(colour, alpha);
			}
			else m_bracketButton->SetColour(wxColour(wxT("Yellow")), 0); // default for bracketHL is Yellow
		}


		wxFont font = m_grid->GetCellFont(0, 0);
		for (unsigned int i = 1; i < settings.GetSize(); ++i) {
			const int row = i-1;
			m_grid->AppendRows();
			m_grid->SetReadOnly(row, 1);
			m_grid->SetReadOnly(row, 2);
			m_grid->SetCellRenderer(row, 1, new ColourCellRenderer(m_themeDict));
			m_grid->SetCellRenderer(row, 2, new ColourCellRenderer(m_themeDict));
			m_grid->SetCellEditor(row, 3, new wxGridCellBoolEditor);
			m_grid->SetCellEditor(row, 4, new wxGridCellBoolEditor);
			m_grid->SetCellEditor(row, 5, new wxGridCellBoolEditor);

			PListDict set;
			if (settings.GetDict(i, set)) {
				const char* name = set.GetString("name");
				if (name) m_grid->SetCellValue(row, 0, wxString(name, wxConvUTF8));

				int fontflags = 0;
				PListDict fontSettings;
				if (set.GetDict("settings", fontSettings)) {
					const char* fg = fontSettings.GetString("foreground");
					if (fg && ParseColourAlpha(fg, colour, alpha)) {
						m_grid->SetCellTextColour(row, 0, colour);
						m_grid->SetCellValue(row, 1, wxString(fg, wxConvUTF8));
					}
					else { // Else use theme-wide foreground
						m_grid->SetCellTextColour(row, 0, themeForeground);
					}

					const char* bg = fontSettings.GetString("background");
					if (bg && ParseColourAlpha(bg, colour, alpha)) {
						m_grid->SetCellBackgroundColour(row, 0, colour);
						m_grid->SetCellValue(row, 2, wxString(bg, wxConvUTF8));
					}
					else { // Else use theme-wide background
						m_grid->SetCellBackgroundColour(row, 0, themeBackground);
					}

					const char* fontStyle = fontSettings.GetString("fontStyle");
					if (fontStyle) {
						if (strstr(fontStyle, "italic") != NULL) fontflags |= wxFONTFLAG_ITALIC;
						if (strstr(fontStyle, "bold") != NULL)fontflags |= wxFONTFLAG_BOLD;
						if (strstr(fontStyle, "underline") != NULL) fontflags |= wxFONTFLAG_UNDERLINED;
					}
				}

				if (fontflags & wxFONTFLAG_BOLD) {
					font.SetWeight(wxFONTWEIGHT_BOLD);
					m_grid->SetCellValue(row, 3, wxT("1"));
				}
				else font.SetWeight(wxFONTWEIGHT_NORMAL);
				if (fontflags & wxFONTFLAG_ITALIC) {
					font.SetStyle(wxFONTSTYLE_ITALIC);
					m_grid->SetCellValue(row, 4, wxT("1"));
				}
				else font.SetStyle(wxFONTSTYLE_NORMAL);
				if (fontflags & wxFONTFLAG_UNDERLINED) {
					font.SetUnderlined(true);
					m_grid->SetCellValue(row, 5, wxT("1"));
				}
				else font.SetUnderlined(false);
				m_grid->SetCellFont(row, 0, font);
			}
		}
	}
	m_grid->EndBatch();

	// Refresh dlg before setting theme (which might cause a delay)
	EnableCtrls();
	Refresh();
	Update();

	// Set the theme
	if (!init) {
		const char* uuid = m_themeDict.GetString("uuid");
		if (uuid) m_syntaxHandler.SetTheme(uuid);
	}
}

void ThemeEditor::OnFontSelect(wxCommandEvent& WXUNUSED(event)) {
	wxFontData fontData;
	fontData.SetAllowSymbols(false);
	fontData.EnableEffects(false);
	fontData.SetInitialFont(m_syntaxHandler.GetFont());

	wxFontDialog dlg(this, fontData);
	if (dlg.ShowModal() == wxID_OK) {
		const wxFontData& fd = dlg.GetFontData();
		wxFont font  = fd.GetChosenFont();

		const wxString desc = wxString::Format(wxT("%s, %dpt"), font.GetFaceName().c_str(), font.GetPointSize());
		m_fontDesc->SetValue(desc);

		// Strip styles
		font.SetWeight(wxFONTWEIGHT_NORMAL);
		font.SetStyle(wxFONTSTYLE_NORMAL);
		font.SetUnderlined(false);

		m_syntaxHandler.SetFont(font);
	}
}

void ThemeEditor::OnFontQuality(wxCommandEvent& event) {
#ifdef __WXMSW__
	int quality = DEFAULT_QUALITY;
	switch(event.GetSelection()) {
		case 1: quality = NONANTIALIASED_QUALITY; break;
		case 2: quality = ANTIALIASED_QUALITY; break;
		case 3: quality = CLEARTYPE_QUALITY; break;
		default: quality = DEFAULT_QUALITY;
	}

	// There is no command to set quality, so we have to manually insert
	// it into the font description
	wxFont font = m_syntaxHandler.GetFont();
	wxArrayString tokens = wxSplit(font.GetNativeFontInfoDesc(), _T(';'));
	tokens[12] = wxString::Format(wxT("%d"), quality);
	const wxString desc = wxJoin(tokens, wxT(';'));
	font.SetNativeFontInfo(desc);

	m_syntaxHandler.SetFont(font);
#endif
}

void ThemeEditor::OnThemeSelected(wxCommandEvent& event) {
	const int ndx = event.GetSelection();
	wxASSERT((unsigned int)ndx < m_themes.size());
	SetTheme(m_themes[ndx]);
}

void ThemeEditor::OnButtonFg(wxCommandEvent& WXUNUSED(event)) {
	wxColour colour = m_fgButton->GetColour();
	unsigned int alpha = m_fgButton->GetAlpha();

	if (AskForColour(colour, alpha)) {
		m_fgButton->SetColour(colour, alpha);
		SetSettingColour(0, "foreground", colour, alpha);
	}
}

void ThemeEditor::OnButtonBg(wxCommandEvent& WXUNUSED(event)) {
	wxColour colour = m_bgButton->GetColour();
	unsigned int alpha = m_bgButton->GetAlpha();

	if (AskForColour(colour, alpha)) {
		m_bgButton->SetColour(colour, alpha);
		SetSettingColour(0, "background", colour, alpha);
	}
}

void ThemeEditor::OnButtonSel(wxCommandEvent& WXUNUSED(event)) {
	wxColour colour = m_selButton->GetColour();
	unsigned int alpha = m_selButton->GetAlpha();

	if (AskForColour(colour, alpha)) {
		m_selButton->SetColour(colour, alpha);
		SetSettingColour(0, "selection", colour, alpha);
	}
}

void ThemeEditor::OnButtonInv(wxCommandEvent& WXUNUSED(event)) {
	wxColour colour = m_invButton->GetColour();
	unsigned int alpha = m_invButton->GetAlpha();

	if (AskForColour(colour, alpha)) {
		m_invButton->SetColour(colour, alpha);
		SetSettingColour(0, "invisibles", colour, alpha);
	}
}

void ThemeEditor::OnButtonLine(wxCommandEvent& WXUNUSED(event)) {
	wxColour colour = m_lineButton->GetColour();
	unsigned int alpha = m_lineButton->GetAlpha();

	if (AskForColour(colour, alpha)) {
		m_lineButton->SetColour(colour, alpha);
		SetSettingColour(0, "lineHighlight", colour, alpha);
	}
}
/*
void ThemeEditor::OnButtonCaret(wxCommandEvent& WXUNUSED(event)) {
	wxColour colour = m_caretButton->GetColour();
	unsigned int alpha = m_caretButton->GetAlpha();

	if (AskForColour(colour, alpha)) {
		m_caretButton->SetColour(colour, alpha);
		SetSettingColour(0, "caret", colour, alpha);
	}
}
*/
void ThemeEditor::OnButtonBracketHL(wxCommandEvent& WXUNUSED(event)) {
	wxColour colour = m_bracketButton->GetColour();
	unsigned int alpha = m_bracketButton->GetAlpha();

	if (AskForColour(colour, alpha)) {
		m_bracketButton->SetColour(colour, alpha);
		SetSettingColour(0, "bracketHighlight", colour, alpha);
	}
}

void ThemeEditor::OnButtonSearchHL(wxCommandEvent& WXUNUSED(event)) {
	wxColour colour = m_searchButton->GetColour();
	unsigned int alpha = m_searchButton->GetAlpha();

	if (AskForColour(colour, alpha)) {
		m_searchButton->SetColour(colour, alpha);
		SetSettingColour(0, "searchHighlight", colour, alpha);
	}
}

void ThemeEditor::OnButtonGutter(wxCommandEvent& WXUNUSED(event)) {
	wxColour colour = m_gutterButton->GetColour();
	unsigned int alpha = m_gutterButton->GetAlpha();

	if (AskForColour(colour, alpha)) {
		m_gutterButton->SetColour(colour, alpha);
		SetSettingColour(0, "gutter", colour, alpha);
	}
}

void ThemeEditor::OnButtonMulti(wxCommandEvent& WXUNUSED(event)) {
	wxColour colour = m_multiButton->GetColour();
	unsigned int alpha = m_multiButton->GetAlpha();

	if (AskForColour(colour, alpha)) {
		m_multiButton->SetColour(colour, alpha);
		SetSettingColour(0, "multiEditHighlight", colour, alpha);
	}
}

void ThemeEditor::OnNewSetting(wxCommandEvent& WXUNUSED(event)) {
	// Find the insert position
	const unsigned int gridPos = (m_currentRow != -1) ? m_currentRow+1 : m_grid->GetNumberRows();

	if (!m_plistHandler.IsThemeEditable(m_themeNdx)) {
		m_plistHandler.GetEditableTheme(m_themeNdx, m_themeDict);
	}

	// Add the new setting
	PListArray settings;
	if (m_themeDict.GetArray("settings", settings)) {
		wxASSERT(settings.GetSize() > 0);

		PListDict set = settings.InsertNewDict(gridPos+1);
		set.SetString("name", "New setting");
		PListDict fontSettings = set.NewDict("settings");
	}

	// Add entry to grid
	m_grid->InsertRows(gridPos);
	m_grid->SetCellValue(gridPos, 0, _("New Setting"));
	m_grid->SetReadOnly(gridPos, 1);
	m_grid->SetReadOnly(gridPos, 2);
	m_grid->SetCellRenderer(gridPos, 1, new ColourCellRenderer(m_themeDict));
	m_grid->SetCellRenderer(gridPos, 2, new ColourCellRenderer(m_themeDict));
	m_grid->SetCellEditor(gridPos, 3, new wxGridCellBoolEditor);
	m_grid->SetCellEditor(gridPos, 4, new wxGridCellBoolEditor);
	m_grid->SetCellEditor(gridPos, 5, new wxGridCellBoolEditor);
	m_grid->SelectRow(gridPos);
	m_grid->SetGridCursor(gridPos, 0);
	m_grid->MakeCellVisible(gridPos, 0);
	m_grid->ForceRefresh();

	EnableCtrls();
}

void ThemeEditor::OnDelSetting(wxCommandEvent& WXUNUSED(event)) {
	// WORKAROUND: GetSelectedRows() does not work
	if (m_currentRow == -1) return;

	// Make sure the theme is editable
	if (!m_plistHandler.IsThemeEditable(m_themeNdx)) {
		m_plistHandler.GetEditableTheme(m_themeNdx, m_themeDict);
	}

	PListArray settings;
	if (!m_themeDict.GetArray("settings", settings)) return;

	settings.DeleteItem(m_currentRow + 1);
	m_grid->DeleteRows(m_currentRow);
	m_currentRow = -1;
	EnableCtrls();

	m_grid->ForceRefresh();
	NotifyThemeChanged();
}

void ThemeEditor::OnNewTheme(wxCommandEvent& WXUNUSED(event)) {
	const wxString themeName = wxGetTextFromUser(_("Create new theme:"), _("New Theme"));
	if (themeName.empty()) return;

	const unsigned int themeNdx = m_plistHandler.NewTheme(themeName);

	m_themes.push_back(PListHandler::cxItemRef(themeName, themeNdx));
	const unsigned int listNdx = m_themeList->GetCount();
	m_themeList->Append(themeName);
	m_themeList->SetSelection(listNdx);

	SetTheme(m_themes.back());
}

void ThemeEditor::OnDelTheme(wxCommandEvent& WXUNUSED(event)) {
	wxArrayInt selections;
	if (m_themeList->GetSelections(selections) == 0) return;
	wxASSERT(selections.GetCount() == 1);

	const unsigned int ndx = selections[0];
	const wxString themeName = m_themeList->GetString(ndx);

	m_plistHandler.DeleteTheme(m_themes[ndx].ref);
	m_themeList->Delete(ndx);
	m_themes.erase(m_themes.begin()+ndx);

	if (m_themeList->GetCount() == 0) {
		Clear();
		m_syntaxHandler.SetDefaultTheme();
	}
	else {
		const unsigned int newThemeNdx = wxMin(ndx, m_themeList->GetCount()-1);
		m_themeList->SetSelection(newThemeNdx);
		SetTheme(m_themes[newThemeNdx]);
	}
}

bool ThemeEditor::AskForColour(wxColour& colour, unsigned int& alpha) {
	if (wxGetKeyState(WXK_SHIFT)) {
		TransparencyDlg dlg(this, alpha);
		if (dlg.ShowModal() == wxID_OK) {
			alpha = dlg.GetAlpha();
			return true;
		}
	}
	else {
		wxColourData cd;
		cd.SetChooseFull(true);
		cd.SetColour(colour);

		wxColourDialog dlg(this, &cd);
		dlg.CentreOnParent();
		if (dlg.ShowModal() == wxID_OK) {
			const wxColourData& cdat = dlg.GetColourData();
			colour = cdat.GetColour();
			return true;
		}
	}

	return false;
}

void ThemeEditor::SizeGrid() {
	if (m_grid) {
		const wxSize size = m_grid->GetClientSize();
		m_grid->SetColSize(0, size.x - (108 + 2));
		m_grid->SetColSize(1, 30);
		m_grid->SetColSize(2, 30);
		m_grid->SetColSize(3, 16);
		m_grid->SetColSize(4, 16);
		m_grid->SetColSize(5, 16);
	}
}

void ThemeEditor::OnSize(wxSizeEvent& event) {
	SizeGrid();
	event.Skip();
}

void ThemeEditor::OnGridSelect(wxGridEvent& event) {
	wxASSERT(m_themeDict.IsOk());
	m_currentRow = event.GetRow();
	const unsigned int ndx = event.GetRow()+1; // 0 is general settings

	// Allow default grid processing
	event.Skip();
	EnableCtrls();

	// Set the selector
	PListArray settings;
	if (m_themeDict.GetArray("settings", settings) && ndx < settings.GetSize()) {
		wxASSERT(ndx < settings.GetSize());

		PListDict set;
		if (settings.GetDict(ndx, set)) {
			const char* selector = set.GetString("scope");
			if (selector) {
				m_selectorCtrl->SetValue(wxString(selector, wxConvUTF8));
				return;
			}
		}
	}

	// If we get to here, there is no selector
	m_selectorCtrl->Clear();
}


void ThemeEditor::OnGridRightClick(wxGridEvent& evt) {
	// Only handle right click for colour cells
	if (evt.GetCol() != 1 && evt.GetCol() != 2) {
		evt.Skip();
		return;
	}

	const wxString colourStr = m_grid->GetCellValue(evt.GetRow(), evt.GetCol());
	wxColour colour;
	unsigned int alpha = 0;
	if (!colourStr.empty()) {
		ParseColourAlpha(colourStr.mb_str(wxConvUTF8), colour, alpha);
	}

	// See if there is a color on the clipboard.
	bool hasClipboardColour = false;
	wxColour clipboardColour;
	unsigned int clipboardAlpha = 0;

	const wxString clipboardColourStr = GetClipboardText();
	if (!clipboardColourStr.empty()) {
		hasClipboardColour = ParseColourAlpha(clipboardColourStr.mb_str(wxConvUTF8), clipboardColour, clipboardAlpha);
	}

	// Need to save the potential copy/paste colors, so the event handlers can use them later.
	if (hasClipboardColour) {
		this->copyColours = CopyColours(evt.GetRow(), evt.GetCol(), colour, alpha, clipboardColour, clipboardAlpha);
	} else {
		this->copyColours = CopyColours(evt.GetRow(), evt.GetCol(), colour, alpha);
	}

	// Create the tabs menu
	wxMenu colourMenu;
	colourMenu.Append(MENU_COPY_COLOUR, _("Copy color"));
	colourMenu.Append(MENU_PASTE_COLOUR, _("Paste color"));

	colourMenu.Enable(MENU_PASTE_COLOUR, hasClipboardColour);

	PopupMenu(&colourMenu);
}

void ThemeEditor::OnCopyColour(wxCommandEvent& WXUNUSED(event)) {
	if(!copyColours.hasCopyColour) return;
	wxString newcolour = WriteColourAlpha(copyColours.copyColour, copyColours.copyAlpha);
	SetClipboardText(newcolour);
}

void ThemeEditor::OnPasteColour(wxCommandEvent& WXUNUSED(event)) {
	if (!copyColours.hasPasteColour) return;
	SetColour(copyColours.pasteColour, copyColours.pasteAlpha, copyColours.row, copyColours.col);
}

void ThemeEditor::SetColour(const wxColour& colour, const unsigned int alpha, const int row, const int col) {
	// Update grid
	wxString newcolour = WriteColourAlpha(colour, alpha);
	if (!newcolour.empty()) m_grid->SetCellValue(row, col, newcolour);

	// Refresh dlg before setting theme (which might cause a delay)
	if (col == 1) m_grid->SetCellTextColour(row, 0, colour);
	else if (col == 2) m_grid->SetCellBackgroundColour(row, 0, colour);
	Refresh();
	Update();

	const unsigned int ndx = row + 1; // 0 is general settings

	if (col == 1) {
		SetSettingColour(ndx, "foreground", colour, alpha);
	}
	else if (col == 2) {
		SetSettingColour(ndx, "background", colour, alpha);
	}
}

void ThemeEditor::OnGridLeftDClick(wxGridEvent& event) {
	// Only handle doubleClick for colour cells
	if (event.GetCol() != 1 && event.GetCol() != 2) {
		event.Skip();
		return;
	}

	const wxString colourStr = m_grid->GetCellValue(event.GetRow(), event.GetCol());
	wxColour colour;
	unsigned int alpha = 0;
	if (!colourStr.empty()) {
		ParseColourAlpha(colourStr.mb_str(wxConvUTF8), colour, alpha);
	}

	if (AskForColour(colour, alpha) && colour.Ok()) {
		SetColour(colour, alpha, event.GetRow(), event.GetCol());
	}
}

void ThemeEditor::OnGridCellChange(wxGridEvent& event) {
	const int row = event.GetRow();
	const int col = event.GetCol();
	const unsigned int ndx = row + 1; // 0 is general settings

	if (col == 0) {
		// Name
		SetSelectorValue(ndx, "name", m_grid->GetCellValue(row, 0), false);
	}
	else if (col >= 3 && col <= 5) {
		// Font Style
		const bool bold = m_grid->GetCellValue(row, 3) == wxT("1") ? true : false;
		const bool italic = m_grid->GetCellValue(row, 4) == wxT("1") ? true : false;
		const bool underline = m_grid->GetCellValue(row, 5) == wxT("1") ? true : false;

		// Refresh grid before setting theme (which might cause a delay)
		wxFont font = m_grid->GetCellFont(row, 0);
		if (bold) font.SetWeight(wxFONTWEIGHT_BOLD);
		else font.SetWeight(wxFONTWEIGHT_NORMAL);
		if (italic) font.SetStyle(wxFONTSTYLE_ITALIC);
		else font.SetStyle(wxFONTSTYLE_NORMAL);
		font.SetUnderlined(underline);
		m_grid->SetCellFont(row, 0, font);
		m_grid->ForceRefresh();

		SetSettingFontStyle(ndx, bold, italic, underline);
	}
}

void ThemeEditor::OnSelectorKillFocus() {
	if (m_currentRow == -1) return;
	const unsigned int ndx = m_currentRow + 1; // 0 is general settings

	// Get current selector
	wxString selector;
	PListArray settings;
	if (m_themeDict.GetArray("settings", settings) && ndx < settings.GetSize()) {
		wxASSERT(ndx < settings.GetSize());

		PListDict set;
		if (settings.GetDict(ndx, set)) {
			const char* scope = set.GetString("scope");
			if (scope) {
				selector = wxString(scope, wxConvUTF8);
			}
		}
	}

	const wxString newText = m_selectorCtrl->GetValue();
	if (newText != selector) {
		SetSelectorValue(ndx, "scope", newText, true);
	}
}

void ThemeEditor::SetSettingColour(unsigned int ndx, const char* id, const wxColour& colour, unsigned int alpha) {
	wxASSERT(m_themeNdx != -1);

	if (!m_plistHandler.IsThemeEditable(m_themeNdx)) {
		m_plistHandler.GetEditableTheme(m_themeNdx, m_themeDict);
	}

	PListArray settings;
	if (m_themeDict.GetArray("settings", settings) && settings.GetSize()) {
		wxASSERT(ndx < settings.GetSize());

		PListDict set;
		PListDict fontSettings;
		if (settings.GetDict(ndx, set) && set.GetDict("settings", fontSettings)) {
			if (colour.Ok()) {
				wxString colourStr = WriteColourAlpha(colour, alpha);
				if (!colourStr.empty()) fontSettings.wxSetString(id, colourStr);
			}
			else {
				fontSettings.DeleteItem(id);
			}

			NotifyThemeChanged();
		}
		else wxASSERT(false);
	}
}

void ThemeEditor::SetSettingFontStyle(unsigned int ndx, bool bold, bool italic, bool underline) {
	wxASSERT(m_themeNdx > 0); // zero is general settings, which do not have fontStyle

	if (!m_plistHandler.IsThemeEditable(m_themeNdx)) {
		m_plistHandler.GetEditableTheme(m_themeNdx, m_themeDict);
	}

	PListArray settings;
	if (m_themeDict.GetArray("settings", settings) && settings.GetSize()) {
		wxASSERT(ndx < settings.GetSize());

		PListDict set;
		PListDict fontSettings;
		if (settings.GetDict(ndx, set) && set.GetDict("settings", fontSettings)) {
			if (!bold && !italic && !underline) {
				fontSettings.DeleteItem("fontStyle");
			}
			else {
				const char* boldStr = "bold";
				const char* italicStr = "italic";
				const char* underlineStr = "underline";

				// Build the font style
				std::vector<char> styleStr;
				if (bold) styleStr.insert(styleStr.begin(), boldStr, boldStr + strlen(boldStr));
				if (italic) {
					if (!styleStr.empty()) styleStr.push_back(' ');
					styleStr.insert(styleStr.begin(), italicStr, italicStr + strlen(italicStr));
				}
				if (underline) {
					if (!styleStr.empty()) styleStr.push_back(' ');
					styleStr.insert(styleStr.begin(), underlineStr, underlineStr + strlen(underlineStr));
				}
				styleStr.push_back('\0');

				fontSettings.SetString("fontStyle", &*styleStr.begin());
			}

			NotifyThemeChanged();
		}
		else wxASSERT(false);
	}
}

void ThemeEditor::SetSelectorValue(unsigned int ndx, const char* key, const wxString& value, bool reloadTheme) {
	wxASSERT(m_themeNdx > 0); // zero is general settings, which do not have scope

	if (!m_plistHandler.IsThemeEditable(m_themeNdx)) {
		m_plistHandler.GetEditableTheme(m_themeNdx, m_themeDict);
	}

	PListArray settings;
	if (m_themeDict.GetArray("settings", settings) && settings.GetSize()) {
		wxASSERT(ndx < settings.GetSize());

		PListDict set;
		PListDict fontSettings;
		if (settings.GetDict(ndx, set)) {
			if (value.empty()) fontSettings.DeleteItem(key);
			else set.SetString(key, value.mb_str(wxConvUTF8));

			if (reloadTheme) NotifyThemeChanged();
			else m_plistHandler.MarkThemeAsModified(m_themeNdx);
		}
		else wxASSERT(false);
	}
}

void ThemeEditor::NotifyThemeChanged() {
	m_plistHandler.MarkThemeAsModified(m_themeNdx);

	// Reload theme and notify editors
	const char* uuid = m_themeDict.GetString("uuid");
	m_syntaxHandler.SetTheme(uuid);
}

// ---- ColourButton ----------------------------------------------------------------

ThemeEditor::ColourButton::ColourButton(wxWindow* parent, wxWindowID id)
: wxBitmapButton(parent, id, wxNullBitmap) {
    Clear();
}

void ThemeEditor::ColourButton::Clear() {
	m_bgColour = wxNullColour;
	m_alpha = 0;
	DrawButton();
}

void ThemeEditor::ColourButton::SetColour(const wxColour& colour, unsigned int alpha) {
	m_bgColour = colour;
	m_alpha = alpha;
	DrawButton();
}

void ThemeEditor::ColourButton::SetColour(const wxColour& colour) {
	m_bgColour = colour;
	DrawButton();
}

void ThemeEditor::ColourButton::SetAlpha(unsigned int alpha) {
	m_alpha = alpha;
	DrawButton();
}

void ThemeEditor::ColourButton::DrawButton() {
    if (!m_bgColour.Ok())
        return;

	wxSize size = GetClientSize();

	// Make a margin
	size.x -= 5;
	size.y -= 5;
	wxBitmap bmp(size.x, size.y);

	// Create a memory DC
	wxMemoryDC mdc;
	mdc.SelectObject(bmp);
	mdc.SetBackground(m_bgColour);
	mdc.Clear();

	if (m_alpha > 0) {
		const wxColour alphaColour(m_alpha, m_alpha, m_alpha);

		// Draw alpha triangle
		wxPoint points[3];
		points[0] = wxPoint(0, size.y);
		points[1] = wxPoint(size.x, 0);
		points[2] = wxPoint(size.x, size.y);
		mdc.SetPen(alphaColour);
		mdc.SetBrush(alphaColour);
		mdc.DrawPolygon(3, points);
	}

	SetBitmapLabel(bmp);
}

// ---- TransparencyDlg ----------------------------------------------------------------


ThemeEditor::TransparencyDlg::TransparencyDlg(wxWindow *parent, unsigned int alpha):
	wxDialog (parent, -1, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxDEFAULT_DIALOG_STYLE|wxRESIZE_BORDER)
{
	SetTitle (_("Set Transparency"));

	wxSizer* mainSizer = new wxBoxSizer(wxVERTICAL);

	m_slider = new wxSlider(this, wxID_ANY, alpha, 0, 255, wxDefaultPosition, wxDefaultSize, wxSL_HORIZONTAL|wxSL_LABELS);
	mainSizer->Add(m_slider, 0, wxEXPAND|wxALL, 5);
	mainSizer->Add(CreateButtonSizer(wxOK|wxCANCEL), 0, wxEXPAND|wxALL, 5);

	SetSizerAndFit(mainSizer);
	Centre();
}

// ---- ColourCellRenderer ----------------------------------------------------------------

void ColourCellRenderer::Draw(wxGrid& grid, wxGridCellAttr& attr, wxDC& dc, const wxRect& rect, int row, int col, bool isSelected) {
	// Draw background first
	wxGridCellRenderer::Draw(grid, attr, dc, rect, row, col, isSelected);

	//const unsigned int ndx = row+1; // 0 is general settings
	const wxRect boxRect = rect.Deflate(3,3);

	// Get Colour and transparancy
	const wxString colourStr = grid.GetCellValue(row, col);
	wxColour colour;
	unsigned int alpha;
	if (!ParseColourAlpha(colourStr.mb_str(wxConvUTF8), colour, alpha)) return;

	dc.SetPen(*wxBLACK);
	dc.SetBrush(colour);

	// Draw colour box
	dc.DrawRectangle(boxRect);

	if (alpha) {
		const wxColour alphaColour(alpha, alpha, alpha);

		// Draw alpha triangle
		wxPoint points[3];
		points[0] = wxPoint(boxRect.x, boxRect.GetBottom());
		points[1] = wxPoint(boxRect.GetRight(), boxRect.y);
		points[2] = wxPoint(boxRect.GetRight(), boxRect.GetBottom() );
		dc.SetPen(alphaColour);
		dc.SetBrush(alphaColour);
		dc.DrawPolygon(3, points);
	}
}


wxSize ColourCellRenderer::GetBestSize(wxGrid& WXUNUSED(grid), wxGridCellAttr& WXUNUSED(attr), wxDC& WXUNUSED(dc), int WXUNUSED(row), int WXUNUSED(col)) {
	return wxSize(-1, -1);
}

wxGridCellRenderer* ColourCellRenderer::Clone() const {
	return new ColourCellRenderer(m_themeDict);
}

// ---- FocusTextCtrl ----------------------------------------------------------------

BEGIN_EVENT_TABLE(ThemeEditor::FocusTextCtrl, wxTextCtrl)
	EVT_KILL_FOCUS(ThemeEditor::FocusTextCtrl::OnKillFocus)
END_EVENT_TABLE()

ThemeEditor::FocusTextCtrl::FocusTextCtrl(ThemeEditor& parent, wxWindowID id):
	wxTextCtrl(&parent, id), m_parentDlg(parent) {}

void ThemeEditor::FocusTextCtrl::OnKillFocus(wxFocusEvent& event) {
	m_parentDlg.OnSelectorKillFocus();
	event.Skip();
}
