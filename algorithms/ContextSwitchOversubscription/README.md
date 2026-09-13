# 반도체 배관 CAD - 배관 간섭 검사와 Context Switch / Oversubscription

오늘 학습한 "context switch와 oversubscription" 개념을 반도체 팹(Fab)
배관 CAD 도면의 간섭 검사(clash detection) 상황에 적용해본 예제입니다.
이전 학습인 "NUMA와 thread affinity"의 연장선이며, 다음 학습
("synchronization wait와 blocking")으로 넘어가기 전에 "thread를 많이
만든다고 병렬 성능이 늘어나는 게 아니다"라는 감각을 코드로 확인하는 것이
목적입니다.

## 상황

반도체 팹 배관 CAD 도면 한 장에는 수만 개의 배관 세그먼트가 있고, 설계
변경 후에는 "새로 옮긴 배관이 기존 배관·구조물과 간섭(clash)하지 않는지"
검사해야 합니다. 배관 하나당 주변 배관들과의 거리를 계산하는 clash
detection은 전형적인 CPU-bound 작업입니다.

도면을 여러 구역(zone)으로 나눠 구역별로 worker thread를 하나씩 만들어
병렬 처리한다고 할 때, 구역 수(=thread 수)를 어떻게 정해야 할까요?
"구역을 잘게 쪼갤수록 빠르다"고 생각하기 쉽지만, 실제로는 hardware
thread 수를 한참 넘어서면 오히려 느려질 수 있습니다.

## Context switch란

OS scheduler는 실행 가능한 여러 thread 중 어떤 thread를 CPU에서 실행할지
결정합니다. 현재 thread가 시간 할당량을 다 쓰거나, 더 높은 우선순위
thread가 준비되거나, I/O·동기화를 기다리게 되면 다른 thread로 실행
주체가 바뀔 수 있습니다.

```
현재 thread 상태 저장 -> 다음 실행 thread 선택 -> 다음 thread 상태 복원 -> 실행 재개
```

비용은 레지스터 저장·복원만이 아닙니다. 다른 thread가 실행되는 동안
기존 thread가 데워둔 cache/TLB가 밀려날 수 있고, 다시 돌아왔을 때 그
데이터가 cache에 없어 메모리에서 다시 채워야 할 수 있습니다.

## 용어 미니 사전

처음 보면 헷갈리는 용어만 한 줄씩 짚고 넘어갑니다.

| 용어 | 한 줄 정의 |
|---|---|
| **Hardware thread** | CPU가 실제로 "동시에" 명령을 처리할 수 있는 물리적 실행 단위 수. 예: 8코어 16스레드 CPU라면 hardware thread는 16. `std::thread::hardware_concurrency()`로 알아낼 수 있습니다. |
| **Software thread** | 프로그램이 `std::thread`나 `FRunnableThread`로 만든 실행 단위. 개수 제한 없이 원하는 만큼 만들 수 있지만, 동시에 "실제로 도는" 것은 hardware thread 수만큼입니다. |
| **Cache / TLB** | CPU 옆에 붙은 초고속 임시 저장 공간. 방금 쓴 데이터를 여기 담아두면 다음에 메모리까지 안 가고 바로 재사용할 수 있어 빠릅니다. TLB는 메모리 주소 변환 정보를 캐싱하는 특수한 캐시입니다. |
| **Cache 재가열 비용** | 다른 thread가 실행되는 동안 내 데이터가 cache에서 밀려나, 내 차례가 다시 왔을 때 그 데이터를 메모리에서 다시 채워야 하는 비용. Context switch가 "공짜가 아닌" 진짜 이유입니다. |
| **Busy waiting** | 결과가 준비될 때까지 thread가 아무것도 안 하면서 계속 확인만 반복하는 것 (예: `while(!ready) {}`). CPU를 점유한 채로 낭비하므로 대개 피해야 할 패턴입니다. |
| **Blocking** | thread가 특정 조건(I/O 완료, lock 획득 등)이 될 때까지 실행을 멈추고 대기하는 것. Busy waiting과 달리 OS가 그 동안 CPU를 다른 thread에 넘겨줄 수 있습니다. |
| **Worker pool** | 작업(task)을 처리하기 위해 미리 만들어 재사용하는 고정 개수의 thread 묶음. 작업마다 매번 새 thread를 만들지 않고, 이미 있는 pool에 작업만 던져 넣는 방식입니다. |

## Oversubscription — [PipeClashOversubscription.cpp](PipeClashOversubscription.cpp)

CPU가 동시에 처리할 수 있는 hardware thread 수보다 활발히 실행하려는
software thread 수가 훨씬 많은 상태입니다. 예를 들어

