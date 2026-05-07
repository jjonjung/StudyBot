# StudyBot

개발자 면접 스터디 플랫폼. UE5 Android 클라이언트 + C++17 서버 + MySQL + Redis 구성.

---

## 아키텍처

```
UE5 Android 클라이언트
        │
        │  HTTP REST / WebSocket
        ▼
┌─────────────────────────────────────────┐
│          Drogon C++17 서버 (포트 3000)   │
│                                         │
│  StatsFilter ── 모든 요청 응답시간 측정  │
│  JwtFilter   ── JWT 검증 + 블랙리스트    │
│                                         │
│  Controllers                            │
│  ├─ AuthController   /api/auth/*        │
│  ├─ CardsController  /api/cards/*       │
│  ├─ ProgressController /api/progress/* │
│  ├─ LobbyController  /api/lobby/*       │
│  └─ StatsController  /api/internal/*   │
│                                         │
│  WebSocket                              │
│  └─ LobbyWebSocket   /ws/lobby          │
└───────┬─────────────────────┬───────────┘
        │ SQL (비동기)         │ hiredis
        ▼                     ▼
   MySQL 8.0              Redis 6+
                              │
                    ┌─────────┴──────────┐
                    │ JWT 블랙리스트      │
                    │ 인터뷰 카드 캐시    │
                    └────────────────────┘

TcpEchoServer (포트 3001, 독립 스레드)
└─ raw POSIX 소켓 + epoll ET 모드
```

---

## 기술 스택

| 영역 | 기술 |
|------|------|
| 클라이언트 | Unreal Engine 5.6 C++, Android (Min SDK 24) |
| 서버 프레임워크 | Drogon C++17 |
| 인증 | JWT (jwt-cpp), bcrypt, Google OAuth2 (libcurl) |
| DB | MySQL 8.0 — 비동기 Prepared Statement |
| 캐시 / 세션 | Redis 6+ (hiredis) — 블랙리스트, 인터뷰 캐시 |
| 네트워크 | POSIX 소켓, epoll ET 모드, WebSocket |
| 동시성 | std::mutex, condition_variable, atomic, ThreadPool |
| 빌드 | CMake 3.20+, FetchContent (hiredis 자동 빌드) |
| 부하 테스트 | Locust 2.x |

---

## 디렉토리 구조

```
server/src/
├── main.cpp                      # 서버 진입점, Redis/TcpEchoServer 초기화
│
├── controllers/
│   ├── AuthController            # POST /api/auth/register·login·logout·google/mobile
│   ├── CardsController           # GET  /api/cards, /interview, /stats, /companies, /{id}
│   │                             # POST /api/cards
│   ├── ProgressController        # GET/PUT /api/progress/*  학습 이력·히트맵
│   ├── LobbyController           # POST/GET /api/lobby/*    로비 생성·참가·강퇴·시작
│   └── StatsController           # GET  /api/internal/stats   서버 성능 지표
│                                 # POST /api/internal/stats/reset
│                                 # GET  /api/internal/echo-stats
│
├── filters/
│   ├── JwtFilter                 # Bearer 토큰 검증 + Redis 블랙리스트 확인
│   └── StatsFilter               # 응답시간·에러율·히스토그램 실시간 집계 (atomic)
│
├── ws/
│   └── LobbyWebSocket            # /ws/lobby  실시간 채팅·멤버 입장/퇴장·게임 시작
│
├── redis/
│   └── RedisManager              # hiredis 동기 래퍼 싱글톤
│                                 # blacklistToken / isBlacklisted / set / get / del
│
├── net/
│   └── TcpEchoServer             # raw socket + epoll ET  TCP 에코 서버 (포트 3001)
│
└── util/
    └── ThreadPool                # mutex + condition_variable 스레드풀
```

---

## API 목록

### 인증 `/api/auth`

| Method | Path | 설명 | 인증 |
|--------|------|------|------|
| POST | `/register` | 회원가입 (username/password/nickname) | - |
| POST | `/login` | 로그인 → JWT 반환 | - |
| POST | `/logout` | 로그아웃 → 토큰 Redis 블랙리스트 등록 | ✅ |
| POST | `/google/mobile` | Google ID Token 검증 → JWT 반환 | - |

### 카드 `/api/cards`

| Method | Path | 설명 | 인증 |
|--------|------|------|------|
| GET | `/` | 카드 목록 (category·difficulty·company·page·limit) | ✅ |
| GET | `/interview` | 랜덤 인터뷰 카드 (Redis 60초 캐시, X-Cache 헤더) | ✅ |
| GET | `/stats` | 카테고리별 카드 수 | ✅ |
| GET | `/companies` | 회사 목록 | ✅ |
| GET | `/{id}` | 카드 단건 조회 | ✅ |
| POST | `/` | 카드 추가 | ✅ |

### 학습 진도 `/api/progress`

