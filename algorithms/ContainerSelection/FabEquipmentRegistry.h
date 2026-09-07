// FabEquipmentRegistry.h
//
// 반도체 Fab 3D 시뮬레이션 예제: 설비(Equipment) 10,000대를 관리한다.
//
// 4가지 질문에 대한 이 시스템의 답:
//   1. 매 tick 전체를 순회하는가?      -> Yes, 모든 설비의 온도/압력을 갱신한다.
//   2. 특정 ID를 자주 찾는가?          -> Yes, UI 클릭/알람 발생 시 EquipmentId로 조회한다.
//   3. 추가·삭제가 잦은가?             -> No, 런타임 중 설비 수는 거의 고정된다.
//   4. 저장 순서가 의미 있는가?        -> No(굳이 필요하지 않음), 하지만 배열 인덱스 안정성은 필요.
//
// 그래서:
//   - TArray<FEquipmentState>  : 매 tick 순회의 본체. 연속 메모리로 캐시 지역성이 좋다.
//   - TMap<FName,int32>        : EquipmentId -> TArray 인덱스 역참조. Find()로 1회 lookup.
//   - TSet<FName>              : "현재 알람 상태" 집합. 존재 여부만 필요하므로 TMap 대신 TSet.

#pragma once

#include "CoreMinimal.h"

USTRUCT()
struct FEquipmentState
{
	GENERATED_BODY()

	UPROPERTY()
	FName EquipmentId;

	UPROPERTY()
	float Temperature = 20.0f;

	UPROPERTY()
	float Pressure = 1.0f;

	UPROPERTY()
	FVector WorldLocation = FVector::ZeroVector;
};

/**
 * 10,000대 규모 Fab 설비를 관리하는 레지스트리.
 *
 * TArray를 "정답 데이터"로 두고, TMap/TSet은 그 위에 얹는 색인(index)이라는
 * 점이 핵심이다. 색인은 배열 인덱스가 바뀔 때마다 함께 갱신해야 한다
 * (예: RemoveEquipment에서 swap-remove를 쓰면 마지막 원소의 인덱스가
 * 바뀌므로 IndexMap도 같이 고쳐줘야 한다).
 */
class FFabEquipmentRegistry
{
public:
	/** 설비 추가 — 배열 끝에 추가(O(1) amortized) + 인덱스맵 갱신 */
	void AddEquipment(const FEquipmentState& NewState);

	/**
	 * 설비 제거 — swap-remove로 O(1) 삭제.
	 * TArray::RemoveAt(Index, 1, false)는 뒤 원소들을 앞으로 당기므로 O(N).
	 * 순서가 중요하지 않다면 RemoveAtSwap으로 마지막 원소를 그 자리에 옮기고
	 * IndexMap만 갱신하는 편이 훨씬 싸다.
	 */
	bool RemoveEquipment(FName EquipmentId);

	/**
	 * 매 tick 호출 — 전체 설비 온도/압력을 갱신한다.
	 * TArray를 인덱스로 순회하므로 캐시 지역성이 좋고, 분기 예측도 단순하다.
	 * 이 경로가 hot path이므로 TMap/TSet lookup은 여기 넣지 않는다.
	 */
	void TickAll(float DeltaSeconds);

	/**
	 * ID로 설비 하나를 자주 찾는 경로.
	 * Contains() 후 operator[]로 두 번 찾지 않고 Find() 한 번으로 처리한다.
	 */
	FEquipmentState* FindEquipment(FName EquipmentId);

	/** 알람 등록 — TSet은 중복을 자동으로 막아준다 (이미 있으면 no-op) */
	void RaiseAlarm(FName EquipmentId);

	void ClearAlarm(FName EquipmentId);

	/** 알람 상태 여부 확인 — TSet::Contains는 평균 O(1) */
	bool IsInAlarm(FName EquipmentId) const;

	/** 현재 알람 설비 개수 (UI 배지 등에 사용) */
	int32 GetAlarmCount() const { return AlarmSet.Num(); }

	int32 GetEquipmentCount() const { return Equipment.Num(); }

private:
	/** 정답 데이터. 매 tick 순회의 대상. */
	TArray<FEquipmentState> Equipment;

	/** EquipmentId -> Equipment 배열 인덱스. ID lookup 전용 색인. */
	TMap<FName, int32> IndexMap;

	/** 현재 알람 상태인 EquipmentId 집합. Key만 필요하므로 TMap이 아닌 TSet. */
	TSet<FName> AlarmSet;
};
