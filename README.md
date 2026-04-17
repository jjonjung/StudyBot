# StudyBot

UE5 모바일 + C++ 서버 기반의 개발자 면접 스터디 플랫폼입니다.

---

## 아키텍처

```
UE5 (모바일 클라이언트)
        ↕ HTTP / WebSocket
    Drogon 서버  (C++ 웹 프레임워크)
        ↕ SQL
      MySQL DB
```

### Drogon 서버 역할

| 기능 | 설명 |
|------|------|
| REST API | `/api/auth/login`, `/api/lobby` 등 HTTP 요청 처리 |
| WebSocket | 로비 실시간 채팅, 멤버 입장/퇴장 이벤트 |
| JWT 검증 | 로그인 토큰 인증 (jwt-cpp와 함께) |
| MySQL 연결 | DB 쿼리 비동기 처리 |
| Google 로그인 | tokeninfo API 호출 (libcurl 사용) |

---

## 기술 스택

| 영역 | 기술 |
|------|------|
| 클라이언트 | Unreal Engine 5.6 (C++) |
| 서버 | Drogon (C++17), jwt-cpp, OpenSSL, libcurl |
| DB | MySQL 8.0 |
| 인증 | JWT, Google OAuth2 |
| 플랫폼 | Android (Min SDK 24) |

---

## 환경 구성 및 테스트

자세한 설치·빌드·테스트 방법은 [docs/05_TestGuide.md](docs/05_TestGuide.md) 를 참고하세요.
