#include "stdafx.h"
#include "DBManager.h"
#include <fstream>
#include <sstream>

// ── helpers ──────────────────────────────────────────────────────────────────

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
    unsigned long rawLen = (unsigned long)raw.GetLength();
    char* p = buf.GetBuffer(rawLen * 2 + 1);
    mysql_real_escape_string(m_pConn, p, raw, rawLen);
    buf.ReleaseBuffer();
    return buf;
}

// CP949/UTF-8 raw bytes → CString(UTF-16)
CString DBManager::DecodeBytes(const std::string& raw, UINT codepage) {
    if (raw.empty()) return _T("");
    int needed = MultiByteToWideChar(codepage, 0,
                                     raw.c_str(), (int)raw.size(),
                                     nullptr, 0);
    if (needed <= 0) return _T("");
    CString result;
    wchar_t* p = result.GetBuffer(needed);
    MultiByteToWideChar(codepage, 0, raw.c_str(), (int)raw.size(), p, needed);
    result.ReleaseBuffer(needed);
    return result;
}

// TSV 한 줄 → 필드 벡터 (따옴표 처리 포함)
std::vector<CString> DBManager::SplitTab(const CString& line) {
    std::vector<CString> fields;
    CString cur;
    bool inQuote = false;
    for (int i = 0; i < line.GetLength(); i++) {
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

// ── singleton ─────────────────────────────────────────────────────────────────

DBManager& DBManager::Get() {
    static DBManager inst;
    return inst;
}

DBManager::DBManager()  : m_pConn(nullptr) {}
DBManager::~DBManager() { Disconnect(); }

// ── connection ────────────────────────────────────────────────────────────────

bool DBManager::Connect(LPCSTR host, unsigned int port,
                        LPCSTR user, LPCSTR pass, LPCSTR db) {
    Disconnect();
    m_pConn = mysql_init(nullptr);
    if (!m_pConn) return false;
    my_bool r = 1;
    mysql_options(m_pConn, MYSQL_OPT_RECONNECT, &r);
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
    if (m_pConn) { mysql_close(m_pConn); m_pConn = nullptr; }
}

// ── CRUD ──────────────────────────────────────────────────────────────────────

bool DBManager::AddCard(const FlashCard& card, CString& err) {
    if (!m_pConn) { err = _T("DB 미연결"); return false; }

    CStringA eCat  = Escape(card.category);
    CStringA eQ    = Escape(card.question);
    CStringA eA    = Escape(card.answer);
    CStringA eDate = Escape(card.studyDate);

    CStringA sql;
    sql.Format(
        "INSERT INTO flash_cards (category,question,answer,study_date)"
        " VALUES('%s','%s','%s','%s')",
        (LPCSTR)eCat, (LPCSTR)eQ, (LPCSTR)eA, (LPCSTR)eDate);

    if (mysql_query(m_pConn, sql) != 0) {
        err = FromUtf8(mysql_error(m_pConn));
        return false;
    }
    return true;
}

bool DBManager::DeleteCard(int id, CString& err) {
    if (!m_pConn) { err = _T("DB 미연결"); return false; }
    CStringA sql;
    sql.Format("DELETE FROM flash_cards WHERE id=%d", id);
    if (mysql_query(m_pConn, sql) != 0) {
        err = FromUtf8(mysql_error(m_pConn));
        return false;
    }
    return true;
}

std::vector<FlashCard> DBManager::GetCards(const CString& category) {
    std::vector<FlashCard> cards;
    if (!m_pConn) return cards;

    CStringA sql =
        "SELECT id,category,question,answer,DATE_FORMAT(study_date,'%Y-%m-%d')"
        " FROM flash_cards";
    if (!category.IsEmpty() && category != _T("전체")) {
        sql += " WHERE category='";
        sql += Escape(category);
        sql += "'";
    }
    sql += " ORDER BY CASE category"
           " WHEN 'CS' THEN 1 WHEN 'C++' THEN 2 ELSE 3 END, id";

    if (mysql_query(m_pConn, sql) != 0) return cards;
    MYSQL_RES* res = mysql_store_result(m_pConn);
    if (!res) return cards;

    MYSQL_ROW row;
    while ((row = mysql_fetch_row(res))) {
        FlashCard c;
        c.id        = row[0] ? atoi(row[0]) : 0;
        c.category  = FromUtf8(row[1]);
        c.question  = FromUtf8(row[2]);
        c.answer    = FromUtf8(row[3]);
        c.studyDate = FromUtf8(row[4]);
        cards.push_back(c);
    }
    mysql_free_result(res);
    return cards;
}

std::vector<FlashCard> DBManager::GetCardsByDate(const CString& date) {
    std::vector<FlashCard> cards;
    if (!m_pConn) return cards;
    CStringA eDate = Escape(date);
    CStringA sql;
    sql.Format(
        "SELECT id,category,question,answer,DATE_FORMAT(study_date,'%%Y-%%m-%%d')"
        " FROM flash_cards WHERE study_date='%s'"
        " ORDER BY CASE category WHEN 'CS' THEN 1 WHEN 'C++' THEN 2 ELSE 3 END",
        (LPCSTR)eDate);
    if (mysql_query(m_pConn, sql) != 0) return cards;
    MYSQL_RES* res = mysql_store_result(m_pConn);
    if (!res) return cards;
    MYSQL_ROW row;
    while ((row = mysql_fetch_row(res))) {
        FlashCard c;
        c.id        = row[0] ? atoi(row[0]) : 0;
        c.category  = FromUtf8(row[1]);
        c.question  = FromUtf8(row[2]);
        c.answer    = FromUtf8(row[3]);
        c.studyDate = FromUtf8(row[4]);
        cards.push_back(c);
    }
    mysql_free_result(res);
    return cards;
}

int DBManager::GetTotalCount() {
    if (!m_pConn) return 0;
    if (mysql_query(m_pConn, "SELECT COUNT(*) FROM flash_cards") != 0) return 0;
    MYSQL_RES* res = mysql_store_result(m_pConn);
    if (!res) return 0;
    MYSQL_ROW row = mysql_fetch_row(res);
    int cnt = (row && row[0]) ? atoi(row[0]) : 0;
    mysql_free_result(res);
    return cnt;
}

// ── CSV 가져오기 ──────────────────────────────────────────────────────────────

int DBManager::ImportCSV(const CString& filePath, CString& err) {
    // 파일 원시 바이트 읽기
    CT2CA pathA(filePath, CP_ACP);
    std::ifstream ifs(pathA, std::ios::binary);
    if (!ifs) { err = _T("파일을 열 수 없습니다."); return -1; }

    std::string rawContent((std::istreambuf_iterator<char>(ifs)),
                            std::istreambuf_iterator<char>());
    ifs.close();

    // BOM 감지 → 인코딩 결정
    UINT codepage = 949;   // 기본: CP949 (한글 Windows)
    size_t startPos = 0;
    if (rawContent.size() >= 3 &&
        (unsigned char)rawContent[0] == 0xEF &&
        (unsigned char)rawContent[1] == 0xBB &&
        (unsigned char)rawContent[2] == 0xBF) {
        codepage = CP_UTF8;
        startPos = 3;
    }

    // 전체를 UTF-16 CString으로 변환
    std::string body = rawContent.substr(startPos);
    CString content  = DecodeBytes(body, codepage);

    // 줄 단위 파싱
    int imported = 0;
    bool firstLine = true;
    int pos = 0;

    while (pos <= content.GetLength()) {
        int nlPos = content.Find(_T('\n'), pos);
        CString line;
        if (nlPos == -1) {
            line = content.Mid(pos);
            pos  = content.GetLength() + 1;
        } else {
            line = content.Mid(pos, nlPos - pos);
            pos  = nlPos + 1;
        }
        line.TrimRight(_T("\r\n "));
        if (line.IsEmpty()) continue;

        if (firstLine) {   // 헤더 행 건너뜀
            firstLine = false;
            continue;
        }

        auto fields = SplitTab(line);
        if (fields.size() < 3) continue;

        CString cat  = fields[0]; cat.Trim();
        CString q    = fields[1]; q.Trim();
        CString a    = fields[2]; a.Trim();
        if (cat.IsEmpty() || q.IsEmpty()) continue;

        FlashCard card;
        card.category  = cat;
        card.question  = q;
        card.answer    = a;
        CTime today    = CTime::GetCurrentTime();
        card.studyDate.Format(_T("%04d-%02d-%02d"),
                              today.GetYear(), today.GetMonth(), today.GetDay());

        CString cardErr;
        if (AddCard(card, cardErr))
            ++imported;
    }

    if (imported == 0)
        err = _T("가져온 카드가 없습니다. 파일 형식을 확인하세요.");
    return imported;
}
