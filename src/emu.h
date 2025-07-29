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

#ifndef MN_EMU_H
#define MN_EMU_H

#include <mapper.h>

typedef struct {
    /* Registers */
    unsigned short int pc;
    unsigned char s;
    unsigned char p;
    unsigned char a;
    unsigned char x;
    unsigned char y;

    unsigned char cycle;
    unsigned char target_cycle;

    unsigned char opcode, t;

    int jammed;
} MNCPU;

typedef struct {
    unsigned char io_bus;
    unsigned char video_mem_bus;

    unsigned char cycle;

    void (*draw_pixel)(long int color);
} MNPPU;

typedef struct {
    /* TODO */

    size_t channel_num;
} MNAPU;

typedef struct {
    MNCPU cpu;
    MNPPU ppu;
    MNAPU apu;
    MNMapper mapper;

    int pal;
} MNEmu;

enum {
    MN_EMU_E_NONE,
    MN_EMU_E_CPU,
    MN_EMU_E_PPU,
    MN_EMU_E_APU,
    MN_EMU_E_MAPPER,

    MN_EMU_E_AMOUNT
};

int mn_emu_init(MNEmu *emu, void draw_pixel(long int color),
                unsigned char *rom, size_t size, int pal);
void mn_emu_pixel(MNEmu *emu);
void mn_emu_free(MNEmu *emu);

#endif /* MN_EMU_H */
