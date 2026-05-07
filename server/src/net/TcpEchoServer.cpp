#include "TcpEchoServer.h"

#include <sys/socket.h>
#include <sys/epoll.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <unistd.h>
#include <cstring>
#include <cerrno>
#include <stdexcept>
#include <iostream>
#include <vector>

// ── 생성자 / 소멸자 ──────────────────────────────────────────────────────────

TcpEchoServer::TcpEchoServer(int port, int maxEvents)
    : port_(port), maxEvents_(maxEvents) {}

TcpEchoServer::~TcpEchoServer() { stop(); }

// ── 공개 인터페이스 ──────────────────────────────────────────────────────────

void TcpEchoServer::start() {
    // 1. 소켓 생성
    listenFd_ = ::socket(AF_INET, SOCK_STREAM, 0);
    if (listenFd_ < 0)
        throw std::runtime_error("[TcpEchoServer] socket() failed");

    // SO_REUSEADDR: 서버 재시작 시 TIME_WAIT 상태 포트 즉시 재사용
    int opt = 1;
    ::setsockopt(listenFd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    // 2. 바인드
    sockaddr_in addr{};
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port        = htons(static_cast<uint16_t>(port_));

    if (::bind(listenFd_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0)
        throw std::runtime_error("[TcpEchoServer] bind() failed on port " + std::to_string(port_));

    // 3. 리슨 — backlog 128: 커널이 대기시킬 연결 큐 최대 크기
    if (::listen(listenFd_, 128) < 0)
        throw std::runtime_error("[TcpEchoServer] listen() failed");

    setNonBlocking(listenFd_);

    running_ = true;
    thread_  = std::thread(&TcpEchoServer::eventLoop, this);
    std::cout << "[TcpEchoServer] listening on port " << port_ << "\n";
}

void TcpEchoServer::stop() {
    running_ = false;
    if (listenFd_ >= 0) {
        ::close(listenFd_);
        listenFd_ = -1;
    }
    if (thread_.joinable()) thread_.join();
}

// ── 이벤트 루프 ──────────────────────────────────────────────────────────────

void TcpEchoServer::eventLoop() {
    // epoll 인스턴스 생성
    int epfd = ::epoll_create1(0);
    if (epfd < 0) {
        std::cerr << "[TcpEchoServer] epoll_create1 failed\n";
        return;
    }

    addToEpoll(epfd, listenFd_);

    std::vector<epoll_event> events(maxEvents_);

    while (running_) {
        // epoll_wait: 이벤트 발생까지 최대 200ms 대기
        // timeout을 짧게 설정해 running_ 플래그를 주기적으로 확인
        int n = ::epoll_wait(epfd, events.data(), maxEvents_, 200);
        if (n < 0) {
            if (errno == EINTR) continue;  // 시그널로 인한 인터럽트는 무시
            break;
        }

        for (int i = 0; i < n; ++i) {
            int fd = events[i].data.fd;
            if (fd == listenFd_) {
                acceptClients(epfd, listenFd_);
            } else {
                handleClient(epfd, fd);
            }
        }
    }

    ::close(epfd);
}

// ── 연결 수락 ────────────────────────────────────────────────────────────────

void TcpEchoServer::acceptClients(int epfd, int listenFd) {
    // ET 모드: 이벤트 1번에 대기 중인 모든 연결을 다 accept 해야 함
    while (true) {
        sockaddr_in clientAddr{};
        socklen_t   addrLen = sizeof(clientAddr);
        int clientFd = ::accept(listenFd,
                                reinterpret_cast<sockaddr*>(&clientAddr),
                                &addrLen);
        if (clientFd < 0) {
            // EAGAIN/EWOULDBLOCK: 더 이상 수락할 연결 없음 (정상 종료 조건)
            if (errno == EAGAIN || errno == EWOULDBLOCK) break;
            std::cerr << "[TcpEchoServer] accept() error: " << strerror(errno) << "\n";
            break;
        }
        setNonBlocking(clientFd);
        addToEpoll(epfd, clientFd);
        ++totalConn_;
    }
}

// ── 클라이언트 데이터 수신 & 에코 ────────────────────────────────────────────

void TcpEchoServer::handleClient(int epfd, int fd) {
    char buf[4096];
    while (true) {
        ssize_t n = ::recv(fd, buf, sizeof(buf), 0);
        if (n > 0) {
            // 받은 데이터를 그대로 되돌려 보냄 (에코)
            ssize_t sent = 0;
            while (sent < n) {
                ssize_t w = ::send(fd, buf + sent, static_cast<size_t>(n - sent), MSG_NOSIGNAL);
                if (w <= 0) break;
                sent += w;
            }
            totalBytes_ += n;

        } else if (n == 0) {
            // 클라이언트가 연결 종료
            epoll_ctl(epfd, EPOLL_CTL_DEL, fd, nullptr);
            ::close(fd);
            break;

        } else {
            // EAGAIN: ET 모드에서 더 읽을 데이터 없음 (다음 이벤트 대기)
            if (errno == EAGAIN || errno == EWOULDBLOCK) break;
            epoll_ctl(epfd, EPOLL_CTL_DEL, fd, nullptr);
            ::close(fd);
            break;
        }
    }
}

// ── 유틸 ─────────────────────────────────────────────────────────────────────

void TcpEchoServer::setNonBlocking(int fd) {
    int flags = ::fcntl(fd, F_GETFL, 0);
    ::fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

void TcpEchoServer::addToEpoll(int epfd, int fd) {
    epoll_event ev{};
    // EPOLLIN | EPOLLET: 읽기 이벤트를 Edge-Triggered 모드로 감시
    // ET 모드는 상태 변화 시 1회만 통지 → 한 번에 모든 데이터를 처리해야 함
    ev.events  = EPOLLIN | EPOLLET;
    ev.data.fd = fd;
    ::epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &ev);
}
