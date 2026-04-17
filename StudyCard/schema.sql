CREATE DATABASE IF NOT EXISTS study_card
  CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci;

USE study_card;

CREATE TABLE IF NOT EXISTS flash_cards (
    id          INT AUTO_INCREMENT PRIMARY KEY,
    category    VARCHAR(20) NOT NULL,
    question    TEXT NOT NULL,
    answer      TEXT NOT NULL,
    study_date  DATE NOT NULL DEFAULT (CURDATE()),
    created_at  TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    INDEX idx_category (category),
    INDEX idx_date (study_date)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

DELIMITER //

DROP PROCEDURE IF EXISTS sp_add_flash_card //
CREATE PROCEDURE sp_add_flash_card(
    IN p_category VARCHAR(20),
    IN p_question TEXT,
    IN p_answer TEXT,
    IN p_study_date DATE
)
BEGIN
    INSERT INTO flash_cards (category, question, answer, study_date)
    VALUES (p_category, p_question, p_answer, p_study_date);
END //

DROP PROCEDURE IF EXISTS sp_delete_flash_card //
CREATE PROCEDURE sp_delete_flash_card(
    IN p_id INT
)
BEGIN
    DELETE FROM flash_cards
     WHERE id = p_id;
END //

DROP PROCEDURE IF EXISTS sp_get_flash_cards //
CREATE PROCEDURE sp_get_flash_cards(
    IN p_category VARCHAR(20)
)
BEGIN
    SELECT id,
           category,
           question,
           answer,
           DATE_FORMAT(study_date, '%Y-%m-%d') AS study_date
      FROM flash_cards
     WHERE p_category IS NULL OR p_category = '' OR category = p_category
     ORDER BY CASE category
                  WHEN 'CS' THEN 1
                  WHEN 'C++' THEN 2
                  ELSE 3
              END,
              id;
END //

DROP PROCEDURE IF EXISTS sp_get_flash_cards_by_date //
CREATE PROCEDURE sp_get_flash_cards_by_date(
    IN p_study_date DATE
)
BEGIN
    SELECT id,
           category,
           question,
           answer,
           DATE_FORMAT(study_date, '%Y-%m-%d') AS study_date
      FROM flash_cards
     WHERE study_date = p_study_date
     ORDER BY CASE category
                  WHEN 'CS' THEN 1
                  WHEN 'C++' THEN 2
                  ELSE 3
              END,
              id;
END //

DROP PROCEDURE IF EXISTS sp_get_flash_card_total_count //
CREATE PROCEDURE sp_get_flash_card_total_count()
BEGIN
    SELECT COUNT(*) AS total_count
      FROM flash_cards;
END //

DELIMITER ;
