# Универсальный формат лицензионного токена TUR55 v2.0

## Общая структура

Универсальный формат лицензионного токена обеспечивает совместимость между различными блокчейнами и содержит полную информацию о транспортном средстве и правах доступа.

## Основной формат

```json
{
  "tur55License": {
    "version": "2.0",
    "licenseId": "multichain://ethereum:1/0xabc123/789",
    "owner": "multichain://ethereum:1/0xdef456",
    "vehiclePublicKey": "ed25519:abc123...",
    "techSpecs": {
      "identification": {
        "vin": "JH4KA7650MC000000",
        "plate": "A123BC177",
        "make": "Toyota",
        "model": "Camry",
        "year": 2023
      },
      "technical": {
        "engine": {
          "type": "gasoline",
          "displacement": "2.5L",
          "powerHP": 203,
          "torqueNM": 250
        },
        "fuel": {
          "tankCapacityL": 60,
          "type": ["ai95", "ai98"],
          "consumptionCityL100": 8.5,
          "consumptionHighwayL100": 6.2
        },
        "electronics": {
          "canBusVersion": "2.0B",
          "obdProtocol": "ISO15765-4",
          "supportedPIDs": ["0105", "010C", "010D"]
        }
      },
      "safety": {
        "maxSpeedKPH": 220,
        "supportedADAS": ["ABS", "ESP", "LKA"],
        "tirePressureMonitoring": true
      },
      "cryptography": {
        "publicKey": "ed25519:abc123...",
        "keyAlgo": "Ed25519",
        "keyStorage": "HSM-TPM2.0"
      }
    },
    "accessControl": {
      "maxDrivers": 5,
      "geofenceRequired": false,
      "speedLimitEnabled": true
    },
    "metadataURI": "ipfs://QmTechSpecsHash",
    "signature": "multichain://ethereum:1/0xabc123/789/signature/def789",
    "royaltyBPS": 100
  }
}
```

## Универсальный идентификатор лицензии

Формат: `tur55://{chain}/{chainId}/{address}/{tokenId}`

Примеры:
- `tur55://ethereum:1/0xabc123/789` - Ethereum Mainnet
- `tur55://polygon:137/0xdef456/123` - Polygon Mainnet
- `tur55://solana:mainnet/ABC123def` - Solana Mainnet
- `tur55://cosmos:juno-1/juno123abc/token/456` - Juno Network

## Структура технических характеристик (techSpecs)

### Идентификация транспортного средства
- `vin`: Уникальный идентификационный номер
- `plate`: Номерной знак
- `make`: Марка автомобиля
- `model`: Модель
- `year`: Год выпуска

### Технические характеристики
- `engine`: Двигатель (тип, объем, мощность, крутящий момент)
- `fuel`: Топливная система (объем бака, типы топлива, расход)
- `electronics`: Электроника (версия CAN, протокол OBD, поддерживаемые PID)
- `safety`: Системы безопасности (макс. скорость, поддерживаемые ADAS)
- `cryptography`: Криптографические параметры (публичный ключ, алгоритм, хранилище ключа)

## Права доступа

- `maxDrivers`: Максимальное количество разрешенных водителей
- `geofenceRequired`: Требуется ли геозонирование
- `speedLimitEnabled`: Включено ли ограничение скорости

## Механизмы безопасности

- Универсальная криптографическая подпись
- Проверка подлинности владельца
- Защита от подделки
- Контроль доступа к техническим данным

## Хранение данных

### На-чейн данные (on-chain)
- Основные идентификаторы
- Криптографические ключи
- Базовые параметры доступа

### Внечейн данные (off-chain)
- Полные технические спецификации (IPFS/Arweave)
- Дополнительные документы
- Изображения и медиафайлы

## Механизмы обновления

1. **Инкрементальные обновления**: Обновление отдельных полей
2. **Полные обновления**: Создание нового токена с обновленными данными
3. **Ссылочные обновления**: Обновление внечейн данных по ссылке

## Совместимость с различными блокчейнами

### Ethereum/EVM
- Реализация через ERC-721 с расширенными метаданными
- Использование IPFS для хранения больших данных

### Solana
- Использование программ и аккаунтов
- Бинарный формат для оптимизации

### Cosmos SDK
- Реализация через CosmWasm
- Использование сообщений для манипуляции данными

### Polkadot/Substrate
- Использование Pallets
- Хранение данных в состояниях

### IOTA/Tangle
- Использование MAM streams
- Оптимизация для IoT устройств