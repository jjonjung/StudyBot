#include "UI/LobbyRoomWidget.h"
#include "Subsystem/LobbySubsystem.h"
#include "Subsystem/CardSubsystem.h"
#include "UnrealStudyBotGameInstance.h"
#include "Components/TextBlock.h"
#include "Blueprint/WidgetTree.h"

void ULobbyRoomWidget::NativeConstruct()
{
    Super::NativeConstruct();

    BtnSend ->OnClicked.AddDynamic(this, &ULobbyRoomWidget::OnSendClicked);
    BtnStart->OnClicked.AddDynamic(this, &ULobbyRoomWidget::OnStartClicked);
    BtnLeave->OnClicked.AddDynamic(this, &ULobbyRoomWidget::OnLeaveClicked);

    auto* GI = Cast<UStudyBotGameInstance>(GetGameInstance());
    if (!GI) return;

    // 로비 기본 정보 표시
    const FLobbyInfo& Info = GI->GetLobbyInfo();
    TxtLobbyName->SetText(FText::FromString(Info.Name));
    TxtLobbyCode->SetText(FText::FromString(Info.Code));
    TxtCategory ->SetText(FText::FromString(Info.Category));

    // Host만 BtnStart 표시
    BtnStart->SetVisibility(
        GI->IsHost() ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);

    // LobbySubsystem 이벤트 바인딩
    if (auto* Lobby = GetGameInstance()->GetSubsystem<ULobbySubsystem>())
    {
        Lobby->OnMemberJoined      .AddDynamic(this, &ULobbyRoomWidget::HandleMemberJoined);
        Lobby->OnMemberLeft        .AddDynamic(this, &ULobbyRoomWidget::HandleMemberLeft);
        Lobby->OnMemberKicked      .AddDynamic(this, &ULobbyRoomWidget::HandleMemberKicked);
        Lobby->OnChatReceived      .AddDynamic(this, &ULobbyRoomWidget::HandleChatReceived);
        Lobby->OnLobbyStarted      .AddDynamic(this, &ULobbyRoomWidget::HandleLobbyStarted);
        Lobby->OnCategoryChanged   .AddDynamic(this, &ULobbyRoomWidget::HandleCategoryChanged);
        Lobby->OnLobbyError        .AddDynamic(this, &ULobbyRoomWidget::HandleLobbyError);
        Lobby->OnMemberReadyChanged.AddDynamic(this, &ULobbyRoomWidget::HandleMemberReadyChanged);
    }

    RefreshMemberList();
}

void ULobbyRoomWidget::NativeDestruct()
{
    if (auto* Lobby = GetGameInstance()->GetSubsystem<ULobbySubsystem>())
    {
        Lobby->OnMemberJoined      .RemoveDynamic(this, &ULobbyRoomWidget::HandleMemberJoined);
        Lobby->OnMemberLeft        .RemoveDynamic(this, &ULobbyRoomWidget::HandleMemberLeft);
        Lobby->OnMemberKicked      .RemoveDynamic(this, &ULobbyRoomWidget::HandleMemberKicked);
        Lobby->OnChatReceived      .RemoveDynamic(this, &ULobbyRoomWidget::HandleChatReceived);
        Lobby->OnLobbyStarted      .RemoveDynamic(this, &ULobbyRoomWidget::HandleLobbyStarted);
        Lobby->OnCategoryChanged   .RemoveDynamic(this, &ULobbyRoomWidget::HandleCategoryChanged);
        Lobby->OnLobbyError        .RemoveDynamic(this, &ULobbyRoomWidget::HandleLobbyError);
        Lobby->OnMemberReadyChanged.RemoveDynamic(this, &ULobbyRoomWidget::HandleMemberReadyChanged);
    }
    Super::NativeDestruct();
}

// ── 버튼 핸들러 ───────────────────────────────────────────

void ULobbyRoomWidget::OnSendClicked()
{
    FString Msg = InputChat->GetText().ToString().TrimStartAndEnd();
    if (Msg.IsEmpty()) return;

    if (auto* Lobby = GetGameInstance()->GetSubsystem<ULobbySubsystem>())
        Lobby->SendChatMessage(Msg);

    InputChat->SetText(FText::GetEmpty());
}

void ULobbyRoomWidget::OnStartClicked()
{
    auto* GI = Cast<UStudyBotGameInstance>(GetGameInstance());
    if (!GI || !GI->IsHost()) return;

    SetMessage(TEXT("인터뷰 시작 중..."));
    BtnStart->SetIsEnabled(false);

    if (auto* Lobby = GetGameInstance()->GetSubsystem<ULobbySubsystem>())
        Lobby->SendStartInterview();
}

