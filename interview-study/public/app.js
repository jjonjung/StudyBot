'use strict';

const API = '/api';
const MEMBERS = ['여민', '은정', '혜선'];
const CATEGORIES = ['CS', 'C++', '자료구조'];
const CATEGORY_NAMES = { 'CS': 'CS 지식', 'C++': 'C++ 프로그래밍', '자료구조': '자료구조' };

// ===== 탭 전환 =====
document.querySelectorAll('.tab-btn').forEach(btn => {
    btn.addEventListener('click', () => {
        document.querySelectorAll('.tab-btn').forEach(b => b.classList.remove('active'));
        document.querySelectorAll('.tab-content').forEach(c => c.classList.remove('active'));
        btn.classList.add('active');
        document.getElementById(`tab-${btn.dataset.tab}`).classList.add('active');
    });
});

// ===== 날짜 유틸 =====
function getToday() {
    const now = new Date();
    const kst = new Date(now.getTime() + 9 * 60 * 60 * 1000);
    return kst.toISOString().split('T')[0];
}

function formatDate(dateStr) {
    const days = ['일', '월', '화', '수', '목', '금', '토'];
    // new Date('YYYY-MM-DD') 를 로컬 자정으로 파싱
    const [y, m, d] = dateStr.split('-').map(Number);
    const date = new Date(y, m - 1, d);
    return `${m}/${d} (${days[date.getDay()]})`;
}

function escapeHtml(text) {
    const div = document.createElement('div');
    div.appendChild(document.createTextNode(text));
    return div.innerHTML;
}

// ===== 날짜 선택기 =====
const datePicker = document.getElementById('date-picker');

datePicker.addEventListener('change', () => {
    loadQuestions(datePicker.value);
    updateShortcutActive(datePicker.value);
});

document.getElementById('btn-today').addEventListener('click', () => {
    const today = getToday();
    datePicker.value = today;
    loadQuestions(today);
    updateShortcutActive(today);
});

// ===== 최근 날짜 단축 버튼 =====
async function loadDates() {
    try {
        const res   = await fetch(`${API}/questions/dates`);
        const dates = await res.json();
        renderShortcuts(dates);
    } catch {
        // 네트워크 오류 시 단축 버튼 생략
    }
}

function renderShortcuts(dates) {
    const container = document.getElementById('date-shortcuts');
    container.innerHTML = '';
    dates.slice(0, 7).forEach(date => {
        const btn = document.createElement('button');
        btn.className    = 'date-shortcut';
        btn.textContent  = formatDate(date);
        btn.dataset.date = date;
        btn.addEventListener('click', () => {
            datePicker.value = date;
            loadQuestions(date);
            updateShortcutActive(date);
        });
        container.appendChild(btn);
    });
    updateShortcutActive(datePicker.value);
}

function updateShortcutActive(date) {
    document.querySelectorAll('.date-shortcut').forEach(btn => {
        btn.classList.toggle('active', btn.dataset.date === date);
    });
}

// ===== 질문 목록 로드 =====
async function loadQuestions(date) {
    const container = document.getElementById('questions-container');
    container.innerHTML = '<div class="loading">불러오는 중...</div>';
    try {
        const res       = await fetch(`${API}/questions?date=${date}`);
        const questions = await res.json();
        renderQuestions(questions, date);
    } catch {
        container.innerHTML = '<div class="empty-state"><p>질문을 불러오는 데 실패했습니다.</p></div>';
    }
}

