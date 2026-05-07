#pragma once
#include <drogon/HttpFilter.h>
#include <atomic>
#include <chrono>
#include <mutex>
#include <map>

// 모든 API 요청에 걸리는 성능 측정 필터
//
// 수집 지표:
//   - 총 요청 수 / 총 에러 수 (4xx+5xx)
//   - 누적 응답시간 합계 → 평균 계산
//   - 응답시간 히스토그램 (버킷: <10ms / <50ms / <100ms / <500ms / 500ms+)
//   - 상태코드별 카운트
//
// GET /api/internal/stats 로 JSON 조회
class StatsFilter : public drogon::HttpFilter<StatsFilter> {
public:
    void doFilter(const drogon::HttpRequestPtr &req,
                  drogon::FilterCallback &&cb,
                  drogon::FilterChainCallback &&ccb) override;

    // 싱글톤 접근
    static StatsFilter& instance();

    // /api/internal/stats 핸들러에서 호출
    Json::Value snapshot() const;
    void reset();

private:
    void record(int statusCode, long long microseconds);

    // lock-free 카운터로 hot path 성능 유지
    std::atomic<long long> totalRequests_{ 0 };
    std::atomic<long long> totalErrors_{ 0 };
    std::atomic<long long> totalUsec_{ 0 };

    // 히스토그램 버킷 (응답시간 분포)
    // [0]<10ms [1]<50ms [2]<100ms [3]<500ms [4]500ms+
    std::atomic<long long> histogram_[5]{};

    // 상태코드별 집계 (mutex 필요)
    mutable std::mutex         statusMutex_;
    std::map<int, long long>   statusCount_;

    std::chrono::steady_clock::time_point startTime_{ std::chrono::steady_clock::now() };
};
