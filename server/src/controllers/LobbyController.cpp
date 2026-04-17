#include "LobbyController.h"
#include <drogon/drogon.h>
#include <random>

static auto dbErrCb(std::function<void(const drogon::HttpResponsePtr &)> cb) {
    return [cb](const drogon::orm::DrogonDbException &e) {
        std::string msg = e.base().what();
        Json::Value err;
        if      (msg.find("LOBBY_NOT_FOUND")   != std::string::npos) err["error"] = "LOBBY_NOT_FOUND";
        else if (msg.find("LOBBY_FULL")        != std::string::npos) err["error"] = "LOBBY_FULL";
        else if (msg.find("LOBBY_NOT_WAITING") != std::string::npos) err["error"] = "LOBBY_NOT_WAITING";
        else if (msg.find("NOT_HOST")          != std::string::npos) err["error"] = "NOT_HOST";
        else                                                           err["error"] = "server error";
        auto res = drogon::HttpResponse::newHttpJsonResponse(err);
        res->setStatusCode(msg.find("NOT_HOST") != std::string::npos
                           ? drogon::k403Forbidden : drogon::k400BadRequest);
        cb(res);
    };
}

static int userId(const drogon::HttpRequestPtr &req) {
    return req->attributes()->get<int>("user_id");
}

