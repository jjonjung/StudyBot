// FabEquipmentRegistry.cpp

#include "FabEquipmentRegistry.h"

void FFabEquipmentRegistry::AddEquipment(const FEquipmentState& NewState)
{
	// 이미 있는 ID면 무시 (identity 충돌 방지)
	if (IndexMap.Contains(NewState.EquipmentId))
	{
		return;
	}

	const int32 NewIndex = Equipment.Add(NewState);
	IndexMap.Add(NewState.EquipmentId, NewIndex);
}

bool FFabEquipmentRegistry::RemoveEquipment(FName EquipmentId)
{
	const int32* FoundIndex = IndexMap.Find(EquipmentId);
	if (!FoundIndex)
	{
		return false;
	}

	const int32 RemoveIndex = *FoundIndex;
	const int32 LastIndex = Equipment.Num() - 1;

	// swap-remove: 마지막 원소를 삭제 위치로 옮기고 배열을 한 칸 줄인다 (O(1)).
	// 순서를 보존할 필요가 없기 때문에 가능한 최적화.
	if (RemoveIndex != LastIndex)
	{
		Equipment[RemoveIndex] = Equipment[LastIndex];
		// 옮겨진 원소의 인덱스를 갱신해야 IndexMap이 계속 정확하다.
		IndexMap[Equipment[RemoveIndex].EquipmentId] = RemoveIndex;
	}

	Equipment.RemoveAt(LastIndex, 1, /*bAllowShrinking=*/false);
	IndexMap.Remove(EquipmentId);
	AlarmSet.Remove(EquipmentId);

	return true;
}

void FFabEquipmentRegistry::TickAll(float DeltaSeconds)
{
	// hot path: TMap/TSet lookup 없이 배열만 순회한다.
	// 10,000개 규모에서 이 연속 순회가 프레임 예산을 좌우한다.
	for (FEquipmentState& State : Equipment)
	{
		// 예시 물리: 목표 온도(정상 가동 온도)로 서서히 수렴
		constexpr float TargetTemperature = 65.0f;
		constexpr float ThermalRate = 0.5f;
		State.Temperature = FMath::FInterpTo(State.Temperature, TargetTemperature, DeltaSeconds, ThermalRate);
	}
}

FEquipmentState* FFabEquipmentRegistry::FindEquipment(FName EquipmentId)
{
	// Find()로 존재 확인과 인덱스 획득을 한 번의 lookup으로 처리
	if (const int32* FoundIndex = IndexMap.Find(EquipmentId))
	{
		return &Equipment[*FoundIndex];
	}
	return nullptr;
}

void FFabEquipmentRegistry::RaiseAlarm(FName EquipmentId)
{
	// TSet::Add는 이미 존재하면 아무 일도 하지 않는다 — 중복 알람 방지가 공짜로 딸려온다.
	AlarmSet.Add(EquipmentId);
}

void FFabEquipmentRegistry::ClearAlarm(FName EquipmentId)
{
	AlarmSet.Remove(EquipmentId);
}

bool FFabEquipmentRegistry::IsInAlarm(FName EquipmentId) const
{
	return AlarmSet.Contains(EquipmentId);
}
