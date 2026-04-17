const express = require('express');
const pool = require('../db/connection');
const auth = require('../middleware/auth');
const { callProcedure, firstRowset, firstRow } = require('../db/procedures');

const router = express.Router();

const VALID_CATEGORIES = ['Unreal', 'C++', 'CS', 'Company', 'Algorithm'];
const VALID_DIFFICULTIES = ['Easy', 'Normal', 'Hard'];

router.get('/', auth, async (req, res) => {
    const { category, difficulty, page = 1, limit = 20 } = req.query;
    const safeCategory = VALID_CATEGORIES.includes(category) ? category : null;
    const safeDifficulty = VALID_DIFFICULTIES.includes(difficulty) ? difficulty : null;
    const safeCompany = req.query.company?.trim() || null;
    const safeLimit = Math.max(1, Number(limit) || 20);
    const safePage = Math.max(1, Number(page) || 1);
    const safeOffset = (safePage - 1) * safeLimit;

    try {
        const rows = firstRowset(await callProcedure(pool, 'sp_cards_list', [
            safeCategory,
            safeDifficulty,
            safeCompany,
            safeLimit,
            safeOffset,
        ]));
        res.json(rows);
    } catch (err) {
        console.error('[cards GET]', err);
        res.status(500).json({ error: 'server error' });
    }
});

router.get('/interview', auth, async (req, res) => {
    const { category, count = 10 } = req.query;
    const safeCategory = VALID_CATEGORIES.includes(category) ? category : null;
    const safeCompany = req.query.company?.trim() || null;
    const safeCount = Math.max(1, Number(count) || 10);

    try {
        const rows = firstRowset(await callProcedure(pool, 'sp_cards_interview', [
            safeCategory,
            safeCompany,
            safeCount,
        ]));
        res.json(rows);
    } catch (err) {
        console.error('[cards/interview]', err);
        res.status(500).json({ error: 'server error' });
    }
});

router.get('/stats', auth, async (req, res) => {
    try {
        const rows = firstRowset(await callProcedure(pool, 'sp_cards_stats'));
        res.json(rows);
    } catch (err) {
        console.error('[cards/stats]', err);
        res.status(500).json({ error: 'server error' });
    }
});

router.get('/companies', auth, async (req, res) => {
    try {
        const rows = firstRowset(await callProcedure(pool, 'sp_cards_companies'));
        res.json(rows.map(row => row.company));
    } catch (err) {
        console.error('[cards/companies]', err);
        res.status(500).json({ error: 'server error' });
    }
});

router.get('/:id', auth, async (req, res) => {
    try {
        const row = firstRow(await callProcedure(pool, 'sp_cards_get_by_id', [
            Number(req.params.id),
        ]));
        if (!row) {
            return res.status(404).json({ error: 'card not found' });
        }
        res.json(row);
    } catch (err) {
        console.error('[cards/:id]', err);
        res.status(500).json({ error: 'server error' });
    }
});

router.post('/', auth, async (req, res) => {
    const {
        category,
        company = null,
        question,
        answer,
        difficulty = 'Normal',
        core_conditions = null,
        selection_reason = null,
        code_cpp = null,
        code_csharp = null,
        time_complexity = null,
    } = req.body;

    if (!VALID_CATEGORIES.includes(category) || !question?.trim() || !answer?.trim()) {
        return res.status(400).json({ error: 'invalid card payload' });
    }

    try {
        const row = firstRow(await callProcedure(pool, 'sp_cards_create', [
            category,
            company,
            question.trim(),
            answer.trim(),
            difficulty,
            core_conditions,
            selection_reason,
            code_cpp,
            code_csharp,
            time_complexity,
        ]));
        res.status(201).json({ id: row?.insert_id });
    } catch (err) {
        console.error('[cards POST]', err);
        res.status(500).json({ error: 'server error' });
    }
});

module.exports = router;
