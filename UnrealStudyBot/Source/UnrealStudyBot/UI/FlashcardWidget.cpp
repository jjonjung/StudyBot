#include "UI/FlashcardWidget.h"

void UFlashcardWidget::NativeConstruct()
{
    Super::NativeConstruct();

    BtnFlip->OnClicked.AddDynamic   (this, &UFlashcardWidget::OnFlipClicked);
    BtnKnown->OnClicked.AddDynamic  (this, &UFlashcardWidget::OnKnownClicked);
    BtnUnknown->OnClicked.AddDynamic(this, &UFlashcardWidget::OnUnknownClicked);

    // 시작은 질문면
    CardSwitcher->SetActiveWidgetIndex(0);
    BtnKnown->SetVisibility(ESlateVisibility::Collapsed);
    BtnUnknown->SetVisibility(ESlateVisibility::Collapsed);
}

void UFlashcardWidget::SetCard(const FFlashCard& Card, int32 CurrentIndex, int32 Total)
{
    m_currentCard = Card;

    TxtQuestion->SetText(FText::FromString(Card.Question));
    TxtAnswer->SetText  (FText::FromString(Card.Answer));
    TxtCategory->SetText(FText::FromString(Card.Category));
    TxtProgress->SetText(FText::FromString(
        FString::Printf(TEXT("%d / %d"), CurrentIndex + 1, Total)));

    // 앞면(질문)으로 초기화
    CardSwitcher->SetActiveWidgetIndex(0);
    BtnKnown->SetVisibility(ESlateVisibility::Collapsed);
    BtnUnknown->SetVisibility(ESlateVisibility::Collapsed);
}

void UFlashcardWidget::OnFlipClicked()
{
    // 뒷면(답변)으로 전환
    CardSwitcher->SetActiveWidgetIndex(1);
    BtnKnown->SetVisibility  (ESlateVisibility::Visible);
    BtnUnknown->SetVisibility(ESlateVisibility::Visible);
}

void UFlashcardWidget::OnKnownClicked()
{
    OnCardJudged.Broadcast(true);
}

void UFlashcardWidget::OnUnknownClicked()
{
    OnCardJudged.Broadcast(false);
}
