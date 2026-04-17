#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "Models/StudyBotTypes.h"
#include "UnrealStudyBotGameInstance.generated.h"

/**
 * UStudyBotGameInstance
 * ─────────────────────────────────────────────────────────
 * 앱 전역 상태 관리:
 *  - 로그인 정보 (JWT · 닉네임 · Google 여부)
 *  - 현재 로비 정보 (LobbyId · Code · IsHost)
 *  - 서버 URL (DefaultGame.ini에서 로드)
 *  - 씬 전환 브리지
 */
UCLASS()
class UNREALSTUDYBOT_API UStudyBotGameInstance : public UGameInstance
{
    GENERATED_BODY()

public:
    virtual void Init() override;

    // ── 인증 정보 ──────────────────────────────────────────
    UFUNCTION(BlueprintCallable, Category="Auth")
    void      SetAuthInfo(const FAuthInfo& Info);

    UFUNCTION(BlueprintPure, Category="Auth")
    FAuthInfo GetAuthInfo() const { return m_authInfo; }

    UFUNCTION(BlueprintPure, Category="Auth")
    bool      IsLoggedIn()  const { return m_authInfo.IsValid(); }

    UFUNCTION(BlueprintCallable, Category="Auth")
    void      Logout();

    // ── 로비 정보 ──────────────────────────────────────────
    UFUNCTION(BlueprintCallable, Category="Lobby")
    void       SetLobbyInfo(const FLobbyInfo& Info, bool bIsHost);

    UFUNCTION(BlueprintPure, Category="Lobby")
    FLobbyInfo GetLobbyInfo() const { return m_currentLobby; }

    UFUNCTION(BlueprintPure, Category="Lobby")
    bool       IsHost() const { return m_bIsHost; }

    UFUNCTION(BlueprintCallable, Category="Lobby")
    void       ClearLobbyInfo();

    // ── API URL ────────────────────────────────────────────
    UFUNCTION(BlueprintPure, Category="Config")
    FString GetBaseUrl()   const { return m_baseUrl; }

    UFUNCTION(BlueprintPure, Category="Config")
    FString GetWsBaseUrl() const { return m_wsBaseUrl; }

    // ── 씬 전환 ───────────────────────────────────────────
    UFUNCTION(BlueprintCallable, Category="Navigation")
    void OpenLoginMap();

    UFUNCTION(BlueprintCallable, Category="Navigation")
    void OpenPreLobbyMap();

    UFUNCTION(BlueprintCallable, Category="Navigation")
    void OpenLobbyRoomMap();

    UFUNCTION(BlueprintCallable, Category="Navigation")
    void OpenInterviewMap(ECardCategory Category);

    /** 하위 호환: 구 LobbyMap 호출 → PreLobbyMap으로 리다이렉트 */
    UFUNCTION(BlueprintCallable, Category="Navigation")
    void OpenLobbyMap();

    UFUNCTION(BlueprintPure, Category="Navigation")
    ECardCategory GetSelectedCategory() const { return m_selectedCategory; }

private:
    FAuthInfo     m_authInfo;
    FLobbyInfo    m_currentLobby;
    bool          m_bIsHost         = false;

    FString       m_baseUrl         = TEXT("http://10.0.2.2:3000");
    FString       m_wsBaseUrl       = TEXT("ws://10.0.2.2:3000");
    ECardCategory m_selectedCategory = ECardCategory::All;
};
