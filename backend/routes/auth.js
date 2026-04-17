const express = require('express');
const bcrypt = require('bcryptjs');
const jwt = require('jsonwebtoken');
const https = require('https');
const crypto = require('crypto');
const pool = require('../db/connection');
const { callProcedure, firstRowset, firstRow } = require('../db/procedures');

const router = express.Router();
const SECRET = process.env.JWT_SECRET || 'studybot-secret-key';
const GOOGLE_ID = process.env.GOOGLE_CLIENT_ID;
const GOOGLE_SEC = process.env.GOOGLE_CLIENT_SECRET;
const SERVER_URL = process.env.SERVER_URL || 'http://localhost:3000';
const SALT_ROUNDS = 10;

function signToken(user) {
    return jwt.sign(
        {
            id: user.id,
            username: user.username || user.google_id,
            nickname: user.nickname,
        },
        SECRET,
        { expiresIn: '24h' }
    );
}

async function upsertGoogleUser({ googleId, email, nickname, avatarUrl }) {
    return firstRow(await callProcedure(pool, 'sp_auth_upsert_google_user', [
        googleId,
        email,
        nickname,
        avatarUrl,
    ]));
}

router.post('/register', async (req, res) => {
    const { username, password, nickname } = req.body;
    if (!username?.trim() || !password?.trim()) {
        return res.status(400).json({ error: 'username and password are required' });
    }

    try {
        const hash = await bcrypt.hash(password, SALT_ROUNDS);
        const row = firstRow(await callProcedure(pool, 'sp_auth_create_user', [
            username.trim(),
            hash,
            (nickname || username).trim(),
        ]));

        res.status(201).json({ id: row?.insert_id, message: 'registered' });
    } catch (err) {
        if (err.code === 'ER_DUP_ENTRY') {
            return res.status(409).json({ error: 'username already exists' });
        }
        console.error('[register]', err);
        res.status(500).json({ error: 'server error' });
    }
});

router.post('/login', async (req, res) => {
    const { username, password } = req.body;
    if (!username || !password) {
        return res.status(400).json({ error: 'username and password are required' });
    }

    try {
        const users = firstRowset(await callProcedure(pool, 'sp_auth_get_login_user', [
            username.trim(),
        ]));

        if (users.length === 0 || !users[0].password_hash) {
            return res.status(401).json({ error: 'invalid credentials' });
        }

        const ok = await bcrypt.compare(password, users[0].password_hash);
        if (!ok) {
            return res.status(401).json({ error: 'invalid credentials' });
        }

        const user = users[0];
        const token = signToken({ ...user, username });
        res.json({ token, nickname: user.nickname, userId: user.id });
    } catch (err) {
        console.error('[login]', err);
        res.status(500).json({ error: 'server error' });
    }
});

router.get('/google', (req, res) => {
    const state = req.query.state || crypto.randomBytes(8).toString('hex');
    const redirect = encodeURIComponent(`${SERVER_URL}/api/auth/google/callback`);
    const scope = encodeURIComponent('openid email profile');

    const url = 'https://accounts.google.com/o/oauth2/v2/auth'
        + `?client_id=${GOOGLE_ID}`
        + `&redirect_uri=${redirect}`
        + '&response_type=code'
        + `&scope=${scope}`
        + `&state=${state}`;

    res.redirect(url);
});

router.get('/google/callback', async (req, res) => {
    const { code, state } = req.query;
    if (!code || !state) {
        return res.status(400).send('missing code or state');
    }

    try {
        const tokenData = await exchangeGoogleCode(code);
        const profile = await fetchGoogleProfile(tokenData.access_token);
        const user = await upsertGoogleUser({
            googleId: profile.sub,
            email: profile.email,
            nickname: profile.name || profile.email,
            avatarUrl: profile.picture,
        });
        const token = signToken({ ...user, username: user.google_id });
        const expires = new Date(Date.now() + 5 * 60 * 1000)
            .toISOString()
            .slice(0, 19)
            .replace('T', ' ');

        await callProcedure(pool, 'sp_auth_upsert_oauth_pending', [
            state,
            token,
            user.nickname,
            user.id,
            expires,
        ]);

        res.send('<html><body><script>'
            + 'document.body.innerText="Google login completed. Return to the app.";'
            + '</script></body></html>');
    } catch (err) {
        console.error('[google/callback]', err);
        res.status(500).send(`google login failed: ${err.message}`);
    }
});

