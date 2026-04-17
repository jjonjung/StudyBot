#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "PreLobbyWidget.generated.h"

/**
 * UPreLobbyWidget — 로비 진입 허브
 * ─────────────────────────────────────────────────────────
 * 로그인 후 첫 화면. 방 만들기 / 코드로 입장 / 로그아웃.
 *
 * Blueprint 바인딩:
 *   TxtNickname   — 로그인 닉네임 표시
 *   BtnCreate     — 방 만들기 (CreateLobbyWidget 표시)
 *   BtnJoin       — 코드 입장 (JoinLobbyWidget 표시)
 *   BtnLogout     — 로그아웃
 *   WgtCreate     — UCreateLobbyWidget (기본 Collapsed)
 *   WgtJoin       — UJoinLobbyWidget   (기본 Collapsed)
 */
UCLASS()
class UNREALSTUDYBOT_API UPreLobbyWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    virtual void NativeConstruct() override;

    // ── UMG 바인딩 ────────────────────────────────────────
    UPROPERTY(meta=(BindWidget)) UTextBlock* TxtNickname;
    UPROPERTY(meta=(BindWidget)) UButton*    BtnCreate;
    UPROPERTY(meta=(BindWidget)) UButton*    BtnJoin;
    UPROPERTY(meta=(BindWidget)) UButton*    BtnLogout;

    // 자식 위젯 패널 (Blueprint에서 오버레이 등에 배치)
    UPROPERTY(meta=(BindWidgetOptional)) UUserWidget* WgtCreate;
    UPROPERTY(meta=(BindWidgetOptional)) UUserWidget* WgtJoin;

private:
    UFUNCTION() void OnCreateClicked();
    UFUNCTION() void OnJoinClicked();
    UFUNCTION() void OnLogoutClicked();

    void CollapseSubWidgets();
};