```
8개 hardware thread
30개 CPU-bound worker thread
```

라면 30개가 동시에 도는 게 아니라 일부만 돌고 나머지는 대기·교체됩니다.
그래서 **thread 수 증가 ≠ 병렬 성능 증가**입니다.

## 나쁜 코드 vs 좋은 코드

"구역을 잘게 쪼갤수록 빠르다"는 직관을 그대로 코드로 옮기면 이렇게 됩니다
([PipeClashOversubscription_UE5.h](PipeClashOversubscription_UE5.h) 발췌).

```cpp
// 나쁜 예 — 구역(zone) 수만큼 매번 새 OS thread를 직접 생성
// 구역이 128개면 thread도 그대로 128개. hardware thread(예: 16개)를
// 훌쩍 넘는 순간부터 oversubscription이 시작된다.
for (int32 z = 0; z < ZoneCount; ++z)
{
    auto Worker = MakeUnique<FPipeClashWorker_Bad>(&Pipes, Begin, End, ThresholdSq);
    FRunnableThread* Thread = FRunnableThread::Create(Worker.Get(), TEXT("PipeClashWorker"));
    // ...
}
```

무엇이 문제인가:

1. **구역 수 = thread 수**가 그대로 고정돼버려서, 도면을 잘게 쪼갤수록
   (병렬성을 높이려는 의도였는데) 오히려 OS가 감당할 수 없는 수의 thread를
   만들게 됩니다.
2. `FRunnableThread::Create`처럼 thread를 새로 만드는 동작 자체에도
   비용이 있습니다. 구역마다 매번 새로 만들면 이 생성 비용이 반복해서
   쌓입니다.
3. hardware thread 수(예: 16개)를 넘는 thread들은 동시에 돌 수 없으므로
   OS가 번갈아 실행시켜야 하고, 그때마다 context switch와 cache 재가열
   비용이 발생합니다 — 아래 실측에서 이 비용이 **2.47배** 느려지는
   결과로 나타납니다.

```cpp
// 좋은 예 — 작업 단위(task)만 등록하고, 실행 주체(thread)는 엔진에 맡긴다
// 구역이 128개든 1000개든 task 개수만 늘어날 뿐, 실제로 도는 thread 수는
// UE Task 시스템의 worker pool 크기(대략 hardware thread 수 근처)로 고정된다.
UE::Tasks::TTask<int32> Task = UE::Tasks::Launch(TEXT("PipeClashZoneTask"),
    [&Pipes, Begin, End, ThresholdSq]() -> int32
    {
        return PipeClashDetail::CountClashesInRange(Pipes, Begin, End, ThresholdSq);
    });
```

핵심 차이를 한 줄로 요약하면: **"작업을 몇 개로 쪼갤지"와 "그 작업을
실행할 thread를 몇 개 만들지"는 서로 다른 결정**입니다. 나쁜 예는 이
둘을 하나로 묶어버려서(구역 수 = thread 수) 구역을 늘릴수록 thread도
같이 늘어나지만, 좋은 예는 작업(task) 개수와 thread(worker) 개수를
분리해, task는 자유롭게 쪼개면서도 실행 thread 수는 hardware capacity에
맞게 고정합니다.

| | 나쁜 예 (`FRunnableThread` 직접 생성) | 좋은 예 (`UE::Tasks::Launch`) |
|---|---|---|
| thread 수 결정 | 구역 수에 정비례 (구역 128개 → thread 128개) | 항상 worker pool 크기로 고정 (엔진이 관리) |
| 구역을 잘게 쪼갤 때 | thread도 같이 늘어나 oversubscription 위험 | task만 늘어날 뿐, thread 수는 그대로 |
| thread 생성 비용 | 구역마다 반복 발생 | worker pool은 한 번만 만들어 재사용 |
| 실측 결과(본 저장소 기준) | 8배 oversubscription 시 2.47배 느려짐 | 적정 구독 수준 유지 |

## 실험 설계

같은 총 작업량(배관 세그먼트 전체에 대한 clash detection)을 thread
수만 바꿔가며 나눠 처리하고 총 소요 시간을 측정합니다.

| 실험 | thread 수 | 의미 |
|---|---|---|
| A | `hardware_concurrency()` | 적정 구독 |
| B | `hardware_concurrency() x 8` | 심한 oversubscription |

