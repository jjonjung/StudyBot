#include "Subsystem/LobbySubsystem.h"
#include "UnrealStudyBotGameInstance.h"
#include "HttpModule.h"
#include "Interfaces/IHttpResponse.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "WebSocketsModule.h"

// ── 수명주기 ─────────────────────────────────────────────

void ULobbySubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    InitWsHandlers();
}

void ULobbySubsystem::Deinitialize()
{
    DisconnectWebSocket();
    Super::Deinitialize();
}

// ── 헬퍼 ─────────────────────────────────────────────────

FString ULobbySubsystem::GetBaseUrl() const
{
    auto* GI = Cast<UStudyBotGameInstance>(GetGameInstance());
    return GI ? GI->GetBaseUrl() : TEXT("http://10.0.2.2:3000");
}

FString ULobbySubsystem::GetWsBaseUrl() const
{
    auto* GI = Cast<UStudyBotGameInstance>(GetGameInstance());
    return GI ? GI->GetWsBaseUrl() : TEXT("ws://10.0.2.2:3000");
}

FString ULobbySubsystem::MakeAuthHeader() const
{
    auto* GI = Cast<UStudyBotGameInstance>(GetGameInstance());
    if (!GI) return {};
    return TEXT("Bearer ") + GI->GetAuthInfo().Token;
}

FLobbyMember ULobbySubsystem::ParseMember(const TSharedPtr<FJsonObject>& Obj) const
{
    FLobbyMember M;
    M.UserId   = (int32)Obj->GetNumberField(TEXT("userId"));
    M.Nickname = Obj->GetStringField(TEXT("nickname"));
    FString RoleStr = Obj->GetStringField(TEXT("role"));
    M.Role     = (RoleStr == TEXT("host")) ? ELobbyRole::Host : ELobbyRole::Member;
    Obj->TryGetBoolField(TEXT("isReady"), M.bIsReady);
    return M;
}

FLobbyInfo ULobbySubsystem::ParseLobbyInfo(const TSharedPtr<FJsonObject>& Obj) const
{
    FLobbyInfo Info;
    Info.LobbyId    = (int32)Obj->GetNumberField(TEXT("lobbyId"));
    Info.Code       = Obj->GetStringField(TEXT("code"));
    Info.Name       = Obj->GetStringField(TEXT("name"));
    Info.Category   = Obj->GetStringField(TEXT("category"));
    Info.MaxMembers = (int32)Obj->GetNumberField(TEXT("maxMembers"));

    const TArray<TSharedPtr<FJsonValue>>* MembersArr;
    if (Obj->TryGetArrayField(TEXT("members"), MembersArr))
    {
        for (auto& Val : *MembersArr)
        {
            auto MObj = Val->AsObject();
            if (MObj.IsValid())
                Info.Members.Add(ParseMember(MObj));
        }
    }
    return Info;
}

// ── CreateLobby ───────────────────────────────────────────

void ULobbySubsystem::CreateLobby(const FString& Name,
                                   const FString& Category,
                                   int32 MaxMembers)
{
    TSharedRef<FJsonObject> Body = MakeShared<FJsonObject>();
    Body->SetStringField(TEXT("name"),       Name);
    Body->SetStringField(TEXT("category"),   Category);
    Body->SetNumberField(TEXT("maxMembers"), MaxMembers);

    FString JsonStr;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&JsonStr);
    FJsonSerializer::Serialize(Body, Writer);

    auto Req = FHttpModule::Get().CreateRequest();
    Req->SetURL(GetBaseUrl() + TEXT("/api/lobby"));
    Req->SetVerb(TEXT("POST"));
    Req->SetHeader(TEXT("Content-Type"),  TEXT("application/json"));
    Req->SetHeader(TEXT("Authorization"), MakeAuthHeader());
    Req->SetContentAsString(JsonStr);
    Req->OnProcessRequestComplete().BindUObject(this, &ULobbySubsystem::OnCreateResponse);
    Req->ProcessRequest();
}

