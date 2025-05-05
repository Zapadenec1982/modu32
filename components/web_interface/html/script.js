/* ModuChill - Скрипт веб-інтерфейсу */

// Ініціалізація при завантаженні сторінки
document.addEventListener('DOMContentLoaded', () => {
    initNavigation();
    initModeSwitching();
    setupEventListeners();
    updateSystemInfo();
    
    // Спочатку отримаємо поточні дані системи
    fetchSystemData();
    
    // Налаштування таймерів оновлення
    setInterval(fetchSystemData, 5000); // Оновлення даних кожні 5 секунд
    setInterval(updateUptime, 60000);   // Оновлення часу роботи кожну хвилину
});

// Ініціалізація навігації між вкладками
function initNavigation() {
    const navLinks = document.querySelectorAll('nav a');
    const pages = document.querySelectorAll('.page');
    
    navLinks.forEach(link => {
        link.addEventListener('click', function(e) {
            e.preventDefault();
            
            // Визначаємо ID цільової сторінки
            const targetId = this.getAttribute('href').substring(1);
            
            // Деактивуємо всі посилання та приховуємо всі сторінки
            navLinks.forEach(link => link.classList.remove('active'));
            pages.forEach(page => page.classList.remove('active'));
            
            // Активуємо вибране посилання та відображаємо відповідну сторінку
            this.classList.add('active');
            document.getElementById(targetId).classList.add('active');
        });
    });
}

// Ініціалізація перемикання режимів
function initModeSwitching() {
    const modeSelect = document.getElementById('mode-select');
    const manualControls = document.getElementById('manual-controls');
    
    modeSelect.addEventListener('change', function() {
        if (this.value === 'manual') {
            manualControls.style.display = 'block';
        } else {
            manualControls.style.display = 'none';
        }
        
        // Відправка інформації про зміну режиму на сервер
        updateMode(this.value);
    });
}

// Налаштування інших обробників подій
function setupEventListeners() {
    // Кнопки керування вручну
    const compressorToggle = document.getElementById('compressor-toggle');
    const fanToggle = document.getElementById('fan-toggle');
    
    compressorToggle.addEventListener('click', () => toggleDevice('compressor'));
    fanToggle.addEventListener('click', () => toggleDevice('fan'));
    
    // Кнопки системи
    const restartButton = document.getElementById('restart-device');
    const updateButton = document.getElementById('update-firmware');
    
    restartButton.addEventListener('click', () => {
        if (confirm('Ви впевнені, що хочете перезавантажити пристрій?')) {
            restartDevice();
        }
    });
    
    updateButton.addEventListener('click', () => {
        alert('Функція оновлення прошивки буде доступна пізніше.');
    });
    
    // Кнопки журналу
    const refreshLogsBtn = document.getElementById('refresh-logs');
    const clearLogsBtn = document.getElementById('clear-logs');
    
    refreshLogsBtn.addEventListener('click', fetchLogs);
    clearLogsBtn.addEventListener('click', clearLogs);
    
    // Форма налаштувань
    const settingsForm = document.getElementById('settings-form');
    const resetSettingsBtn = document.getElementById('reset-settings');
    
    settingsForm.addEventListener('submit', function(e) {
        e.preventDefault();
        saveSettings();
    });
    
    resetSettingsBtn.addEventListener('click', () => {
        if (confirm('Ви впевнені, що хочете скинути всі налаштування до значень за замовчуванням?')) {
            resetSettings();
        }
    });
    
    // Вибір рівня логів
    const logLevelSelect = document.getElementById('log-level');
    logLevelSelect.addEventListener('change', fetchLogs);
}

// Оновлення інформації системи
function updateSystemInfo() {
    const firmwareVersion = document.getElementById('firmware-version');
    const deviceId = document.getElementById('device-id');
    const modulesCount = document.getElementById('modules-count');
    
    // Заглушки для початкових значень
    firmwareVersion.textContent = '0.1.0';
    deviceId.textContent = 'MC-001';
    modulesCount.textContent = '0';
    
    // Глобальний лічильник часу роботи (початкове значення)
    window.uptimeMinutes = 0;
    updateUptime();
}

