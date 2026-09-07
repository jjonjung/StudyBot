# 코드 설명 — 개념이 실제 구현에서 어떻게 나타나는가

[README.md](README.md)에서 정리한 "4가지 질문 → 자료구조 선택" 기준이
[FabEquipmentRegistry](FabEquipmentRegistry.h) / [DxfEntityIndex](DxfEntityIndex.h)
두 예제에서 각각 어느 코드 줄로 구현됐는지 짝지어 설명한다.

---

## 공통 설계 패턴: "정답 데이터 1개 + 색인 여러 개"

두 예제 모두 같은 뼈대를 쓴다.

```
TArray  = 정답 데이터 (Source of Truth) — 모든 필드를 가진 완전한 원소
TMap    = "ID로 그 원소를 빨리 찾기 위한" 색인 (원소 자체가 아니라 배열 인덱스를 저장)
TSet    = "그 ID가 어떤 집합에 속하는가"만 필요할 때 쓰는 색인 (값 없이 존재 여부만)
```

색인은 정답 데이터가 아니므로, **정답 데이터가 바뀌는 지점마다 색인도 같이 갱신**해야
한다. 이 동기화를 놓치면 버그가 나므로, 두 예제 모두 추가/삭제 메서드 안에서
색인 갱신을 같은 함수 내에 묶어뒀다 — 호출자가 따로 신경 쓸 필요가 없게.

---

## 예제 1 — [FabEquipmentRegistry](FabEquipmentRegistry.h) (반도체 Fab 3D 시뮬레이션)

### 이 시스템의 4가지 질문에 대한 답 → 자료구조 매핑

| 질문 | 답 | 코드 위치 |
|---|---|---|
| 매 tick 전체 순회하는가? | Yes — 전 설비 온도 갱신 | [`TickAll()`](FabEquipmentRegistry.cpp:44) |
| ID를 자주 찾는가? | Yes — UI 클릭/알람 조회 | [`FindEquipment()`](FabEquipmentRegistry.cpp:57) |
| 추가·삭제가 잦은가? | No — 런타임 중 설비 수 거의 고정 | [`AddEquipment()`](FabEquipmentRegistry.cpp:5) / [`RemoveEquipment()`](FabEquipmentRegistry.cpp:17) |
| 순서가 의미 있는가? | No — 배열 인덱스만 안정적이면 됨 | swap-remove 사용 근거 |

### `TArray<FEquipmentState> Equipment` — 정답 데이터

[FabEquipmentRegistry.h:88](FabEquipmentRegistry.h)에 선언된 이 배열이 "진짜 데이터"다.
`TickAll()`이 매 프레임 이 배열만 range-for로 순회한다([FabEquipmentRegistry.cpp:48](FabEquipmentRegistry.cpp)).
`TMap`/`TSet` lookup이 이 루프 안에 전혀 없다는 점이 핵심 — hot path에서는
색인 조회 비용조차 아낀다. 이것이 README의 "1. 매 tick 전체 순회 → TArray"가
그대로 코드로 옮겨진 부분이다.

### `TMap<FName, int32> IndexMap` — ID → 배열 인덱스 색인

`FindEquipment()`가 실제로 하는 일:

```cpp
if (const int32* FoundIndex = IndexMap.Find(EquipmentId))
{
    return &Equipment[*FoundIndex];
}
```

이게 README의 "`Contains` 후 `operator[]`로 두 번 찾지 말고 `Find` 한 번" 규칙을
그대로 구현한 부분이다. `IndexMap`은 `FEquipmentState` 전체를 복사해 들고 있지
않고 `int32` 인덱스만 들고 있다 — Key(ID)만 필요한 게 아니라 Key→Value(인덱스)
매핑이 필요하므로 `TSet`이 아니라 `TMap`을 쓴 이유다.

### swap-remove — "순서 무의미 + 삭제 잦지 않음"의 실제 효과

