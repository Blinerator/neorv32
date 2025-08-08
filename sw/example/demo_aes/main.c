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

// AES Register offsets
#define REG_CTRL        0x00
#define REG_IV_ENC_0    0x40
#define REG_KEY_ENC_0   0x50
#define REG_PT_ENC_0    0x60
#define REG_CB_ENC_0    0x70
#define REG_IV_DEC_0    0xC0
#define REG_KEY_DEC_0   0xD0
#define REG_CB_DEC_0    0xE0
#define REG_PT_DEC_0    0xF0

// Control Register Bits
#define CTRL_RESET_ENC  (1 << 0)
#define CTRL_START_ENC  (1 << 3)
#define CTRL_DONE_ENC   (1 << 7)
#define CTRL_RESET_DEC  (1 << 15)
#define CTRL_START_DEC  (1 << 19)
#define CTRL_DONE_DEC   (1 << 23)

// Test data
// "This data is not for prying eyes!" in utf-8 and padded to AES block size
#define PLAINTEXT_IN 0x546869732064617461206973206e6f7420666f7220707279696e672065796573210f0f0f0f0f0f0f0f0f0f0f0f0f0f0f
#define IV_IN 0xED4A6097CA02ABBB9D85A20DAD30B29F // Randomly generated
#define KEY_IN 0x896FC88039F45498991E1F555914653F // Randomly generated

// We expect the output to be:
#define CIPHERTEXT_OUT 0x6b5af2ab6ce09dd2787595b3307648642e5c5b25e1b50333a8d56d374667e341de2d58fbc43de6551da4ac771f276075 

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
    
    // Write key (only need to do this once)
    for(int i = 0; i < 4; i++) {
        write_cfs(REG_KEY_ENC_0 + (i * 4), key[i]);
    }
    
    // Write initial IV (only need to do this once)
    for(int i = 0; i < 4; i++) {
        write_cfs(REG_IV_ENC_0 + (i * 4), iv[i]);
    }
    
    // Process each block
    for(size_t block = 0; block < num_blocks; block++) {
        // Write plaintext for this block
        for(int i = 0; i < 4; i++) {
            write_cfs(REG_PT_ENC_0 + (i * 4), pt[block * 4 + i]);
        }
        
        // Start encryption
        write_cfs(REG_CTRL, CTRL_RESET_ENC | CTRL_START_ENC);
        
        // Wait for completion
        while(!(read_cfs(REG_CTRL) & CTRL_DONE_ENC));
        
        // Read ciphertext
    for(int i = 0; i < 4; i++) {
        ct[block * 4 + i] = read_cfs(REG_CB_ENC_0 + (i * 4));
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
    
    // Write key (only need to do this once)
    for(int i = 0; i < 4; i++) {
        write_cfs(REG_KEY_DEC_0 + (i * 4), key[i]);
    }
    
    // Write initial IV (only need to do this once)
    for(int i = 0; i < 4; i++) {
        write_cfs(REG_IV_DEC_0 + (i * 4), iv[i]);
    }
    
    // Process each block
    for(size_t block = 0; block < num_blocks; block++) {
        // Write ciphertext for this block
        for(int i = 0; i < 4; i++) {
            write_cfs(REG_CB_DEC_0 + (i * 4), ct[block * 4 + i]);
        }
        
        // Start decryption
        write_cfs(REG_CTRL, CTRL_RESET_DEC | CTRL_START_DEC);
        
        // Wait for completion
        while(!(read_cfs(REG_CTRL) & CTRL_DONE_DEC));
        
        // Read plaintext
    for(int i = 0; i < 4; i++) {
        pt[block * 4 + i] = read_cfs(REG_PT_DEC_0 + (i * 4));
    }
  }
}

/**********************************************************************//**
 * This program generates a simple dimming sequence for PWM channels 0 to 3.
 *
 * @note This program requires the PWM controller to be synthesized (the UART is optional).
 *
 * @return !=0 if error.
 **************************************************************************/
int main() {

  // capture all exceptions and give debug info via UART
  // this is not required, but keeps us safe
  neorv32_rte_setup();

  // use UART0 if implemented
  if (neorv32_uart0_available()) {
    // setup UART at default baud rate, no interrupts
    neorv32_uart0_setup(BAUD_RATE, 0);

    // say hello
    neorv32_uart0_printf("<<< AES demo program >>>\n");
    
    // Break 128-bit values into 32-bit words
    uint32_t key[4] = {
        0x896FC880,  // Most significant 32 bits
        0x39F45498,
        0x991E1F55,
        0x5914653F   // Least significant 32 bits
    };
    
    uint32_t iv[4] = {
        0xED4A6097,
        0xCA02ABBB,
        0x9D85A20D,
        0xAD30B29F
    };
    
    uint32_t plaintext[12] = {  // 3 blocks of 128 bits each
        0x54686973, 0x20646174, 0x61206973, 0x206e6f74,  // Block 1
        0x20666f72, 0x20707279, 0x696e6720, 0x65796573,  // Block 2
        0x210f0f0f, 0x0f0f0f0f, 0x0f0f0f0f, 0x0f0f0f0f   // Block 3
    };
    
    uint32_t ciphertext[12];  // Will hold the encrypted result
    
    // Encrypt the data (3 blocks)
    aes_encode(key, iv, plaintext, ciphertext, 3);
    
    // Print results
    neorv32_uart0_printf("\nEncrypted data:\n");
    for(int i = 0; i < 12; i++) {
        neorv32_uart0_printf("%08x", ciphertext[i]);
        if ((i + 1) % 4 == 0) neorv32_uart0_printf("\n");
    }
  }
  
  return 0;
}
