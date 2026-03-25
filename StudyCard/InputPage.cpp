#include "stdafx.h"
#include "InputPage.h"
#include "DBManager.h"

BEGIN_MESSAGE_MAP(CInputPage, CDialogEx)
    ON_BN_CLICKED(IDC_BTN_ADD,        &CInputPage::OnBnClickedAdd)
    ON_BN_CLICKED(IDC_BTN_CLEAR,      &CInputPage::OnBnClickedClear)
    ON_BN_CLICKED(IDC_BTN_IMPORT_CSV, &CInputPage::OnBnClickedImportCsv)
END_MESSAGE_MAP()

CInputPage::CInputPage(CWnd* pParent)
    : CDialogEx(IDD_INPUT_PAGE, pParent) {}

void CInputPage::DoDataExchange(CDataExchange* pDX) {
    CDialogEx::DoDataExchange(pDX);
    DDX_Control(pDX, IDC_DATE_INPUT,    m_dtInput);
    DDX_Control(pDX, IDC_COMBO_CAT_IN,  m_cmbCat);
    DDX_Control(pDX, IDC_EDIT_QUESTION, m_edtQ);
    DDX_Control(pDX, IDC_EDIT_ANSWER,   m_edtA);
}

BOOL CInputPage::OnInitDialog() {
    CDialogEx::OnInitDialog();

    // 레이블 한글 설정
    SetDlgItemText(IDC_BTN_ADD,        _T("카드 추가"));
    SetDlgItemText(IDC_BTN_CLEAR,      _T("초기화"));
    SetDlgItemText(IDC_BTN_IMPORT_CSV, _T("CSV 가져오기"));

    // 카테고리 콤보
    m_cmbCat.AddString(_T("CS"));
    m_cmbCat.AddString(_T("C++"));
    m_cmbCat.AddString(_T("자료구조"));
    m_cmbCat.SetCurSel(0);

    // 오늘 날짜
    CTime today = CTime::GetCurrentTime();
    m_dtInput.SetTime(&today);

    UpdateCardCount();
    return TRUE;
}

CString CInputPage::GetPickedDate() const {
    CTime t;
    m_dtInput.GetTime(t);
    CString s;
    s.Format(_T("%04d-%02d-%02d"), t.GetYear(), t.GetMonth(), t.GetDay());
    return s;
}

void CInputPage::UpdateCardCount() {
    int n = DBManager::Get().GetTotalCount();
    CString msg;
    msg.Format(_T("총 카드 수: %d"), n);
    SetDlgItemText(IDC_STATIC_CARD_COUNT, msg);
}

void CInputPage::OnBnClickedAdd() {
    if (!DBManager::Get().IsConnected()) {
        AfxMessageBox(_T("DB에 연결되지 않았습니다."), MB_ICONWARNING);
        return;
    }

    // 카테고리
    int sel = m_cmbCat.GetCurSel();
    if (sel == CB_ERR) {
        AfxMessageBox(_T("카테고리를 선택해주세요."), MB_ICONWARNING);
        return;
    }
    CString cat;
    m_cmbCat.GetLBText(sel, cat);

    // 질문
    CString q;
    m_edtQ.GetWindowText(q);
    q.Trim();
    if (q.IsEmpty()) {
        AfxMessageBox(_T("질문 내용을 입력해주세요."), MB_ICONWARNING);
        m_edtQ.SetFocus();
        return;
    }

    // 답변
    CString a;
    m_edtA.GetWindowText(a);
    a.Trim();
    if (a.IsEmpty()) {
        AfxMessageBox(_T("예상 답변을 입력해주세요."), MB_ICONWARNING);
        m_edtA.SetFocus();
        return;
    }

    FlashCard card;
    card.category  = cat;
    card.question  = q;
    card.answer    = a;
    card.studyDate = GetPickedDate();

    CString err;
    if (DBManager::Get().AddCard(card, err)) {
        AfxMessageBox(_T("카드가 추가되었습니다!"), MB_ICONINFORMATION);
        m_edtQ.SetWindowText(_T(""));
        m_edtA.SetWindowText(_T(""));
        m_cmbCat.SetCurSel(0);
        m_edtQ.SetFocus();
        UpdateCardCount();
    } else {
        AfxMessageBox(err, MB_ICONERROR);
    }
}

void CInputPage::OnBnClickedClear() {
    m_edtQ.SetWindowText(_T(""));
    m_edtA.SetWindowText(_T(""));
    m_cmbCat.SetCurSel(0);
    m_edtQ.SetFocus();
}

void CInputPage::OnBnClickedImportCsv() {
    if (!DBManager::Get().IsConnected()) {
        AfxMessageBox(_T("먼저 DB에 연결해주세요."), MB_ICONWARNING);
        return;
    }

    CFileDialog dlg(TRUE, _T("csv"), nullptr,
                    OFN_FILEMUSTEXIST | OFN_HIDEREADONLY,
                    _T("CSV 파일 (*.csv)|*.csv|모든 파일 (*.*)|*.*||"));
    if (dlg.DoModal() != IDOK) return;

    CString err;
    int n = DBManager::Get().ImportCSV(dlg.GetPathName(), err);

    if (n > 0) {
        CString msg;
        msg.Format(_T("%d개의 카드를 가져왔습니다!"), n);
        AfxMessageBox(msg, MB_ICONINFORMATION);
        UpdateCardCount();
    } else {
        AfxMessageBox(err.IsEmpty() ? _T("가져오기 실패") : err, MB_ICONERROR);
    }
}
