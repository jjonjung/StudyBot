-- interview_study DB 초기화
CREATE DATABASE IF NOT EXISTS interview_study
  CHARACTER SET utf8mb4
  COLLATE utf8mb4_unicode_ci;

USE interview_study;

CREATE TABLE IF NOT EXISTS questions (
    id          INT AUTO_INCREMENT PRIMARY KEY,
    study_date  DATE NOT NULL,
    member      VARCHAR(20) NOT NULL COMMENT '여민, 은정, 혜선',
    category    ENUM('CS', 'C++', '자료구조') NOT NULL,
    question    TEXT NOT NULL,
    created_at  TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    UNIQUE KEY uq_date_member_category (study_date, member, category),
    INDEX idx_date (study_date)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
