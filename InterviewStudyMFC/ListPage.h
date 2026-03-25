#pragma once
#include "resource.h"
#include "DBManager.h"

class CListPage : public CDialogEx {
public:
    explicit CListPage(CWnd* pParent = nullptr);
    enum { IDD = IDD_LIST_PAGE };

    // 외부에서 호출: 현재 날짜 기준으로 목록 갱신
    void Refresh();

protected:
    virtual void DoDataExchange(CDataExchange* pDX) override;
    virtual BOOL OnInitDialog() override;

    afx_msg void OnBnClickedToday();
    afx_msg void OnDtnDatetimechangePicker(NMHDR* pNMHDR, LRESULT* pResult);
    afx_msg void OnBnClickedDelete();
    afx_msg void OnNMDblclkList(NMHDR* pNMHDR, LRESULT* pResult);
    afx_msg void OnNMRClickList(NMHDR* pNMHDR, LRESULT* pResult);

    DECLARE_MESSAGE_MAP()

private:
    CDateTimeCtrl  m_dtPicker;
    CListCtrl      m_listCtrl;

    void    SetupColumns();
    CString GetPickedDate() const;
    int     GetSelectedId() const;

    // 카테고리별 색상
    COLORREF CategoryColor(const CString& cat) const;
};
