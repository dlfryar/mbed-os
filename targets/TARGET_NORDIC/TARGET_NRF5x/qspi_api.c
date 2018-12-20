/*
 * Copyright (c) 2017 Nordic Semiconductor ASA
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 *   1. Redistributions of source code must retain the above copyright notice, this list
 *      of conditions and the following disclaimer.
 *
 *   2. Redistributions in binary form, except as embedded into a Nordic Semiconductor ASA
 *      integrated circuit in a product or a software update for such product, must reproduce
 *      the above copyright notice, this list of conditions and the following disclaimer in
 *      the documentation and/or other materials provided with the distribution.
 *
 *   3. Neither the name of Nordic Semiconductor ASA nor the names of its contributors may be
 *      used to endorse or promote products derived from this software without specific prior
 *      written permission.
 *
 *   4. This software, with or without modification, must only be used with a
 *      Nordic Semiconductor ASA integrated circuit.
 *
 *   5. Any software provided in binary or object form under this license must not be reverse
 *      engineered, decompiled, modified and/or disassembled.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include "qspi_api.h"

#if DEVICE_QSPI

#include "nrf_drv_common.h"
#include "nrf_drv_qspi.h"

/* 
TODO
    - config inside obj - nordic headers have some problems with inclusion
    - free - is it really empty, nothing to do there?
    - prepare command - support more protocols that nordic can do (now limited)
    - nordic does not support
        - alt
        - dummy cycles
*/

#define MBED_HAL_QSPI_MAX_FREQ          32000000UL

// NRF supported R/W opcodes
#define FASTREAD_opcode     0x0B
#define READ2O_opcode       0x3B
#define READ2IO_opcode      0xBB
#define READ4O_opcode       0x6B
#define READ4IO_opcode      0xEB
#define READSFDP_opcode     0x5A

#define PP_opcode           0x02
#define PP2O_opcode         0xA2
#define PP4O_opcode         0x32
#define PP4IO_opcode        0x38

#define SCK_DELAY           0x05
#define WORD_MASK           0x03

#define SFDP_LENGTH         0x04

static nrf_drv_qspi_config_t config;

// Private helper function to track initialization
static ret_code_t _qspi_drv_init(void);
// Private helper function to set NRF frequency divider
nrf_qspi_frequency_t nrf_frequency(int hz);
// Private helper function to read SFDP data
qspi_status_t sfdp_read(qspi_t *obj, const qspi_command_t *command, void *data, size_t *length);

