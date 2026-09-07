# Context Switch & Oversubscription - 코드 분석 가이드

## 🎯 학습 흐름

**이전**: NUMA, thread affinity  
**오늘**: **Context switch, oversubscription** ← 지금 여기  
**다음**: synchronization wait, blocking

---

## 1️⃣ Context Switch 비용 분석

### 개념
OS scheduler가 CPU에서 실행 중인 thread를 바꿀 때:

```
1. 현재 thread 상태 저장
   └─ CPU 레지스터
   └─ 프로그램 카운터 (PC)
   └─ 스택 포인터
   
2. 다음 thread 선택
   └─ Scheduler 알고리즘 적용
   
3. 다음 thread 상태 복원
   └─ 레지스터 로드
   
4. 실행 재개
```

### 숨겨진 비용

| 항목 | 설명 | 영향 |
|------|------|------|
| **L1/L2 캐시 무효화** | 이전 thread 데이터 제거 | 메모리 레이턴시 100배 증가 |
| **TLB (Translation Lookaside Buffer) 플러시** | 가상→물리 주소 매핑 초기화 | 페이지 테이블 재조회 필요 |
| **Branch predictor 버림** | CPU 분기 예측 리셋 | 파이프라인 스톨 |
| **Scheduler overhead** | Context switch 자체 시간 | ~100-1000 CPU cycles |

### 코드에서 보기

```cpp
// ❌ 문제: 30개 thread, 8개 core
// Scheduler가 30/8 = 3.75배 빈번하게 context switch 수행
for (int i = 0; i < 30; ++i) {
    threads.emplace_back([i]() {
        // 작업...
    });
}
// → 각 thread는 자주 preempted (시간 할당량 종료 등)
// → 캐시 재가열 비용 반복
```

---

## 2️⃣ Oversubscription 문제

### 정의
**Runnable thread 수 > Hardware thread capacity**

```
예시:
Hardware: 8 cores
Runnable threads: 30 개
→ 22개는 항상 대기 또는 context switch 진행 중
```

### 성능 악화 메커니즘

```
더 많은 thread 생성
    ↓
Context switch 빈도 증가 (8개 core가 30개 관리)
    ↓
각 thread의 실행 시간 조각화 (time slice 짧아짐)
    ↓
캐시 데이터 재사용 기회 감소
    ↓
Context switch + cache miss 악순환
    ↓
🔴 전체 성능 저하 (더 느려짐!)
```

### 코드에서 보기

```cpp
// ❌ Blocking wait로 인한 oversubscription
for (int w = 0; w < 10; ++w) {  // 10개 worker
    workers.emplace_back([&]() {
        for (int t = 0; t < tasks; ++t) {
            DoWork();
            
            // ❌ 여기서 blocking!
            lock_guard<mutex> lock(ioMutex);
            this_thread::sleep_for(100ms);  // I/O 시뮬레이션
            // → 이 thread는 여전히 scheduler 관리 대상
            // → 다른 thread 실행할 capacity를 점유
        }
    });
}
```

---

## 3️⃣ 나쁜 코드 패턴 분석

### 패턴 1: 무분별한 Thread 생성

```cpp
❌ BadApproach_DirectThreadCreation::ProcessTasks_DirectCreation()
```

**문제점:**
- 매 작업마다 새로운 thread 객체 생성
- OS thread 생성 비용: ~1-10μs (작은 작업 시 무시할 수 없음)
- 30개 thread × 생성 비용 = 심각한 오버헤드

**Performance Impact:**
```
실행 시간: 169ms (단일 core 환경에서)
예상: 30개 task × 50ms = 1500ms
실제: 169ms (여러 core이므로)

하지만 thread 생성 + 소멸 비용 포함되어 있음
→ Task 시간이 더 길면 문제 덜함
→ Task가 매우 짧으면 생성 비용이 큼
```

**메모리 오버헤드:**
```cpp
// 각 thread는 메모리 점유:
// - Stack space: 1MB (Windows), 2MB (Linux)
// - TCB (Thread Control Block): ~1KB
// 30개 thread = 30-60MB 메모리
```

---

### 패턴 2: Blocking Wait

```cpp
❌ BadApproach_BlockingWait::ProcessWithBlockingWait()
```

**문제점:**

