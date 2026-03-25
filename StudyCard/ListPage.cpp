#include "stdafx.h"
#include "ListPage.h"
#include "DBManager.h"

BEGIN_MESSAGE_MAP(CListPage, CDialogEx)
    ON_BN_CLICKED(IDC_BTN_REFRESH,     &CListPage::OnBnClickedRefresh)
    ON_BN_CLICKED(IDC_BTN_DELETE_CARD, &CListPage::OnBnClickedDelete)
    ON_CBN_SELCHANGE(IDC_COMBO_FILTER,  &CListPage::OnCbnSelchangeFilter)
    ON_NOTIFY(NM_DBLCLK, IDC_LIST_CARDS, &CListPage::OnNMDblclkList)
END_MESSAGE_MAP()

CListPage::CListPage(CWnd* pParent)
    : CDialogEx(IDD_LIST_PAGE, pParent) {}

void CListPage::DoDataExchange(CDataExchange* pDX) {
    CDialogEx::DoDataExchange(pDX);
    DDX_Control(pDX, IDC_COMBO_FILTER, m_cmbFilter);
    DDX_Control(pDX, IDC_LIST_CARDS,   m_listCtrl);
}

BOOL CListPage::OnInitDialog() {
    CDialogEx::OnInitDialog();

    SetDlgItemText(IDC_BTN_REFRESH,     _T("새로고침"));
    SetDlgItemText(IDC_BTN_DELETE_CARD, _T("삭제"));

    m_cmbFilter.AddString(_T("전체"));
    m_cmbFilter.AddString(_T("CS"));
    m_cmbFilter.AddString(_T("C++"));
    m_cmbFilter.AddString(_T("자료구조"));
    m_cmbFilter.SetCurSel(0);

    SetupColumns();
    Refresh();
    return TRUE;
}

void CListPage::SetupColumns() {
    m_listCtrl.SetExtendedStyle(LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);
    CRect rc;
    m_listCtrl.GetClientRect(&rc);
    int w = rc.Width();
    m_listCtrl.InsertColumn(0, _T("No"),      LVCFMT_CENTER, (int)(w * 0.05));
    m_listCtrl.InsertColumn(1, _T("카테고리"),LVCFMT_CENTER, (int)(w * 0.10));
    m_listCtrl.InsertColumn(2, _T("질문"),    LVCFMT_LEFT,   (int)(w * 0.45));
    m_listCtrl.InsertColumn(3, _T("예상 답변"),LVCFMT_LEFT,  (int)(w * 0.35));
    m_listCtrl.InsertColumn(4, _T("날짜"),    LVCFMT_CENTER, (int)(w * 0.05));
}

void CListPage::Refresh() {
    m_listCtrl.DeleteAllItems();
    if (!DBManager::Get().IsConnected()) {
        SetDlgItemText(IDC_STATIC_TOTAL, _T("DB 미연결"));
        return;
    }

    CString cat;
    int sel = m_cmbFilter.GetCurSel();
    if (sel > 0) m_cmbFilter.GetLBText(sel, cat);

    auto cards = DBManager::Get().GetCards(cat);

    for (int i = 0; i < (int)cards.size(); i++) {
        const auto& c = cards[i];
        CString no;
        no.Format(_T("%d"), i + 1);
        int idx = m_listCtrl.InsertItem(i, no);
        m_listCtrl.SetItemText(idx, 1, c.category);
        m_listCtrl.SetItemText(idx, 2, c.question);
        m_listCtrl.SetItemText(idx, 3, c.answer);
        m_listCtrl.SetItemText(idx, 4, c.studyDate);
        m_listCtrl.SetItemData(idx, (DWORD_PTR)c.id);
    }

    CString total;
    total.Format(_T("총 %d개"), (int)cards.size());
    SetDlgItemText(IDC_STATIC_TOTAL, total);
}

int CListPage::GetSelectedId() const {
    int sel = m_listCtrl.GetNextItem(-1, LVNI_SELECTED);
    return (sel == -1) ? -1 : (int)m_listCtrl.GetItemData(sel);
}

void CListPage::OnBnClickedRefresh()    { Refresh(); }
void CListPage::OnCbnSelchangeFilter()  { Refresh(); }

void CListPage::OnBnClickedDelete() {
    int id = GetSelectedId();
    if (id == -1) { AfxMessageBox(_T("삭제할 카드를 선택하세요."), MB_ICONINFORMATION); return; }
    if (AfxMessageBox(_T("선택한 카드를 삭제하시겠습니까?"),
                      MB_YESNO | MB_ICONQUESTION) != IDYES) return;
    CString err;
    if (DBManager::Get().DeleteCard(id, err)) Refresh();
    else AfxMessageBox(err, MB_ICONERROR);
}

void CListPage::OnNMDblclkList(NMHDR* /*pNMHDR*/, LRESULT* pResult) {
    OnBnClickedDelete();
    *pResult = 0;
}
