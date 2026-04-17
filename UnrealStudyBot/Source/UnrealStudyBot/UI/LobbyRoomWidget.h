#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/EditableTextBox.h"
#include "Components/ScrollBox.h"
#include "Components/VerticalBox.h"
#include "Models/StudyBotTypes.h"
#include "LobbyRoomWidget.generated.h"

/**
 * ULobbyRoomWidget — 로비 대기실 화면
 * ─────────────────────────────────────────────────────────
 * 멤버 목록 / 채팅 / 인터뷰 시작(Host 전용) / 나가기.
 *
 * Blueprint 바인딩:
 *   TxtLobbyName    — 방 이름
 *   TxtCategory     — 현재 카테고리
 *   TxtLobbyCode    — 입장 코드 표시
 *   MemberList      — VerticalBox (멤버 행을 동적 추가)
 *   ChatScroll      — ScrollBox (채팅 메시지 스크롤)
 *   InputChat       — EditableTextBox (채팅 입력)
 *   BtnSend         — 채팅 전송
 *   BtnStart        — 인터뷰 시작 (Host 전용, 기본 Collapsed)
 *   BtnLeave        — 로비 나가기
 *   TxtMessage      — 시스템 메시지 표시
 *
 * 멤버 행 Template:
 *   WBP_MemberRow (Blueprint) 또는 TextBlock 동적 생성으로 대체 가능.
 *   RefreshMemberList() 에서 MemberList를 매번 Clear + Re-fill.
 */
UCLASS()
class UNREALSTUDYBOT_API ULobbyRoomWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;

    // ── UMG 바인딩 ────────────────────────────────────────
    UPROPERTY(meta=(BindWidget)) UTextBlock*       TxtLobbyName;
    UPROPERTY(meta=(BindWidget)) UTextBlock*       TxtCategory;
    UPROPERTY(meta=(BindWidget)) UTextBlock*       TxtLobbyCode;
    UPROPERTY(meta=(BindWidget)) UVerticalBox*     MemberList;
    UPROPERTY(meta=(BindWidget)) UScrollBox*       ChatScroll;
    UPROPERTY(meta=(BindWidget)) UEditableTextBox* InputChat;
    UPROPERTY(meta=(BindWidget)) UButton*          BtnSend;
    UPROPERTY(meta=(BindWidget)) UButton*          BtnStart;
    UPROPERTY(meta=(BindWidget)) UButton*          BtnLeave;
    UPROPERTY(meta=(BindWidget)) UTextBlock*       TxtMessage;

    /** 멤버 행 Blueprint 위젯 클래스 (디자이너가 할당) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Lobby")
    TSubclassOf<UUserWidget> MemberRowClass;

private:
    // ── 버튼 핸들러 ──────────────────────────────────────
    UFUNCTION() void OnSendClicked();
    UFUNCTION() void OnStartClicked();
    UFUNCTION() void OnLeaveClicked();

    // ── LobbySubsystem 이벤트 핸들러 ─────────────────────
    UFUNCTION() void HandleMemberJoined    (const FLobbyMember& Member);
    UFUNCTION() void HandleMemberLeft      (int32 UserId);
    UFUNCTION() void HandleMemberKicked    (int32 UserId);
    UFUNCTION() void HandleChatReceived    (const FString& Nickname, const FString& Message);
    UFUNCTION() void HandleLobbyStarted   (const FString& Category, int32 CardCount);
    UFUNCTION() void HandleCategoryChanged (const FString& Category);
    UFUNCTION() void HandleLobbyError      (int32 Code, const FString& Message);
    UFUNCTION() void HandleMemberReadyChanged(int32 UserId);

    // ── UI 갱신 ──────────────────────────────────────────
    void RefreshMemberList();
    void AppendChatMessage(const FString& Nickname, const FString& Message);
    void SetMessage       (const FString& Msg, bool bError = false);
};
