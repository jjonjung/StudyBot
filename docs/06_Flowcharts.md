# 06. Flowcharts (Mermaid) — v3

> GitHub / VS Code Markdown Preview Enhanced / Notion 에서 렌더링됩니다.

---

## 1. 시스템 전체 구조 (v3)

```mermaid
graph TB
    subgraph UE56["Android 앱 (UE 5.6)"]
        LW[ULoginWidget\n로컬/Google 로그인]
        PLW[UPreLobbyWidget\n방 만들기/코드 입장]
        CLW[UCreateLobbyWidget]
        JLW[UJoinLobbyWidget]
        LRW[ULobbyRoomWidget\n채팅·멤버·시작]
        IW[UInterviewWidget]
        ACW[UAlgorithmCardWidget]
        CW[UContributionWidget\n잔디 히트맵]

        LW --> PLW
        PLW --> CLW & JLW
        CLW & JLW --> LRW
        LRW --> IW
        IW --> ACW
    end

    subgraph Subsystems["GameInstanceSubsystem"]
        AUTH[UAuthSubsystem\n로컬+Google JNI]
        CARD[UCardSubsystem\n카드·진도·히트맵]
        LOBBY[ULobbySubsystem\nREST+WebSocket]
        GI[UStudyBotGameInstance\n토큰·로비코드·IsHost]
    end

    subgraph Server["C++ 백엔드 (Drogon)"]
        AC[AuthController]
        LC[LobbyController]
        CC[CardController]
        WSC[LobbyWsController]
        WSM[WsSessionManager\nunordered_map]
        JF[JwtFilter]
        LC & CC --> JF
        WSC --> WSM
    end

    subgraph DB["MySQL 8.0 (Stored Procedures)"]
        U[(users)]
        F[(flashcards)]
        UP[(user_progress)]
        IS[(interview_sessions)]
        DS[(daily_scores)]
        LB[(lobbies)]
        LM[(lobby_members)]
        LMG[(lobby_messages)]
    end

    LW --> AUTH --> GI
    LRW --> LOBBY --> GI
    IW --> CARD --> GI

    AUTH -- "POST /api/auth/*" --> AC
    LOBBY -- "POST /api/lobby/*" --> LC
    LOBBY -- "ws://*/ws/lobby/{id}" --> WSC
    CARD -- "GET /api/cards/*\nPUT /api/progress/*" --> CC

    AC --> U
    LC --> LB & LM
    WSC --> LMG
    CC --> F & UP & IS & DS
```

---

## 2. 로컬 로그인 → PreLobby 흐름

```mermaid
flowchart TD
    A([BtnLogin 클릭]) --> B{입력값 검증}
    B -- 빈 값 --> C[오류 표시]
    B -- 정상 --> D[SetButtonsEnabled false]
    D --> E[POST /api/auth/login]

    E --> F{백엔드 AuthController}
    F --> G[CALL sp_auth_get_login_user]
    G -- 미존재 --> H[401 invalid credentials]
    G -- 존재 --> I[bcrypt_verify pw vs hash]
    I -- 불일치 --> H
    I -- 일치 --> J[JwtUtil::Sign 24h]
    J --> K[200 token · nickname · userId]

    H --> L[OnLoginResult false]
    K --> M[OnLoginResult true]
    L --> N[SetButtonsEnabled true\n오류 메시지]
    M --> O[FAuthInfo 저장\nGameInstance::SetAuthInfo]
    O --> P[GameInstance::OpenPreLobbyMap]
    P --> Q([PreLobbyMap])
```

---

## 3. Google 로그인 흐름 (JNI)

