#include "Subsystem/CardSubsystem.h"
#include "UnrealStudyBotGameInstance.h"
#include "HttpModule.h"
#include "Interfaces/IHttpResponse.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

// ── 헬퍼 ─────────────────────────────────────────────────

FString UCardSubsystem::MakeAuthHeader() const
{
    auto* GI = Cast<UStudyBotGameInstance>(GetGameInstance());
    if (!GI) return {};
    return TEXT("Bearer ") + GI->GetAuthInfo().Token;
}

FString UCardSubsystem::CategoryToString(ECardCategory Cat) const
{
    switch (Cat)
    {
    case ECardCategory::Unreal:    return TEXT("Unreal");
    case ECardCategory::Cpp:       return TEXT("C++");
    case ECardCategory::CS:        return TEXT("CS");
    case ECardCategory::Company:   return TEXT("Company");
    case ECardCategory::Algorithm: return TEXT("Algorithm");
    default:                       return TEXT("");
    }
}

// ── FetchCards ────────────────────────────────────────────

void UCardSubsystem::FetchCards(ECardCategory Category)
{
    auto* GI = Cast<UStudyBotGameInstance>(GetGameInstance());
    if (!GI) return;

    FString Url = GI->GetBaseUrl() + TEXT("/api/cards?limit=100");
    FString CatStr = CategoryToString(Category);
    if (!CatStr.IsEmpty())
        Url += TEXT("&category=") + FGenericPlatformHttp::UrlEncode(CatStr);

    auto Req = FHttpModule::Get().CreateRequest();
    Req->SetURL(Url);
    Req->SetVerb(TEXT("GET"));
    Req->SetHeader(TEXT("Authorization"), MakeAuthHeader());
    Req->OnProcessRequestComplete().BindUObject(this, &UCardSubsystem::OnFetchResponse);
    Req->ProcessRequest();
}

// ── FetchInterviewCards ───────────────────────────────────

void UCardSubsystem::FetchInterviewCards(ECardCategory Category, int32 Count)
{
    auto* GI = Cast<UStudyBotGameInstance>(GetGameInstance());
    if (!GI) return;

    FString Url = GI->GetBaseUrl()
        + FString::Printf(TEXT("/api/cards/interview?count=%d"), Count);

    FString CatStr = CategoryToString(Category);
    if (!CatStr.IsEmpty())
        Url += TEXT("&category=") + FGenericPlatformHttp::UrlEncode(CatStr);

    auto Req = FHttpModule::Get().CreateRequest();
    Req->SetURL(Url);
    Req->SetVerb(TEXT("GET"));
    Req->SetHeader(TEXT("Authorization"), MakeAuthHeader());
    Req->OnProcessRequestComplete().BindUObject(this, &UCardSubsystem::OnFetchResponse);
    Req->ProcessRequest();
}

void UCardSubsystem::OnFetchResponse(FHttpRequestPtr, FHttpResponsePtr Res, bool bOk)
{
    if (!bOk || !Res.IsValid() || Res->GetResponseCode() != 200)
    {
        OnError.Broadcast(false, TEXT("카드 로딩 실패"));
        return;
    }
    m_cachedCards = ParseCards(Res->GetContentAsString());
    OnCardsLoaded.Broadcast(m_cachedCards);
}

TArray<FFlashCard> UCardSubsystem::ParseCards(const FString& JsonStr)
{
    TArray<FFlashCard> Result;
    TArray<TSharedPtr<FJsonValue>> Arr;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonStr);
    if (!FJsonSerializer::Deserialize(Reader, Arr)) return Result;

    for (auto& Val : Arr)
    {
        auto Obj = Val->AsObject();
        if (!Obj.IsValid()) continue;

        FFlashCard Card;
        Card.Id         = (int32)Obj->GetNumberField(TEXT("id"));
        Card.Category   = Obj->GetStringField(TEXT("category"));
        Obj->TryGetStringField(TEXT("company"), Card.Company);
        Card.Question   = Obj->GetStringField(TEXT("question"));
        Card.Answer     = Obj->GetStringField(TEXT("answer"));
        Card.Difficulty = Obj->GetStringField(TEXT("difficulty"));

        // 알고리즘 전용 필드 (NULL이면 TryGet이 기본값 유지)
        Obj->TryGetStringField(TEXT("core_conditions"),  Card.CoreConditions);
        Obj->TryGetStringField(TEXT("selection_reason"), Card.SelectionReason);
        Obj->TryGetStringField(TEXT("code_cpp"),         Card.CodeCpp);
        Obj->TryGetStringField(TEXT("code_csharp"),      Card.CodeCSharp);
        Obj->TryGetStringField(TEXT("time_complexity"),  Card.TimeComplexity);

        Result.Add(Card);
    }
    return Result;
}