qspi_status_t qspi_prepare_command(qspi_t *obj, const qspi_command_t *command, bool write) 
{
    // we need to remap opcodes to NRF ID's
    // most commmon are 1-1-1, 1-1-4, 1-4-4

    // 1-1-1
    if (command->instruction.bus_width == QSPI_CFG_BUS_SINGLE &&
        command->address.bus_width == QSPI_CFG_BUS_SINGLE &&
        command->data.bus_width == QSPI_CFG_BUS_SINGLE) {
        if (write) {
            if (command->instruction.value == PP_opcode) {
                config.prot_if.writeoc = NRF_QSPI_WRITEOC_PP;
            } else {
                return QSPI_STATUS_INVALID_PARAMETER;
            }
        } else {
            if (command->instruction.value == FASTREAD_opcode ||
                command->instruction.value == READSFDP_opcode) {
                config.prot_if.readoc = NRF_QSPI_READOC_FASTREAD;
            } else {
                return QSPI_STATUS_INVALID_PARAMETER;
            }
        }
    // 1-1-4
    } else if (command->instruction.bus_width == QSPI_CFG_BUS_SINGLE &&
        command->address.bus_width == QSPI_CFG_BUS_SINGLE &&
        command->data.bus_width == QSPI_CFG_BUS_QUAD) {
        // 1_1_4
        if (write) {
            if (command->instruction.value == PP4O_opcode) {
                config.prot_if.writeoc = NRF_QSPI_WRITEOC_PP4O;
            } else {
                return QSPI_STATUS_INVALID_PARAMETER;
            }
        } else {
            if (command->instruction.value == READ4O_opcode) {
                config.prot_if.readoc = NRF_QSPI_READOC_READ4O;
            } else {
                return QSPI_STATUS_INVALID_PARAMETER;
            }
        }
    // 1-4-4
    } else if (command->instruction.bus_width == QSPI_CFG_BUS_SINGLE &&
        command->address.bus_width == QSPI_CFG_BUS_QUAD &&
        command->data.bus_width == QSPI_CFG_BUS_QUAD) {
        // 1_4_4
        if (write) {
            if (command->instruction.value == PP4IO_opcode) {
                config.prot_if.writeoc = NRF_QSPI_WRITEOC_PP4IO;
            } else {
                return QSPI_STATUS_INVALID_PARAMETER;
            }
        } else {
            if (command->instruction.value == READ4IO_opcode) {
                config.prot_if.readoc = NRF_QSPI_READOC_READ4IO;
            } else {
                return QSPI_STATUS_INVALID_PARAMETER;
            }
        }
    // 1-1-2
    } else if (command->instruction.bus_width == QSPI_CFG_BUS_SINGLE &&
        command->address.bus_width == QSPI_CFG_BUS_SINGLE &&
        command->data.bus_width == QSPI_CFG_BUS_DUAL) {
        // 1-1-2
        if (write) {
            if (command->instruction.value == PP2O_opcode) {
                config.prot_if.writeoc = NRF_QSPI_WRITEOC_PP2O;
            } else {
                return QSPI_STATUS_INVALID_PARAMETER;
            }
        } else {
            if (command->instruction.value == READ2O_opcode) {
                config.prot_if.readoc = NRF_QSPI_READOC_READ2O;
            } else {
                return QSPI_STATUS_INVALID_PARAMETER;
            }
        }
    // 1-2-2
    } else if (command->instruction.bus_width == QSPI_CFG_BUS_SINGLE &&
        command->address.bus_width == QSPI_CFG_BUS_DUAL &&
        command->data.bus_width == QSPI_CFG_BUS_DUAL) {
        // 1-2-2
        if (write) {
            // 1-2-2 write is not supported
            return QSPI_STATUS_INVALID_PARAMETER;
        } else {
            if (command->instruction.value == READ2IO_opcode) {
                config.prot_if.readoc = NRF_QSPI_READOC_READ2IO;
            } else {
                return QSPI_STATUS_INVALID_PARAMETER;
            }
        }
    } else {
        return QSPI_STATUS_INVALID_PARAMETER;
    }
    
    // supporting only 24 or 32 bit address
    if (command->address.size == QSPI_CFG_ADDR_SIZE_24) {
        config.prot_if.addrmode = NRF_QSPI_ADDRMODE_24BIT;
    } else if (command->address.size == QSPI_CFG_ADDR_SIZE_32) {
        config.prot_if.addrmode = NRF_QSPI_ADDRMODE_32BIT;
    } else {
        return QSPI_STATUS_INVALID_PARAMETER;
    }
    
    //Configure QSPI with new command format
    ret_code_t ret_status = _qspi_drv_init();
    if (ret_status != NRF_SUCCESS ) {
        if (ret_status == NRF_ERROR_INVALID_PARAM) {
            return QSPI_STATUS_INVALID_PARAMETER;
        } else {
            return QSPI_STATUS_ERROR;
        }
    }
    
    return QSPI_STATUS_OK;
}