```mermaid
sequenceDiagram
    participant UE as UE5 앱
    participant JNI as Java JNI Bridge
    participant GSI as Google Sign-In SDK
    participant SRV as C++ 서버
    participant GOO as Google tokeninfo API

    UE->>JNI: LoginWithGoogle() → signIn(Activity) [JNI 호출]
    JNI->>GSI: silentSignIn() 또는 signIn 인텐트
    GSI-->>JNI: GoogleSignInAccount.getIdToken()
    JNI-->>UE: nativeOnIdToken(idToken) → GAuthSubsystem [게임 스레드]
    UE->>SRV: POST /api/auth/google/mobile {idToken}
    SRV->>GOO: GET tokeninfo?id_token={idToken}
    GOO-->>SRV: {sub, email, name, picture, aud}
    SRV->>SRV: aud == CLIENT_ID 검증
    SRV->>SRV: CALL sp_auth_upsert_google_user
    SRV-->>UE: 200 {token, nickname, userId}
    UE->>UE: SetAuthInfo(bIsGoogle=true)
    UE->>UE: OpenPreLobbyMap()
```

---

## 4. 로비 생성 → WebSocket 연결

```mermaid
flowchart TD
    A([BtnCreate 클릭]) --> B[입력값 검증\n이름·카테고리 확인]
    B --> C[POST /api/lobby\n{name, category, maxMembers}]

    C --> D{LobbyController}
    D --> E[JwtFilter: userId 추출]
    E --> F[category 화이트리스트 검증]
    F --> G[generateLobbyCode: 6자리]
    G --> H[CALL sp_lobby_create\nINSERT lobbies + lobby_members host]
    H --> I[201 {lobbyId, code, name, category}]

    I --> J[FLobbyInfo 파싱]
    J --> K[GameInstance::SetLobbyInfo\nbHost=true]
    K --> L[ConnectWebSocket\nws://server/ws/lobby/1?token=JWT]

    L --> M{JwtFilter WebSocket}
    M -- 검증 실패 --> N[연결 거부]
    M -- 검증 성공 --> O[WsSessionManager::addSession]
    O --> P[OnLobbyCreated.Broadcast]
    P --> Q([LobbyRoomMap — Host])
```

---

## 5. 로비 코드 입장 흐름

```mermaid
flowchart TD
    A([InputCode 입력 → BtnJoin]) --> B[POST /api/lobby/join\n{code:"ABC123"}]

    B --> C{LobbyController::joinLobby}
    C --> D[CALL sp_lobby_join\ncode 검색]
    D --> E{유효성 검사}
    E -- LOBBY_NOT_FOUND --> F[404 오류]
    E -- LOBBY_NOT_WAITING --> G[400 오류]
    E -- LOBBY_FULL --> H[409 오류]
    E -- 입장 가능 --> I[INSERT lobby_members\nrole='member']

    I --> J[200 {lobbyId, members:[...]}]
    J --> K[FLobbyInfo 파싱\nbHost=false]
    K --> L[ConnectWebSocket\nws://server/ws/lobby/1]
    L --> M[WsSessionManager::addSession]
    M --> N[broadcastExcept member/joined\n→ 기존 멤버들에게]
    N --> O[OnLobbyJoined.Broadcast]
    O --> P([LobbyRoomMap — Member])

    F & G & H --> Q[OnLobbyError.Broadcast\n오류 메시지 표시]
```

---

## 6. WebSocket 이벤트 흐름 (로비 내)

