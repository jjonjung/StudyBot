require('dotenv').config();
const express = require('express');
const cors    = require('cors');

const authRouter     = require('./routes/auth');
const cardsRouter    = require('./routes/cards');
const progressRouter = require('./routes/progress');

const app  = express();
const PORT = process.env.PORT || 3000;

app.use(cors());
app.use(express.json());

// ── 라우터 ────────────────────────────────────────────────
app.use('/api/auth',     authRouter);
app.use('/api/cards',    cardsRouter);
app.use('/api/progress', progressRouter);

// ── 헬스체크 ─────────────────────────────────────────────
app.get('/health', (_, res) => res.json({
    status: 'ok',
    version: '2.0.0',
    features: ['algorithm-category','google-oauth','heatmap'],
}));

app.listen(PORT, () => {
    console.log(`[StudyBot v2] 서버 실행 중: http://localhost:${PORT}`);
});
