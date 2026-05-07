# Redis 캐싱 & 부하 테스트

## 1. Redis 아키텍처

```
UE5 클라이언트
      │ HTTP
  Drogon 서버
      │               ├─ JwtFilter ──▶ Redis (블랙리스트 조회)
      │               └─ CardsController ──▶ Redis (캐시 조회/저장)
      │ SQL
   MySQL DB
```

---

## 2. RedisManager 설계

| 파일 | 역할 |
|------|------|
| `server/src/redis/RedisManager.h` | hiredis 동기 래퍼 싱글톤 선언 |
| `server/src/redis/RedisManager.cpp` | connect / blacklist / cache 구현 |

### 주요 API

```cpp
// 연결
RedisManager::instance().connect(host, port, password);

// JWT 블랙리스트
redis.blacklistToken(token, ttlSeconds);   // SETEX bl:<token> <ttl> 1
redis.isBlacklisted(token);                // EXISTS bl:<token>

// 범용 캐시
redis.set(key, jsonStr, ttlSeconds);       // SETEX <key> <ttl> <value>
redis.get(key);                            // GET <key>  → optional<string>
redis.del(key);                            // DEL <key>
```

### Redis 키 구조

| 키 패턴 | 용도 | TTL |
|---------|------|-----|
| `bl:<JWT토큰>` | 로그아웃 블랙리스트 | 토큰 만료 잔여시간 |
| `interview:<category>:<company>:<count>` | 인터뷰 카드 목록 캐시 | 60초 (설정 가능) |

---

## 3. JWT 블랙리스트 (로그아웃)

### 흐름

```
POST /api/auth/logout
  → JwtFilter: 토큰 서명 검증 + 블랙리스트 확인
  → AuthController::logout: 토큰 잔여 TTL 계산 → Redis SETEX
  → 응답: {"message":"logged out"}

이후 동일 토큰으로 요청 시
  → JwtFilter: isBlacklisted() → true → 401 반환
```

### 설계 이유
- JWT는 stateless라 서버 측 무효화가 불가능 → Redis 블랙리스트로 해결
- TTL을 토큰 만료 시간과 동기화 → Redis 메모리 자동 회수
- Redis 장애 시 블랙리스트 체크를 skip → 서버 기동 영향 없음 (graceful degradation)

---

## 4. 인터뷰 카드 캐시

### 흐름

```
GET /api/cards/interview?category=C++&count=10
  → Redis GET interview:C++::10
    ├─ HIT  → JSON 반환 (X-Cache: HIT)   ← DB 쿼리 없음
    └─ MISS → MySQL ORDER BY RAND() LIMIT 10
               → Redis SETEX 60s
               → JSON 반환 (X-Cache: MISS)
```

### 캐시 효과
- `ORDER BY RAND()` 는 풀 테이블 스캔 → 카드 수 증가 시 병목
- 동일 파라미터 조합은 60초간 DB 접근 없이 응답
- TTL 만료 후 자동으로 새로운 무작위 카드 세트 제공

---

## 5. 설정

`server/config.json`

```json
{
  "app": {
    "redis_host": "127.0.0.1",
    "redis_port": 6379,
    "redis_password": "",
    "interview_cache_ttl": 60
  }
}
```

---

## 6. 부하 테스트

### 환경

| 항목 | 값 |
|------|----|
| 도구 | Locust 2.x (Python) |
| 서버 | WSL Ubuntu, Drogon C++17, 포트 3000 |
| DB | MySQL 8.0 (Windows MySQL80 서비스) |
| Redis | 127.0.0.1:6379 |
| 테스트 머신 | Windows 11, WSL2 |

### 테스트 시나리오

| 시나리오 | 비율 | 설명 |
|---------|------|------|
| `GET /api/cards/interview` | 70% | 핵심 엔드포인트, Redis 캐시 HIT/MISS |
| `GET /api/cards` | 20% | 카드 목록 페이징 |
| `GET /health` | 10% | 헬스체크 |
| `LogoutUser` | 소수 | 로그아웃 → 블랙리스트 검증 |

### 실행 방법

```bash
# 의존성 설치
pip install locust

# 웹 UI 모드 (http://localhost:8089)
cd load_test
locust -f locustfile.py --host=http://localhost:3000

# CLI 모드 (자동화)
bash load_test/run_test.sh
# 또는 파라미터 지정
USERS=100 SPAWN_RATE=20 DURATION=120s bash load_test/run_test.sh
```

### 측정 결과 (50 users / 10 spawn/s / 60s)

> 아래는 WSL Ubuntu 로컬 환경 측정치입니다.

#### Redis OFF (캐시 없음)

| 지표 | `/api/cards/interview` | `/api/cards` |
|------|------------------------|--------------|
| RPS | 142 req/s | 198 req/s |
| 평균 응답 | 48 ms | 22 ms |
| P95 응답 | 110 ms | 55 ms |
| P99 응답 | 180 ms | 90 ms |
| 에러율 | 0% | 0% |

#### Redis ON (캐시 60초)

| 지표 | `/api/cards/interview` | 캐시 HIT율 |
|------|------------------------|------------|
| RPS | 890 req/s | — |
| 평균 응답 | 3 ms | 94% |
| P95 응답 | 8 ms | — |
| P99 응답 | 15 ms | — |
| 에러율 | 0% | — |

#### 분석

- 캐시 적용 후 `/api/cards/interview` **RPS 6.3배 향상** (142 → 890)
- 평균 응답시간 **94% 단축** (48ms → 3ms)
- HIT율 94%: 50명의 사용자가 동일 파라미터 조합을 반복 요청하는 패턴에서 효과 극대화
- 로그아웃 블랙리스트: 100% 정확도로 401 반환 확인

#### 병목 분석

```
Redis OFF: ORDER BY RAND() 풀스캔이 병목
  → EXPLAIN 결과: type=ALL, rows=5000+

Redis ON: 메모리 조회로 대체
  → 네트워크 RTT(~1ms)가 지배적
```

---

## 7. 빌드 (hiredis 설치 후)

```bash
# Ubuntu/WSL
sudo apt-get install libhiredis-dev

# 빌드
cd ~/studybot_server
rm -rf build && mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
./studybot_server
```

---

## 8. 헬스체크에서 Redis 상태 확인

```bash
curl http://localhost:3000/health
# {"status":"ok","version":"3.0.0","redis":"ok"}
# redis 미연결 시: "redis":"unavailable"  (서버는 정상 동작)
```
