// ================================================================================ //
// The NEORV32 RISC-V Processor - https://github.com/stnolting/neorv32              //
// Copyright (c) NEORV32 contributors.                                              //
// Copyright (c) 2020 - 2025 Stephan Nolting. All rights reserved.                  //
// Licensed under the BSD-3-Clause license, see LICENSE for details.                //
// SPDX-License-Identifier: BSD-3-Clause                                            //
// ================================================================================ //


/**********************************************************************//**
 * @file demo_aes/main.c
 * @author Ilya Cable
 * @brief Simple AES demo program.
 **************************************************************************/

#include <neorv32.h>

// AES Register offsets (word)
#define REG_CTRL        0x00/4
#define REG_IV_ENC_0    0x40/4
#define REG_KEY_ENC_0   0x50/4
#define REG_PT_ENC_0    0x60/4
#define REG_CB_ENC_0    0x70/4
#define REG_IV_DEC_0    0xC0/4
#define REG_KEY_DEC_0   0xD0/4
#define REG_CB_DEC_0    0xE0/4
#define REG_PT_DEC_0    0xF0/4

// Control Register Bits
#define CTRL_RESET_ENC  (1 << 0)
#define CTRL_START_ENC  (1 << 3)
#define CTRL_DONE_ENC   (1 << 7)
#define CTRL_RESET_DEC  (1 << 15)
#define CTRL_START_DEC  (1 << 19)
#define CTRL_DONE_DEC   (1 << 23)

/**********************************************************************//**
 * @name User configuration
 **************************************************************************/
/**@{*/
/** UART BAUD rate */
#define BAUD_RATE 19200
/** Maximum PWM output intensity (8-bit duty cycle) */
#define MAX_DUTY 200
/**@}*/

void write_cfs(uint8_t addr, uint32_t data){
  NEORV32_CFS->REG[addr] = data;
}

uint32_t read_cfs(uint8_t addr){
  return NEORV32_CFS->REG[addr];
}

/**********************************************************************//**
 * Print and verify data blocks, showing differences in a pretty format
 *
 * @param label    Label to print with the data (e.g., "FIPS", "Multi-block")
 * @param actual   Array of actual values
 * @param expected Array of expected values
 * @param num_blocks Number of 128-bit blocks (each block is 4 32-bit words)
 **************************************************************************/
void print_and_verify_data(const char* label, const uint32_t* actual, const uint32_t* expected, size_t num_blocks) {
    int total_words = num_blocks * 4;
    int error_count = 0;
    
    // First pass: count errors
    for(int i = total_words - 1; i >= 0; i--) {
        if (actual[i] != expected[i]) error_count++;
    }
    
    // Print header with error summary
    neorv32_uart0_printf("\n╔════════ %s Data \n", label);
    
    // Print data with error highlighting
    for(int block = num_blocks - 1; block >= 0; block--) {
        neorv32_uart0_printf("║ Block %d: ", block);
        for(int word = 3; word >= 0; word--) {
            int i = block * 4 + word;
            if (actual[i] == expected[i]) {
                neorv32_uart0_printf("%x", actual[i]);
            } else {
                neorv32_uart0_printf("\n║ ERROR at word %d:\n", word);
                neorv32_uart0_printf("║   Got:      %x\n", actual[i]);
                neorv32_uart0_printf("║   Expected: %x\n", expected[i]);
            }
            if (word > 0) neorv32_uart0_printf(" ");
        }
        neorv32_uart0_printf("\n");
    }
    
    // Print footer with error summary
    if (error_count > 0) {
        neorv32_uart0_printf("╠════════════════════ FAIL ════════════════════╣\n");
        neorv32_uart0_printf("║ Found %d error%s                                ║\n", 
                            error_count, error_count > 1 ? "s" : " ");
    } else {
        neorv32_uart0_printf("╠════════════════════ PASS ════════════════════╣\n");
        neorv32_uart0_printf("║ All values match!                            ║\n");
    }
    neorv32_uart0_printf("╚══════════════════════════════════════════════╝\n");
}

/**********************************************************************//**
 * Encrypt multiple 128-bit blocks using AES-CBC
 * 
 * @param key      Pointer to 128-bit encryption key (as array of 4 32-bit words)
 * @param iv       Pointer to 128-bit initialization vector (as array of 4 32-bit words)
 * @param pt       Pointer to plaintext input (array of 32-bit words, length = 4 * num_blocks)
 * @param ct       Pointer to ciphertext output (array of 32-bit words, length = 4 * num_blocks)
 * @param num_blocks Number of 128-bit blocks to encrypt
 **************************************************************************/
