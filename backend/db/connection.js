const mysql = require('mysql2/promise');
require('dotenv').config();

const pool = mysql.createPool({
    host:               process.env.DB_HOST     || 'localhost',
    port:               Number(process.env.DB_PORT || 3306),
    user:               process.env.DB_USER     || 'root',
    password:           process.env.DB_PASSWORD || '',
    database:           process.env.DB_NAME     || 'studybot',
    charset:            'utf8mb4',
    waitForConnections: true,
    connectionLimit:    10,
    queueLimit:         0,
    timezone:           '+09:00',
});

module.exports = pool;
