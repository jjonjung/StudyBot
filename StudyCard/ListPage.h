#pragma once
#include "resource.h"
#include "DBManager.h"

class CListPage : public CDialogEx {
public:
    explicit CListPage(CWnd* pParent = nullptr);
    enum { IDD = IDD_LIST_PAGE };

    void Refresh();

protected:
    virtual void DoDataExchange(CDataExchange* pDX) override;
    virtual BOOL OnInitDialog() override;

    afx_msg void OnBnClickedRefresh();
    afx_msg void OnBnClickedDelete();
    afx_msg void OnCbnSelchangeFilter();
    afx_msg void OnNMDblclkList(NMHDR* pNMHDR, LRESULT* pResult);

    DECLARE_MESSAGE_MAP()

private:
    CComboBox m_cmbFilter;
    CListCtrl m_listCtrl;

    void SetupColumns();
    int  GetSelectedId() const;
};
