// PipeClashOversubscription_UE5.h
//
// PipeClashOversubscription.cpp(순수 C++, std::thread)와 동일한 "배관 간섭
// 검사(clash detection)" 작업을, UE5 실전 코드로 옮긴 버전. UE5 프로젝트
// (예: UnrealStudyBot)에서 배관 CAD 도면 검증 툴을 만든다고 가정한다.
//
// 핵심 메시지: 오늘 학습한 "직접 thread를 남발하지 말고 Tasks System과
// worker pool을 우선 사용하라"는 원칙을 그대로 코드로 보여준다.
//   - FWorkerBad:  구역마다 직접 FRunnableThread/std::thread를 생성 (안티패턴)
//   - FWorkerGood: UE::Tasks::Launch로 task를 등록해 UE의 task scheduler가
//                  worker pool(보통 hardware_concurrency 근처 개수) 위에서
//                  알아서 스케줄링하게 맡긴다 (권장 패턴)
//
// UE::Tasks 시스템은 CPU 코어 수에 맞춰 미리 만들어둔 worker thread pool
// 위에 task를 올려 실행한다. 즉 "이번 프레임에 구역이 128개다"라고 해서
// thread를 128개 만드는 게 아니라, 소수의 worker가 128개 task를 순서대로
// 처리한다 — PipeClashOversubscription.cpp의 실험 A(hardware_concurrency
// 개수만큼 thread)에 해당하는 구조를 프레임워크가 대신 관리해주는 것이다.

#pragma once

#include "CoreMinimal.h"
#include "Tasks/Task.h"
#include "HAL/RunnableThread.h"
#include "HAL/Runnable.h"
#include "HAL/PlatformProcess.h"
#include "Async/Async.h"

// ---------------------------------------------------------------------
// 배관 세그먼트 — PipeClashOversubscription.cpp의 FPipeSegment와 동일한
// 최소 구조. clash detection의 본질(좌표 간 거리 비교)만 남겼다.
// ---------------------------------------------------------------------
struct FPipeSegmentPoint
{
    float X = 0.0f;
    float Y = 0.0f;
    float Z = 0.0f;
};

namespace PipeClashDetail
{
    // std::thread 버전의 CountClashesInRange와 동일한 연산.
    inline int32 CountClashesInRange(const TArray<FPipeSegmentPoint>& Pipes, int32 Begin, int32 End, float ThresholdSq)
    {
        int32 ClashCount = 0;
        const int32 N = Pipes.Num();
        constexpr int32 Window = 64;
        for (int32 i = Begin; i < End; ++i)
        {
            const FPipeSegmentPoint& A = Pipes[i];
            const int32 Last = FMath::Min(i + Window, N);
            for (int32 j = i + 1; j < Last; ++j)
            {
                const FPipeSegmentPoint& B = Pipes[j];
                const float dx = A.X - B.X;
                const float dy = A.Y - B.Y;
                const float dz = A.Z - B.Z;
                const float DistSq = dx * dx + dy * dy + dz * dz;
                if (DistSq < ThresholdSq)
                {
                    ++ClashCount;
                }
            }
        }
        return ClashCount;
    }
}

// ---------------------------------------------------------------------
// 안티패턴 — 구역(zone) 수만큼 직접 FRunnableThread를 생성한다.
// ---------------------------------------------------------------------
//
// 도면을 세밀하게 쪼갤수록(예: 128구역) 128개의 OS thread가 그대로
// 생성된다. 프로젝트 세팅상 hardware thread가 8~16개 수준이라면 이는
// PipeClashOversubscription.cpp 실험 B와 같은 심한 oversubscription이고,
// 실측에서 확인했듯 총 처리 시간이 오히려 늘어날 수 있다. 게다가
// FRunnableThread는 생성/소멸 자체에도 비용이 있어 "짧은 작업마다 매번
// 새로 만드는" 이 패턴은 이중으로 불리하다.
class FPipeClashWorker_Bad : public FRunnable
{
public:
    FPipeClashWorker_Bad(const TArray<FPipeSegmentPoint>* InPipes, int32 InBegin, int32 InEnd, float InThresholdSq)
        : Pipes(InPipes), Begin(InBegin), End(InEnd), ThresholdSq(InThresholdSq)
    {
    }

    virtual uint32 Run() override
    {
        Result = PipeClashDetail::CountClashesInRange(*Pipes, Begin, End, ThresholdSq);
        return 0;
    }

    int32 GetResult() const { return Result; }

private:
    const TArray<FPipeSegmentPoint>* Pipes;
    int32 Begin;
    int32 End;
    float ThresholdSq;
    int32 Result = 0;
};

