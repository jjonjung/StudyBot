#pragma once
// 시스템 설치 시: <hiredis/hiredis.h>, FetchContent 빌드 시: <hiredis.h>
#if __has_include(<hiredis/hiredis.h>)
#  include <hiredis/hiredis.h>
#else
#  include <hiredis.h>
#endif
#include <string>
#include <optional>
#include <mutex>

// hiredis 동기 클라이언트 래퍼 (싱글톤)
// 용도:
//   1. JWT 블랙리스트  — 로그아웃된 토큰을 TTL과 함께 저장
//   2. 인터뷰 카드 캐시 — category+count 조합 결과를 60초 캐싱
class RedisManager {
public:
    static RedisManager& instance();

    // 연결 (main 또는 config 로드 후 1회 호출)
    bool connect(const std::string& host, int port, const std::string& password = "");

    // ── 블랙리스트 ──────────────────────────────────────────
    // token을 블랙리스트에 추가. ttlSeconds = 토큰 만료 잔여 시간
    void blacklistToken(const std::string& token, int ttlSeconds);
    bool isBlacklisted(const std::string& token);

    // ── 범용 캐시 ────────────────────────────────────────────
    void set(const std::string& key, const std::string& value, int ttlSeconds);
    std::optional<std::string> get(const std::string& key);
    void del(const std::string& key);

    bool isConnected() const { return ctx_ != nullptr; }

private:
    RedisManager() = default;
    ~RedisManager();
    RedisManager(const RedisManager&) = delete;
    RedisManager& operator=(const RedisManager&) = delete;

    redisContext* ctx_{ nullptr };
    mutable std::mutex mutex_;

    // 재접속 없이 단순 명령 전송; 실패 시 nullptr 반환
    redisReply* command(const char* fmt, ...);
};
