#pragma once
#include "resource.h"
#include "ListPage.h"
#include "AddPage.h"
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

    afx_msg void OnTcnSelchangeTab(NMHDR* pNMHDR, LRESULT* pResult);
    afx_msg void OnBnClickedDbConnect();
    afx_msg BOOL OnEraseBkgnd(CDC* pDC);
    afx_msg HBRUSH OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor);

    DECLARE_MESSAGE_MAP()

private:
    CTabCtrl   m_tabCtrl;
    CListPage  m_listPage;
    CAddPage   m_addPage;
    HICON      m_hIcon;

    CBrush     m_brBg;       // 다이얼로그 배경 브러시
    CBrush     m_brCard;     // 카드(그룹박스) 배경 브러시

    void ShowTabPage(int idx);
    void LayoutPages();
    void UpdateDbStatus();
    void SetRuleTexts();
};
