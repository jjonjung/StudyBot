/*
 * Context Switch와 Oversubscription 이해하기
 * 
 * 문제 상황: Runnable thread > Hardware thread capacity
 * → Excessive context switch → Cache invalidation → Performance degradation
 */

#include <iostream>
#include <thread>
#include <vector>
#include <chrono>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <functional>

using namespace std;
using Clock = chrono::high_resolution_clock;

// ============================================================================
// ❌ 문제 코드 1: 각 작업마다 thread 직접 생성
// ============================================================================
class BadApproach_DirectThreadCreation {
public:
    /*
     * 문제점:
     * 1. 매 작업마다 thread 생성 → OS scheduler overhead
     * 2. 30개 작업 = 30개 thread → 8개 CPU core일 때 
     *    22개 thread는 대기 → context switch 폭증
     * 3. Thread 생성/소멸 비용이 작업 시간보다 클 수 있음
     */
    void ProcessTasks_DirectCreation() {
        const int TASK_COUNT = 30;
        vector<thread> threads;
        
        auto startTime = Clock::now();
        
        // ❌ 안티패턴: 매 작업마다 새로운 thread 생성
        for (int i = 0; i < TASK_COUNT; ++i) {
            threads.emplace_back([i]() {
                // CPU-bound 작업 시뮬레이션 (50ms)
                auto workStart = Clock::now();
                volatile int sum = 0;
                while (Clock::now() - workStart < chrono::milliseconds(50)) {
                    sum += i % 100;
                }
            });
        }
        
        // 모든 thread 완료 대기
        for (auto& t : threads) {
            t.join();
        }
        
        auto elapsed = Clock::now() - startTime;
        cout << "❌ Direct Creation: " 
             << chrono::duration_cast<chrono::milliseconds>(elapsed).count() 
             << "ms for " << TASK_COUNT << " tasks\n";
        cout << "   → Context switch 폭증, cache thrashing\n\n";
    }
};


// ============================================================================
// ❌ 문제 코드 2: Blocking 대기로 worker capacity 낭비
// ============================================================================
class BadApproach_BlockingWait {
public:
    /*
     * 문제점:
     * 1. wait 중에도 thread가 점유 → oversubscription
     * 2. 10개 worker 중 5개가 I/O 대기하면
     *    실질적으로는 5개 worker만 동작
     * 3. Scheduler가 대기 중인 thread도 자주 context switch
     */
    void ProcessWithBlockingWait() {
        mutex ioMutex;
        int completedTasks = 0;
        
        auto startTime = Clock::now();
        
        vector<thread> workers;
        const int WORKER_COUNT = 10;
        const int TASK_COUNT = 20;
        
        for (int w = 0; w < WORKER_COUNT; ++w) {
            workers.emplace_back([&, w]() {
                for (int t = 0; t < (TASK_COUNT / WORKER_COUNT); ++t) {
                    // CPU 작업
                    volatile int sum = 0;
                    for (int i = 0; i < 10000000; ++i) {
                        sum += i;
                    }
                    
                    // ❌ 문제: I/O 작업 후 blocking 대기
                    {
                        lock_guard<mutex> lock(ioMutex);  // I/O 시뮬레이션
                        this_thread::sleep_for(chrono::milliseconds(100));
                        
                        // 이 동안 thread는 여전히 scheduler에 의해 context switch됨
                        // 다른 thread를 위한 slot을 비지 않음
                        completedTasks++;
                    }
                }
            });
        }
        
        for (auto& t : workers) {
            t.join();
        }
        
        auto elapsed = Clock::now() - startTime;
        cout << "❌ Blocking Wait: " 
             << chrono::duration_cast<chrono::milliseconds>(elapsed).count() 
             << "ms for " << TASK_COUNT << " tasks\n";
        cout << "   → I/O 대기 중 thread가 capacity 점유 (oversubscription)\n\n";
    }
};


// ============================================================================
// ✅ 개선 코드 1: Worker Pool + Task Queue
// ============================================================================
class GoodApproach_WorkerPool {
public:
    /*
     * 개선점:
     * 1. Thread 수 = CPU core 수 (보통 hardware thread 수)
     * 2. Task queue에 작업만 추가 → thread 생성 비용 제거
     * 3. Context switch 최소화 (pool size = core count)
     */
    