```mermaid
sequenceDiagram
    participant H as Host 앱
    participant SRV as Drogon WsController
    participant WSM as WsSessionManager
    participant M as Member 앱

    Note over H,M: 채팅 메시지 전송
    H->>SRV: {event:"chat/send", payload:{message:"안녕"}}
    SRV->>SRV: CALL sp_lobby_message_save
    SRV->>WSM: broadcast(lobbyId, chat/message)
    WSM->>H: {event:"chat/message", payload:{nickname:"호스트",...}}
    WSM->>M: {event:"chat/message", payload:{nickname:"호스트",...}}

    Note over H,M: Host 면접장 변경
    H->>SRV: {event:"lobby/set_category", payload:{category:"Unreal"}}
    SRV->>SRV: role='host' 검증 + UPDATE lobbies
    SRV->>WSM: broadcast lobby/category_changed
    WSM->>H: {event:"lobby/category_changed", payload:{category:"Unreal"}}
    WSM->>M: {event:"lobby/category_changed", payload:{category:"Unreal"}}

    Note over H,M: Host 인터뷰 시작
    H->>SRV: {event:"lobby/start", payload:{}}
    SRV->>SRV: CALL sp_lobby_start\n(role='host' 검증 + status='in_progress')
    SRV->>WSM: broadcast lobby/started
    WSM->>H: {event:"lobby/started", payload:{category:"Unreal",cardCount:10}}
    WSM->>M: {event:"lobby/started", payload:{category:"Unreal",cardCount:10}}
    H->>H: FetchInterviewCards → OpenInterviewMap
    M->>M: FetchInterviewCards → OpenInterviewMap
```

---

## 7. 멤버 강퇴 흐름

```mermaid
flowchart TD
    A([Host BtnKick 클릭\ntargetUserId=3]) --> B[POST /api/lobby/1/kick\n{targetUserId:3}]
    B --> C{LobbyController::kickMember}
    C --> D[CALL sp_lobby_kick\nhostId·lobbyId·targetId]
    D --> E{role='host' 검증}
    E -- NOT_HOST --> F[403 오류 응답]
    E -- 확인됨 --> G[DELETE lobby_members\nWHERE user_id=3]
    G --> H[200 {message}]
    H --> I[LobbyService → WSM::broadcast\nmember/kicked payload={userId:3}]

    I --> J{각 클라이언트 수신}
    J -- userId==자신 --> K[강퇴됨 메시지\n→ OpenPreLobbyMap]
    J -- 다른 멤버 --> L[m_memberMap.Remove\n→ RefreshMemberList]
```

---

## 8. JWT 인증 미들웨어 흐름

```mermaid
flowchart TD
    A([HTTP 또는 WebSocket 요청]) --> B{Authorization 헤더?}
    B -- 없음 --> C[401 Unauthorized]
    B -- 있음 --> D{Bearer 형식?}
    D -- 아님 --> C
    D -- 맞음 --> E[JwtUtil::Verify token]
    E -- 만료/위조 --> F[401 Token invalid]
    E -- 성공 --> G[req.attributes\nuserId · nickname 저장]
    G --> H([Controller 진입])
```

---

## 9. 인터뷰 세션 흐름

```mermaid
flowchart TD
    A[StartInterview cards] --> B{cards.Category\n== Algorithm?}
    B -- Yes --> C[UAlgorithmCardWidget\nFaceSwitcher + CodeSwitcher]
    B -- No --> D[UFlashcardWidget\nCardSwitcher]

    C & D --> E[NPCDialogueComponent\nindex=-1 → NextQuestion → 0]
    E --> F[OnQuestionChanged\nBroadcast]
    F --> G[TxtNPCSpeech 업데이트\nSetCard]

    G --> H{BtnFlip 클릭}
    H --> I[답변면 표시\nBtnKnown · BtnUnknown]

    I --> J{사용자 판정}
    J -- 알았어요 --> K[SaveProgress PUT\nbKnown=true · 백그라운드]
    J -- 몰랐어요 --> L[SaveProgress PUT\nbKnown=false]

    K & L --> M{NextQuestion}
    M -- index < deck.Num --> F
    M -- index >= deck.Num --> N[OnInterviewDone]

    N --> O[결과 화면\n점수 표시]
    O --> P[SaveInterviewSession\nPOST /api/progress/session\n트랜잭션: interview_sessions + daily_scores]

    O --> Q{버튼 선택}
    Q -- 다시 풀기 --> R[GetCachedCards\n서버 재요청 없음]
    R --> A
    Q -- 로비로 --> S[OpenPreLobbyMap]
```

---

## 10. 카드 데이터 파이프라인 (import_cards.py)

