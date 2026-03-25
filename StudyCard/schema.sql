-- StudyCard 스키마
CREATE DATABASE IF NOT EXISTS study_card
  CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci;

USE study_card;

CREATE TABLE IF NOT EXISTS flash_cards (
    id          INT AUTO_INCREMENT PRIMARY KEY,
    category    VARCHAR(20) NOT NULL COMMENT 'CS, C++, 자료구조',
    question    TEXT NOT NULL,
    answer      TEXT NOT NULL,
    study_date  DATE NOT NULL DEFAULT (CURDATE()),
    created_at  TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    INDEX idx_category (category),
    INDEX idx_date (study_date)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