```cpp
lock_guard<mutex> lock(ioMutex);          // ← 임계영역 진입
this_thread::sleep_for(100ms);            // ← I/O 대기
completedTasks++;
// ← lock 해제

// 그 100ms 동안 thread는?
// ✗ CPU 작업 안 함
// ✗ 다른 thread에게 slot 양보 안 함
// ✓ Scheduler는 여전히 이 thread를 관리
```

**Capacity 낭비 메커니즘:**

```
10개 worker, 각각 작업 구조:
1. CPU work (30ms)
2. I/O wait (100ms) ← blocking!
3. Callback (1ms)

실제 capacity 사용:
├─ 작업 1-10: 10/10 worker 사용
├─ I/O 대기: 10/10 worker 여전히 점유
│   (다른 작업이 준비돼도 실행 못함!)
└─ 결과: 병렬도 감소

시간대별:
0-30ms:   10개 모두 CPU work (효율 100%)
30-130ms: 10개 모두 I/O wait (효율 0%, but still occupied!)
130-160ms: 10개 모두 callback (효율 100%)

Oversubscription 안 되지만, capacity 낭비!
```

**Performance Impact:**
```
실행 시간: 2075ms (20개 task)

분석:
- 순차 시간 = 20 task × (30 + 100 + 1)ms = 2620ms
- 10개 worker이므로: 2620 / 10 = 262ms 예상
- 실제: 2075ms (동기화 오버헤드 포함)

문제: I/O 대기 중에도 thread가 scheduler overhead 야기
```

---

## 4️⃣ 좋은 코드 패턴 분석

### 패턴 1: Worker Pool (CPU-bound)

```cpp
✅ GoodApproach_WorkerPool::ProcessTasks_WorkerPool()
```

**핵심 아이디어:**

```cpp
TaskPool pool(8);  // Hardware thread와 맞춤

// ✅ thread 생성: 한 번만 (풀 초기화 시)
// ✅ thread 재사용: 같은 thread가 여러 작업 처리

for (int i = 0; i < 30; ++i) {
    pool.EnqueueTask([i]() { /* 작업 */ });
}
```

**개선점:**

| 항목 | Before | After |
|------|--------|-------|
| **Thread 생성** | 30회 | 1회 |
| **Runnable threads** | 30개 | 8개 |
| **Context switch** | 빈번 (3.75배) | 최소 (1배) |
| **캐시 효율** | 낮음 (자주 무효화) | 높음 (같은 thread 재사용) |

**Worker Loop 분석:**

```cpp
void WorkerLoop() {
    while (true) {
        function<void()> task;
        
        // ✅ 임계영역 최소화
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
        }  // ← lock 해제
        
        if (task) {
            task();  // ✅ 임계영역 외에서 작업 수행
            // → 다른 worker가 queue 접근 가능
            // → Contention 감소
        }
    }
}
```

**성능:**
```
실행 시간: 259ms (30개 task)
thread 생성 비용 제거 + context switch 최소화
```

---

### 패턴 2: Async Pattern (I/O-bound)

```cpp
✅ GoodApproach_AsyncPattern::ProcessWithAsync()
```

**핵심 아이디어:**

```cpp
// ❌ 이전: Blocking wait
lock_guard<mutex> lock(ioMutex);
sleep_for(100ms);  // 이 동안 thread 점유

// ✅ 개선: Callback
tasks[i].onComplete = [i]() {
    // I/O 완료 후 호출됨
};
// → thread는 즉시 다음 작업으로 이동
```

**메커니즘:**

```
Timeline:

Worker 1: [Task 0 CPU] → [CPU 끝] → [다음 task 즉시 처리]
          (I/O는 background에서 비동기)
                ↓
          Task 0 I/O 완료 → onComplete() 호출 → result update

Worker 2: [Task 1 CPU] → [CPU 끝] → [다음 task 즉시 처리]
...

✅ 결과: Worker는 항상 일함, I/O 대기 없음
```

**Capacity 활용:**

```
I/O 동안:
├─ Worker 1: Task 5 처리 중 ✓
├─ Worker 2: Task 6 처리 중 ✓
├─ Worker 3: Task 7 처리 중 ✓
├─ Worker 4: Task 8 처리 중 ✓
├─ Worker 5-8: 대기 또는 추가 I/O
└─ 결과: Thread capacity 100% 활용

❌ Blocking 방식:
├─ Worker 1: I/O 대기 중... (점유)
├─ Worker 2: I/O 대기 중... (점유)
├─ ...
└─ 결과: 실질 capacity 크게 감소
```

