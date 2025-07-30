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

#include <ppu.h>

#include <cpu.h>

int mn_ppu_init(MNPPU *ppu, void draw_pixel(long int color)) {
    /* TODO */
    ppu->draw_pixel = draw_pixel;
    ppu->cycle = 0;

    ppu->since_start = 0;
    ppu->startup_time = 29658;

    return 0;
}

/*
 * Border region:
 * 16 pixels left
 * 11 pixels right
 * 2 pixels  down
 *
 */
void mn_ppu_cycle(MNPPU *ppu, MNEmu *emu) {
    /* TODO */
    if(ppu->cycle >= 3){
        mn_cpu_cycle(&emu->cpu, emu);
        ppu->cycle = 0;
    }

    ppu->draw_pixel(0xAAAAAA);

    ppu->cycle++;
}

unsigned char mn_ppu_read(MNPPU *ppu, unsigned short int reg) {
    switch(reg){
        case MN_PPU_CTRL:
            break;
        case MN_PPU_MASK:
            break;
        case MN_PPU_STATUS:
            break;
        case MN_PPU_OAMADDR:
            break;
        case MN_PPU_OAMDATA:
            break;
        case MN_PPU_PPUSCROLL:
            break;
        case MN_PPU_PPUADDR:
            break;
        case MN_PPU_PPUDATA:
            break;
    }
    return 0;
}

void mn_ppu_write(MNPPU *ppu, unsigned short int reg, unsigned char value) {
    switch(reg){
        case MN_PPU_CTRL:
            if(ppu->since_start < ppu->startup_time) break;
            break;
        case MN_PPU_MASK:
            if(ppu->since_start < ppu->startup_time) break;
            break;
        case MN_PPU_STATUS:
            break;
        case MN_PPU_OAMADDR:
            break;
        case MN_PPU_OAMDATA:
            break;
        case MN_PPU_PPUSCROLL:
            if(ppu->since_start < ppu->startup_time) break;
            break;
        case MN_PPU_PPUADDR:
            if(ppu->since_start < ppu->startup_time) break;
            break;
        case MN_PPU_PPUDATA:
            break;
    }
}

void mn_ppu_free(MNPPU *ppu) {
    /* TODO */
    (void)ppu;
}
