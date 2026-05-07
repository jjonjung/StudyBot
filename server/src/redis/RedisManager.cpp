#include "RedisManager.h"
#include <stdarg.h>
#include <cstring>
#include <iostream>

RedisManager& RedisManager::instance() {
    static RedisManager inst;
    return inst;
}

RedisManager::~RedisManager() {
    if (ctx_) {
        redisFree(ctx_);
        ctx_ = nullptr;
    }
}

bool RedisManager::connect(const std::string& host, int port, const std::string& password) {
    std::lock_guard<std::mutex> lock(mutex_);

    struct timeval timeout{ 2, 0 };  // 2초 연결 타임아웃
    ctx_ = redisConnectWithTimeout(host.c_str(), port, timeout);

    if (!ctx_ || ctx_->err) {
        std::cerr << "[Redis] connect failed: "
                  << (ctx_ ? ctx_->errstr : "allocation error") << "\n";
        if (ctx_) { redisFree(ctx_); ctx_ = nullptr; }
        return false;
    }

    if (!password.empty()) {
        auto* r = static_cast<redisReply*>(
            redisCommand(ctx_, "AUTH %s", password.c_str()));
        if (!r || r->type == REDIS_REPLY_ERROR) {
            std::cerr << "[Redis] AUTH failed\n";
            if (r) freeReplyObject(r);
            redisFree(ctx_); ctx_ = nullptr;
            return false;
        }
        freeReplyObject(r);
    }

    std::cout << "[Redis] connected to " << host << ":" << port << "\n";
    return true;
}

// ── 내부 헬퍼 ──────────────────────────────────────────────────────────────

redisReply* RedisManager::command(const char* fmt, ...) {
    if (!ctx_) return nullptr;
    va_list ap;
    va_start(ap, fmt);
    auto* r = static_cast<redisReply*>(redisvCommand(ctx_, fmt, ap));
    va_end(ap);
    if (!r || ctx_->err) {
        std::cerr << "[Redis] command error: " << ctx_->errstr << "\n";
        if (r) { freeReplyObject(r); r = nullptr; }
    }
    return r;
}

// ── 블랙리스트 ─────────────────────────────────────────────────────────────

void RedisManager::blacklistToken(const std::string& token, int ttlSeconds) {
    std::lock_guard<std::mutex> lock(mutex_);
    // SETEX bl:<token> <ttl> "1"
    auto* r = command("SETEX bl:%s %d 1", token.c_str(), ttlSeconds);
    if (r) freeReplyObject(r);
}

bool RedisManager::isBlacklisted(const std::string& token) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto* r = command("EXISTS bl:%s", token.c_str());
    if (!r) return false;
    bool found = (r->type == REDIS_REPLY_INTEGER && r->integer == 1);
    freeReplyObject(r);
    return found;
}

// ── 범용 캐시 ──────────────────────────────────────────────────────────────

void RedisManager::set(const std::string& key, const std::string& value, int ttlSeconds) {
    std::lock_guard<std::mutex> lock(mutex_);
    // SETEX key ttl value  (바이너리 안전하게 %b 사용)
    auto* r = command("SETEX %s %d %b",
                      key.c_str(), ttlSeconds,
                      value.data(), value.size());
    if (r) freeReplyObject(r);
}

std::optional<std::string> RedisManager::get(const std::string& key) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto* r = command("GET %s", key.c_str());
    if (!r) return std::nullopt;

    std::optional<std::string> result;
    if (r->type == REDIS_REPLY_STRING)
        result = std::string(r->str, r->len);
    freeReplyObject(r);
    return result;
}

void RedisManager::del(const std::string& key) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto* r = command("DEL %s", key.c_str());
    if (r) freeReplyObject(r);
}
