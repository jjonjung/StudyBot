#pragma once
#include "resource.h"

class CInputPage : public CDialogEx {
public:
    explicit CInputPage(CWnd* pParent = nullptr);
    enum { IDD = IDD_INPUT_PAGE };

    void UpdateCardCount();

protected:
    virtual void DoDataExchange(CDataExchange* pDX) override;
    virtual BOOL OnInitDialog() override;

    afx_msg void OnBnClickedAdd();
    afx_msg void OnBnClickedClear();
    afx_msg void OnBnClickedImportCsv();

    DECLARE_MESSAGE_MAP()

private:
    CDateTimeCtrl m_dtInput;
    CComboBox     m_cmbCat;
    CEdit         m_edtQ;
    CEdit         m_edtA;

    CString GetPickedDate() const;
};
