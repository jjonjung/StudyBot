const express = require('express');
const pool = require('../db/connection');
const auth = require('../middleware/auth');
const { callProcedure, firstRowset, firstRow } = require('../db/procedures');

const router = express.Router();

const VALID_CATEGORIES = ['Unreal', 'C++', 'CS', 'Company', 'Algorithm', 'Mixed'];

router.get('/', auth, async (req, res) => {
    try {
        const rows = firstRowset(await callProcedure(pool, 'sp_progress_list', [
            req.user.id,
        ]));
        res.json(rows);
    } catch (err) {
        console.error('[progress GET]', err);
        res.status(500).json({ error: 'server error' });
    }
});

router.get('/summary', auth, async (req, res) => {
    try {
        const rows = firstRowset(await callProcedure(pool, 'sp_progress_summary', [
            req.user.id,
        ]));
        res.json(rows);
    } catch (err) {
        console.error('[progress/summary]', err);
        res.status(500).json({ error: 'server error' });
    }
});

router.put('/:cardId', auth, async (req, res) => {
    const { known = 0, score = 0 } = req.body;
    const cardId = Number(req.params.cardId);
    if (Number.isNaN(cardId)) {
        return res.status(400).json({ error: 'invalid cardId' });
    }

    try {
        await callProcedure(pool, 'sp_progress_upsert', [
            req.user.id,
            cardId,
            Number(known),
            Number(score),
        ]);
        res.json({ message: 'saved' });
    } catch (err) {
        console.error('[progress PUT]', err);
        res.status(500).json({ error: 'server error' });
    }
});

router.post('/session', auth, async (req, res) => {
    const { category = 'Mixed', total_cards, known_count } = req.body;
    if (total_cards == null || known_count == null) {
        return res.status(400).json({ error: 'total_cards and known_count are required' });
    }

    const safeCategory = VALID_CATEGORIES.includes(category) ? category : 'Mixed';
    const safeTotalCards = Math.max(0, Number(total_cards) || 0);
    const safeKnownCount = Math.max(0, Number(known_count) || 0);

    try {
        const row = firstRow(await callProcedure(pool, 'sp_progress_create_session', [
            req.user.id,
            safeCategory,
            safeTotalCards,
            safeKnownCount,
        ]));
        res.status(201).json({ id: row?.insert_id });
    } catch (err) {
        console.error('[progress/session]', err);
        res.status(500).json({ error: 'server error' });
    }
});

router.get('/sessions', auth, async (req, res) => {
    try {
        const rows = firstRowset(await callProcedure(pool, 'sp_progress_sessions', [
            req.user.id,
            20,
        ]));
        res.json(rows);
    } catch (err) {
        console.error('[progress/sessions]', err);
        res.status(500).json({ error: 'server error' });
    }
});

router.get('/heatmap', auth, async (req, res) => {
    const year = Number(req.query.year) || new Date().getFullYear();

    try {
        const rows = firstRowset(await callProcedure(pool, 'sp_progress_heatmap', [
            req.user.id,
            year,
        ]));
        res.json(rows);
    } catch (err) {
        console.error('[progress/heatmap]', err);
        res.status(500).json({ error: 'server error' });
    }
});

module.exports = router;
