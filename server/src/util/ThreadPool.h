#pragma once
#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <atomic>
#include <stdexcept>

// 고정 크기 스레드풀
//
// 설계 포인트 (OS 이해 증명):
//   - 작업 큐(std::queue)를 mutex로 보호 — critical section 최소화
//   - condition_variable로 유휴 스레드 슬립 → CPU 낭비 없음
//   - stop() 시 모든 스레드에 notify_all() → graceful shutdown
//   - 작업 제출은 enqueue(), 완료 대기는 waitAll()
class ThreadPool {
public:
    explicit ThreadPool(size_t numThreads);
    ~ThreadPool();

    // 작업 추가. 서버 종료 후 enqueue 시 예외 발생
    void enqueue(std::function<void()> task);

    // 큐에 남은 작업이 모두 완료될 때까지 블로킹
    void waitAll();

    void stop();

    size_t queueSize() const;
    size_t threadCount() const { return workers_.size(); }

private:
    std::vector<std::thread>          workers_;
    std::queue<std::function<void()>> tasks_;

    mutable std::mutex      queueMutex_;
    std::condition_variable cv_;
    std::condition_variable cvDone_;

    std::atomic<int>  activeTasks_{ 0 };
    bool              stopped_{ false };
};
