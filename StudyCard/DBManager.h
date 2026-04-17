#pragma once
#include <mysql.h>
#include <vector>

struct FlashCard {
    int     id = 0;
    CString category;
    CString question;
    CString answer;
    CString studyDate;
    bool    known = false;
};

class DBManager {
public:
    static DBManager& Get();

    bool Connect(LPCSTR host, unsigned int port,
                 LPCSTR user, LPCSTR pass, LPCSTR db);
    void Disconnect();
    bool IsConnected() const { return m_pConn != nullptr; }

    bool AddCard(const FlashCard& card, CString& err);
    bool DeleteCard(int id, CString& err);
    std::vector<FlashCard> GetCards(const CString& category = _T(""));
    std::vector<FlashCard> GetCardsByDate(const CString& date);
    int GetTotalCount();

    int ImportCSV(const CString& filePath, CString& err);

private:
    DBManager();
    ~DBManager();
    DBManager(const DBManager&) = delete;
    DBManager& operator=(const DBManager&) = delete;

    MYSQL* m_pConn;

    static CStringA ToUtf8(const CString& s);
    static CString FromUtf8(const char* s);
    CStringA Escape(const CString& s);
    bool ExecuteNonQuery(const CStringA& sql, CString& err);
    MYSQL_RES* ExecuteQuery(const CStringA& sql, CString& err);
    void ClearPendingResults();

    static CString DecodeBytes(const std::string& raw, UINT codepage);
    static std::vector<CString> SplitTab(const CString& line);
};
