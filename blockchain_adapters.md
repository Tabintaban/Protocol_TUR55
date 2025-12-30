# Архитектура адаптеров блокчейнов TUR55 v2.0

## Общее описание

Архитектура адаптеров блокчейнов обеспечивает блокчейн-агностичность протокола TUR55 v2.0, позволяя ему работать на различных блокчейнах с единым интерфейсом и форматами данных.

## Универсальный интерфейс адаптера

```typescript
interface IBlockchainAdapter {
  // Основные операции с лицензиями
  mintLicense(owner: string, techSpecs: TechSpecs): Promise<LicenseId>;
  verifyLicense(licenseId: LicenseId, user: string): Promise<boolean>;
  getLicenseData(licenseId: LicenseId): Promise<LicenseData>;
  transferLicense(licenseId: LicenseId, newOwner: string): Promise<Transaction>;
  burnLicense(licenseId: LicenseId): Promise<Transaction>;

  // Операции с роялти
  payRoyalty(amount: number, royaltyAddress: string): Promise<Transaction>;
  getRoyaltyInfo(licenseId: LicenseId): Promise<RoyaltyInfo>;

  // Вспомогательные функции
 getChainInfo(): ChainInfo;
  validateAddress(address: string): boolean;
  signMessage(message: string, privateKey: string): string;
  verifySignature(message: string, signature: string, publicKey: string): boolean;
}

interface TechSpecs {
  identification: {
    vin: string;
    plate: string;
    make: string;
    model: string;
    year: number;
  };
  technical: {
    engine: {
      type: string;
      displacement: string;
      powerHP: number;
      torqueNM: number;
    };
    fuel: {
      tankCapacityL: number;
      type: string[];
      consumptionCityL100: number;
      consumptionHighwayL100: number;
    };
    electronics: {
      canBusVersion: string;
      obdProtocol: string;
      supportedPIDs: string[];
    };
  };
 safety: {
    maxSpeedKPH: number;
    supportedADAS: string[];
    tirePressureMonitoring: boolean;
  };
  cryptography: {
    publicKey: string;
    keyAlgo: string;
    keyStorage: string;
  };
}

interface LicenseData {
  id: LicenseId;
  owner: string;
  techSpecs: TechSpecs;
  metadataURI: string;
  signature: string;
  createdAt: Date;
  updatedAt: Date;
}
```

## Реализации адаптеров

### Ethereum/EVM адаптер

```typescript
class EthereumAdapter implements IBlockchainAdapter {
  private provider: ethers.providers.Provider;
  private signer?: ethers.Signer;
  private licenseContract: ethers.Contract;
  private readonly chainId: number;

  constructor(providerUrl: string, contractAddress: string, chainId: number) {
    this.provider = new ethers.providers.JsonRpcProvider(providerUrl);
    this.licenseContract = new ethers.Contract(contractAddress, ABI, this.provider);
    this.chainId = chainId;
  }

  async mintLicense(owner: string, techSpecs: TechSpecs): Promise<LicenseId> {
    if (!this.signer) throw new Error("Signer not configured");

    // Подготовка данных лицензии
    const techSpecsHash = ethers.utils.keccak256(
      ethers.utils.toUtf8Bytes(JSON.stringify(techSpecs))
    );

    // Хранение полных данных во внечейн хранилище (IPFS)
    const ipfsHash = await this.uploadToIPFS(techSpecs);
    
    const tx = await this.licenseContract.connect(this.signer).mintWithTechSpecs(
      owner,
      techSpecs.vin,
      techSpecs.maxFuelLiters,
      techSpecs.engineType,
      `ipfs://${ipfsHash}`
    );

    const receipt = await tx.wait();
    const tokenId = this.extractTokenId(receipt);
    
    return `tur55://ethereum:${this.chainId}/${contractAddress}/${tokenId}`;
  }

  async verifyLicense(licenseId: LicenseId, user: string): Promise<boolean> {
    const parsedId = this.parseLicenseId(licenseId);
    if (!parsedId) return false;

    try {
      const owner = await this.licenseContract.ownerOf(parsedId.tokenId);
      return owner.toLowerCase() === user.toLowerCase();
    } catch (error) {
      console.error("License verification failed:", error);
      return false;
    }
  }

  async getLicenseData(licenseId: LicenseId): Promise<LicenseData> {
    const parsedId = this.parseLicenseId(licenseId);
    if (!parsedId) throw new Error("Invalid license ID");

    // Получение базовой информации из контракта
    const [owner, metadataURI] = await Promise.all([
      this.licenseContract.ownerOf(parsedId.tokenId),
      this.licenseContract.tokenURI(parsedId.tokenId)
    ]);

    // Загрузка полных технических спецификаций из IPFS
    let techSpecs: TechSpecs;
    if (metadataURI.startsWith("ipfs://")) {
      const ipfsHash = metadataURI.substring(7);
      techSpecs = await this.downloadFromIPFS(ipfsHash);
    } else {
      // Если данные вложены в URI
      techSpecs = JSON.parse(metadataURI);
    }

    return {
      id: licenseId,
      owner,
      techSpecs,
      metadataURI,
      signature: "", // Подпись может быть добавлена в зависимости от реализации
      createdAt: new Date(), // Дата может быть получена из событий контракта
      updatedAt: new Date()
    };
 }

  private parseLicenseId(licenseId: string): { tokenId: string; contractAddress: string } | null {
    const match = licenseId.match(/tur55:\/\/ethereum:(\d+)\/(0x[a-fA-F0-9]{40})\/(\d+)/);
    if (!match) return null;
    
    return {
      contractAddress: match[2],
      tokenId: match[3]
    };
  }

  // Другие методы реализации...
}
```

### Solana адаптер

```typescript
import { Connection, Keypair, PublicKey, Transaction, sendAndConfirmTransaction } from '@solana/web3.js';
import { Program, Provider, AnchorProvider, BN } from '@project-serum/anchor';
import { IDL, Tur55License } from './idl/tur55_license';

