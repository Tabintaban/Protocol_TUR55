# Система межцепочечной миграции TUR55 v2.0

## Общее описание

Система межцепочечной миграции позволяет перемещать лицензии между различными блокчейнами без потери данных или прав доступа. Это ключевая функция блокчейн-агностичного протокола TUR55 v2.0.

## Общие принципы

### Универсальный идентификатор лицензии
Формат: `tur55://{chain}/{chainId}/{address}/{tokenId}`

Примеры:
- `tur55://ethereum:1/0xabc123/789` - Ethereum Mainnet
- `tur55://polygon:137/0xdef456/123` - Polygon Mainnet
- `tur55://solana:mainnet/ABC123def` - Solana Mainnet
- `tur55://cosmos:juno-1/juno123abc/token/456` - Juno Network

### Процесс миграции
1. Блокировка исходной лицензии
2. Создание доказательства владения
3. Передача доказательства в целевую цепочку
4. Создание новой лицензии в целевой цепочке
5. Разблокировка исходной лицензии (в случае отмены)

## Архитектура миграции

```
┌─────────────────┐    ┌─────────────────┐    ┌─────────────────┐
│   Исходная      │    │   Межцепочечный │    │   Целевая       │
│   цепочка       │───▶│   мост          │───▶│   цепочка       │
│                 │    │                 │    │                 │
│ • Блокировка    │    │ • Проверка      │    │ • Создание      │
│   лицензии      │    │   владения      │    │   новой         │
│ • Создание      │    │ • Передача      │    │   лицензии      │
│   доказательства│    │   доказательства│    │ • Восстановление│
│                 │    │                 │    │   данных        │
└─────────────────┘    └─────────────────┘    └─────────────────┘
```

## Процесс миграции

### 1. Инициация миграции

```solidity
// Контракт моста в исходной цепочке
contract Tur55CrossChainBridge {
    struct MigrationRequest {
        uint256 licenseId;
        string targetChain;
        address targetAddress;
        uint256 timestamp;
        bool completed;
    }
    
    mapping(uint256 => MigrationRequest) public migrationRequests;
    uint256 public nextRequestId;
    
    event MigrationInitiated(
        uint256 indexed requestId,
        uint256 indexed licenseId,
        string targetChain,
        address indexed owner
    );
    
    function initiateMigration(
        uint256 licenseId,
        string memory targetChain,
        address targetAddress
    ) external returns (uint256 requestId) {
        // Проверка владения лицензией
        require(ownerOf(licenseId) == msg.sender, "Not owner");
        
        // Блокировка лицензии
        _lockLicense(licenseId);
        
        // Создание запроса на миграцию
        requestId = nextRequestId++;
        migrationRequests[requestId] = MigrationRequest({
            licenseId: licenseId,
            targetChain: targetChain,
            targetAddress: targetAddress,
            timestamp: block.timestamp,
            completed: false
        });
        
        emit MigrationInitiated(requestId, licenseId, targetChain, msg.sender);
        
        // Вызов межцепочечного моста
        _sendToTargetChain(requestId, licenseId, targetChain, targetAddress);
    }
}
```

### 2. Создание доказательства владения

```typescript
// Создание доказательства владения для миграции
interface OwnershipProof {
  licenseId: string;
  ownerId: string;
  sourceChain: string;
  targetChain: string;
  timestamp: number;
  signature: string;
  licenseData: any; // полные данные лицензии
  nonce: string;
}

function createOwnershipProof(
  licenseId: string,
  ownerId: string,
  sourceChain: string,
  targetChain: string
): OwnershipProof {
  const proof: OwnershipProof = {
    licenseId,
    ownerId,
    sourceChain,
    targetChain,
    timestamp: Date.now(),
    licenseData: getLicenseData(licenseId),
    nonce: generateNonce()
  };
  
  // Подпись доказательства владельцем
  proof.signature = signProof(proof, ownerId);
  
  return proof;
}
```

