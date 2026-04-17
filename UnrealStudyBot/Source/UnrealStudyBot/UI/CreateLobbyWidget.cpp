#include "UI/CreateLobbyWidget.h"
#include "Subsystem/LobbySubsystem.h"
#include "UnrealStudyBotGameInstance.h"

void UCreateLobbyWidget::NativeConstruct()
{
    Super::NativeConstruct();

    BtnCreate->OnClicked.AddDynamic(this, &UCreateLobbyWidget::OnCreateClicked);
    BtnBack  ->OnClicked.AddDynamic(this, &UCreateLobbyWidget::OnBackClicked);

    // 카테고리 선택지 초기화
    CbCategory->ClearOptions();
    for (const FString& Cat : { TEXT("Unreal"), TEXT("C++"), TEXT("CS"),
                                 TEXT("Company"), TEXT("Algorithm") })
        CbCategory->AddOption(Cat);
    CbCategory->SetSelectedIndex(0);

    // 최대 인원 선택지 초기화
    CbMaxMembers->ClearOptions();
    for (int32 i = 2; i <= 6; ++i)
        CbMaxMembers->AddOption(FString::FromInt(i));
    CbMaxMembers->SetSelectedIndex(2); // 기본 4명

    if (auto* Lobby = GetGameInstance()->GetSubsystem<ULobbySubsystem>())
    {
        Lobby->OnLobbyCreated.AddDynamic(this, &UCreateLobbyWidget::HandleLobbyCreated);
        Lobby->OnLobbyError  .AddDynamic(this, &UCreateLobbyWidget::HandleLobbyError);
    }
}

void UCreateLobbyWidget::NativeDestruct()
{
    if (auto* Lobby = GetGameInstance()->GetSubsystem<ULobbySubsystem>())
    {
        Lobby->OnLobbyCreated.RemoveDynamic(this, &UCreateLobbyWidget::HandleLobbyCreated);
        Lobby->OnLobbyError  .RemoveDynamic(this, &UCreateLobbyWidget::HandleLobbyError);
    }
    Super::NativeDestruct();
}

void UCreateLobbyWidget::OnCreateClicked()
{
    FString Name = InputName->GetText().ToString().TrimStartAndEnd();
    if (Name.IsEmpty())
    {
        SetMessage(TEXT("방 이름을 입력해 주세요."), true);
        return;
    }

    FString Category   = CbCategory  ->GetSelectedOption();
    int32   MaxMembers = FCString::Atoi(*CbMaxMembers->GetSelectedOption());

    SetMessage(TEXT("방 생성 중..."));
    SetButtonsEnabled(false);

    if (auto* Lobby = GetGameInstance()->GetSubsystem<ULobbySubsystem>())
        Lobby->CreateLobby(Name, Category, MaxMembers);
}

void UCreateLobbyWidget::OnBackClicked()
{
    SetVisibility(ESlateVisibility::Collapsed);
}

void UCreateLobbyWidget::HandleLobbyCreated(const FLobbyInfo& Info)
{
    SetButtonsEnabled(true);
    if (auto* GI = Cast<UStudyBotGameInstance>(GetGameInstance()))
        GI->OpenLobbyRoomMap();
}

void UCreateLobbyWidget::HandleLobbyError(int32 Code, const FString& Message)
{
    SetButtonsEnabled(true);
    SetMessage(Message, true);
}

void UCreateLobbyWidget::SetButtonsEnabled(bool bEnabled)
{
    BtnCreate->SetIsEnabled(bEnabled);
    BtnBack  ->SetIsEnabled(bEnabled);
}

void UCreateLobbyWidget::SetMessage(const FString& Msg, bool bError)
{
    TxtMessage->SetText(FText::FromString(Msg));
    TxtMessage->SetColorAndOpacity(FSlateColor(
        bError ? FLinearColor(1.f, 0.2f, 0.2f) : FLinearColor(0.8f, 0.8f, 0.8f)
    ));
}
