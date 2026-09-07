# 발표자료 — 배관 간섭 검사, thread는 많을수록 좋은가

오늘 학습한 "context switch와 oversubscription" 개념을, 반도체 팹
배관 CAD 도면의 간섭 검사(clash detection) 상황으로 재구성하고 실측
수치까지 뽑아 정리했다. 발표 흐름: **개념 → 실무 사용처 → 문제 상황
→ 실험 설계 → 실측 비교 → UE5 실전 적용 → 결론**.

---

## 0. 개념 — Context Switch와 Oversubscription이란

### 비유로 먼저

한 명의 작업자(CPU 코어)가 여러 개의 서류함(thread)을 번갈아 처리한다고
하자.

- 서류함이 코어 수만큼(예: 8개)이면, 작업자는 서류함 하나를 붙잡고
  어느 정도 몰입해서 처리한 뒤 다음 서류함으로 넘어간다.
- 서류함이 30개로 늘어나면, 작업자는 서류함 하나를 잠깐 들여다보다
  곧바로 다음 서류함으로 넘어가야 한다. 매번 "지금까지 어디까지
  했는지" 다시 파악하는 시간(context switch)이 늘어나고, 서류함을
  다시 집어들 때마다 필요한 자료(cache에 데워둔 데이터)를 다시
  찾아야 할 수도 있다.

**Context switch** — OS scheduler가 CPU에서 실행하는 thread를 바꾸는
과정.

```
현재 thread 상태 저장 -> 다음 실행 thread 선택 -> 다음 thread 상태 복원 -> 실행 재개
```

현재 thread가 시간 할당량을 다 쓰거나, 더 높은 우선순위 thread가
준비되거나, I/O·동기화를 기다리게 되면 발생한다. 비용은 레지스터
저장·복원만이 아니다 — 다른 thread가 실행되는 동안 기존 thread가
데워둔 cache/TLB가 밀려날 수 있고, 복귀했을 때 그 데이터가 없어 다시
메모리에서 채워야 하는 "cache 재가열 비용"이 따라온다.

**Oversubscription** — CPU가 실질적으로 동시에 처리할 수 있는 hardware
thread 수보다, 활발히 실행하려는 software thread 수가 훨씬 많은 상태.

```
8개 hardware thread
30개 CPU-bound worker thread
```

30개가 동시에 도는 게 아니라 일부만 돌고 나머지는 대기·교체된다.

### 그래서 언제 뭘 쓰나 — 한눈에 요약

| 상황 | 권장 |
|---|---|
| CPU-bound 작업을 병렬로 나누고 싶다 | task system + worker pool (hardware thread 수 근처) |
| I/O 대기·장기 blocking이 섞여 있다 | 비동기 API 또는 적절한 blocking 구조 검토 |
| 짧은 작업마다 매번 새 thread를 만들고 싶다 | 생성·스케줄링 비용 때문에 보통 피함 |

다만 이 표는 "이론적으로 기대할 수 있는 경향"이다. 4장의 실측 결과가
그 크기를 구체적으로 보여준다.

---

## 0-1. 실무에서 이 개념이 자주 등장하는 상황

| 상황 | 왜 문제가 되는가 |
|---|---|
| **게임 엔진의 대량 병렬 작업**(애니메이션, 물리, AI, 렌더링 준비) | 프레임마다 수백~수천 개의 작은 작업이 발생한다. 작업마다 thread를 새로 만들면 생성 비용과 oversubscription이 프레임 예산을 잡아먹는다. UE5 **Tasks System**이 worker pool을 미리 만들어두고 재사용하는 이유다. |
| **서버의 요청 처리(스레드 풀)** | 동시 접속자 수만큼 thread를 만들면 접속자가 늘어날수록 서버가 느려질 수 있다. 대부분의 서버 프레임워크가 고정 크기 thread pool + 작업 큐 구조를 쓰는 이유다. |
| **비동기 I/O 대기** | 네트워크·디스크 I/O를 기다리는 동안 thread를 그냥 blocking시키면 그 thread는 아무 일도 못 하면서 자리만 차지한다. 비동기 API(콜백, coroutine, UE Task의 협조적 대기)로 그 시간에 다른 작업을 처리하게 하는 것이 표준적인 해법이다. |
| **UE5 Tasks System의 oversubscription 메커니즘** | 기존에는 wait 구간에서 worker thread가 그냥 막혀 있었다(busy waiting). UE 5.5부터는 그 구간에 standby thread를 깨워 다른 task를 대신 처리하게 하고, wait이 끝나면 다시 park한다 — "일시적으로 줄어든 capacity를 보완"하는 것이지 thread를 무한정 늘리자는 뜻이 아니다. |