### 3. Межцепочечная передача

Система поддерживает различные межцепочечные решения:

#### LayerZero
```solidity
import "@layerzerolabs/solidity-examples/contracts/lzApp/NonblockingLzApp.sol";

contract Tur55LzBridge is NonblockingLzApp {
    function _sendToTargetChain(
        uint256 requestId,
        uint256 licenseId,
        string memory targetChain,
        address targetAddress
    ) internal {
        bytes memory payload = abi.encode(
            requestId,
            licenseId,
            msg.sender, // текущий владелец
            getLicenseData(licenseId), // данные лицензии
            block.timestamp
        );
        
        _lzSend(
            destinationChainId, // определяется по targetChain
            payload,
            payable(msg.sender),
            address(0x0), // refund address
            bytes("") // adapter params
        );
    }
    
    function _nonblockingLzReceive(
        uint16 _srcChainId,
        bytes memory _srcAddress,
        uint64 _nonce,
        bytes memory _payload
    ) internal override {
        // Обработка полученного сообщения в целевой цепочке
        (
            uint256 requestId,
            uint256 licenseId,
            address owner,
            bytes memory licenseData,
            uint256 timestamp
        ) = abi.decode(_payload, (uint256, uint256, address, bytes, uint256));
        
        _processMigration(requestId, licenseId, owner, licenseData, _srcChainId);
    }
}
```

#### Axelar
```solidity
import {AxelarExecutable} from "@axelar-network/axelar-gmp-sdk-solidity/contracts/AxelarExecutable.sol";
import {IAxelarGateway} from "@axelar-network/axelar-gmp-sdk-solidity/contracts/interfaces/IAxelarGateway.sol";
import {IAxelarGasService} from "@axelar-network/axelar-gmp-sdk-solidity/contracts/interfaces/IAxelarGasService.sol";

contract Tur55AxelarBridge is AxelarExecutable {
    function _sendToTargetChain(
        uint256 requestId,
        uint256 licenseId,
        string memory targetChain,
        address targetAddress
    ) internal {
        bytes memory payload = abi.encode(
            requestId,
            licenseId,
            msg.sender,
            getLicenseData(licenseId),
            block.timestamp
        );
        
        gateway.callContract(
            targetChain,
            address(this),
            payload
        );
    }
    
    function _execute(
        string calldata sourceChain,
        string calldata sourceAddress,
        bytes calldata payload
    ) external override {
        require(msg.sender == address(gateway), "Invalid caller");
        
        (
            uint256 requestId,
            uint256 licenseId,
            address owner,
            bytes memory licenseData,
            uint256 timestamp
        ) = abi.decode(payload, (uint256, uint256, address, bytes, uint256));
        
        _processMigration(requestId, licenseId, owner, licenseData);
    }
}
```

## Реализация для различных блокчейнов

### Ethereum → Polygon миграция

```solidity
// Контракт в Ethereum (исходная цепочка)
contract Tur55EthereumBridge {
    // Контракт лицензии
    ITur55License public licenseContract;
    
    function migrateToPolygon(uint256 licenseId, address targetAddress) external {
        // Проверка владения
        require(licenseContract.ownerOf(licenseId) == msg.sender, "Not owner");
        
        // Блокировка лицензии
        licenseContract.lockToken(licenseId);
        
        // Подготовка данных для миграции
        bytes memory licenseData = licenseContract.getTokenData(licenseId);
        
        // Вызов моста Polygon
        IPolygonBridge(polygonBridgeAddress).sendRequest(
            "polygon",
            abi.encode(msg.sender, licenseId, licenseData, block.timestamp)
        );
    }
}

// Контракт в Polygon (целевая цепочка)
contract Tur55PolygonBridge {
    ITur55License public licenseContract;
    
    function receiveFromEthereum(
        address originalOwner,
        uint256 originalLicenseId,
        bytes memory licenseData,
        uint256 timestamp
    ) external onlyFromEthereumBridge {
        // Создание новой лицензии в Polygon
        uint256 newLicenseId = licenseContract.mintWithCustomData(
            originalOwner,
            licenseData
        );
        
        emit MigrationCompleted(originalLicenseId, newLicenseId, originalOwner, "ethereum", "polygon");
    }
}
```

