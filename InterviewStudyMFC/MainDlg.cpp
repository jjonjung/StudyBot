#include "stdafx.h"
#include "MainDlg.h"
#include "DBManager.h"

BEGIN_MESSAGE_MAP(CMainDlg, CDialogEx)
    ON_WM_PAINT()
    ON_WM_ERASEBKGND()
    ON_WM_CTLCOLOR()
    ON_NOTIFY(TCN_SELCHANGE, IDC_TAB, &CMainDlg::OnTcnSelchangeTab)
    ON_BN_CLICKED(IDC_BTN_DB_CONNECT, &CMainDlg::OnBnClickedDbConnect)
END_MESSAGE_MAP()

// 색상 상수
static const COLORREF CLR_BG      = RGB( 18,  28,  45);   // 다이얼로그 배경
static const COLORREF CLR_CARD    = RGB( 25,  38,  60);   // 카드 배경
static const COLORREF CLR_TEXT    = RGB(220, 230, 242);   // 기본 텍스트
static const COLORREF CLR_MUTED   = RGB(100, 120, 148);   // 흐린 텍스트
static const COLORREF CLR_ACCENT  = RGB( 59, 130, 246);   // 파란색 강조

CMainDlg::CMainDlg(CWnd* pParent)
    : CDialogEx(IDD_MAIN_DLG, pParent)
    , m_hIcon(AfxGetApp()->LoadStandardIcon(IDI_APPLICATION))
{
    m_brBg.CreateSolidBrush(CLR_BG);
    m_brCard.CreateSolidBrush(CLR_CARD);
}

void CMainDlg::DoDataExchange(CDataExchange* pDX) {
    CDialogEx::DoDataExchange(pDX);
    DDX_Control(pDX, IDC_TAB, m_tabCtrl);
}

// ─── initialization ───────────────────────────────────────────────────────────

BOOL CMainDlg::OnInitDialog() {
    CDialogEx::OnInitDialog();

    SetWindowText(_T("면접 스터디 - C++ 게임 개발"));
    SetIcon(m_hIcon, TRUE);
    SetIcon(m_hIcon, FALSE);

    // 탭 항목 추가
    m_tabCtrl.InsertItem(0, _T("  질문 목록  "));
    m_tabCtrl.InsertItem(1, _T("  질문 등록  "));

    // 탭 배경색 (TabCtrl 자체 배경은 시스템 기본)

    // 차일드 페이지 생성
    m_listPage.Create(IDD_LIST_PAGE, &m_tabCtrl);
    m_addPage.Create(IDD_ADD_PAGE,  &m_tabCtrl);

    LayoutPages();
    ShowTabPage(0);

    // 하단 버튼 / 상태 텍스트
    GetDlgItem(IDC_BTN_DB_CONNECT)->SetWindowText(_T("DB 연결"));

    // 규칙 텍스트
    SetRuleTexts();
    UpdateDbStatus();

    return TRUE;
}

// ─── layout helpers ───────────────────────────────────────────────────────────

void CMainDlg::LayoutPages() {
    // 탭 컨트롤 내부 표시 영역 계산 (탭 헤더 제외)
    CRect rcDisplay;
    m_tabCtrl.GetClientRect(&rcDisplay);
    m_tabCtrl.AdjustRect(FALSE, &rcDisplay);

    // 페이지들은 m_tabCtrl 의 자식 → tab 클라이언트 좌표계
    m_listPage.MoveWindow(rcDisplay);
    m_addPage.MoveWindow(rcDisplay);
}

void CMainDlg::ShowTabPage(int idx) {
    m_listPage.ShowWindow(idx == 0 ? SW_SHOW : SW_HIDE);
    m_addPage.ShowWindow(idx == 1 ? SW_SHOW : SW_HIDE);
    if (idx == 0) m_listPage.Refresh();
}

// ─── rules text ───────────────────────────────────────────────────────────────

void CMainDlg::SetRuleTexts() {
    // 그룹박스 제목
    GetDlgItem(IDC_GRP_RULES)->SetWindowText(_T("  스터디 규칙"));

    SetDlgItemText(IDC_STATIC_RULE1,
        _T("[01]  월~금 매일 오후 9시에 스터디를 진행합니다."));
    SetDlgItemText(IDC_STATIC_RULE2,
        _T("[02]  오후 3시까지  자료구조 · CS · C++  문제를 각각 1개씩 올립니다."));
    SetDlgItemText(IDC_STATIC_RULE3,
        _T("[03]  다른 사람이 올린 질문의 답변을 외워서 스터디 시간에 대답합니다."));
    SetDlgItemText(IDC_STATIC_TARGETS,
        _T("목표 역량 : C++ 게임 컨텐츠 개발  |  MFC 테스트 클라이언트  |  DB 설계 (MySQL)"));
}

// ─── DB status ────────────────────────────────────────────────────────────────

void CMainDlg::UpdateDbStatus() {
    bool connected = DBManager::Get().IsConnected();
    SetDlgItemText(IDC_STATIC_DB_STATUS,
        connected ? _T("● DB 연결됨") : _T("○ DB 미연결  —  상단 'DB 연결' 버튼을 클릭하세요"));
    GetDlgItem(IDC_BTN_DB_CONNECT)->SetWindowText(
        connected ? _T("재연결") : _T("DB 연결"));
}

// ─── events ───────────────────────────────────────────────────────────────────

void CMainDlg::OnTcnSelchangeTab(NMHDR* /*pNMHDR*/, LRESULT* pResult) {
    ShowTabPage(m_tabCtrl.GetCurSel());
    *pResult = 0;
}

void CMainDlg::OnBnClickedDbConnect() {
    CConnectDlg dlg(this);
    if (dlg.DoModal() == IDOK) {
        UpdateDbStatus();
        m_listPage.Refresh();
    }
}

// ─── painting ─────────────────────────────────────────────────────────────────

BOOL CMainDlg::OnEraseBkgnd(CDC* pDC) {
    CRect rc;
    GetClientRect(&rc);
    pDC->FillSolidRect(rc, CLR_BG);
    return TRUE;
}

// 각 자식 컨트롤의 배경/텍스트 색상 지정
HBRUSH CMainDlg::OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor) {
    HBRUSH hBr = CDialogEx::OnCtlColor(pDC, pWnd, nCtlColor);

    int id = pWnd->GetDlgCtrlID();

    // 규칙 텍스트: 밝은 색 텍스트 + 카드 배경
    if (id == IDC_STATIC_RULE1 || id == IDC_STATIC_RULE2 || id == IDC_STATIC_RULE3) {
        pDC->SetTextColor(CLR_TEXT);
        pDC->SetBkColor(CLR_CARD);
        return m_brCard;
    }

    // 타깃/상태 텍스트: 흐린 색
    if (id == IDC_STATIC_TARGETS || id == IDC_STATIC_DB_STATUS) {
        pDC->SetTextColor(CLR_MUTED);
        pDC->SetBkColor(CLR_BG);
        return m_brBg;
    }

    // 그룹박스 프레임
    if (id == IDC_GRP_RULES) {
        pDC->SetTextColor(CLR_ACCENT);
        pDC->SetBkColor(CLR_BG);
        return m_brBg;
    }

    // 나머지 정적 텍스트
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
        dc.DrawIcon((rc.Width() - cx + 1) / 2, (rc.Height() - cy + 1) / 2, m_hIcon);
    } else {
        CDialogEx::OnPaint();
    }
}

HCURSOR CMainDlg::OnQueryDragIcon() {
    return (HCURSOR)m_hIcon;
}
