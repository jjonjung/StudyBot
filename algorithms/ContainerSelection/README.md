# 자료구조 선택 기준 — TArray / 정렬 배열 / TSet / TMap

Big-O만 보고 고르면 틀리기 쉽다. 같은 "10,000개짜리 컬렉션"이라도
**그 프로그램이 실제로 무슨 연산을 얼마나 자주 하는지**에 따라 정답이 갈린다.

## 핵심 질문 4가지

10,000개의 데이터(적 상태든, 설비 상태든, 도면 엔티티든)를 다룬다고 하자.
아래 4가지 질문에 대한 답이 자료구조 선택을 결정한다.

| 질문 | 답이 "그렇다"일 때 유리한 구조 |
|---|---|
| 1. 매 tick(프레임/스캔) 전체를 순회하는가? | `TArray` — 연속 메모리, 캐시 지역성, 순회 상수 비용이 낮음 |
| 2. 특정 ID를 아주 자주 찾는가? | `TMap`(Key→Value) 또는 `TSet`(존재만) — 평균 O(1) 조회 |
| 3. 추가·삭제가 잦은가? | 삭제 빈도가 높으면 `TArray`의 중간 삭제(O(N) shift)보다 `TMap`/`TSet`이 유리. 삽입만 잦고 끝에 추가면 `TArray::Add`가 더 쌈 |
| 4. 저장 순서가 의미 있는가? | `TArray` 또는 "정렬 상태를 유지하는 배열 + Binary Search" |

## 왜 Big-O만으로는 부족한가

- `TArray` 선형 탐색은 O(N)이지만, 원소가 작고 개수가 적으면(수십~수백 개)
  실제로는 `TMap`의 해시 계산 비용보다 빠를 수 있다. 캐시 미스가 없기 때문.
- `TMap`/`TSet`은 평균 O(1)이지만 해시 계산 비용, 버킷 충돌, 메모리 단편화
  (요소가 흩어져 있어 캐시 지역성이 나쁨)라는 대가가 있다.
- 정렬 배열 + Binary Search는 조회는 O(log N)으로 빠르지만, 삽입/삭제 시
  정렬 상태를 유지하려면 O(N) 이동 비용이 든다. **삽입·삭제가 드물고
  조회가 잦은 "거의 정적인 데이터"**에 적합하다.

## 요약 매트릭스

| 목적 | 우선 검토 |
|---|---|
| 전체 순회가 hot path | `TArray` |
| ID/Key lookup이 hot path | `TMap` (Key→Value) / `TSet` (존재 여부만) |
| 정렬된 데이터를 반복 조회, 변경은 드묾 | 정렬 `TArray` + `Algo::BinarySearch` |
| 순서 보존이 중요 | `TArray` 또는 정렬 구조 |
| 중복 방지 / 존재 여부만 필요 | `TSet` |
| Key ↔ Value 임의 접근 | `TMap` |

## `Find` vs `Contains` + `operator[]`

```cpp
// 나쁜 예 — 같은 Key를 두 번 조회 (Contains에서 한 번, operator[]에서 또 한 번)
if (EquipmentMap.Contains(EquipmentId))
{
    FEquipmentState& State = EquipmentMap[EquipmentId];
    State.Temperature = NewTemp;
}

// 좋은 예 — Find 한 번으로 존재 확인 + 포인터 획득을 동시에 처리
if (FEquipmentState* State = EquipmentMap.Find(EquipmentId))
{
    State->Temperature = NewTemp;
}
```

`TMap::Find`는 존재 확인과 값 접근을 한 번의 해시 lookup으로 묶어준다.
Epic 공식 문서(UE 5.8)도 이 패턴을 권장한다.

## 실제 우열을 가르는 변수

요소 크기, 데이터 개수, hash 계산 비용, 충돌률, 삽입·삭제 빈도, 캐시 지역성,
allocator, 목표 플랫폼(콘솔/모바일은 캐시 미스 페널티가 더 큼) — 이 모든 것이
실제 벤치마크 결과에 영향을 준다. **"이론상 더 낮은 Big-O"가 아니라
"이 workload에서 실측이 더 빠른가"로 검증한다.**

---

## 이 폴더의 예제

프로필 두 도메인(반도체 fab 3D 시뮬레이션, 2D DXF CAD 설계)에 맞춰
같은 개념을 두 번 다른 문맥으로 보여준다.

### 1. [FabEquipmentRegistry](FabEquipmentRegistry.h) — UE5 반도체 Fab 3D 시뮬레이션

10,000대 설비(Equipment)를 매 tick 시뮬레이션하면서, 동시에 특정 설비를
ID로 자주 찾고, 알람 상태인 설비 집합을 관리하는 상황.

- `TArray<FEquipmentState>` — 매 tick 전체 순회(온도/압력 업데이트)는 배열로
- `TMap<FName, int32>` — EquipmentId → 배열 인덱스 역참조는 맵으로
- `TSet<FName>` — "지금 알람 상태인 설비 집합"은 셋으로 (중복 없는 존재 여부)

### 2. [DxfEntityIndex](DxfEntityIndex.h) — 2D DXF CAD 설계

DXF 도면의 엔티티(배관, 심볼, 텍스트)를 레이어별로 관리하며, 태그로
빈번히 조회하고, X좌표 기준 정렬된 상태를 유지해 범위 검색을 하는 상황.

- `TArray<FDxfEntity>` — 레이어 전체 export/렌더링 시 순회
- `TMap<FString, int32>` — 태그 문자열 → 인덱스, 클릭/스크립트 조회용
- 정렬 `TArray` + `Algo::BinarySearch` — X좌표 범위 내 엔티티 검색(변경이 드문 완성 도면)
- `TSet<FString>` — 이미 처리된 태그 중복 방지(BOM 집계, 간섭 체크 스캔 시)
