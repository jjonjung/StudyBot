#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/UniformGridPanel.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Components/ComboBoxString.h"
#include "Models/StudyBotTypes.h"
#include "ContributionWidget.generated.h"

/**
 * UContributionWidget — GitHub 스타일 잔디(기여도) 위젯
 * ─────────────────────────────────────────────────────────
 * 365일(53주×7일) 타일 그리드를 UUniformGridPanel로 동적 생성합니다.
 *
 * Blueprint 바인딩:
 *   HeatmapGrid   — UUniformGridPanel (53열×7행)
 *   CbCategory    — UComboBoxString   (전체/카테고리 필터)
 *   TxtYear       — UTextBlock        (현재 연도)
 *   TxtSummary    — UTextBlock        ("총 N일, X% 정답")
 *
 * 색상 강도:
 *   ratio == 0   → #EBEDF0 (회색, 미학습)
 *   0  < r ≤ .25 → #9BE9A8 (연녹)
 *   .25< r ≤ .50 → #40C463 (중간)
 *   .50< r ≤ .75 → #30A14E (진초록)
 *   .75< r ≤ 1   → #216E39 (최진초록)
 */
UCLASS()
class UNREALSTUDYBOT_API UContributionWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    virtual void NativeConstruct() override;

    // 외부에서 데이터 주입 (CardSubsystem::OnHeatmapLoaded 바인딩)
    UFUNCTION(BlueprintCallable, Category="Heatmap")
    void SetHeatmapData(const TArray<FHeatmapEntry>& Entries);

    // 연도 / 카테고리 필터 변경
    UFUNCTION(BlueprintCallable, Category="Heatmap")
    void Refresh(int32 Year = 0, const FString& CategoryFilter = TEXT(""));

protected:
    UPROPERTY(meta=(BindWidget)) UUniformGridPanel* HeatmapGrid;
    UPROPERTY(meta=(BindWidget)) UComboBoxString*   CbCategory;
    UPROPERTY(meta=(BindWidget)) UTextBlock*        TxtYear;
    UPROPERTY(meta=(BindWidget)) UTextBlock*        TxtSummary;

private:
    void BuildGrid(const TArray<FHeatmapEntry>& Entries,
                   const FString& CategoryFilter);

    FLinearColor RatioToColor(float Ratio) const;

    // ratio → 격자 색상 (다크모드 친화적 초록 계열)
    static const TArray<FLinearColor> COLOR_STEPS;

    int32   m_currentYear = 0;
    FString m_currentCategory;
    TArray<FHeatmapEntry> m_allEntries;

    UFUNCTION()
    void OnCategoryChanged(FString SelectedItem, ESelectInfo::Type Type);
};