qspi_status_t qspi_init(qspi_t *obj, PinName io0, PinName io1, PinName io2, PinName io3, PinName sclk, PinName ssel, uint32_t hz, uint8_t mode)
{
    (void)(obj);
    if (hz > MBED_HAL_QSPI_MAX_FREQ) {
        return QSPI_STATUS_INVALID_PARAMETER;
    }

    // memset(config, 0, sizeof(config));

    config.pins.sck_pin = (uint32_t)sclk;
    config.pins.csn_pin = (uint32_t)ssel;
    config.pins.io0_pin = (uint32_t)io0;
    config.pins.io1_pin = (uint32_t)io1;
    config.pins.io2_pin = (uint32_t)io2;
    config.pins.io3_pin = (uint32_t)io3;
    config.irq_priority = SPI_DEFAULT_CONFIG_IRQ_PRIORITY;

    config.phy_if.sck_freq  = nrf_frequency(hz);
    config.phy_if.sck_delay = SCK_DELAY;
    config.phy_if.dpmen = false;
    config.phy_if.spi_mode = mode == 0 ? NRF_QSPI_MODE_0 : NRF_QSPI_MODE_1;

    //Use _qspi_drv_init private function to initialize
    ret_code_t ret = _qspi_drv_init();
    if (ret == NRF_SUCCESS ) {
        return QSPI_STATUS_OK;
    } else if (ret == NRF_ERROR_INVALID_PARAM) {
        return QSPI_STATUS_INVALID_PARAMETER;
    } else {
        return QSPI_STATUS_ERROR;
    }
}

qspi_status_t qspi_free(qspi_t *obj)
{
    (void)(obj);
    // possibly here uninit from SDK driver
    return QSPI_STATUS_OK;
}

qspi_status_t qspi_frequency(qspi_t *obj, int hz)
{
    config.phy_if.sck_freq  = nrf_frequency(hz);

    // use sync version, no handler
    ret_code_t ret = _qspi_drv_init();
    if (ret == NRF_SUCCESS ) {
        return QSPI_STATUS_OK;
    } else if (ret == NRF_ERROR_INVALID_PARAM) {
        return QSPI_STATUS_INVALID_PARAMETER;
    } else {
        return QSPI_STATUS_ERROR;
    }
}

qspi_status_t qspi_write(qspi_t *obj, const qspi_command_t *command, const void *data, size_t *length)
{
    // length needs to be rounded up to the next WORD (4 bytes)
    if ((*length & WORD_MASK) > 0) {
        return QSPI_STATUS_INVALID_PARAMETER;
    }
		
    qspi_status_t status = qspi_prepare_command(obj, command, true);
    if (status != QSPI_STATUS_OK) {
        return status;
    }

    // write here does not return how much it transfered, we return transfered all
    ret_code_t ret = nrf_drv_qspi_write(data, *length, command->address.value);
    if (ret == NRF_SUCCESS ) {
        return QSPI_STATUS_OK;
    } else {
        return QSPI_STATUS_ERROR;
    }
}

qspi_status_t qspi_read(qspi_t *obj, const qspi_command_t *command, void *data, size_t *length)
{
    // length needs to be rounded up to the next WORD (4 bytes)
    if ((*length & WORD_MASK) > 0) {
        return QSPI_STATUS_INVALID_PARAMETER;
    }
    
    // check to see if this is an SFDP read
    if (command->instruction.value == READSFDP_opcode) {
        // send the SFDP command
        qspi_status_t status = sfdp_read(obj, command, data, length );
        return status;
    } else {
        qspi_status_t status = qspi_prepare_command(obj, command, false);
        if (status != QSPI_STATUS_OK) {
            return status;
        }
    }

    ret_code_t ret = nrf_drv_qspi_read(data, *length, command->address.value);
    if (ret == NRF_SUCCESS ) {
        return QSPI_STATUS_OK;
    } else {
        return QSPI_STATUS_ERROR;
    }
}

