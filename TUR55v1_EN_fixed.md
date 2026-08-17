**STANDARD TUR55**

**Trusted Unified Regulation (TUR55) for executing blockchain commands
on IoT devices.**

**Version: v1.0**

**Status: Fixed**

**Date of fixation (UTC): 10.04.2026**

**Author**

-   **Alias/Identifier:** \_NOMOS-H\_

-   **Public Key Fingerprint:**
    2D0C4604CCF007C1AFDDADBBDBB49B9C183EB014\_\_

**Content Integrity**

-   **Document Hash (SHA-256):**
    **deca5cc3e52b9f87988e4062ad5f869ff744497f2197f4a144b9a2be9b8bacbd**\_

-   **IPFS CID:** **QmPsFiRb7wtxzsDD8QsZwRDzwW4iUSk94V3sh7MmBkfUEH**\_\_

-   **Blockchain Anchor (tx / block):** **TUR55v1_EN.docx.ots**\_

**TUR55 is an open standard for transmitting commands from blockchain to
IoT devices. It solves the problem of the lack of a unified protocol for
executors (analogous to oracles, but for execution)**

**1. Scope**

This standard describes a unified protocol for transmitting and
executing commands from smart contracts to resource-constrained devices
(ESP32, STM32, Raspberry Pi Pico, and others) running MicroPython.

The protocol provides:

\- blockchain event monitoring;

\- formation of an encrypted executable package;

\- package delivery via any communication channels;

\- secure script execution in an isolated environment (sandbox);

\- optional execution confirmation.

**2. TUR55 Protocol Layers**

The standard architecture is divided into four logical layers, similar
to the OSI/TCP/IP model.

**Layer 1: Event Layer (Blockchain Event Layer)**

Responsible for recording the occurrence of a condition in the smart
contract.

\- The smart contract generates an event when a condition occurs
(payment, expiration, DAO voting, etc.).

\- The event contains:

\- \`user_wallet\` --- payer or initiator address;

\- \`amount\` --- amount or limit (optional);

\- \`expires\` --- command validity time;

\- \`device_id\` --- target device identifier;

\- \`metadata\` --- additional data (action type, parameters).

**Layer 2: Executor Layer (Executor Layer)**

A component (service, bot, relay) that listens to the blockchain and
forms an executable package.

\- Subscribes to smart contract events.

\- Extracts data and forms a JSON envelope.

\- Embeds data into a Python script template.

\- Encrypts the resulting script.

\- Transfers the encrypted package to the delivery layer.

**Layer 3: Delivery Layer (Delivery Layer)**

Responsible for transferring the package to the target device.

\- Supported channels:

\- MQTT;

\- HTTP/HTTPS;

\- Bluetooth (BLE);

\- LoRaWAN;

\- QR-code / NFC;

\- physical media (SD card, USB).

\- The package is delivered as a binary file or base64 string.

**Layer 4: Execution Layer (Execution Layer)**

Executed on the IoT device in a secure environment.

\- Package decryption using hardware or embedded key.

\- Checking the validity period (\`expires\`).

\- Parsing JSON into a Python script.

\- Script execution in a sandbox with time, memory, and system access
restrictions.

\- Optional sending of execution confirmation.

**3. Data Formats**

**3.1. Smart Contract Event (EVM example)**

\`\`\`solidity

event ExecutorTriggered(

address indexed userWallet,

uint256 amount,

uint256 expires,

bytes32 deviceId,

bytes metadata

);

\`\`\`

**3.2. JSON Envelope (formed by executor)**

\`\`\`json

{

\"tur55_version\": \"1.0\",

\"command_id\": \"0x7e5f8a3c\...\",

\"device_id\": \"esp32_001\",

\"source\": {

\"smart_contract\": \"0x1234\...\",

\"transaction_hash\": \"0xabcd\...\",

\"block_number\": 12345678

},

\"execution\": {

\"user_wallet\": \"0xUserWallet\...\",

\"amount\": 50.5,

\"expires\": 1741500000

},

\"custom_data\": {

\"action\": \"unlock\",

\"duration\": 30

}

}

\`\`\`

**3.3. Python Script Template with Data Substitution**

\`\`\`python

\# === TUR55 EXECUTION CONTEXT ===

import machine

import time

\# Data from blockchain (substituted by executor)

user_wallet = \"\$USER_WALLET\"

amount = \$AMOUNT

expires = \$EXPIRES

custom_data = \$CUSTOM_JSON

\# === USER CODE ===

\# (written by device developer)

def main():

print(f\"Executing for {user_wallet}, amount: {amount}\")

if custom_data.get(\"action\") == \"unlock\":

machine.Pin(2, machine.Pin.OUT).value(1)

time.sleep(custom_data.get(\"duration\", 30))

machine.Pin(2).value(0)

if \_\_name\_\_ == \"\_\_main\_\_\":

main()

\`\`\`

**3.4. Encrypted Package (structure)**

\`\`\`

\[HEADER: TUR55v1\]

\[ENCRYPTED_SESSION_KEY\] (encrypted with device public key)

\[ENCRYPTED_SCRIPT\] (AES-256-GCM)

\[SIGNATURE\] (ECDSA / Ed25519 signature executor)

\`\`\`

**4. Cryptographic Requirements**

  ------------------------------------------------------------------------
  Function               Algorithm               Purpose
  ---------------------- ----------------------- -------------------------
  Script Encryption      AES-256-GCM             Command Confidentiality

  Session Key Encryption ECIES or RSA-OAEP       Key Transfer to Device

  Package Signature      Ed25519 or ECDSA        Source Authentication
                         secp256k1               

  Hashing                SHA3-256 or Blake2s     Integrity

  Device Key             Stored in secure memory Device Identification
                         (eFuse, Secure Element) 
  ------------------------------------------------------------------------

**5. TUR55 Command Lifecycle**

1\. **Event Occurrence** in the smart contract (payment, license
expiration, voting, etc.).

2\. **Executor** (service) receives the event, forms JSON, embeds data
into Python script.

3\. **Encryption** of script and session key, package signing.

4\. **Delivery** of package to device via any available channel (MQTT,
BLE, QR, LoRa).

5\. **Device** receives the package, verifies signature, decrypts,
checks \`expires\`.

6\. **Execution** of script in sandbox:

\- execution time limit (timeout);

\- memory limit;

\- prohibition of unsafe operations (file system, network, unless
permitted).

7\. **Optionally**: the device forms a confirmation, signs it, and sends
it back to the executor or directly to the blockchain.

**6. Security and Isolation (Sandbox)**

\- **Forbidden MicroPython Modules**: \`os\`, \`socket\`, \`urandom\`
(if not required).

\- **Limitations**:

\- Maximum execution time: 5--30 seconds (configurable).

\- Maximum memory: 16--64 KB (depending on device).

\- GPIO access only to predefined pins.

\- **Hardware Protection** (recommended):

\- Using Secure Element (ATECC608, SE050) for key storage.

\- Hardware AES acceleration.

**7. Compatibility and Transports**

TUR55 is not tied to a specific blockchain or communication channel.
Supported:

\- **Blockchains**: Ethereum, Polygon, BSC, IOTA (Tangle), Solana,
Cosmos and others.

\- **Transports**: MQTT, HTTP, BLE, LoRaWAN, QR, NFC, physical media.

**8. Usage Example: Software License**

1\. User pays for subscription in the smart contract.

2\. Executor forms a script that disables API access after the
expiration date.

3\. Encrypted package is delivered to the MicroPython server.

4\. Server executes the script in sandbox, blocking the API key.

5\. Optionally sends confirmation to the blockchain.

**9. TUR55 Compliance Levels**

  -----------------------------------------------------------------------
  Level                               Capabilities
  ----------------------------------- -----------------------------------
  TUR55/Bronze                        Package reception, decryption,
                                      execution, UART log

  TUR55/Silver                        \+ Result signature, sending via
                                      MQTT/HTTP

  TUR55/Gold                          \+ Secure boot, secure key storage,
                                      remote attestation
  -----------------------------------------------------------------------

**10. Conclusion**

**TUR55v1** establishes a unified standard for transmitting commands
from blockchain to the physical world, solving the fragmentation problem
existing among executors (analogous to oracles, but for execution). The
protocol is **blockchain-agnostic**, **transport-independent** and
ensures secure code execution in a constrained environment.

**11. License Terms**

**11.1. Standard Status**

Standard **TUR55v1 (Trusted Unified Regulation)** is **an open public
standard** (Open Public Standard).

Any person or organization has the right to:

\- use the specification to implement compatible devices, software, and
services;

\- distribute and cite the document in whole or in part;

\- create forks and modifications of the protocol provided that
authorship is preserved and a link to the original is provided.

**11.2. Mandatory Usage Terms**

When using the standard in any way (including implementations, forks,
derivative works, commercial products) **mandatory**:

1\. Preserve authorship attribution:

-\"Based on TUR55 Standard (Trusted Unified Regulation), author:
NOMOS-H\"

2\. Include a link to the original specification:

-\"Original: \[URL GitHub / IPFS / Arweave\]\"

3\. Do not remove or hide built-in royalty mechanisms (if implemented in
code), unless otherwise agreed with the author.

11.3. Commercial Use

Commercial use of the standard **is permitted without royalty
payments**, but:

\- It is prohibited to pass off derivative standards or implementations
as \"official TUR55\" without explicit indication of modification.

\- It is prohibited to use the trade name \"TUR55\" or its stylization
in the names of commercial products without written permission from the
author.

**11.4. Exceptions and Limitations**

The author of the standard **is not liable** for any damage, direct or
indirect, arising from the use of this standard or its implementations.

The standard is provided \"as is\" (AS IS), without any warranties.

**11.5. Contact Information**

For licensing inquiries, obtaining permission to use the trade name, or
entering into commercial agreements:

\- Email: tur55@protonmail.com

\- GitHub: https://github.com/Tabintaban

\- Discord: tur55

**12. Version Statement**

This document represents **the first fixed version (v1.0).**\
Changes are not permitted.\
Future changes must be introduced only as new versions.

The date of birth of the TUR55 protocol is 10.04.2026 years, the mention
of the protocol TUR55 earlier than this date is drafts and preliminary
design.

**13. Declaration of Authorship**

I, NOMOS-H, hereby declare authorship and priority\
of the content described in this document as of the fixation date stated
above.

**Cryptographic Signature**

(signature)

**End of document**