공통점: **작업 단위(task)를 쪼개는 것과 그것을 실행할 thread 수를
결정하는 것은 별개의 문제**라는 것. 작업은 잘게 쪼개도 되지만, 실행
주체는 hardware capacity에 맞춰 제한된 worker pool이어야 한다.

---

## 1. 문제 상황

반도체 팹 배관 CAD 도면 한 장에는 수만 개의 배관 세그먼트가 있고,
설계 변경 후에는 "새로 옮긴 배관이 기존 배관·구조물과 간섭(clash)하지
않는지" 검사해야 한다. 배관 하나당 주변 배관들과의 거리를 계산하는
clash detection은 전형적인 CPU-bound 작업이다.

도면을 여러 구역(zone)으로 나눠 구역별로 worker thread를 하나씩 만들어
병렬 처리한다고 할 때, **구역을 잘게 쪼갤수록(=thread를 많이 만들수록)
빠를 것이라는 직관이 맞는지**를 확인해본다.

---

## 2. 실험 설계

같은 총 작업량(배관 200,000개 전체에 대한 clash detection)을 thread
수만 바꿔가며 나눠 처리하고 총 소요 시간을 측정한다.

```cpp
// PipeClashOversubscription.cpp — RunClashDetectionParallel()
// ThreadCount개로 작업을 균등 분할해 병렬 실행하고 총 소요 시간을 잰다
for (unsigned t = 0; t < ThreadCount; ++t)
{
    const size_t Begin = (N * t) / ThreadCount;
    const size_t End = (N * (t + 1)) / ThreadCount;
    Workers.emplace_back([&Pipes, Begin, End, ThresholdSq, &PartialCounts, t]()
    {
        PartialCounts[t] = CountClashesInRange(Pipes, Begin, End, ThresholdSq);
    });
}
```

| 실험 | thread 수 | 의미 |
|---|---|---|
| A | `hardware_concurrency()` | 적정 구독 |
| B | `hardware_concurrency() x 8` | 심한 oversubscription |

두 실험 모두 **처리하는 총 연산량은 동일**하다 — thread 수가 늘어난
만큼 thread 하나가 맡는 몫이 줄어든다. 이상적인(context switch 비용이
0인) 세계라면 두 실험의 총 소요 시간은 비슷해야 한다. 실측 차이가
있다면 그것이 곧 "과도한 thread 생성·교체 비용"이다.

---

## 3. 실측 결과

MSVC `/O2`, 배관 200,000개, 10회 반복 평균으로 측정했다.

```
이 머신의 hardware_concurrency(): 16

[실험 A] thread 수 = 16   (hardware_concurrency 그대로)
  평균 소요 시간: 3.3715 ms

[실험 B] thread 수 = 128  (hardware_concurrency x 8, 심한 oversubscription)
  평균 소요 시간: 8.3208 ms

-> Oversubscribed/Normal 비율: 2.47x
```

> 재현: `PipeClashOversubscription.cpp`를 컴파일해 실행하면 동일한
> 방식으로 직접 확인할 수 있다.
> (`g++ -O2 -std=c++17 -pthread PipeClashOversubscription.cpp -o bench`)

### 왜 이렇게 나왔는가

- thread 수(128)가 이 머신의 hardware thread 수(16)를 8배 넘어서면서,
  OS scheduler가 훨씬 더 자주 thread를 교체(context switch)해야 했다.
- thread가 교체될 때마다 직전까지 CPU cache에 데워져 있던 데이터가
  다른 thread의 작업으로 밀려나고, 다시 그 thread 차례가 됐을 때
  데이터를 캐시에서 다시 채워야 하는 재가열 비용이 반복됐다.
- 결과적으로 "같은 총 연산량을 더 잘게 쪼갰을 뿐"인데 총 처리 시간은
  오히려 2.47배 늘어났다 — **thread 수 증가가 병렬 성능 증가로
  이어지지 않는다**는 것을 실측으로 보여준다.

**이 반전(혹은 확인)이 발표의 핵심 포인트다.** "작업을 잘게 쪼개면
빨라질 것"이라는 직관과 실측이 어긋나는 지점이며, 오늘 학습한 원칙 —
*CPU-bound 작업은 hardware capacity에 맞는 thread(또는 task worker)
수로 처리해야 하고, 그 이상은 context switch·cache 재가열 비용만
키운다* — 을 코드로 증명한 사례다.

---

## 4. C++ → UE5, 두 버전의 관계

