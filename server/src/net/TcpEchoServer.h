#pragma once
#include <string>
#include <atomic>
#include <thread>

// Raw POSIX 소켓 + epoll 기반 논블로킹 TCP 에코 서버
//
// 설계 목적:
//   - socket/bind/listen/accept 직접 구현으로 TCP 계층 이해 증명
//   - epoll ET(Edge-Triggered) 모드로 다수 연결을 단일 스레드에서 처리
//   - Drogon HTTP 서버와 독립적으로 별도 포트(기본 3001)에서 동작
//
// 사용:
//   TcpEchoServer echo(3001);
//   echo.start();   // 별도 스레드에서 이벤트 루프 시작
//   echo.stop();
class TcpEchoServer {
public:
    explicit TcpEchoServer(int port = 3001, int maxEvents = 64);
    ~TcpEchoServer();

    void start();
    void stop();

    // 누적 통계
    long long totalConnections() const { return totalConn_.load(); }
    long long totalBytesEchoed() const { return totalBytes_.load(); }

private:
    void eventLoop();
    void acceptClients(int epfd, int listenFd);
    void handleClient(int epfd, int fd);
    void setNonBlocking(int fd);
    void addToEpoll(int epfd, int fd);

    int  port_;
    int  maxEvents_;
    int  listenFd_{ -1 };

    std::atomic<bool>      running_{ false };
    std::thread            thread_;
    std::atomic<long long> totalConn_{ 0 };
    std::atomic<long long> totalBytes_{ 0 };
};