### Solana реализация

```rust
use anchor_lang::prelude::*;
use anchor_spl::token::{Token, TokenAccount, Mint, Transfer};
use std::str::FromStr;

#[account]
pub struct MigrationRequest {
    pub request_id: u64,
    pub license_token_account: Pubkey,
    pub original_owner: Pubkey,
    pub target_chain: String,
    pub target_address: String,
    pub license_data_hash: [u8; 32],
    pub timestamp: i64,
    pub completed: bool,
}

#[derive(Accounts)]
pub struct InitiateMigration<'info> {
    #[account(mut)]
    pub owner: Signer<'info>,
    #[account(mut)]
    pub license_token_account: Account<'info, TokenAccount>,
    #[account(init, payer = owner, space = 8 + 32)]
    pub migration_request: Account<'info, MigrationRequest>,
    pub system_program: Program<'info, System>,
    pub token_program: Program<'info, Token>,
}

impl<'info> InitiateMigration<'info> {
    pub fn execute(
        &mut self,
        request_id: u64,
        target_chain: String,
        target_address: String,
        license_data: Vec<u8>,
    ) -> Result<()> {
        // Проверка владения токеном
        require!(
            self.license_token_account.owner == self.owner.key(),
            MigrationError::InvalidOwner
        );
        
        // Блокировка токена (перевод в специальный аккаунт)
        let cpi_accounts = Transfer {
            from: self.license_token_account.to_account_info(),
            to: self.locked_tokens_account.to_account_info(),
            authority: self.owner.to_account_info(),
        };
        
        let cpi_program = self.token_program.to_account_info();
        let cpi_context = CpiContext::new(cpi_program, cpi_accounts);
        anchor_spl::token::transfer(cpi_context, self.license_token_account.amount)?;
        
        // Создание запроса на миграцию
        self.migration_request.request_id = request_id;
        self.migration_request.license_token_account = self.license_token_account.key();
        self.migration_request.original_owner = self.owner.key();
        self.migration_request.target_chain = target_chain;
        self.migration_request.target_address = target_address;
        self.migration_request.license_data_hash = 
            solana_program::keccak::hash(&license_data).to_bytes();
        self.migration_request.timestamp = Clock::get()?.unix_timestamp;
        self.migration_request.completed = false;
        
        Ok(())
    }
}

#[error_code]
pub enum MigrationError {
    #[msg("Invalid owner")]
    InvalidOwner,
    #[msg("Migration already completed")]
    AlreadyCompleted,
    #[msg("Invalid target chain")]
    InvalidTargetChain,
}
```

### Cosmos SDK реализация