class SolanaAdapter implements IBlockchainAdapter {
  private connection: Connection;
  private program: Program<Tur55License>;
  private provider: Provider;
  private readonly cluster: string;

  constructor(rpcUrl: string, programId: string, cluster: string = 'mainnet-beta') {
    this.connection = new Connection(rpcUrl);
    this.cluster = cluster;
    
    const wallet = new Wallet(Keypair.generate()); // В реальности должен быть предоставлен
    this.provider = new AnchorProvider(this.connection, wallet, {
      commitment: 'confirmed',
    });
    
    this.program = new Program(IDL, programId, this.provider);
  }

  async mintLicense(owner: string, techSpecs: TechSpecs): Promise<LicenseId> {
    const ownerPublicKey = new PublicKey(owner);
    const licenseKeypair = Keypair.generate();

    // Подготовка данных для аккаунта лицензии
    const techSpecsBuffer = Buffer.from(JSON.stringify(techSpecs));

    try {
      const tx = await this.program.methods
        .mintLicense(
          techSpecs.vin,
          techSpecs.maxFuelLiters,
          techSpecs.engineType,
          techSpecsBuffer
        )
        .accounts({
          license: licenseKeypair.publicKey,
          owner: ownerPublicKey,
          systemProgram: SystemProgram.programId,
        })
        .signers([licenseKeypair])
        .rpc();

      const licenseId = `tur55://solana:${this.cluster}/${licenseKeypair.publicKey.toBase58()}`;
      return licenseId;
    } catch (error) {
      console.error("Mint license failed:", error);
      throw error;
    }
  }

  async verifyLicense(licenseId: LicenseId, user: string): Promise<boolean> {
    try {
      const parsedId = this.parseLicenseId(licenseId);
      if (!parsedId) return false;

      const licenseAccount = await this.program.account.license.fetch(parsedId.publicKey);
      return licenseAccount.owner.equals(new PublicKey(user));
    } catch (error) {
      console.error("License verification failed:", error);
      return false;
    }
  }

  async getLicenseData(licenseId: LicenseId): Promise<LicenseData> {
    const parsedId = this.parseLicenseId(licenseId);
    if (!parsedId) throw new Error("Invalid license ID");

    const licenseAccount = await this.program.account.license.fetch(parsedId.publicKey);
    
    // Десериализация технических спецификаций
    const techSpecs: TechSpecs = JSON.parse(licenseAccount.techSpecs.toString());

    return {
      id: licenseId,
      owner: licenseAccount.owner.toBase58(),
      techSpecs,
      metadataURI: "", // В Solana может использоваться другой механизм
      signature: "", // Подпись в Solana системе
      createdAt: new Date(licenseAccount.createdAt.toNumber() * 1000),
      updatedAt: new Date(licenseAccount.updatedAt.toNumber() * 100)
    };
  }

