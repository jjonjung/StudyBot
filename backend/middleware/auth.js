const jwt = require('jsonwebtoken');

const SECRET = process.env.JWT_SECRET || 'studybot-secret-key';

/**
 * JWT 인증 미들웨어
 * Authorization: Bearer <token>
 */
function authMiddleware(req, res, next) {
    const header = req.headers['authorization'];
    if (!header || !header.startsWith('Bearer ')) {
        return res.status(401).json({ error: '인증 토큰이 없습니다.' });
    }

    const token = header.slice(7);
    try {
        req.user = jwt.verify(token, SECRET);
        next();
    } catch {
        return res.status(401).json({ error: '토큰이 만료되었거나 유효하지 않습니다.' });
    }
}

module.exports = authMiddleware;