**Performance:**
```
실행 시간: 70ms (20개 task)
- I/O 대기 중 worker 유휴 없음
- Blocking으로 인한 lock contention 없음
```

---

## 5️⃣ 실무 적용 (Unreal Engine)

### CPU-bound 작업

```cpp
// ❌ 나쁜 예
for (int i = 0; i < 30; ++i) {
    FSimpleDelegate Task = [i]() { DoWork(i); };
    FThreadFactory::CreateThread(Task);
}

// ✅ 좋은 예
FGraphEventArray Tasks;
for (int i = 0; i < 30; ++i) {
    Tasks.Add(
        FFunctionGraphTask::CreateAndDispatchWhenReady(
            [i]() { DoWork(i); },
            TStatId(),
            nullptr,
            ENamedThreads::AnyThread
        )
    );
}
FTaskGraphInterface::Get().WaitUntilTasksComplete(Tasks);
```

**차이:**
- FFunctionGraphTask: 내부적으로 worker pool 사용
- ENamedThreads::AnyThread: Core count에 맞춘 thread 할당
- Thread 생성/소멸 오버헤드 없음

### I/O-bound 작업

```cpp
// ❌ 나쁜 예
void LoadAsset() {
    UPackage* Package = LoadPackage(...);  // blocking!
}

// ✅ 좋은 예
IAssetRegistry::Get().OnFilesLoaded().AddLambda([this]() {
    // 로딩 완료 후 콜백
    OnAssetLoaded();
});
```

**차이:**
- 콜백 기반: I/O 동안 thread free
- Worker는 다른 작업 처리
- Capacity 낭비 없음

### Oversubscription 메커니즘 (UE 5.5+)

```cpp
// wait 중에 standby worker 깨우기
class FOversubscribedWorkQueue {
    // task가 wait 상태 진입
    // → scheduler가 standby worker 깨움
    // → wait이 끝나면 다시 park
    
    // 결과: blocking 없이 capacity 보조
};
```

---

## 6️⃣ Context Switch 횟수 측정

### Linux (perf)
```bash
perf stat -e context-switches ./multithreading_refactor
```

**예상 결과:**
```
❌ Direct Creation:     ~1000-5000 context-switches
✅ Worker Pool:          ~100-500 context-switches
```

### Windows (VTune)
```
VTune → CPU Utilization → Thread Context Switches
```

### macOS
```bash
instruments -t "System Trace" ./multithreading_refactor
```

---

## 7️⃣ 성능 최적화 체크리스트

### 진단

- [ ] Runnable thread 수 > Hardware thread 수인가?
- [ ] Direct thread 생성을 반복하는가?
- [ ] Blocking wait이 있는가?
- [ ] Lock contention이 높은가? (mutex 대기 시간)

### 개선

#### CPU-bound
- [ ] Worker pool 사용 (size = core count)
- [ ] FFunctionGraphTask 또는 FAsyncTask 사용
- [ ] 작은 작업은 배치 처리

#### I/O-bound
- [ ] Async API 사용 (파일, 네트워크)
- [ ] 콜백 기반 구조
- [ ] Promise/Future 고려

#### 모니터링
- [ ] Unreal Insights에서 CPU 스케줄링 확인
- [ ] VTune으로 context switch 횟수 측정
- [ ] Frame time profiling (게임 루프)

---

## 8️⃣ 핵심 정리

| 개념 | 문제 | 해결 |
|------|------|------|
| **Context Switch** | 캐시 무효화, TLB 플러시 | Worker pool로 switch 최소화 |
| **Oversubscription** | Runnable > Hardware | Thread 수 = Core 수 제한 |
| **Blocking Wait** | Capacity 낭비 | Async/callback으로 전환 |
| **Thread 생성** | 반복 오버헤드 | Thread pool/재사용 |

**면접 답변:**
> Context switch는 OS가 CPU에서 실행할 thread를 바꿀 때 캐시와 TLB가 무효화되는 비용입니다. 
> Runnable thread가 hardware capacity를 초과하면 scheduler가 자주 switch하게 되어 성능이 악화됩니다.
> 이를 피하기 위해 CPU-bound는 worker pool (size = core count)을, 
> I/O-bound는 async 구조를 사용하며, Unreal Insights로 실제 scheduling을 확인합니다.

