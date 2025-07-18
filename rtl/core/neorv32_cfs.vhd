-- ================================================================================ --
-- NEORV32 SoC - Custom Functions Subsystem (CFS)                                   --
-- -------------------------------------------------------------------------------- --
-- Intended for tightly-coupled, application-specific custom co-processors. This    --
-- module provides up to 64x 32-bit memory-mapped interface registers, one CPU      --
-- interrupt request signal and custom IO conduits for processor-external or chip-  --
-- external interface.                                                              --
-- -------------------------------------------------------------------------------- --
-- The NEORV32 RISC-V Processor - https://github.com/stnolting/neorv32              --
-- Copyright (c) NEORV32 contributors.                                              --
-- Copyright (c) 2020 - 2025 Stephan Nolting. All rights reserved.                  --
-- Licensed under the BSD-3-Clause license, see LICENSE for details.                --
-- SPDX-License-Identifier: BSD-3-Clause                                            --
-- ================================================================================ --

library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

library neorv32;
use neorv32.neorv32_package.all;

entity neorv32_cfs is
  generic (
    CFS_CONFIG   : std_ulogic_vector(31 downto 0) := (others => '0'); -- custom CFS configuration generic
    CFS_IN_SIZE  : natural := 256; -- size of CFS input conduit in bits
    CFS_OUT_SIZE : natural := 256 -- size of CFS output conduit in bits
  );
  port (
    clk_i       : in  std_ulogic; -- global clock line
    rstn_i      : in  std_ulogic; -- global reset line, low-active, use as async
    bus_req_i   : in  bus_req_t; -- bus request
    bus_rsp_o   : out bus_rsp_t; -- bus response
    clkgen_en_o : out std_ulogic; -- enable clock generator
    clkgen_i    : in  std_ulogic_vector(7 downto 0); -- "clock" inputs
    irq_o       : out std_ulogic; -- interrupt request
    cfs_in_i    : in  std_ulogic_vector(CFS_IN_SIZE-1 downto 0); -- custom inputs
    cfs_out_o   : out std_ulogic_vector(CFS_OUT_SIZE-1 downto 0) -- custom outputs
  );
end neorv32_cfs;

architecture neorv32_cfs_rtl of neorv32_cfs is
  -- Register addresses
  constant CONTROL     : integer := 0;
  constant IV_ENC_BAR  : integer := 16; 
  constant KEY_ENC_BAR : integer := 20;
  constant PT_ENC_BAR  : integer := 24;
  constant CB_ENC_BAR  : integer := 28;
  constant IV_DEC_BAR  : integer := 48; 
  constant KEY_DEC_BAR : integer := 52;
  constant PT_DEC_BAR  : integer := 56;
  constant CB_DEC_BAR  : integer := 60;

  -- default CFS interface registers --
  signal control_reg : std_ulogic_vector(31 downto 0);

  signal reset_enc : std_ulogic;
  signal reset_dec : std_ulogic;

  signal init_vec_enc : std_ulogic_vector(127 downto 0);
  signal key_enc : std_ulogic_vector(127 downto 0);
  signal plaintext_enc : std_ulogic_vector(127 downto 0);
  signal cipherblock_enc : std_ulogic_vector(127 downto 0);
  signal start_enc : std_ulogic;
  signal done_enc : std_ulogic;

  signal init_vec_dec : std_ulogic_vector(127 downto 0);
  signal key_dec : std_ulogic_vector(127 downto 0);
  signal cipherblock_dec : std_ulogic_vector(127 downto 0);
  signal plaintext_dec : std_ulogic_vector(127 downto 0);
  signal start_dec : std_ulogic;
  signal done_dec : std_ulogic;

