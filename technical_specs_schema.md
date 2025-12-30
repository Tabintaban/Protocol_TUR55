# Схема технических спецификаций транспортных средств TUR55 v2.0

## Общая схема

Схема технических спецификаций определяет структуру данных, содержащих полную информацию о транспортном средстве, необходимую для корректной работы протокола TUR55 v2.0.

## Структура технических спецификаций

```json
{
  "identification": {
    "vin": "string",           // Уникальный идентификационный номер
    "plate": "string",         // Номерной знак
    "make": "string",          // Марка автомобиля
    "model": "string",         // Модель
    "year": "number",          // Год выпуска
    "type": "string",          // Тип ТС (car, truck, motorcycle, etc.)
    "category": "string",      // Категория (A, B, C, D, etc.)
    "color": "string",         // Цвет
    "weight": {
      "curbWeightKg": "number",    // Снаряженная масса
      "grossWeightKg": "number",   // Полная масса
      "payloadKg": "number"        // Грузоподъемность
    }
  },
  "technical": {
    "engine": {
      "type": "string",              // Тип двигателя (gasoline, diesel, electric, hybrid)
      "displacement": "string",      // Объем двигателя (например, "2.5L")
      "powerHP": "number",           // Мощность в лошадиных силах
      "powerKW": "number",           // Мощность в киловаттах
      "torqueNM": "number",          // Крутящий момент (Н·м)
      "cylinders": "number",         // Количество цилиндров
      "fuelSystem": "string",        // Тип топливной системы
      "transmission": {
        "type": "string",            // Тип трансмиссии (manual, automatic, cvt)
        "gears": "number"            // Количество передач
      }
    },
    "fuel": {
      "tankCapacityL": "number",         // Объем топливного бака (литры)
      "type": ["string"],                // Типы топлива (ai95, ai98, diesel, electric)
      "consumptionCityL100": "number",   // Расход в городе (л/100км)
      "consumptionHighwayL100": "number", // Расход на трассе (л/100км)
      "consumptionMixedL100": "number",   // Смешанный расход (л/100км)
      "rangeKm": "number",               // Запас хода (км)
      "chargingTimeH": "number"          // Время зарядки (для электромобилей)
    },
    "drivetrain": {
      "configuration": "string",         // Привод (FWD, RWD, AWD, 4WD)
      "axles": "number",                 // Количество осей
      "wheelSize": "string"              // Размер колес
    },
    "electronics": {
      "canBusVersion": "string",         // Версия CAN-шины
      "obdProtocol": "string",           // Протокол OBD
      "supportedPIDs": ["string"],       // Поддерживаемые PID-ы
      "infotainment": {
        "hasGPS": "boolean",             // Наличие GPS
        "hasBluetooth": "boolean",       // Наличие Bluetooth
        "hasWifi": "boolean",            // Наличие Wi-Fi
        "hasOTA": "boolean"              // Возможность обновления по воздуху
      }
    },
    "telematics": {
      "hasTelematics": "boolean",        // Наличие телематики
      "connectionType": "string",        // Тип подключения (4G, 5G, etc.)
      "simIntegrated": "boolean",        // Интегрированная SIM-карта
      "dataPlanRequired": "boolean"      // Требуется ли тарифный план
    }
  },
  "safety": {
    "maxSpeedKPH": "number",             // Максимальная скорость (км/ч)
    "acceleration0100": "number",        // Разгон до 100 км/ч (сек)
    "brakingDistance": {
      "from100to0M": "number"            // Тормозной путь с 100 до 0 (м)
    },
    "supportedADAS": ["string"],         // Поддерживаемые ADAS (ABS, ESP, LKA, AEB, etc.)
    "airbags": {
      "count": "number",                 // Количество подушек безопасности
      "types": ["string"]                // Типы подушек (front, side, curtain, etc.)
    },
    "security": {
      "immobilizer": "boolean",          // Иммобилайзер
      "alarm": "boolean",                // Сигнализация
      "keyless": "boolean",              // Бесключевой доступ
      "remoteStart": "boolean"           // Дистанционный запуск
    },
    "tirePressureMonitoring": "boolean", // Мониторинг давления в шинах
    "tireSize": "string"                 // Размер шин
  },
 "cryptography": {
    "publicKey": "string",               // Публичный криптографический ключ
    "keyAlgo": "string",                 // Алгоритм шифрования (Ed25519, secp256k1, etc.)
    "keyStorage": "string",              // Тип хранилища ключа (HSM, TPM, etc.)
    "encryptionProtocol": "string",      // Протокол шифрования
    "certificateChain": "string"         // Цепочка сертификатов
  },
  "environmental": {
    "emissions": {
      "co2GPKM": "number",               // Выбросы CO2 (г/км)
      "euroStandard": "string"           // Евро стандарт (Евро-5, Евро-6, etc.)
    },
    "efficiencyClass": "string"          // Класс эффективности (A+, B, C, etc.)
  },
  "manufacturing": {
    "manufacturer": "string",            // Производитель
    "assemblyPlant": "string",           // Завод-изготовитель
    "productionDate": "string",          // Дата производства
    "warranty": {
      "durationMonths": "number",        // Срок гарантии (месяцы)
      "distanceKm": "number"             // Гарантийный пробег (км)
    }
  }
}
```