  private parseLicenseId(licenseId: string): { publicKey: PublicKey } | null {
    const match = licenseId.match(/tur55:\/\/solana:(mainnet-beta|devnet|testnet)\/([1-9A-HJ-NP-Za-km-z]{32,44})/);
    if (!match) return null;

    try {
      const publicKey = new PublicKey(match[2]);
      return { publicKey };
    } catch (error) {
      return null;
    }
  }

  // Другие методы реализации...
}
```

### Cosmos SDK адаптер

```typescript
import { SigningCosmWasmClient, CosmWasmClient } from "@cosmjs/cosmwasm-stargate";
import { DirectSecp256k1HdWallet } from "@cosmjs/proto-signing";
import { GasPrice, calculateFee } from "@cosmjs/stargate";

class CosmosAdapter implements IBlockchainAdapter {
  private client: CosmWasmClient | SigningCosmWasmClient;
  private contractAddress: string;
  private readonly chainId: string;
  private wallet?: DirectSecp256k1HdWallet;

  constructor(rpcUrl: string, contractAddress: string, chainId: string) {
    this.contractAddress = contractAddress;
    this.chainId = chainId;
    // Инициализация клиента
    this.client = CosmWasmClient.connect(rpcUrl);
  }

  async mintLicense(owner: string, techSpecs: TechSpecs): Promise<LicenseId> {
    if (!this.wallet) throw new Error("Wallet not configured");

    const signingClient = this.client as SigningCosmWasmClient;
    
    // Подготовка сообщения для CosmWasm контракта
    const msg = {
      mint_license: {
        owner,
        vehicle_pubkey: techSpecs.cryptography.publicKey,
        tech_specs: techSpecs,
        metadata_uri: await this.uploadToIPFS(techSpecs)
      }
    };

    const fee = calculateFee(20000, GasPrice.fromString("0.025ucosm"));
    
    const result = await signingClient.execute(
      (await this.wallet.getAccounts())[0].address,
      this.contractAddress,
      msg,
      fee
    );

    // Извлечение ID лицензии из результата
    const licenseId = this.extractLicenseIdFromResponse(result);
    return `tur55://cosmos:${this.chainId}/${this.contractAddress}/token/${licenseId}`;
  }

  async verifyLicense(licenseId: LicenseId, user: string): Promise<boolean> {
    try {
      const parsedId = this.parseLicenseId(licenseId);
      if (!parsedId) return false;

      // Вызов query метода контракта для проверки владельца
      const response: any = await (this.client as CosmWasmClient).queryContractSmart(
        this.contractAddress,
        {
          license_info: { token_id: parsedId.tokenId }
        }
      );

      return response.owner === user;
    } catch (error) {
      console.error("License verification failed:", error);
      return false;
    }
  }

  async getLicenseData(licenseId: LicenseId): Promise<LicenseData> {
    const parsedId = this.parseLicenseId(licenseId);
    if (!parsedId) throw new Error("Invalid license ID");

    const response: any = await (this.client as CosmWasmClient).queryContractSmart(
      this.contractAddress,
      {
        license_info: { token_id: parsedId.tokenId }
      }
    );

    // Загрузка полных спецификаций из IPFS или другого источника
    let techSpecs: TechSpecs;
    if (response.metadata_uri?.startsWith("ipfs://")) {
      techSpecs = await this.downloadFromIPFS(response.metadata_uri.substring(7));
    } else {
      techSpecs = response.tech_specs || {};
    }

    return {
      id: licenseId,
      owner: response.owner,
      techSpecs,
      metadataURI: response.metadata_uri,
      signature: response.signature || "",
      createdAt: new Date(response.created_at || Date.now()),
      updatedAt: new Date(response.updated_at || Date.now())
    };
 }

  private parseLicenseId(licenseId: string): { tokenId: string } | null {
    const match = licenseId.match(/tur55:\/\/cosmos:([a-zA-Z0-9-]+)\/([a-z0-9]+)\/token\/([a-zA-Z0-9-]+)/);
    if (!match) return null;

    return { tokenId: match[3] };
  }

  // Другие методы реализации...
}
```

## Менеджер адаптеров

```typescript
class AdapterManager {
  private adapters: Map<string, IBlockchainAdapter> = new Map();
 private defaultAdapter: IBlockchainAdapter | null = null;

