# Register Map

The AES core is controlled through a set of memory-mapped registers in the NEORV32 Custom Functions Subsystem (CFS). All registers are 32-bit wide.

## Memory Map
| Offset (byte) | Name | Type | Description |
|--------|------|------|-------------|
| 0x00 | CTRL | RW/RO | Control/Status Register |
| | .RESET_ENC[0] | RW | Reset encryption core (active low) |
| | .START_ENC[3] | RW | Start encryption operation |
| | .DONE_ENC[7] | RO | Encryption operation complete |
| | .RESET_DEC[15] | RW | Reset decryption core (active low) |
| | .START_DEC[19] | RW | Start decryption operation |
| | .DONE_DEC[23] | RO | Decryption operation complete |
| 0x40 | IV_ENC_0 | RW | Initialization Vector for encryption (bits 31:0) |
| 0x44 | IV_ENC_1 | RW | Initialization Vector for encryption (bits 63:32) |
| 0x48 | IV_ENC_2 | RW | Initialization Vector for encryption (bits 95:64) |
| 0x4C | IV_ENC_3 | RW | Initialization Vector for encryption (bits 127:96) |
| 0x50 | KEY_ENC_0 | RW | Encryption key (bits 31:0) |
| 0x54 | KEY_ENC_1 | RW | Encryption key (bits 63:32) |
| 0x58 | KEY_ENC_2 | RW | Encryption key (bits 95:64) |
| 0x5C | KEY_ENC_3 | RW | Encryption key (bits 127:96) |
| 0x60 | PT_ENC_0 | RW | Plaintext input for encryption (bits 31:0) |
| 0x64 | PT_ENC_1 | RW | Plaintext input for encryption (bits 63:32) |
| 0x68 | PT_ENC_2 | RW | Plaintext input for encryption (bits 95:64) |
| 0x6C | PT_ENC_3 | RW | Plaintext input for encryption (bits 127:96) |
| 0x70 | CB_ENC_0 | RO | Ciphertext output from encryption (bits 31:0) |
| 0x74 | CB_ENC_1 | RO | Ciphertext output from encryption (bits 63:32) |
| 0x78 | CB_ENC_2 | RO | Ciphertext output from encryption (bits 95:64) |
| 0x7C | CB_ENC_3 | RO | Ciphertext output from encryption (bits 127:96) |
| 0xC0 | IV_DEC_0 | RW | Initialization Vector for decryption (bits 31:0) |
| 0xC4 | IV_DEC_1 | RW | Initialization Vector for decryption (bits 63:32) |
| 0xC8 | IV_DEC_2 | RW | Initialization Vector for decryption (bits 95:64) |
| 0xCC | IV_DEC_3 | RW | Initialization Vector for decryption (bits 127:96) |
| 0xD0 | KEY_DEC_0 | RW | Decryption key (bits 31:0) |
| 0xD4 | KEY_DEC_1 | RW | Decryption key (bits 63:32) |
| 0xD8 | KEY_DEC_2 | RW | Decryption key (bits 95:64) |
| 0xDC | KEY_DEC_3 | RW | Decryption key (bits 127:96) |
| 0xE0 | CB_DEC_0 | RW | Ciphertext input for decryption (bits 31:0) |
| 0xE4 | CB_DEC_1 | RW | Ciphertext input for decryption (bits 63:32) |
| 0xE8 | CB_DEC_2 | RW | Ciphertext input for decryption (bits 95:64) |
| 0xEC | CB_DEC_3 | RW | Ciphertext input for decryption (bits 127:96) |
| 0xF0 | PT_DEC_0 | RO | Plaintext output from decryption (bits 31:0) |
| 0xF4 | PT_DEC_1 | RO | Plaintext output from decryption (bits 63:32) |
| 0xF8 | PT_DEC_2 | RO | Plaintext output from decryption (bits 95:64) |
| 0xFC | PT_DEC_3 | RO | Plaintext output from decryption (bits 127:96) |

## Operation
1. Write the initialization vector and key to the respective registers
2. For encryption:
   - Write the plaintext to PT_ENC registers
   - Set START_ENC bit
   - Wait for DONE_ENC bit to be set
   - Read the ciphertext from CB_ENC registers
3. For decryption:
   - Write the ciphertext to CB_DEC registers
   - Set START_DEC bit
   - Wait for DONE_DEC bit to be set
   - Read the plaintext from PT_DEC registers  