# 2026-09-14 DEV TECH 학습 노트 — release-acquire publication

출처: GAME & DEV DAILY (yeomin1220@gmail.com) 2026-09-13일자 [5] 📚 오늘의 개발 전문지식 DEV TECH
커리큘럼 위치: mutex → RW lock → atomic → **release/acquire** → relaxed/seq_cst (다음: relaxed·seq_cst와 비용/보장 범위 비교)

---

## 1. 개념 요약 (쉬운 설명)

`std::atomic<bool>` 같은 플래그 하나는 "원자적으로 읽고 쓴다"는 것만 보장합니다. 그런데 실무에서 우리가 진짜 원하는 건 대부분
"이 플래그가 true가 됐으면, 그 전에 준비해둔 **다른 일반 데이터**(구조체, 버퍼, 포인터)도 안전하게 다 보인다"는 것입니다.
이 두 가지는 별개의 문제입니다.

- **원자성(atomicity)**: 그 변수 자체를 읽고 쓸 때 값이 반쪽만 써지거나(torn write) 동시 접근으로 깨지지 않는다는 보장.
- **메모리 순서(memory ordering) / publication**: 그 변수를 기준으로 "이전에 쓴 다른 메모리"가 다른 스레드에도 순서대로
  보이게 만드는 보장.

`memory_order_relaxed`는 첫 번째만 보장합니다. 반면 **release-acquire**는 두 가지를 짝지어 성립합니다.

- 생산자(producer) 스레드가 일반 데이터를 다 쓴 뒤, 같은 원자 변수에 `store(value, memory_order_release)`.
- 소비자(consumer) 스레드가 그 원자 변수를 `load(memory_order_acquire)`로 읽어서 **같은 값**을 관찰하면,
  두 연산 사이에 *synchronizes-with* 관계가 생깁니다.
- 그 결과 producer가 release 이전에 수행한 모든 일반 메모리 write가 consumer의 acquire 이후 시점에 안전하게 보장되어 보입니다.

즉 release-acquire는 **뮤텍스보다 가벼운 "1회성 발행(publish) 신호"**입니다. 상호배제(mutual exclusion)가 필요 없고,
"준비 끝났다"는 신호와 함께 데이터 뭉치를 한 번 안전하게 넘기기만 하면 되는 producer-consumer 핸드오프에 적합합니다.

한 줄 요약: *relaxed는 그 변수 하나만 안전하고, release-acquire는 그 변수를 통해 딸려오는 다른 데이터까지 안전합니다.*

---

## 2. Bad Code — 문제가 있는 코드 (relaxed로 준비 신호를 잘못 구현)

백그라운드 스트리밍 워커가 메시 정점 데이터를 로드해 렌더 스레드에 넘겨주는 상황입니다.

```cpp
#include <atomic>
#include <cstdint>

struct FStreamedChunk
{
    int32_t VertexCount = 0;
    float*  VertexBuffer = nullptr;   // 워커 스레드가 힙에 할당해서 채움
};

FStreamedChunk        GChunk;
std::atomic<bool>     GChunkReady{false};

// [스트리밍 워커 스레드] 디스크에서 메시 데이터를 읽어 GChunk를 채운다
void StreamingWorker_Bad()
{
    GChunk.VertexBuffer = LoadVertexDataFromDisk();  // 힙 할당 + 데이터 채움
    GChunk.VertexCount  = 40000;

    // 문제: relaxed는 "GChunkReady 값 자체"의 원자성만 보장할 뿐,
    // 방금 전에 쓴 VertexBuffer/VertexCount를 다른 스레드에 "공개"하지 않는다.
    GChunkReady.store(true, std::memory_order_relaxed);
}

// [렌더 스레드] 준비되면 즉시 그린다
void RenderThread_Bad()
{
    if (GChunkReady.load(std::memory_order_relaxed))
    {
        // 컴파일러/CPU가 명령어 순서를 재배치할 수 있어서,
        // 이 시점에 VertexBuffer가 아직 nullptr이거나
        // VertexCount만 갱신되고 VertexBuffer는 옛 값일 수 있다.
        DrawMesh(GChunk.VertexBuffer, GChunk.VertexCount); // 널 포인터 크래시, 또는 깨진 지오메트리 렌더링
    }
}
```