void ULobbySubsystem::OnCreateResponse(FHttpRequestPtr, FHttpResponsePtr Res, bool bOk)
{
    if (!bOk || !Res.IsValid())
    {
        OnLobbyError.Broadcast(0, TEXT("서버에 연결할 수 없습니다."));
        return;
    }

    TSharedPtr<FJsonObject> Json;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Res->GetContentAsString());
    FJsonSerializer::Deserialize(Reader, Json);

    if (Res->GetResponseCode() != 201 || !Json.IsValid())
    {
        FString Err = Json.IsValid() ? Json->GetStringField(TEXT("error")) : TEXT("로비 생성 실패");
        OnLobbyError.Broadcast(Res->GetResponseCode(), Err);
        return;
    }

    FLobbyInfo Info = ParseLobbyInfo(Json);

    // GameInstance에 저장 → IsHost = true
    if (auto* GI = Cast<UStudyBotGameInstance>(GetGameInstance()))
        GI->SetLobbyInfo(Info, true);

    // 멤버 캐시 구성
    m_memberMap.Empty();
    for (const FLobbyMember& M : Info.Members)
        m_memberMap.Add(M.UserId, M);

    // WebSocket 연결 후 OnLobbyCreated는 OnWsConnected에서 브로드캐스트
    ConnectWebSocket(Info.LobbyId);
    OnLobbyCreated.Broadcast(Info);
}

// ── JoinLobbyByCode ───────────────────────────────────────

void ULobbySubsystem::JoinLobbyByCode(const FString& Code)
{
    TSharedRef<FJsonObject> Body = MakeShared<FJsonObject>();
    Body->SetStringField(TEXT("code"), Code);

    FString JsonStr;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&JsonStr);
    FJsonSerializer::Serialize(Body, Writer);

    auto Req = FHttpModule::Get().CreateRequest();
    Req->SetURL(GetBaseUrl() + TEXT("/api/lobby/join"));
    Req->SetVerb(TEXT("POST"));
    Req->SetHeader(TEXT("Content-Type"),  TEXT("application/json"));
    Req->SetHeader(TEXT("Authorization"), MakeAuthHeader());
    Req->SetContentAsString(JsonStr);
    Req->OnProcessRequestComplete().BindUObject(this, &ULobbySubsystem::OnJoinResponse);
    Req->ProcessRequest();
}

void ULobbySubsystem::OnJoinResponse(FHttpRequestPtr, FHttpResponsePtr Res, bool bOk)
{
    if (!bOk || !Res.IsValid())
    {
        OnLobbyError.Broadcast(0, TEXT("서버에 연결할 수 없습니다."));
        return;
    }

    TSharedPtr<FJsonObject> Json;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Res->GetContentAsString());
    FJsonSerializer::Deserialize(Reader, Json);

    int32 Code = Res->GetResponseCode();
    if (Code != 200 || !Json.IsValid())
    {
        FString ErrMsg;
        if (Code == 404)      ErrMsg = TEXT("존재하지 않는 로비 코드입니다.");
        else if (Code == 409) ErrMsg = TEXT("로비가 가득 찼습니다.");
        else if (Code == 400) ErrMsg = TEXT("이미 시작된 로비입니다.");
        else                  ErrMsg = Json.IsValid() ? Json->GetStringField(TEXT("error")) : TEXT("입장 실패");

        OnLobbyError.Broadcast(Code, ErrMsg);
        return;
    }

    FLobbyInfo Info = ParseLobbyInfo(Json);

    if (auto* GI = Cast<UStudyBotGameInstance>(GetGameInstance()))
        GI->SetLobbyInfo(Info, false);

    m_memberMap.Empty();
    for (const FLobbyMember& M : Info.Members)
        m_memberMap.Add(M.UserId, M);

    ConnectWebSocket(Info.LobbyId);
    OnLobbyJoined.Broadcast(Info);
}

// ── KickMember ────────────────────────────────────────────

void ULobbySubsystem::KickMember(int32 TargetUserId)
{
    auto* GI = Cast<UStudyBotGameInstance>(GetGameInstance());
    if (!GI) return;

    int32 LobbyId = GI->GetLobbyInfo().LobbyId;

    TSharedRef<FJsonObject> Body = MakeShared<FJsonObject>();
    Body->SetNumberField(TEXT("targetUserId"), TargetUserId);

    FString JsonStr;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&JsonStr);
    FJsonSerializer::Serialize(Body, Writer);

    auto Req = FHttpModule::Get().CreateRequest();
    Req->SetURL(GetBaseUrl() + FString::Printf(TEXT("/api/lobby/%d/kick"), LobbyId));
    Req->SetVerb(TEXT("POST"));
    Req->SetHeader(TEXT("Content-Type"),  TEXT("application/json"));
    Req->SetHeader(TEXT("Authorization"), MakeAuthHeader());
    Req->SetContentAsString(JsonStr);
    Req->OnProcessRequestComplete().BindUObject(this, &ULobbySubsystem::OnKickResponse);
    Req->ProcessRequest();
}