// Оновлення часу роботи системи
function updateUptime() {
    const uptimeElement = document.getElementById('uptime');
    window.uptimeMinutes = (window.uptimeMinutes || 0) + 1;
    
    if (window.uptimeMinutes < 60) {
        uptimeElement.textContent = `${window.uptimeMinutes} хв`;
    } else {
        const hours = Math.floor(window.uptimeMinutes / 60);
        const minutes = window.uptimeMinutes % 60;
        uptimeElement.textContent = `${hours} год ${minutes} хв`;
    }
}

// API-функції для взаємодії з сервером

// Отримання даних системи
function fetchSystemData() {
    // Імітація отримання даних від сервера (буде замінено реальним API запитом)
    simulateAPIResponse().then(data => {
        updateUI(data);
    }).catch(error => {
        console.error('Помилка отримання даних:', error);
    });
}

// Оновлення UI на основі отриманих даних
function updateUI(data) {
    // Оновлення температури
    document.getElementById('current-temp').textContent = data.temperature.toFixed(1);
    document.getElementById('temp-setpoint').textContent = data.setPoint.toFixed(1);
    document.getElementById('temp-hysteresis').textContent = data.hysteresis.toFixed(1);
    
    // Оновлення статусів пристроїв
    const compressorStatus = document.getElementById('compressor-status');
    const fanStatus = document.getElementById('fan-status');
    const modeStatus = document.getElementById('mode-status');
    const wifiStatus = document.getElementById('wifi-status');
    
    compressorStatus.textContent = data.compressor ? 'Увімк' : 'Вимк';
    compressorStatus.className = data.compressor ? 'status-value status-on' : 'status-value status-off';
    
    fanStatus.textContent = data.fan ? 'Увімк' : 'Вимк';
    fanStatus.className = data.fan ? 'status-value status-on' : 'status-value status-off';
    
    modeStatus.textContent = translateMode(data.mode);
    
    wifiStatus.textContent = data.wifi ? 'Підключено' : 'Відключено';
    wifiStatus.className = data.wifi ? 'status-value status-on' : 'status-value status-off';
    
    // Оновлення вибраного режиму у випадаючому списку
    document.getElementById('mode-select').value = data.mode;
    
    // Оновлення системної інформації
    document.getElementById('free-memory').textContent = `${data.freeMemory} KB`;
    document.getElementById('modules-count').textContent = data.modulesCount || '0';
}

// Переклад режимів системи
function translateMode(mode) {
    const modes = {
        'auto': 'Автоматичний',
        'manual': 'Ручний',
        'defrost': 'Розморожування',
        'off': 'Вимкнено'
    };
    return modes[mode] || mode;
}

// Функція для перемикання пристроїв (у ручному режимі)
function toggleDevice(device) {
    // Заглушка, буде замінена реальним API запитом
    console.log(`Перемикання ${device}`);
    
    // Імітація запиту до API
    fetch(`/api/toggle/${device}`, {
        method: 'POST'
    }).then(response => {
        if (!response.ok) {
            throw new Error('Не вдалося перемкнути пристрій');
        }
        return response.json();
    }).then(() => {
        // Отримання оновлених даних після перемикання
        fetchSystemData();
    }).catch(error => {
        console.error('Помилка перемикання пристрою:', error);
        alert(`Не вдалося перемкнути ${device}: ${error.message}`);
    });
}

// Функція оновлення режиму роботи
function updateMode(mode) {
    // Заглушка, буде замінена реальним API запитом
    console.log(`Зміна режиму на ${mode}`);
    
    // Імітація запиту до API
    fetch('/api/mode', {
        method: 'POST',
        headers: {
            'Content-Type': 'application/json'
        },
        body: JSON.stringify({ mode })
    }).then(response => {
        if (!response.ok) {
            throw new Error('Не вдалося змінити режим');
        }
        return response.json();
    }).then(() => {
        // Отримання оновлених даних після зміни режиму
        fetchSystemData();
    }).catch(error => {
        console.error('Помилка зміни режиму:', error);
        alert(`Не вдалося змінити режим: ${error.message}`);
    });
}

