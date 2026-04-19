# 01. 전체 아키텍처 및 설계 선택 이유 (v3)

## 시스템 구성

```
┌─────────────────────────────────────────────────────────────────┐
│                    Android 앱 (UE 5.6)                          │
│                                                                 │
│  LoginWidget ──(로컬/Google JNI)──▶ PreLobbyMap                 │
│                                       ├─ CreateLobbyWidget      │
│                                       └─ JoinLobbyWidget        │
│                                            ↓ WebSocket 연결     │
│                                       LobbyRoomWidget           │
│                                       (채팅·멤버·준비·시작)     │
│                                            ↓ Host 시작 신호     │
│                                       InterviewMap              │
│                                       ├─ FlashcardWidget        │
│                                       ├─ AlgorithmCardWidget    │
│                                       └─ ContributionWidget     │
│                                                                 │
│  ↕ HTTP/JSON (JWT)          ↕ WebSocket (ws://)                 │
├─────────────────────────────────────────────────────────────────┤
│                  C++ 백엔드 (Drogon 프레임워크)                   │
│                                                                 │
│  AuthController   LobbyController   CardController              │
│       │                │                 │                      │
│  AuthService      LobbyService      CardService                 │
│       │                │                 │                      │
│  AuthRepository   LobbyRepository   CardRepository             │
│                        │                                        │
│               LobbyWsController ──── WsSessionManager          │
│                                                                 │
│  JwtFilter (drogon::HttpFilter) — 모든 인증 필요 라우트 적용     │
│  ↕ Drogon DbClient (mysql2 Connection Pool)                     │
├─────────────────────────────────────────────────────────────────┤
│                       MySQL 8.0                                 │
│                                                                 │
│  users / flashcards / user_progress /                           │
│  interview_sessions / daily_scores /                            │
│  lobbies / lobby_members / lobby_invites / lobby_messages       │
│                                                                 │
│  모든 접근은 Stored Procedure CALL sp_*() 경유                    │
└─────────────────────────────────────────────────────────────────┘
```

---

## 계층별 설계 이유

### 1. 백엔드를 C++ Drogon으로 전환한 이유

| 항목 | Node.js (v2) | C++ Drogon (v3) |
|------|-------------|-----------------|
| WebSocket | 별도 라이브러리(ws) 필요 | HTTP + WebSocket 단일 프레임워크 |
| 타입 안전성 | 런타임 오류 위험 | 컴파일 타임 검증 |
| 동시 접속 성능 | 싱글 스레드 이벤트 루프 | 멀티스레드 + C++20 코루틴 |
| DB 연동 | mysql2 npm | Drogon DbClient 내장 (커넥션 풀) |
| JSON | JSON.parse() 동적 | json::value 정적 처리 |
| 배포 | Node.js 런타임 필요 | 단일 바이너리, 런타임 없음 |

**Drogon 선택 근거:**
- HTTP + WebSocket을 동일 포트(3000)에서 동시 처리
- `drogon::DbClient`로 `CALL sp_*()` 비동기 실행
- C++20 코루틴 → `co_await` 스타일로 콜백 지옥 제거
- `drogon::HttpFilter` → JwtFilter 미들웨어 선언적 등록

---

### 2. UE5 GameInstanceSubsystem 패턴 유지 이유

```
AuthSubsystem, CardSubsystem, LobbySubsystem
→ UGameInstanceSubsystem 상속 (v3에서 LobbySubsystem 추가)
```

| 방법 | 문제점 |
|------|--------|
| `GameMode` | 맵 전환 시 새로 생성 → 토큰·로비 코드 소실 |
| `StaticClass 싱글톤` | GC 제외 → 메모리 위험 |
| `Actor` | 맵 전환 시 소멸 |
| **GameInstanceSubsystem** | 앱 종료 전까지 유지, 씬 전환 무관 |