void ULobbySubsystem::OnKickResponse(FHttpRequestPtr, FHttpResponsePtr Res, bool bOk)
{
    if (!bOk || !Res.IsValid() || Res->GetResponseCode() != 200)
    {
        OnLobbyError.Broadcast(Res.IsValid() ? Res->GetResponseCode() : 0,
                               TEXT("강퇴 실패"));
    }
    // 성공 시 서버가 member/kicked WebSocket 이벤트를 브로드캐스트함
}

// ── LeaveLobby ────────────────────────────────────────────

void ULobbySubsystem::LeaveLobby()
{
    auto* GI = Cast<UStudyBotGameInstance>(GetGameInstance());
    if (!GI) return;

    int32 LobbyId = GI->GetLobbyInfo().LobbyId;

    auto Req = FHttpModule::Get().CreateRequest();
    Req->SetURL(GetBaseUrl() + FString::Printf(TEXT("/api/lobby/%d/leave"), LobbyId));
    Req->SetVerb(TEXT("POST"));
    Req->SetHeader(TEXT("Authorization"), MakeAuthHeader());
    Req->OnProcessRequestComplete().BindUObject(this, &ULobbySubsystem::OnLeaveResponse);
    Req->ProcessRequest();
}

void ULobbySubsystem::OnLeaveResponse(FHttpRequestPtr, FHttpResponsePtr Res, bool bOk)
{
    DisconnectWebSocket();
    if (auto* GI = Cast<UStudyBotGameInstance>(GetGameInstance()))
    {
        GI->ClearLobbyInfo();
        GI->OpenPreLobbyMap();
    }
}

// ── CloseLobby (Host 전용) ────────────────────────────────

void ULobbySubsystem::CloseLobby()
{
    auto* GI = Cast<UStudyBotGameInstance>(GetGameInstance());
    if (!GI || !GI->IsHost()) return;

    int32 LobbyId = GI->GetLobbyInfo().LobbyId;

    auto Req = FHttpModule::Get().CreateRequest();
    Req->SetURL(GetBaseUrl() + FString::Printf(TEXT("/api/lobby/%d/close"), LobbyId));
    Req->SetVerb(TEXT("POST"));
    Req->SetHeader(TEXT("Authorization"), MakeAuthHeader());
    Req->OnProcessRequestComplete().BindUObject(this, &ULobbySubsystem::OnCloseResponse);
    Req->ProcessRequest();
}

void ULobbySubsystem::OnCloseResponse(FHttpRequestPtr, FHttpResponsePtr Res, bool bOk)
{
    // 성공 시 서버가 lobby/closed WS 이벤트 브로드캐스트 → HandleLobbyClosed
}

// ── WebSocket 이벤트 전송 ─────────────────────────────────

void ULobbySubsystem::SendChatMessage(const FString& Message)
{
    TSharedRef<FJsonObject> Payload = MakeShared<FJsonObject>();
    Payload->SetStringField(TEXT("message"), Message);
    SendWsEvent(TEXT("chat/send"), Payload);
}

void ULobbySubsystem::SendSetCategory(const FString& Category)
{
    TSharedRef<FJsonObject> Payload = MakeShared<FJsonObject>();
    Payload->SetStringField(TEXT("category"), Category);
    SendWsEvent(TEXT("lobby/set_category"), Payload);
}

void ULobbySubsystem::SendStartInterview()
{
    SendWsEventEmpty(TEXT("lobby/start"));
}

void ULobbySubsystem::SendReady(bool bReady)
{
    TSharedRef<FJsonObject> Payload = MakeShared<FJsonObject>();
    Payload->SetBoolField(TEXT("ready"), bReady);
    SendWsEvent(TEXT("member/ready"), Payload);
}

// ── 멤버 조회 ─────────────────────────────────────────────