`RemoveEquipment()`([FabEquipmentRegistry.cpp:17](FabEquipmentRegistry.cpp))가
`TArray::RemoveAt`(뒤 원소를 전부 당기는 O(N))을 쓰지 않고, 마지막 원소를
삭제 위치로 복사한 뒤 배열을 한 칸 줄이는 방식을 쓴다:

```cpp
Equipment[RemoveIndex] = Equipment[LastIndex];
IndexMap[Equipment[RemoveIndex].EquipmentId] = RemoveIndex;  // 옮겨진 원소의 색인 갱신
Equipment.RemoveAt(LastIndex, 1, false);
```

이게 가능한 이유가 정확히 README 질문 4번("순서가 의미 있는가? → No")이다.
순서가 중요했다면 이 최적화를 쓸 수 없었을 것이다. 동시에, 인덱스를 옮겼으니
`IndexMap`도 같이 고쳐야 한다는 게 "색인은 정답 데이터가 바뀔 때 같이
갱신해야 한다"는 공통 패턴의 구체적 사례다.

### `TSet<FName> AlarmSet` — 존재 여부만 필요한 경우

`RaiseAlarm` / `IsInAlarm`은 알람 설비의 *상태 값*이 필요한 게 아니라
"지금 알람인가 아닌가"만 필요하다. 그래서 `TMap<FName, bool>`이 아니라
`TSet<FName>`을 썼다 — Key만 있고 Value가 없는 구조가 정확히 이 요구에 맞는다.
`RaiseAlarm`의 주석대로 `TSet::Add`는 이미 있는 원소를 다시 넣어도 안전하므로
"중복 알람 방지" 로직을 따로 안 짜도 된다.

---

## 예제 2 — [DxfEntityIndex](DxfEntityIndex.h) (2D DXF CAD 설계)

Fab 예제와 같은 패턴이지만, README의 "정렬 배열 + Binary Search" 항목을
실제로 보여주기 위해 색인을 하나 더 추가한 버전이다.

| 질문 | 답 | 코드 위치 |
|---|---|---|
| 전체 순회하는가? | Yes — 레이어 export/렌더링 | [`GetAllEntities()`](DxfEntityIndex.h:45) |
| ID(태그) lookup이 잦은가? | Yes — 클릭/스크립트 조회 | [`FindByTag()`](DxfEntityIndex.cpp:20) |
| 정렬 상태를 반복 조회하는가? | Yes, 단 "완성된 도면"에서만 | [`BuildSortedIndex()`](DxfEntityIndex.cpp:29) / [`FindByXRange()`](DxfEntityIndex.cpp:43) |
| 존재 여부만 필요한가? | Yes — BOM 집계 시 중복 태그 스킵 | [`CollectUniqueTagsInLayer()`](DxfEntityIndex.cpp:72) |

### `TMap<FString, int32> TagToIndex` — Fab의 IndexMap과 동일한 역할

`FindByTag()`도 `Find()` 한 번으로 존재 확인과 인덱스 획득을 같이 처리한다.
Fab 예제의 `FindEquipment()`와 완전히 같은 패턴 — 도메인만 설비 ID에서
도면 태그로 바뀌었을 뿐이다. 기존 [PipeTagLookup](../PipeTagLookup/README.md)
AutoLISP 예제가 손으로 구현했던 해시테이블을, UE5에서는 `TMap`이 대신
해준다고 보면 된다.

### 정렬 `TArray<TPair<double,int32>> SortedByX` + `Algo::BinarySearch` — README에서 유일하게 새로 등장하는 구조

이게 이 예제의 핵심 추가 포인트다. README 요약표의 "정렬된 데이터 반복 조회,
변경은 드묾 → 정렬 TArray + Binary Search" 행을 코드로 옮긴 부분이다.

**왜 `TMap`이 아니라 정렬 배열인가**: `TMap`은 정확히 하나의 Key로 조회할 때
빠르지만, "X가 100~500 사이인 것 전부"처럼 **범위 검색**에는 쓸 수 없다
(해시는 순서 정보를 버리기 때문). 반면 정렬된 배열은 이웃한 값들이 물리적으로
붙어있으므로 범위 검색이 자연스럽다.