씬 전환(LoginMap → PreLobbyMap → LobbyRoomMap → InterviewMap)을 거쳐도
JWT 토큰, 로비 코드, 멤버 목록이 `GameInstance` 안에서 유지됩니다.

---

### 3. Google 로그인 — JNI + Google Sign-In SDK 방식 선택 이유

v3에서 브라우저 런치 + 서버 폴링 방식을 **JNI 직접 연동 방식**으로 교체합니다.

| 방식 | UX | 복잡도 | 채택 |
|------|-----|--------|------|
| 브라우저 런치 + 서버 폴링 (v2) | 브라우저 전환 필요 | 낮음 | — |
| **JNI + play-services-auth (v3)** | 앱 내 Google 팝업 | 중간 | **✅** |
| Deep Link 콜백 | 앱 내 처리 | 높음 | — |

**JNI 방식 채택 이유:**
```
Android Google Sign-In SDK가 앱 내 팝업으로 계정 선택 처리
→ GoogleSignInAccount.getIdToken() 획득
→ JNI nativeOnIdToken() 콜백으로 UE C++ 전달
→ POST /api/auth/google/mobile {idToken}
→ 서버: GET https://oauth2.googleapis.com/tokeninfo?id_token=... 검증
→ JWT 발급 → 앱 저장
```

- 브라우저 전환 없이 앱 내에서 완결 → UX 개선
- `oauth_pending` 테이블 및 폴링 엔드포인트 불필요 → 서버 단순화
- Android 권장 방식(Credential Manager API)으로 향후 마이그레이션 용이

---

### 4. Host 기반 로비 — Lobby Host vs Listen Server Host 구분

| 개념 | 정의 | 저장 위치 | Phase |
|------|------|-----------|-------|
| **Lobby Host (방장)** | 로비를 생성한 유저, DB role='host' | `lobby_members.role` | Phase 1 |
| **Listen Server Host** | UE NM_ListenServer, 기기가 서버 역할 | UE NetMode | Phase 2 |
| **Dedicated Server** | 별도 프로세스가 게임 호스팅 | 서버 인프라 | Phase 2+ |

**Phase 1 설계 원칙:**
- Lobby Host는 DB `role='host'`로만 관리
- 모든 Host 전용 액션(강퇴·카테고리 선택·시작)은 서버에서 `role` 재검증
- 클라이언트의 `IsHost` 플래그는 UI 표시 전용, 권한 판단 금지
- 인터뷰 진행 자체는 각 클라이언트가 독립적으로 서버 API 호출 (동기화 불필요)

---

### 5. WebSocket — 폴링 대체 이유

v2의 `GET /api/auth/google/poll` 2초 폴링 방식을 WebSocket으로 일반화합니다.

| 통신 방식 | 로비 채팅 | 멤버 동기화 | 인터뷰 시작 신호 |
|-----------|-----------|-------------|-----------------|
| REST 폴링 | 2초 지연 + 불필요한 요청 | 같음 | 같음 |
| **WebSocket** | 즉시 | 즉시 | 즉시 |

```
ws://server:3000/ws/lobby/{lobbyId}?token={JWT}
→ JwtFilter에서 토큰 검증 후 handshake 허용
→ WsSessionManager: unordered_map<lobbyId, vector<WsSession>>
→ 로비 내 브로드캐스트: O(N), N = 로비 멤버 수 ≤ 6
```

**WsSessionManager 자료구조 선택 이유:**
- `unordered_map<int, vector<WsSession>>`: 로비별 세션 목록 → O(1) 로비 조회
- `unordered_map<void*, WsSession*>`: 연결→세션 역매핑 → O(1) 연결 제거
- `shared_mutex`: 읽기 다중 허용, 쓰기 배타적 → 동시 브로드캐스트 안전

---

### 6. Repository + Stored Procedure 패턴 유지 이유

Node.js → C++ 전환 후에도 DB 접근 패턴을 동일하게 유지합니다.

