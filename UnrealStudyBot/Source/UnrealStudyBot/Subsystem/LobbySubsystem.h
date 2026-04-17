#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Http.h"
#include "IWebSocket.h"
#include "Models/StudyBotTypes.h"
#include "LobbySubsystem.generated.h"

// ── 이벤트 선언 ──────────────────────────────────────────────
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam (FOnLobbyCreated,       const FLobbyInfo&,   Info);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam (FOnLobbyJoined,        const FLobbyInfo&,   Info);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnLobbyError,         int32, Code, const FString&, Message);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam (FOnMemberJoined,       const FLobbyMember&, Member);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam (FOnMemberLeft,         int32, UserId);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam (FOnMemberKicked,       int32, UserId);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnChatReceived,       const FString&, Nickname, const FString&, Message);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnLobbyStarted,       const FString&, Category, int32, CardCount);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam (FOnCategoryChanged,    const FString&, Category);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam (FOnMemberReadyChanged, int32, UserId);

/**
 * ULobbySubsystem
 * ─────────────────────────────────────────────────────────
 * 로비 관련 REST API 호출 및 WebSocket 연결을 담당합니다.
 * GameInstanceSubsystem이므로 맵 전환 후에도 WebSocket이 유지됩니다.
 *
 * 이벤트 디스패치:
 *   TMap<FString, FWsHandler> m_wsHandlers 로 O(1) 라우팅.
 *   서버 이벤트명 → 전용 Handle* 메서드.
 *
 * 멤버 캐시:
 *   TMap<int32, FLobbyMember> m_memberMap (UserId → Member)
 *   GetSortedMembers()로 Host 우선 정렬된 배열 반환.
 */
UCLASS()
class UNREALSTUDYBOT_API ULobbySubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    virtual void Initialize  (FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    // ── 이벤트 ────────────────────────────────────────────
    UPROPERTY(BlueprintAssignable) FOnLobbyCreated       OnLobbyCreated;
    UPROPERTY(BlueprintAssignable) FOnLobbyJoined        OnLobbyJoined;
    UPROPERTY(BlueprintAssignable) FOnLobbyError         OnLobbyError;
    UPROPERTY(BlueprintAssignable) FOnMemberJoined       OnMemberJoined;
    UPROPERTY(BlueprintAssignable) FOnMemberLeft         OnMemberLeft;
    UPROPERTY(BlueprintAssignable) FOnMemberKicked       OnMemberKicked;
    UPROPERTY(BlueprintAssignable) FOnChatReceived       OnChatReceived;
    UPROPERTY(BlueprintAssignable) FOnLobbyStarted       OnLobbyStarted;
    UPROPERTY(BlueprintAssignable) FOnCategoryChanged    OnCategoryChanged;
    UPROPERTY(BlueprintAssignable) FOnMemberReadyChanged OnMemberReadyChanged;

    // ── REST API ──────────────────────────────────────────
    UFUNCTION(BlueprintCallable, Category="Lobby")
    void CreateLobby(const FString& Name, const FString& Category,
                     int32 MaxMembers = 4);

    UFUNCTION(BlueprintCallable, Category="Lobby")
    void JoinLobbyByCode(const FString& Code);

    UFUNCTION(BlueprintCallable, Category="Lobby")
    void KickMember(int32 TargetUserId);

    UFUNCTION(BlueprintCallable, Category="Lobby")
    void LeaveLobby();

    UFUNCTION(BlueprintCallable, Category="Lobby")
    void CloseLobby();

    // ── WebSocket 이벤트 전송 ─────────────────────────────
    UFUNCTION(BlueprintCallable, Category="Lobby")
    void SendChatMessage(const FString& Message);

    UFUNCTION(BlueprintCallable, Category="Lobby")
    void SendSetCategory(const FString& Category);

    UFUNCTION(BlueprintCallable, Category="Lobby")
    void SendStartInterview();

    UFUNCTION(BlueprintCallable, Category="Lobby")
    void SendReady(bool bReady);

    // ── 멤버 조회 ─────────────────────────────────────────
    /** Host 먼저, 나머지는 UserId 오름차순으로 정렬된 멤버 배열 반환 */
    UFUNCTION(BlueprintPure, Category="Lobby")
    TArray<FLobbyMember> GetSortedMembers() const;

    UFUNCTION(BlueprintPure, Category="Lobby")
    bool IsConnected() const { return m_bWsConnected; }

private:
    // ── HTTP ─────────────────────────────────────────────
    void OnCreateResponse(FHttpRequestPtr, FHttpResponsePtr, bool);
    void OnJoinResponse  (FHttpRequestPtr, FHttpResponsePtr, bool);
    void OnKickResponse  (FHttpRequestPtr, FHttpResponsePtr, bool);
    void OnLeaveResponse (FHttpRequestPtr, FHttpResponsePtr, bool);
    void OnCloseResponse (FHttpRequestPtr, FHttpResponsePtr, bool);

    // ── WebSocket 연결 ────────────────────────────────────
    void ConnectWebSocket(int32 LobbyId);
    void DisconnectWebSocket();
    void SendWsEvent     (const FString& Event,
                          const TSharedRef<FJsonObject>& Payload);
    void SendWsEventEmpty(const FString& Event);

    // WebSocket 리스너
    void OnWsConnected();
    void OnWsMessage(const FString& Msg);
    void OnWsError  (const FString& Err);
    void OnWsClosed (int32 StatusCode, const FString& Reason, bool bWasClean);

    // ── WebSocket 이벤트 핸들러 ───────────────────────────
    void InitWsHandlers();
    void HandleChatMessage    (const TSharedPtr<FJsonObject>& P);
    void HandleMemberJoined   (const TSharedPtr<FJsonObject>& P);
    void HandleMemberLeft     (const TSharedPtr<FJsonObject>& P);
    void HandleMemberKicked   (const TSharedPtr<FJsonObject>& P);
    void HandleCategoryChanged(const TSharedPtr<FJsonObject>& P);
    void HandleLobbyStarted   (const TSharedPtr<FJsonObject>& P);
    void HandleLobbyClosed    (const TSharedPtr<FJsonObject>& P);
    void HandleMemberReady    (const TSharedPtr<FJsonObject>& P);
    void HandleWsError        (const TSharedPtr<FJsonObject>& P);

    // ── 헬퍼 ─────────────────────────────────────────────
    FString      GetBaseUrl()   const;
    FString      GetWsBaseUrl() const;
    FString      MakeAuthHeader() const;
    FLobbyInfo   ParseLobbyInfo(const TSharedPtr<FJsonObject>& Obj) const;
    FLobbyMember ParseMember   (const TSharedPtr<FJsonObject>& Obj) const;

    // ── 상태 ─────────────────────────────────────────────
    TSharedPtr<IWebSocket>    m_ws;
    bool                      m_bWsConnected = false;
    TMap<int32, FLobbyMember> m_memberMap;          // UserId → Member

    // O(1) WebSocket 이벤트 라우터
    using FWsHandler = TFunction<void(const TSharedPtr<FJsonObject>&)>;
    TMap<FString, FWsHandler> m_wsHandlers;
};
