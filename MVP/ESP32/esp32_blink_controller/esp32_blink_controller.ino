#include <WiFi.h>
#include <WebServer.h>
#include <SPIFFS.h>
#include <ArduinoJson.h>

// Пин встроенного светодиода на ESP32 (обычно GPIO2)
#define LED_PIN 2

// Конфигурация WiFi точки доступа
const char* ssid = "ESP32_Blink_Controller";
const char* password = "123456789";

// Веб-сервер на порту 80
WebServer server(80);

// Структура для хранения параметров мигания
struct BlinkStep {
  int lowDuration;   // Длительность LOW сигнала в миллисекундах
  int highDuration;  // Длительность HIGH сигнала в миллисекундах
};

// Переменные для управления миганием
BlinkStep* sequence = nullptr;
int sequenceLength = 0;
int totalDuration = 0; // в секундах
bool isRunning = false;
bool timerExpired = false;
bool blinkTimerStarted = false;
unsigned long startTime = 0;
int currentStep = 0;
unsigned long stepStartTime = 0;
bool stepState = false; // false = LOW, true = HIGH
unsigned long lastUpdateTime = 0;

// Инициализация
void setup() {
  // Инициализация Serial для отладки
  Serial.begin(115200);
  
  // Инициализация пина светодиода
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);
  
  // Инициализация SPIFFS
 if (!SPIFFS.begin(true)) {
    Serial.println("Ошибка инициализации SPIFFS");
    // Попытка форматирования SPIFFS при ошибке
    if (!SPIFFS.format()) {
      Serial.println("Ошибка форматирования SPIFFS");
    } else {
      Serial.println("SPIFFS отформатирована успешно");
    }
    if (!SPIFFS.begin(true)) {
      Serial.println("SPIFFS все равно не инициализирована");
    }
 }
  
  // Загрузка конфигурации из JSON файла
  if (!loadConfigFromJSON()) {
    Serial.println("Не удалось загрузить конфигурацию из JSON файла");
  } else {
    Serial.println("Конфигурация успешно загружена");
  }
  
  // Настройка WiFi в режиме точки доступа
  WiFi.softAP(ssid, password);
  IPAddress IP = WiFi.softAPIP();
  Serial.print("Точка доступа создана. IP адрес: ");
  Serial.println(IP);
  
  // Настройка HTTP endpoints
  server.on("/", HTTP_GET, handleRoot);
  server.on("/style.css", HTTP_GET, handleCSS);
  server.on("/script.js", HTTP_GET, handleJS);
  server.on("/api/check-config", HTTP_GET, handleCheckConfig);
  server.on("/api/control", HTTP_POST, handleControl);
  server.on("/api/timer-status", HTTP_GET, handleTimerStatus);
  
  // Запуск веб-сервера
  server.begin();
  Serial.println("HTTP сервер запущен");
  
  // Инициализация переменных
  lastUpdateTime = millis();
}

// Основной цикл
void loop() {
  // Обработка HTTP запросов
  server.handleClient();
  
  // Обновление состояния мигания если таймер не истек
  if (!timerExpired && isRunning) {
    handleBlinkSequence();
    checkTimer();
  }
  
  // Если таймер истек, остановить мигание
  if (timerExpired && isRunning) {
    stopBlinking();
  }
}