  registerAdapter(chainName: string, adapter: IBlockchainAdapter): void {
    this.adapters.set(chainName, adapter);
  }

  getAdapterForLicense(licenseId: string): IBlockchainAdapter | null {
    const chainName = this.extractChainName(licenseId);
    return this.adapters.get(chainName) || null;
  }

  setDefaultAdapter(adapter: IBlockchainAdapter): void {
    this.defaultAdapter = adapter;
 }

  getDefaultAdapter(): IBlockchainAdapter | null {
    return this.defaultAdapter;
  }

  private extractChainName(licenseId: string): string {
    // Извлечение названия цепи из универсального ID
    const match = licenseId.match(/tur55:\/\/([^:\/]+)/);
    return match ? match[1] : '';
  }

  // Универсальные методы для работы с любым адаптером
 async universalMintLicense(
    chainName: string, 
    owner: string, 
    techSpecs: TechSpecs
  ): Promise<LicenseId> {
    const adapter = this.adapters.get(chainName);
    if (!adapter) throw new Error(`Adapter for ${chainName} not found`);
    
    return await adapter.mintLicense(owner, techSpecs);
  }

  async universalVerifyLicense(licenseId: string, user: string): Promise<boolean> {
    const adapter = this.getAdapterForLicense(licenseId);
    if (!adapter) throw new Error(`No adapter found for license: ${licenseId}`);
    
    return await adapter.verifyLicense(licenseId, user);
  }
}
```

## Универсальные функции для всех адаптеров

### Валидация адресов

```typescript
class AddressValidator {
  static validateEthereumAddress(address: string): boolean {
    return /^0x[a-fA-F0-9]{40}$/.test(address);
  }

  static validateSolanaAddress(address: string): boolean {
    // Проверка длины и допустимых символов для Solana адреса
    return /^[1-9A-HJ-NP-Za-km-z]{32,4}$/.test(address);
  }

  static validateCosmosAddress(address: string): boolean {
    // Проверка Cosmos адреса (обычно начинается с префикса, например, "cosmos1")
    return /^cosmos1[0-9a-z]{38}$/.test(address);
  }

  static validateAddress(address: string, chainType: string): boolean {
    switch (chainType) {
      case 'ethereum':
      case 'polygon':
      case 'bsc':
        return this.validateEthereumAddress(address);
      case 'solana':
        return this.validateSolanaAddress(address);
      case 'cosmos':
      case 'juno':
      case 'osmosis':
        return this.validateCosmosAddress(address);
      default:
        throw new Error(`Unknown chain type: ${chainType}`);
    }
  }
}
```

### Универсальная система идентификаторов

```typescript
class UniversalIdentifier {
  static createLicenseId(chain: string, chainId: string, address: string, tokenId: string): string {
    return `tur55://${chain}/${chainId}/${address}/token/${tokenId}`;
  }

  static parseLicenseId(id: string): { 
    chain: string; 
    chainId: string; 
    address: string; 
    tokenId: string 
  } | null {
    const match = id.match(/tur55:\/\/([^\/]+)\/([^\/]+)\/([^\/]+)\/token\/(.+)/);
    if (!match) return null;

    return {
      chain: match[1],
      chainId: match[2],
      address: match[3],
      tokenId: match[4]
    };
  }

  static isValidLicenseId(id: string): boolean {
    return this.parseLicenseId(id) !== null;
  }
}
```

## Стратегии обработки ошибок

### Обработка специфичных для цепи ошибок

```typescript
interface ChainSpecificError {
  chain: string;
  code: string | number;
  message: string;
  retryable: boolean;
}

class ErrorHandlingStrategy {
  static handleEthereumError(error: any): ChainSpecificError {
    if (error.code === 'INSUFFICIENT_FUNDS') {
      return {
        chain: 'ethereum',
        code: error.code,
        message: 'Insufficient funds for gas',
        retryable: false
      };
    } else if (error.code === -32000) {
      return {
        chain: 'ethereum',
        code: error.code,
        message: error.message,
        retryable: true
      };
    }
    
    return {
      chain: 'ethereum',
      code: error.code || 'UNKNOWN_ERROR',
      message: error.message,
      retryable: true
    };
 }

  static handleSolanaError(error: any): ChainSpecificError {
    if (error.message?.includes('AccountNotFound')) {
      return {
        chain: 'solana',
        code: 'ACCOUNT_NOT_FOUND',
        message: 'Account does not exist',
        retryable: false
      };
    }
    
    return {
      chain: 'solana',
      code: error.code || 'UNKNOWN_ERROR',
      message: error.message,
      retryable: true
    };
  }

