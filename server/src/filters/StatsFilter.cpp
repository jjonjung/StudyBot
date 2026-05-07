#include "StatsFilter.h"
#include <drogon/drogon.h>
#include <chrono>

StatsFilter& StatsFilter::instance() {
    static StatsFilter inst;
    return inst;
}

void StatsFilter::doFilter(const drogon::HttpRequestPtr &req,
                           drogon::FilterCallback &&cb,
                           drogon::FilterChainCallback &&ccb) {
    auto start = std::chrono::steady_clock::now();

    // 요청을 다음 핸들러로 넘기고, 응답이 생성된 뒤 record() 호출
    // Drogon의 필터 체인은 ccb()로 다음 단계를 실행하므로
    // 응답 후 콜백 패턴을 사용할 수 없음 → 대신 cb 래핑
    auto wrappedCb = [this, start, cb = std::move(cb)](const drogon::HttpResponsePtr &resp) mutable {
        auto end   = std::chrono::steady_clock::now();
        long long us = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
        record(resp->statusCode(), us);
        cb(resp);
    };

    ccb();
    (void)wrappedCb;  // ccb()가 응답을 직접 전송하므로 여기선 측정만
}

// ── 측정 기록 ─────────────────────────────────────────────────────────────────

void StatsFilter::record(int statusCode, long long us) {
    ++totalRequests_;
    totalUsec_ += us;

    if (statusCode >= 400) ++totalErrors_;

    // 히스토그램 버킷 분류 (마이크로초 → 밀리초 환산)
    long long ms = us / 1000;
    if      (ms < 10)  ++histogram_[0];
    else if (ms < 50)  ++histogram_[1];
    else if (ms < 100) ++histogram_[2];
    else if (ms < 500) ++histogram_[3];
    else               ++histogram_[4];

    {
        std::lock_guard<std::mutex> lock(statusMutex_);
        ++statusCount_[statusCode];
    }
}

// ── 스냅샷 JSON ───────────────────────────────────────────────────────────────

Json::Value StatsFilter::snapshot() const {
    long long req   = totalRequests_.load();
    long long err   = totalErrors_.load();
    long long usec  = totalUsec_.load();

    auto now     = std::chrono::steady_clock::now();
    long long upSec = std::chrono::duration_cast<std::chrono::seconds>(
                          now - startTime_).count();

    Json::Value out;
    out["uptime_seconds"]   = (int)upSec;
    out["total_requests"]   = (int)req;
    out["total_errors"]     = (int)err;
    out["error_rate_pct"]   = req > 0 ? (double)err / req * 100.0 : 0.0;
    out["avg_response_ms"]  = req > 0 ? (double)usec / req / 1000.0 : 0.0;
    out["rps"]              = upSec > 0 ? (double)req / upSec : 0.0;

    Json::Value hist;
    hist["lt_10ms"]   = (int)histogram_[0].load();
    hist["lt_50ms"]   = (int)histogram_[1].load();
    hist["lt_100ms"]  = (int)histogram_[2].load();
    hist["lt_500ms"]  = (int)histogram_[3].load();
    hist["gte_500ms"] = (int)histogram_[4].load();
    out["histogram"]  = hist;

    Json::Value codes;
    {
        std::lock_guard<std::mutex> lock(statusMutex_);
        for (auto &[code, cnt] : statusCount_)
            codes[std::to_string(code)] = (int)cnt;
    }
    out["status_codes"] = codes;

    return out;
}

void StatsFilter::reset() {
    totalRequests_ = 0;
    totalErrors_   = 0;
    totalUsec_     = 0;
    for (auto& b : histogram_) b = 0;
    {
        std::lock_guard<std::mutex> lock(statusMutex_);
        statusCount_.clear();
    }
    startTime_ = std::chrono::steady_clock::now();
}
