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

#include "GotoFileDlg.h"

#include <map>
#include <algorithm>

#include <wx/dir.h>

#include "ProjectInfoHandler.h"
#include "SearchListBox.h"

class FileEntry {
public:
	FileEntry() {};
	FileEntry(const wxString& dirpath, const wxString& filename);
	void SetPath(const wxString& path);
	void Clear();

	wxString name;
	wxString nameLower;
	wxString path;
};

class DirState {
public:
	static DirState* FromPath(const wxString& path) {
		DirState* dirState = new DirState(path);
		if (dirState->IsOpened()) return dirState;

		delete dirState;
		return NULL;
	}

	DirState(const wxString& path):
		walkingFiles(false),
		prefix(path + wxFILE_SEP_PATH)
	{
		dir.Open(path);
	}

	bool WalkFolders() {
		// Find next folder included by our filters.
		bool found = false, included = false;
		do {
			if (nextDirName.empty())
				found = dir.GetFirst(&nextDirName, wxEmptyString, wxDIR_DIRS);
			else
				found = dir.GetNext(&nextDirName);

			included = found && (!filter || filter->IsDirectoryIncluded(nextDirName));
		} while (found && !included);
		return found;
	}

	bool WalkFiles(wxString& file) {
		// Find next file included by our filters.
		bool found = false, included = false;
		do {
			if (walkingFiles) found = dir.GetNext(&file);
			else {
				found = dir.GetFirst(&file, wxEmptyString, wxDIR_FILES);
				walkingFiles = true;
			}

			included = found && (!filter || filter->IsFileIncluded(file));
		} while (found && !included);
		return found;
	}

	wxString FullFolderName() const { return prefix + nextDirName; }

	cxProjectInfo* info;
	const cxProjectInfo* filter;
	wxString prefix;
	wxString nextDirName;

private:
	bool IsOpened() const {return dir.IsOpened();}
	wxDir dir;
	bool walkingFiles;
};

class GotoFileList : public SearchListBox {
public:
	GotoFileList(wxWindow* parent, wxWindowID id, const std::vector<FileEntry*>& actions, const wxString& project_root=wxEmptyString);
	~GotoFileList();

	void Find(const wxString& text, const std::map<wxString,wxString>& triggers);
	const FileEntry* GetSelectedAction();

	void UpdateList(const bool reloadAll = false);

private:
	void OnDrawItem(wxDC& dc, const wxRect& rect, size_t n) const;
	int FindPath(const wxString& path) const;
	void AddFileIfMatching(const wxString& searchtext, FileEntry* file_entry);
	int FindMatchesAndSelection(const std::map<wxString,wxString>& triggers);

	class aItem {
	public:
		aItem() : file_entry(NULL), rank(0) {};
		aItem(const FileEntry* a, const std::vector<unsigned int>& hl);
		bool operator<(const aItem& ai) const;
		void swap(aItem& ai);

		const FileEntry* file_entry;
		std::vector<unsigned int> hlChars;
		unsigned int rank;
	};

	void iter_swap(std::vector<aItem>::iterator a, std::vector<aItem>::iterator b) {
		a->swap(*b);
	};

	const wxString m_project_root;
	const std::vector<FileEntry*>& m_actions;
	unsigned int m_actionCount;

	std::vector<aItem> m_items;
	wxString m_searchText;

	FileEntry* m_tempEntry;	// What is this used for, exactly?
};


// Ctrl id's
enum {
	CTRL_SEARCH,
	CTRL_ALIST
};

BEGIN_EVENT_TABLE(GotoFileDlg, wxDialog)
	EVT_TEXT(CTRL_SEARCH, GotoFileDlg::OnSearch)
	EVT_TEXT_ENTER(CTRL_SEARCH, GotoFileDlg::OnAction)
	EVT_LISTBOX_DCLICK(CTRL_ALIST, GotoFileDlg::OnAction)
	EVT_LISTBOX(CTRL_ALIST, GotoFileDlg::OnListSelection)
	EVT_SIZE(GotoFileDlg::OnSize)
	EVT_IDLE(GotoFileDlg::OnIdle)
END_EVENT_TABLE()