function renderQuestions(questions, date) {
    const container = document.getElementById('questions-container');

    if (!questions.length) {
        container.innerHTML = `
            <div class="empty-state">
                <p>📭 ${formatDate(date)}에 등록된 질문이 없습니다.</p>
            </div>`;
        return;
    }

    // category → member → question
    const grouped = {};
    CATEGORIES.forEach(c => { grouped[c] = {}; });
    questions.forEach(q => { grouped[q.category][q.member] = q; });

    const html = CATEGORIES.map(cat => {
        const catMap   = grouped[cat];
        const count    = Object.keys(catMap).length;
        const catClass = cat === 'CS' ? 'cs' : cat === 'C++' ? 'cpp' : 'ds';

        const cards = MEMBERS.map(member => {
            const q          = catMap[member];
            const mClass     = member === '여민' ? 'yemin' : member === '은정' ? 'eunjung' : 'hyesun';
            if (!q) {
                return `
                <div class="question-card empty">
                    <div class="question-member">
                        <span class="question-member-name question-member-name--${mClass}">${member}</span>
                    </div>
                    <p class="question-text question-text--empty">미제출</p>
                </div>`;
            }
            const time = new Date(q.created_at).toLocaleTimeString('ko-KR', { hour: '2-digit', minute: '2-digit' });
            return `
            <div class="question-card">
                <div class="question-member">
                    <span class="question-member-name question-member-name--${mClass}">${member}</span>
                    <span class="question-time">${time}</span>
                </div>
                <p class="question-text">${escapeHtml(q.question)}</p>
                <button class="btn-delete" onclick="deleteQuestion(${q.id},'${date}')">✕</button>
            </div>`;
        }).join('');

        return `
        <div class="category-section">
            <div class="category-header category-header--${catClass}">
                <span class="category-tag category-tag--${catClass}">${cat}</span>
                <span class="category-name">${CATEGORY_NAMES[cat]}</span>
                <span class="category-count">${count} / ${MEMBERS.length}</span>
            </div>
            <div class="category-questions">${cards}</div>
        </div>`;
    }).join('');

    container.innerHTML = `<div class="questions-grid">${html}</div>`;
}

// ===== 질문 삭제 =====
async function deleteQuestion(id, date) {
    if (!confirm('이 질문을 삭제하시겠습니까?')) return;
    try {
        const res  = await fetch(`${API}/questions/${id}`, { method: 'DELETE' });
        const data = await res.json();
        if (res.ok) {
            showToast('질문이 삭제되었습니다.', 'success');
            loadQuestions(date);
            loadDates();
        } else {
            showToast(data.error, 'error');
        }
    } catch {
        showToast('삭제에 실패했습니다.', 'error');
    }
}

// ===== 질문 등록 폼 =====
const form     = document.getElementById('question-form');
const formDate = document.getElementById('form-date');

formDate.value = getToday();

form.addEventListener('submit', async e => {
    e.preventDefault();
    const submitBtn = form.querySelector('.btn-submit');
    submitBtn.disabled    = true;
    submitBtn.textContent = '등록 중...';

    const body = Object.fromEntries(new FormData(form).entries());

    try {
        const res  = await fetch(`${API}/questions`, {
            method:  'POST',
            headers: { 'Content-Type': 'application/json' },
            body:    JSON.stringify(body)
        });
        const data = await res.json();

        if (res.ok) {
            showToast('질문이 등록되었습니다!', 'success');
            form.reset();
            formDate.value = getToday();
            loadDates();
            // 목록 탭으로 이동 후 해당 날짜 표시
            document.querySelector('[data-tab="list"]').click();
            datePicker.value = body.study_date;
            loadQuestions(body.study_date);
            updateShortcutActive(body.study_date);
        } else {
            showToast(data.error, 'error');
        }
    } catch {
        showToast('등록에 실패했습니다.', 'error');
    } finally {
        submitBtn.disabled    = false;
        submitBtn.textContent = '등록하기';
    }
});

// ===== Toast =====
function showToast(message, type = 'success') {
    const toast = document.getElementById('toast');
    toast.textContent = message;
    toast.className   = `toast ${type} show`;
    setTimeout(() => { toast.className = 'toast'; }, 3000);
}

// ===== 초기화 =====
(async () => {
    await loadDates();
    const today = getToday();
    datePicker.value = today;
    loadQuestions(today);
    updateShortcutActive(today);
})();