**왜 항상 정렬 상태를 유지하지 않고 `BuildSortedIndex()`를 따로 호출하는가**:
`AddEntity()`를 호출할 때마다 정렬 순서를 유지하려면 삽입 위치를 찾아
배열 중간에 끼워 넣어야 해서 매번 O(N) 비용이 든다. 이 예제는 "도면 편집이
끝난 뒤, 검토/BOM 집계 단계에서 반복 조회"라는 워크플로우를 가정하므로,
정렬을 딱 한 번(`BuildSortedIndex`, O(N log N))만 하고 그 이후 조회를
전부 O(log N)으로 받는 게 이득이다. `AddEntity()`가 끝에 `bSortedIndexDirty = true`를
세팅하는 것도 이 가정을 지키기 위한 안전장치 — "정렬 인덱스가 최신인지"를
추적해서, 최신이 아니면 `FindByXRange()`가 빈 결과를 반환하도록 막는다.

**Binary Search가 실제로 하는 일** (`FindByXRange()`):

```cpp
const int32 StartIdx = Algo::LowerBoundBy(SortedByX, MinX, &TPair<double, int32>::Key);
for (int32 i = StartIdx; i < SortedByX.Num(); ++i)
{
    if (SortedByX[i].Key > MaxX) break;
    Result.Add(&Entities[SortedByX[i].Value]);
}
```

`LowerBoundBy`가 O(log N)으로 "MinX가 들어갈 자리"를 찾고, 그 지점부터는
이미 정렬돼 있으므로 `MaxX`를 넘는 순간 바로 멈춘다. 선형 탐색이었다면
매번 전체 엔티티를 다 봐야 했을 것을 로그 스케일로 줄인 것이다.

### `TSet<FString> Seen` — Fab의 AlarmSet과 동일한 역할, 다른 문맥

`CollectUniqueTagsInLayer()`는 레이어 전체를 순회(`TArray`)하면서, 같은 태그가
여러 뷰포트에 중복 등장하는 DXF 특성 때문에 "이미 집계했는지"를 걸러야 한다.
Fab 예제의 `AlarmSet`과 마찬가지로 값은 필요 없고 존재 여부만 필요하므로 `TSet`.
여기서는 `Add()`의 두 번째 인자(`bool* bAlreadyInSet`)로 삽입 시도와 중복 여부
확인을 한 번의 호출로 합쳤다 — README의 "같은 key를 두 번 조회하지 말라" 원칙을
`TSet`에도 동일하게 적용한 것.

---

## 두 예제를 관통하는 요약

| 개념(README) | Fab 예제 구현 | DXF 예제 구현 |
|---|---|---|
| 전체 순회 hot path → TArray | `Equipment` + `TickAll()` | `Entities` + `GetAllEntities()` |
| ID lookup hot path → TMap, `Find()` 1회 | `IndexMap` + `FindEquipment()` | `TagToIndex` + `FindByTag()` |
| 존재 여부만 → TSet | `AlarmSet` | `Seen` (지역 변수) |
| 정렬 데이터 반복 조회 → 정렬 배열 + Binary Search | (해당 없음 — 설비는 좌표 범위 검색 불필요) | `SortedByX` + `FindByXRange()` |
| 삭제가 드물고 순서 무관 → swap-remove로 O(1) | `RemoveEquipment()` | (해당 없음 — 예제에 삭제 미구현) |

두 예제가 서로 "빠진 부분"을 채워주도록 의도적으로 구성했다 — Fab 예제는
삭제 최적화(swap-remove)를, DXF 예제는 정렬 배열+Binary Search를 각각
대표로 보여준다. 실제 프로젝트에서는 한 클래스 안에 이 다섯 가지 패턴이
동시에 필요할 수도 있다.