qspi_status_t qspi_command_transfer(qspi_t *obj, const qspi_command_t *command, const void *tx_data, size_t tx_size, void *rx_data, size_t rx_size)
{
    ret_code_t ret_code;
    uint32_t data_size = tx_size + rx_size;
    uint8_t data[8];
    
    nrf_qspi_cinstr_conf_t qspi_cinstr_config;
    qspi_cinstr_config.opcode    = command->instruction.value;
    qspi_cinstr_config.io2_level = true;
    qspi_cinstr_config.io3_level = true;
    qspi_cinstr_config.wipwait   = false;
    qspi_cinstr_config.wren      = false;

    if(!command->address.disabled && data_size == 0) {
        // erase command with address
        if (command->address.size == QSPI_CFG_ADDR_SIZE_24) {
            qspi_cinstr_config.length = NRF_QSPI_CINSTR_LEN_4B;
        } else if (command->address.size == QSPI_CFG_ADDR_SIZE_32) {
            qspi_cinstr_config.length = NRF_QSPI_CINSTR_LEN_5B;
        } else {
            return QSPI_STATUS_INVALID_PARAMETER;
        }
        uint32_t address_size = (uint32_t)qspi_cinstr_config.length - 1;
        uint8_t *address_bytes = (uint8_t *)&command->address.value;
        for (uint32_t i = 0; i < address_size; ++i) {
            data[i] = address_bytes[address_size - 1 - i];
        }
    } else if (data_size < 9) {
        qspi_cinstr_config.length = (nrf_qspi_cinstr_len_t)(NRF_QSPI_CINSTR_LEN_1B + data_size);
        // preparing data to send
        for (uint32_t i = 0; i < tx_size; ++i) {
            data[i] = ((uint8_t *)tx_data)[i];
        }
    } else {
        return QSPI_STATUS_ERROR;
    }
 
    ret_code = nrf_drv_qspi_cinstr_xfer(&qspi_cinstr_config, data, data);
    if (ret_code != NRF_SUCCESS) {
        return QSPI_STATUS_ERROR;
    }
 
    // preparing received data
    for (uint32_t i = 0; i < rx_size; ++i) {
        // Data is sending as a normal SPI transmission so there is one buffer to send and receive data.
        ((uint8_t *)rx_data)[i] = data[i];
    }

    return QSPI_STATUS_OK;
}

// Private helper function to track initialization
static ret_code_t _qspi_drv_init(void) 
{
    static bool _initialized = false;
    ret_code_t ret = NRF_ERROR_INVALID_STATE;
    
    if(_initialized) {
        //NRF implementation prevents calling init again. But we need to call init again to program the new command settings in the IFCONFIG registers. 
        //So, we have to uninit qspi first and call init again. 
        nrf_drv_qspi_uninit();
    }
    ret = nrf_drv_qspi_init(&config, NULL , NULL);
    if( ret == NRF_SUCCESS )
        _initialized = true;
    return ret;
}

// Private helper to set NRF frequency divider
nrf_qspi_frequency_t nrf_frequency(int hz)
{
    nrf_qspi_frequency_t freq = NRF_QSPI_FREQ_32MDIV16;

    // Convert hz to closest NRF frequency divider
    if (hz < 2130000)
        freq = NRF_QSPI_FREQ_32MDIV16; // 2.0 MHz, minimum supported frequency
    else if (hz < 2290000)
        freq = NRF_QSPI_FREQ_32MDIV15; // 2.13 MHz
    else if (hz < 2460000)
        freq = NRF_QSPI_FREQ_32MDIV14; // 2.29 MHz
    else if (hz < 2660000)
        freq = NRF_QSPI_FREQ_32MDIV13; // 2.46 Mhz
    else if (hz < 2900000)
        freq = NRF_QSPI_FREQ_32MDIV12; // 2.66 MHz
    else if (hz < 3200000)
        freq = NRF_QSPI_FREQ_32MDIV11; // 2.9 MHz
    else if (hz < 3550000)
        freq = NRF_QSPI_FREQ_32MDIV10; // 3.2 MHz
    else if (hz < 4000000)
        freq = NRF_QSPI_FREQ_32MDIV9; // 3.55 MHz
    else if (hz < 4570000)
        freq = NRF_QSPI_FREQ_32MDIV8; // 4.0 MHz
    else if (hz < 5330000)
        freq = NRF_QSPI_FREQ_32MDIV7; // 4.57 MHz
    else if (hz < 6400000)
        freq = NRF_QSPI_FREQ_32MDIV6; // 5.33 MHz
    else if (hz < 8000000)
        freq = NRF_QSPI_FREQ_32MDIV5; // 6.4 MHz
    else if (hz < 10600000)
        freq = NRF_QSPI_FREQ_32MDIV4; // 8.0 MHz
    else if (hz < 16000000)
        freq = NRF_QSPI_FREQ_32MDIV3; // 10.6 MHz
    else if (hz < 32000000)
        freq = NRF_QSPI_FREQ_32MDIV2; // 16 MHz
    else
        freq = NRF_QSPI_FREQ_32MDIV1; // 32 MHz

    return freq;
}

