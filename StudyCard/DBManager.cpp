#include "stdafx.h"
#include "DBManager.h"
#include <fstream>
#include <sstream>

CStringA DBManager::ToUtf8(const CString& s) {
    CT2CA utf8(s, CP_UTF8);
    return CStringA(utf8);
}

CString DBManager::FromUtf8(const char* s) {
    if (!s) return _T("");
    CA2CT wide(s, CP_UTF8);
    return CString(wide);
}

CStringA DBManager::Escape(const CString& s) {
    if (!m_pConn) return ToUtf8(s);
    CStringA raw = ToUtf8(s);
    CStringA buf;
    unsigned long rawLen = static_cast<unsigned long>(raw.GetLength());
    char* p = buf.GetBuffer(rawLen * 2 + 1);
    mysql_real_escape_string(m_pConn, p, raw, rawLen);
    buf.ReleaseBuffer();
    return buf;
}

bool DBManager::ExecuteNonQuery(const CStringA& sql, CString& err) {
    if (mysql_query(m_pConn, sql) != 0) {
        err = FromUtf8(mysql_error(m_pConn));
        ClearPendingResults();
        return false;
    }
    MYSQL_RES* res = mysql_store_result(m_pConn);
    if (res) mysql_free_result(res);
    ClearPendingResults();
    return true;
}

MYSQL_RES* DBManager::ExecuteQuery(const CStringA& sql, CString& err) {
    if (mysql_query(m_pConn, sql) != 0) {
        err = FromUtf8(mysql_error(m_pConn));
        ClearPendingResults();
        return nullptr;
    }
    return mysql_store_result(m_pConn);
}

void DBManager::ClearPendingResults() {
    while (mysql_next_result(m_pConn) == 0) {
        MYSQL_RES* extra = mysql_store_result(m_pConn);
        if (extra) mysql_free_result(extra);
    }
}

CString DBManager::DecodeBytes(const std::string& raw, UINT codepage) {
    if (raw.empty()) return _T("");
    int needed = MultiByteToWideChar(codepage, 0, raw.c_str(), static_cast<int>(raw.size()), nullptr, 0);
    if (needed <= 0) return _T("");
    CString result;
    wchar_t* p = result.GetBuffer(needed);
    MultiByteToWideChar(codepage, 0, raw.c_str(), static_cast<int>(raw.size()), p, needed);
    result.ReleaseBuffer(needed);
    return result;
}

std::vector<CString> DBManager::SplitTab(const CString& line) {
    std::vector<CString> fields;
    CString cur;
    bool inQuote = false;
    for (int i = 0; i < line.GetLength(); ++i) {
        TCHAR c = line[i];
        if (c == _T('"')) {
            inQuote = !inQuote;
        } else if (c == _T('\t') && !inQuote) {
            fields.push_back(cur);
            cur.Empty();
        } else {
            cur += c;
        }
    }
    fields.push_back(cur);
    return fields;
}

DBManager& DBManager::Get() {
    static DBManager inst;
    return inst;
}

DBManager::DBManager() : m_pConn(nullptr) {}
DBManager::~DBManager() { Disconnect(); }

bool DBManager::Connect(LPCSTR host, unsigned int port,
                        LPCSTR user, LPCSTR pass, LPCSTR db) {
    Disconnect();
    m_pConn = mysql_init(nullptr);
    if (!m_pConn) return false;
    my_bool reconnect = 1;
    mysql_options(m_pConn, MYSQL_OPT_RECONNECT, &reconnect);
    mysql_options(m_pConn, MYSQL_SET_CHARSET_NAME, "utf8mb4");
    if (!mysql_real_connect(m_pConn, host, user, pass, db, port, nullptr, 0)) {
        mysql_close(m_pConn);
        m_pConn = nullptr;
        return false;
    }
    mysql_set_character_set(m_pConn, "utf8mb4");
    return true;
}

void DBManager::Disconnect() {
    if (m_pConn) {
        mysql_close(m_pConn);
        m_pConn = nullptr;
    }
}

bool DBManager::AddCard(const FlashCard& card, CString& err) {
    if (!m_pConn) {
        err = _T("DB not connected");
        return false;
    }

    CStringA sql;
    sql.Format(
        "CALL sp_add_flash_card('%s','%s','%s','%s')",
        static_cast<LPCSTR>(Escape(card.category)),
        static_cast<LPCSTR>(Escape(card.question)),
        static_cast<LPCSTR>(Escape(card.answer)),
        static_cast<LPCSTR>(Escape(card.studyDate))
    );
    return ExecuteNonQuery(sql, err);
}

