# Формат сессионного файла TUR55 v2.0

## Общее описание

Сессионный файл представляет собой автономный документ, содержащий информацию о текущем сеансе использования транспортного средства. Он содержит ссылку на лицензию, правила доступа и криптографическую защиту.

## Основной формат

```json
{
  "protocol": "TUR55 v2.0",
  "license": {
    "id": "multichain://ethereum:1/0xabc123/789",
    "chainId": "ethereum:1",
    "verificationProof": "tx:0xabc123def456...",
    "blockchainAdapter": "ethereum"
  },
  "session": {
    "id": "session:uuid-v4-string",
    "type": "fuel_based",
    "limit": 50.5,
    "startTime": "2024-01-15T10:00:00Z",
    "endTime": "2024-01-17T10:00:00Z",
    "rules": {
      "maxSpeed": 120,
      "validFrom": "2024-01-15T10:00:00Z",
      "validUntil": "2024-01-17T10:00:00Z",
      "geofences": [
        {
          "type": "polygon",
          "coordinates": [
            {"lat": 55.7558, "lng": 37.6176},
            {"lat": 55.7500, "lng": 37.6200},
            {"lat": 55.7450, "lng": 37.6150},
            {"lat": 55.7500, "lng": 37.6100}
          ],
          "name": "Moscow Central Zone"
        }
      ],
      "driverRestrictions": {
        "allowedDrivers": ["multichain://ethereum:1/0xdef456", "multichain://solana:mainnet/ABC123def"],
        "maxDrivers": 3
      },
      "usageLimits": {
        "maxDistanceKm": 500,
        "maxTimeHours": 48,
        "maxFuelLiters": 50
      }
    }
  },
  "crypto": {
    "vehiclePublicKey": "ed25519:abc123def...",
    "encryptedSessionKey": "ecies:xyz789...",
    "founderSignature": "signature:multichain...",
    "hash": "sha3-256:hash_value_here",
    "nonce": "random_nonce_value"
  },
 "metadata": {
    "createdAt": "2024-01-15T09:30:00Z",
    "creator": "multichain://ethereum:1/0xcreator_address",
    "purpose": "corporate_fleet_usage",
    "version": "2.0"
  }
}
```

## Типы сессий

### 1. Топливная сессия (fuel_based)
- Ограничивает использование по количеству топлива
- Подходит для бензиновых и дизельных автомобилей

### 2. Временная сессия (time_based)
- Ограничивает использование по времени
- Подходит для аренды и каршеринга

### 3. Дистанционная сессия (distance_based)
- Ограничивает использование по пройденному расстоянию
- Подходит для такси и доставки

### 4. Комбинированная сессия (combined)
- Использует несколько типов ограничений
- Наиболее гибкий вариант

## Правила сессии

### Ограничения скорости
- `maxSpeed`: Максимальная разрешенная скорость (км/ч)
- Может быть динамическим (изменяться в зависимости от геозоны)

### Геозоны
- `geofences`: Список разрешенных или запрещенных зон
- Поддержка полигональных, круговых и линейных зон
- Проверка в реальном времени

### Ограничения водителя
- `allowedDrivers`: Список разрешенных владельцев кошельков
- `maxDrivers`: Максимальное количество водителей
- Проверка криптографических подписей

### Ограничения использования
- `maxDistanceKm`: Максимальное расстояние (км)
- `maxTimeHours`: Максимальное время использования (часы)
- `maxFuelLiters`: Максимальное количество топлива (литры)

## Криптографическая защита

### Публичный ключ транспортного средства
- `vehiclePublicKey`: Криптографический ключ ТС
- Используется для шифрования сессионного ключа
- Позволяет ТС расшифровать команды управления

### Зашифрованный сессионный ключ
- `encryptedSessionKey`: Сессионный ключ, зашифрованный открытым ключом ТС
- Используется для симметричного шифрования команд
- Обеспечивает конфиденциальность сессии