| Method | Path | 설명 | 인증 |
|--------|------|------|------|
| GET | `/` | 전체 학습 이력 | ✅ |
| GET | `/summary` | 카테고리별 요약 (known_count, avg_score) | ✅ |
| GET | `/sessions` | 최근 20개 인터뷰 세션 | ✅ |
| GET | `/heatmap` | 연도별 일별 학습 히트맵 | ✅ |
| PUT | `/{cardId}` | 카드 학습 결과 저장 (known, score) | ✅ |
| POST | `/session` | 인터뷰 세션 저장 | ✅ |

### 로비 `/api/lobby`

| Method | Path | 설명 | 인증 |
|--------|------|------|------|
| POST | `/` | 로비 생성 (name, category, maxMembers) | ✅ |
| POST | `/join` | 초대코드로 로비 참가 | ✅ |
| GET | `/{id}` | 로비 정보·멤버 조회 | ✅ |
| POST | `/{id}/kick` | 멤버 강퇴 (host 전용) | ✅ |
| POST | `/{id}/start` | 게임 시작 (host 전용) | ✅ |
| POST | `/{id}/close` | 로비 종료 (host 전용) | ✅ |

### WebSocket `/ws/lobby`

연결: `ws://host:3000/ws/lobby?lobbyId=1&token=<JWT>`

| event (수신) | 설명 |
|-------------|------|
| `chat/send` | 채팅 전송 |
| `lobby/set_category` | 카테고리 변경 |
| `lobby/start` | 게임 시작 (host) |
| `lobby/save_message` | 메시지 DB 저장 |

| event (송신) | 설명 |
|-------------|------|
| `member/joined` | 멤버 입장 알림 |
| `member/left` | 멤버 퇴장 알림 |
| `chat/message` | 채팅 브로드캐스트 |
| `lobby/category_changed` | 카테고리 변경 알림 |
| `lobby/started` | 게임 시작 알림 |

### 서버 내부 지표 `/api/internal`

| Method | Path | 설명 |
|--------|------|------|
| GET | `/stats` | 총요청수·평균응답시간·에러율·히스토그램·상태코드별 집계 |
| POST | `/stats/reset` | 통계 초기화 (부하테스트 전 리셋) |
| GET | `/echo-stats` | TcpEchoServer 누적 연결수·처리 바이트 |

---

## Redis 키 구조

| 키 패턴 | 용도 | TTL |
|---------|------|-----|
| `bl:<JWT토큰>` | 로그아웃 블랙리스트 | 토큰 만료 잔여시간 |
| `interview:<category>:<company>:<count>` | 인터뷰 카드 캐시 | 60초 (설정 변경 가능) |

---

## TcpEchoServer (포트 3001)

공고 요건 "TCP/UDP 네트워크·소켓 프로그래밍 이해"를 직접 증명하는 독립 컴포넌트.

- `socket()` → `bind()` → `listen()` → `accept()` 직접 구현
- `epoll` Edge-Triggered 모드: 상태 변화 시 1회 통지, 루프 내 모든 데이터 소진
- `SO_REUSEADDR`: 재시작 시 TIME_WAIT 포트 즉시 재사용
- `O_NONBLOCK`: 논블로킹 I/O로 단일 스레드에서 N개 연결 처리
- `atomic` 카운터로 lock 없이 통계 집계

```bash
# 동작 확인
echo "hello" | nc localhost 3001   # → hello 응답
curl http://localhost:3000/api/internal/echo-stats
```

---

## ThreadPool

공고 요건 "자료구조·운영체제 이해"를 직접 증명하는 유틸리티.

- `std::queue` 작업 큐를 `mutex`로 보호 (critical section 최소화)
- `condition_variable`로 유휴 스레드 슬립 → CPU 낭비 없음
- `notify_one()`으로 thundering herd 방지
- `waitAll()`로 graceful drain

---

## 빌드 (WSL Ubuntu)

```bash
# hiredis는 CMake FetchContent로 자동 다운로드 — sudo 불필요
bash /mnt/c/Users/EJ/Desktop/Fork/StudyBot/build.sh
```

수동 빌드:

```bash
rsync -a --delete /mnt/c/Users/EJ/Desktop/Fork/StudyBot/server/ ~/studybot_server/
cd ~/studybot_server && rm -rf build && mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
./studybot_server
```

---

## 부하 테스트

```bash
pip install locust

# 웹 UI (http://localhost:8089)
cd load_test && locust -f locustfile.py --host=http://localhost:3000

# CLI 자동화 (결과 CSV·HTML 저장)
bash load_test/run_test.sh

# 서버 측 실시간 지표 확인
curl http://localhost:3000/api/internal/stats | python3 -m json.tool
```

---

## 환경 설정 (`server/config.json`)

```json
{
  "listeners": [{ "address": "0.0.0.0", "port": 3000 }],
  "db_clients": [{
    "rdbms": "mysql", "host": "172.23.208.1", "port": 3306,
    "dbname": "studybot", "user": "root", "passwd": "0000",
    "connection_number": 10
  }],
  "app": {
    "jwt_secret": "studybot-secret-key",
    "google_client_id": "xxxx.apps.googleusercontent.com",
    "redis_host": "127.0.0.1",
    "redis_port": 6379,
    "redis_password": "",
    "interview_cache_ttl": 60,
    "echo_port": 3001
  }
}
```
