#include "LobbyWebSocket.h"
#include <drogon/drogon.h>
#include <jwt-cpp/jwt.h>
#include <json/json.h>

static Json::Value parseJson(const std::string &s) {
    Json::Value root;
    Json::Reader reader;
    reader.parse(s, root);
    return root;
}

static std::string toStr(const Json::Value &v) {
    Json::FastWriter w;
    return w.write(v);
}

void LobbyWebSocket::handleNewConnection(const drogon::HttpRequestPtr &req,
                                         const drogon::WebSocketConnectionPtr &conn) {
    auto lobbyIdStr = req->getParameter("lobbyId");
    int lobbyId = lobbyIdStr.empty() ? 0 : std::stoi(lobbyIdStr);

    auto token = req->getParameter("token");
    if (token.empty()) {
        conn->shutdown(drogon::CloseCode::kViolation, "token required");
        return;
    }

    try {
        auto secret = drogon::app().getCustomConfig()["jwt_secret"].asString();
        auto decoded = jwt::decode(token);
        jwt::verify().allow_algorithm(jwt::algorithm::hs256{secret}).verify(decoded);

        int uid = std::stoi(decoded.get_payload_claim("id").as_string());
        std::string nick = "";

        std::lock_guard<std::mutex> lock(mutex_);
        auto key = conn->peerAddr().toIpPort();
        clients_[key] = {uid, nick, lobbyId, conn};
        lobbyConnections_[lobbyId].insert(key);

        Json::Value joined;
        joined["event"] = "member/joined";
        joined["payload"]["userId"]   = uid;
        joined["payload"]["nickname"] = nick;
        joined["payload"]["role"]     = "member";
        broadcast(lobbyId, joined, conn);

    } catch (...) {
        conn->shutdown(drogon::CloseCode::kViolation, "invalid token");
    }
}

void LobbyWebSocket::handleNewMessage(const drogon::WebSocketConnectionPtr &conn,
                                      std::string &&msg,
                                      const drogon::WebSocketMessageType &type) {
    if (type != drogon::WebSocketMessageType::Text) return;

    auto key = conn->peerAddr().toIpPort();
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = clients_.find(key);
    if (it == clients_.end()) return;

    auto &client  = it->second;
    auto  payload = parseJson(msg);
    auto  event   = payload["event"].asString();

    if (event == "chat/send") {
        Json::Value out;
        out["event"]              = "chat/message";
        out["payload"]["userId"]  = client.userId;
        out["payload"]["nickname"]= client.nickname;
        out["payload"]["message"] = payload["payload"]["message"].asString();
        broadcast(client.lobbyId, out);

    } else if (event == "lobby/set_category") {
        auto category = payload["payload"]["category"].asString();
        Json::Value out;
        out["event"]               = "lobby/category_changed";
        out["payload"]["category"] = category;
        broadcast(client.lobbyId, out);

    } else if (event == "lobby/start") {
        auto db     = drogon::app().getDbClient();
        int lobbyId = client.lobbyId;
        int hostId  = client.userId;
        db->execSqlAsync(
            "SELECT role FROM lobby_members WHERE lobby_id=? AND user_id=?",
            [this, db, lobbyId, conn](const drogon::orm::Result &r) {
                if (r.empty() || r[0]["role"].as<std::string>() != "host") {
                    Json::Value err;
                    err["event"]              = "error";
                    err["payload"]["message"] = "권한이 없습니다.";
                    sendTo(conn, err);
                    return;
                }
                db->execSqlAsync(
                    "UPDATE lobbies SET status='in_progress' WHERE id=? AND status='waiting'",
                    [this, db, lobbyId](const drogon::orm::Result &) {
                        db->execSqlAsync(
                            "SELECT category FROM lobbies WHERE id=?",
                            [this, lobbyId](const drogon::orm::Result &r2) {
                                Json::Value out;
                                out["event"]               = "lobby/started";
                                out["payload"]["category"] = r2[0]["category"].as<std::string>();
                                out["payload"]["cardCount"] = 10;
                                std::lock_guard<std::mutex> lock(mutex_);
                                broadcast(lobbyId, out);
                            },
                            [](const drogon::orm::DrogonDbException &) {},
                            lobbyId
                        );
                    },
                    [](const drogon::orm::DrogonDbException &) {},
                    lobbyId
                );
            },
            [this, conn](const drogon::orm::DrogonDbException &) {
                Json::Value err;
                err["event"]              = "error";
                err["payload"]["message"] = "서버 오류";
                sendTo(conn, err);
            },
            lobbyId, hostId
        );

    } else if (event == "lobby/save_message") {
        auto text = payload["payload"]["message"].asString();
        auto db   = drogon::app().getDbClient();
        db->execSqlAsync(
            "INSERT INTO lobby_messages (lobby_id,user_id,message) VALUES (?,?,?)",
            [](const drogon::orm::Result &) {},
            [](const drogon::orm::DrogonDbException &) {},
            client.lobbyId, client.userId, text
        );
    }
}

void LobbyWebSocket::handleConnectionClosed(const drogon::WebSocketConnectionPtr &conn) {
    auto key = conn->peerAddr().toIpPort();
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = clients_.find(key);
    if (it == clients_.end()) return;

    auto &client = it->second;
    Json::Value left;
    left["event"]              = "member/left";
    left["payload"]["userId"]  = client.userId;
    left["payload"]["nickname"]= client.nickname;

    int lobbyId = client.lobbyId;
    lobbyConnections_[lobbyId].erase(key);
    clients_.erase(it);
    broadcast(lobbyId, left);

    auto db = drogon::app().getDbClient();
    db->execSqlAsync(
        "DELETE FROM lobby_members WHERE lobby_id=? AND user_id=?",
        [](const drogon::orm::Result &) {},
        [](const drogon::orm::DrogonDbException &) {},
        lobbyId, client.userId
    );
}

void LobbyWebSocket::broadcast(int lobbyId, const Json::Value &msg,
                               const drogon::WebSocketConnectionPtr &exclude) {
    auto it = lobbyConnections_.find(lobbyId);
    if (it == lobbyConnections_.end()) return;
    auto str = toStr(msg);
    for (auto &key : it->second) {
        auto cit = clients_.find(key);
        if (cit == clients_.end()) continue;
        if (exclude && cit->second.conn == exclude) continue;
        cit->second.conn->send(str);
    }
}

void LobbyWebSocket::sendTo(const drogon::WebSocketConnectionPtr &conn, const Json::Value &msg) {
    conn->send(toStr(msg));
}