### Подпись основателя
- `founderSignature`: Криптографическая подпись основателя протокола
- Подтверждает подлинность сессионного файла
- Предотвращает подделку

### Хэш целостности
- `hash`: Криптографический хэш всего документа
- Позволяет проверить целостность данных
- Обнаруживает любые изменения

## Универсальная система валидации

```typescript
class UniversalSessionValidator {
  async validate(sessionFile: SessionFile): Promise<ValidationResult> {
    // 1. Проверка структуры и формата
    const structureValid = this.validateSchema(sessionFile);
    if (!structureValid) {
      return { valid: false, error: "Invalid session file structure" };
    }

    // 2. Извлечение информации о блокчейне из license.id
    const chainInfo = parseChainId(sessionFile.license.id);
    
    // 3. Выбор нужного адаптера
    const adapter = this.getAdapter(chainInfo.chain);
    
    // 4. Проверка лицензии на исходном блокчейне
    const licenseValid = await adapter.verifyLicense(
      sessionFile.license.id,
      sessionFile.metadata.creator
    );
    
    if (!licenseValid) {
      return { valid: false, error: "License verification failed" };
    }

    // 5. Проверка подлинности создателя
    const creatorValid = await this.verifyCreator(sessionFile);
    
    if (!creatorValid) {
      return { valid: false, error: "Creator authentication failed" };
    }

    // 6. Проверка криптографической подписи основателя
    const founderSigValid = verifyFounderSignature(sessionFile);
    
    if (!founderSigValid) {
      return { valid: false, error: "Founder signature invalid" };
    }

    // 7. Проверка хэша целостности
    const integrityValid = this.verifyIntegrity(sessionFile);
    
    if (!integrityValid) {
      return { valid: false, error: "Session file integrity compromised" };
    }

    // 8. Проверка, что теххарактеристики подходят
    const licenseData = await adapter.getLicenseData(sessionFile.license.id);
    const specsValid = await this.validateTechSpecs(
      licenseData.techSpecs,
      sessionFile.session.rules
    );
    
    if (!specsValid) {
      return { valid: false, error: "Technical specs do not match session rules" };
    }

    // 9. Проверка актуальности времени
    const timeValid = this.validateTimeConstraints(sessionFile);
    
    if (!timeValid) {
      return { valid: false, error: "Session time constraints violated" };
    }

    // 10. Проверка соответствия правил теххарактеристикам
    const complianceValid = await this.validateRulesCompliance(
      licenseData.techSpecs,
      sessionFile.session.rules
    );

    if (!complianceValid) {
      return { valid: false, error: "Session rules violate technical specs" };
    }

    return { valid: true, sessionData: sessionFile.session };
  }

 private validateSchema(sessionFile: SessionFile): boolean {
    // Проверка структуры сессионного файла
    const requiredFields = ['protocol', 'license', 'session', 'crypto'];
    return requiredFields.every(field => field in sessionFile);
  }

  private async verifyCreator(sessionFile: SessionFile): Promise<boolean> {
    // Проверка подлинности создателя
    // Может включать проверку прав доступа к лицензии
    return true; // Заглушка - реализация зависит от конкретного блокчейна
  }

  private verifyIntegrity(sessionFile: SessionFile): boolean {
    // Вычисление хэша всего документа и сравнение сохраненным
    const computedHash = this.computeHash(sessionFile);
    return computedHash === sessionFile.crypto.hash;
  }

  private validateTimeConstraints(sessionFile: SessionFile): boolean {
    const now = new Date();
    const validFrom = new Date(sessionFile.session.rules.validFrom);
    const validUntil = new Date(sessionFile.session.rules.validUntil);
    
    return now >= validFrom && now <= validUntil;
  }

  private async validateTechSpecs(
    techSpecs: VehicleSpecs, 
    rules: SessionRules
  ): Promise<boolean> {
    // Проверка соответствия правил техническим характеристикам
    if (rules.maxSpeed && techSpecs.safety.maxSpeedKPH < rules.maxSpeed) {
      return false;
    }

    if (rules.usageLimits?.maxFuelLiters && 
        techSpecs.technical.fuel.tankCapacityL < rules.usageLimits.maxFuelLiters) {
      return false;
    }

    return true;
  }

  private async validateRulesCompliance(
    techSpecs: VehicleSpecs,
    rules: SessionRules
  ): Promise<boolean> {
    // Проверка соответствия правил техническим возможностям ТС
    // Реализация аналогична смарт-контракту в блокчейне
    return true; // Заглушка
  }
}
```

