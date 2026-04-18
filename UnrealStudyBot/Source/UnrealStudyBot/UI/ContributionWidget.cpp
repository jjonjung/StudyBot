#include "UI/ContributionWidget.h"
#include "Components/UniformGridSlot.h"
#include "Components/Image.h"
#include "Subsystem/CardSubsystem.h"
#include "Misc/DateTime.h"

// ratio 구간별 색상 (GitHub 잔디 팔레트)
const TArray<FLinearColor> UContributionWidget::COLOR_STEPS =
{
    FLinearColor(0.921f, 0.933f, 0.941f, 1.f), // 0   미학습  #EBEDF0
    FLinearColor(0.608f, 0.914f, 0.659f, 1.f), // .25 연녹    #9BE9A8
    FLinearColor(0.251f, 0.769f, 0.388f, 1.f), // .50 중간    #40C463
    FLinearColor(0.188f, 0.631f, 0.306f, 1.f), // .75 진초록  #30A14E
    FLinearColor(0.129f, 0.431f, 0.224f, 1.f), // 1   최진녹  #216E39
};

void UContributionWidget::NativeConstruct()
{
    Super::NativeConstruct();

    // 카테고리 필터 콤보박스 초기화
    if (CbCategory)
    {
        CbCategory->ClearOptions();
        for (auto& Cat : {TEXT("전체"), TEXT("Unreal"), TEXT("C++"),
                          TEXT("CS"), TEXT("Company"), TEXT("Algorithm")})
            CbCategory->AddOption(Cat);
        CbCategory->SetSelectedOption(TEXT("전체"));
        CbCategory->OnSelectionChanged.AddDynamic(this, &UContributionWidget::OnCategoryChanged);
    }

    // 연도 레이블
    m_currentYear = FDateTime::Now().GetYear();
    if (TxtYear)
        TxtYear->SetText(FText::FromString(FString::FromInt(m_currentYear)));

    // 잔디 데이터 요청
    if (auto* CS = GetGameInstance()
                       ->GetSubsystem<UCardSubsystem>())
    {
        CS->OnHeatmapLoaded.AddDynamic(this, &UContributionWidget::SetHeatmapData);
        CS->FetchHeatmap(m_currentYear);
    }
}

void UContributionWidget::SetHeatmapData(const TArray<FHeatmapEntry>& Entries)
{
    m_allEntries = Entries;
    BuildGrid(Entries, m_currentCategory);
}

void UContributionWidget::Refresh(int32 Year, const FString& CategoryFilter)
{
    m_currentYear     = (Year > 0) ? Year : FDateTime::Now().GetYear();
    m_currentCategory = CategoryFilter;

    if (TxtYear)
        TxtYear->SetText(FText::FromString(FString::FromInt(m_currentYear)));

    if (auto* CS = GetGameInstance()->GetSubsystem<UCardSubsystem>())
        CS->FetchHeatmap(m_currentYear);
}

void UContributionWidget::BuildGrid(
    const TArray<FHeatmapEntry>& Entries, const FString& CategoryFilter)
{
    if (!HeatmapGrid) return;
    HeatmapGrid->ClearChildren();

    // ── 날짜→Ratio 맵 구축 O(N) ──────────────────────────
    // 같은 날 여러 카테고리의 known_count, cards_done을 집계
    TMap<FString, TPair<int32,int32>> DayMap; // date → {known, done}
    for (const auto& E : Entries)
    {
        if (!CategoryFilter.IsEmpty() && CategoryFilter != TEXT("전체")
            && E.Category != CategoryFilter)
            continue;
        auto& Pair = DayMap.FindOrAdd(E.ScoreDate);
        Pair.Key   += E.KnownCount;
        Pair.Value += E.CardsDone;
    }

    // ── 53주 × 7일 그리드 생성 ────────────────────────────
    FDateTime StartDate(m_currentYear, 1, 1);
    // 첫 번째 일요일부터 시작
    int32 DayOfWeek = (int32)StartDate.GetDayOfWeek(); // 0=Mon ... 6=Sun
    int32 Offset    = (DayOfWeek == 6) ? 0 : (DayOfWeek + 1);
    StartDate -= FTimespan::FromDays(Offset);

    int32  TotalKnown = 0, TotalDone = 0;
    int32  ActiveDays = 0;

    for (int32 Week = 0; Week < 53; ++Week)
    {
        for (int32 Day = 0; Day < 7; ++Day)
        {
            FDateTime CurDate = StartDate + FTimespan::FromDays(Week * 7 + Day);
            FString   DateStr = FString::Printf(TEXT("%04d-%02d-%02d"),
                CurDate.GetYear(), CurDate.GetMonth(), CurDate.GetDay());

            float Ratio = 0.f;
            if (auto* Pair = DayMap.Find(DateStr))
            {
                TotalKnown += Pair->Key;
                TotalDone  += Pair->Value;
                if (Pair->Value > 0)
                {
                    Ratio = (float)Pair->Key / Pair->Value;
                    ActiveDays++;
                }
            }

            // 타일 Image 생성
            UImage* Tile = NewObject<UImage>(this);
            Tile->SetColorAndOpacity(RatioToColor(Ratio));

            auto* TileSlot = HeatmapGrid->AddChildToUniformGrid(Tile, Day, Week);
            if (TileSlot)
            {
                TileSlot->SetHorizontalAlignment(HAlign_Fill);
                TileSlot->SetVerticalAlignment(VAlign_Fill);
            }
        }
    }

    // ── 요약 텍스트 ───────────────────────────────────────
    if (TxtSummary)
    {
        float Pct = TotalDone > 0 ? (float)TotalKnown / TotalDone * 100.f : 0.f;
        TxtSummary->SetText(FText::FromString(
            FString::Printf(TEXT("활동 %d일  |  총 %d/%d문제  (%.1f%%)"),
                ActiveDays, TotalKnown, TotalDone, Pct)));
    }
}

FLinearColor UContributionWidget::RatioToColor(float Ratio) const
{
    if (Ratio <= 0.f)   return COLOR_STEPS[0];
    if (Ratio <= 0.25f) return COLOR_STEPS[1];
    if (Ratio <= 0.50f) return COLOR_STEPS[2];
    if (Ratio <= 0.75f) return COLOR_STEPS[3];
    return COLOR_STEPS[4];
}

void UContributionWidget::OnCategoryChanged(FString SelectedItem, ESelectInfo::Type)
{
    m_currentCategory = (SelectedItem == TEXT("전체")) ? TEXT("") : SelectedItem;
    BuildGrid(m_allEntries, m_currentCategory);
}