GotoFileDlg::GotoFileDlg(wxWindow *parent, ProjectInfoHandler& project):
	wxDialog (parent, -1, _("Go to File"), wxDefaultPosition, wxDefaultSize, wxDEFAULT_DIALOG_STYLE|wxRESIZE_BORDER),
	m_project(project), m_isDone(false), m_filesLoaded(false)
{
	// Create controls
	m_searchCtrl = new wxTextCtrl(this, CTRL_SEARCH, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxTE_PROCESS_ENTER);

	const wxString& project_root = m_project.GetRoot().GetPath();
	m_cmdList = new GotoFileList(this, CTRL_ALIST, m_files, project_root);
	m_pathStatic = new wxStaticText(this, wxID_ANY, wxEmptyString);

	// Add custom event handler
	m_searchCtrl->Connect(wxEVT_CHAR, wxKeyEventHandler(GotoFileDlg::OnSearchChar), NULL, this);

	// Create Layout
	wxBoxSizer *mainSizer = new wxBoxSizer(wxVERTICAL);
		mainSizer->Add(m_searchCtrl, 0, wxEXPAND);
		mainSizer->Add(m_cmdList, 1, wxEXPAND);
		mainSizer->Add(m_pathStatic, 0, wxEXPAND|wxALL, 5);

	SetSizer(mainSizer);
	SetSize(400, 500);
	Centre();
}

GotoFileDlg::~GotoFileDlg() {
	// Clean up allocated file entries
	for (std::vector<FileEntry*>::iterator p = m_files.begin(); p != m_files.end(); ++p) {
		delete *p;
	}

	// Clean up allocated dir traversing state
	for (std::vector<DirState*>::iterator d = m_dirStack.begin(); d != m_dirStack.end(); ++d) {
		delete *d;
	}
}

const wxString& GotoFileDlg::GetSelection() const {return m_cmdList->GetSelectedAction()->path;}
const wxString GotoFileDlg::GetTrigger() const {return m_searchCtrl->GetValue();}

void GotoFileDlg::OnIdle(wxIdleEvent& event) {
	if (!m_project.HasProject()) return; // No project, so no files to load.
	if (m_filesLoaded) return; // Already done loading files.

	if (m_dirStack.empty()) {
		const wxString root_path = m_project.GetRoot().GetPath();
		m_files.reserve(100);
		BuildFileList(root_path);
	}
	else {
		DirState& dirState = *m_dirStack.back();

		// Get next subdir
		if (dirState.WalkFolders()) {
			const wxString fulldirname = dirState.FullFolderName();
			wxLogDebug(fulldirname);
			BuildFileList(fulldirname);
		}
		else {
			// All sub-dirs visited
			if (dirState.info) {
				m_filters.pop_back();
				delete dirState.info;
			}

			delete m_dirStack.back();
			m_dirStack.pop_back();

			if (m_dirStack.empty())
				m_filesLoaded = true;
		}
	}

	m_cmdList->UpdateList();
	if (!m_dirStack.empty()) event.RequestMore();
}

void GotoFileDlg::BuildFileList(const wxString& path) {
	DirState* dirState = DirState::FromPath(path);
	if (!dirState) return;

	m_dirStack.push_back(dirState);

	// Load filters for this dir
	dirState->info = new cxProjectInfo;
	if (dirState->info->Load(m_project.GetRoot(), path, true)) {
		m_filters.push_back(dirState->info);
	}
	else {
		delete dirState->info;
		dirState->info = NULL;
	}

	// Set active filter
	dirState->filter = m_filters.empty() ? NULL : m_filters.back();

	// Get all files
	wxString eachFilename;
	while(dirState->WalkFiles(eachFilename))
		m_files.push_back(new FileEntry(dirState->prefix, eachFilename));
}

void GotoFileDlg::OnSearch(wxCommandEvent& event) {
	m_cmdList->Find(event.GetString(), m_project.GetTriggers());
	UpdateStatusbar();
}

void GotoFileDlg::OnAction(wxCommandEvent& WXUNUSED(event)) {
	if(m_cmdList->GetSelectedCount() == 1) {
		m_project.SetTrigger(m_searchCtrl->GetValue(), m_cmdList->GetSelectedAction()->path);
		m_isDone = true;
		EndModal(wxID_OK);
	}
}

void GotoFileDlg::OnSearchChar(wxKeyEvent& event) {
	switch ( event.GetKeyCode() )
    {
	case WXK_UP:
		m_cmdList->SelectPrev();
		UpdateStatusbar();
		return;
	case WXK_DOWN:
		m_cmdList->SelectNext();
		UpdateStatusbar();
		return;
	case WXK_ESCAPE:
		m_isDone = true;
		EndModal(wxID_CANCEL);
		return;
	}

	// no, we didn't process it
    event.Skip();
}

void GotoFileDlg::OnSize(wxSizeEvent& event) {
	UpdateStatusbar();
	event.Skip();
}

void GotoFileDlg::OnListSelection(wxCommandEvent& WXUNUSED(event)) {
	UpdateStatusbar();
}