// Загрузка конфигурации из JSON файла
bool loadConfigFromJSON() {
  // Открываем файл config.json из SPIFFS
  File configFile = SPIFFS.open("/config.json", "r");
  if (!configFile) {
    Serial.println("Failed to open config.json");
    return false;
  }

  // Читаем содержимое файла
  size_t size = configFile.size();
  std::unique_ptr<char[]> buf(new char[size + 1]);
  configFile.readBytes(buf.get(), size);
  configFile.close();
  buf.get()[size] = '\0';

  // Парсим JSON
  DynamicJsonDocument doc(4096); // Достаточно для большинства config.json
  DeserializationError error = deserializeJson(doc, buf.get());
  if (error) {
    Serial.println("Failed to parse config.json");
    return false;
  }

  // Извлекаем последовательность миганий
  JsonArray sequenceArray = doc["sequence"];
  sequenceLength = sequenceArray.size();
  
  // Освобождаем предыдущую память, если нужно
  if (sequence != nullptr) {
    delete[] sequence;
  }
  
  // Выделяем память для новой последовательности
  sequence = new BlinkStep[sequenceLength];
  
  // Заполняем структуру данными из JSON
  for (int i = 0; i < sequenceLength; i++) {
    sequence[i].lowDuration = sequenceArray[i]["LOW"];
    sequence[i].highDuration = sequenceArray[i]["HIGH"];
  }
  
  // Извлекаем общую длительность
  totalDuration = doc["duration"];
  
  Serial.printf("Загружено %d шагов мигания, общая длительность: %d секунд\n", sequenceLength, totalDuration);
  
  return true;
}

// Обработка главной страницы
void handleRoot() {
  // Отправляем HTML файл из SPIFFS
  if (SPIFFS.exists("/index.html")) {
    File file = SPIFFS.open("/index.html", "r");
    if(file) {
      server.streamFile(file, "text/html");
      file.close();
    } else {
      server.send(500, "text/plain", "Failed to open index.html");
    }
  } else {
    server.send(404, "text/plain", "index.html not found");
  }
}

// Обработка CSS запросов
void handleCSS() {
  if (SPIFFS.exists("/style.css")) {
    File file = SPIFFS.open("/style.css", "r");
    if(file) {
      server.streamFile(file, "text/css");
      file.close();
    } else {
      server.send(500, "text/plain", "Failed to open style.css");
    }
  } else {
    server.send(404, "text/plain", "style.css not found");
  }
}

// Обработка JavaScript запросов
void handleJS() {
  if (SPIFFS.exists("/script.js")) {
    File file = SPIFFS.open("/script.js", "r");
    if(file) {
      server.streamFile(file, "application/javascript");
      file.close();
    } else {
      server.send(500, "text/plain", "Failed to open script.js");
    }
  } else {
    server.send(404, "text/plain", "script.js not found");
  }
}

// Обработка проверки конфигурации
void handleCheckConfig() {
  // Проверяем наличие и валидность config.json
  bool configExists = SPIFFS.exists("/config.json");
  bool isValid = false;
  int seqLength = 0;
  int duration = 0;
  
  if (configExists) {
    // Проверяем валидность JSON
    File configFile = SPIFFS.open("/config.json", "r");
    if (configFile) {
      size_t size = configFile.size();
      std::unique_ptr<char[]> buf(new char[size + 1]);
      configFile.readBytes(buf.get(), size);
      configFile.close();
      buf.get()[size] = '\0';
      
      DynamicJsonDocument doc(4096);
      DeserializationError error = deserializeJson(doc, buf.get());
      if (!error) {
        isValid = true;
        seqLength = doc["sequence"].size();
        duration = doc["duration"];
      }
    }
  }
  
  // Формируем JSON ответ
  String json = "{";
  json += "\"configExists\":" + String(configExists ? "true" : "false") + ",";
  json += "\"isValid\":" + String(isValid ? "true" : "false") + ",";
  json += "\"sequenceLength\":" + String(seqLength) + ",";
  json += "\"totalDuration\":" + String(duration);
  json += "}";
  
  server.send(200, "application/json", json);
}