static std::string genCode() {
    static const char chars[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    static std::mt19937 rng(std::random_device{}());
    static std::uniform_int_distribution<> dist(0, sizeof(chars) - 2);
    std::string code(6, ' ');
    for (auto &c : code) c = chars[dist(rng)];
    return code;
}

static Json::Value buildLobbyResponse(const drogon::orm::Result &r) {
    Json::Value out;
    out["lobbyId"]  = r[0]["id"].as<int>();
    out["code"]     = r[0]["code"].as<std::string>();
    out["name"]     = r[0]["name"].as<std::string>();
    out["category"] = r[0]["category"].as<std::string>();
    Json::Value members(Json::arrayValue);
    for (auto &row : r) {
        Json::Value m;
        m["userId"]   = row["user_id"].as<int>();
        m["nickname"] = row["nickname"].as<std::string>();
        m["role"]     = row["role"].as<std::string>();
        m["isReady"]  = row["is_ready"].as<int>();
        members.append(m);
    }
    out["members"] = members;
    return out;
}

void LobbyController::create(const drogon::HttpRequestPtr &req,
                             std::function<void(const drogon::HttpResponsePtr &)> &&cb) {
    auto body = req->getJsonObject();
    if (!body || !body->isMember("name")) {
        Json::Value err; err["error"] = "name is required";
        auto res = drogon::HttpResponse::newHttpJsonResponse(err);
        res->setStatusCode(drogon::k400BadRequest);
        cb(res); return;
    }

    auto name       = (*body)["name"].asString();
    auto category   = body->isMember("category")   ? (*body)["category"].asString() : "Mixed";
    int  maxMembers = body->isMember("maxMembers")  ? (*body)["maxMembers"].asInt()  : 6;
    auto code       = genCode();
    int  uid        = userId(req);

    auto db = drogon::app().getDbClient();
    db->execSqlAsync(
        "INSERT INTO lobbies (name,host_user_id,category,max_members,code) VALUES (?,?,?,?,?)",
        [cb, db, uid, name, category, maxMembers, code](const drogon::orm::Result &r) {
            int lobbyId = (int)r.insertId();
            db->execSqlAsync(
                "INSERT INTO lobby_members (lobby_id,user_id,role) VALUES (?,?,'host')",
                [cb, lobbyId, code, name, category, maxMembers](const drogon::orm::Result &) {
                    Json::Value out;
                    out["lobbyId"]    = lobbyId;
                    out["code"]       = code;
                    out["name"]       = name;
                    out["category"]   = category;
                    out["maxMembers"] = maxMembers;
                    auto res = drogon::HttpResponse::newHttpJsonResponse(out);
                    res->setStatusCode(drogon::k201Created);
                    cb(res);
                },
                [cb](const drogon::orm::DrogonDbException &) {
                    Json::Value err; err["error"] = "server error";
                    auto res = drogon::HttpResponse::newHttpJsonResponse(err);
                    res->setStatusCode(drogon::k500InternalServerError);
                    cb(res);
                },
                lobbyId, uid
            );
        },
        dbErrCb(cb), name, uid, category, maxMembers, code
    );
}

void LobbyController::join(const drogon::HttpRequestPtr &req,
                           std::function<void(const drogon::HttpResponsePtr &)> &&cb) {
    auto body = req->getJsonObject();
    if (!body || !body->isMember("code")) {
        Json::Value err; err["error"] = "code is required";
        auto res = drogon::HttpResponse::newHttpJsonResponse(err);
        res->setStatusCode(drogon::k400BadRequest);
        cb(res); return;
    }

    auto code = (*body)["code"].asString();
    int  uid  = userId(req);
    auto db   = drogon::app().getDbClient();

    db->execSqlAsync(
        "SELECT id,max_members,status FROM lobbies WHERE code=? LIMIT 1",
        [cb, db, uid, code](const drogon::orm::Result &r) {
            if (r.empty()) {
                Json::Value err; err["error"] = "LOBBY_NOT_FOUND";
                auto res = drogon::HttpResponse::newHttpJsonResponse(err);
                res->setStatusCode(drogon::k404NotFound);
                cb(res); return;
            }
            int lobbyId    = r[0]["id"].as<int>();
            int maxMembers = r[0]["max_members"].as<int>();
            auto status    = r[0]["status"].as<std::string>();

            if (status != "waiting") {
                Json::Value err; err["error"] = "LOBBY_NOT_WAITING";
                auto res = drogon::HttpResponse::newHttpJsonResponse(err);
                res->setStatusCode(drogon::k400BadRequest);
                cb(res); return;
            }

            db->execSqlAsync(
                "SELECT COUNT(*) AS cnt FROM lobby_members WHERE lobby_id=?",
                [cb, db, uid, lobbyId, maxMembers](const drogon::orm::Result &r2) {
                    if (r2[0]["cnt"].as<int>() >= maxMembers) {
                        Json::Value err; err["error"] = "LOBBY_FULL";
                        auto res = drogon::HttpResponse::newHttpJsonResponse(err);
                        res->setStatusCode(drogon::k400BadRequest);
                        cb(res); return;
                    }
                    db->execSqlAsync(
                        "INSERT IGNORE INTO lobby_members (lobby_id,user_id,role) VALUES (?,'member')",
                        [cb, db, lobbyId](const drogon::orm::Result &) {
                            db->execSqlAsync(
                                "SELECT l.id,l.code,l.name,l.category,l.max_members,l.status,"
                                "lm.user_id,u.nickname,u.avatar_url,lm.role,lm.is_ready "
                                "FROM lobbies l "
                                "JOIN lobby_members lm ON lm.lobby_id=l.id "
                                "JOIN users u ON u.id=lm.user_id "
                                "WHERE l.id=? ORDER BY lm.joined_at",
                                [cb](const drogon::orm::Result &r3) {
                                    cb(drogon::HttpResponse::newHttpJsonResponse(buildLobbyResponse(r3)));
                                },
                                [cb](const drogon::orm::DrogonDbException &) {
                                    Json::Value err; err["error"] = "server error";
                                    auto res = drogon::HttpResponse::newHttpJsonResponse(err);
                                    res->setStatusCode(drogon::k500InternalServerError);
                                    cb(res);
                                },
                                lobbyId
                            );
                        },
                        [cb](const drogon::orm::DrogonDbException &) {
                            Json::Value err; err["error"] = "server error";
                            auto res = drogon::HttpResponse::newHttpJsonResponse(err);
                            res->setStatusCode(drogon::k500InternalServerError);
                            cb(res);
                        },
                        lobbyId, uid
                    );
                },
                [cb](const drogon::orm::DrogonDbException &) {
                    Json::Value err; err["error"] = "server error";
                    auto res = drogon::HttpResponse::newHttpJsonResponse(err);
                    res->setStatusCode(drogon::k500InternalServerError);
                    cb(res);
                },
                lobbyId
            );
        },
        dbErrCb(cb), code
    );
}

void LobbyController::get(const drogon::HttpRequestPtr &req,
                          std::function<void(const drogon::HttpResponsePtr &)> &&cb,
                          int id) {
    auto db = drogon::app().getDbClient();
    db->execSqlAsync(
        "SELECT l.id,l.code,l.name,l.category,l.max_members,l.status,"
        "lm.user_id,u.nickname,u.avatar_url,lm.role,lm.is_ready "
        "FROM lobbies l "
        "JOIN lobby_members lm ON lm.lobby_id=l.id "
        "JOIN users u ON u.id=lm.user_id "
        "WHERE l.id=? ORDER BY lm.role DESC,lm.joined_at",
        [cb](const drogon::orm::Result &r) {
            if (r.empty()) {
                Json::Value err; err["error"] = "LOBBY_NOT_FOUND";
                auto res = drogon::HttpResponse::newHttpJsonResponse(err);
                res->setStatusCode(drogon::k404NotFound);
                cb(res); return;
            }
            cb(drogon::HttpResponse::newHttpJsonResponse(buildLobbyResponse(r)));
        },
        dbErrCb(cb), id
    );
}

void LobbyController::kick(const drogon::HttpRequestPtr &req,
                           std::function<void(const drogon::HttpResponsePtr &)> &&cb,
                           int id) {
    auto body = req->getJsonObject();
    if (!body || !body->isMember("targetUserId")) {
        Json::Value err; err["error"] = "targetUserId is required";
        auto res = drogon::HttpResponse::newHttpJsonResponse(err);
        res->setStatusCode(drogon::k400BadRequest);
        cb(res); return;
    }

    int targetId = (*body)["targetUserId"].asInt();
    int uid      = userId(req);
    auto db      = drogon::app().getDbClient();

    db->execSqlAsync(
        "SELECT role FROM lobby_members WHERE lobby_id=? AND user_id=?",
        [cb, db, id, targetId](const drogon::orm::Result &r) {
            if (r.empty() || r[0]["role"].as<std::string>() != "host") {
                Json::Value err; err["error"] = "NOT_HOST";
                auto res = drogon::HttpResponse::newHttpJsonResponse(err);
                res->setStatusCode(drogon::k403Forbidden);
                cb(res); return;
            }
            db->execSqlAsync(
                "DELETE FROM lobby_members WHERE lobby_id=? AND user_id=? AND role!='host'",
                [cb](const drogon::orm::Result &) {
                    Json::Value out; out["message"] = "kicked";
                    cb(drogon::HttpResponse::newHttpJsonResponse(out));
                },
                [cb](const drogon::orm::DrogonDbException &) {
                    Json::Value err; err["error"] = "server error";
                    auto res = drogon::HttpResponse::newHttpJsonResponse(err);
                    res->setStatusCode(drogon::k500InternalServerError);
                    cb(res);
                },
                id, targetId
            );
        },
        dbErrCb(cb), id, uid
    );
}

void LobbyController::start(const drogon::HttpRequestPtr &req,
                            std::function<void(const drogon::HttpResponsePtr &)> &&cb,
                            int id) {
    int uid = userId(req);
    auto db = drogon::app().getDbClient();

    db->execSqlAsync(
        "SELECT role FROM lobby_members WHERE lobby_id=? AND user_id=?",
        [cb, db, id](const drogon::orm::Result &r) {
            if (r.empty() || r[0]["role"].as<std::string>() != "host") {
                Json::Value err; err["error"] = "NOT_HOST";
                auto res = drogon::HttpResponse::newHttpJsonResponse(err);
                res->setStatusCode(drogon::k403Forbidden);
                cb(res); return;
            }
            db->execSqlAsync(
                "UPDATE lobbies SET status='in_progress' WHERE id=? AND status='waiting'",
                [cb, db, id](const drogon::orm::Result &) {
                    db->execSqlAsync(
                        "SELECT category FROM lobbies WHERE id=?",
                        [cb](const drogon::orm::Result &r2) {
                            Json::Value out;
                            out["category"] = r2[0]["category"].as<std::string>();
                            cb(drogon::HttpResponse::newHttpJsonResponse(out));
                        },
                        [cb](const drogon::orm::DrogonDbException &) {
                            Json::Value err; err["error"] = "server error";
                            auto res = drogon::HttpResponse::newHttpJsonResponse(err);
                            res->setStatusCode(drogon::k500InternalServerError);
                            cb(res);
                        },
                        id
                    );
                },
                [cb](const drogon::orm::DrogonDbException &) {
                    Json::Value err; err["error"] = "server error";
                    auto res = drogon::HttpResponse::newHttpJsonResponse(err);
                    res->setStatusCode(drogon::k500InternalServerError);
                    cb(res);
                },
                id
            );
        },
        dbErrCb(cb), id, uid
    );
}

void LobbyController::close(const drogon::HttpRequestPtr &req,
                            std::function<void(const drogon::HttpResponsePtr &)> &&cb,
                            int id) {
    int uid = userId(req);
    auto db = drogon::app().getDbClient();

    db->execSqlAsync(
        "SELECT role FROM lobby_members WHERE lobby_id=? AND user_id=?",
        [cb, db, id](const drogon::orm::Result &r) {
            if (r.empty() || r[0]["role"].as<std::string>() != "host") {
                Json::Value err; err["error"] = "NOT_HOST";
                auto res = drogon::HttpResponse::newHttpJsonResponse(err);
                res->setStatusCode(drogon::k403Forbidden);
                cb(res); return;
            }
            db->execSqlAsync(
                "UPDATE lobbies SET status='closed' WHERE id=?",
                [cb](const drogon::orm::Result &) {
                    Json::Value out; out["message"] = "closed";
                    cb(drogon::HttpResponse::newHttpJsonResponse(out));
                },
                [cb](const drogon::orm::DrogonDbException &) {
                    Json::Value err; err["error"] = "server error";
                    auto res = drogon::HttpResponse::newHttpJsonResponse(err);
                    res->setStatusCode(drogon::k500InternalServerError);
                    cb(res);
                },
                id
            );
        },
        dbErrCb(cb), id, uid
    );
}