// 구역 수(ZoneCount)만큼 thread를 직접 만들어 처리하는 안티패턴 함수.
// ZoneCount를 hardware_concurrency보다 훨씬 크게 주면(예: 128) 실측
// oversubscription 상황을 그대로 재현한다.
inline int32 RunClashDetection_ManualThreads(const TArray<FPipeSegmentPoint>& Pipes, int32 ZoneCount, float ThresholdSq)
{
    TArray<TUniquePtr<FPipeClashWorker_Bad>> Workers;
    TArray<FRunnableThread*> Threads;
    Workers.Reserve(ZoneCount);
    Threads.Reserve(ZoneCount);

    const int32 N = Pipes.Num();
    for (int32 z = 0; z < ZoneCount; ++z)
    {
        const int32 Begin = (N * z) / ZoneCount;
        const int32 End = (N * (z + 1)) / ZoneCount;
        auto Worker = MakeUnique<FPipeClashWorker_Bad>(&Pipes, Begin, End, ThresholdSq);
        // 구역마다 매번 새 OS thread 생성 — 구역이 많을수록 생성 비용도,
        // context switch 빈도도 함께 늘어난다.
        FRunnableThread* Thread = FRunnableThread::Create(Worker.Get(), TEXT("PipeClashWorker"));
        Threads.Add(Thread);
        Workers.Add(MoveTemp(Worker));
    }

    int32 Total = 0;
    for (int32 z = 0; z < ZoneCount; ++z)
    {
        Threads[z]->WaitForCompletion();
        Total += Workers[z]->GetResult();
        delete Threads[z];
    }
    return Total;
}

// ---------------------------------------------------------------------
// 권장 패턴 — UE::Tasks::Launch로 task를 등록한다.
// ---------------------------------------------------------------------
//
// 구역을 몇 개로 쪼개든(128개든 1000개든) 실제로 CPU에서 도는 thread
// 수는 UE Task 시스템의 worker pool 크기(대략 hardware_concurrency 근처,
// 엔진이 관리)로 제한된다. 즉 "task를 잘게 쪼개는 것"과 "thread를 직접
// 늘리는 것"은 다른 이야기다 — 잘게 쪼갠 task는 소수의 worker가 순서대로
// 소화하므로 oversubscription 문제가 생기지 않는다.
//
// 또한 UE 5.5+의 oversubscription 메커니즘(문서 인용)은 "task가 뭔가를
// 기다리며 blocking되는 구간"에 한해 standby thread를 깨워 다른 task를
// 대신 처리하게 하는 보완책이지, 애초에 CPU-bound task 수만큼 thread를
// 늘리라는 뜻이 아니다. 이 함수처럼 순수 CPU-bound 연산이라면 worker
// pool 크기 그대로 처리하는 것이 기본 동작이다.
inline int32 RunClashDetection_Tasks(const TArray<FPipeSegmentPoint>& Pipes, int32 ZoneCount, float ThresholdSq)
{
    const int32 N = Pipes.Num();
    TArray<UE::Tasks::TTask<int32>> Tasks;
    Tasks.Reserve(ZoneCount);

    for (int32 z = 0; z < ZoneCount; ++z)
    {
        const int32 Begin = (N * z) / ZoneCount;
        const int32 End = (N * (z + 1)) / ZoneCount;

        // Task를 "생성"하는 것과 그 안의 코드가 실제로 새 OS thread에서
        // 도는 것은 별개다 - UE Task scheduler가 기존 worker pool 중
        // 놀고 있는 worker에게 이 task를 배정한다.
        UE::Tasks::TTask<int32> Task = UE::Tasks::Launch(TEXT("PipeClashZoneTask"),
            [&Pipes, Begin, End, ThresholdSq]() -> int32
            {
                return PipeClashDetail::CountClashesInRange(Pipes, Begin, End, ThresholdSq);
            });
        Tasks.Add(MoveTemp(Task));
    }

    int32 Total = 0;
    for (UE::Tasks::TTask<int32>& Task : Tasks)
    {
        // Wait()는 이 task가 끝날 때까지 "막연히 blocking"하는 게 아니라,
        // 대기하는 동안 호출 thread가 다른 task를 대신 처리할 수 있게
        // 하는 UE Task 시스템의 협조적 대기(cooperative wait) 경로를 탄다.
        Task.Wait();
        Total += Task.GetResult();
    }
    return Total;
}

// ---------------------------------------------------------------------
// 선택 가이드 (오늘 학습 결론을 이 도메인에 맞게 정리)
// ---------------------------------------------------------------------
//
// - 배관 CAD 도면의 clash detection처럼 "쪼개서 병렬로 돌리고 싶은
//   CPU-bound 작업"이 있다면, 구역을 몇 개로 나누든 FPipeClashWorker_Bad
//   방식(구역마다 FRunnableThread 직접 생성)은 피한다. 구역 수가
//   hardware thread 수를 넘는 순간부터 oversubscription이 시작되고,
//   PipeClashOversubscription.cpp의 실측처럼 총 처리 시간이 오히려
//   늘어날 수 있다.
//
// - 대신 RunClashDetection_Tasks처럼 UE::Tasks::Launch로 작업 단위를
//   등록한다. 작업을 잘게 쪼개는 것 자체는 자유롭게 해도 된다 — 실제
//   thread 수를 결정하는 것은 UE의 worker pool이지 task 개수가 아니기
//   때문이다.
//
// - I/O 대기나 다른 thread와의 동기화처럼 "당장 CPU를 안 쓰고 기다리는"
//   구간이 있다면, UE 5.5+의 oversubscription 메커니즘이 그 시간 동안
//   standby thread를 깨워 다른 task를 처리하게 해준다. 이건 "thread를
//   무한정 늘려도 된다"는 뜻이 아니라 "일시적으로 줄어든 worker capacity를
//   보완하는 장치"이므로, CPU-bound 작업 자체를 무작정 잘게 쪼개 thread를
//   늘리는 이유로 오해하면 안 된다.
//
// - 최종 판단은 항상 목표 플랫폼에서 Unreal Insights의 Timing Insights
//   (thread 별 타임라인, context switch 표시)로 실측한 뒤에 내린다.
