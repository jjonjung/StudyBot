#pragma once
#include <mysql.h>
#include <vector>

struct FlashCard {
    int     id        = 0;
    CString category;
    CString question;
    CString answer;
    CString studyDate;
    bool    known     = false;   // 세션 내 플래그 (DB 미저장)
};

class DBManager {
public:
    static DBManager& Get();

    bool Connect(LPCSTR host, unsigned int port,
                 LPCSTR user, LPCSTR pass, LPCSTR db);
    void Disconnect();
    bool IsConnected() const { return m_pConn != nullptr; }

    // 카드 CRUD
    bool AddCard(const FlashCard& card, CString& err);
    bool DeleteCard(int id, CString& err);
    std::vector<FlashCard> GetCards(const CString& category = _T(""));
    std::vector<FlashCard> GetCardsByDate(const CString& date);
    int  GetTotalCount();

    // CSV 일괄 가져오기 (CP949 또는 UTF-8 자동 감지)
    int  ImportCSV(const CString& filePath, CString& err);

private:
    DBManager();
    ~DBManager();
    DBManager(const DBManager&) = delete;
    DBManager& operator=(const DBManager&) = delete;

    MYSQL* m_pConn;

    static CStringA ToUtf8(const CString& s);
    static CString  FromUtf8(const char* s);
    CStringA        Escape(const CString& s);

    // CSV 파싱 helpers
    static CString  DecodeBytes(const std::string& raw, UINT codepage);
    static std::vector<CString> SplitTab(const CString& line);
};
