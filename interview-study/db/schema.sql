CREATE DATABASE IF NOT EXISTS interview_study
  CHARACTER SET utf8mb4
  COLLATE utf8mb4_unicode_ci;

USE interview_study;

CREATE TABLE IF NOT EXISTS questions (
    id          INT AUTO_INCREMENT PRIMARY KEY,
    study_date  DATE NOT NULL,
    member      VARCHAR(20) NOT NULL,
    category    ENUM('CS', 'C++', '자료구조') NOT NULL,
    question    TEXT NOT NULL,
    created_at  TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    UNIQUE KEY uq_date_member_category (study_date, member, category),
    INDEX idx_date (study_date)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

DELIMITER //

DROP PROCEDURE IF EXISTS sp_questions_list //
CREATE PROCEDURE sp_questions_list(
    IN p_study_date DATE
)
BEGIN
    SELECT id, study_date, member, category, question, created_at
      FROM questions
     WHERE p_study_date IS NULL OR study_date = p_study_date
     ORDER BY study_date DESC,
              FIELD(category, 'CS', 'C++', '자료구조'),
              FIELD(member, '여민', '은정', '혜선');
END //

DROP PROCEDURE IF EXISTS sp_question_dates //
CREATE PROCEDURE sp_question_dates()
BEGIN
    SELECT DISTINCT study_date
      FROM questions
     ORDER BY study_date DESC
     LIMIT 30;
END //

DROP PROCEDURE IF EXISTS sp_question_create //
CREATE PROCEDURE sp_question_create(
    IN p_study_date DATE,
    IN p_member VARCHAR(20),
    IN p_category VARCHAR(20),
    IN p_question TEXT
)
BEGIN
    INSERT INTO questions (study_date, member, category, question)
    VALUES (p_study_date, p_member, p_category, p_question);

    SELECT LAST_INSERT_ID() AS insert_id;
END //

DROP PROCEDURE IF EXISTS sp_question_delete //
CREATE PROCEDURE sp_question_delete(
    IN p_id INT
)
BEGIN
    DELETE FROM questions
     WHERE id = p_id;

    SELECT ROW_COUNT() AS affected_rows;
END //

DELIMITER ;
