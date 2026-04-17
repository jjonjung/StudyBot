#include "UI/JoinLobbyWidget.h"
#include "Subsystem/LobbySubsystem.h"
#include "UnrealStudyBotGameInstance.h"

void UJoinLobbyWidget::NativeConstruct()
{
    Super::NativeConstruct();

    BtnJoin->OnClicked.AddDynamic(this, &UJoinLobbyWidget::OnJoinClicked);
    BtnBack->OnClicked.AddDynamic(this, &UJoinLobbyWidget::OnBackClicked);

    if (auto* Lobby = GetGameInstance()->GetSubsystem<ULobbySubsystem>())
    {
        Lobby->OnLobbyJoined.AddDynamic(this, &UJoinLobbyWidget::HandleLobbyJoined);
        Lobby->OnLobbyError .AddDynamic(this, &UJoinLobbyWidget::HandleLobbyError);
    }
}

void UJoinLobbyWidget::NativeDestruct()
{
    if (auto* Lobby = GetGameInstance()->GetSubsystem<ULobbySubsystem>())
    {
        Lobby->OnLobbyJoined.RemoveDynamic(this, &UJoinLobbyWidget::HandleLobbyJoined);
        Lobby->OnLobbyError .RemoveDynamic(this, &UJoinLobbyWidget::HandleLobbyError);
    }
    Super::NativeDestruct();
}

void UJoinLobbyWidget::OnJoinClicked()
{
    FString Code = InputCode->GetText().ToString().TrimStartAndEnd().ToUpper();
    if (Code.IsEmpty())
    {
        SetMessage(TEXT("입장 코드를 입력해 주세요."), true);
        return;
    }
    if (Code.Len() != 6)
    {
        SetMessage(TEXT("코드는 6자리여야 합니다."), true);
        return;
    }

    SetMessage(TEXT("입장 중..."));
    SetButtonsEnabled(false);

    if (auto* Lobby = GetGameInstance()->GetSubsystem<ULobbySubsystem>())
        Lobby->JoinLobbyByCode(Code);
}

void UJoinLobbyWidget::OnBackClicked()
{
    SetVisibility(ESlateVisibility::Collapsed);
}

void UJoinLobbyWidget::HandleLobbyJoined(const FLobbyInfo& Info)
{
    SetButtonsEnabled(true);
    if (auto* GI = Cast<UStudyBotGameInstance>(GetGameInstance()))
        GI->OpenLobbyRoomMap();
}

void UJoinLobbyWidget::HandleLobbyError(int32 Code, const FString& Message)
{
    SetButtonsEnabled(true);
    SetMessage(Message, true);
}

void UJoinLobbyWidget::SetButtonsEnabled(bool bEnabled)
{
    BtnJoin->SetIsEnabled(bEnabled);
    BtnBack->SetIsEnabled(bEnabled);
}

void UJoinLobbyWidget::SetMessage(const FString& Msg, bool bError)
{
    TxtMessage->SetText(FText::FromString(Msg));
    TxtMessage->SetColorAndOpacity(FSlateColor(
        bError ? FLinearColor(1.f, 0.2f, 0.2f) : FLinearColor(0.8f, 0.8f, 0.8f)
    ));
}
