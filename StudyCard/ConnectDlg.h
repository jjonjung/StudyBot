#pragma once
#include "resource.h"

class CConnectDlg : public CDialogEx {
public:
    explicit CConnectDlg(CWnd* pParent = nullptr);
    enum { IDD = IDD_CONNECT_DLG };

protected:
    virtual void DoDataExchange(CDataExchange* pDX) override;
    virtual BOOL OnInitDialog() override;
    virtual void OnOK() override;
    DECLARE_MESSAGE_MAP()

private:
    CEdit   m_edtHost, m_edtPort, m_edtUser, m_edtPass, m_edtDB;
    CStatic m_stStatus;
};
