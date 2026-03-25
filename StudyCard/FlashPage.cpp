#include "stdafx.h"
#include "FlashPage.h"
#include "DBManager.h"

// ── 색상 상수 ─────────────────────────────────────────────────────────────────
static const COLORREF CLR_BG_Q    = RGB( 15,  30,  60);   // 질문면 배경
static const COLORREF CLR_BG_A    = RGB( 10,  50,  35);   // 답변면 배경
static const COLORREF CLR_TEXT_W  = RGB(230, 240, 255);   // 밝은 텍스트
static const COLORREF CLR_TEXT_G  = RGB(150, 200, 150);   // 답변 서브텍스트
static const COLORREF CLR_HINT    = RGB(100, 120, 150);   // 힌트 텍스트

BEGIN_MESSAGE_MAP(CFlashPage, CDialogEx)
    ON_WM_DRAWITEM()
    ON_WM_TIMER()
    ON_BN_CLICKED(IDC_BTN_CARD_FLIP,    &CFlashPage::OnBnClickedCardFlip)
    ON_BN_CLICKED(IDC_BTN_CAT_ALL,      &CFlashPage::OnBnClickedCatAll)
    ON_BN_CLICKED(IDC_BTN_CAT_CS,       &CFlashPage::OnBnClickedCatCS)
    ON_BN_CLICKED(IDC_BTN_CAT_CPP,      &CFlashPage::OnBnClickedCatCpp)
    ON_BN_CLICKED(IDC_BTN_CAT_DS,       &CFlashPage::OnBnClickedCatDs)
    ON_BN_CLICKED(IDC_BTN_PREV,         &CFlashPage::OnBnClickedPrev)
    ON_BN_CLICKED(IDC_BTN_NEXT,         &CFlashPage::OnBnClickedNext)
    ON_BN_CLICKED(IDC_BTN_KNOWN,        &CFlashPage::OnBnClickedKnown)
    ON_BN_CLICKED(IDC_BTN_UNKNOWN,      &CFlashPage::OnBnClickedUnknown)
    ON_BN_CLICKED(IDC_BTN_SHUFFLE,      &CFlashPage::OnBnClickedShuffle)
    ON_BN_CLICKED(IDC_BTN_RESET_SESSION,&CFlashPage::OnBnClickedReset)
END_MESSAGE_MAP()

CFlashPage::CFlashPage(CWnd* pParent)
    : CDialogEx(IDD_FLASH_PAGE, pParent)
    , m_nCurrent(-1)
    , m_bShowingQuestion(true)
    , m_nFlipStep(0)
    , m_nKnown(0)
    , m_nUnknown(0)
{}

void CFlashPage::DoDataExchange(CDataExchange* pDX) {
    CDialogEx::DoDataExchange(pDX);
}

BOOL CFlashPage::OnInitDialog() {
    CDialogEx::OnInitDialog();

    // 한글 버튼 레이블
    SetDlgItemText(IDC_BTN_CAT_ALL,       _T("전 체"));
    SetDlgItemText(IDC_BTN_CAT_DS,        _T("자료구조"));
    SetDlgItemText(IDC_BTN_SHUFFLE,       _T("셔 플"));
    SetDlgItemText(IDC_BTN_PREV,          _T("◀ 이전"));
    SetDlgItemText(IDC_BTN_NEXT,          _T("다음 ▶"));
    SetDlgItemText(IDC_BTN_KNOWN,         _T("✔ 알았다"));
    SetDlgItemText(IDC_BTN_UNKNOWN,       _T("✘ 몰랐다"));
    SetDlgItemText(IDC_BTN_RESET_SESSION, _T("↺ 세션 초기화"));
    SetDlgItemText(IDC_STATIC_PROGRESS,   _T("카드를 불러오세요"));

    return TRUE;
}

// ─── 카드 로드 ───────────────────────────────────────────────────────────────

