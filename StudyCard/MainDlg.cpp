#include "stdafx.h"
#include "MainDlg.h"
#include "DBManager.h"

static const COLORREF CLR_BG      = RGB(12, 20, 38);
static const COLORREF CLR_TEXT    = RGB(200, 220, 255);
static const COLORREF CLR_MUTED   = RGB( 80, 110, 160);
static const COLORREF CLR_ACCENT  = RGB( 59, 130, 246);

BEGIN_MESSAGE_MAP(CMainDlg, CDialogEx)
    ON_WM_PAINT()
    ON_WM_ERASEBKGND()
    ON_WM_CTLCOLOR()
    ON_NOTIFY(TCN_SELCHANGE, IDC_TAB, &CMainDlg::OnTcnSelchangeTab)
    ON_BN_CLICKED(IDC_BTN_CONNECT, &CMainDlg::OnBnClickedConnect)
END_MESSAGE_MAP()

CMainDlg::CMainDlg(CWnd* pParent)
    : CDialogEx(IDD_MAIN_DLG, pParent)
    , m_hIcon(AfxGetApp()->LoadStandardIcon(IDI_APPLICATION))
{
    m_brBg.CreateSolidBrush(CLR_BG);
}

void CMainDlg::DoDataExchange(CDataExchange* pDX) {
    CDialogEx::DoDataExchange(pDX);
    DDX_Control(pDX, IDC_TAB, m_tabCtrl);
}

BOOL CMainDlg::OnInitDialog() {
    CDialogEx::OnInitDialog();

    SetWindowText(_T("Study Card  —  C++ 게임 개발 면접 준비"));
    SetIcon(m_hIcon, TRUE);
    SetIcon(m_hIcon, FALSE);

    // 부제목
    SetDlgItemText(IDC_STATIC_SUBTITLE,
        _T("  면접 스터디 플래시 카드  |  CS  ·  C++  ·  자료구조"));
    SetDlgItemText(IDC_BTN_CONNECT, _T("DB 연결"));

    // 탭 항목
    m_tabCtrl.InsertItem(0, _T("  질문 등록  "));
    m_tabCtrl.InsertItem(1, _T("  카드 목록  "));
    m_tabCtrl.InsertItem(2, _T("  플래시 카드  "));

    // 차일드 페이지 생성
    m_inputPage.Create(IDD_INPUT_PAGE, &m_tabCtrl);
    m_listPage.Create(IDD_LIST_PAGE,   &m_tabCtrl);
    m_flashPage.Create(IDD_FLASH_PAGE, &m_tabCtrl);

    LayoutPages();
    ShowTabPage(0);
    UpdateConnStatus();

    return TRUE;
}

// ─── layout ──────────────────────────────────────────────────────────────────

void CMainDlg::LayoutPages() {
    CRect rcDisplay;
    m_tabCtrl.GetClientRect(&rcDisplay);
    m_tabCtrl.AdjustRect(FALSE, &rcDisplay);

    m_inputPage.MoveWindow(rcDisplay);
    m_listPage.MoveWindow(rcDisplay);
    m_flashPage.MoveWindow(rcDisplay);
}

void CMainDlg::ShowTabPage(int idx) {
    m_inputPage.ShowWindow(idx == 0 ? SW_SHOW : SW_HIDE);
    m_listPage.ShowWindow (idx == 1 ? SW_SHOW : SW_HIDE);
    m_flashPage.ShowWindow(idx == 2 ? SW_SHOW : SW_HIDE);

    if (idx == 1) m_listPage.Refresh();
    if (idx == 2) m_flashPage.LoadCards();
}

// ─── tab change ──────────────────────────────────────────────────────────────

void CMainDlg::OnTcnSelchangeTab(NMHDR* /*pNMHDR*/, LRESULT* pResult) {
    ShowTabPage(m_tabCtrl.GetCurSel());
    *pResult = 0;
}

// ─── DB connect ──────────────────────────────────────────────────────────────

void CMainDlg::OnBnClickedConnect() {
    CConnectDlg dlg(this);
    if (dlg.DoModal() == IDOK) {
        UpdateConnStatus();
        m_inputPage.UpdateCardCount();
        // 현재 탭이 플래시/목록이면 갱신
        int cur = m_tabCtrl.GetCurSel();
        if (cur == 1) m_listPage.Refresh();
        if (cur == 2) m_flashPage.LoadCards();
    }
}

void CMainDlg::UpdateConnStatus() {
    bool ok = DBManager::Get().IsConnected();
    SetDlgItemText(IDC_STATIC_CONN,
        ok ? _T("● DB 연결됨  —  study_card")
           : _T("○ DB 미연결  —  우측 'DB 연결' 버튼을 클릭하세요"));
    SetDlgItemText(IDC_BTN_CONNECT, ok ? _T("재연결") : _T("DB 연결"));
}

// ─── 배경 / 텍스트 색상 ──────────────────────────────────────────────────────

BOOL CMainDlg::OnEraseBkgnd(CDC* pDC) {
    CRect rc;
    GetClientRect(&rc);
    pDC->FillSolidRect(rc, CLR_BG);
    return TRUE;
}

HBRUSH CMainDlg::OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor) {
    HBRUSH hBr = CDialogEx::OnCtlColor(pDC, pWnd, nCtlColor);
    int id = pWnd->GetDlgCtrlID();

    if (id == IDC_STATIC_SUBTITLE) {
        pDC->SetTextColor(CLR_ACCENT);
        pDC->SetBkColor(CLR_BG);
        return m_brBg;
    }
    if (id == IDC_STATIC_CONN) {
        pDC->SetTextColor(CLR_MUTED);
        pDC->SetBkColor(CLR_BG);
        return m_brBg;
    }
    if (nCtlColor == CTLCOLOR_STATIC) {
        pDC->SetTextColor(CLR_TEXT);
        pDC->SetBkColor(CLR_BG);
        return m_brBg;
    }
    return hBr;
}

void CMainDlg::OnPaint() {
    if (IsIconic()) {
        CPaintDC dc(this);
        SendMessage(WM_ICONERASEBKGND, (WPARAM)dc.GetSafeHdc(), 0);
        int cx = GetSystemMetrics(SM_CXICON);
        int cy = GetSystemMetrics(SM_CYICON);
        CRect rc; GetClientRect(&rc);
        dc.DrawIcon((rc.Width()-cx+1)/2, (rc.Height()-cy+1)/2, m_hIcon);
    } else {
        CDialogEx::OnPaint();
    }
}

HCURSOR CMainDlg::OnQueryDragIcon() { return (HCURSOR)m_hIcon; }
