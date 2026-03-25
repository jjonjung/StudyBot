#include "stdafx.h"
#include "App.h"
#include "MainDlg.h"

BEGIN_MESSAGE_MAP(CStudyCardApp, CWinApp)
END_MESSAGE_MAP()

CStudyCardApp theApp;

CStudyCardApp::CStudyCardApp() {}

BOOL CStudyCardApp::InitInstance() {
    CWinApp::InitInstance();
    AfxEnableControlContainer();

    INITCOMMONCONTROLSEX icc;
    icc.dwSize = sizeof(icc);
    icc.dwICC  = ICC_DATE_CLASSES | ICC_LISTVIEW_CLASSES | ICC_TAB_CLASSES;
    InitCommonControlsEx(&icc);

    CMainDlg dlg;
    m_pMainWnd = &dlg;
    dlg.DoModal();
    return FALSE;
}