```rust
use cosmwasm_std::{
    DepsMut, Env, MessageInfo, Response, StdResult, Addr, CosmosMsg, WasmMsg,
    to_binary, Attribute,
};
use cw_storage_plus::{Item, Map};
use schemars::JsonSchema;

#[derive(Serialize, Deserialize, Clone, Debug, PartialEq, JsonSchema)]
pub struct MigrationRequest {
    pub request_id: String,
    pub license_token_id: String,
    pub original_owner: Addr,
    pub target_chain: String,
    pub target_address: String,
    pub license_data_hash: String,
    pub timestamp: u64,
    pub completed: bool,
}

pub const MIGRATION_REQUESTS: Map<&str, MigrationRequest> = Map::new("migration_requests");
pub const NEXT_REQUEST_ID: Item<u64> = Item::new("next_request_id");

#[derive(Serialize, Deserialize, Clone, Debug, PartialEq, JsonSchema)]
pub struct MigrateMsg {
    pub target_chain: String,
    pub target_address: String,
    pub license_token_id: String,
}

pub fn execute_migrate(
    deps: DepsMut,
    env: Env,
    info: MessageInfo,
    msg: MigrateMsg,
) -> StdResult<Response> {
    // Проверка владения лицензией (предполагается отдельная проверка в CW721 контракте)
    
    let request_id = NEXT_REQUEST_ID.may_load(deps.storage)?.unwrap_or(0);
    NEXT_REQUEST_ID.save(deps.storage, &(request_id + 1))?;
    
    let license_data = get_license_data(deps.as_ref(), &msg.license_token_id)?;
    let license_data_hash = sha256_hash(&license_data);
    
    let migration_request = MigrationRequest {
        request_id: request_id.to_string(),
        license_token_id: msg.license_token_id.clone(),
        original_owner: info.sender.clone(),
        target_chain: msg.target_chain.clone(),
        target_address: msg.target_address.clone(),
        license_data_hash,
        timestamp: env.block.time.seconds(),
        completed: false,
    };
    
    MIGRATION_REQUESTS.save(deps.storage, &request_id.to_string(), &migration_request)?;
    
    // Блокировка лицензии
    let lock_msg = WasmMsg::Execute {
        contract_addr: env.contract.address.to_string(),
        msg: to_binary(&ExecuteMsg::LockLicense {
            token_id: msg.license_token_id,
        })?,
        funds: vec![],
    };
    
    // Вызов IBC для передачи в другую цепочку
    let ibc_msg = create_ibc_transfer_msg(
        &msg.target_chain,
        &msg.target_address,
        &migration_request,
    )?;
    
    let attributes = vec![
        Attribute::new("action", "initiate_migration"),
        Attribute::new("request_id", request_id.to_string()),
        Attribute::new("target_chain", msg.target_chain),
        Attribute::new("license_token_id", msg.license_token_id),
    ];
    
    Ok(Response::new()
        .add_message(lock_msg)
        .add_message(ibc_msg)
        .add_attributes(attributes))
}

fn create_ibc_transfer_msg(
    target_chain: &str,
    target_address: &str,
    migration_request: &MigrationRequest,
) -> StdResult<CosmosMsg> {
    // Создание IBC сообщения для передачи в целевую цепочку
    // Реализация зависит от конкретного IBC канала
    todo!("Implement IBC transfer logic")
}
```

## Обработка в целевой цепочке

### Получение и валидация

```typescript
class CrossChainReceiver {
  async processMigration(
    sourceChain: string,
    payload: MigrationPayload
  ): Promise<ProcessResult> {
    // 1. Валидация доказательства
    const isValid = await this.validateProof(payload);
    if (!isValid) {
      return { success: false, error: "Invalid proof" };
    }
    
    // 2. Проверка уникальности (защита от double spend)
    const isUnique = await this.checkUniqueness(payload.requestId, sourceChain);
    if (!isUnique) {
      return { success: false, error: "Duplicate request" };
    }
    
    // 3. Восстановление данных лицензии
    const licenseData = await this.reconstructLicenseData(payload);
    
    // 4. Создание новой лицензии в целевой цепочке
    const newLicenseId = await this.createLicenseInTargetChain(
      payload.owner,
      licenseData
    );
    
    // 5. Фиксация успешной миграции
    await this.recordMigration(payload.requestId, newLicenseId);
    
    return { 
      success: true, 
      newLicenseId,
      originalLicenseId: payload.licenseId 
    };
  }
  
  private async validateProof(payload: MigrationPayload): Promise<boolean> {
    // Проверка криптографической подписи
    // Проверка целостности данных
    // Проверка времени действия
    return true; // Заглушка
  }
  
  private async checkUniqueness(
    requestId: string, 
    sourceChain: string
  ): Promise<boolean> {
    // Проверка, что этот запрос не был обработан ранее
    const key = `${sourceChain}:${requestId}`;
    const record = await this.storage.get(key);
    return record === null;
  }
}
```