두 실험 모두 "처리하는 총 연산량"은 동일합니다 — thread 수가 늘어난
만큼 thread 하나가 처리하는 몫이 줄어듭니다. 이상적인(context switch
비용이 0인) 세계라면 두 실험의 총 소요 시간은 비슷해야 합니다. 실측
차이가 있다면 그것이 곧 "과도한 thread 생성·교체 비용"입니다.

### 실측 결과

이 저장소에서 MSVC `/O2`, 배관 200,000개, 10회 반복 평균으로 측정한
결과는 다음과 같았습니다 (환경에 따라 달라질 수 있는 참고용 수치입니다).

```
이 머신의 hardware_concurrency(): 16

[실험 A] thread 수 = 16   (hardware_concurrency 그대로)
  평균 소요 시간: 3.3715 ms

[실험 B] thread 수 = 128  (hardware_concurrency x 8, 심한 oversubscription)
  평균 소요 시간: 8.3208 ms

-> Oversubscribed/Normal 비율: 2.47x
```

thread를 8배 늘렸더니 총 소요 시간이 **2.47배 느려졌습니다.** 같은 총
연산량을 더 잘게 쪼갰을 뿐인데 오히려 느려진 것은, thread 수가 hardware
thread 수(16)를 훨씬 넘어서면서 OS가 더 자주 context switch를 해야
했고, 그때마다 다른 thread가 데워둔 cache/TLB 때문에 재가열 비용이
쌓였기 때문으로 보입니다. 정확한 배율은 CPU 코어 수·OS 스케줄러·부하
상황에 따라 달라지므로, 이 수치 자체보다 "thread를 늘릴수록 항상
빨라지는 건 아니다"라는 방향성을 확인하는 용도로 삼아야 합니다.

## 실행 방법

```
g++ -O2 -std=c++17 -pthread PipeClashOversubscription.cpp -o bench
./bench
```

Windows에서 MSVC로 빌드할 때는 `/utf-8` 옵션 없이도 됩니다.

```
cl /O2 /std:c++17 /EHsc PipeClashOversubscription.cpp
```

배관 200,000개를 생성하고, 실험 A(적정 thread 수)와 실험 B(8배
oversubscription)를 각각 10회 반복해 평균 시간(ms)과 비율을 출력합니다.

### Windows에서 한글이 깨져 보인다면

이 파일은 UTF-8(BOM 포함)로 저장돼 있습니다. AosVsSoa/PipeThermalScan.cpp와
동일한 이유로, 화면 출력은 `printf` 대신 `PrintUtf8()`/`PrintfUtf8()`
헬퍼를 통해 나갑니다 — `u8"..."` 리터럴로 UTF-8 바이트를 표준 보장대로
확보한 뒤 `WriteConsoleW`로 직접 콘솔에 쓰고, 출력이 파일/파이프로
리다이렉트된 경우에는 UTF-8 바이트를 그대로 내보내는 방식입니다. 자세한
원리는 [PipeClashOversubscription.cpp](PipeClashOversubscription.cpp)
상단 `PrintUtf8()` 주석 및 `AosVsSoa/PipeThermalScan.cpp` 참고.

## UE5 실전 버전 — [PipeClashOversubscription_UE5.h](PipeClashOversubscription_UE5.h)

`std::thread`로 직접 thread를 만드는 대신 UE5 `UE::Tasks::Launch`로
옮긴 버전입니다.

- `FPipeClashWorker_Bad` / `RunClashDetection_ManualThreads` — 구역
  수만큼 `FRunnableThread`를 직접 생성하는 안티패턴. 구역을 잘게 쪼갤수록
  그대로 thread가 늘어나 oversubscription으로 이어집니다.
- `RunClashDetection_Tasks` — `UE::Tasks::Launch`로 task를 등록하는 권장
  패턴. task를 몇 개로 쪼개든 실제 실행은 UE의 worker pool(대략
  hardware thread 수 근처, 엔진이 관리)이 순서대로 처리하므로
  oversubscription이 생기지 않습니다.

Unreal Engine 5.8 Tasks 문서는 기다리는 동안 worker를 막는 구조가
scalability를 제한한다고 설명하며, UE 5.5부터 기존 busy waiting을
oversubscription 메커니즘으로 교체했습니다. wait 구간에서는 standby
thread를 깨워 다른 task를 처리하게 하고, 기간이 끝나면 추가 thread를
다시 park합니다. 중요한 점은 이 메커니즘이 "thread를 무한히 많이
만들자"는 뜻이 아니라, blocking 때문에 worker capacity가 일시적으로
줄었을 때 보완하는 장치라는 것입니다.

파일 하단의 "선택 가이드" 주석에 실무 판단 기준을 정리해두었습니다.