void ULobbyRoomWidget::OnLeaveClicked()
{
    if (auto* Lobby = GetGameInstance()->GetSubsystem<ULobbySubsystem>())
        Lobby->LeaveLobby();
}

// ── LobbySubsystem 이벤트 핸들러 ─────────────────────────

void ULobbyRoomWidget::HandleMemberJoined(const FLobbyMember& Member)
{
    RefreshMemberList();
    AppendChatMessage(TEXT("시스템"),
        FString::Printf(TEXT("%s 님이 입장했습니다."), *Member.Nickname));
}

void ULobbyRoomWidget::HandleMemberLeft(int32 UserId)
{
    RefreshMemberList();
}

void ULobbyRoomWidget::HandleMemberKicked(int32 UserId)
{
    RefreshMemberList();
}

void ULobbyRoomWidget::HandleChatReceived(const FString& Nickname, const FString& Message)
{
    AppendChatMessage(Nickname, Message);
}

void ULobbyRoomWidget::HandleLobbyStarted(const FString& Category, int32 CardCount)
{
    // 카드 로드 → InterviewMap 이동
    auto* GI = Cast<UStudyBotGameInstance>(GetGameInstance());
    if (!GI) return;

    // ECardCategory로 변환
    ECardCategory Cat = ECardCategory::Unreal;
    if      (Category == TEXT("C++"))       Cat = ECardCategory::Cpp;
    else if (Category == TEXT("CS"))        Cat = ECardCategory::CS;
    else if (Category == TEXT("Company"))   Cat = ECardCategory::Company;
    else if (Category == TEXT("Algorithm")) Cat = ECardCategory::Algorithm;

    if (auto* Cards = GetGameInstance()->GetSubsystem<UCardSubsystem>())
        Cards->FetchInterviewCards(Cat, CardCount);

    GI->OpenInterviewMap(Cat);
}

void ULobbyRoomWidget::HandleCategoryChanged(const FString& Category)
{
    TxtCategory->SetText(FText::FromString(Category));
}

void ULobbyRoomWidget::HandleLobbyError(int32 Code, const FString& Message)
{
    SetMessage(Message, true);
    BtnStart->SetIsEnabled(true);
}

void ULobbyRoomWidget::HandleMemberReadyChanged(int32 UserId)
{
    RefreshMemberList();
}

// ── UI 갱신 ───────────────────────────────────────────────

void ULobbyRoomWidget::RefreshMemberList()
{
    MemberList->ClearChildren();

    auto* Lobby = GetGameInstance()->GetSubsystem<ULobbySubsystem>();
    if (!Lobby) return;

    TArray<FLobbyMember> Members = Lobby->GetSortedMembers();
    for (const FLobbyMember& M : Members)
    {
        // MemberRowClass가 지정된 경우 Blueprint 위젯 사용,
        // 아니면 TextBlock 폴백으로 닉네임만 표시
        if (MemberRowClass)
        {
            UUserWidget* Row = CreateWidget<UUserWidget>(this, MemberRowClass);
            if (Row) MemberList->AddChild(Row);
        }
        else
        {
            FString Label = M.Nickname;
            if (M.IsHost())   Label += TEXT(" [호스트]");
            if (M.bIsReady)   Label += TEXT(" ✓");

            UTextBlock* Txt = WidgetTree->ConstructWidget<UTextBlock>();
            Txt->SetText(FText::FromString(Label));
            MemberList->AddChild(Txt);
        }
    }
}

void ULobbyRoomWidget::AppendChatMessage(const FString& Nickname, const FString& Message)
{
    FString Line = FString::Printf(TEXT("[%s] %s"), *Nickname, *Message);

    UTextBlock* Txt = WidgetTree->ConstructWidget<UTextBlock>();
    Txt->SetText(FText::FromString(Line));
    Txt->SetAutoWrapText(true);
    ChatScroll->AddChild(Txt);
    ChatScroll->ScrollToEnd();
}

void ULobbyRoomWidget::SetMessage(const FString& Msg, bool bError)
{
    TxtMessage->SetText(FText::FromString(Msg));
    TxtMessage->SetColorAndOpacity(FSlateColor(
        bError ? FLinearColor(1.f, 0.2f, 0.2f) : FLinearColor(0.8f, 0.8f, 0.8f)
    ));
}