void GotoFileDlg::UpdateStatusbar() {
	if(m_cmdList->GetSelectedCount() != 1) {
		m_pathStatic->SetLabel(wxEmptyString);
		return;
	}

	wxString path = GetSelection();

	// Calc extension width
	wxClientDC dc(this);
	dc.SetFont(m_pathStatic->GetFont());
	int w, h;
	static const wxString ext = wxT("...");
	dc.GetTextExtent(ext, &w, &h);
	const unsigned int extwidth = w;

	// See if we have to resize the path to fit
	unsigned int len = path.length();
	const unsigned int ctrlWidth = GetClientSize().x - 10;
	dc.GetTextExtent(path, &w, &h);
	if (len && w > (int)ctrlWidth) {
		do {
			path.erase(0, 1);
			dc.GetTextExtent(path, &w, &h);
		} while (--len > 0 && extwidth + w > ctrlWidth);
		path.Prepend(ext);
	}

	m_pathStatic->SetLabel(path);
}

// ---- FileEntry ----------------------------------------------------

FileEntry::FileEntry(const wxString& dirpath, const wxString& filename) {
	name = filename;
	nameLower = name.Lower();
	path = dirpath + filename;
}

void FileEntry::SetPath(const wxString& p) {
	wxFileName filepath(p);
	name = filepath.GetFullName();
	nameLower = name.Lower();
	path = p;
}

void FileEntry::Clear() {
	name.clear();
	nameLower.clear();
	path.clear();
}

// --- ActionList --------------------------------------------------------

GotoFileList::GotoFileList(wxWindow* parent, wxWindowID id, const std::vector<FileEntry*>& actions, const wxString& project_root):
	SearchListBox(parent, id),
	m_project_root(project_root),
	m_actions(actions), m_actionCount(0)
{
	m_tempEntry = new FileEntry();
	UpdateList();
}

GotoFileList::~GotoFileList(){
	if (m_tempEntry) delete m_tempEntry;
}

void GotoFileList::OnDrawItem(wxDC& dc, const wxRect& rect, size_t n) const {
	const bool isCurrent = IsCurrent(n);

	if (isCurrent) dc.SetTextForeground(m_hlTextColor);
	else dc.SetTextForeground(m_textColor);

	const FileEntry& file_entry = *m_items[n].file_entry;

	// m_project_root
	wxFileName displayPath(file_entry.path);
	displayPath.MakeRelativeTo(this->m_project_root);
	//displayPath.SetFullName(file_entry.name);

	const wxString& name = displayPath.GetFullPath();

	const int path_size = displayPath.GetPath().size() + 1;

	const std::vector<unsigned int>& hl = m_items[n].hlChars;
	std::vector<unsigned int> offsets(hl.size());
	for(unsigned int i = 0; i < hl.size(); i++) {
		offsets[i] = hl[i] + path_size;
	}

	/*// Calc extension width
	static const wxString ext = wxT("..  ");
	dc.SetFont(m_font);
	int w, h;
	dc.GetTextExtent(ext, &w, &h);
	const unsigned int extwidth = w;

	// See if we have to resize the action name to fit
	// note that this is not 100% correct as bold chars take up a bit more space.
	unsigned int len = name.length();
	dc.GetTextExtent(name, &w, &h);
	if (w > (int)rightBorder) {
		do {
			name.resize(--len);
			dc.GetTextExtent(name, &w, &h);
		} while (len > 0 && w + extwidth > (int)rightBorder);
		name += ext;
	}*/

	// Draw action name
	DrawItemText(dc, rect, name, offsets, isCurrent);
}

void GotoFileList::UpdateList(const bool reloadAll) {
	const FileEntry* selEntry = GetSelectedAction();
	const int topLine = GetFirstVisibleLine();
	int selection = -1;
	const unsigned int startItem = reloadAll ? 0 : m_items.size();

	// Insert new items
	if (m_searchText.empty()) {
		// Copy all actions to items
		m_items.resize(m_actions.size());
		for (unsigned int i = startItem; i < m_actions.size(); ++i) {
			m_items[i].file_entry = m_actions[i];
			m_items[i].hlChars.clear();
		}
	}
	else {
		// Copy matching actions to items
		for (unsigned int i = m_actionCount; i < m_actions.size(); ++i) {
			if (m_tempEntry->path.empty() || m_actions[i]->path != m_tempEntry->path)
				AddFileIfMatching(m_searchText, m_actions[i]);
		}
	}

	// Sort the items
	if (startItem) {
		sort(m_items.begin() + startItem, m_items.end());
		inplace_merge(m_items.begin(), m_items.begin() + startItem, m_items.end());
	}
	else sort(m_items.begin(), m_items.end());

	// Keep same selection
	if (selEntry) {
		for (unsigned int i = 0; i < m_items.size(); ++i) {
			if (m_items[i].file_entry == selEntry) {
				selection = i;
				break;
			}
		}
	}

	// Refresh and redraw listCtrl
	Freeze();
	SetItemCount(m_items.size());
	SetSelection(selection);

	if (selection == -1)
		ScrollToLine(topLine);
	else if (!IsVisible(selection))
		ScrollToLine(selection);

	RefreshAll();
	Thaw();

	m_actionCount = m_actions.size();
}