    class TaskPool {
    private:
        vector<thread> workers;
        queue<function<void()>> taskQueue;
        mutex queueMutex;
        condition_variable cvTask, cvDone;
        bool shutdown = false;
        int activeTasks = 0;
        
    public:
        TaskPool(int numWorkers = 8) {
            for (int i = 0; i < numWorkers; ++i) {
                workers.emplace_back(&TaskPool::WorkerLoop, this);
            }
        }
        
        ~TaskPool() {
            Shutdown();
        }
        
        void EnqueueTask(function<void()> task) {
            {
                lock_guard<mutex> lock(queueMutex);
                if (shutdown) return;
                taskQueue.push(task);
                activeTasks++;
            }
            cvTask.notify_one();  // 대기 중인 worker 깨우기
        }
        
        void WaitForCompletion() {
            unique_lock<mutex> lock(queueMutex);
            cvDone.wait(lock, [this]() { return activeTasks == 0 && taskQueue.empty(); });
        }
        
    private:
        void WorkerLoop() {
            while (true) {
                function<void()> task;
                
                {
                    unique_lock<mutex> lock(queueMutex);
                    cvTask.wait(lock, [this]() { 
                        return !taskQueue.empty() || shutdown; 
                    });
                    
                    if (shutdown && taskQueue.empty()) break;
                    
                    if (!taskQueue.empty()) {
                        task = move(taskQueue.front());
                        taskQueue.pop();
                    }
                }
                
                if (task) {
                    task();  // 임계 영역 외에서 작업 수행
                    {
                        lock_guard<mutex> lock(queueMutex);
                        activeTasks--;
                        if (activeTasks == 0) {
                            cvDone.notify_one();
                        }
                    }
                }
            }
        }
        
        void Shutdown() {
            {
                lock_guard<mutex> lock(queueMutex);
                shutdown = true;
            }
            cvTask.notify_all();
            for (auto& t : workers) {
                t.join();
            }
        }
    };
    
    void ProcessTasks_WorkerPool() {
        TaskPool pool(8);  // 8개 hardware thread에 맞춘 worker
        const int TASK_COUNT = 30;
        
        auto startTime = Clock::now();
        
        for (int i = 0; i < TASK_COUNT; ++i) {
            pool.EnqueueTask([i]() {
                // CPU-bound 작업
                volatile int sum = 0;
                auto workStart = Clock::now();
                while (Clock::now() - workStart < chrono::milliseconds(50)) {
                    sum += i % 100;
                }
            });
        }
        
        pool.WaitForCompletion();
        
        auto elapsed = Clock::now() - startTime;
        cout << "✅ Worker Pool: " 
             << chrono::duration_cast<chrono::milliseconds>(elapsed).count() 
             << "ms for " << TASK_COUNT << " tasks\n";
        cout << "   → Context switch 최소화, thread pool size = CPU cores\n";
        cout << "   → Task queue로 작업만 추가\n\n";
    }
};


// ============================================================================
// ✅ 개선 코드 2: Async Pattern (I/O-bound 작업)
// ============================================================================
class GoodApproach_AsyncPattern {
public:
    /*
     * 개선점 (I/O-bound 패턴):
     * 1. Blocking 대신 async callback 사용
     * 2. I/O 완료될 때까지 worker thread 다른 작업 처리
     * 3. Capacity 낭비 없음 = 더 많은 parallel 작업 가능
     * 
     * 실제 구현에서는:
     * - Callback 기반 (C++)
     * - Coroutine (C++20)
     * - Unreal Async: FFunctionGraphTask
     */
    
    struct AsyncTask {
        function<void()> work;
        function<void()> onComplete;
        bool isDone = false;
    };
    
