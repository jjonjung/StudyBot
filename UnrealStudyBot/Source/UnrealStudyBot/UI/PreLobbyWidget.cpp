#include "UI/PreLobbyWidget.h"
#include "UnrealStudyBotGameInstance.h"

void UPreLobbyWidget::NativeConstruct()
{
    Super::NativeConstruct();

    BtnCreate->OnClicked.AddDynamic(this, &UPreLobbyWidget::OnCreateClicked);
    BtnJoin  ->OnClicked.AddDynamic(this, &UPreLobbyWidget::OnJoinClicked);
    BtnLogout->OnClicked.AddDynamic(this, &UPreLobbyWidget::OnLogoutClicked);

    // 닉네임 표시
    if (auto* GI = Cast<UStudyBotGameInstance>(GetGameInstance()))
        TxtNickname->SetText(FText::FromString(GI->GetAuthInfo().Nickname));

    CollapseSubWidgets();
}

void UPreLobbyWidget::OnCreateClicked()
{
    CollapseSubWidgets();
    if (WgtCreate)
        WgtCreate->SetVisibility(ESlateVisibility::Visible);
}

void UPreLobbyWidget::OnJoinClicked()
{
    CollapseSubWidgets();
    if (WgtJoin)
        WgtJoin->SetVisibility(ESlateVisibility::Visible);
}

void UPreLobbyWidget::OnLogoutClicked()
{
    if (auto* GI = Cast<UStudyBotGameInstance>(GetGameInstance()))
        GI->Logout();
}

void UPreLobbyWidget::CollapseSubWidgets()
{
    if (WgtCreate) WgtCreate->SetVisibility(ESlateVisibility::Collapsed);
    if (WgtJoin)   WgtJoin  ->SetVisibility(ESlateVisibility::Collapsed);
}