// Check if we have a matching trigger
int GotoFileList::FindMatchesAndSelection(const std::map<wxString,wxString>& triggers) {
	if (!m_items.size()) return wxNOT_FOUND;

	int selection = wxNOT_FOUND;
	std::map<wxString,wxString>::const_iterator p = triggers.find(m_searchText);
	if (p != triggers.end())
		selection = FindPath(p->second);

	if (selection != wxNOT_FOUND)
		return selection;

	// Check if we have a partial match, using saved triggers
	for (p = triggers.begin(); p != triggers.end(); ++p) {
		if (!p->first.StartsWith(m_searchText)) continue;

		selection = FindPath(p->second);
		if (selection != wxNOT_FOUND) continue;

		// Since we have a trigger but it is not in list yet
		// Let's check if it exists and add it temporarily
		if (wxFileExists(p->second)) {
			m_tempEntry->SetPath(p->second);

			// Add entry with highlighted search chars
			AddFileIfMatching(m_searchText, m_tempEntry);

			// Find position it will end up after sort
			std::vector<aItem>::iterator insPos = lower_bound(m_items.begin(), m_items.end()-1, m_items.back());
			selection = distance(m_items.begin(), insPos);

			// Move item to correct position
			if (m_items.size() > 1)
				inplace_merge(m_items.begin(), m_items.end()-1, m_items.end());

			return selection;
		}
	}

	return selection;
}

void GotoFileList::Find(const wxString& searchtext, const std::map<wxString,wxString>& triggers) {
	m_tempEntry->Clear();

	if (searchtext.empty()) {
		// Remove highlights
		m_searchText.clear();
		SetSelection(-1); // de-select
		ScrollToLine(0);
		UpdateList(true);
		return;
	}

	// Convert to lower case for case insensitive search
	m_searchText = searchtext.Lower();

	// Find all matching filenames
	m_items.clear();
	std::vector<unsigned int> hlChars;
	for (unsigned int i = 0; i < m_actions.size(); ++i)
		AddFileIfMatching(m_searchText, m_actions[i]);

	sort(m_items.begin(), m_items.end());

	// Check if we have a matching trigger
	int selection = FindMatchesAndSelection(triggers);

	// Update display
	Freeze();
	SetItemCount(m_items.size());

	if (m_items.empty())
		SetSelection(-1); // deselect
	else if (selection != wxNOT_FOUND)
		SetSelection(selection);
	else
		SetSelection(0);

	RefreshAll();
	Thaw();
}

void GotoFileList::AddFileIfMatching(const wxString& text, FileEntry* file_entry) {
	wxASSERT(!text.empty());

	const wxString& name = file_entry->nameLower;

	// The string positions of the characters to highlight. (Note, variable is static!)
	static std::vector<unsigned int> hlChars;
	hlChars.clear();

	unsigned int charpos = 0;
	wxChar c = text[charpos];
	for (unsigned int textpos = 0; textpos < name.size(); ++textpos) {
		if (name[textpos] != c) continue;

		hlChars.push_back(textpos);
		++charpos;
		if (charpos == text.size()) {
			// All chars found.
			m_items.push_back(aItem(file_entry, hlChars));
			break;
		}
		else c = text[charpos];
	}
}

int GotoFileList::FindPath(const wxString& path) const {
	for (unsigned int i = 0; i < m_items.size(); ++i)
		if (m_items[i].file_entry->path == path)
			return i;

	return wxNOT_FOUND;
}

const FileEntry* GotoFileList::GetSelectedAction() {
	const int sel = GetSelection();
	return (sel == -1) ? NULL : m_items[sel].file_entry;
}

// --- aItem --------------------------------------------------------

GotoFileList::aItem::aItem(const FileEntry* a, const std::vector<unsigned int>& hl):
	file_entry(a), hlChars(hl)
{
	// Calculate rank (total distance between chars)
	this->rank = 0;
	if (hlChars.size() <= 1) return;

	unsigned int prev = hlChars[0]+1;
	for (std::vector<unsigned int>::const_iterator p = hlChars.begin()+1; p != hlChars.end(); ++p) {
		this->rank += *p - prev;
		prev = *p+1;
	}
}

void GotoFileList::aItem::swap(aItem& ai) {
	const FileEntry* const tempEntry = file_entry;
	file_entry = ai.file_entry;
	ai.file_entry = tempEntry;

	const unsigned int tempRank = rank;
	rank = ai.rank;
	ai.rank = tempRank;

	hlChars.swap(ai.hlChars);
}

bool GotoFileList::aItem::operator < (const aItem& ai) const {
	if (rank < ai.rank) return true;
	if (rank > ai.rank) return false;
	if (file_entry && ai.file_entry) return (file_entry->name) < (ai.file_entry->name);
	return false;
}