## Автоматическая генерация правил на основе техспецификаций

Система может автоматически генерировать правила доступа и ограничения на основе технических характеристик:

```typescript
// Пример генерации правил на основе техспецификаций
function generateSmartRules(techSpecs: VehicleSpecs): SessionRules {
  const rules: SessionRules = {
    // Ограничение скорости на основе максимальной скорости ТС
    maxSpeed: Math.min(techSpecs.safety.maxSpeedKPH, 120), // Не более 120 км/ч или макс. скорости ТС
    
    // Предупреждение о топливе на основе объема бака
    fuelWarningLevel: techSpecs.technical.fuel.tankCapacityL * 0.2, // 20% от объема бака
    
    // Разрешенные типы топлива
    allowedFuelTypes: techSpecs.technical.fuel.type,
    
    // Требования к системам безопасности
    requireADAS: techSpecs.safety.supportedADAS.includes('ESP'),
    
    // Мониторинг давления в шинах
    tirePressureMonitoring: techSpecs.safety.tirePressureMonitoring,
    
    // Ограничения по весу на основе грузоподъемности
    payloadLimit: techSpecs.identification.weight.payloadKg,
    
    // Геозоны на основе типа ТС
    restrictedAreas: determineRestrictedAreas(techSpecs),
    
    // Время использования на основе времени зарядки (для электромобилей)
    maxUsageTime: calculateMaxUsageTime(techSpecs),
    
    // Требования к водителю на основе сложности управления
    requiredLicenseCategory: determineRequiredLicense(techSpecs)
  };
  
  return rules;
}
```

## Проверка соответствия правил техспецификациям

Система должна проверять, что устанавливаемые правила соответствуют техническим возможностям транспортного средства:

```solidity
// Пример проверки соответствия правил техспецификациям в смарт-контракте
function validateRulesCompliance(
    uint256 licenseId, 
    SessionRules calldata rules
) external view returns (bool) {
    VehicleSpecs memory specs = getTechSpecs(licenseId);
    
    // Проверка ограничения скорости
    require(rules.maxSpeed <= specs.safety.maxSpeedKPH, "Speed exceeds vehicle capability");
    
    // Проверка ограничения топлива
    require(rules.fuelLimit <= specs.technical.fuel.tankCapacityL, "Fuel limit exceeds tank capacity");
    
    // Проверка типов разрешенного топлива
    require(isSubset(rules.allowedFuelTypes, specs.technical.fuel.type), "Invalid fuel type");
    
    // Проверка ограничений по весу
    require(rules.payloadLimit <= specs.identification.weight.payloadKg, "Payload exceeds capacity");
    
    // Проверка, что геозоны доступны для этого типа ТС
    if (rules.geofences.length > 0) {
        require(specs.electronics.infotainment.hasGPS, "Vehicle has no GPS for geofencing");
    }
    
    return true;
}
```

## Хранение и обновление техспецификаций

### Варианты хранения

1. **Полностью в токене** (для небольших наборов данных)
2. **Хеш в токене, полные данные во внечейн хранилище** (IPFS, Arweave)
3. **Разделенное хранение** (базовые данные в токене, детальные во внечейн хранилище)

### Формат внечейн данных

```json
{
  "schemaVersion": "2.0",
  "timestamp": "2024-01-15T10:00:00Z",
  "checksum": "sha3-256:...",
  "techSpecs": {
    // Полная структура техспецификаций
  },
  "additionalData": {
    "documents": [
      {
        "type": "manual",
        "uri": "ipfs://QmManualHash",
        "format": "pdf"
      },
      {
        "type": "maintenanceHistory",
        "uri": "ipfs://QmMaintenanceHash",
        "format": "json"
      }
    ],
    "images": [
      {
        "type": "exterior",
        "uri": "ipfs://QmExteriorImageHash",
        "format": "jpg"
      }
    ]
  }
}
```

## Валидация техспецификаций

Система должна обеспечивать валидацию технических спецификаций:

1. **Проверка структуры данных**
2. **Проверка соответствия значений**
3. **Проверка криптографической целостности**
4. **Проверка подлинности источника**

## Интеграция с различными блокчейнами

### Ethereum
- Использование JSON Schema для валидации
- Хранение хеша в NFT метаданных
- Использование IPFS для хранения полных данных

### Solana
- Бинарная сериализация (Borsh)
- Хранение в отдельных аккаунтах
- Использование Pubkey для идентификации

### Cosmos SDK
- Использование Protobuf для определения структур
- Хранение в состоянии контракта
- Использование IBC для межцепочечной передачи данных

### Polkadot
- Использование SCALE codec для сериализации
- Хранение в состояниях Pallet
- Использование XCM для межцепочечных операций

## Обновление техспецификаций

Система поддерживает несколько механизмов обновления:

1. **Создание нового токена** с обновленными данными
2. **Обновление внечейн данных** по сохраненной ссылке
3. **Использование версионирования** для отслеживания изменений
4. **Механизмы согласования** для коллективного обновления данных