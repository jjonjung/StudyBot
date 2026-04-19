# 06. C++ Drogon 서버 구축 로그 (2026-04-18)

## 개요

Windows 11 환경에서 WSL을 이용해 Drogon C++ 서버를 구축하고,
MySQL 연결 및 REST API / WebSocket 동작까지 확인한 전체 과정입니다.

---

## 1. WSL 설치

> **실행 환경: PowerShell (관리자 권한으로 실행)**

```powershell
wsl --install
```

- 재부팅 후 Ubuntu 앱 자동 설치
- 시작 메뉴에서 **Ubuntu** 앱 실행 → 사용자 이름 / 비밀번호 설정
- 이후 모든 서버 작업은 WSL bash 터미널에서 진행

---

## 2. 빌드 도구 및 의존성 설치

> **실행 환경: WSL bash**

```bash
sudo apt update && sudo apt upgrade -y
sudo apt install -y cmake libssl-dev libcurl4-openssl-dev libjsoncpp-dev
sudo apt install -y libmysqlclient-dev uuid-dev g++ build-essential
sudo apt install -y libmariadb-dev mysql-client-core-8.0
```

---

## 3. Drogon 빌드 및 설치

Drogon은 Linux 전용 비동기 API(`epoll`, `_start`/`_cont`)를 사용하므로
WSL 환경에서 빌드해야 합니다.

> **주의**: Ubuntu의 `libmysqlclient-dev`(Oracle MySQL)는 비동기 API 미지원.
> 반드시 `libmariadb-dev`를 설치한 후 빌드해야 합니다.

```bash
cd ~
git clone https://github.com/drogonframework/drogon
cd drogon && git submodule update --init
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release -DBUILD_MYSQL=ON \
  -DMYSQL_INCLUDE_DIRS=/usr/include/mariadb \
  -DMYSQL_LIBRARIES=/usr/lib/x86_64-linux-gnu/libmariadb.so
make -j$(nproc) && sudo make install
```

---

## 4. jwt-cpp 설치

```bash
cd ~
git clone https://github.com/Thalhammer/jwt-cpp
cd jwt-cpp && mkdir build && cd build
cmake .. && sudo make install
```

---

## 5. libbcrypt 설치

Ubuntu에 `libbcrypt-dev` 패키지가 없으므로 소스에서 빌드합니다.

```bash
cd ~
git clone https://github.com/trusch/libbcrypt
cd libbcrypt && mkdir build && cd build
cmake .. && make && sudo make install
```

---

## 6. MySQL 연결 설정 (WSL ↔ Windows MySQL)

Windows에 MySQL 8.0이 설치된 경우 WSL에서 직접 `127.0.0.1`로 접속이 안 됩니다.
아래 절차로 연결합니다.

### 6-1. Windows MySQL 서비스 시작

> **실행 환경: CMD (관리자 권한)**

```cmd
net start MySQL80
```

### 6-2. WSL에서 Windows 호스트 IP 확인

```bash
ip route | grep default | awk '{print $3}'
# 예: 172.23.208.1
```

### 6-3. MySQL root 외부 접속 허용

> **실행 환경: CMD (관리자 권한)**

```cmd
"C:\Program Files\MySQL\MySQL Server 8.0\bin\mysql.exe" -u root -p
```

```sql
CREATE USER 'root'@'%' IDENTIFIED BY '비밀번호';
GRANT ALL PRIVILEGES ON *.* TO 'root'@'%' WITH GRANT OPTION;
ALTER USER 'root'@'%' IDENTIFIED BY '비밀번호';
FLUSH PRIVILEGES;
```

### 6-4. WSL에서 접속 확인

```bash
mysql -h 172.23.208.1 -u root -p
```

---

## 7. DB 초기화

```bash
mysql -h 172.23.208.1 -u root -p -e \
  "CREATE DATABASE IF NOT EXISTS studybot CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci;"

mysql -h 172.23.208.1 -u root -p < \
  /mnt/c/Users/EJ/Desktop/Fork/StudyBot/backend/db/schema.sql
```

생성된 테이블 확인:

```bash
mysql -h 172.23.208.1 -u root -p -e "USE studybot; SHOW TABLES;"
```

```
+--------------------+
| Tables_in_studybot |
+--------------------+
| daily_scores       |
| flashcards         |
| interview_sessions |
| lobbies            |
| lobby_invites      |
| lobby_members      |
| lobby_messages     |
| oauth_pending      |
| user_progress      |
| users              |
+--------------------+
```

---

## 8. C++ 서버 소스 구조

