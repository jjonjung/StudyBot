#pragma once
#include "resource.h"

class CAddPage : public CDialogEx {
public:
    explicit CAddPage(CWnd* pParent = nullptr);
    enum { IDD = IDD_ADD_PAGE };

protected:
    virtual void DoDataExchange(CDataExchange* pDX) override;
    virtual BOOL OnInitDialog() override;

    afx_msg void OnBnClickedSubmit();
    DECLARE_MESSAGE_MAP()

private:
    CDateTimeCtrl  m_dtAdd;
    CComboBox      m_cmbMember;
    CComboBox      m_cmbCategory;
    CEdit          m_edtQuestion;
};