void CFlashPage::LoadCards(const CString& category) {
    m_strFilter = category;
    m_cards = DBManager::Get().GetCards(category);

    m_indices.clear();
    for (int i = 0; i < (int)m_cards.size(); i++)
        m_indices.push_back(i);

    m_nCurrent = m_cards.empty() ? -1 : 0;
    m_bShowingQuestion = true;
    m_nFlipStep = 0;
    m_nKnown = m_nUnknown = 0;

    UpdateCategoryButtons();
    UpdateProgress();

    // 카드 버튼 다시 그리기
    CWnd* pBtn = GetDlgItem(IDC_BTN_CARD_FLIP);
    if (pBtn) pBtn->Invalidate();
}

// ─── 현재 카드 포인터 ────────────────────────────────────────────────────────

const FlashCard* CFlashPage::CurrentCard() const {
    if (m_nCurrent < 0 || m_nCurrent >= (int)m_indices.size()) return nullptr;
    int idx = m_indices[m_nCurrent];
    if (idx < 0 || idx >= (int)m_cards.size()) return nullptr;
    return &m_cards[idx];
}

// ─── owner-draw 카드 그리기 ──────────────────────────────────────────────────

void CFlashPage::OnDrawItem(int nIDCtl, LPDRAWITEMSTRUCT lpDIS) {
    if (nIDCtl != IDC_BTN_CARD_FLIP) {
        CDialogEx::OnDrawItem(nIDCtl, lpDIS);
        return;
    }
    CDC* pDC = CDC::FromHandle(lpDIS->hDC);
    CRect rcBase(lpDIS->rcItem);
    DrawCard(pDC, rcBase);
}

