CREATE DATABASE IF NOT EXISTS studybot
  CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci;

USE studybot;

CREATE TABLE IF NOT EXISTS users (
    id            INT AUTO_INCREMENT PRIMARY KEY,
    username      VARCHAR(50)  NULL,
    password_hash VARCHAR(255) NULL,
    nickname      VARCHAR(50)  NOT NULL DEFAULT '',
    google_id     VARCHAR(100) NULL UNIQUE,
    email         VARCHAR(255) NULL,
    avatar_url    VARCHAR(500) NULL,
    created_at    TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    INDEX idx_username (username),
    INDEX idx_google_id (google_id)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS flashcards (
    id               INT AUTO_INCREMENT PRIMARY KEY,
    category         ENUM('Unreal','C++','CS','Company','Algorithm') NOT NULL,
    company          VARCHAR(50) NULL,
    question         TEXT NOT NULL,
    answer           TEXT NOT NULL,
    difficulty       ENUM('Easy','Normal','Hard') NOT NULL DEFAULT 'Normal',
    core_conditions  TEXT NULL,
    selection_reason TEXT NULL,
    code_cpp         MEDIUMTEXT NULL,
    code_csharp      MEDIUMTEXT NULL,
    time_complexity  VARCHAR(100) NULL,
    created_at       TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    INDEX idx_category (category),
    INDEX idx_difficulty (difficulty)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS user_progress (
    id         INT AUTO_INCREMENT PRIMARY KEY,
    user_id    INT NOT NULL,
    card_id    INT NOT NULL,
    known      TINYINT(1) NOT NULL DEFAULT 0,
    score      TINYINT NOT NULL DEFAULT 0,
    studied_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
    FOREIGN KEY (user_id) REFERENCES users(id) ON DELETE CASCADE,
    FOREIGN KEY (card_id) REFERENCES flashcards(id) ON DELETE CASCADE,
    UNIQUE KEY uq_user_card (user_id, card_id),
    INDEX idx_user (user_id)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS interview_sessions (
    id          INT AUTO_INCREMENT PRIMARY KEY,
    user_id     INT NOT NULL,
    category    ENUM('Unreal','C++','CS','Company','Algorithm','Mixed') NOT NULL DEFAULT 'Mixed',
    total_cards INT NOT NULL DEFAULT 0,
    known_count INT NOT NULL DEFAULT 0,
    played_at   TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY (user_id) REFERENCES users(id) ON DELETE CASCADE,
    INDEX idx_user_session (user_id, played_at)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS daily_scores (
    id          INT AUTO_INCREMENT PRIMARY KEY,
    user_id     INT NOT NULL,
    category    ENUM('Unreal','C++','CS','Company','Algorithm','Mixed') NOT NULL,
    score_date  DATE NOT NULL,
    cards_done  INT NOT NULL DEFAULT 0,
    known_count INT NOT NULL DEFAULT 0,
    FOREIGN KEY (user_id) REFERENCES users(id) ON DELETE CASCADE,
    UNIQUE KEY uq_user_cat_date (user_id, category, score_date),
    INDEX idx_user_date (user_id, score_date)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS oauth_pending (
    state      VARCHAR(32) PRIMARY KEY,
    token      VARCHAR(1024) NOT NULL,
    nickname   VARCHAR(50) NOT NULL,
    user_id    INT NOT NULL,
    expires_at DATETIME NOT NULL,
    INDEX idx_expires (expires_at)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

DELIMITER //

DROP PROCEDURE IF EXISTS sp_auth_upsert_google_user //
CREATE PROCEDURE sp_auth_upsert_google_user(
    IN p_google_id VARCHAR(100),
    IN p_email VARCHAR(255),
    IN p_nickname VARCHAR(50),
    IN p_avatar_url VARCHAR(500)
)
BEGIN
    DECLARE v_user_id INT DEFAULT NULL;

    SELECT id INTO v_user_id
      FROM users
     WHERE google_id = p_google_id
     LIMIT 1;

    IF v_user_id IS NULL THEN
        INSERT INTO users (google_id, email, nickname, avatar_url)
        VALUES (p_google_id, p_email, p_nickname, p_avatar_url);
        SET v_user_id = LAST_INSERT_ID();
    END IF;

    SELECT id, google_id, nickname
      FROM users
     WHERE id = v_user_id;
END //

DROP PROCEDURE IF EXISTS sp_auth_create_user //
CREATE PROCEDURE sp_auth_create_user(
    IN p_username VARCHAR(50),
    IN p_password_hash VARCHAR(255),
    IN p_nickname VARCHAR(50)
)
BEGIN
    INSERT INTO users (username, password_hash, nickname)
    VALUES (p_username, p_password_hash, p_nickname);

    SELECT LAST_INSERT_ID() AS insert_id;
END //

DROP PROCEDURE IF EXISTS sp_auth_get_login_user //
CREATE PROCEDURE sp_auth_get_login_user(
    IN p_username VARCHAR(50)
)
BEGIN
    SELECT id, username, password_hash, nickname
      FROM users
     WHERE username = p_username
     LIMIT 1;
END //

DROP PROCEDURE IF EXISTS sp_auth_upsert_oauth_pending //
CREATE PROCEDURE sp_auth_upsert_oauth_pending(
    IN p_state VARCHAR(32),
    IN p_token VARCHAR(1024),
    IN p_nickname VARCHAR(50),
    IN p_user_id INT,
    IN p_expires_at DATETIME
)
BEGIN
    INSERT INTO oauth_pending (state, token, nickname, user_id, expires_at)
    VALUES (p_state, p_token, p_nickname, p_user_id, p_expires_at)
    ON DUPLICATE KEY UPDATE
        token = VALUES(token),
        nickname = VALUES(nickname),
        user_id = VALUES(user_id),
        expires_at = VALUES(expires_at);
END //

DROP PROCEDURE IF EXISTS sp_auth_get_valid_oauth_pending //
CREATE PROCEDURE sp_auth_get_valid_oauth_pending(
    IN p_state VARCHAR(32)
)
BEGIN
    SELECT token, nickname, user_id
      FROM oauth_pending
     WHERE state = p_state
       AND expires_at > NOW();
END //

DROP PROCEDURE IF EXISTS sp_auth_delete_oauth_pending //
CREATE PROCEDURE sp_auth_delete_oauth_pending(
    IN p_state VARCHAR(32)
)
BEGIN
    DELETE FROM oauth_pending
     WHERE state = p_state;
END //

DROP PROCEDURE IF EXISTS sp_auth_delete_expired_oauth_pending //
CREATE PROCEDURE sp_auth_delete_expired_oauth_pending()
BEGIN
    DELETE FROM oauth_pending
     WHERE expires_at < NOW();
END //

DROP PROCEDURE IF EXISTS sp_cards_list //
CREATE PROCEDURE sp_cards_list(
    IN p_category VARCHAR(20),
    IN p_difficulty VARCHAR(20),
    IN p_company VARCHAR(50),
    IN p_limit INT,
    IN p_offset INT
)
BEGIN
    SELECT id, category, company, question, answer, difficulty,
           core_conditions, selection_reason, code_cpp, code_csharp, time_complexity
      FROM flashcards
     WHERE (p_category IS NULL OR category = p_category)
       AND (p_difficulty IS NULL OR difficulty = p_difficulty)
       AND (p_company IS NULL OR company = p_company)
     ORDER BY id
     LIMIT p_offset, p_limit;
END //

DROP PROCEDURE IF EXISTS sp_cards_interview //
CREATE PROCEDURE sp_cards_interview(
    IN p_category VARCHAR(20),
    IN p_company VARCHAR(50),
    IN p_count INT
)
BEGIN
    SELECT id, category, company, question, answer, difficulty,
           core_conditions, selection_reason, code_cpp, code_csharp, time_complexity
      FROM flashcards
     WHERE (p_category IS NULL OR category = p_category)
       AND (p_company IS NULL OR company = p_company)
     ORDER BY RAND()
     LIMIT p_count;
END //

DROP PROCEDURE IF EXISTS sp_cards_stats //
CREATE PROCEDURE sp_cards_stats()
BEGIN
    SELECT category, COUNT(*) AS total
      FROM flashcards
     GROUP BY category;
END //

DROP PROCEDURE IF EXISTS sp_cards_companies //
CREATE PROCEDURE sp_cards_companies()
BEGIN
    SELECT DISTINCT company
      FROM flashcards
     WHERE category = 'Company'
       AND company IS NOT NULL
     ORDER BY company;
END //

DROP PROCEDURE IF EXISTS sp_cards_get_by_id //
CREATE PROCEDURE sp_cards_get_by_id(
    IN p_id INT
)
BEGIN
    SELECT id, category, company, question, answer, difficulty,
           core_conditions, selection_reason, code_cpp, code_csharp, time_complexity
      FROM flashcards
     WHERE id = p_id;
END //

DROP PROCEDURE IF EXISTS sp_cards_create //
CREATE PROCEDURE sp_cards_create(
    IN p_category VARCHAR(20),
    IN p_company VARCHAR(50),
    IN p_question TEXT,
    IN p_answer TEXT,
    IN p_difficulty VARCHAR(20),
    IN p_core_conditions TEXT,
    IN p_selection_reason TEXT,
    IN p_code_cpp MEDIUMTEXT,
    IN p_code_csharp MEDIUMTEXT,
    IN p_time_complexity VARCHAR(100)
)
BEGIN
    INSERT INTO flashcards (
        category, company, question, answer, difficulty,
        core_conditions, selection_reason, code_cpp, code_csharp, time_complexity
    ) VALUES (
        p_category, p_company, p_question, p_answer, p_difficulty,
        p_core_conditions, p_selection_reason, p_code_cpp, p_code_csharp, p_time_complexity
    );

    SELECT LAST_INSERT_ID() AS insert_id;
END //

DROP PROCEDURE IF EXISTS sp_progress_list //
CREATE PROCEDURE sp_progress_list(
    IN p_user_id INT
)
BEGIN
    SELECT p.card_id, f.category, f.question, p.known, p.score, p.studied_at
      FROM user_progress p
      JOIN flashcards f ON f.id = p.card_id
     WHERE p.user_id = p_user_id
     ORDER BY p.studied_at DESC;
END //

DROP PROCEDURE IF EXISTS sp_progress_summary //
CREATE PROCEDURE sp_progress_summary(
    IN p_user_id INT
)
BEGIN
    SELECT f.category,
           COUNT(*) AS studied,
           SUM(p.known) AS known_count,
           AVG(p.score) AS avg_score
      FROM user_progress p
      JOIN flashcards f ON f.id = p.card_id
     WHERE p.user_id = p_user_id
     GROUP BY f.category;
END //

DROP PROCEDURE IF EXISTS sp_progress_upsert //
CREATE PROCEDURE sp_progress_upsert(
    IN p_user_id INT,
    IN p_card_id INT,
    IN p_known TINYINT,
    IN p_score TINYINT
)
BEGIN
    INSERT INTO user_progress (user_id, card_id, known, score)
    VALUES (p_user_id, p_card_id, p_known, p_score)
    ON DUPLICATE KEY UPDATE
        known = VALUES(known),
        score = VALUES(score),
        studied_at = NOW();
END //

DROP PROCEDURE IF EXISTS sp_progress_create_session //
CREATE PROCEDURE sp_progress_create_session(
    IN p_user_id INT,
    IN p_category VARCHAR(20),
    IN p_total_cards INT,
    IN p_known_count INT
)
BEGIN
    DECLARE v_session_id INT DEFAULT 0;

    DECLARE EXIT HANDLER FOR SQLEXCEPTION
    BEGIN
        ROLLBACK;
        RESIGNAL;
    END;

    START TRANSACTION;

    INSERT INTO interview_sessions (user_id, category, total_cards, known_count)
    VALUES (p_user_id, p_category, p_total_cards, p_known_count);

    SET v_session_id = LAST_INSERT_ID();

    INSERT INTO daily_scores (user_id, category, score_date, cards_done, known_count)
    VALUES (p_user_id, p_category, CURDATE(), p_total_cards, p_known_count)
    ON DUPLICATE KEY UPDATE
        cards_done = cards_done + VALUES(cards_done),
        known_count = known_count + VALUES(known_count);

    COMMIT;

    SELECT v_session_id AS insert_id;
END //

DROP PROCEDURE IF EXISTS sp_progress_sessions //
CREATE PROCEDURE sp_progress_sessions(
    IN p_user_id INT,
    IN p_limit INT
)
BEGIN
    SELECT *
      FROM interview_sessions
     WHERE user_id = p_user_id
     ORDER BY played_at DESC
     LIMIT p_limit;
END //

DROP PROCEDURE IF EXISTS sp_progress_heatmap //
CREATE PROCEDURE sp_progress_heatmap(
    IN p_user_id INT,
    IN p_year INT
)
BEGIN
    SELECT score_date,
           category,
           cards_done,
           known_count,
           ROUND(known_count / GREATEST(cards_done, 1), 4) AS ratio
      FROM daily_scores
     WHERE user_id = p_user_id
       AND YEAR(score_date) = p_year
     ORDER BY score_date;
END //

DELIMITER ;

-- ══════════════════════════════════════════════════════════════
-- v3 신규: 로비 시스템 테이블
-- ══════════════════════════════════════════════════════════════

CREATE TABLE IF NOT EXISTS lobbies (
    id           INT AUTO_INCREMENT PRIMARY KEY,
    code         VARCHAR(6) NOT NULL UNIQUE,
    name         VARCHAR(100) NOT NULL,
    host_user_id INT NOT NULL,
    category     ENUM('Unreal','C++','CS','Company','Algorithm','Mixed')
                 NOT NULL DEFAULT 'Mixed',
    max_members  TINYINT NOT NULL DEFAULT 6,
    status       ENUM('waiting','in_progress','closed')
                 NOT NULL DEFAULT 'waiting',
    created_at   TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY (host_user_id) REFERENCES users(id) ON DELETE CASCADE,
    INDEX idx_code   (code),
    INDEX idx_status (status)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS lobby_members (
    id         INT AUTO_INCREMENT PRIMARY KEY,
    lobby_id   INT NOT NULL,
    user_id    INT NOT NULL,
    role       ENUM('host','member') NOT NULL DEFAULT 'member',
    is_ready   TINYINT(1) NOT NULL DEFAULT 0,
    joined_at  TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY (lobby_id) REFERENCES lobbies(id) ON DELETE CASCADE,
    FOREIGN KEY (user_id)  REFERENCES users(id)   ON DELETE CASCADE,
    UNIQUE KEY uq_lobby_user (lobby_id, user_id),
    INDEX idx_lobby (lobby_id)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS lobby_invites (
    id         INT AUTO_INCREMENT PRIMARY KEY,
    lobby_id   INT NOT NULL,
    code       VARCHAR(6) NOT NULL UNIQUE,
    created_by INT NOT NULL,
    expires_at DATETIME NOT NULL,
    FOREIGN KEY (lobby_id)   REFERENCES lobbies(id) ON DELETE CASCADE,
    FOREIGN KEY (created_by) REFERENCES users(id)   ON DELETE CASCADE,
    INDEX idx_expires (expires_at)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS lobby_messages (
    id         INT AUTO_INCREMENT PRIMARY KEY,
    lobby_id   INT NOT NULL,
    user_id    INT NOT NULL,
    message    VARCHAR(500) NOT NULL,
    sent_at    TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY (lobby_id) REFERENCES lobbies(id) ON DELETE CASCADE,
    FOREIGN KEY (user_id)  REFERENCES users(id)   ON DELETE CASCADE,
    INDEX idx_lobby_time (lobby_id, sent_at)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

-- ── 로비 Stored Procedures ────────────────────────────────────

DELIMITER //

DROP PROCEDURE IF EXISTS sp_lobby_create //
CREATE PROCEDURE sp_lobby_create(
    IN p_host_id   INT,
    IN p_name      VARCHAR(100),
    IN p_category  VARCHAR(20),
    IN p_max       TINYINT,
    IN p_code      VARCHAR(6)
)
BEGIN
    DECLARE v_lobby_id INT;

    INSERT INTO lobbies (name, host_user_id, category, max_members, code)
    VALUES (p_name, p_host_id, p_category, p_max, p_code);

    SET v_lobby_id = LAST_INSERT_ID();

    INSERT INTO lobby_members (lobby_id, user_id, role)
    VALUES (v_lobby_id, p_host_id, 'host');

    SELECT v_lobby_id AS lobby_id, p_code AS code;
END //

DROP PROCEDURE IF EXISTS sp_lobby_join //
CREATE PROCEDURE sp_lobby_join(
    IN p_user_id INT,
    IN p_code    VARCHAR(6)
)
BEGIN
    DECLARE v_lobby_id  INT;
    DECLARE v_max       TINYINT;
    DECLARE v_cur_count INT;
    DECLARE v_status    VARCHAR(20);

    SELECT id, max_members, status
      INTO v_lobby_id, v_max, v_status
      FROM lobbies
     WHERE code = p_code
     LIMIT 1;

    IF v_lobby_id IS NULL THEN
        SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT = 'LOBBY_NOT_FOUND';
    END IF;

    IF v_status != 'waiting' THEN
        SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT = 'LOBBY_NOT_WAITING';
    END IF;

    SELECT COUNT(*) INTO v_cur_count
      FROM lobby_members
     WHERE lobby_id = v_lobby_id;

    IF v_cur_count >= v_max THEN
        SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT = 'LOBBY_FULL';
    END IF;

    INSERT IGNORE INTO lobby_members (lobby_id, user_id, role)
    VALUES (v_lobby_id, p_user_id, 'member');

    SELECT l.id, l.code, l.name, l.category, l.max_members, l.status,
           lm.user_id, u.nickname, u.avatar_url, lm.role, lm.is_ready
      FROM lobbies l
      JOIN lobby_members lm ON lm.lobby_id = l.id
      JOIN users u          ON u.id = lm.user_id
     WHERE l.id = v_lobby_id
     ORDER BY lm.joined_at;
END //

DROP PROCEDURE IF EXISTS sp_lobby_get //
CREATE PROCEDURE sp_lobby_get(IN p_lobby_id INT)
BEGIN
    SELECT l.id, l.code, l.name, l.category, l.max_members, l.status,
           lm.user_id, u.nickname, u.avatar_url, lm.role, lm.is_ready
      FROM lobbies l
      JOIN lobby_members lm ON lm.lobby_id = l.id
      JOIN users u          ON u.id = lm.user_id
     WHERE l.id = p_lobby_id
     ORDER BY lm.role DESC, lm.joined_at;
END //

DROP PROCEDURE IF EXISTS sp_lobby_kick //
CREATE PROCEDURE sp_lobby_kick(
    IN p_host_id   INT,
    IN p_lobby_id  INT,
    IN p_target_id INT
)
BEGIN
    DECLARE v_role VARCHAR(10);

    SELECT role INTO v_role
      FROM lobby_members
     WHERE lobby_id = p_lobby_id AND user_id = p_host_id;

    IF v_role != 'host' THEN
        SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT = 'NOT_HOST';
    END IF;

    DELETE FROM lobby_members
     WHERE lobby_id = p_lobby_id
       AND user_id = p_target_id
       AND role != 'host';
END //

DROP PROCEDURE IF EXISTS sp_lobby_start //
CREATE PROCEDURE sp_lobby_start(
    IN p_host_id  INT,
    IN p_lobby_id INT
)
BEGIN
    DECLARE v_role     VARCHAR(10);
    DECLARE v_category VARCHAR(20);

    SELECT role INTO v_role
      FROM lobby_members
     WHERE lobby_id = p_lobby_id AND user_id = p_host_id;

    IF v_role != 'host' THEN
        SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT = 'NOT_HOST';
    END IF;

    UPDATE lobbies SET status = 'in_progress'
     WHERE id = p_lobby_id AND status = 'waiting';

    SELECT category INTO v_category FROM lobbies WHERE id = p_lobby_id;
    SELECT v_category AS category;
END //

DROP PROCEDURE IF EXISTS sp_lobby_close //
CREATE PROCEDURE sp_lobby_close(
    IN p_host_id  INT,
    IN p_lobby_id INT
)
BEGIN
    DECLARE v_role VARCHAR(10);

    SELECT role INTO v_role
      FROM lobby_members
     WHERE lobby_id = p_lobby_id AND user_id = p_host_id;

    IF v_role != 'host' THEN
        SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT = 'NOT_HOST';
    END IF;

    UPDATE lobbies SET status = 'closed'
     WHERE id = p_lobby_id;
END //

DROP PROCEDURE IF EXISTS sp_lobby_message_save //
CREATE PROCEDURE sp_lobby_message_save(
    IN p_lobby_id INT,
    IN p_user_id  INT,
    IN p_message  VARCHAR(500)
)
BEGIN
    INSERT INTO lobby_messages (lobby_id, user_id, message)
    VALUES (p_lobby_id, p_user_id, p_message);
    SELECT LAST_INSERT_ID() AS id;
END //

DROP PROCEDURE IF EXISTS sp_lobby_member_leave //
CREATE PROCEDURE sp_lobby_member_leave(
    IN p_lobby_id INT,
    IN p_user_id  INT
)
BEGIN
    DELETE FROM lobby_members
     WHERE lobby_id = p_lobby_id AND user_id = p_user_id;

    -- 남은 멤버 수 반환 (0이면 로비 종료 처리용)
    SELECT COUNT(*) AS remaining
      FROM lobby_members
     WHERE lobby_id = p_lobby_id;
END //

DELIMITER ;