// Отримання журналу
function fetchLogs() {
    const logLevel = document.getElementById('log-level').value;
    const logContent = document.getElementById('log-content');
    
    logContent.textContent = 'Завантаження журналу...';
    
    // Імітація запиту до API
    fetch(`/api/logs?level=${logLevel}`)
        .then(response => {
            if (!response.ok) {
                throw new Error('Не вдалося отримати журнали');
            }
            return response.text();
        })
        .then(data => {
            logContent.textContent = data || 'Журнал порожній';
        })
        .catch(error => {
            console.error('Помилка отримання журналу:', error);
            logContent.textContent = `Помилка: ${error.message}`;
        });
}

// Очищення журналу
function clearLogs() {
    const logContent = document.getElementById('log-content');
    
    // Імітація запиту до API
    fetch('/api/logs/clear', { method: 'POST' })
        .then(response => {
            if (!response.ok) {
                throw new Error('Не вдалося очистити журнал');
            }
            return response.json();
        })
        .then(() => {
            logContent.textContent = 'Журнал очищено';
        })
        .catch(error => {
            console.error('Помилка очищення журналу:', error);
            alert(`Не вдалося очистити журнал: ${error.message}`);
        });
}

// Перезавантаження пристрою
function restartDevice() {
    // Імітація запиту до API
    fetch('/api/restart', { method: 'POST' })
        .then(response => {
            if (!response.ok) {
                throw new Error('Не вдалося перезавантажити пристрій');
            }
            alert('Пристрій перезавантажується...');
            setTimeout(() => {
                window.location.reload();
            }, 5000);
        })
        .catch(error => {
            console.error('Помилка перезавантаження:', error);
            alert(`Не вдалося перезавантажити пристрій: ${error.message}`);
        });
}

// Збереження налаштувань
function saveSettings() {
    const settings = {
        control: {
            set_temp: parseFloat(document.getElementById('set-temp').value),
            hysteresis: parseFloat(document.getElementById('set-hysteresis').value),
            min_compressor_off_time: parseInt(document.getElementById('min-compressor-off').value)
        },
        device: {
            name: document.getElementById('device-name').value
        },
        web: {
            password: document.getElementById('admin-password').value
        },
        sensors: {
            temp_sensor_type: document.getElementById('sensor-type').value,
            ds18b20_pin: parseInt(document.getElementById('sensor-pin').value)
        }
    };
    
    // Перевірка валідності даних
    if (isNaN(settings.control.set_temp) || isNaN(settings.control.hysteresis) || isNaN(settings.control.min_compressor_off_time)) {
        alert('Будь ласка, вкажіть коректні числові значення для налаштувань температури');
        return;
    }
    
    // Імітація запиту до API
    fetch('/api/settings', {
        method: 'POST',
        headers: {
            'Content-Type': 'application/json'
        },
        body: JSON.stringify(settings)
    }).then(response => {
        if (!response.ok) {
            throw new Error('Не вдалося зберегти налаштування');
        }
        return response.json();
    }).then(() => {
        alert('Налаштування успішно збережено');
        fetchSystemData(); // Оновлення даних після збереження
    }).catch(error => {
        console.error('Помилка збереження налаштувань:', error);
        alert(`Не вдалося зберегти налаштування: ${error.message}`);
    });
}

// Скидання налаштувань
function resetSettings() {
    // Імітація запиту до API
    fetch('/api/settings/reset', { method: 'POST' })
        .then(response => {
            if (!response.ok) {
                throw new Error('Не вдалося скинути налаштування');
            }
            return response.json();
        })
        .then(() => {
            alert('Налаштування скинуто до значень за замовчуванням');
            // Перезавантаження сторінки після скидання налаштувань
            window.location.reload();
        })
        .catch(error => {
            console.error('Помилка скидання налаштувань:', error);
            alert(`Не вдалося скинути налаштування: ${error.message}`);
        });
}

// Функція імітації відповіді API (для тестування без серверної частини)
function simulateAPIResponse() {
    return new Promise(resolve => {
        // Імітація затримки мережі
        setTimeout(() => {
            // Заглушка даних для відображення
            resolve({
                temperature: 4.2 + (Math.random() * 0.6 - 0.3), // Імітація невеликих коливань температури
                setPoint: 5.0,
                hysteresis: 1.0,
                compressor: Math.random() > 0.5, // Випадковий стан компресора
                fan: Math.random() > 0.3, // Випадковий стан вентилятора
                mode: 'auto',
                wifi: true,
                freeMemory: Math.floor(230 + Math.random() * 20),
                modulesCount: 0
            });
        }, 200);
    });
}