```
server/
├── CMakeLists.txt
├── config.json
└── src/
    ├── main.cpp
    ├── controllers/
    │   ├── AuthController.h / .cpp     (회원가입, 로그인, Google 로그인)
    │   ├── CardsController.h / .cpp    (카드 조회, 인터뷰, 통계)
    │   ├── LobbyController.h / .cpp    (로비 생성/입장/강퇴/시작)
    │   └── ProgressController.h / .cpp (진도 저장, 잔디, 세션)
    ├── filters/
    │   └── JwtFilter.h / .cpp          (JWT 인증 미들웨어)
    └── ws/
        └── LobbyWebSocket.h / .cpp     (실시간 채팅/이벤트)
```

---

## 9. 서버 빌드

> **주의**: `/mnt/c/` 경로에서 CMake 빌드 시 권한 오류 발생.
> 반드시 WSL 홈 디렉토리(`~`)로 복사 후 빌드해야 합니다.

```bash
cp -r /mnt/c/Users/EJ/Desktop/Fork/StudyBot/server ~/studybot_server
cd ~/studybot_server && mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
```

---

## 10. 서버 실행 및 동작 확인

```bash
cd ~/studybot_server/build
./studybot_server &
```

### 헬스체크

```bash
curl http://localhost:3000/health
# {"status":"ok","version":"3.0.0"}
```

### 회원가입

```bash
curl -X POST http://localhost:3000/api/auth/register \
  -H "Content-Type: application/json" \
  -d '{"username":"host1","password":"test1234","nickname":"호스트"}'
# {"id":1,"message":"registered"}
```

### 로그인

```bash
curl -X POST http://localhost:3000/api/auth/login \
  -H "Content-Type: application/json" \
  -d '{"username":"host1","password":"test1234"}'
# {"nickname":"호스트","token":"eyJ...","userId":1}
```

### 로비 생성

```bash
TOKEN=$(curl -s -X POST http://localhost:3000/api/auth/login \
  -H "Content-Type: application/json" \
  -d '{"username":"host1","password":"test1234"}' \
  | grep -o '"token":"[^"]*"' | cut -d'"' -f4)

curl -X POST http://localhost:3000/api/lobby \
  -H "Content-Type: application/json" \
  -H "Authorization: Bearer $TOKEN" \
  -d '{"name":"스터디룸A","category":"Algorithm","maxMembers":4}'
# {"category":"Algorithm","code":"ZR3PZN","lobbyId":1,"maxMembers":4,"name":"스터디룸A"}
```

### WebSocket 채팅 테스트

```bash
# websocat 설치
wget -O ~/websocat \
  https://github.com/vi/websocat/releases/download/v1.13.0/websocat.x86_64-unknown-linux-musl
chmod +x ~/websocat

# 연결
~/websocat "ws://localhost:3000/ws/lobby?lobbyId=1&token=$TOKEN"

# 메시지 전송 (연결 후 입력)
{"event":"chat/send","payload":{"message":"hello"}}
# 수신: {"event":"chat/message","payload":{"message":"hello","nickname":"","userId":1}}
```

---

## 11. 트러블슈팅

| 증상 | 원인 | 해결 |
|------|------|------|
| `git submodule` 권한 오류 | `/mnt/c/` 경로 제한 | WSL 홈(`~`)에서 작업 |
| `No CMAKE_CXX_COMPILER` | g++ 미설치 | `sudo apt install g++ build-essential` |
| `Could not find UUID` | uuid-dev 미설치 | `sudo apt install uuid-dev` |
| `MySql was not found` | Oracle MySQL 비동기 API 미지원 | `libmariadb-dev` 설치 후 경로 직접 지정 |
| `MYSQL_OPT_NONBLOCK` 오류 | Oracle MySQL 클라이언트 사용 | MariaDB 클라이언트로 교체 |
| `No database supported` | Drogon MySQL 지원 없이 빌드됨 | `-DBUILD_MYSQL=ON` + MariaDB 경로 지정 후 재빌드 |
| WSL에서 MySQL 접속 불가 | Windows IP가 `127.0.0.1` 아님 | `ip route`로 호스트 IP 확인 후 사용 |
| JWT 한글 크래시 | jwt-cpp 멀티바이트 처리 문제 | nickname 클레임 제거, 로그인 응답값 클라이언트 캐싱 |
| WebSocket 404 | Drogon WS 경로 파라미터 미지원 | `{lobbyId}` 제거, 쿼리 파라미터(`?lobbyId=`)로 변경 |
