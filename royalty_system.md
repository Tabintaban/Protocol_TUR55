# Система роялти TUR55 v2.0

## Общее описание

Система роялти в протоколе TUR55 v2.0 обеспечивает автоматические выплаты основателю протокола с каждой транзакции, связанной с использованием лицензий. Система универсальна и работает на различных блокчейнах с учетом их специфики.

## Общие принципы

### Процент роялти
- Базовый процент: 1% (100 basis points)
- Универсальный для всех блокчейнов
- Может быть настроен для разных типов операций

### Адрес получателя
- Универсальный адрес основателя
- Может отличаться на разных блокчейнах
- Поддержка мультиподписи для дополнительной безопасности

## Формат роялти в лицензии

```json
{
  "tur55License": {
    // ... другие поля ...
    "royalty": {
      "bps": 100,  // 1% в basis points
      "founderAddress": "multichain://ethereum:1/0xfounder_address",
      "paymentToken": "native",  // native или указанный токен
      "aggregationRequired": false  // требуется ли агрегация
    }
  }
}
```

## Реализация для различных блокчейнов

### Ethereum/EVM

```solidity
// Абстрактный контракт роялти
abstract contract UniversalRoyalty {
    // Адрес основателя (устанавливается в конструкторе конкретного контракта)
    address public immutable FOUNDER_ADDRESS;
    
    // Процент роялти (единый для всех цепей)
    uint256 public constant ROYALTY_BPS = 100; // 1%
    
    // Событие для отслеживания выплат
    event RoyaltyPaid(
        address indexed licenseOwner,
        address indexed founder,
        uint256 amount,
        uint256 indexed tokenId
    );

    constructor(address _founderAddress) {
        FOUNDER_ADDRESS = _founderAddress;
    }

    // Универсальная функция выплаты роялти
    function _payRoyalty(uint256 amount) internal virtual {
        uint256 royalty = (amount * ROYALTY_BPS) / 10000;
        
        if (royalty > 0) {
            (bool success, ) = FOUNDER_ADDRESS.call{value: royalty}("");
            require(success, "Royalty payment failed");
            emit RoyaltyPaid(msg.sender, FOUNDER_ADDRESS, royalty, 0); // tokenId может быть добавлен
        }
    }
    
    // Функция для выплаты роялти с указанием токена
    function _payRoyaltyWithToken(uint256 amount, uint256 tokenId) internal virtual {
        uint256 royalty = (amount * ROYALTY_BPS) / 10000;
        
        if (royalty > 0) {
            IERC20 token = IERC20(_getTokenAddress(tokenId));
            require(token.transfer(FOUNDER_ADDRESS, royalty), "Token transfer failed");
            emit RoyaltyPaid(msg.sender, FOUNDER_ADDRESS, royalty, tokenId);
        }
    
    function _getTokenAddress(uint256 tokenId) internal view virtual returns (address);
}
```

### Solana

```rust
use anchor_lang::prelude::*;
use anchor_spl::token::{Token, TokenAccount, Transfer, MintTo, Mint};
use anchor_spl::associated_token::AssociatedToken;

#[account]
pub struct RoyaltyInfo {
    pub founder_pubkey: Pubkey,
    pub royalty_bps: u16, // basis points (100 = 1%)
    pub collected_fees: u64, // собранные комиссии в lamports
}

#[derive(Accounts)]
pub struct PayRoyalty<'info> {
    #[account(mut)]
    pub payer: Signer<'info>,
    #[account(mut)]
    pub founder_account: SystemAccount<'info>,
    pub system_program: Program<'info, System>,
    /// CHECK: Проверяется в логике
    pub royalty_info: Account<'info, RoyaltyInfo>,
}

impl<'info> PayRoyalty<'info> {
    pub fn execute(&mut self, amount: u64) -> Result<()> {
        let royalty_amount = (amount as u128 * self.royalty_info.royalty_bps as u128 / 10000) as u64;
        
        if royalty_amount > 0 {
            // Перевод роялти основателю
            **self.payer.to_account_info().try_borrow_mut_lamports()? -= royalty_amount;
            **self.founder_account.to_account_info().try_borrow_mut_lamports()? += royalty_amount;
        }
        
        // Обновление статистики
        self.royalty_info.collected_fees += royalty_amount;
        
        Ok(())
    }
}
```

### Cosmos SDK (CosmWasm)

```rust
use cosmwasm_std::{DepsMut, Env, MessageInfo, Response, coins, Addr, CosmosMsg, BankMsg, Uint128};
use cw_storage_plus::Item;
use schemars::JsonSchema;

#[derive(Serialize, Deserialize, Clone, Debug, PartialEq, JsonSchema)]
pub struct RoyaltyInfo {
    pub founder_address: Addr,
    pub royalty_bps: u16, // basis points (100 = 1%)
    pub collected_fees: Uint128, // собранные комиссии
}

pub const ROYALTY_INFO: Item<RoyaltyInfo> = Item::new("royalty_info");

pub fn pay_royalty(
    deps: DepsMut,
    env: Env,
    info: MessageInfo,
    amount: Uint128,
) -> Result<Response, ContractError> {
    let mut royalty_info = ROYALTY_INFO.load(deps.storage)?;
    
    // Вычисление суммы роялти
    let royalty_amount = (amount.u128() * royalty_info.royalty_bps as u128 / 1000) as u128;
    
    if royalty_amount > 0 {
        let royalty_msg = CosmosMsg::Bank(BankMsg::Send {
            to_address: royalty_info.founder_address.to_string(),
            amount: coins(royalty_amount, env.contract_info().denom),
        });
        
        // Обновление статистики
        royalty_info.collected_fees = royalty_info.collected_fees.checked_add(Uint128::from(royalty_amount))?;
        ROYALTY_INFO.save(deps.storage, &royalty_info)?;
        
        Ok(Response::new()
            .add_message(royalty_msg)
            .add_attribute("action", "pay_royalty")
            .add_attribute("amount", royalty_amount.to_string()))
    } else {
        Ok(Response::new()
            .add_attribute("action", "no_royalty"))
    }
}
```

