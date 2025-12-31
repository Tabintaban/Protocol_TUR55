let isRunning = false;
let timerExpired = false;
let totalDuration = 180; // 3 минуты по умолчанию

// Функция проверки конфигурации
function checkConfig() {
  fetch('/api/check-config').then(response => response.json()).then(data => {
    const statusDiv = document.getElementById('status');
    if (data.configExists && data.isValid) {
      statusDiv.innerHTML = 'Состояние: Конфигурация найдена и валидна'; statusDiv.style.backgroundColor = '#d4edda';
      document.getElementById('controlBtn').disabled = false;
      totalDuration = data.totalDuration;
    } else if (!data.configExists) {
      statusDiv.innerHTML = 'Состояние: Конфигурация отсутствует'; statusDiv.style.backgroundColor = '#f8d7da';
      document.getElementById('controlBtn').disabled = true;
    } else {
      statusDiv.innerHTML = 'Состояние: Конфигурация невалидна'; statusDiv.style.backgroundColor = '#f8d7da';
      document.getElementById('controlBtn').disabled = true;
    }
  }).catch(error => { console.error('Error:', error); });
}

// Функция запуска/останова мигания
function toggleBlinking() {
  const command = isRunning ? 'stop' : 'start';
  fetch('/api/control', { method: 'POST', headers: { 'Content-Type': 'application/json' },
  body: JSON.stringify({ command: command }) }).then(response => response.json()).then(data => {
    if (data.success) {
      isRunning = !isRunning;
      const btn = document.getElementById('controlBtn');
      if (isRunning) { btn.innerHTML = 'Стоп'; btn.className = 'btn btn-stop'; }
      else { btn.innerHTML = 'Старт'; btn.className = 'btn'; }
      if (data.currentStatus === 'expired') {
        timerExpired = true;
        document.getElementById('expiredMsg').style.display = 'block';
        document.getElementById('controlBtn').disabled = true;
      }
    }
  }).catch(error => { console.error('Error:', error); });
}

// Функция обновления таймера
function updateTimer() {
  if (isRunning || timerExpired) {
    fetch('/api/timer-status').then(response => response.json()).then(data => {
      const timerDiv = document.getElementById('timer');
      const minutes = Math.floor(data.timeRemaining / 60);
      const seconds = data.timeRemaining % 60;
      timerDiv.innerHTML = 'Время: ' + minutes.toString().padStart(2, '0') + ':' + seconds.toString().padStart(2, '0');
      if (data.timerExpired) {
        timerExpired = true;
        document.getElementById('expiredMsg').style.display = 'block';
        document.getElementById('controlBtn').disabled = true;
        isRunning = false;
        const btn = document.getElementById('controlBtn');
        btn.innerHTML = 'Старт'; btn.className = 'btn';
      }
    }).catch(error => { console.error('Error:', error); });
  }
}

// Периодическое обновление таймера
setInterval(updateTimer, 1000);
checkConfig(); // Проверяем конфигурацию при загрузке