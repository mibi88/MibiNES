/* mibines - A small NES emulator.
 * by Mibi88
 *
 * This software is licensed under the BSD-3-Clause license:
 *
 * Copyright 2025 Mibi88
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its
 * contributors may be used to endorse or promote products derived from this
 * software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <mappers/nrom.h>

#include <emu.h>

#include <ppu.h>

#include <ctrl.h>

#include <stdlib.h>

#if MN_NROM_DEBUG_RW
#include <stdio.h>
#endif

#define MN_NROM_RAM_SIZE 0x800
#define MN_NROM_VRAM_SIZE (0x400*2+0x20)

typedef struct {
    unsigned char ram[MN_NROM_RAM_SIZE];
    unsigned char vram[MN_NROM_VRAM_SIZE];
    unsigned char *rom;
    unsigned char *chr;
    size_t size;
    size_t prg_rom_start;
    size_t prg_rom_size;
    size_t chr_rom_size;
    unsigned char bus;

    unsigned int horizontal : 1;
    unsigned int chr_ram : 1;
} MNNROM;

static unsigned char mn_nrom_read(void *_emu, void *_mapper,
                                  unsigned short int addr);

static int mn_nrom_init(void *_emu, void *_mapper, unsigned char *rom,
                        size_t size) {
    MNNROM *nrom;
    MNEmu *emu = _emu;

    ((MNMapper*)_mapper)->data = malloc(sizeof(MNNROM));
    nrom = ((MNMapper*)_mapper)->data;

    if(nrom == NULL) return 1;

    mn_mapper_ram_init(nrom->ram, MN_NROM_RAM_SIZE);
    mn_mapper_ram_init(nrom->vram, MN_NROM_VRAM_SIZE);
    nrom->rom = rom;
    nrom->size = size;

    nrom->prg_rom_start = 16;

    /* NOTE: We have already checked that rom contains at least 16 bytes when
     * searching the mapper. */

    nrom->horizontal = !(rom[6]&1);
    if(rom[6]&(1<<2)){
        /* This ROM has a trainer */
        nrom->prg_rom_start += 512;
    }

    nrom->prg_rom_size = rom[4]*16*1024;

    if(!rom[5]){
        nrom->chr_ram = 1;
        nrom->chr_rom_size = 0;
        nrom->chr = malloc(0x2000);
        mn_mapper_ram_init(nrom->chr, 0x2000);
        if(nrom->chr == NULL){
            return 1;
        }
    }else{
        nrom->chr_rom_size = rom[5]*8*1024;
        nrom->chr = rom+nrom->prg_rom_start+nrom->prg_rom_size;
    }

    if(size < nrom->prg_rom_start+nrom->prg_rom_size+nrom->chr_rom_size){
        /* The ROM file is too small */
        return 1;
    }

    emu->cpu.pc = mn_nrom_read(_emu, _mapper, 0xFFFC)|
                  (mn_nrom_read(_emu, _mapper, 0xFFFD)<<8);

    return 0;
}

static unsigned char mn_nrom_read(void *_emu, void *_mapper,
                                  unsigned short int addr) {
    MNNROM *rom = ((MNMapper*)_mapper)->data;
    MNEmu *emu = _emu;

#if MN_NROM_DEBUG_RW
    printf("<- %04x\n", addr);
#endif

    if(addr >= 0x8000){
        return (rom->bus = rom->rom[rom->prg_rom_start+(addr-0x8000)%
                                          rom->prg_rom_size]);
    }else if(addr < 0x0800){
        return (rom->bus = rom->ram[addr]);
    }else if(addr < 0x2000){
        return (rom->bus = rom->ram[addr%0x0800]);
    }else if(addr < 0x4000){
        return (rom->bus = mn_ppu_read(&emu->ppu, emu, addr&7));
    }else if(addr < 0x4018){
        /* TODO: Read from the APU. */
        /* TODO: Let the APU handle $4016 and $4017. */
        /* TODO: Correctly return open bus for reads at $4016 and $4017. */
        if(addr == 0x4016){
            return (rom->bus = mn_ctrl_read(&emu->ctrl1, emu));
        }else if(addr == 0x4017){
            return (rom->bus = mn_ctrl_read(&emu->ctrl2, emu));
        }
    }else if(addr < 0x4020){
        /* CPU test mode. */
    }

    /* Unmapped space */
    return rom->bus;
}