  static handleCosmosError(error: any): ChainSpecificError {
    if (error.message?.includes('out of gas')) {
      return {
        chain: 'cosmos',
        code: 'OUT_OF_GAS',
        message: 'Transaction ran out of gas',
        retryable: true
      };
    }
    
    return {
      chain: 'cosmos',
      code: error.code || 'UNKNOWN_ERROR',
      message: error.message,
      retryable: true
    };
  }
}
```

## Тестирование адаптеров

### Универсальный тестовый фреймворк

```typescript
class AdapterTestSuite {
  static async runAllTests(adapter: IBlockchainAdapter, chainName: string): Promise<TestResult[]> {
    const results: TestResult[] = [];

    // Тест создания лицензии
    results.push(await this.testMintLicense(adapter, chainName));
    
    // Тест проверки лицензии
    results.push(await this.testVerifyLicense(adapter, chainName));
    
    // Тест получения данных лицензии
    results.push(await this.testGetLicenseData(adapter, chainName));
    
    // Тест роялти
    results.push(await this.testRoyaltyOperations(adapter, chainName));

    return results;
 }

  private static async testMintLicense(adapter: IBlockchainAdapter, chainName: string): Promise<TestResult> {
    try {
      const mockTechSpecs: TechSpecs = {
        identification: {
          vin: "TEST123456789",
          plate: "TEST001",
          make: "Test",
          model: "Model",
          year: 2023
        },
        technical: {
          engine: {
            type: "electric",
            displacement: "0L",
            powerHP: 200,
            torqueNM: 300
          },
          fuel: {
            tankCapacityL: 0,
            type: ["electric"],
            consumptionCityL100: 0,
            consumptionHighwayL100: 0
          },
          electronics: {
            canBusVersion: "2.0",
            obdProtocol: "ISO15765-4",
            supportedPIDs: ["0105", "010C", "010D"]
          }
        },
        safety: {
          maxSpeedKPH: 200,
          supportedADAS: ["ABS", "ESP"],
          tirePressureMonitoring: true
        },
        cryptography: {
          publicKey: "test_public_key",
          keyAlgo: "Ed25519",
          keyStorage: "HSM"
        }
      };

      const licenseId = await adapter.mintLicense("test_owner_address", mockTechSpecs);
      
      return {
        testName: "Mint License",
        passed: UniversalIdentifier.isValidLicenseId(licenseId),
        details: `Created license: ${licenseId}`
      };
    } catch (error) {
      return {
        testName: "Mint License",
        passed: false,
        details: `Error: ${error.message}`
      };
    }
  }

  // Другие тесты...
}

interface TestResult {
  testName: string;
  passed: boolean;
  details: string;
}
```

## Мониторинг и логирование

### Универсальная система логирования

```typescript
class UniversalLogger {
  private static instance: UniversalLogger;
  private loggers: Map<string, Logger> = new Map();

  static getInstance(): UniversalLogger {
    if (!UniversalLogger.instance) {
      UniversalLogger.instance = new UniversalLogger();
    }
    return UniversalLogger.instance;
  }

  addLogger(chainName: string, logger: Logger): void {
    this.loggers.set(chainName, logger);
  }

  log(chainName: string, level: LogLevel, message: string, data?: any): void {
    const logger = this.loggers.get(chainName);
    if (logger) {
      logger.log(level, message, data);
    } else {
      console.log(`[${chainName}] ${level}: ${message}`, data);
    }
  }

 error(chainName: string, message: string, error?: any): void {
    this.log(chainName, LogLevel.ERROR, message, error);
  }

  info(chainName: string, message: string, data?: any): void {
    this.log(chainName, LogLevel.INFO, message, data);
  }
}

enum LogLevel {
  DEBUG = "debug",
  INFO = "info",
  WARN = "warn",
  ERROR = "error"
}

interface Logger {
  log(level: LogLevel, message: string, data?: any): void;
}
```

## Заключение

Архитектура адаптеров блокчейнов TUR55 v2.0 обеспечивает:
- Единый интерфейс для работы с различными блокчейнами
- Простое добавление новых блокчейнов
- Сохранение целостности данных между цепочками
- Универсальные форматы и идентификаторы
- Единые механизмы безопасности и валидации