## Обратная миграция и отмена

Система поддерживает возможность отмены миграции в определенных случаях:

```solidity
contract Tur55MigrationManager {
    struct MigrationRecord {
        uint256 originalLicenseId;
        uint256 newLicenseId;
        string sourceChain;
        string targetChain;
        address owner;
        uint256 migrationTimestamp;
        bool isReversed;
    }
    
    mapping(bytes32 => MigrationRecord) public migrationRecords; // requestId -> record
    
    // Возможность отмены в течение 24 часов
    modifier withinReversalWindow(uint256 timestamp) {
        require(
            block.timestamp <= timestamp + 24 hours,
            "Reversal window expired"
        );
        _;
    }
    
    function reverseMigration(bytes32 requestId) 
        external 
        withinReversalWindow(migrationRecords[requestId].migrationTimestamp)
    {
        MigrationRecord storage record = migrationRecords[requestId];
        require(msg.sender == record.owner, "Not owner");
        require(!record.isReversed, "Already reversed");
        
        // Уничтожение лицензии в целевой цепочке
        // Разблокировка в исходной цепочке
        // ...
        
        record.isReversed = true;
    }
}
```

## Безопасность межцепочечной миграции

### Защита от атак

1. **Double spend защита**: Уникальные идентификаторы запросов
2. **Временные ограничения**: Ограниченное время действия запросов
3. **Криптографическая верификация**: Подтверждение владения
4. **Аудит операций**: Полная история миграций

### Контроль доступа

- Только владелец может инициировать миграцию
- Возможность делегирования прав
- Поддержка мультиподписи

## Мониторинг и отчетность

Система предоставляет полную прозрачность миграционных процессов:

```typescript
interface MigrationReport {
  totalMigrations: number;
 bySourceChain: Record<string, number>;
  byTargetChain: Record<string, number>;
  successRate: number;
  avgProcessingTime: number;
  volumeByToken: Record<string, number>;
}

class MigrationAnalytics {
  async generateReport(): Promise<MigrationReport> {
    // Сбор и анализ статистики миграций
    return {
      totalMigrations: await this.getTotalMigrations(),
      bySourceChain: await this.getMigrationsBySourceChain(),
      byTargetChain: await this.getMigrationsByTargetChain(),
      successRate: await this.getSuccessRate(),
      avgProcessingTime: await this.getAverageProcessingTime(),
      volumeByToken: await this.getVolumeByToken()
    };
 }
}
```

## Восстановление данных при миграции

При миграции важно сохранить все данные лицензии:

```typescript
interface MigrationDataBundle {
  tokenMetadata: any;           // Метаданные токена
  technicalSpecs: VehicleSpecs; // Технические характеристики
  accessControl: AccessControl; // Права доступа
  usageHistory: UsageRecord[];  // История использования
  royaltyInfo: RoyaltyInfo;     // Информация о роялти
  customData: Record<string, any>; // Пользовательские данные
  signature: string;            // Криптографическая подпись
}

function prepareMigrationBundle(
  licenseId: string,
  chain: string
): MigrationDataBundle {
  const licenseData = getLicenseData(licenseId, chain);
  
  return {
    tokenMetadata: licenseData.metadata,
    technicalSpecs: licenseData.techSpecs,
    accessControl: licenseData.accessControl,
    usageHistory: licenseData.usageHistory || [],
    royaltyInfo: licenseData.royalty,
    customData: licenseData.customData || {},
    signature: createDataSignature(licenseData)
  };
}
```

## Поддержка различных типов активов

Система поддерживает миграцию различных типов лицензий:

1. **Лицензии на транспортные средства**
2. **Лицензии на оборудование**
3. **Лицензии на недвижимость**
4. **Лицензии на интеллектуальную собственность**

Каждый тип имеет свои особенности миграции, но использует общую архитектуру.