#include "stdafx.h"
#include "AddPage.h"
#include "DBManager.h"

BEGIN_MESSAGE_MAP(CAddPage, CDialogEx)
    ON_BN_CLICKED(IDC_BTN_SUBMIT, &CAddPage::OnBnClickedSubmit)
END_MESSAGE_MAP()

CAddPage::CAddPage(CWnd* pParent)
    : CDialogEx(IDD_ADD_PAGE, pParent) {}

void CAddPage::DoDataExchange(CDataExchange* pDX) {
    CDialogEx::DoDataExchange(pDX);
    DDX_Control(pDX, IDC_DATE_ADD,       m_dtAdd);
    DDX_Control(pDX, IDC_COMBO_MEMBER,   m_cmbMember);
    DDX_Control(pDX, IDC_COMBO_CATEGORY, m_cmbCategory);
    DDX_Control(pDX, IDC_EDIT_QUESTION,  m_edtQuestion);
}

BOOL CAddPage::OnInitDialog() {
    CDialogEx::OnInitDialog();

    // 레이블 한글 설정 (SetDlgItemText로 IDC_STATIC에는 접근 불가,
    //  .rc 에 한글 인코딩 이슈가 있을 경우 아래에서 직접 설정)
    GetDlgItem(IDC_BTN_SUBMIT)->SetWindowText(_T("등록하기"));

    // 멤버 항목
    m_cmbMember.AddString(_T("여민"));
    m_cmbMember.AddString(_T("은정"));
    m_cmbMember.AddString(_T("혜선"));

    // 카테고리 항목
    m_cmbCategory.AddString(_T("CS"));
    m_cmbCategory.AddString(_T("C++"));
    m_cmbCategory.AddString(_T("자료구조"));

    // 오늘 날짜 기본값
    CTime today = CTime::GetCurrentTime();
    m_dtAdd.SetTime(&today);

    return TRUE;
}

void CAddPage::OnBnClickedSubmit() {
    if (!DBManager::Get().IsConnected()) {
        AfxMessageBox(
            _T("DB에 연결되지 않았습니다.\n상단의 'DB 연결' 버튼을 클릭하세요."),
            MB_ICONWARNING);
        return;
    }

    // ── 날짜 ──────────────────────────────────────────────────────────
    CTime t;
    m_dtAdd.GetTime(t);
    CString date;
    date.Format(_T("%04d-%02d-%02d"), t.GetYear(), t.GetMonth(), t.GetDay());

    // ── 멤버 ──────────────────────────────────────────────────────────
    int mSel = m_cmbMember.GetCurSel();
    if (mSel == CB_ERR) {
        AfxMessageBox(_T("멤버를 선택해주세요."), MB_ICONWARNING);
        return;
    }
    CString member;
    m_cmbMember.GetLBText(mSel, member);

    // ── 카테고리 ──────────────────────────────────────────────────────
    int cSel = m_cmbCategory.GetCurSel();
    if (cSel == CB_ERR) {
        AfxMessageBox(_T("카테고리를 선택해주세요."), MB_ICONWARNING);
        return;
    }
    CString category;
    m_cmbCategory.GetLBText(cSel, category);

    // ── 질문 ──────────────────────────────────────────────────────────
    CString question;
    m_edtQuestion.GetWindowText(question);
    question.Trim();
    if (question.IsEmpty()) {
        AfxMessageBox(_T("질문 내용을 입력해주세요."), MB_ICONWARNING);
        m_edtQuestion.SetFocus();
        return;
    }

    // ── DB 저장 ───────────────────────────────────────────────────────
    CString err;
    if (DBManager::Get().AddQuestion(date, member, category, question, err)) {
        AfxMessageBox(_T("질문이 등록되었습니다!"), MB_ICONINFORMATION);
        m_cmbMember.SetCurSel(-1);
        m_cmbCategory.SetCurSel(-1);
        m_edtQuestion.SetWindowText(_T(""));
        m_edtQuestion.SetFocus();
    } else {
        AfxMessageBox(err, MB_ICONERROR);
    }
}
