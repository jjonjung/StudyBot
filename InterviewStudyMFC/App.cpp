#include "stdafx.h"
#include "App.h"
#include "MainDlg.h"

BEGIN_MESSAGE_MAP(CInterviewStudyApp, CWinApp)
END_MESSAGE_MAP()

CInterviewStudyApp theApp;

CInterviewStudyApp::CInterviewStudyApp() {}

BOOL CInterviewStudyApp::InitInstance() {
    CWinApp::InitInstance();
    AfxEnableControlContainer();

    // 공통 컨트롤 초기화 (DateTimePicker, ListView, TabControl 등)
    INITCOMMONCONTROLSEX icc;
    icc.dwSize = sizeof(icc);
    icc.dwICC  = ICC_DATE_CLASSES | ICC_LISTVIEW_CLASSES | ICC_TAB_CLASSES;
    InitCommonControlsEx(&icc);

    CMainDlg dlg;
    m_pMainWnd = &dlg;
    dlg.DoModal();

    return FALSE;
}