```cpp
// Repository 계층에서 SP 호출만 담당
Task<LobbyDto> LobbyRepository::callSpLobbyCreate(...)
{
    auto result = co_await db_->execSqlCoro(
        "CALL sp_lobby_create(?,?,?,?,?)", ...);
    ...
}
```

| 장점 | 내용 |
|------|------|
| SQL Injection 방어 | 파라미터 바인딩, 동적 문자열 조립 없음 |
| 로직 중앙화 | SQL 변경 시 SP만 교체, 서버 재배포 없음 |
| 권한 검증 일관성 | SP 내부에서 `role='host'` 체크 → 우회 불가 |
| 테스트 용이 | SP 직접 호출로 Repository 독립 테스트 |

---

### 7. Algorithm 카테고리 — 컬럼 확장 설계 (v2 유지)

```sql
-- flashcards 테이블 NULL 컬럼 확장
core_conditions  TEXT NULL
selection_reason TEXT NULL
code_cpp         MEDIUMTEXT NULL
code_csharp      MEDIUMTEXT NULL
time_complexity  VARCHAR(100) NULL
```

별도 테이블 대신 NULL 컬럼으로 처리한 이유:
- Algorithm 카드 조회마다 JOIN 불필요
- 현재 카드 수 규모에서 NULL 컬럼 오버헤드 무시 가능
- `sp_cards_interview`가 항상 전체 컬럼 SELECT → 클라이언트에서 NULL 무시

---

### 8. 잔디(Contribution Graph) — daily_scores 설계 (v2 유지)

```sql
UNIQUE KEY uq_user_cat_date (user_id, category, score_date)
ON DUPLICATE KEY UPDATE
  cards_done  = cards_done  + VALUES(cards_done),
  known_count = known_count + VALUES(known_count)
```

- 같은 날 여러 번 세션 완료 → 누적 합산
- `sp_progress_create_session`에서 트랜잭션으로 `interview_sessions INSERT` + `daily_scores UPSERT` 원자 처리

---

### 9. SOLID 원칙 적용 지침

| 원칙 | 서버 적용 | UE 적용 |
|------|-----------|---------|
| S (단일 책임) | Controller=HTTP변환, Service=비즈니스, Repository=DB | Subsystem=HTTP, Widget=표시만 |
| O (개방-폐쇄) | WsEvent 핸들러를 TMap으로 등록, 추가만 | DispatchWsEvent Handlers map에 추가만 |
| L (리스코프) | LobbyDto가 인터페이스 계약 준수 | FFlashCard 구조체 일관성 유지 |
| I (인터페이스 분리) | WsSessionManager 브로드캐스트/조회 분리 | Delegate 구독은 Widget 자율 |
| D (의존성 역전) | Service는 Repository 인터페이스 참조 | Widget은 Subsystem만 참조 |

---

## Phase 1 / Phase 2 로드맵

### Phase 1 — 현재 구현 범위

```
인증     로컬 로그인 (bcrypt + JWT) ✅
         Google JNI 로그인 ✅
로비     생성 (POST /api/lobby) ✅
         코드 입장 (POST /api/lobby/join) ✅
         채팅 (WebSocket chat/send) ✅
         면접장 선택 (WebSocket lobby/set_category) ✅
         인터뷰 시작 (WebSocket lobby/start) ✅
         멤버 강퇴 (POST /api/lobby/{id}/kick) ✅
카드     5개 카테고리 조회 ✅
         Algorithm 전용 컬럼 ✅
진도     UPSERT, 세션, 잔디 heatmap ✅
```

### Phase 2 — 다음 구현 범위

```
로비     Ready 상태 동기화
         Host 권한 위임
         Reconnect + Heartbeat (지수 백오프)
알림     Firebase FCM 로비 초대
음성     Vivox SDK 또는 Agora 연동
멀티     UE5 Listen Server 전환 (NM_ListenServer)
         Iris Replication 도입
```