    void ProcessWithAsync() {
        const int TASK_COUNT = 20;
        const int WORKERS = 8;
        
        vector<AsyncTask> tasks(TASK_COUNT);
        queue<int> taskIndices;
        mutex taskMutex;
        condition_variable cvWorker;
        int completedCount = 0;
        mutex resultMutex;
        
        auto startTime = Clock::now();
        
        // Task 설정
        for (int i = 0; i < TASK_COUNT; ++i) {
            tasks[i].work = [i]() {
                // CPU 작업
                volatile int sum = 0;
                for (int j = 0; j < 10000000; ++j) {
                    sum += j;
                }
            };
            
            tasks[i].onComplete = [i, &resultMutex, &completedCount]() {
                // I/O 완료 후 (simulator로는 즉시)
                lock_guard<mutex> lock(resultMutex);
                completedCount++;
                cout << "  Task " << i << " completed\n";
            };
            
            {
                lock_guard<mutex> lock(taskMutex);
                taskIndices.push(i);
            }
        }
        
        // Worker threads: I/O 대기 없이 다음 작업 처리
        vector<thread> workers;
        for (int w = 0; w < WORKERS; ++w) {
            workers.emplace_back([&]() {
                while (true) {
                    int taskIdx = -1;
                    
                    {
                        unique_lock<mutex> lock(taskMutex);
                        if (!taskIndices.empty()) {
                            taskIdx = taskIndices.front();
                            taskIndices.pop();
                        }
                    }
                    
                    if (taskIdx == -1) break;
                    
                    // ✅ CPU 작업 수행 (임계영역 외)
                    tasks[taskIdx].work();
                    
                    // ✅ Callback 호출 (I/O 대신 callback)
                    tasks[taskIdx].onComplete();
                    // → I/O 대기 중 thread가 block되지 않음
                }
            });
        }
        
        for (auto& t : workers) {
            t.join();
        }
        
        auto elapsed = Clock::now() - startTime;
        cout << "✅ Async Pattern: " 
             << chrono::duration_cast<chrono::milliseconds>(elapsed).count() 
             << "ms for " << TASK_COUNT << " tasks\n";
        cout << "   → I/O 대기 중 worker는 다른 작업 처리\n";
        cout << "   → No blocking → No capacity waste\n\n";
    }
};


// ============================================================================
// 비교 분석
// ============================================================================
int main() {
    cout << "═══════════════════════════════════════════════════════════════\n";
    cout << "Context Switch & Oversubscription 실습\n";
    cout << "═══════════════════════════════════════════════════════════════\n\n";
    
    cout << "📌 시스템 정보:\n";
    cout << "   Hardware threads: " << thread::hardware_concurrency() << "\n";
    cout << "   Scenario: 30개 CPU-bound task, 8 core system\n\n";
    
    // ❌ 문제 코드들
    cout << "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n";
    cout << "❌ 문제 있는 접근\n";
    cout << "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n\n";
    
    {
        BadApproach_DirectThreadCreation bad;
        bad.ProcessTasks_DirectCreation();
    }
    
    {
        BadApproach_BlockingWait bad;
        bad.ProcessWithBlockingWait();
    }
    
    // ✅ 개선된 코드들
    cout << "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n";
    cout << "✅ 개선된 접근\n";
    cout << "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n\n";
    
    {
        GoodApproach_WorkerPool good;
        good.ProcessTasks_WorkerPool();
    }
    
    {
        GoodApproach_AsyncPattern good;
        good.ProcessWithAsync();
    }
    
    cout << "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n";
    cout << "\n📚 핵심 개념 정리\n";
    cout << "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n\n";
    
    cout << "1️⃣  Context Switch 비용\n";
    cout << "   • Register/cache 상태 저장: ~100 cycles\n";
    cout << "   • Cache miss: 다음 실행 시 데이터가 cache에 없을 수 있음\n";
    cout << "   • TLB miss: 가상→물리 주소 변환 다시 필요\n\n";
    
    cout << "2️⃣  Oversubscription 문제\n";
    cout << "   • Runnable thread > Hardware thread capacity\n";
    cout << "   • Scheduler가 자주 context switch → 성능 악화\n";
    cout << "   • 더 많은 thread ≠ 더 빠른 성능\n\n";
    
    cout << "3️⃣  해결 전략\n";
    cout << "   ├─ CPU-bound: Worker pool (size = core count)\n";
    cout << "   ├─ I/O-bound: Async/callback (blocking 피하기)\n";
    cout << "   └─ 모니터링: Unreal Insights, VTune로 context switch 확인\n\n";
    
    cout << "4️⃣  Unreal Engine 적용\n";
    cout << "   • FFunctionGraphTask로 task 추가\n";
    cout << "   • wait 구간에서 standby worker 깨우기\n";
    cout << "   • 직접 thread 생성 지양\n\n";
    
    return 0;
}
