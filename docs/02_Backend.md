# 02. 백엔드 코드 흐름 및 구현 상세 (v3 — C++ Drogon)

## 파일 구조

```
server/
├── CMakeLists.txt
├── config.json.example          ← 포트·DB·JWT Secret·Google Client ID
├── main.cpp                     ← Drogon 앱 초기화, 라우트 등록
├── controllers/
│   ├── AuthController.h/.cpp    ← /api/auth/*
│   ├── LobbyController.h/.cpp   ← /api/lobby/*
│   ├── CardController.h/.cpp    ← /api/cards/*
│   ├── ProgressController.h/.cpp← /api/progress/*
│   └── LobbyWsController.h/.cpp ← WebSocket ws://*/ws/lobby/{id}
├── services/
│   ├── AuthService.h/.cpp
│   ├── LobbyService.h/.cpp
│   └── CardService.h/.cpp
├── repositories/
│   ├── AuthRepository.h/.cpp
│   ├── LobbyRepository.h/.cpp
│   └── CardRepository.h/.cpp
├── websocket/
│   └── WsSessionManager.h/.cpp  ← 연결 관리 (unordered_map)
├── middleware/
│   └── JwtFilter.h/.cpp         ← drogon::HttpFilter, Bearer 검증
├── models/
│   ├── AuthDto.h
│   ├── LobbyDto.h
│   └── CardDto.h
└── db/
    └── schema.sql               ← 전체 테이블 + Stored Procedure
```

---

## main.cpp — 진입점

```cpp
int main()
{
    auto& app = drogon::app();

    // DB 설정 (config.json에서 로드)
    app.loadConfigFile("config.json");

    // 필터 등록
    app.registerFilter<JwtFilter>();

    // 컨트롤러 자동 등록 (METHOD_LIST_BEGIN 선언 기반)
    app.run();
}
```

**Drogon 라우트 선언 방식:**
각 Controller의 `METHOD_LIST_BEGIN`에서 경로·HTTP 메서드·적용 필터를 선언합니다.
`main.cpp`는 `app.run()`만 호출하면 자동으로 등록됩니다.

---

## middleware/JwtFilter — JWT 검증

```cpp
// 모든 /api/lobby/*, /api/cards/*, /api/progress/* 에 자동 적용
class JwtFilter : public drogon::HttpFilter<JwtFilter>
{
public:
    Task<> doFilter(HttpRequestPtr req,
                    FilterCallback  fcb,
                    FilterChainCallback fccb) override
    {
        auto auth = req->getHeader("Authorization");
        if (auth.size() < 8 || auth.substr(0, 7) != "Bearer ") {
            auto resp = makeJsonResp(401, "Unauthorized");
            fcb(resp); co_return;
        }
        try {
            auto claims = JwtUtil::Verify(auth.substr(7));
            // 이후 라우터에서 req->getAttribute<int>("userId") 로 접근
            req->getAttributes()->insert("userId",   claims.userId);
            req->getAttributes()->insert("nickname", claims.nickname);
            fccb();
        } catch (...) {
            fcb(makeJsonResp(401, "Token invalid or expired"));
        }
    }
};
```

---

## controllers/AuthController

### POST /api/auth/register

```
{ username, password, nickname }
     ↓ 입력값 검증
     ↓ AuthService::RegisterLocal()
          ↓ bcrypt_hash(password, cost=10)
          ↓ CALL sp_auth_create_user(username, hash, nickname)
     ↓ 201 { id, message }
     실패(중복) → 409 { error }
```

**bcrypt cost=10 이유:** 현재 하드웨어에서 약 100ms, brute-force와 응답속도 균형점.

### POST /api/auth/login

```
{ username, password }
     ↓ CALL sp_auth_get_login_user(username)
     ↓ bcrypt_verify(password, hash)
     ↓ JwtUtil::Sign({id, nickname}, 24h)
     ↓ 200 { token, nickname, userId }
```

오류 메시지를 "invalid credentials"로 통일 → 아이디 존재 여부 유출 방지.

### POST /api/auth/google/mobile — JNI 연동 엔드포인트