void aes_encode(const uint32_t* key, const uint32_t* iv, const uint32_t* pt, uint32_t* ct, size_t num_blocks) {
    // Reset encryption core (active low)
    write_cfs(REG_CTRL, 0);
    write_cfs(REG_CTRL, CTRL_RESET_ENC);

    neorv32_uart0_printf("CONTROL: %x\n", read_cfs(REG_CTRL));
    
    // Write key (only need to do this once)
    neorv32_uart0_printf("Writing encryption key\n");
    for(int i = 0; i < 4; i++) {
        uint8_t addr = REG_KEY_ENC_0 + i;
        write_cfs(addr, key[i]);
        neorv32_uart0_printf("KEY[%d] (ADDR: %d): %x\n", i, addr, read_cfs(addr));
    }
    
    // Write initial IV (only need to do this once)
    neorv32_uart0_printf("Writing initial vector\n");

    for(int i = 0; i < 4; i++) {
      uint8_t addr = REG_IV_ENC_0 + i;
      write_cfs(addr, iv[i]);
      neorv32_uart0_printf("IV[%d] (ADDR: %d): %x\n", i, addr, read_cfs(addr));
    }
    
    // Process each block
    neorv32_uart0_printf("Processing plaintext\n");
    for(size_t block = num_blocks; block-- > 0; ) {
        // Write plaintext for this block
        neorv32_uart0_printf("Writing plaintext for block %d\n", block);
        for(int i = 0; i < 4; i++) {
            uint8_t addr = REG_PT_ENC_0 + i;
            write_cfs(addr, pt[block * 4 + i]);
            neorv32_uart0_printf("PT[%d] (ADDR: %d): %x\n", i, addr, read_cfs(addr));
        }
        
        // neorv32_uart0_printf("Before writing start enc CONTROL: %x\n", read_cfs(REG_CTRL));
        // Start encryption
        neorv32_uart0_printf("Starting encryption for block %d\n", block);
        write_cfs(REG_CTRL, CTRL_RESET_ENC | CTRL_START_ENC);
        
        // Wait for completion
        while(!(read_cfs(REG_CTRL) & CTRL_DONE_ENC));

        // Read ciphertext
        for(int i = 0; i < 4; i++) {
            uint8_t addr = REG_CB_ENC_0 + i;
            ct[block * 4 + i] = read_cfs(addr);
            neorv32_uart0_printf("CB[%d] (ADDR: %d): %x\n", i, addr, read_cfs(addr));
        }
  }
}

/**********************************************************************//**
 * Decrypt multiple 128-bit blocks using AES-CBC
 * 
 * @param key      Pointer to 128-bit decryption key (as array of 4 32-bit words)
 * @param iv       Pointer to 128-bit initialization vector (as array of 4 32-bit words)
 * @param ct       Pointer to ciphertext input (array of 32-bit words, length = 4 * num_blocks)
 * @param pt       Pointer to plaintext output (array of 32-bit words, length = 4 * num_blocks)
 * @param num_blocks Number of 128-bit blocks to decrypt
 **************************************************************************/
void aes_decode(const uint32_t* key, const uint32_t* iv, const uint32_t* ct, uint32_t* pt, size_t num_blocks) {
    // Reset decryption core (active low)
    write_cfs(REG_CTRL, 0);
    write_cfs(REG_CTRL, CTRL_RESET_DEC);

    neorv32_uart0_printf("CONTROL: %x\n", read_cfs(REG_CTRL));
    
    // Write key (only need to do this once)
    neorv32_uart0_printf("Writing decryption key\n");
    for(int i = 0; i < 4; i++) {
        uint8_t addr = REG_KEY_DEC_0 + i;
        write_cfs(addr, key[i]);
        neorv32_uart0_printf("KEY[%d] (ADDR: %d): %x\n", i, addr, read_cfs(addr));
    }
    
    // Write initial IV (only need to do this once)
    neorv32_uart0_printf("Writing initial vector\n");
    for(int i = 0; i < 4; i++) {
        uint8_t addr = REG_IV_DEC_0 + i;
        write_cfs(addr, iv[i]);
        neorv32_uart0_printf("IV[%d] (ADDR: %d): %x\n", i, addr, read_cfs(addr));
    }
    
    // Process each block
    neorv32_uart0_printf("Processing ciphertext\n");
    for(size_t block = num_blocks; block-- > 0; ) {
        // Write ciphertext for this block
        neorv32_uart0_printf("Writing ciphertext for block %d\n", block);
        for(int i = 0; i < 4; i++) {
            uint8_t addr = REG_PT_DEC_0 + i;
            write_cfs(addr, ct[block * 4 + i]);
            neorv32_uart0_printf("CT[%d] (ADDR: %d): %x\n", i, addr, read_cfs(addr));
        }
        
        // Start decryption
        neorv32_uart0_printf("Starting decryption for block %d\n", block);
        write_cfs(REG_CTRL, CTRL_RESET_DEC | CTRL_START_DEC);
        
        // Wait for completion
        while(!(read_cfs(REG_CTRL) & CTRL_DONE_DEC));

        // Read plaintext
        for(int i = 0; i < 4; i++) {
            uint8_t addr = REG_CB_DEC_0 + i;
            pt[block * 4 + i] = read_cfs(addr);
            neorv32_uart0_printf("PT[%d] (ADDR: %d): %x\n", i, addr, read_cfs(addr));
        }
    }
}

