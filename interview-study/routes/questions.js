const express = require('express');
const router = express.Router();
const pool = require('../db/connection');
const { callProcedure, firstRowset, firstRow } = require('../db/procedures');

const MEMBERS = ['여민', '은정', '혜선'];
const CATEGORIES = ['CS', 'C++', '자료구조'];

router.get('/', async (req, res) => {
    try {
        const date = req.query.date || null;
        const rows = firstRowset(await callProcedure(pool, 'sp_questions_list', [date]));
        res.json(rows);
    } catch (err) {
        console.error('[questions GET]', err);
        res.status(500).json({ error: 'server error' });
    }
});

router.get('/dates', async (req, res) => {
    try {
        const rows = firstRowset(await callProcedure(pool, 'sp_question_dates'));
        res.json(rows.map(row => {
            const d = new Date(row.study_date);
            return d.toISOString().split('T')[0];
        }));
    } catch (err) {
        console.error('[questions/dates]', err);
        res.status(500).json({ error: 'server error' });
    }
});

router.post('/', async (req, res) => {
    try {
        const { study_date, member, category, question } = req.body;

        if (!study_date || !member || !category || !question?.trim()) {
            return res.status(400).json({ error: 'missing required fields' });
        }
        if (!MEMBERS.includes(member)) {
            return res.status(400).json({ error: 'invalid member' });
        }
        if (!CATEGORIES.includes(category)) {
            return res.status(400).json({ error: 'invalid category' });
        }

        const row = firstRow(await callProcedure(pool, 'sp_question_create', [
            study_date,
            member,
            category,
            question.trim(),
        ]));
        res.status(201).json({ id: row?.insert_id, message: 'created' });
    } catch (err) {
        if (err.code === 'ER_DUP_ENTRY') {
            return res.status(409).json({ error: 'duplicate question for date/member/category' });
        }
        console.error('[questions POST]', err);
        res.status(500).json({ error: 'server error' });
    }
});

router.delete('/:id', async (req, res) => {
    try {
        const row = firstRow(await callProcedure(pool, 'sp_question_delete', [
            Number(req.params.id),
        ]));
        if (!row?.affected_rows) {
            return res.status(404).json({ error: 'question not found' });
        }
        res.json({ message: 'deleted' });
    } catch (err) {
        console.error('[questions DELETE]', err);
        res.status(500).json({ error: 'server error' });
    }
});

module.exports = router;
