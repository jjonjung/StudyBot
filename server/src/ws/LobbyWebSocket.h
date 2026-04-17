#pragma once
#include <drogon/WebSocketController.h>
#include <unordered_map>
#include <set>
#include <mutex>

struct LobbyClient {
    int userId;
    std::string nickname;
    int lobbyId;
    drogon::WebSocketConnectionPtr conn;
};

class LobbyWebSocket : public drogon::WebSocketController<LobbyWebSocket> {
public:
    WS_PATH_LIST_BEGIN
        WS_PATH_ADD("/ws/lobby");
    WS_PATH_LIST_END

    void handleNewMessage(const drogon::WebSocketConnectionPtr &conn,
                          std::string &&msg,
                          const drogon::WebSocketMessageType &type) override;
    void handleNewConnection(const drogon::HttpRequestPtr &req,
                             const drogon::WebSocketConnectionPtr &conn) override;
    void handleConnectionClosed(const drogon::WebSocketConnectionPtr &conn) override;

private:
    std::mutex mutex_;
    std::unordered_map<std::string, LobbyClient> clients_;
    std::unordered_map<int, std::set<std::string>> lobbyConnections_;

    void broadcast(int lobbyId, const Json::Value &msg,
                   const drogon::WebSocketConnectionPtr &exclude = nullptr);
    void sendTo(const drogon::WebSocketConnectionPtr &conn, const Json::Value &msg);
};