begin

  -- CFS Generics ---------------------------------------------------------------------------
  -- -------------------------------------------------------------------------------------------
  -- In it's default version the CFS provides three configuration generics:
  -- > CFS_IN_SIZE  - configures the size (in bits) of the CFS input conduit cfs_in_i
  -- > CFS_OUT_SIZE - configures the size (in bits) of the CFS output conduit cfs_out_o
  -- > CFS_CONFIG   - is a blank 32-bit generic. It is intended as a "generic conduit" to propagate
  --                  custom configuration flags from the top entity down to this module.


  -- CFS IOs --------------------------------------------------------------------------------
  -- -------------------------------------------------------------------------------------------
  -- By default, the CFS provides two IO signals (cfs_in_i and cfs_out_o) that are available at the processor's top entity.
  -- These are intended as "conduits" to propagate custom signals from this module and the processor top entity.
  --
  -- If the CFU output signals are to be used outside the chip, it is recommended to register these signals.

  cfs_out_o <= (others => '0'); -- not used for this design


  -- Reset System ---------------------------------------------------------------------------
  -- -------------------------------------------------------------------------------------------
  -- The CFS can be reset using the global rstn_i signal. This signal should be used as asynchronous reset and is active-low.
  -- Note that rstn_i can be asserted by a processor-external reset, the on-chip debugger and also by the watchdog.
  --
  -- Most default peripheral devices of the NEORV32 do NOT use a dedicated hardware reset at all. Instead, these units are
  -- reset by writing ZERO to a specific "control register" located right at the beginning of the device's address space
  -- (so this register is cleared at first). The crt0 start-up code writes ZERO to every single address in the processor's
  -- IO space - including the CFS. Make sure that this initial clearing does not cause any unintended CFS actions.


  -- Clock System ---------------------------------------------------------------------------
  -- -------------------------------------------------------------------------------------------
  -- The processor top unit implements a clock generator providing 8 "derived clocks".
  -- Actually, these signals should not be used as direct clock signals, but as *clock enable* signals.
  -- clkgen_i is always synchronous to the main system clock (clk_i).
  --
  -- The following clock dividers are available:
  -- > clkgen_i(clk_div2_c)    -> MAIN_CLK/2
  -- > clkgen_i(clk_div4_c)    -> MAIN_CLK/4
  -- > clkgen_i(clk_div8_c)    -> MAIN_CLK/8
  -- > clkgen_i(clk_div64_c)   -> MAIN_CLK/64
  -- > clkgen_i(clk_div128_c)  -> MAIN_CLK/128
  -- > clkgen_i(clk_div1024_c) -> MAIN_CLK/1024
  -- > clkgen_i(clk_div2048_c) -> MAIN_CLK/2048
  -- > clkgen_i(clk_div4096_c) -> MAIN_CLK/4096
  --
  -- For instance, if you want to drive a clock process at MAIN_CLK/8 clock speed you can use the following construct:
  --
  --   if (rstn_i = '0') then -- async and low-active reset (if required at all)
  --   ...
  --   elsif rising_edge(clk_i) then -- always use the main clock for all clock processes
  --     if (clkgen_i(clk_div8_c) = '1') then -- the div8 "clock" is actually a clock enable
  --       ...
  --     end if;
  --   end if;
  --
  -- The clkgen_i input clocks are available when at least one IO/peripheral device (for example UART0) requires the clocks
  -- generated by the clock generator. The CFS can enable the clock generator by itself by setting the clkgen_en_o signal high.
  -- The CFS cannot ensure to deactivate the clock generator by setting the clkgen_en_o signal low as other peripherals might
  -- still keep the generator activated. Make sure to deactivate the CFS's clkgen_en_o if no clocks are required in here to
  -- reduce dynamic power consumption.

  clkgen_en_o <= '0'; -- not used


  -- Interrupt ------------------------------------------------------------------------------
  -- -------------------------------------------------------------------------------------------
  -- The CFS features a single interrupt signal, which is connected to the CPU's "fast interrupt" channel 1 (FIRQ1).
  -- The according CPU interrupt becomes pending as long as <irq_o> is high.

  irq_o <= '0'; -- not used


  -- Read/Write Access ----------------------------------------------------------------------
  -- -------------------------------------------------------------------------------------------
  -- Here we are reading/writing from/to the interface registers of the module and generate the CPU access handshake (bus response).
  --
  -- The CFS provides up to 64kB of memory-mapped address space (16 address bits, byte-addressing) that can be used for custom
  -- memories and interface registers. If the complete 16-bit address space is not required, only the minimum LSBs required for
  -- address decoding can be used. In this case, however, the implemented registers are replicated (several times) across the CFS
  -- address space.
  --
  -- Following the interface protocol, each read or write access has to be acknowledged in the following cycle using the ack_o
  -- signal (or even later if the module needs additional time). If no ACK is generated at all, the bus access will time out
  -- and cause a bus access fault exception. The current CPU privilege level is available via the 'priv_i' signal (0 = user mode,
  -- 1 = machine mode), which can be used to constrain access to certain registers or features to privileged software only.
  --
  -- This module also provides an optional ERROR signal to indicate a faulty access operation (for example when accessing an
  -- unused, read-only or "locked" CFS register address). This signal may only be set when the module is actually accessed
  -- and is set INSTEAD of the ACK signal. Setting the ERR signal will raise a bus access exception with a "Device Error" qualifier
  -- that can be handled by the application software. Note that the current privilege level should not be exposed to software to
  -- maintain full virtualization. Hence, CFS-based "privilege escalation" should trigger a bus access exception (e.g. by setting 'err_o').
  
  bus_access: process(rstn_i, clk_i)
  begin
    if (rstn_i = '0') then
      bus_rsp_o     <= rsp_terminate_c;
    elsif rising_edge(clk_i) then -- synchronous interface for read and write accesses
      -- transfer/access acknowledge --
      bus_rsp_o.ack <= bus_req_i.stb;

      -- tie to zero if not explicitly used --
      bus_rsp_o.err <= '0'; -- set high together with bus_rsp_o.ack if there is an access error

      -- defaults --
      bus_rsp_o.data <= (others => '0'); -- the output HAS TO BE ZERO if there is no actual (read) access

      -- bus access --
      if (bus_req_i.stb = '1') then -- valid access cycle, STB is high for one cycle

        -- write access (word-wise) --
        if (bus_req_i.rw = '1') then
          case to_integer(unsigned(bus_req_i.addr(15 downto 2))) is
            when CONTROL =>
              -- TODO: don't write to ro bits
              control_reg <= bus_req_i.data;

            -- encryption IV
            when IV_ENC_BAR =>
              init_vec_enc(31 downto 0) <= bus_req_i.data;
            when IV_ENC_BAR + 1 =>
              init_vec_enc(63 downto 32) <= bus_req_i.data;
            when IV_ENC_BAR + 2 =>
              init_vec_enc(95 downto 64) <= bus_req_i.data;
            when IV_ENC_BAR + 3 =>
              init_vec_enc(127 downto 96) <= bus_req_i.data;

            -- encryption key
            when KEY_ENC_BAR =>
              key_enc(31 downto 0) <= bus_req_i.data;
            when KEY_ENC_BAR + 1 =>
              key_enc(63 downto 32) <= bus_req_i.data;
            when KEY_ENC_BAR + 2 =>
              key_enc(95 downto 64) <= bus_req_i.data;
            when KEY_ENC_BAR + 3 =>
              key_enc(127 downto 96) <= bus_req_i.data;

            -- encryption plaintext
            when PT_ENC_BAR =>
              plaintext_enc(31 downto 0) <= bus_req_i.data;
            when PT_ENC_BAR + 1 =>
              plaintext_enc(63 downto 32) <= bus_req_i.data;
            when PT_ENC_BAR + 2 =>
              plaintext_enc(95 downto 64) <= bus_req_i.data;
            when PT_ENC_BAR + 3 =>
              plaintext_enc(127 downto 96) <= bus_req_i.data;

            -- decryption IV
            when IV_DEC_BAR =>
              init_vec_dec(31 downto 0) <= bus_req_i.data;
            when IV_DEC_BAR + 1 =>
              init_vec_dec(63 downto 32) <= bus_req_i.data;
            when IV_DEC_BAR + 2 =>
              init_vec_dec(95 downto 64) <= bus_req_i.data;
            when IV_DEC_BAR + 3 =>
              init_vec_dec(127 downto 96) <= bus_req_i.data;

            -- decryption key
            when KEY_DEC_BAR =>
              key_dec(31 downto 0) <= bus_req_i.data;
            when KEY_DEC_BAR + 1 =>
              key_dec(63 downto 32) <= bus_req_i.data;
            when KEY_DEC_BAR + 2 =>
              key_dec(95 downto 64) <= bus_req_i.data;
            when KEY_DEC_BAR + 3 =>
              key_dec(127 downto 96) <= bus_req_i.data;

            -- decryption ciphertext input
            when CB_DEC_BAR =>
              cipherblock_dec(31 downto 0) <= bus_req_i.data;
            when CB_DEC_BAR + 1 =>
              cipherblock_dec(63 downto 32) <= bus_req_i.data;
            when CB_DEC_BAR + 2 =>
              cipherblock_dec(95 downto 64) <= bus_req_i.data;
            when CB_DEC_BAR + 3 =>
              cipherblock_dec(127 downto 96) <= bus_req_i.data;

            -- decryption plaintext output
            when PT_DEC_BAR =>
              plaintext_dec(31 downto 0) <= bus_req_i.data;
            when PT_DEC_BAR + 1 =>
              plaintext_dec(63 downto 32) <= bus_req_i.data;
            when PT_DEC_BAR + 2 =>
              plaintext_dec(95 downto 64) <= bus_req_i.data;
            when PT_DEC_BAR + 3 =>
              plaintext_dec(127 downto 96) <= bus_req_i.data;

            when others =>
              null;
          end case;

        -- read access (word-wise) --
        else
          bus_rsp_o.data <= (others => '0');
          case to_integer(unsigned(bus_req_i.addr(15 downto 2))) is
            when CONTROL =>
              bus_rsp_o.data <= control_reg;

            -- encryption IV
            when IV_ENC_BAR =>
              bus_rsp_o.data <= init_vec_enc(31 downto 0);
            when IV_ENC_BAR + 1 =>
              bus_rsp_o.data <= init_vec_enc(63 downto 32);
            when IV_ENC_BAR + 2 =>
              bus_rsp_o.data <= init_vec_enc(95 downto 64);
            when IV_ENC_BAR + 3 =>
              bus_rsp_o.data <= init_vec_enc(127 downto 96);

            -- encryption key
            when KEY_ENC_BAR =>
              bus_rsp_o.data <= key_enc(31 downto 0);
            when KEY_ENC_BAR + 1 =>
              bus_rsp_o.data <= key_enc(63 downto 32);
            when KEY_ENC_BAR + 2 =>
              bus_rsp_o.data <= key_enc(95 downto 64);
            when KEY_ENC_BAR + 3 =>
              bus_rsp_o.data <= key_enc(127 downto 96);

            -- encryption plaintext
            when PT_ENC_BAR =>
              bus_rsp_o.data <= plaintext_enc(31 downto 0);
            when PT_ENC_BAR + 1 =>
              bus_rsp_o.data <= plaintext_enc(63 downto 32);
            when PT_ENC_BAR + 2 =>
              bus_rsp_o.data <= plaintext_enc(95 downto 64);
            when PT_ENC_BAR + 3 =>
              bus_rsp_o.data <= plaintext_enc(127 downto 96);

            -- decryption IV
            when IV_DEC_BAR =>
              bus_rsp_o.data <= init_vec_dec(31 downto 0);
            when IV_DEC_BAR + 1 =>
              bus_rsp_o.data <= init_vec_dec(63 downto 32);
            when IV_DEC_BAR + 2 =>
              bus_rsp_o.data <= init_vec_dec(95 downto 64);
            when IV_DEC_BAR + 3 =>
              bus_rsp_o.data <= init_vec_dec(127 downto 96);

            -- decryption key
            when KEY_DEC_BAR =>
              bus_rsp_o.data <= key_dec(31 downto 0);
            when KEY_DEC_BAR + 1 =>
              bus_rsp_o.data <= key_dec(63 downto 32);
            when KEY_DEC_BAR + 2 =>
              bus_rsp_o.data <= key_dec(95 downto 64);
            when KEY_DEC_BAR + 3 =>
              bus_rsp_o.data <= key_dec(127 downto 96);

            -- decryption ciphertext input
            when CB_DEC_BAR =>
              bus_rsp_o.data <= cipherblock_dec(31 downto 0);
            when CB_DEC_BAR + 1 =>
              bus_rsp_o.data <= cipherblock_dec(63 downto 32);
            when CB_DEC_BAR + 2 =>
              bus_rsp_o.data <= cipherblock_dec(95 downto 64);
            when CB_DEC_BAR + 3 =>
              bus_rsp_o.data <= cipherblock_dec(127 downto 96);

            -- decryption plaintext output
            when PT_DEC_BAR =>
              bus_rsp_o.data <= plaintext_dec(31 downto 0);
            when PT_DEC_BAR + 1 =>
              bus_rsp_o.data <= plaintext_dec(63 downto 32);
            when PT_DEC_BAR + 2 =>
              bus_rsp_o.data <= plaintext_dec(95 downto 64);
            when PT_DEC_BAR + 3 =>
              bus_rsp_o.data <= plaintext_dec(127 downto 96);

            when others =>
              null;
          end case;
        end if;


      end if;
    end if;
  end process bus_access;

  reset_enc <= not control_reg(0); -- Active low  
  start_enc <= control_reg(3);  
  done_enc  <= control_reg(7); 
  reset_dec <= not control_reg(15); -- Active low  
  start_dec <= control_reg(19);  
  done_dec  <= control_reg(23); 

  -- CFS Function Core ----------------------------------------------------------------------
  -- -------------------------------------------------------------------------------------------
  -- Instantiate AES core
  aes_128_inst : entity work.aes_128_top_wrapper_simple(rtl)
  generic map
  (
    MODE = "ENC_DEC"
  )
  port map
  (
    clk       => clk_i,
    reset_enc => reset_enc,
    reset_dec => reset_dec,

    -- Encryption Interface
    init_vec_enc    => init_vec_enc, 
    key_enc         => key_enc, 
    plaintext_enc   => plaintext_enc, 
    cipherblock_enc => cipherblock_enc, 
    start_enc       => start_enc, 
    done_enc        => done_enc, 

    -- Decryption Interface
    init_vec_dec    => init_vec_dec,        
    key_dec         => key_dec,   
    cipherblock_dec => cipherblock_dec,        
    plaintext_dec   => plaintext_dec,          
    start_dec       => start_dec,    
    done_dec        => done_dec 
  );

end neorv32_cfs_rtl;
