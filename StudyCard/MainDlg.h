#pragma once
#include "resource.h"
#include "InputPage.h"
#include "ListPage.h"
#include "FlashPage.h"
#include "ConnectDlg.h"

class CMainDlg : public CDialogEx {
public:
    explicit CMainDlg(CWnd* pParent = nullptr);
    enum { IDD = IDD_MAIN_DLG };

protected:
    virtual void DoDataExchange(CDataExchange* pDX) override;
    virtual BOOL OnInitDialog() override;
    virtual void OnPaint() override;
    virtual HCURSOR OnQueryDragIcon();

    afx_msg BOOL   OnEraseBkgnd(CDC* pDC);
    afx_msg HBRUSH OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor);
    afx_msg void   OnTcnSelchangeTab(NMHDR* pNMHDR, LRESULT* pResult);
    afx_msg void   OnBnClickedConnect();

    DECLARE_MESSAGE_MAP()

private:
    CTabCtrl    m_tabCtrl;
    CInputPage  m_inputPage;
    CListPage   m_listPage;
    CFlashPage  m_flashPage;

    HICON  m_hIcon;
    CBrush m_brBg;

    void ShowTabPage(int idx);
    void LayoutPages();
    void UpdateConnStatus();
};
