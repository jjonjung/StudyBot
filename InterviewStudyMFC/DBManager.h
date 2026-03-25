#pragma once
#include <mysql.h>
#include <vector>

// MySQL C API 연결 정보 + 에러 코드
#define MYSQL_ERR_DUP_ENTRY 1062

struct Question {
    int     id;
    CString date;
    CString member;
    CString category;
    CString question;
    CString createdAt;   // "HH:MM" format
};

// MySQL 접근 싱글톤
class DBManager {
public:
    static DBManager& Get();

    bool Connect(LPCSTR host, unsigned int port,
                 LPCSTR user, LPCSTR password, LPCSTR dbName);
    void Disconnect();
    bool IsConnected() const { return m_pConn != nullptr; }

    std::vector<CString>   GetDates();
    std::vector<Question>  GetQuestions(const CString& date);

    bool AddQuestion(const CString& date, const CString& member,
                     const CString& category, const CString& question,
                     CString& outError);

    bool DeleteQuestion(int id, CString& outError);

private:
    DBManager();
    ~DBManager();
    DBManager(const DBManager&) = delete;
    DBManager& operator=(const DBManager&) = delete;

    MYSQL*   m_pConn;

    // UTF-16 CString -> UTF-8 CStringA
    static CStringA ToUtf8(const CString& s);
    // UTF-8 char* -> UTF-16 CString
    static CString  FromUtf8(const char* s);

    CStringA Escape(const CString& s);
};
