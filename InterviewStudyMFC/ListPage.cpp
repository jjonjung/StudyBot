#include "stdafx.h"
#include "ListPage.h"
#include "DBManager.h"

BEGIN_MESSAGE_MAP(CListPage, CDialogEx)
    ON_BN_CLICKED(IDC_BTN_TODAY, &CListPage::OnBnClickedToday)
    ON_NOTIFY(DTN_DATETIMECHANGE, IDC_DATE_PICKER, &CListPage::OnDtnDatetimechangePicker)
    ON_BN_CLICKED(IDC_BTN_DELETE, &CListPage::OnBnClickedDelete)
    ON_NOTIFY(NM_DBLCLK,  IDC_LIST_QUESTIONS, &CListPage::OnNMDblclkList)
    ON_NOTIFY(NM_RCLICK,  IDC_LIST_QUESTIONS, &CListPage::OnNMRClickList)
END_MESSAGE_MAP()

CListPage::CListPage(CWnd* pParent)
    : CDialogEx(IDD_LIST_PAGE, pParent) {}

void CListPage::DoDataExchange(CDataExchange* pDX) {
    CDialogEx::DoDataExchange(pDX);
    DDX_Control(pDX, IDC_DATE_PICKER,    m_dtPicker);
    DDX_Control(pDX, IDC_LIST_QUESTIONS, m_listCtrl);
}

BOOL CListPage::OnInitDialog() {
    CDialogEx::OnInitDialog();

    // 컨트롤 텍스트 (한글)
    SetDlgItemText(IDC_STATIC,    _T("날짜 :"));
    GetDlgItem(IDC_BTN_TODAY)->SetWindowText(_T("오늘"));
    GetDlgItem(IDC_BTN_DELETE)->SetWindowText(_T("선택 삭제"));

    SetupColumns();

    // 오늘 날짜로 초기화
    CTime today = CTime::GetCurrentTime();
    m_dtPicker.SetTime(&today);

    Refresh();
    return TRUE;
}

// ─── columns ──────────────────────────────────────────────────────────────────

void CListPage::SetupColumns() {
    m_listCtrl.SetExtendedStyle(LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);

    CRect rc;
    m_listCtrl.GetClientRect(&rc);
    int w = rc.Width();

    m_listCtrl.InsertColumn(0, _T("멤버"),      LVCFMT_LEFT,  (int)(w * 0.10));
    m_listCtrl.InsertColumn(1, _T("카테고리"),  LVCFMT_LEFT,  (int)(w * 0.12));
    m_listCtrl.InsertColumn(2, _T("질문 내용"), LVCFMT_LEFT,  (int)(w * 0.68));
    m_listCtrl.InsertColumn(3, _T("시간"),      LVCFMT_CENTER,(int)(w * 0.10));
}

// ─── date helper ─────────────────────────────────────────────────────────────

CString CListPage::GetPickedDate() const {
    CTime t;
    m_dtPicker.GetTime(t);
    CString s;
    s.Format(_T("%04d-%02d-%02d"), t.GetYear(), t.GetMonth(), t.GetDay());
    return s;
}

// ─── refresh list ────────────────────────────────────────────────────────────

void CListPage::Refresh() {
    m_listCtrl.DeleteAllItems();
    if (!DBManager::Get().IsConnected()) return;

    auto questions = DBManager::Get().GetQuestions(GetPickedDate());

    for (int i = 0; i < (int)questions.size(); ++i) {
        const auto& q = questions[i];
        int idx = m_listCtrl.InsertItem(i, q.member);
        m_listCtrl.SetItemText(idx, 1, q.category);
        m_listCtrl.SetItemText(idx, 2, q.question);
        m_listCtrl.SetItemText(idx, 3, q.createdAt);
        m_listCtrl.SetItemData(idx, (DWORD_PTR)q.id);
    }
}

// ─── event handlers ──────────────────────────────────────────────────────────

void CListPage::OnBnClickedToday() {
    CTime today = CTime::GetCurrentTime();
    m_dtPicker.SetTime(&today);
    Refresh();
}

void CListPage::OnDtnDatetimechangePicker(NMHDR* /*pNMHDR*/, LRESULT* pResult) {
    Refresh();
    *pResult = 0;
}

int CListPage::GetSelectedId() const {
    int sel = m_listCtrl.GetNextItem(-1, LVNI_SELECTED);
    if (sel == -1) return -1;
    return (int)m_listCtrl.GetItemData(sel);
}

void CListPage::OnBnClickedDelete() {
    int id = GetSelectedId();
    if (id == -1) {
        AfxMessageBox(_T("삭제할 질문을 목록에서 선택하세요."), MB_ICONINFORMATION);
        return;
    }

    if (AfxMessageBox(_T("선택한 질문을 삭제하시겠습니까?"),
                      MB_YESNO | MB_ICONQUESTION) != IDYES)
        return;

    CString err;
    if (DBManager::Get().DeleteQuestion(id, err)) {
        Refresh();
    } else {
        AfxMessageBox(err, MB_ICONERROR);
    }
}

void CListPage::OnNMDblclkList(NMHDR* /*pNMHDR*/, LRESULT* pResult) {
    OnBnClickedDelete();
    *pResult = 0;
}

void CListPage::OnNMRClickList(NMHDR* /*pNMHDR*/, LRESULT* pResult) {
    // 우클릭 컨텍스트 메뉴
    int id = GetSelectedId();
    if (id == -1) { *pResult = 0; return; }

    CMenu menu;
    menu.CreatePopupMenu();
    menu.AppendMenu(MF_STRING, 1, _T("이 질문 삭제"));

    CPoint pt;
    GetCursorPos(&pt);

    if (menu.TrackPopupMenu(TPM_RETURNCMD | TPM_RIGHTBUTTON, pt.x, pt.y, this) == 1)
        OnBnClickedDelete();

    *pResult = 0;
}

COLORREF CListPage::CategoryColor(const CString& cat) const {
    if (cat == _T("CS"))   return RGB(59, 130, 246);
    if (cat == _T("C++"))  return RGB(239, 68, 68);
    return RGB(16, 185, 129);  // 자료구조
}