```mermaid
flowchart TD
    A1[unreal_interview_qa.xlsx] --> L1[load_unreal_xlsx\nhint=Unreal +6점]
    A2[cpp_tech_interview_flashcards.xlsx] --> L2[load_cpp_xlsx\nhint=C++ +6점]
    A3[CS_260325.csv\ncp949] --> L3[load_cs_csv]
    A4[company_interview_qa.xlsx] --> L4[load_company_xlsx\ncategory=Company 고정]
    A5[algorithm_interview_qa.xlsx] --> L5[load_algorithm_xlsx\ncategory=Algorithm 고정]

    L1 & L2 & L3 --> CLS[classify\n키워드 점수 계산]
    CLS --> SC{최고점}
    SC -- Unreal --> U[Unreal]
    SC -- C++ --> C[C++]
    SC -- CS --> S[CS]
    L4 --> CO[Company]
    L5 --> AL[Algorithm]

    U & C & S & CO & AL --> XLS[flashcards_classified.xlsx\n5개 시트]
    U & C & S & CO & AL --> DB{--db 옵션?}
    DB -- Yes --> INS[CALL sp_cards_create\n10개 파라미터]
    DB -- No --> SKIP[xlsx만 저장]
```

---

## 11. 씬(맵) 전환 전체 흐름 (v3)

```mermaid
flowchart LR
    A([앱 실행]) --> B[LoginMap]
    B -->|로컬/Google 로그인 성공| C[PreLobbyMap]
    C -->|방 만들기| D[LobbyRoomMap\nHost]
    C -->|코드 입장| E[LobbyRoomMap\nMember]
    D & E -->|Host 인터뷰 시작| F[InterviewMap]
    F -->|결과 후 로비로| C
    C -->|로그아웃| B

    subgraph GI["UStudyBotGameInstance (항상 유지)"]
        T[JWT Token]
        N[Nickname · bIsGoogle]
        LI[LobbyId · Code · IsHost]
        HR[IsHost flag]
        SC[SelectedCategory]
    end

    B -.->|SetAuthInfo| T & N
    C -.->|SetLobbyInfo| LI & HR
    D & E -.->|WebSocket 연결| LI
    F -.->|GetSelectedCategory| SC
```

---

## 12. 잔디 히트맵 데이터 흐름

```mermaid
flowchart TD
    A[인터뷰 세션 완료] --> B[SaveInterviewSession\nPOST /api/progress/session]
    B --> C[sp_progress_create_session\nSTART TRANSACTION]
    C --> D[INSERT interview_sessions]
    C --> E[UPSERT daily_scores\ncards_done += · known_count +=]
    D & E --> F[COMMIT]

    G([ContributionWidget 표시]) --> H[FetchHeatmap year=2025\nGET /api/progress/heatmap]
    H --> I[sp_progress_heatmap\nSELECT daily_scores WHERE year=2025]
    I --> J[TArray FHeatmapEntry\nscore_date · category · ratio]
    J --> K[BuildGrid\nTMap 날짜별 집계 O N]
    K --> L[53×7 UImage 타일\n5단계 색상 적용]
    L --> M[ContribGrid 표시]
```

---

## 13. NPCDialogueComponent 상태 머신

```mermaid
stateDiagram-v2
    [*] --> Idle
    Idle --> Questioning : StartInterview(cards)
    Questioning --> AnswerRevealed : RevealAnswer()
    AnswerRevealed --> Questioning : MarkKnown() 또는 MarkUnknown() → NextQuestion
    Questioning --> Done : index >= deck.Num()
    Done --> Questioning : StartInterview(cards) 재시작
    Done --> [*]

    state Questioning {
        [*] --> ShowingQuestion
        ShowingQuestion --> [*]
        note right of ShowingQuestion
            OnQuestionChanged 브로드캐스트
            bAnswerRevealed = false
            index = -1에서 시작
        end note
    }

    state AnswerRevealed {
        [*] --> WaitingJudge
        WaitingJudge --> [*]
    }
```