// Обработка команды управления
void handleControl() {
  if (server.method() != HTTP_POST) {
    server.send(405, "text/plain", "Method Not Allowed");
    return;
  }
  
  // Читаем тело запроса
  String requestBody = server.arg("plain");
  
  // Парсим JSON команды
  DynamicJsonDocument doc(1024);
  DeserializationError error = deserializeJson(doc, requestBody);
  if (error) {
    server.send(400, "application/json", "{\"success\":false,\"message\":\"Invalid JSON\"}");
    return;
  }
  
  String command = doc["command"];
  
  // Проверяем команду
  if (command != "start" && command != "stop") {
    server.send(400, "application/json", "{\"success\":false,\"message\":\"Invalid command\"}");
    return;
  }
  
  // Обрабатываем команду
  bool success = false;
  String currentStatus = "idle";
  
  if (command == "start") {
    if (timerExpired) {
      // Если таймер истек, не позволяем запуск
      success = false;
      currentStatus = "expired";
    } else {
      startBlinking();
      success = true;
      currentStatus = "running";
    }
  } else if (command == "stop") {
    stopBlinking();
    success = true;
    currentStatus = "idle";
  }
  
  // Формируем JSON ответ
  String json = "{";
  json += "\"success\":" + String(success ? "true" : "false") + ",";
  json += "\"currentStatus\":\"" + currentStatus + "\"";
  json += "}";
  
  server.send(200, "application/json", json);
}

// Обработка запроса состояния таймера
void handleTimerStatus() {
  unsigned long currentTime = millis();
  int timeElapsed = 0;
  int timeRemaining = totalDuration;
  
  if (blinkTimerStarted) {
    timeElapsed = (currentTime - startTime) / 1000;
    timeRemaining = totalDuration - timeElapsed;
    
    if (timeRemaining < 0) {
      timeRemaining = 0;
    }
  }
  
  // Формируем JSON ответ
  String json = "{";
  json += "\"timerExpired\":" + String(timerExpired ? "true" : "false") + ",";
  json += "\"timeRemaining\":" + String(timeRemaining) + ",";
  json += "\"timeElapsed\":" + String(timeElapsed) + ",";
  json += "\"totalDuration\":" + String(totalDuration);
  json += "}";
  
  server.send(200, "application/json", json);
}

// Запуск мигания
void startBlinking() {
  if (sequence == nullptr || sequenceLength == 0) {
    Serial.println("Невозможно запустить мигание: последовательность не загружена");
    return;
  }
  
  if (timerExpired) {
    Serial.println("Невозможно запустить мигание: время истекло");
    return;
  }
  
  isRunning = true;
  blinkTimerStarted = true;
  startTime = millis();
  currentStep = 0;
  stepState = false; // Начинаем с LOW
  stepStartTime = millis();
  
  // Устанавливаем начальное состояние светодиода
  digitalWrite(LED_PIN, LOW);
  
  Serial.println("Мигание запущено");
}

// Остановка мигания
void stopBlinking() {
  isRunning = false;
  digitalWrite(LED_PIN, LOW);
  Serial.println("Мигание остановлено");
}

// Обработка последовательности миганий
void handleBlinkSequence() {
  if (sequence == nullptr || sequenceLength == 0) {
    return;
  }
  
  unsigned long currentTime = millis();
  unsigned long elapsed = currentTime - stepStartTime;
  
  // Получаем текущий шаг
  BlinkStep current = sequence[currentStep];
  
  if (!stepState) {
    // Сейчас LOW состояние
    if (elapsed >= current.lowDuration) {
      // Переходим к HIGH состоянию
      stepState = true;
      stepStartTime = currentTime;
      digitalWrite(LED_PIN, HIGH);
    }
  } else {
    // Сейчас HIGH состояние
    if (elapsed >= current.highDuration) {
      // Переходим к следующему шагу или возвращаемся к первому
      currentStep = (currentStep + 1) % sequenceLength;
      stepState = false;
      stepStartTime = currentTime;
      digitalWrite(LED_PIN, LOW);
    }
  }
}

// Проверка таймера
void checkTimer() {
  if (blinkTimerStarted && !timerExpired) {
    unsigned long currentTime = millis();
    int timeElapsed = (currentTime - startTime) / 1000;
    
    if (timeElapsed >= totalDuration) {
      timerExpired = true;
      Serial.println("Таймер истек - мигание остановлено");
    }
  }
}