bool DBManager::DeleteCard(int id, CString& err) {
    if (!m_pConn) {
        err = _T("DB not connected");
        return false;
    }

    CStringA sql;
    sql.Format("CALL sp_delete_flash_card(%d)", id);
    return ExecuteNonQuery(sql, err);
}

std::vector<FlashCard> DBManager::GetCards(const CString& category) {
    std::vector<FlashCard> cards;
    if (!m_pConn) return cards;

    CStringA sql;
    if (!category.IsEmpty() && category != _T("All")) {
        sql.Format("CALL sp_get_flash_cards('%s')", static_cast<LPCSTR>(Escape(category)));
    } else {
        sql = "CALL sp_get_flash_cards(NULL)";
    }

    CString err;
    MYSQL_RES* res = ExecuteQuery(sql, err);
    if (!res) return cards;

    MYSQL_ROW row;
    while ((row = mysql_fetch_row(res))) {
        FlashCard c;
        c.id = row[0] ? atoi(row[0]) : 0;
        c.category = FromUtf8(row[1]);
        c.question = FromUtf8(row[2]);
        c.answer = FromUtf8(row[3]);
        c.studyDate = FromUtf8(row[4]);
        cards.push_back(c);
    }

    mysql_free_result(res);
    ClearPendingResults();
    return cards;
}

std::vector<FlashCard> DBManager::GetCardsByDate(const CString& date) {
    std::vector<FlashCard> cards;
    if (!m_pConn) return cards;

    CStringA sql;
    sql.Format("CALL sp_get_flash_cards_by_date('%s')", static_cast<LPCSTR>(Escape(date)));

    CString err;
    MYSQL_RES* res = ExecuteQuery(sql, err);
    if (!res) return cards;

    MYSQL_ROW row;
    while ((row = mysql_fetch_row(res))) {
        FlashCard c;
        c.id = row[0] ? atoi(row[0]) : 0;
        c.category = FromUtf8(row[1]);
        c.question = FromUtf8(row[2]);
        c.answer = FromUtf8(row[3]);
        c.studyDate = FromUtf8(row[4]);
        cards.push_back(c);
    }

    mysql_free_result(res);
    ClearPendingResults();
    return cards;
}

int DBManager::GetTotalCount() {
    if (!m_pConn) return 0;

    CString err;
    MYSQL_RES* res = ExecuteQuery("CALL sp_get_flash_card_total_count()", err);
    if (!res) return 0;

    MYSQL_ROW row = mysql_fetch_row(res);
    const int count = (row && row[0]) ? atoi(row[0]) : 0;
    mysql_free_result(res);
    ClearPendingResults();
    return count;
}

int DBManager::ImportCSV(const CString& filePath, CString& err) {
    CT2CA pathA(filePath, CP_ACP);
    std::ifstream ifs(pathA, std::ios::binary);
    if (!ifs) {
        err = _T("Failed to open file");
        return -1;
    }

    std::string rawContent((std::istreambuf_iterator<char>(ifs)),
                           std::istreambuf_iterator<char>());
    ifs.close();

    UINT codepage = 949;
    size_t startPos = 0;
    if (rawContent.size() >= 3 &&
        static_cast<unsigned char>(rawContent[0]) == 0xEF &&
        static_cast<unsigned char>(rawContent[1]) == 0xBB &&
        static_cast<unsigned char>(rawContent[2]) == 0xBF) {
        codepage = CP_UTF8;
        startPos = 3;
    }

    CString content = DecodeBytes(rawContent.substr(startPos), codepage);
    int imported = 0;
    bool firstLine = true;
    int pos = 0;

    while (pos <= content.GetLength()) {
        int nlPos = content.Find(_T('\n'), pos);
        CString line;
        if (nlPos == -1) {
            line = content.Mid(pos);
            pos = content.GetLength() + 1;
        } else {
            line = content.Mid(pos, nlPos - pos);
            pos = nlPos + 1;
        }

        line.TrimRight(_T("\r\n "));
        if (line.IsEmpty()) continue;

        if (firstLine) {
            firstLine = false;
            continue;
        }

        auto fields = SplitTab(line);
        if (fields.size() < 3) continue;

        CString cat = fields[0];
        CString q = fields[1];
        CString a = fields[2];
        cat.Trim();
        q.Trim();
        a.Trim();
        if (cat.IsEmpty() || q.IsEmpty()) continue;

        FlashCard card;
        card.category = cat;
        card.question = q;
        card.answer = a;
        CTime today = CTime::GetCurrentTime();
        card.studyDate.Format(_T("%04d-%02d-%02d"),
                              today.GetYear(), today.GetMonth(), today.GetDay());

        CString cardErr;
        if (AddCard(card, cardErr)) {
            ++imported;
        }
    }

    if (imported == 0) {
        err = _T("No valid rows were imported");
    }
    return imported;
}