// Private helper to read nRF SFDP data
qspi_status_t sfdp_read(qspi_t *obj, const qspi_command_t *command, void *data, size_t *length)
{
    static uint32_t offset = 0;
    static bool b_init = false;
    
    // SFDP data captured from nRF52840 Macronix MX25R6435F using SPI
    char sfdp_rx[120] =   { 0x53, 0x46, 0x44, 0x50, 0x6, 0x1, 0x2, 0xFF,
                            0x0, 0x6, 0x1, 0x10, 0x30, 0x0, 0x0, 0xFF,
                            0xC2, 0x0, 0x1, 0x4, 0x10, 0x1, 0x0, 0xFF,
                            0x84, 0x0, 0x1, 0x2, 0xC0, 0x0, 0x0, 0xFF,
                            0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
                            0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
                            0xE5, 0x20, 0xF1, 0xFF, 0xFF, 0xFF, 0xFF, 0x3,
                            0x44, 0xEB, 0x8, 0x6B, 0x8, 0x3B, 0x4, 0xBB,
                            0xEE, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x0, 0xFF,
                            0xFF, 0xFF, 0x0, 0xFF, 0xC, 0x20, 0xF, 0x52,
                            0x10, 0xD8, 0x0, 0xFF, 0x23, 0x72, 0xF5, 0x0,
                            0x82, 0xED, 0x4, 0xCC, 0x44, 0x83, 0x48, 0x44,
                            0x30, 0xB0, 0x30, 0xB0, 0xF7, 0xC4, 0xD5, 0x5C,
                            0x0, 0xBE, 0x29, 0xFF, 0xF0, 0xD0, 0xFF, 0xFF };
    
    // SFDP header information captured from the custom mm board using SPI
    char mm_rx[120] =   { 0x53, 0x46, 0x44, 0x50, 0x6, 0x1, 0x1, 0xFF, 
                            0x0, 0x6, 0x1, 0x10, 0x30, 0x0, 0x0, 0xFF, 
                            0x9D, 0x5, 0x1, 0x3, 0x80, 0x0, 0x0, 0x2, 
                            0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 
                            0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 
                            0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 
                            0xE5, 0x20, 0xF9, 0xFF, 0xFF, 0xFF, 0xFF, 0x3, 
                            0x44, 0xEB, 0x8, 0x6B, 0x8, 0x3B, 0x80, 0xBB, 
                            0xFE, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x0, 0xFF, 
                            0xFF, 0xFF, 0x44, 0xEB, 0xC, 0x20, 0xF, 0x52, 
                            0x10, 0xD8, 0x0, 0xFF, 0x23, 0x4A, 0xC9, 0x0, 
                            0x82, 0xD8, 0x11, 0xC3, 0xCC, 0xCD, 0x68, 0x46, 
                            0x7A, 0x75, 0x7A, 0x75, 0xF7, 0xA2, 0xD5, 0x5C, 
                            0x4A, 0x42, 0x2C, 0xFF, 0xF0, 0x30, 0xC0, 0x80 }; 
    
    // SFDP header is read 8 bytes at a time, from the beginning of the data
    if (*length < (sfdp_rx[11] * 4) ) {
        if (offset > 24) {
            offset = 0;
        }
        memcpy(data, (sfdp_rx + offset), *length);
        offset += 8;
    }
    // SFDP paramter table length in DWORDs (4 bytes) is read at index 11
    else if (*length == (sfdp_rx[11] * 4)){
        // SFDP parameters are read at the offset specified at index 12
        memcpy(data, (sfdp_rx + sfdp_rx[12]), *length);
    }
    
    return QSPI_STATUS_OK;
}


#endif

/** @}*/