```
{ idToken }   ← Android JNI에서 Google Sign-In SDK로 획득한 ID Token
     ↓ HTTPS GET googleapis.com/tokeninfo?id_token={idToken}
     ↓ aud == GOOGLE_CLIENT_ID 검증
     ↓ { sub, email, name, picture } 파싱
     ↓ CALL sp_auth_upsert_google_user(sub, email, name, picture)
          → 최초: INSERT users (google_id, email, nickname, avatar_url)
          → 재로그인: 기존 행 조회
     ↓ JwtUtil::Sign({id, nickname}, 24h)
     ↓ 200 { token, nickname, userId }
```

`GET /api/auth/google`·`GET /api/auth/google/callback`·`GET /api/auth/google/poll` 엔드포인트는
v3에서 제거합니다 (JNI 방식으로 대체).

---

## controllers/LobbyController

```cpp
class LobbyController : public drogon::HttpController<LobbyController>
{
public:
    METHOD_LIST_BEGIN
    ADD_METHOD_TO(createLobby, "/api/lobby",              Post,   "JwtFilter");
    ADD_METHOD_TO(joinLobby,   "/api/lobby/join",         Post,   "JwtFilter");
    ADD_METHOD_TO(getLobby,    "/api/lobby/{id}",         Get,    "JwtFilter");
    ADD_METHOD_TO(kickMember,  "/api/lobby/{id}/kick",    Post,   "JwtFilter");
    ADD_METHOD_TO(leaveLobby,  "/api/lobby/{id}/leave",   Post,   "JwtFilter");
    ADD_METHOD_TO(closeLobby,  "/api/lobby/{id}/close",   Delete, "JwtFilter");
    METHOD_LIST_END
    ...
};
```

### POST /api/lobby — 로비 생성

```
{ name, category, maxMembers }
     ↓ category 화이트리스트 검증
       VALID = {Unreal, C++, CS, Company, Algorithm, Mixed}
     ↓ LobbyService::CreateLobby(hostId, name, category, maxMembers)
          ↓ code = generateLobbyCode()  // 6자리 base36 랜덤 (중복 검사)
          ↓ CALL sp_lobby_create(hostId, name, category, maxMembers, code)
               → INSERT lobbies
               → INSERT lobby_members (role='host')
     ↓ 201 { lobbyId, code, name, category, maxMembers }
```

### POST /api/lobby/join — 코드로 입장

```
{ code }
     ↓ CALL sp_lobby_join(userId, code)
          → 로비 존재 확인
          → status='waiting' 확인
          → 현재 인원 < maxMembers 확인
          → INSERT lobby_members (role='member')
          → 로비 + 전체 멤버 목록 반환
     ↓ 200 { lobbyId, code, name, category, members: [...] }
     실패 → 404 LOBBY_NOT_FOUND / 409 LOBBY_FULL / 400 LOBBY_NOT_WAITING
```

### POST /api/lobby/{id}/kick — 멤버 강퇴

```
{ targetUserId }
     ↓ CALL sp_lobby_kick(hostId, lobbyId, targetUserId)
          → role='host' 검증 (아니면 SIGNAL NOT_HOST)
          → DELETE lobby_members WHERE user_id=targetUserId
     ↓ 200 { message }
     ↓ LobbyService가 WsSessionManager::broadcast(lobbyId, member/kicked)
```

---

## controllers/LobbyWsController — WebSocket

```cpp
class LobbyWsController
    : public drogon::WebSocketController<LobbyWsController>
{
    WS_PATH_LIST_BEGIN
    WS_PATH_ADD("/ws/lobby/{lobbyId}", "JwtFilter");
    WS_PATH_LIST_END
    ...
};
```

### 연결 수립 (handleNewConnection)

```
1. JwtFilter에서 검증된 userId, nickname 읽기
2. lobbyId = path parameter
3. WsSessionManager::addSession({userId, lobbyId, nickname, conn})
4. 같은 로비 전체에 member/joined 브로드캐스트
```

### 메시지 수신 (handleNewMessage)

```
raw JSON 파싱 → { event, payload }
     ↓
switch(event):
  "chat/send"          → onChatSend()
  "lobby/set_category" → onSetCategory()  [Host 전용]
  "lobby/start"        → onStartInterview()[Host 전용]
  "lobby/leave"        → onLeave()
```

### 연결 종료 (handleConnectionClosed)

