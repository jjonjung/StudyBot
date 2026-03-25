#include "stdafx.h"
#include "ConnectDlg.h"
#include "DBManager.h"

BEGIN_MESSAGE_MAP(CConnectDlg, CDialogEx)
END_MESSAGE_MAP()

CConnectDlg::CConnectDlg(CWnd* pParent)
    : CDialogEx(IDD_CONNECT_DLG, pParent) {}

void CConnectDlg::DoDataExchange(CDataExchange* pDX) {
    CDialogEx::DoDataExchange(pDX);
    DDX_Control(pDX, IDC_EDIT_HOST,          m_edtHost);
    DDX_Control(pDX, IDC_EDIT_PORT,          m_edtPort);
    DDX_Control(pDX, IDC_EDIT_USER,          m_edtUser);
    DDX_Control(pDX, IDC_EDIT_PASS,          m_edtPass);
    DDX_Control(pDX, IDC_EDIT_DBNAME,        m_edtDB);
    DDX_Control(pDX, IDC_STATIC_CONN_STATUS, m_stStatus);
}

BOOL CConnectDlg::OnInitDialog() {
    CDialogEx::OnInitDialog();
    SetWindowText(_T("DB 연결 설정"));
    GetDlgItem(IDOK)->SetWindowText(_T("연결"));
    m_edtHost.SetWindowText(_T("localhost"));
    m_edtPort.SetWindowText(_T("3306"));
    m_edtUser.SetWindowText(_T("root"));
    m_edtDB.SetWindowText(_T("study_card"));
    return TRUE;
}

void CConnectDlg::OnOK() {
    CString host, portStr, user, pass, db;
    m_edtHost.GetWindowText(host);
    m_edtPort.GetWindowText(portStr);
    m_edtUser.GetWindowText(user);
    m_edtPass.GetWindowText(pass);
    m_edtDB.GetWindowText(db);

    if (host.IsEmpty() || user.IsEmpty() || db.IsEmpty()) {
        m_stStatus.SetWindowText(_T("Host / User / Database를 입력하세요."));
        return;
    }
    unsigned int port = (unsigned int)_ttoi(portStr);
    if (!port) port = 3306;

    m_stStatus.SetWindowText(_T("연결 중..."));
    UpdateWindow();

    CT2CA h(host, CP_ACP), u(user, CP_ACP), p(pass, CP_ACP), d(db, CP_ACP);
    if (DBManager::Get().Connect(h, port, u, p, d))
        CDialogEx::OnOK();
    else
        m_stStatus.SetWindowText(_T("연결 실패. 정보를 확인해주세요."));
}
