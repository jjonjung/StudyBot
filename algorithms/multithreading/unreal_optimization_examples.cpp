/*
 * Unreal Engine: Context Switch & Oversubscription 최적화
 * 
 * 게임 개발 시나리오 기반 실제 코드
 * - Asset loading (I/O-bound)
 * - Physics simulation (CPU-bound)
 * - AI behavior tree (mixed)
 */

#pragma once

#include "CoreMinimal.h"
#include "Async/Async.h"
#include "Async/AsyncWork.h"
#include "Async/TaskGraphInterfaces.h"
#include "Containers/Queue.h"
#include "HAL/Event.h"
#include "HAL/RunnableThread.h"

// ============================================================================
// ❌ 안티패턴 1: 게임 루프에서 매 프레임 thread 생성
// ============================================================================
class AEnemySpawner_BadApproach : public AActor {
public:
    /*
     * 문제점:
     * - 매 프레임 UpdateSpawn()에서 새로운 thread 생성
     * - 60fps = 초당 60개 thread 생성/소멸
     * - Context switch로 frame time 튀어오름
     * - Cache thrashing으로 메인 게임 루프 성능 저하
     */
    
    virtual void Tick(float DeltaTime) override {
        Super::Tick(DeltaTime);
        UpdateSpawn(DeltaTime);
    }
    
    void UpdateSpawn(float DeltaTime) {
        EnemySpawnTimer += DeltaTime;
        
        if (EnemySpawnTimer >= SpawnInterval) {
            EnemySpawnTimer = 0.0f;
            
            // ❌ 매 스폰마다 새로운 thread!
            FRunnableThread* Thread = FRunnableThreadFactory::Create(
                new FSpawnWorker(this),
                TEXT("EnemySpawnThread")
            );
            // → 60 fps × 여러 스폰 = 심각한 overhead
            // → Context switch 증가 → frame drops
        }
    }
    
private:
    class FSpawnWorker : public FRunnable {
    private:
        AEnemySpawner_BadApproach* Owner;
        
    public:
        FSpawnWorker(AEnemySpawner_BadApproach* InOwner) : Owner(InOwner) {}
        
        virtual uint32 Run() override {
            // 적 생성 로직
            return 0;
        }
    };
    
    float EnemySpawnTimer = 0.0f;
    float SpawnInterval = 1.0f;
};


// ============================================================================
// ✅ 개선 1: Task 기반 접근 (CPU-bound)
// ============================================================================
class AEnemySpawner_TaskBased : public AActor {
public:
    /*
     * 개선점:
     * - FFunctionGraphTask 사용: 내부적으로 worker pool
     * - Thread 재사용, context switch 최소화
     * - Task scheduling으로 CPU 부하 분산
     */
    
    virtual void Tick(float DeltaTime) override {
        Super::Tick(DeltaTime);
        UpdateSpawn(DeltaTime);
    }
    
    void UpdateSpawn(float DeltaTime) {
        EnemySpawnTimer += DeltaTime;
        
        if (EnemySpawnTimer >= SpawnInterval) {
            EnemySpawnTimer = 0.0f;
            
            // ✅ FFunctionGraphTask: worker pool 사용
            FFunctionGraphTask::CreateAndDispatchWhenReady(
                [this]() {
                    SpawnEnemyOnWorker();
                },
                TStatId(),                      // 통계 ID (profiling)
                nullptr,                        // 선행 작업 없음
                ENamedThreads::AnyThread        // 아무 worker thread 사용
            );
            // → Thread 생성 오버헤드 없음
            // → Pool에서 자동 관리
            // → Context switch 최소 (pool size = core count)
        }
    }
    
private:
    void SpawnEnemyOnWorker() {
        // Worker thread에서 실행
        // 계산 집약적인 작업: AI 행동 트리, 물리 초기화 등
        
        FVector SpawnLocation = CalculateSpawnLocation();
        FEnemyData EnemyData = PrepareEnemyData();
        
        // ⚠️ 조건: 여기서 UObject 생성 불가!
        // → 나중에 game thread로 이동
    }
    
    float EnemySpawnTimer = 0.0f;
    float SpawnInterval = 1.0f;
};


// ============================================================================
// ❌ 안티패턴 2: I/O 작업에서 blocking wait
// ============================================================================
class UAssetLoader_BadApproach {
public:
    /*
     * 문제점:
     * - Asset 로드 동안 thread blocking
     * - 다른 작업이 대기 중인데도 capacity 점유
     * - Streaming 중 frame drops
     * - UE4.27 스타일 (구식)
     */
    
    void LoadAssetBlocking(const FString& AssetPath) {
        // ❌ 이 부분에서 thread가 완전히 block됨
        UPackage* Package = LoadPackage(nullptr, *AssetPath, LOAD_None);
        UObject* Asset = FindObject<UObject>(Package, TEXT("Asset"));
        
        if (Asset) {
            OnAssetLoaded(Asset);  // 동기식 콜백
        }
        // → I/O 완료까지 이 thread는 scheduler 관리 대상
        // → 다른 작업에 slot 양보 안 함
        // → Oversubscription 가능성 높음
    }
    
private:
    void OnAssetLoaded(UObject* Asset) {
        // Handle asset
    }
};