TArray<FLobbyMember> ULobbySubsystem::GetSortedMembers() const
{
    TArray<FLobbyMember> Members;
    m_memberMap.GenerateValueArray(Members);

    // Host 먼저, 나머지는 UserId 오름차순
    Members.Sort([](const FLobbyMember& A, const FLobbyMember& B)
    {
        if (A.IsHost() != B.IsHost()) return A.IsHost();
        return A.UserId < B.UserId;
    });
    return Members;
}

// ── WebSocket 연결 ────────────────────────────────────────

void ULobbySubsystem::ConnectWebSocket(int32 LobbyId)
{
    DisconnectWebSocket();

    auto* GI = Cast<UStudyBotGameInstance>(GetGameInstance());
    if (!GI) return;

    FString Token  = GI->GetAuthInfo().Token;
    FString WsUrl  = GetWsBaseUrl()
        + FString::Printf(TEXT("/ws/lobby/%d?token=%s"), LobbyId, *Token);

    m_ws = FWebSocketsModule::Get().CreateWebSocket(WsUrl, TEXT("ws"));

    m_ws->OnConnected().AddUObject(this, &ULobbySubsystem::OnWsConnected);
    m_ws->OnMessage().AddUObject(this, &ULobbySubsystem::OnWsMessage);
    m_ws->OnConnectionError().AddUObject(this, &ULobbySubsystem::OnWsError);
    m_ws->OnClosed().AddUObject(this, &ULobbySubsystem::OnWsClosed);

    m_ws->Connect();
}

void ULobbySubsystem::DisconnectWebSocket()
{
    if (m_ws.IsValid())
    {
        m_ws->Close();
        m_ws.Reset();
    }
    m_bWsConnected = false;
}

void ULobbySubsystem::SendWsEvent(const FString& Event,
                                   const TSharedRef<FJsonObject>& Payload)
{
    if (!m_ws.IsValid() || !m_bWsConnected) return;

    TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
    Root->SetStringField(TEXT("event"), Event);
    Root->SetObjectField(TEXT("payload"), Payload);

    FString Msg;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Msg);
    FJsonSerializer::Serialize(Root, Writer);

    m_ws->Send(Msg);
}

void ULobbySubsystem::SendWsEventEmpty(const FString& Event)
{
    SendWsEvent(Event, MakeShared<FJsonObject>());
}

// ── WebSocket 리스너 ──────────────────────────────────────

void ULobbySubsystem::OnWsConnected()
{
    m_bWsConnected = true;
    UE_LOG(LogTemp, Log, TEXT("[LobbySubsystem] WebSocket 연결됨"));
}

void ULobbySubsystem::OnWsMessage(const FString& Msg)
{
    TSharedPtr<FJsonObject> Root;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Msg);
    if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid()) return;

    FString Event;
    if (!Root->TryGetStringField(TEXT("event"), Event)) return;

    TSharedPtr<FJsonObject> Payload;
    const TSharedPtr<FJsonObject>* PayloadPtr;
    if (Root->TryGetObjectField(TEXT("payload"), PayloadPtr))
        Payload = *PayloadPtr;
    else
        Payload = MakeShared<FJsonObject>();

    // O(1) 이벤트 라우팅
    if (const FWsHandler* Handler = m_wsHandlers.Find(Event))
        (*Handler)(Payload);
    else
        UE_LOG(LogTemp, Warning, TEXT("[LobbySubsystem] 알 수 없는 WS 이벤트: %s"), *Event);
}

void ULobbySubsystem::OnWsError(const FString& Err)
{
    m_bWsConnected = false;
    UE_LOG(LogTemp, Error, TEXT("[LobbySubsystem] WebSocket 오류: %s"), *Err);
    OnLobbyError.Broadcast(0, TEXT("연결이 끊겼습니다. 재연결 중..."));
}

void ULobbySubsystem::OnWsClosed(int32 StatusCode, const FString& Reason, bool bWasClean)
{
    m_bWsConnected = false;
    UE_LOG(LogTemp, Log, TEXT("[LobbySubsystem] WebSocket 닫힘 (%d: %s)"),
           StatusCode, *Reason);
}

// ── WebSocket 이벤트 핸들러 초기화 ───────────────────────

