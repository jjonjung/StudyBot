# 04. 전체 데이터 흐름 (v3)

## 시나리오 1: 로컬 회원가입 → 로그인

```
[Android 앱 — ULoginWidget]
  사용자 아이디/비밀번호 입력 → BtnRegister 클릭
       ↓
  UAuthSubsystem::Register(user, pass, nick)
       ↓ POST /api/auth/register { username, password, nickname }

[C++ 서버 — AuthController]
       ↓ AuthService::RegisterLocal()
       ↓ bcrypt_hash(password, cost=10)
       ↓ CALL sp_auth_create_user(username, hash, nickname)
       ↓ 201 { id, message }

[Android 앱]
  OnRegisterResult.Broadcast(true, "회원가입 성공")
  ULoginWidget: 성공 메시지 표시

  사용자 BtnLogin 클릭
       ↓
  UAuthSubsystem::Login(user, pass)
       ↓ POST /api/auth/login { username, password }

[C++ 서버]
       ↓ CALL sp_auth_get_login_user(username)
       ↓ bcrypt_verify(password, hash)
       ↓ JwtUtil::Sign({id, nickname}, 24h)
       ↓ 200 { token, nickname, userId }

[Android 앱]
  FAuthInfo { Token, Nickname, UserId, bIsGoogle=false }
  GameInstance::SetAuthInfo()
  OnLoginResult.Broadcast(true)
  ULoginWidget → GameInstance::OpenPreLobbyMap()
```

---

## 시나리오 2: Google 로그인 (JNI)

```
[Android 앱 — ULoginWidget]
  사용자 BtnGoogle 클릭
       ↓
  UAuthSubsystem::LoginWithGoogle()
       ↓ JNI 호출: GoogleAuthHelper.signIn(activity, nativePtr)

[Android Java — GoogleAuthHelper]
       ↓ GoogleSignIn.getClient(gso).silentSignIn()
         또는 GoogleSignIn 인텐트 실행 (계정 선택 팝업)
       ↓ GoogleSignInAccount.getIdToken() 획득
       ↓ nativeOnIdToken(nativePtr, idToken)

[Android 앱 — JNI 콜백]
       ↓ AsyncTask(GameThread) →
  UAuthSubsystem::SendGoogleIdTokenToServer(idToken)
       ↓ POST /api/auth/google/mobile { idToken: "eyJ..." }

[C++ 서버 — AuthController::googleMobile]
       ↓ AuthService::VerifyGoogleIdToken(idToken)
       ↓ HTTPS GET googleapis.com/tokeninfo?id_token={idToken}
       ↓ aud == GOOGLE_CLIENT_ID 검증
       ↓ { sub, email, name, picture } 파싱
       ↓ CALL sp_auth_upsert_google_user(sub, email, name, picture)
            → 최초 가입: INSERT users
            → 재로그인: SELECT id WHERE google_id=sub
       ↓ JwtUtil::Sign({id, nickname}, 24h)
       ↓ 200 { token, nickname, userId }

[Android 앱]
  FAuthInfo { Token, Nickname, UserId, bIsGoogle=true }
  GameInstance::SetAuthInfo()
  OnLoginResult.Broadcast(true, "Google 로그인 성공")
  ULoginWidget → GameInstance::OpenPreLobbyMap()
```

---

## 시나리오 3: 로비 생성 (Host)

```
[PreLobbyMap — UPreLobbyWidget]
  사용자 BtnCreate 클릭 → UCreateLobbyWidget 표시
  이름·카테고리·최대 인원 입력 → BtnCreate 클릭
       ↓
  ULobbySubsystem::CreateLobby("스터디룸A", "Algorithm", 4)
       ↓ POST /api/lobby
         Authorization: Bearer {JWT}
         { name, category, maxMembers }

[C++ 서버 — LobbyController::createLobby]
       ↓ JwtFilter: userId, nickname 추출
       ↓ category 화이트리스트 검증
       ↓ LobbyService::CreateLobby(userId, name, category, max)
            ↓ code = generateLobbyCode()  → "ABC123" (6자리, 중복 재시도)
            ↓ CALL sp_lobby_create(userId, name, category, max, code)
                 → INSERT lobbies
                 → INSERT lobby_members (role='host')
       ↓ 201 { lobbyId:1, code:"ABC123", name, category, maxMembers }

[Android 앱]
  OnCreateLobbyResponse()
       ↓ FLobbyInfo 파싱
       ↓ GameInstance::SetLobbyInfo(info, bHost=true)
       ↓ ULobbySubsystem::ConnectWebSocket(lobbyId=1)
            → ws://server/ws/lobby/1?token={JWT}
            → JwtFilter 통과 → WsSessionManager::addSession(session)
       ↓ OnLobbyCreated.Broadcast(info)
  UCreateLobbyWidget → GameInstance::OpenLobbyRoomMap()
```

---

## 시나리오 4: 로비 코드로 입장 (Member)

