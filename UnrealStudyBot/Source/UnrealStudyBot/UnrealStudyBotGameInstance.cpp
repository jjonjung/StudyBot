#include "UnrealStudyBotGameInstance.h"
#include "Kismet/GameplayStatics.h"

void UStudyBotGameInstance::Init()
{
    Super::Init();

    // DefaultGame.ini [StudyBot] 섹션에서 URL 로드
    FString ServerHost;
    GConfig->GetString(TEXT("StudyBot"), TEXT("ServerHost"), ServerHost, GGameIni);
    if (ServerHost.IsEmpty())
        ServerHost = TEXT("localhost:3000");
    m_baseUrl   = TEXT("http://") + ServerHost;
    m_wsBaseUrl = TEXT("ws://")   + ServerHost;
}

// ── 인증 ─────────────────────────────────────────────────

void UStudyBotGameInstance::SetAuthInfo(const FAuthInfo& Info)
{
    m_authInfo = Info;
}

void UStudyBotGameInstance::Logout()
{
    m_authInfo = FAuthInfo{};
    ClearLobbyInfo();
    OpenLoginMap();
}

// ── 로비 ─────────────────────────────────────────────────

void UStudyBotGameInstance::SetLobbyInfo(const FLobbyInfo& Info, bool bIsHost)
{
    m_currentLobby = Info;
    m_bIsHost      = bIsHost;
}

void UStudyBotGameInstance::ClearLobbyInfo()
{
    m_currentLobby = FLobbyInfo{};
    m_bIsHost      = false;
}

// ── 씬 전환 ──────────────────────────────────────────────

void UStudyBotGameInstance::OpenLoginMap()
{
    UGameplayStatics::OpenLevel(this, FName("LoginMap"));
}

void UStudyBotGameInstance::OpenPreLobbyMap()
{
    ClearLobbyInfo();
    UGameplayStatics::OpenLevel(this, FName("PreLobbyMap"));
}

void UStudyBotGameInstance::OpenLobbyRoomMap()
{
    UGameplayStatics::OpenLevel(this, FName("LobbyRoomMap"));
}

void UStudyBotGameInstance::OpenLobbyMap()
{
    OpenPreLobbyMap(); // 하위 호환
}

void UStudyBotGameInstance::OpenInterviewMap(ECardCategory Category)
{
    m_selectedCategory = Category;
    UGameplayStatics::OpenLevel(this, FName("InterviewMap"));
}