// ============================================================================
// ✅ 개선 2: Async/Callback 패턴 (I/O-bound)
// ============================================================================
class UAssetLoader_AsyncBased {
public:
    /*
     * 개선점:
     * - I/O 작업: 백그라운드에서 진행
     * - Main thread: I/O 대기 없이 계속 실행
     * - 콜백: I/O 완료 후 호출
     * - UE5.0+ 권장 방식
     */
    
    void LoadAssetAsync(const FString& AssetPath) {
        // ✅ Async action: I/O 동안 thread 해제
        Async(
            EAsyncExecution::ThreadPool,        // Worker pool 사용
            [this, AssetPath]() {
                // Worker thread에서 실행: I/O 작업
                LoadPackageAsync(AssetPath);
            },
            [this](const FString& LoadedPath) {
                // Main thread로 돌아와서 실행: 결과 처리
                OnAssetLoadedAsync(LoadedPath);
            }
        );
        // ✅ 즉시 반환! 이 함수는 blocking 아님
        // → Thread pool의 worker는 다음 작업으로 이동
        // → I/O 동안 다른 작업 처리 가능
    }
    
private:
    void LoadPackageAsync(const FString& AssetPath) {
        // Worker thread에서 실행
        UPackage* Package = LoadPackage(nullptr, *AssetPath, LOAD_None);
        // I/O 완료 대기 가능
    }
    
    void OnAssetLoadedAsync(const FString& LoadedPath) {
        // Game thread에서 실행
        UObject* Asset = FindObject<UObject>(nullptr, *LoadedPath);
        if (Asset) {
            // UObject 작업 안전함
        }
    }
};


// ============================================================================
// ❌ 안티패턴 3: Physics simulation에서 과도한 thread
// ============================================================================
class FPhysicsSimulator_BadApproach {
public:
    /*
     * 문제점:
     * - Physics substep마다 thread 생성
     * - 90fps × 4 substeps = 초당 360개 thread 생성
     * - Oversubscription 극심
     * - Physics frame time 폭증
     */
    
    void SimulatePhysics_BadPattern() {
        const int NumSubsteps = 4;
        
        for (int Step = 0; Step < NumSubsteps; ++Step) {
            // ❌ substep마다 새 thread
            FRunnableThread* Thread = FRunnableThreadFactory::Create(
                new FPhysicsWorker(Step),
                *FString::Printf(TEXT("PhysicsSubstep_%d"), Step)
            );
            // → 너무 많은 thread 생성
            // → Context switch 폭증
            // → Physics 성능 급락
        }
    }
    
private:
    class FPhysicsWorker : public FRunnable {
    private:
        int Step;
    public:
        FPhysicsWorker(int InStep) : Step(InStep) {}
        virtual uint32 Run() override {
            // Physics simulation
            return 0;
        }
    };
};


// ============================================================================
// ✅ 개선 3: Task-based Physics (CPU-bound 정석)
// ============================================================================
class FPhysicsSimulator_TaskBased {
public:
    /*
     * 개선점:
     * - FFunctionGraphTask 배치: worker pool 분배
     * - Dependency 관리: substep 간 순서 보장
     * - Context switch 최소화
     * - UE Physics 내부 방식
     */
    
    void SimulatePhysics_GoodPattern() {
        const int NumSubsteps = 4;
        FGraphEventArray Tasks;
        
        for (int Step = 0; Step < NumSubsteps; ++Step) {
            // ✅ Task 배치: worker pool 관리
            FGraphEventRef Task = FFunctionGraphTask::CreateAndDispatchWhenReady(
                [this, Step]() {
                    SimulateSubstep(Step);
                },
                TStatId(),
                nullptr,
                ENamedThreads::AnyThread  // Core count에 맞춘 분배
            );
            Tasks.Add(Task);
        }
        
        // ✅ 모든 task 완료 대기
        FTaskGraphInterface::Get().WaitUntilTasksComplete(Tasks);
        // → Worker 4개면 4개 task 동시 실행
        // → Thread 생성 없음
        // → Context switch 최소
    }
    
private:
    void SimulateSubstep(int Step) {
        // Physics simulation for substep
    }
};


// ============================================================================
// ✅ 고급: Custom Task System (Unreal 5.5+ oversubscription)
// ============================================================================
class FAdvancedTaskSystem {
public:
    /*
     * Unreal 5.5부터:
     * - wait() 중에 standby worker 깨우기
     * - blocking 없이 oversubscription 처리
     * - 더 효율적인 resource 사용
     */
    
    // 예: 10개 worker, 하나는 I/O wait 중
    // → scheduler가 자동으로 1개 standby worker 깨움
    // → 9개 worker로 계속 작업 처리
    
