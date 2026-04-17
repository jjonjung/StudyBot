#include "UI/AlgorithmCardWidget.h"

void UAlgorithmCardWidget::NativeConstruct()
{
    Super::NativeConstruct();

    BtnFlip->OnClicked.AddDynamic    (this, &UAlgorithmCardWidget::OnFlipClicked);
    BtnCpp->OnClicked.AddDynamic     (this, &UAlgorithmCardWidget::OnCppClicked);
    BtnCSharp->OnClicked.AddDynamic  (this, &UAlgorithmCardWidget::OnCSharpClicked);
    BtnKnown->OnClicked.AddDynamic   (this, &UAlgorithmCardWidget::OnKnownClicked);
    BtnUnknown->OnClicked.AddDynamic (this, &UAlgorithmCardWidget::OnUnknownClicked);

    // 초기 상태: 질문면, C++ 탭
    FaceSwitcher->SetActiveWidgetIndex(0);
    CodeSwitcher->SetActiveWidgetIndex(0);
}

void UAlgorithmCardWidget::SetCard(const FFlashCard& Card)
{
    m_card = Card;
    FaceSwitcher->SetActiveWidgetIndex(0); // 항상 질문면으로 리셋

    // ── 질문면 ────────────────────────────────────────────
    TxtQuestion->SetText(FText::FromString(Card.Question));

    FString ComplexLabel = Card.TimeComplexity.IsEmpty()
        ? TEXT("복잡도 미정") : Card.TimeComplexity;
    TxtTimeComplexity->SetText(FText::FromString(ComplexLabel));

    // ── 답변면 ────────────────────────────────────────────
    TxtAnswer->SetText(FText::FromString(Card.Answer));

    TxtCoreConditions->SetText(FText::FromString(
        Card.CoreConditions.IsEmpty()
            ? TEXT("—") : Card.CoreConditions));

    TxtSelectionReason->SetText(FText::FromString(
        Card.SelectionReason.IsEmpty()
            ? TEXT("—") : Card.SelectionReason));

    // ── 코드 탭 ──────────────────────────────────────────
    TxtCodeCpp->SetText(FText::FromString(
        Card.CodeCpp.IsEmpty() ? TEXT("// 코드 없음") : Card.CodeCpp));
    TxtCodeCSharp->SetText(FText::FromString(
        Card.CodeCSharp.IsEmpty() ? TEXT("// 코드 없음") : Card.CodeCSharp));

    // 양쪽 코드가 없으면 C++ 기본 탭 유지
    CodeSwitcher->SetActiveWidgetIndex(0);
}

void UAlgorithmCardWidget::OnFlipClicked()
{
    // 질문↔답변 토글
    int32 Current = FaceSwitcher->GetActiveWidgetIndex();
    FaceSwitcher->SetActiveWidgetIndex(Current == 0 ? 1 : 0);
}