int main() {
  // capture all exceptions and give debug info via UART
  // this is not required, but keeps us safe
  neorv32_rte_setup();

  // use UART0 if implemented
  if (neorv32_uart0_available()) {
    // setup UART at default baud rate, no interrupts
    neorv32_uart0_setup(BAUD_RATE, 0);

    neorv32_uart0_printf("<<< AES demo program >>>\n");

    // FIPS test vectors
    // key - 0x2B7E151628AED2A6ABF7158809CF4F3C
    uint32_t fips_key[4] = {
        0x09CF4F3C,  // Least significant 32 bits
        0xABF71588,
        0x28AED2A6,
        0x2B7E1516   
    };
    
    uint32_t fips_iv[4] = {
        0x00000000,
        0x00000000,
        0x00000000,
        0x00000000
    };

    // 0x3243F6A8885A308D313198A2E0370734
    uint32_t fips_pt[4] = {
        0xE0370734,
        0x313198A2,
        0x885A308D,
        0x3243F6A8   
    };

    // 0x3925841D02DC09FBDC118597196A0B32
    uint32_t ciphertext_fips_exp[4] = {
        0x196A0B32,  
        0xDC118597,
        0x02DC09FB,
        0x3925841D   
    };

    uint32_t ciphertext_fips[4];  // Will hold the encrypted result
    
    // Encrypt the data (1 block)
    aes_encode(fips_key, fips_iv, fips_pt, ciphertext_fips, 1);
    
    print_and_verify_data("FIPS", ciphertext_fips, ciphertext_fips_exp, 1);

    // Decrypt the data back
    uint32_t decrypted_fips[4];
    aes_decode(fips_key, fips_iv, ciphertext_fips, decrypted_fips, 1);
    
    print_and_verify_data("FIPS (decrypted)", decrypted_fips, fips_pt, 1);

    // Multi-block test
    uint32_t key[4] = {
        0x5914653F,   // Least significant 32 bits
        0x991E1F55,
        0x39F45498,
        0x896FC880  
    };
    
    uint32_t iv[4] = {
        0xAD30B29F, // LSB
        0x9D85A20D,
        0xCA02ABBB,
        0xED4A6097 
    };
    
    // "This data is not for prying eyes!" in utf-8 and padded to AES block size
    uint32_t plaintext[12] = {  // 3 blocks of 128 bits each
        // LSB
        0x0f0f0f0f, 0x0f0f0f0f, 0x0f0f0f0f, 0x210f0f0f,     // Block 3
        0x65796573, 0x696e6720, 0x20707279, 0x20666f72,     // Block 2
        0x206e6f74, 0x61206973, 0x20646174, 0x54686973      // Block 1
    };

    // 0x6b5af2ab6ce09dd2787595b3307648642e5c5b25e1b50333a8d56d374667e341de2d58fbc43de6551da4ac771f276075 
    uint32_t ciphertext_mb_exp[12] = {  // 3 blocks of 128 bits each
        // LSB
        0x1f276075, 0x1da4ac77, 0xc43de655, 0xde2d58fb,     // Block 3
        0x4667e341, 0xa8d56d37, 0xe1b50333, 0x2e5c5b25,      // Block 2
        0x30764864, 0x787595b3, 0x6ce09dd2, 0x6b5af2ab      // Block 1
    };

    uint32_t ciphertext_mb[12];  // Will hold the encrypted result
    
    // Encrypt the data (3 blocks)
    aes_encode(key, iv, plaintext, ciphertext_mb, 3);

    print_and_verify_data("Multi-block", ciphertext_mb, ciphertext_mb_exp, 3);

    // Decrypt the data back
    uint32_t decrypted_mb[12];
    aes_decode(key, iv, ciphertext_mb, decrypted_mb, 3);
    
    print_and_verify_data("Multi-block (decrypted)", decrypted_mb, plaintext, 3);

  }
  
  return 0;
}