```
[PreLobbyMap — UJoinLobbyWidget]
  사용자 코드 "ABC123" 입력 → BtnJoin 클릭
       ↓
  ULobbySubsystem::JoinLobbyByCode("ABC123")
       ↓ POST /api/lobby/join { code: "ABC123" }

[C++ 서버 — LobbyController::joinLobby]
       ↓ CALL sp_lobby_join(userId, "ABC123")
            → 로비 status='waiting' 확인
            → 인원 < maxMembers 확인
            → INSERT lobby_members (role='member')
            → 로비 + 전체 멤버 목록 반환
       ↓ 200 { lobbyId:1, code, name, category, members:[...] }

[Android 앱]
  OnJoinLobbyResponse()
       ↓ FLobbyInfo 파싱 (기존 멤버 포함)
       ↓ GameInstance::SetLobbyInfo(info, bHost=false)
       ↓ ULobbySubsystem::ConnectWebSocket(1)
            → WsSessionManager::addSession
            → 기존 멤버들에게 member/joined 브로드캐스트

[기존 멤버 앱들]
  WebSocket 수신: { event: "member/joined",
                   payload: {userId:2, nickname:"김철수", role:"member"} }
       ↓ m_wsHandlers["member/joined"] → HandleMemberJoined(payload)
       ↓ m_memberMap.Add(2, newMember)
       ↓ OnMemberJoined.Broadcast(newMember)
  ULobbyRoomWidget::HandleMemberJoined → RefreshMemberList()
```

---

## 시나리오 5: 채팅 메시지 전송

```
[ULobbyRoomWidget]
  사용자 채팅 입력 → BtnSend 클릭
       ↓
  ULobbySubsystem::SendChatMessage("안녕하세요")
       ↓ WebSocket 전송:
         { event: "chat/send", payload: { message: "안녕하세요" } }

[C++ 서버 — LobbyWsController::onChatSend]
       ↓ CALL sp_lobby_message_save(lobbyId, userId, message)
       ↓ WsSessionManager::broadcast(lobbyId,
           { event: "chat/message",
             payload: { userId, nickname:"김철수", message, sentAt } })

[로비 내 모든 멤버 앱]
  WebSocket 수신 → m_wsHandlers["chat/message"] → HandleChatMessage(payload)
       ↓ OnChatReceived.Broadcast("김철수", "안녕하세요")
  ULobbyRoomWidget::HandleChatReceived → AppendChatMessage() → ChatScroll 갱신
```

---

## 시나리오 6: Host가 면접장 변경 후 인터뷰 시작

```
[Host 앱 — ULobbyRoomWidget]
  카테고리 선택 UI에서 "Algorithm" 선택
       ↓
  ULobbySubsystem::SendSetCategory("Algorithm")
       ↓ WebSocket: { event: "lobby/set_category",
                     payload: { category: "Algorithm" } }

[C++ 서버 — LobbyWsController::onSetCategory]
       ↓ session.userId == lobby.host_user_id 확인
       ↓ UPDATE lobbies SET category='Algorithm' WHERE id=lobbyId
       ↓ WsSessionManager::broadcast(lobbyId,
           { event: "lobby/category_changed",
             payload: { category: "Algorithm" } })

[모든 멤버 앱]
  OnCategoryChanged.Broadcast("Algorithm")
  ULobbyRoomWidget::HandleCategoryChanged → TxtCategory = "Algorithm" 업데이트

  ─────── Host가 BtnStart 클릭 ─────────────────────────────────

[Host 앱]
  ULobbySubsystem::SendStartInterview()
       ↓ WebSocket: { event: "lobby/start", payload: {} }

[C++ 서버 — LobbyWsController::onStartInterview]
       ↓ CALL sp_lobby_start(hostUserId, lobbyId)
            → role='host' 검증 (아니면 SIGNAL NOT_HOST → error 이벤트)
            → UPDATE lobbies SET status='in_progress'
       ↓ WsSessionManager::broadcast(lobbyId,
           { event: "lobby/started",
             payload: { category: "Algorithm", cardCount: 10 } })

[모든 멤버 앱]
  m_wsHandlers["lobby/started"] → HandleLobbyStarted(payload)
       ↓ OnLobbyStarted.Broadcast("Algorithm", 10)
  ULobbyRoomWidget::HandleLobbyStarted
       ↓ UCardSubsystem::FetchInterviewCards(ECardCategory::Algorithm, 10)
            → GET /api/cards/interview?category=Algorithm&count=10
       ↓ GameInstance::OpenInterviewMap(ECardCategory::Algorithm)
```

---

## 시나리오 7: 인터뷰 진행