    void AdvancedTaskQueuing() {
        // UE 5.5+: 개념적 흐름
        FGraphEventRef MainTask = FFunctionGraphTask::CreateAndDispatchWhenReady(
            [this]() {
                // Main work
                DoMainWork();
                
                // I/O wait 필요
                FEvent* WaitEvent = FPlatformProcess::GetSynchEventFromPool();
                
                // ✅ wait 시작: scheduler가 standby worker 깨움
                WaitEvent->Wait();
                
                // I/O 완료: work 계속
                CompleteWork();
                WaitEvent->Trigger();
            },
            TStatId(),
            nullptr,
            ENamedThreads::AnyThread
        );
        // → Traditional blocking과 달리
        // → capacity 낭비 없음
    }
    
private:
    void DoMainWork() {}
    void CompleteWork() {}
};


// ============================================================================
// 📊 성능 모니터링: Unreal Insights
// ============================================================================
class FPerformanceMonitoring {
public:
    /*
     * 필수 확인 항목:
     * 1. CPU Scheduling 스크린
     *    - Context switch 빈도
     *    - Thread migration
     * 2. Task Graph
     *    - Task 분배
     *    - Worker 활용도
     * 3. Frame Time
     *    - Physics frame time
     *    - Game thread frame time
     */
    
    void ProfileExample() {
        // Code to measure
        {
            SCOPE_CYCLE_COUNTER(STAT_MyPhysicsTime);
            SimulatePhysics();
        }
        
        // Unreal Insights:
        // 1. Project Settings → Engine → CPU
        //    - "Enable CPU Profiling"
        // 2. Tools → Insights
        // 3. CPU Scheduling 탭에서 context switch 확인
        // 
        // 목표:
        // - Context switch < 1000/frame (60fps 기준)
        // - Worker thread utilization > 80%
        // - Context switches with > 1ms latency < 5%
    }
    
private:
    void SimulatePhysics() {}
};


// ============================================================================
// 📝 체크리스트: 코드 리뷰
// ============================================================================
/*
 * 멀티스레드 코드 리뷰 체크리스트
 * 
 * ❌ 위험 신호:
 * [ ] FRunnableThread::Create() 루프 내에서 호출?
 * [ ] Tick()에서 매 프레임 thread 생성?
 * [ ] I/O 작업을 blocking으로 처리?
 * [ ] for (int i = 0; i < 30; ++i) { thread.join(); }?
 * [ ] Async 콜백 없이 sleep_for()?
 * [ ] RunableThread를 임시로 사용 후 delete?
 * [ ] Mutex 잠금 중에 I/O 작업?
 * 
 * ✅ 안전 신호:
 * [ ] FFunctionGraphTask::CreateAndDispatchWhenReady() 사용?
 * [ ] ENamedThreads::AnyThread 지정?
 * [ ] FAsyncTask 템플릿 활용?
 * [ ] 콜백 기반 I/O (Async<T>)?
 * [ ] Unreal Insights에서 context switch 확인?
 * [ ] Thread 수 = Core 수 또는 그 이하?
 * [ ] Blocking 최소화 (wait 시간 < 1ms)?
 * [ ] Task graph에서 균형 잡힌 분배?
 * 
 * 게임루프별:
 * - Physics: Task 기반 (substep 병렬화)
 * - Rendering: GPU 작업 (CPU 대기 최소)
 * - AI: Task graph (tree evaluation 분산)
 * - Streaming: Async (I/O 동안 game thread 자유)
 */


// ============================================================================
// 면접 기술 면접 답변 템플릿
// ============================================================================
/*
 * Q: "Context switch의 비용이 뭔가요?"
 * 
 * A: "OS가 CPU에서 실행할 thread를 바꿀 때:
 *    1. 현재 thread의 CPU 상태(레지스터, PC) 저장
 *    2. 다음 thread의 상태 복원
 *    3. L1/L2 캐시와 TLB가 무효화되거나 재로드됨
 *    4. 결과적으로 메모리 액세스 성능 급락
 *    
 *    일반적으로 100-1000 CPU cycles 비용이 들고,
 *    캐시 miss로 인한 메모리 대기가 훨씬 더 큼"
 * 
 * Q: "Oversubscription이 뭐고 왜 문제인가요?"
 * 
 * A: "Runnable thread 수가 hardware thread 수를 초과하는 상황입니다.
 *    예: 8개 core에 30개 thread
 *    
 *    문제:
 *    - Scheduler가 자주 context switch 수행
 *    - 캐시 thrashing으로 성능 악화
 *    - 스레드 수 증가 ≠ 성능 증가
 *    
 *    해결:
 *    - CPU-bound: worker pool (size = core count)
 *    - I/O-bound: async/callback (blocking 제거)"
 * 
 * Q: "Unreal에서 어떻게 적용하나요?"
 * 
 * A: "FFunctionGraphTask를 사용합니다.
 *    - 내부적으로 worker pool 관리
 *    - Thread 생성 오버헤드 없음
 *    - I/O는 Async<T> 또는 FSimpleDelegate
 *    
 *    모니터링: Unreal Insights에서
 *    - CPU Scheduling 탭: context switch 확인
 *    - Task Graph: worker 활용도 분석
 *    
 *    목표: context switch < 1000/frame, worker utilization > 80%"
 */
