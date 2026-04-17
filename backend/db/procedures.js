function buildCallSql(name, paramCount) {
    const placeholders = Array.from({ length: paramCount }, () => '?').join(', ');
    return `CALL ${name}(${placeholders})`;
}

async function callProcedure(executor, name, params = []) {
    const [rows] = await executor.query(buildCallSql(name, params.length), params);
    return rows;
}

function firstRowset(rows) {
    if (!Array.isArray(rows)) return [];
    if (Array.isArray(rows[0])) return rows[0];
    return rows;
}

function firstRow(rows) {
    return firstRowset(rows)[0] || null;
}

module.exports = {
    callProcedure,
    firstRowset,
    firstRow,
};
