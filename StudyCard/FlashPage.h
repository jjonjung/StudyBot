#pragma once
#include "resource.h"
#include "DBManager.h"

class CFlashPage : public CDialogEx {
public:
    explicit CFlashPage(CWnd* pParent = nullptr);
    enum { IDD = IDD_FLASH_PAGE };

    void LoadCards(const CString& category = _T(""));

protected:
    virtual void DoDataExchange(CDataExchange* pDX) override;
    virtual BOOL OnInitDialog() override;

    // 카드 flip 버튼 (owner-draw)
    afx_msg void OnDrawItem(int nIDCtl, LPDRAWITEMSTRUCT lpDIS);
    afx_msg void OnBnClickedCardFlip();

    // 카테고리 필터
    afx_msg void OnBnClickedCatAll();
    afx_msg void OnBnClickedCatCS();
    afx_msg void OnBnClickedCatCpp();
    afx_msg void OnBnClickedCatDs();

    // 네비게이션
    afx_msg void OnBnClickedPrev();
    afx_msg void OnBnClickedNext();

    // 결과
    afx_msg void OnBnClickedKnown();
    afx_msg void OnBnClickedUnknown();
    afx_msg void OnBnClickedShuffle();
    afx_msg void OnBnClickedReset();

    // 뒤집기 애니메이션
    afx_msg void OnTimer(UINT_PTR nIDEvent);

    DECLARE_MESSAGE_MAP()

private:
    // ── 카드 데이터 ──────────────────────────────────────────────────
    std::vector<FlashCard> m_cards;
    std::vector<int>       m_indices;   // 현재 순서 (셔플 지원)
    int                    m_nCurrent;  // 현재 인덱스 (into m_indices)
    CString                m_strFilter; // 현재 카테고리 필터

    // ── 플립 상태 ─────────────────────────────────────────────────────
    bool  m_bShowingQuestion;  // true=질문면, false=답변면
    int   m_nFlipStep;         // 0=정지, 1~20=애니메이션 중

    // ── 세션 통계 ─────────────────────────────────────────────────────
    int m_nKnown;
    int m_nUnknown;

    // ── 카드 그리기 ───────────────────────────────────────────────────
    void DrawCard(CDC* pDC, const CRect& rcBase);
    void DrawWrappedText(CDC* pDC, const CString& text,
                         CRect rc, COLORREF clr, int fontSize, bool bold);

    COLORREF CategoryColor(const CString& cat) const;
    COLORREF CategoryColorLight(const CString& cat) const;

    // ── UI 헬퍼 ──────────────────────────────────────────────────────
    void UpdateProgress();
    void UpdateCategoryButtons();
    void NavigateTo(int idx);
    void StartFlip();

    const FlashCard* CurrentCard() const;
};