void ULobbySubsystem::InitWsHandlers()
{
    m_wsHandlers.Add(TEXT("chat/message"),
        [this](const TSharedPtr<FJsonObject>& P) { HandleChatMessage(P); });

    m_wsHandlers.Add(TEXT("member/joined"),
        [this](const TSharedPtr<FJsonObject>& P) { HandleMemberJoined(P); });

    m_wsHandlers.Add(TEXT("member/left"),
        [this](const TSharedPtr<FJsonObject>& P) { HandleMemberLeft(P); });

    m_wsHandlers.Add(TEXT("member/kicked"),
        [this](const TSharedPtr<FJsonObject>& P) { HandleMemberKicked(P); });

    m_wsHandlers.Add(TEXT("lobby/category_changed"),
        [this](const TSharedPtr<FJsonObject>& P) { HandleCategoryChanged(P); });

    m_wsHandlers.Add(TEXT("lobby/started"),
        [this](const TSharedPtr<FJsonObject>& P) { HandleLobbyStarted(P); });

    m_wsHandlers.Add(TEXT("lobby/closed"),
        [this](const TSharedPtr<FJsonObject>& P) { HandleLobbyClosed(P); });

    m_wsHandlers.Add(TEXT("member/ready"),
        [this](const TSharedPtr<FJsonObject>& P) { HandleMemberReady(P); });

    m_wsHandlers.Add(TEXT("error"),
        [this](const TSharedPtr<FJsonObject>& P) { HandleWsError(P); });
}

// ── WebSocket 이벤트 핸들러 구현 ─────────────────────────

void ULobbySubsystem::HandleChatMessage(const TSharedPtr<FJsonObject>& P)
{
    FString Nickname = P->GetStringField(TEXT("nickname"));
    FString Message  = P->GetStringField(TEXT("message"));
    OnChatReceived.Broadcast(Nickname, Message);
}

void ULobbySubsystem::HandleMemberJoined(const TSharedPtr<FJsonObject>& P)
{
    FLobbyMember NewMember = ParseMember(P);
    m_memberMap.Add(NewMember.UserId, NewMember);
    OnMemberJoined.Broadcast(NewMember);
}

void ULobbySubsystem::HandleMemberLeft(const TSharedPtr<FJsonObject>& P)
{
    int32 UserId = (int32)P->GetNumberField(TEXT("userId"));
    m_memberMap.Remove(UserId);
    OnMemberLeft.Broadcast(UserId);
}

void ULobbySubsystem::HandleMemberKicked(const TSharedPtr<FJsonObject>& P)
{
    int32 UserId = (int32)P->GetNumberField(TEXT("userId"));
    m_memberMap.Remove(UserId);
    OnMemberKicked.Broadcast(UserId);

    // 내가 강퇴됐는지 확인
    auto* GI = Cast<UStudyBotGameInstance>(GetGameInstance());
    if (GI && GI->GetAuthInfo().UserId == UserId)
    {
        DisconnectWebSocket();
        GI->ClearLobbyInfo();
        GI->OpenPreLobbyMap();
    }
}

void ULobbySubsystem::HandleCategoryChanged(const TSharedPtr<FJsonObject>& P)
{
    FString Category = P->GetStringField(TEXT("category"));
    OnCategoryChanged.Broadcast(Category);
}

void ULobbySubsystem::HandleLobbyStarted(const TSharedPtr<FJsonObject>& P)
{
    FString Category  = P->GetStringField(TEXT("category"));
    int32   CardCount = (int32)P->GetNumberField(TEXT("cardCount"));
    OnLobbyStarted.Broadcast(Category, CardCount);
}

void ULobbySubsystem::HandleLobbyClosed(const TSharedPtr<FJsonObject>& P)
{
    DisconnectWebSocket();
    if (auto* GI = Cast<UStudyBotGameInstance>(GetGameInstance()))
    {
        GI->ClearLobbyInfo();
        GI->OpenPreLobbyMap();
    }
}

void ULobbySubsystem::HandleMemberReady(const TSharedPtr<FJsonObject>& P)
{
    int32 UserId = (int32)P->GetNumberField(TEXT("userId"));
    bool  bReady = false;
    P->TryGetBoolField(TEXT("ready"), bReady);

    if (FLobbyMember* M = m_memberMap.Find(UserId))
        M->bIsReady = bReady;

    OnMemberReadyChanged.Broadcast(UserId);
}

void ULobbySubsystem::HandleWsError(const TSharedPtr<FJsonObject>& P)
{
    FString Msg = P->GetStringField(TEXT("message"));
    OnLobbyError.Broadcast(0, Msg);
}