static void mn_nrom_write(void *_emu, void *_mapper, unsigned short int addr,
                          unsigned char value) {
    MNNROM *rom = ((MNMapper*)_mapper)->data;
    MNEmu *emu = _emu;

#if MN_NROM_DEBUG_RW
    printf("*%04x = %02x\n", addr, value);
#endif

    if(addr < 0x0800){
        rom->ram[addr] = value;
        rom->bus = value;
    }else if(addr < 0x2000){
        rom->ram[addr%0x0800] = value;
        rom->bus = value;
    }else if(addr < 0x4000){
        mn_ppu_write(&emu->ppu, emu, addr&7, value);
        rom->bus = value;
    }else if(addr < 0x4018){
        /* TODO: Write to the APU. */
        /* TODO: Get rid of this if. */
        if(addr == 0x4014){
            emu->dma.page = value;
            emu->dma.do_oam_dma = 1;
        }
        /* TODO: Let the APU handle $4016. */
        if(addr == 0x4016){
            rom->bus = value;
            emu->ctrl1.strobe = value;
            emu->ctrl2.strobe = value;
        }
    }else if(addr < 0x4020){
        /* CPU test mode. */
    }

    /* Unmapped space */
}

static unsigned char mn_nrom_vram_read(void *_emu, void *_mapper,
                                       unsigned short int addr) {
    MNNROM *rom = ((MNMapper*)_mapper)->data;
    MNEmu *emu = _emu;

    if(addr < 0x2000){
        return rom->chr[addr];
    }else if(addr < 0x3000){
        if(rom->horizontal){
            return rom->vram[((addr-0x2000)&~0xC00)|((addr&0x800)>>1)];
        }else{
            return rom->vram[(addr-0x2000)&0x7FF];
        }
    }else if(addr >= 0x3F00){
        if(!(addr&3)){
            return rom->vram[0x800+(addr&0xF)];
        }
        return rom->vram[0x800+(addr&0x1F)];
    }

    return emu->ppu.io_bus;
}

static void mn_nrom_vram_write(void *_emu, void *_mapper,
                               unsigned short int addr, unsigned char value) {
    MNNROM *rom = ((MNMapper*)_mapper)->data;
    (void)_emu;

    if(addr >= 0x2000 && addr < 0x3000){
        if(rom->horizontal){
            rom->vram[((addr-0x2000)&~0xC00)|((addr&0x800)>>1)] = value;
        }else{
            rom->vram[(addr-0x2000)&0x7FF] = value;
        }
    }else if(addr >= 0x3F00){
        if(!(addr&3)){
            rom->vram[0x800+(addr&0xF)] = value;
        }
        rom->vram[0x800+(addr&0x1F)] = value;
    }else if(rom->chr_ram && addr < 0x2000){
        rom->chr[addr] = value;
    }
}

static void mn_nrom_reset(void *_emu, void *_mapper) {
    /* TODO */
    MNEmu *emu = _emu;
    (void)_mapper;

    emu->cpu.pc = mn_nrom_read(_emu, _mapper, 0xFFFC)|
                  (mn_nrom_read(_emu, _mapper, 0xFFFD)<<8);
}

static void mn_nrom_hard_reset(void *_emu, void *_mapper) {
    /* TODO */
    MNEmu *emu = _emu;
    (void)_mapper;

    emu->cpu.pc = mn_nrom_read(_emu, _mapper, 0xFFFC)|
                  (mn_nrom_read(_emu, _mapper, 0xFFFD)<<8);
}

void mn_nrom_free(void *_emu, void *_mapper) {
    MNNROM *rom = ((MNMapper*)_mapper)->data;
    (void)_emu;
    free(rom);
}

MNMapper mn_mapper_nrom = {
    mn_nrom_init,
    mn_nrom_read,
    mn_nrom_write,
    mn_nrom_vram_read,
    mn_nrom_vram_write,
    mn_nrom_reset,
    mn_nrom_hard_reset,
    mn_nrom_free,
    NULL
};
