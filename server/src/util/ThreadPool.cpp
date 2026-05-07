#include "ThreadPool.h"

ThreadPool::ThreadPool(size_t numThreads) {
    workers_.reserve(numThreads);
    for (size_t i = 0; i < numThreads; ++i) {
        workers_.emplace_back([this] {
            while (true) {
                std::function<void()> task;
                {
                    std::unique_lock<std::mutex> lock(queueMutex_);
                    // 작업이 생기거나 종료 신호가 올 때까지 슬립
                    cv_.wait(lock, [this] {
                        return stopped_ || !tasks_.empty();
                    });
                    if (stopped_ && tasks_.empty()) return;
                    task = std::move(tasks_.front());
                    tasks_.pop();
                }
                ++activeTasks_;
                task();
                // 작업 완료 후 대기 중인 waitAll()에 알림
                if (--activeTasks_ == 0) {
                    std::lock_guard<std::mutex> lock(queueMutex_);
                    if (tasks_.empty()) cvDone_.notify_all();
                }
            }
        });
    }
}

ThreadPool::~ThreadPool() { stop(); }

void ThreadPool::enqueue(std::function<void()> task) {
    {
        std::lock_guard<std::mutex> lock(queueMutex_);
        if (stopped_) throw std::runtime_error("ThreadPool: enqueue on stopped pool");
        tasks_.push(std::move(task));
    }
    cv_.notify_one();  // 유휴 스레드 1개만 깨움 — thundering herd 방지
}

void ThreadPool::waitAll() {
    std::unique_lock<std::mutex> lock(queueMutex_);
    cvDone_.wait(lock, [this] {
        return tasks_.empty() && activeTasks_.load() == 0;
    });
}

void ThreadPool::stop() {
    {
        std::lock_guard<std::mutex> lock(queueMutex_);
        stopped_ = true;
    }
    cv_.notify_all();  // 모든 스레드를 깨워 종료 조건 확인
    for (auto& t : workers_)
        if (t.joinable()) t.join();
}

size_t ThreadPool::queueSize() const {
    std::lock_guard<std::mutex> lock(queueMutex_);
    return tasks_.size();
}