router.get('/google/poll', async (req, res) => {
    const { state } = req.query;
    if (!state) {
        return res.status(400).json({ error: 'state is required' });
    }

    try {
        const rows = firstRowset(await callProcedure(pool, 'sp_auth_get_valid_oauth_pending', [
            state,
        ]));

        if (rows.length === 0) {
            return res.status(202).json({ pending: true });
        }

        const { token, nickname, user_id } = rows[0];
        await callProcedure(pool, 'sp_auth_delete_oauth_pending', [state]);
        res.json({ token, nickname, userId: user_id });
    } catch (err) {
        console.error('[google/poll]', err);
        res.status(500).json({ error: 'server error' });
    }
});

router.post('/google/mobile', async (req, res) => {
    const { idToken } = req.body;
    if (!idToken) {
        return res.status(400).json({ error: 'idToken is required' });
    }

    try {
        const profile = await verifyGoogleIdToken(idToken);
        const user = await upsertGoogleUser({
            googleId: profile.sub,
            email: profile.email,
            nickname: profile.name || profile.email,
            avatarUrl: profile.picture,
        });
        const token = signToken({ ...user, username: user.google_id });
        res.json({ token, nickname: user.nickname, userId: user.id });
    } catch (err) {
        console.error('[google/mobile]', err);
        res.status(401).json({ error: 'google token verification failed' });
    }
});

async function cleanExpiredPending() {
    try {
        await callProcedure(pool, 'sp_auth_delete_expired_oauth_pending');
    } catch {
        // no-op
    }
}

cleanExpiredPending();
setInterval(cleanExpiredPending, 10 * 60 * 1000);

function httpsGet(url) {
    return new Promise((resolve, reject) => {
        https.get(url, res => {
            let data = '';
            res.on('data', chunk => {
                data += chunk;
            });
            res.on('end', () => {
                try {
                    resolve(JSON.parse(data));
                } catch {
                    reject(new Error('failed to parse JSON'));
                }
            });
        }).on('error', reject);
    });
}

function httpsPost(url, postData) {
    return new Promise((resolve, reject) => {
        const urlObj = new URL(url);
        const body = new URLSearchParams(postData).toString();
        const opts = {
            hostname: urlObj.hostname,
            path: urlObj.pathname,
            method: 'POST',
            headers: {
                'Content-Type': 'application/x-www-form-urlencoded',
                'Content-Length': Buffer.byteLength(body),
            },
        };

        const req = https.request(opts, res => {
            let data = '';
            res.on('data', chunk => {
                data += chunk;
            });
            res.on('end', () => {
                try {
                    resolve(JSON.parse(data));
                } catch {
                    reject(new Error('failed to parse JSON'));
                }
            });
        });

        req.on('error', reject);
        req.write(body);
        req.end();
    });
}

async function exchangeGoogleCode(code) {
    return httpsPost('https://oauth2.googleapis.com/token', {
        code,
        client_id: GOOGLE_ID,
        client_secret: GOOGLE_SEC,
        redirect_uri: `${SERVER_URL}/api/auth/google/callback`,
        grant_type: 'authorization_code',
    });
}

async function fetchGoogleProfile(accessToken) {
    return httpsGet(
        `https://www.googleapis.com/oauth2/v3/userinfo?access_token=${accessToken}`
    );
}

async function verifyGoogleIdToken(idToken) {
    const data = await httpsGet(
        `https://oauth2.googleapis.com/tokeninfo?id_token=${idToken}`
    );

    if (data.error) {
        throw new Error(data.error);
    }
    if (data.aud !== GOOGLE_ID) {
        throw new Error('client_id mismatch');
    }

    return data;
}

module.exports = router;
