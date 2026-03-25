const express = require('express');
const router  = express.Router();
const pool    = require('../db/connection');

const MEMBERS    = ['여민', '은정', '혜선'];
const CATEGORIES = ['CS', 'C++', '자료구조'];

// GET /api/questions?date=YYYY-MM-DD
router.get('/', async (req, res) => {
    try {
        const { date } = req.query;
        let query  = 'SELECT * FROM questions';
        const params = [];

        if (date) {
            query += ' WHERE study_date = ?';
            params.push(date);
        }

        query += ' ORDER BY study_date DESC, FIELD(category,"CS","C++","자료구조"), FIELD(member,"여민","은정","혜선")';

        const [rows] = await pool.execute(query, params);
        res.json(rows);
    } catch (err) {
        console.error(err);
        res.status(500).json({ error: '서버 오류가 발생했습니다.' });
    }
});

// GET /api/questions/dates  — 질문이 있는 날짜 목록
router.get('/dates', async (req, res) => {
    try {
        const [rows] = await pool.execute(
            'SELECT DISTINCT study_date FROM questions ORDER BY study_date DESC LIMIT 30'
        );
        res.json(rows.map(r => {
            const d = new Date(r.study_date);
            return d.toISOString().split('T')[0];
        }));
    } catch (err) {
        console.error(err);
        res.status(500).json({ error: '서버 오류가 발생했습니다.' });
    }
});

// POST /api/questions
router.post('/', async (req, res) => {
    try {
        const { study_date, member, category, question } = req.body;

        if (!study_date || !member || !category || !question?.trim()) {
            return res.status(400).json({ error: '모든 필드를 입력해주세요.' });
        }
        if (!MEMBERS.includes(member)) {
            return res.status(400).json({ error: '유효하지 않은 멤버입니다.' });
        }
        if (!CATEGORIES.includes(category)) {
            return res.status(400).json({ error: '유효하지 않은 카테고리입니다.' });
        }

        const [result] = await pool.execute(
            'INSERT INTO questions (study_date, member, category, question) VALUES (?, ?, ?, ?)',
            [study_date, member, category, question.trim()]
        );

        res.status(201).json({ id: result.insertId, message: '질문이 등록되었습니다.' });
    } catch (err) {
        if (err.code === 'ER_DUP_ENTRY') {
            return res.status(409).json({ error: '이미 해당 날짜에 같은 카테고리 질문을 등록하셨습니다.' });
        }
        console.error(err);
        res.status(500).json({ error: '서버 오류가 발생했습니다.' });
    }
});

// DELETE /api/questions/:id
router.delete('/:id', async (req, res) => {
    try {
        const [result] = await pool.execute(
            'DELETE FROM questions WHERE id = ?',
            [req.params.id]
        );
        if (result.affectedRows === 0) {
            return res.status(404).json({ error: '질문을 찾을 수 없습니다.' });
        }
        res.json({ message: '질문이 삭제되었습니다.' });
    } catch (err) {
        console.error(err);
        res.status(500).json({ error: '서버 오류가 발생했습니다.' });
    }
});

module.exports = router;