| 파일 | 역할 |
|---|---|
| [PipeClashOversubscription.cpp](PipeClashOversubscription.cpp) | **벤치마크** — 표준 C++(`std::thread`)로 적정 구독/oversubscription을 각각 구현하고 실측 시간을 직접 측정 |
| [PipeClashOversubscription_UE5.h](PipeClashOversubscription_UE5.h) | **실전 적용** — 구역마다 직접 thread를 만드는 안티패턴(`FRunnableThread`)과 UE Tasks System(`UE::Tasks::Launch`)을 대비 |

```cpp
// PipeClashOversubscription_UE5.h — 안티패턴: 구역 수만큼 thread를 직접 생성
FRunnableThread* Thread = FRunnableThread::Create(Worker.Get(), TEXT("PipeClashWorker"));
// 구역이 128개면 thread도 128개 — hardware thread 수를 넘는 순간부터 oversubscription
```

```cpp
// PipeClashOversubscription_UE5.h — 권장: UE::Tasks::Launch로 task를 등록
UE::Tasks::TTask<int32> Task = UE::Tasks::Launch(TEXT("PipeClashZoneTask"),
    [&Pipes, Begin, End, ThresholdSq]() -> int32
    {
        return PipeClashDetail::CountClashesInRange(Pipes, Begin, End, ThresholdSq);
    });
```

**핵심 메시지**: task를 몇 개로 쪼개든(128개든 1000개든) 실제로 CPU에서
도는 thread 수는 UE Task 시스템의 worker pool 크기(대략 hardware
thread 수 근처, 엔진이 관리)로 제한된다. "작업을 잘게 쪼개는 것"과
"thread를 직접 늘리는 것"은 다른 문제이고, task로 등록하면 앞 장의
실험 A(적정 구독)에 해당하는 구조를 엔진이 대신 유지해준다.

### UE 5.5+ oversubscription 메커니즘과의 관계

Epic 공식 문서(UE 5.8 Tasks System)는 wait 구간에서 worker를 그냥
막아두는 구조(busy waiting)가 scalability를 제한한다고 설명하며, UE
5.5부터 이를 oversubscription 메커니즘으로 교체했다. task가 뭔가를
기다리며 blocking되는 동안 standby thread를 깨워 다른 task를 처리하게
하고, 대기가 끝나면 추가 thread를 다시 park한다.

중요한 점: 이것은 "CPU-bound task 수만큼 thread를 늘리자"는 뜻이
**아니다**. blocking 때문에 일시적으로 줄어든 worker capacity를
보완하는 장치이며, 이 발표 3장에서 실측한 것처럼 순수 CPU-bound
작업에서 thread(또는 worker)를 hardware capacity 이상으로 늘리는 것과는
반대 방향의 문제를 다룬다.

---

## 5. 발표 요약 한 줄

> **"작업을 잘게 쪼갤수록 빠르다"와 "thread를 많이 만들수록 빠르다"는
> 다른 문제다 — 이번 벤치마크에서는 같은 총 연산량을 hardware thread
> 수의 8배로 나눴더니 오히려 2.47배 느려졌고, 그 이유는 과도한 context
> switch와 그에 따른 cache 재가열 비용이었다. CPU-bound 작업은 직접
> thread를 늘리기보다 UE Tasks System 같은 worker pool 기반 구조에
> 맡기고, 목표 플랫폼에서 Unreal Insights로 직접 측정해서 결론을
> 내려야 한다.**

---

## 부록 — 참고 자료

- Epic UE 5.8 Tasks System: https://dev.epicgames.com/documentation/en-us/unreal-engine/tasks-systems-in-unreal-engine
- Microsoft Thread Scheduling: https://learn.microsoft.com/en-us/windows/win32/procthread/scheduling-priorities
- Intel VTune Thread Migration: https://www.intel.com/content/www/us/en/docs/vtune-profiler/cookbook/2024-0/os-thread-migration.html

## 부록 — 면접 30초 요약

> "Context switch는 OS가 CPU에서 실행하는 thread를 바꾸면서 현재 실행
> 상태를 저장하고 다음 상태를 복원하는 과정입니다. Runnable thread가
> hardware capacity보다 과도하게 많으면 scheduling·cache 재가열 비용이
> 늘 수 있습니다 — 실제로 배관 간섭 검사를 hardware_concurrency의 8배
> thread로 나눠 실측했더니 2.47배 느려지는 것을 확인했습니다. Unreal에서는
> 직접 thread를 남발하기보다 Tasks System과 비동기 구조를 우선 사용하고
> Insights로 실제 wait와 oversubscription을 확인하겠습니다."