## 파생 개념 — CPU-bound와 I/O-bound는 "적정 thread 수"가 다르다

이 문서의 실측은 전부 **CPU-bound** 작업(clash detection 연산) 기준이라
"적정 thread 수 ≈ hardware thread 수"였습니다. 하지만 thread가 CPU
연산이 아니라 **I/O 대기**(디스크 읽기, 네트워크 응답 대기 등)로 시간을
보낸다면 이 공식이 달라집니다.

- CPU-bound thread는 대기 없이 CPU를 계속 쓰기 때문에, hardware thread
  수보다 많이 만들면 서로 CPU를 뺏는 순수 손해만 발생합니다.
- I/O-bound thread는 대부분의 시간을 "기다리며 blocking"된 상태로
  보냅니다. 이때는 CPU가 놀고 있으므로, 그 시간에 다른 thread를 실행시켜
  CPU를 계속 바쁘게 만드는 편이 유리합니다. 즉 hardware thread 수보다
  **더 많은** thread를 만드는 것이 오히려 정당화됩니다.

실무에서 자주 쓰는 대략적인 감(정확한 공식이라기보다 방향성):

```
필요한 thread 수 ≈ hardware thread 수 x (1 / (1 - 대기 비율))

예: hardware thread 8개, 작업 시간의 80%를 I/O 대기로 보낸다면
    8 x (1 / (1 - 0.8)) = 8 x 5 = 약 40개
```

이 값 자체보다 "대기 비율이 높을수록 hardware thread 수를 넘는 thread를
만들어도 손해가 아니라 오히려 이득일 수 있다"는 방향을 이해하는 것이
중요합니다. 정확한 최적치는 항상 실측(벤치마크·프로파일링)으로
확인해야 합니다.

이것이 바로 UE 5.5+ **oversubscription 메커니즘**이 하는 일과
연결됩니다 — task가 wait(blocking)에 들어가는 그 순간에만 standby
thread를 깨워 "지금 비어 있는 CPU 자원"을 메꾸고, wait이 끝나면 다시
줄입니다. CPU-bound 작업처럼 "항상 바쁜" thread를 무작정 늘리는 것과는
정반대 상황(= 일시적으로 노는 CPU를 채우는 것)에 대한 해법입니다.

### 다음 학습으로 이어지는 지점 — synchronization wait와 blocking

오늘 다룬 것은 "thread가 CPU를 얼마나 나눠 쓰는가"였다면, 다음 학습
주제인 **synchronization wait와 blocking**은 "thread가 서로를 기다리게
만드는 지점(mutex, lock, condition variable 등)에서 무슨 일이 벌어지는가"를
다룹니다. 두 주제는 같은 실로 연결되어 있습니다.

- 어떤 thread가 lock을 기다리며 blocking되면, 그 thread는 CPU를 쓰지
  않는 상태가 됩니다 — 이 "쓰지 않는 시간"을 다른 thread가 채워 쓸 수
  있는가가 바로 oversubscription 메커니즘의 존재 이유입니다.
- 반대로 lock을 너무 오래 붙잡고 있거나, 여러 thread가 같은 lock을
  두고 경쟁(contention)하면, thread 수를 아무리 정확히 맞춰도 그
  lock 대기 시간 자체가 병목이 됩니다. 즉 oversubscription 문제를
  해결해도 synchronization 설계가 나쁘면 여전히 느릴 수 있습니다.
- 다음 학습에서는 "thread 개수를 얼마로 할까"에서 한 단계 나아가
  "thread들이 서로를 얼마나, 어떻게 기다리게 만들까"를 다루게 됩니다.

## 결론 요약

- `CPU-bound 작업` → task system과 worker pool 우선
- `I/O 대기·장기 blocking` → 비동기 API 또는 적절한 blocking 구조 검토
- `짧은 작업마다 직접 thread 생성` → 생성·스케줄링 비용 때문에 보통 피함
- 목표 플랫폼에서 Unreal Insights와 벤치마크를 통해 항상 실측 확인

## 참고 자료

- Epic UE 5.8 Tasks System: https://dev.epicgames.com/documentation/en-us/unreal-engine/tasks-systems-in-unreal-engine
- Microsoft Thread Scheduling: https://learn.microsoft.com/en-us/windows/win32/procthread/scheduling-priorities
- Intel VTune Thread Migration: https://www.intel.com/content/www/us/en/docs/vtune-profiler/cookbook/2024-0/os-thread-migration.html

## 발표자료

- [PRESENTATION.md](PRESENTATION.md) — 문제 상황부터 실측 비교, UE5
  적용까지 발표용으로 정리한 문서.