### Polkadot (Substrate)

```rust
use frame_support::{
    decl_module, decl_storage, decl_event, dispatch, traits::{Get, Currency, ExistenceRequirement}
};
use frame_system::{self as system, ensure_signed};
use sp_runtime::traits::StaticLookup;

pub trait Config: frame_system::Config {
    type Event: From<Event<Self>> + Into<<Self as frame_system::Config>::Event>;
    type Currency: Currency<Self::AccountId>;
    type FounderAccountId: Get<Self::AccountId>;
    type RoyaltyBasisPoints: Get<u16>; // 10 = 1%
}

decl_storage! {
    trait Store for Module<T: Config> as RoyaltyModule {
        CollectedFees get(fn collected_fees): map hasher(blackblake2) T::AccountId => BalanceOf<T>;
    }
}

decl_event! {
    pub enum Event<T> where
        AccountId = <T as frame_system::Config>::AccountId,
        Balance = BalanceOf<T>,
    {
        RoyaltyPaid(AccountId, AccountId, Balance),
    }
}

decl_module! {
    pub struct Module<T: Config> for enum Call where origin: T::Origin {
        fn deposit_event() = default;

        pub fn pay_royalty(origin, amount: BalanceOf<T>) -> dispatch::DispatchResult {
            let _ = ensure_signed(origin)?;

            let founder_account = T::FounderAccountId::get();
            let royalty_basis_points = T::RoyaltyBasisPoints::get();
            
            let royalty_amount = (amount * royalty_basis_points.into()) / 10000u32.into();
            
            if royalty_amount > Zero::zero() {
                T::Currency::transfer(
                    &Self::account_id(),
                    &founder_account,
                    royalty_amount,
                    ExistenceRequirement::KeepAlive,
                )?;

                <CollectedFees<T>>::mutate(&founder_account, |fees| *fees += royalty_amount);
                
                Self::deposit_event(RawEvent::RoyaltyPaid(
                    Self::account_id(),
                    founder_account,
                    royalty_amount,
                ));
            }

            Ok(())
        }
    }
}
```

## Межцепочечная агрегация роялти

Для блокчейнов с низкой стоимостью транзакций может быть эффективнее агрегировать роялти и выводить их периодически:

```typescript
// Сервис агрегации роялти со всех цепей
class RoyaltyAggregator {
  private chains: ChainAdapter[] = [];
  
  async aggregateRoyalties(founderAddress: string): Promise<RoyaltyReport> {
    const reports = await Promise.all(
      this.chains.map(chain => this.getChainRoyalties(chain, founderAddress))
    );
    
    const total = reports.reduce((sum, r) => sum + r.total, 0);
    
    return {
      total,
      byChain: reports,
      timestamp: Date.now(),
      withdrawalAddress: founderAddress
    };
  }

  async processWithdrawal(report: RoyaltyReport): Promise<WithdrawalResult> {
    // Обработка вывода средств с учетом особенностей каждой цепи
    const results = await Promise.all(
      report.byChain.map(chainReport => 
        this.processChainWithdrawal(chainReport)
      )
    );
    
    return {
      success: results.every(r => r.success),
      results,
      totalAmount: report.total
    };
  }

  private async processChainWithdrawal(chainReport: ChainRoyaltyReport): Promise<WithdrawalResult> {
    // Логика вывода для конкретной цепи
    // Учитывает минимальные суммы, комиссии и т.д.
    return { success: true, amount: chainReport.total, chain: chainReport.chain };
  }
}
```

## Управление роялти

### Изменение процента роялти
- Возможность изменения через управляемый процесс
- Может различаться для разных типов лицензий
- Уведомление всех участников экосистемы

### Многоадресный получатель
- Поддержка мультиподписи для получения роялти
- Возможность распределения между несколькими адресами
- Поддержка DAO для управления доходами

### Отчетность
- Прозрачная отчетность по выплатам
- Возможность аудита
- Интеграция с аналитическими системами

## Безопасность системы роялти

### Защита от обхода
- Интеграция в основные операции протокола
- Контроль на уровне смарт-контрактов
- Невозможность создания лицензий без роялти

### Аудит и прозрачность
- Публичные отчеты о выплатах
- Возможность проверки сумм
- История всех транзакций

## Интеграция с сессионными файлами

Система роялти может также учитывать использование в сессионных файлах:

```json
{
  "protocol": "TUR55 v2.0",
  "license": {
    // ... основная информация ...
    "royaltyPayment": {
      "amount": 100000, // в минимальных единицах
      "currency": "native",
      "paymentProof": "multichain://ethereum:1/tx/0xabc..."
    }
  }
  // ... остальные поля сессии ...
}
```

## Статистика и аналитика

Система собирает статистику по роялти для аналитики:

1. Общая сумма собранных роялти
2. Распределение по блокчейнам
3. Распределение по типам лицензий
4. Временные тренды
5. Эффективность разных цепей