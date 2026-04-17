#pragma once

#include "CoreMinimal.h"
#include "StudyBotTypes.generated.h"

// ── 카테고리 ────────────────────────────────────────────────
UENUM(BlueprintType)
enum class ECardCategory : uint8
{
    All       UMETA(DisplayName = "전체"),
    Unreal    UMETA(DisplayName = "Unreal"),
    Cpp       UMETA(DisplayName = "C++"),
    CS        UMETA(DisplayName = "CS"),
    Company   UMETA(DisplayName = "기업 기출"),
    Algorithm UMETA(DisplayName = "알고리즘"),
};

// ── 난이도 ───────────────────────────────────────────────────
UENUM(BlueprintType)
enum class ECardDifficulty : uint8
{
    Easy   UMETA(DisplayName = "입문"),
    Normal UMETA(DisplayName = "실무"),
    Hard   UMETA(DisplayName = "심화"),
};

// ── 플래시카드 데이터 ─────────────────────────────────────
USTRUCT(BlueprintType)
struct FFlashCard
{
    GENERATED_BODY()

    // ── 공통 필드 ────────────────────────────────────────
    UPROPERTY(BlueprintReadOnly) int32   Id         = 0;
    UPROPERTY(BlueprintReadOnly) FString Category;
    UPROPERTY(BlueprintReadOnly) FString Company;       // Company 카테고리만
    UPROPERTY(BlueprintReadOnly) FString Question;
    UPROPERTY(BlueprintReadOnly) FString Answer;
    UPROPERTY(BlueprintReadOnly) FString Difficulty;

    // ── 알고리즘 전용 필드 ──────────────────────────────
    UPROPERTY(BlueprintReadOnly) FString CoreConditions;    // 핵심 조건 정리
    UPROPERTY(BlueprintReadOnly) FString SelectionReason;   // 알고리즘 선택 이유
    UPROPERTY(BlueprintReadOnly) FString CodeCpp;           // C++ 모범 코드
    UPROPERTY(BlueprintReadOnly) FString CodeCSharp;        // C# 모범 코드
    UPROPERTY(BlueprintReadOnly) FString TimeComplexity;    // 시간복잡도

    // ── 세션 내 플래그 (DB 비저장) ──────────────────────
    UPROPERTY(BlueprintReadWrite) bool  bKnown = false;
    UPROPERTY(BlueprintReadWrite) int32 Score  = 0;

    bool IsAlgorithm() const { return Category == TEXT("Algorithm"); }
};

// ── 인터뷰 세션 결과 ──────────────────────────────────────
USTRUCT(BlueprintType)
struct FInterviewResult
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly) FString Category;
    UPROPERTY(BlueprintReadOnly) int32   TotalCards  = 0;
    UPROPERTY(BlueprintReadOnly) int32   KnownCount  = 0;

    float GetScore() const
    {
        return TotalCards > 0 ? (float)KnownCount / TotalCards * 100.f : 0.f;
    }
};

// ── 로그인 정보 ──────────────────────────────────────────
USTRUCT(BlueprintType)
struct FAuthInfo
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly) FString Token;
    UPROPERTY(BlueprintReadOnly) FString Nickname;
    UPROPERTY(BlueprintReadOnly) int32   UserId    = 0;
    UPROPERTY(BlueprintReadOnly) FString AvatarUrl;  // Google 프로필 이미지
    UPROPERTY(BlueprintReadOnly) bool    bIsGoogle = false;

    bool IsValid() const { return !Token.IsEmpty(); }
};

// ── 잔디(기여도) 1일 1행 ─────────────────────────────────
USTRUCT(BlueprintType)
struct FHeatmapEntry
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly) FString ScoreDate;   // "YYYY-MM-DD"
    UPROPERTY(BlueprintReadOnly) FString Category;
    UPROPERTY(BlueprintReadOnly) int32   CardsDone  = 0;
    UPROPERTY(BlueprintReadOnly) int32   KnownCount = 0;
    UPROPERTY(BlueprintReadOnly) float   Ratio      = 0.f; // 0~1
};

// ── 방(Room) 설정 정보 ───────────────────────────────────
USTRUCT(BlueprintType)
struct FRoomConfig
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly) ECardCategory Category = ECardCategory::Unreal;
    UPROPERTY(BlueprintReadOnly) FString        Label;
    UPROPERTY(BlueprintReadOnly) FLinearColor   RoomColor = FLinearColor::White;
};

// ── 로비 역할 ────────────────────────────────────────────
UENUM(BlueprintType)
enum class ELobbyRole : uint8
{
    Host   UMETA(DisplayName = "호스트"),
    Member UMETA(DisplayName = "멤버"),
};

// ── 로비 상태 ─────────────────────────────────────────────
UENUM(BlueprintType)
enum class ELobbyStatus : uint8
{
    Waiting    UMETA(DisplayName = "대기"),
    InProgress UMETA(DisplayName = "진행 중"),
    Closed     UMETA(DisplayName = "종료"),
};

// ── 로비 멤버 ─────────────────────────────────────────────
USTRUCT(BlueprintType)
struct FLobbyMember
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly) int32      UserId   = 0;
    UPROPERTY(BlueprintReadOnly) FString    Nickname;
    UPROPERTY(BlueprintReadOnly) ELobbyRole Role     = ELobbyRole::Member;
    UPROPERTY(BlueprintReadOnly) bool       bIsReady = false;

    bool IsHost() const { return Role == ELobbyRole::Host; }
};

// ── 로비 정보 ─────────────────────────────────────────────
USTRUCT(BlueprintType)
struct FLobbyInfo
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly) int32              LobbyId    = 0;
    UPROPERTY(BlueprintReadOnly) FString            Code;        // 6자리 입장 코드
    UPROPERTY(BlueprintReadOnly) FString            Name;
    UPROPERTY(BlueprintReadOnly) FString            Category;
    UPROPERTY(BlueprintReadOnly) int32              MaxMembers = 4;
    UPROPERTY(BlueprintReadOnly) TArray<FLobbyMember> Members;

    bool IsValid() const { return LobbyId > 0; }
};