// ── FetchHeatmap ──────────────────────────────────────────

void UCardSubsystem::FetchHeatmap(int32 Year)
{
    auto* GI = Cast<UStudyBotGameInstance>(GetGameInstance());
    if (!GI) return;

    FString Url = GI->GetBaseUrl() + TEXT("/api/progress/heatmap");
    if (Year > 0)
        Url += FString::Printf(TEXT("?year=%d"), Year);

    auto Req = FHttpModule::Get().CreateRequest();
    Req->SetURL(Url);
    Req->SetVerb(TEXT("GET"));
    Req->SetHeader(TEXT("Authorization"), MakeAuthHeader());
    Req->OnProcessRequestComplete().BindUObject(this, &UCardSubsystem::OnHeatmapResponse);
    Req->ProcessRequest();
}

void UCardSubsystem::OnHeatmapResponse(FHttpRequestPtr, FHttpResponsePtr Res, bool bOk)
{
    if (!bOk || !Res.IsValid() || Res->GetResponseCode() != 200)
    {
        OnError.Broadcast(false, TEXT("잔디 데이터 로딩 실패"));
        return;
    }
    OnHeatmapLoaded.Broadcast(ParseHeatmap(Res->GetContentAsString()));
}

TArray<FHeatmapEntry> UCardSubsystem::ParseHeatmap(const FString& JsonStr)
{
    TArray<FHeatmapEntry> Result;
    TArray<TSharedPtr<FJsonValue>> Arr;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonStr);
    if (!FJsonSerializer::Deserialize(Reader, Arr)) return Result;

    for (auto& Val : Arr)
    {
        auto Obj = Val->AsObject();
        if (!Obj.IsValid()) continue;

        FHeatmapEntry Entry;
        Entry.ScoreDate  = Obj->GetStringField(TEXT("score_date"));
        Entry.Category   = Obj->GetStringField(TEXT("category"));
        Entry.CardsDone  = (int32)Obj->GetNumberField(TEXT("cards_done"));
        Entry.KnownCount = (int32)Obj->GetNumberField(TEXT("known_count"));
        Entry.Ratio      = (float)Obj->GetNumberField(TEXT("ratio"));
        Result.Add(Entry);
    }
    return Result;
}

// ── SaveProgress ──────────────────────────────────────────

void UCardSubsystem::SaveProgress(int32 CardId, bool bKnown, int32 Score)
{
    auto* GI = Cast<UStudyBotGameInstance>(GetGameInstance());
    if (!GI) return;

    TSharedRef<FJsonObject> Body = MakeShared<FJsonObject>();
    Body->SetBoolField  (TEXT("known"), bKnown);
    Body->SetNumberField(TEXT("score"), Score);

    FString JsonStr;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&JsonStr);
    FJsonSerializer::Serialize(Body, Writer);

    auto Req = FHttpModule::Get().CreateRequest();
    Req->SetURL(GI->GetBaseUrl() + FString::Printf(TEXT("/api/progress/%d"), CardId));
    Req->SetVerb(TEXT("PUT"));
    Req->SetHeader(TEXT("Content-Type"),  TEXT("application/json"));
    Req->SetHeader(TEXT("Authorization"), MakeAuthHeader());
    Req->SetContentAsString(JsonStr);
    Req->OnProcessRequestComplete().BindUObject(this, &UCardSubsystem::OnProgressResponse);
    Req->ProcessRequest();
}

void UCardSubsystem::SaveInterviewSession(const FInterviewResult& Result)
{
    auto* GI = Cast<UStudyBotGameInstance>(GetGameInstance());
    if (!GI) return;

    TSharedRef<FJsonObject> Body = MakeShared<FJsonObject>();
    Body->SetStringField(TEXT("category"),    Result.Category);
    Body->SetNumberField(TEXT("total_cards"), Result.TotalCards);
    Body->SetNumberField(TEXT("known_count"), Result.KnownCount);

    FString JsonStr;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&JsonStr);
    FJsonSerializer::Serialize(Body, Writer);

    auto Req = FHttpModule::Get().CreateRequest();
    Req->SetURL(GI->GetBaseUrl() + TEXT("/api/progress/session"));
    Req->SetVerb(TEXT("POST"));
    Req->SetHeader(TEXT("Content-Type"),  TEXT("application/json"));
    Req->SetHeader(TEXT("Authorization"), MakeAuthHeader());
    Req->SetContentAsString(JsonStr);
    Req->ProcessRequest(); // fire-and-forget
}

void UCardSubsystem::OnProgressResponse(FHttpRequestPtr, FHttpResponsePtr Res, bool bOk)
{
    if (bOk && Res.IsValid() && Res->GetResponseCode() == 200)
        OnProgressSaved.Broadcast();
}