## Спецификации валидации для разных блокчейнов

### Ethereum
- Использование смарт-контрактов для проверки
- Поддержка сложных логических выражений
- Возможность хранения исторических данных

### Solana
- Использование программ для проверки
- Высокая скорость валидации
- Поддержка параллельных вычислений

### Cosmos SDK
- Использование CosmWasm для проверки
- Поддержка IBC для межцепочечной валидации
- Возможность кастомных модулей

### Polkadot
- Использование Substrate pallets
- Поддержка XCM для межцепочечных операций
- Высокая степень кастомизации

## Автоматическая генерация правил

Система может автоматически генерировать правила на основе технических характеристик:

```typescript
class RuleGenerator {
  generateRulesFromTechSpecs(techSpecs: VehicleSpecs, sessionType: SessionType): SessionRules {
    const rules: SessionRules = {
      // Ограничение скорости на основе максимальной скорости ТС
      maxSpeed: Math.min(techSpecs.safety.maxSpeedKPH * 0.8, 120),
      
      // Временные ограничения
      validFrom: new Date().toISOString(),
      validUntil: new Date(Date.now() + 48 * 60 * 60 * 1000).toISOString(), // +48 часов
      
      // Ограничения на основе типа сессии
      usageLimits: this.generateUsageLimits(techSpecs, sessionType),
      
      // Геозоны на основе типа ТС
      geofences: this.generateGeofences(techSpecs),
      
      // Ограничения водителя на основе систем безопасности
      driverRestrictions: this.generateDriverRestrictions(techSpecs)
    };

    return rules;
  }

  private generateUsageLimits(techSpecs: VehicleSpecs, sessionType: SessionType): UsageLimits {
    const limits: UsageLimits = {};

    switch (sessionType) {
      case 'fuel_based':
        limits.maxFuelLiters = techSpecs.technical.fuel.tankCapacityL * 0.8; // 80% от бака
        break;
      case 'distance_based':
        limits.maxDistanceKm = this.calculateMaxDistance(techSpecs);
        break;
      case 'time_based':
        limits.maxTimeHours = 24; // Максимум 24 часа
        break;
    }

    return limits;
  }

  private generateGeofences(techSpecs: VehicleSpecs): Geofence[] {
    // Генерация геозон на основе типа ТС
    // Например, грузовикам могут быть недоступны узкие улицы в центре города
    return [];
  }

  private generateDriverRestrictions(techSpecs: VehicleSpecs): DriverRestrictions {
    // Ограничения водителя на основе систем безопасности
    // Например, для сложных ТС может потребоваться специальная квалификация
    return {
      maxDrivers: techSpecs.accessControl.maxDrivers
    };
  }
}
```

## Обработка ошибок и исключительных ситуаций

Система должна корректно обрабатывать различные ситуации:

1. **Истечение срока действия сессии**
2. **Нарушение правил использования**
3. **Проблемы с подключением к интернету**
4. **Отказ системы ТС**
5. **Подозрительная активность**

## Безопасность сессионных файлов

1. **Шифрование чувствительных данных**
2. **Криптографическая подпись всех изменений**
3. **Защита от повторного воспроизведения**
4. **Контроль целостности**
5. **Ограничение по времени действия**