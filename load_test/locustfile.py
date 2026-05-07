"""
StudyBot 서버 부하 테스트
대상: /api/cards/interview  (Redis 캐시 HIT/MISS 비교)

실행:
  locust -f locustfile.py --host=http://localhost:3000

웹 UI: http://localhost:8089
CLI:   locust -f locustfile.py --host=http://localhost:3000 \
               --headless -u 50 -r 10 --run-time 60s \
               --csv=results/result
"""
import json
import time
import random
from locust import HttpUser, task, between, events

# ──────────────────────────────────────────────
# 전역 통계 (캐시 HIT/MISS 집계)
# ──────────────────────────────────────────────
cache_stats = {"hit": 0, "miss": 0}

@events.request.add_listener
def on_request(response_time, response, **kwargs):
    if response is not None:
        header = response.headers.get("X-Cache", "")
        if header == "HIT":
            cache_stats["hit"] += 1
        elif header == "MISS":
            cache_stats["miss"] += 1

@events.test_stop.add_listener
def on_test_stop(environment, **kwargs):
    total = cache_stats["hit"] + cache_stats["miss"]
    ratio = cache_stats["hit"] / total * 100 if total else 0
    print(f"\n[Cache Stats] HIT={cache_stats['hit']} MISS={cache_stats['miss']} "
          f"HIT_RATE={ratio:.1f}%")


# ──────────────────────────────────────────────
# 인증 헬퍼
# ──────────────────────────────────────────────
TEST_USERS = [
    {"username": "load_user1", "password": "test1234"},
    {"username": "load_user2", "password": "test1234"},
    {"username": "load_user3", "password": "test1234"},
]

CATEGORIES = ["C++", "CS", "Algorithm", "Unreal", ""]
COUNTS     = [5, 10, 20]


# ──────────────────────────────────────────────
# 유저 행동 시나리오
# ──────────────────────────────────────────────
class StudyBotUser(HttpUser):
    wait_time = between(0.5, 2.0)
    token: str = ""

    def on_start(self):
        """세션 시작 시 로그인하여 JWT 획득"""
        cred = random.choice(TEST_USERS)
        res = self.client.post(
            "/api/auth/login",
            json=cred,
            name="/api/auth/login",
        )
        if res.status_code == 200:
            self.token = res.json().get("token", "")
        else:
            # 계정이 없으면 먼저 등록
            self.client.post("/api/auth/register", json=cred)
            res2 = self.client.post("/api/auth/login", json=cred)
            if res2.status_code == 200:
                self.token = res2.json().get("token", "")

    def _auth(self):
        return {"Authorization": f"Bearer {self.token}"}

    # ── 메인 시나리오: 인터뷰 카드 요청 (70%) ──────────────────
    @task(7)
    def get_interview_cards(self):
        category = random.choice(CATEGORIES)
        count    = random.choice(COUNTS)
        params   = {"count": count}
        if category:
            params["category"] = category

        self.client.get(
            "/api/cards/interview",
            params=params,
            headers=self._auth(),
            name="/api/cards/interview",
        )

    # ── 카드 목록 페이징 (20%) ────────────────────────────────
    @task(2)
    def list_cards(self):
        category = random.choice(CATEGORIES)
        params   = {"page": random.randint(1, 3), "limit": 20}
        if category:
            params["category"] = category

        self.client.get(
            "/api/cards",
            params=params,
            headers=self._auth(),
            name="/api/cards",
        )

    # ── 헬스체크 (10%) ───────────────────────────────────────
    @task(1)
    def health_check(self):
        self.client.get("/health", name="/health")


# ──────────────────────────────────────────────
# 로그아웃 포함 시나리오 (캐시 블랙리스트 검증)
# ──────────────────────────────────────────────
class LogoutUser(HttpUser):
    """로그인 → 카드 조회 → 로그아웃 → 재요청 거부 확인"""
    wait_time = between(1, 3)
    weight    = 1  # 전체 유저 중 소수만 이 시나리오

    def on_start(self):
        cred = {"username": "logout_tester", "password": "test1234"}
        self.client.post("/api/auth/register", json=cred)
        res = self.client.post("/api/auth/login", json=cred)
        self.token = res.json().get("token", "") if res.status_code == 200 else ""

    @task
    def logout_flow(self):
        if not self.token:
            return
        headers = {"Authorization": f"Bearer {self.token}"}

        # 로그아웃 전 정상 요청
        self.client.get("/api/cards/interview",
                        headers=headers, name="[logout] before logout")

        # 로그아웃
        self.client.post("/api/auth/logout",
                         headers=headers, name="[logout] POST /logout")

        # 블랙리스트된 토큰으로 재요청 → 401 기대
        with self.client.get("/api/cards/interview",
                             headers=headers,
                             name="[logout] after logout (expect 401)",
                             catch_response=True) as res:
            if res.status_code == 401:
                res.success()
            else:
                res.failure(f"expected 401, got {res.status_code}")

        # 재로그인으로 새 토큰 획득
        cred = {"username": "logout_tester", "password": "test1234"}
        r2 = self.client.post("/api/auth/login", json=cred)
        if r2.status_code == 200:
            self.token = r2.json().get("token", "")