```
WsSessionManager::removeSession(conn)
     ↓ 같은 로비에 member/left 브로드캐스트
     ↓ 로비 멤버 수 0이면 sp_lobby_close 호출
```

### WebSocket 이벤트 프로토콜 전체

| 방향 | 이벤트명 | 페이로드 | 설명 |
|------|----------|----------|------|
| C→S | `chat/send` | `{message}` | 채팅 전송 |
| C→S | `lobby/set_category` | `{category}` | 면접장 선택 (Host만) |
| C→S | `lobby/start` | `{}` | 인터뷰 시작 (Host만) |
| C→S | `lobby/leave` | `{}` | 명시적 퇴장 |
| C→S | `member/ready` | `{ready}` | 준비 토글 |
| S→C | `chat/message` | `{userId, nickname, message, sentAt}` | 채팅 브로드캐스트 |
| S→C | `member/joined` | `{userId, nickname, role, avatarUrl}` | 멤버 입장 |
| S→C | `member/left` | `{userId, nickname}` | 멤버 퇴장 |
| S→C | `member/kicked` | `{userId}` | 강퇴 |
| S→C | `member/ready` | `{userId, isReady}` | 준비 상태 (Phase 2) |
| S→C | `lobby/category_changed` | `{category}` | 면접장 변경 |
| S→C | `lobby/started` | `{category, cardCount}` | 인터뷰 시작 신호 |
| S→C | `lobby/closed` | `{reason}` | 로비 종료 |
| S→C | `error` | `{code, message}` | 오류 (권한 없음 등) |

---

## websocket/WsSessionManager — 연결 관리

```cpp
struct WsSession {
    int    userId;
    int    lobbyId;
    std::string nickname;
    WebSocketConnectionPtr conn;
};

class WsSessionManager {
    // 로비별 세션 목록: O(1) 로비 조회
    std::unordered_map<int, std::vector<WsSession>> lobbyMap_;
    // 연결별 역매핑: O(1) 연결 제거
    std::unordered_map<void*, WsSession*>            connMap_;
    mutable std::shared_mutex                        mutex_;

public:
    void addSession(const WsSession& session);
    void removeSession(const WebSocketConnectionPtr& conn);
    WsSession* findByConn(const WebSocketConnectionPtr& conn);
    void broadcast(int lobbyId, const Json::Value& event);
    void broadcastExcept(int lobbyId, int excludeUserId, const Json::Value& event);
};
```

**자료구조 선택 이유:**
- `unordered_map`: O(1) 평균 삽입·조회 (vector 순차 탐색 대비)
- `shared_mutex`: 브로드캐스트(읽기)는 동시 허용, 입장·퇴장(쓰기)만 배타적
- 로비당 최대 멤버 수 6명 → `vector<WsSession>` 제거는 실질 O(1)

---

## repositories/ — DB 접근 계층

```cpp
// 모든 Repository는 Drogon DbClient의 코루틴 API 사용
class LobbyRepository {
    drogon::orm::DbClientPtr db_;
public:
    Task<LobbyDto> callSpLobbyCreate(int hostId, const std::string& name,
                                     const std::string& category,
                                     int maxMembers, const std::string& code)
    {
        auto result = co_await db_->execSqlCoro(
            "CALL sp_lobby_create(?,?,?,?,?)",
            hostId, name, category, maxMembers, code);
        LobbyDto dto;
        dto.id   = result[0]["lobby_id"].as<int>();
        dto.code = result[0]["code"].as<std::string>();
        co_return dto;
    }
    ...
};
```

**Repository 계층에서 직접 JSON/HTTP 처리 금지:**
→ Service 계층만 비즈니스 로직, Controller 계층만 HTTP 변환.

---

## CardController — 기존 기능 유지

### GET /api/cards

```
?category=Unreal&difficulty=Normal&page=1&limit=20
     ↓ category 화이트리스트: [Unreal, C++, CS, Company, Algorithm]
     ↓ CALL sp_cards_list(category, difficulty, company, limit, offset)
     ↓ 페이징 결과 반환
```

### GET /api/cards/interview

```
?category=Algorithm&count=10
     ↓ CALL sp_cards_interview(category, company, count)
       → ORDER BY RAND() LIMIT count
     ↓ Algorithm 카테고리: core_conditions, code_cpp 등 포함
```