**왜 문제인가:** `GChunkReady`는 원자적으로 true/false가 되지만, `relaxed`는 그 이전에 일어난 `VertexBuffer`,
`VertexCount` 쓰기를 다른 스레드에 순서대로 보이게 해주지 않습니다. x86처럼 메모리 모델이 상대적으로 강한 CPU에서는
우연히 재현되지 않다가, ARM 계열(모바일/콘솔)처럼 메모리 모델이 약한 CPU나 컴파일러 최적화 수준이 바뀌는 순간
간헐적으로만 터지는, 재현하기 매우 어려운 크래시나 그래픽 깨짐으로 나타납니다.

---

## 3. Refactored Code — release-acquire로 고친 코드

```cpp
#include <atomic>
#include <cstdint>

struct FStreamedChunk
{
    int32_t VertexCount = 0;
    float*  VertexBuffer = nullptr;
};

FStreamedChunk        GChunk;
std::atomic<bool>     GChunkReady{false};

// [스트리밍 워커 스레드]
void StreamingWorker_Fixed()
{
    GChunk.VertexBuffer = LoadVertexDataFromDisk();
    GChunk.VertexCount  = 40000;

    // release: 이 지점 "이전"에 일어난 모든 일반 메모리 write를 여기서 공개(publish)한다.
    GChunkReady.store(true, std::memory_order_release);
}

// [렌더 스레드]
void RenderThread_Fixed()
{
    // acquire가 release와 짝을 이뤄 같은 값을 읽으면 synchronizes-with 관계가 성립되고,
    // Worker가 release 이전에 쓴 VertexBuffer/VertexCount가 안전하게 보인다.
    if (GChunkReady.load(std::memory_order_acquire))
    {
        DrawMesh(GChunk.VertexBuffer, GChunk.VertexCount); // 안전: 항상 완전히 채워진 데이터만 보임
    }
}
```

바뀐 것은 딱 두 단어(`release`, `acquire`)뿐이지만, 컴파일러와 CPU에게 "이 원자 연산을 기준으로 앞뒤 메모리 접근을
재배치하지 말라"는 펜스를 세워주는 효과가 생깁니다. 뮤텍스로 감싸는 것보다 훨씬 가볍고, `seq_cst`보다는 제약이 적어 더
빠를 수 있습니다 — 단, 정확성을 먼저 확보한 뒤에만 `relaxed`/`release-acquire`로 좁혀야 안전합니다.

---

## 4. 적용 시나리오

### 게임 엔진 (핵심 시나리오)

위 예제 자체가 실제 게임 엔진에서 매일 벌어지는 패턴입니다. Unreal의 스트리밍 매니저나 Unity의 어드레서블 로딩 시스템은
백그라운드 워커 스레드에서 메시·텍스처·오디오 청크를 비동기로 로드한 뒤, "이제 렌더 스레드가 써도 된다"는 신호를
플래그 하나로 넘깁니다. 이때 신호를 `relaxed`로 잘못 구현하면, 특히 콘솔/모바일(ARM, 약한 메모리 모델)에서만
간헐적으로 재현되는 크래시나 "몇 프레임 동안 텍스처가 깨져 보이는" 버그가 생기고, PC(x86)에서는 QA를 통과해버려
발견이 더 늦어집니다. 같은 패턴은 물리 스레드가 계산한 최신 트랜스폼 스냅샷을 렌더 스레드에 넘기거나, 게임 스레드의
입력 스냅샷을 네트워크 송신 스레드에 넘길 때도 그대로 적용됩니다.

### 반도체 팹 시뮬레이션 (같은 패턴의 다른 적용)

이산 이벤트 기반 팹 시뮬레이터에서 설비(equipment) 컨트롤러 스레드가 한 웨이퍼 랏(lot)의 공정 스텝을 끝내고
수율·센서 판독값이 담긴 "결과 스냅샷"을 스케줄러/디스패처 스레드에 넘겨 다음 공정을 결정하게 하는 경우가 이와
동일한 구조입니다. 시뮬레이션 클럭이 초당 수천 건의 랏 이벤트를 처리해야 할 때, 매 이벤트마다 뮤텍스를 잡는 대신
release-acquire로 결과 스냅샷을 발행하면 락 경합 없이 안전하게 핸드오프할 수 있습니다.

---

*이 노트는 매주 월/목요일 아침 GAME & DEV DAILY의 [5]번 섹션을 읽어 자동으로 생성됩니다.*