```
[InterviewMap — UInterviewWidget]
  CardSubsystem::OnCardsLoaded → StartInterview(cards)
       ↓ cards.Category == "Algorithm"?
         Yes → UAlgorithmCardWidget 사용
         No  → UFlashcardWidget 사용
       ↓ NPCDialogueComponent::StartInterview(cards)
            ↓ m_currentIndex = -1 → NextQuestion() → index=0
            ↓ OnQuestionChanged.Broadcast(cards[0])

  ── 카드 뒤집기 ────────────────────────────────────────────────
  FlashcardWidget::OnFlipClicked()
       ↓ CardSwitcher → 답변면(1)
       ↓ BtnKnown · BtnUnknown 표시

  ── 알았어요 클릭 ──────────────────────────────────────────────
  OnCardJudged(true)
       ↓ CardSubsystem::SaveProgress(cardId, true, 5)
            → PUT /api/progress/{cardId} { known:1, score:5 }
            → (백그라운드, 응답 대기 안 함)
       ↓ NPCDialogueComponent::MarkKnown() → NextQuestion()

  ── 10문제 완료 ────────────────────────────────────────────────
  OnInterviewDone(result)
       ↓ ScreenSwitcher → 결과 화면
       ↓ CardSubsystem::SaveInterviewSession("Algorithm", 10, 7)
            → POST /api/progress/session
              { category:"Algorithm", total_cards:10, known_count:7 }
            → sp_progress_create_session (트랜잭션):
                INSERT interview_sessions
                UPSERT daily_scores (누적 합산)
       ↓ 201 { id: sessionId }

  ── 결과 화면 ──────────────────────────────────────────────────
  BtnRetry → CardSubsystem::GetCachedCards() → 재시작 (서버 재요청 없음)
  BtnLobby → GameInstance::OpenPreLobbyMap()
```

---

## 시나리오 8: 멤버 강퇴

```
[Host 앱 — ULobbyRoomWidget]
  특정 멤버 항목의 BtnKick 클릭 (targetUserId=3)
       ↓
  ULobbySubsystem::KickMember(3)
       ↓ POST /api/lobby/{lobbyId}/kick { targetUserId: 3 }
         Authorization: Bearer {Host JWT}

[C++ 서버]
       ↓ CALL sp_lobby_kick(hostId, lobbyId, 3)
            → role='host' 검증
            → DELETE lobby_members WHERE user_id=3
       ↓ 200 { message }
       ↓ LobbyService → WsSessionManager::broadcast(lobbyId,
           { event: "member/kicked", payload: { userId: 3 } })

[강퇴된 멤버 앱 & 나머지 멤버 앱들]
  m_wsHandlers["member/kicked"] → HandleMemberKicked(payload)
       ↓ m_memberMap.Remove(3)
       ↓ OnMemberKicked.Broadcast(3)
       ↓ userId==자신 확인 → DisconnectWebSocket() → ClearLobbyInfo() → OpenPreLobbyMap()
  (다른 멤버) ULobbyRoomWidget::HandleMemberKicked → RefreshMemberList()
```

---

## JWT 토큰 흐름

```
로그인 성공 (로컬 또는 Google)
  ↓ 서버: JwtUtil::Sign({id, nickname}, 24h) → "eyJ..."
  ↓ GameInstance::m_authInfo.Token = "eyJ..."

이후 모든 HTTP 요청
  ↓ 헤더: Authorization: Bearer eyJ...
  ↓ 서버 JwtFilter:
       JwtUtil::Verify(token) → { userId, nickname }
       req->attributes["userId"] = userId
  ↓ Controller: req->getAttribute<int>("userId")

WebSocket 연결
  ↓ ws://server/ws/lobby/{id}?token=eyJ...
  ↓ JwtFilter가 handleNewConnection 전에 검증
  ↓ WsSession에 userId, nickname 저장

24시간 경과 후
  ↓ JwtUtil::Verify() 예외 → JwtFilter → 401
  ↓ 앱: OnLobbyError / OnLoginResult(false) → OpenPreLobbyMap() or 로그인 화면
```

---

## 에러 흐름

```
네트워크 오류 (서버 다운)
  ↓ HTTP bOk=false → 각 Subsystem OnError 브로드캐스트
  ↓ Widget: 오류 메시지 표시, 버튼 재활성화

로비 코드 없음 (LOBBY_NOT_FOUND)
  ↓ 서버 404 → OnLobbyError.Broadcast("존재하지 않는 로비 코드입니다.")

로비 만원 (LOBBY_FULL)
  ↓ 서버 409 → OnLobbyError.Broadcast("로비가 가득 찼습니다.")

Host가 아닌 멤버가 시작 시도
  ↓ sp_lobby_start 내부: SIGNAL NOT_HOST
  ↓ 서버 → WebSocket: { event: "error", payload: { message: "권한 없음" } }
  ↓ ULobbySubsystem → OnLobbyError.Broadcast("권한이 없습니다.")

WebSocket 연결 끊김
  ↓ OnWsClosed() → 지수 백오프 재연결 (Phase 2)
  ↓ Phase 1: OnLobbyError.Broadcast("연결이 끊겼습니다. 재연결 중...")
```