void CFlashPage::DrawCard(CDC* pDC, const CRect& rcBase) {
    // ── 애니메이션: 카드 너비 계산 ─────────────────────────────────────
    float scale = 1.0f;
    if (m_nFlipStep > 0 && m_nFlipStep < 20) {
        float t = m_nFlipStep / 10.0f;
        scale = (t <= 1.0f) ? (1.0f - t) : (t - 1.0f);
        if (scale < 0.02f) scale = 0.02f;
    }

    int fullW = rcBase.Width();
    int animW = (int)(fullW * scale);
    int midX  = rcBase.left + fullW / 2;

    CRect rc = rcBase;
    rc.left  = midX - animW / 2;
    rc.right = midX + animW / 2;

    // ── 배경 전체 지우기 ────────────────────────────────────────────────
    COLORREF bgDialog = RGB(18, 28, 45);
    pDC->FillSolidRect(rcBase, bgDialog);
    if (rc.Width() < 4) return;

    // ── 카드 배경 ───────────────────────────────────────────────────────
    COLORREF bgCard = m_bShowingQuestion ? CLR_BG_Q : CLR_BG_A;
    CBrush brCard(bgCard);
    pDC->FillRect(rc, &brCard);

    // ── 카테고리 컬러 테두리 ────────────────────────────────────────────
    const FlashCard* pCard = CurrentCard();
    CString cat = pCard ? pCard->category : _T("CS");
    COLORREF borderClr = CategoryColor(cat);

    CPen penBorder(PS_SOLID, 4, borderClr);
    CPen* pOldPen = pDC->SelectObject(&penBorder);
    CBrush* pOldBr = (CBrush*)pDC->SelectStockObject(NULL_BRUSH);
    CRect rcBorder = rc;
    rcBorder.DeflateRect(2, 2);
    pDC->RoundRect(rcBorder, CPoint(12, 12));
    pDC->SelectObject(pOldPen);
    pDC->SelectObject(pOldBr);

    if (!pCard || scale < 0.3f) return;   // 애니메이션 중 텍스트 숨김

    int padding = 20;
    CRect rcInner = rcBorder;
    rcInner.DeflateRect(padding, padding);

    // ── 카드 타입 뱃지 ("질문" / "답변") ───────────────────────────────
    CString badge = m_bShowingQuestion ? _T("질  문") : _T("답  변");
    COLORREF badgeBg = CategoryColorLight(cat);

    CFont fBadge;
    fBadge.CreateFont(13, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                      DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                      CLEARTYPE_QUALITY, DEFAULT_PITCH, _T("맑은 고딕"));
    CFont* pOld = pDC->SelectObject(&fBadge);
    CSize szBadge = pDC->GetTextExtent(badge);

    CRect rcBadge(rcInner.left, rcInner.top,
                  rcInner.left + szBadge.cx + 20,
                  rcInner.top  + szBadge.cy + 8);
    CBrush brBadge(badgeBg);
    pDC->FillRect(rcBadge, &brBadge);
    pDC->SetBkMode(TRANSPARENT);
    pDC->SetTextColor(RGB(255, 255, 255));
    pDC->DrawText(badge, rcBadge, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    pDC->SelectObject(pOld);

    // ── 카테고리 태그 (우측 상단) ─────────────────────────────────────
    {
        CFont fCat;
        fCat.CreateFont(12, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                        CLEARTYPE_QUALITY, FIXED_PITCH, _T("Consolas"));
        CFont* p = pDC->SelectObject(&fCat);
        pDC->SetTextColor(borderClr);
        CRect rcCat = rcInner;
        rcCat.top += 2;
        pDC->DrawText(cat, rcCat, DT_RIGHT | DT_TOP | DT_SINGLELINE);
        pDC->SelectObject(p);
    }

    // ── 메인 텍스트 (질문 or 답변) ─────────────────────────────────────
    CRect rcText = rcInner;
    rcText.top += rcBadge.Height() + 18;
    rcText.bottom -= 40;

    CString mainText = m_bShowingQuestion ? pCard->question : pCard->answer;
    COLORREF textClr = m_bShowingQuestion ? CLR_TEXT_W : CLR_TEXT_G;
    DrawWrappedText(pDC, mainText, rcText, textClr, 15, false);

    // ── 하단 힌트 ──────────────────────────────────────────────────────
    CFont fHint;
    fHint.CreateFont(11, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                     DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                     CLEARTYPE_QUALITY, DEFAULT_PITCH, _T("맑은 고딕"));
    CFont* pH = pDC->SelectObject(&fHint);
    CRect rcHint = rcInner;
    rcHint.top = rcBorder.bottom - padding - 20;
    pDC->SetTextColor(CLR_HINT);
    CString hint = m_bShowingQuestion
        ? _T("클릭하여 답변 보기  ▼")
        : _T("클릭하여 질문으로 되돌아가기  ▲");
    pDC->DrawText(hint, rcHint, DT_CENTER | DT_BOTTOM | DT_SINGLELINE);
    pDC->SelectObject(pH);
}

void CFlashPage::DrawWrappedText(CDC* pDC, const CString& text,
                                  CRect rc, COLORREF clr,
                                  int fontSize, bool bold) {
    CFont font;
    font.CreateFont(fontSize, 0, 0, 0, bold ? FW_BOLD : FW_NORMAL,
                    FALSE, FALSE, FALSE,
                    DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                    CLEARTYPE_QUALITY, DEFAULT_PITCH, _T("맑은 고딕"));
    CFont* pOld = pDC->SelectObject(&font);
    pDC->SetTextColor(clr);
    pDC->SetBkMode(TRANSPARENT);
    pDC->DrawText(text, rc, DT_WORDBREAK | DT_CENTER | DT_VCENTER | DT_NOPREFIX);
    pDC->SelectObject(pOld);
}

// ─── 색상 테이블 ─────────────────────────────────────────────────────────────

COLORREF CFlashPage::CategoryColor(const CString& cat) const {
    if (cat == _T("CS"))   return RGB( 59, 130, 246);
    if (cat == _T("C++"))  return RGB(239,  68,  68);
    return                        RGB( 16, 185, 129);
}

COLORREF CFlashPage::CategoryColorLight(const CString& cat) const {
    if (cat == _T("CS"))   return RGB( 30,  80, 170);
    if (cat == _T("C++"))  return RGB(160,  40,  40);
    return                        RGB( 10, 120,  80);
}

// ─── flip 애니메이션 ─────────────────────────────────────────────────────────

void CFlashPage::StartFlip() {
    m_nFlipStep = 1;
    SetTimer(IDT_FLIP, 25, nullptr);   // 25ms 간격
}

void CFlashPage::OnTimer(UINT_PTR nIDEvent) {
    if (nIDEvent == IDT_FLIP) {
        m_nFlipStep++;

        // 중간(10)에서 앞/뒤 전환
        if (m_nFlipStep == 10)
            m_bShowingQuestion = !m_bShowingQuestion;

        // 카드 버튼 재그리기
        CWnd* pBtn = GetDlgItem(IDC_BTN_CARD_FLIP);
        if (pBtn) pBtn->Invalidate(FALSE);

        if (m_nFlipStep >= 20) {
            KillTimer(IDT_FLIP);
            m_nFlipStep = 0;
        }
    }
    CDialogEx::OnTimer(nIDEvent);
}

// ─── 카드 flip 클릭 ──────────────────────────────────────────────────────────

void CFlashPage::OnBnClickedCardFlip() {
    if (!CurrentCard() || m_nFlipStep > 0) return;
    StartFlip();
}

// ─── 네비게이션 ──────────────────────────────────────────────────────────────

void CFlashPage::NavigateTo(int idx) {
    if (m_cards.empty()) return;
    m_nCurrent = max(0, min(idx, (int)m_indices.size() - 1));
    m_bShowingQuestion = true;
    m_nFlipStep = 0;
    UpdateProgress();
    CWnd* pBtn = GetDlgItem(IDC_BTN_CARD_FLIP);
    if (pBtn) pBtn->Invalidate();
}

void CFlashPage::OnBnClickedPrev() { NavigateTo(m_nCurrent - 1); }
void CFlashPage::OnBnClickedNext() { NavigateTo(m_nCurrent + 1); }

// ─── 카테고리 필터 ───────────────────────────────────────────────────────────

void CFlashPage::OnBnClickedCatAll() { LoadCards(_T("")); }
void CFlashPage::OnBnClickedCatCS()  { LoadCards(_T("CS")); }
void CFlashPage::OnBnClickedCatCpp() { LoadCards(_T("C++")); }
void CFlashPage::OnBnClickedCatDs()  { LoadCards(_T("자료구조")); }

void CFlashPage::UpdateCategoryButtons() {
    // 활성 버튼 강조는 단순 텍스트로 표시
    // (실제 색상 변경은 OnCtlColor에서 처리 가능)
}

// ─── 결과 버튼 ───────────────────────────────────────────────────────────────

void CFlashPage::OnBnClickedKnown() {
    if (!CurrentCard()) return;
    m_cards[m_indices[m_nCurrent]].known = true;
    m_nKnown++;
    UpdateProgress();
    // 다음 카드로
    if (m_nCurrent + 1 < (int)m_indices.size())
        NavigateTo(m_nCurrent + 1);
    else
        AfxMessageBox(_T("마지막 카드입니다! 세션을 초기화하거나 필터를 변경하세요."),
                      MB_ICONINFORMATION);
}

void CFlashPage::OnBnClickedUnknown() {
    if (!CurrentCard()) return;
    m_nUnknown++;
    UpdateProgress();
    // 모르는 카드는 덱 끝으로 다시 추가
    m_indices.push_back(m_indices[m_nCurrent]);
    NavigateTo(m_nCurrent + 1);
}

void CFlashPage::OnBnClickedShuffle() {
    if (m_indices.empty()) return;
    std::mt19937 rng(std::random_device{}());
    std::shuffle(m_indices.begin(), m_indices.end(), rng);
    NavigateTo(0);
    AfxMessageBox(_T("카드 순서가 섞였습니다!"), MB_ICONINFORMATION);
}

void CFlashPage::OnBnClickedReset() {
    m_nKnown = m_nUnknown = 0;
    m_indices.clear();
    for (int i = 0; i < (int)m_cards.size(); i++) {
        m_cards[i].known = false;
        m_indices.push_back(i);
    }
    NavigateTo(0);
}

// ─── 진행 상황 업데이트 ──────────────────────────────────────────────────────

void CFlashPage::UpdateProgress() {
    if (m_cards.empty()) {
        SetDlgItemText(IDC_STATIC_PROGRESS, _T("카드 없음 — DB 연결 후 카드를 가져오세요"));
        return;
    }
    CString msg;
    int total   = (int)m_cards.size();
    int current = (m_nCurrent >= 0) ? m_nCurrent + 1 : 0;
    int queued  = (int)m_indices.size();
    msg.Format(_T("%d / %d  |  ✔ %d  ✘ %d  |  남은 카드: %d"),
               current, total, m_nKnown, m_nUnknown, queued - current);
    SetDlgItemText(IDC_STATIC_PROGRESS, msg);
}