### GET /api/cards/companies

```
CALL sp_cards_companies()
→ SELECT DISTINCT company FROM flashcards WHERE category='Company'
```

---

## ProgressController — 기존 기능 유지

### PUT /api/progress/:cardId

```
CALL sp_progress_upsert(userId, cardId, known, score)
→ INSERT ... ON DUPLICATE KEY UPDATE (단일 쿼리 UPSERT)
```

### POST /api/progress/session

```
CALL sp_progress_create_session(userId, category, total, known)
→ 트랜잭션: interview_sessions INSERT + daily_scores UPSERT 누적
```

### GET /api/progress/heatmap

```
?year=2025
→ CALL sp_progress_heatmap(userId, year)
→ { score_date, category, cards_done, known_count, ratio }
→ UContributionWidget이 53×7 그리드로 렌더링
```

---

## DB 스키마 요약

```
users              로컬(username+hash) + Google(google_id) 통합
flashcards         5개 카테고리, Algorithm 전용 5개 NULL 컬럼
user_progress      카드별 최신 학습 결과 (UPSERT)
interview_sessions 인터뷰 세션 이력 (성장 그래프)
daily_scores       날짜별 누적 학습량 (잔디 히트맵)
oauth_pending      ← v3에서 제거 (JNI 방식으로 대체)
lobbies            로비 기본 정보 (code, name, category, status)
lobby_members      로비 멤버 목록 (role: host/member)
lobby_invites      초대 링크 (향후 확장)
lobby_messages     채팅 이력 (WebSocket 메시지 영구 저장)
```

### users 테이블 — 로컬/Google 통합 설계

```sql
username      VARCHAR(50) NULL   -- 로컬 계정만
password_hash VARCHAR(255) NULL  -- 로컬 계정만
google_id     VARCHAR(100) NULL UNIQUE  -- Google 계정만
email, avatar_url               -- Google 계정에서 수신
```

두 방식 모두 로그인 후 동일한 JWT 구조로 통합됩니다.

### 외래키 CASCADE

```sql
FOREIGN KEY (user_id) REFERENCES users(id) ON DELETE CASCADE
```

회원 탈퇴 시 진도·세션·잔디·로비 멤버십이 자동 삭제됩니다.

---

## API 엔드포인트 전체 목록

| 메서드 | 경로 | 인증 | 설명 |
|--------|------|------|------|
| POST | /api/auth/register | ✗ | 로컬 회원가입 |
| POST | /api/auth/login | ✗ | 로컬 로그인 → JWT |
| POST | /api/auth/google/mobile | ✗ | Google ID Token 검증 → JWT |
| POST | /api/lobby | ✓ | 로비 생성 |
| POST | /api/lobby/join | ✓ | 코드로 입장 |
| GET | /api/lobby/{id} | ✓ | 로비 정보 + 멤버 조회 |
| POST | /api/lobby/{id}/kick | ✓ | 멤버 강퇴 (Host 전용) |
| POST | /api/lobby/{id}/leave | ✓ | 로비 퇴장 |
| DELETE | /api/lobby/{id}/close | ✓ | 로비 종료 (Host 전용) |
| WS | /ws/lobby/{id}?token= | ✓ | 실시간 채팅·동기화 |
| GET | /api/cards | ✓ | 카드 목록 (필터·페이징) |
| GET | /api/cards/interview | ✓ | 랜덤 카드 N장 |
| GET | /api/cards/stats | ✓ | 카테고리별 카드 수 |
| GET | /api/cards/companies | ✓ | 등록 기업 목록 |
| GET | /api/cards/:id | ✓ | 카드 단건 조회 |
| POST | /api/cards | ✓ | 카드 추가 |
| GET | /api/progress | ✓ | 학습 진도 전체 |
| GET | /api/progress/summary | ✓ | 카테고리별 요약 |
| PUT | /api/progress/:cardId | ✓ | 카드 결과 저장/갱신 |
| POST | /api/progress/session | ✓ | 세션 저장 + 잔디 누적 |
| GET | /api/progress/sessions | ✓ | 세션 이력 |
| GET | /api/progress/heatmap | ✓ | 연간 잔디 데이터 |
| GET | /health | ✗ | 서버 상태 확인 |
