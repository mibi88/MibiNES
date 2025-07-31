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

int mn_ppu_init(MNPPU *ppu, unsigned char *palette,
                void draw_pixel(long int color)) {
    /* TODO */
    ppu->draw_pixel = draw_pixel;
    ppu->cycles_since_cpu_cycle = 0;

    ppu->since_start = 0;
    ppu->startup_time = 29658;

    ppu->cycle = 0;
    ppu->scanline = 261;

    ppu->palette = palette;

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
    MNCPU *cpu = &emu->cpu;

    if(ppu->scanline <= 239 || ppu->scanline == 261){
        /* Visible scanlines or pre-render scanline */

        if(ppu->scanline == 261){
            if(ppu->cycle == 1){
                ppu->vblank = 0;
                cpu->nmi_pin = 0;
            }
            if(ppu->cycle >= 280 && cpu->cycle <= 304){
                /* The PPU repeatedly copies these bits in these cycles of the
                 * pre-render scanline. */
                ppu->v &= ~((((1<<4)-1)<<11)|((1<<5)-1)<<5);
                ppu->v |= cpu->t&((((1<<4)-1)<<11)|((1<<5)-1)<<5);
            }
        }
        if(ppu->cycle == 0){
            /* Cycle 0 is an IDLE cycle */
        }else if(ppu->cycle <= 256){
            if(ppu->cycle == 256){
                /* Increment Y */
            }
        }else if(ppu->cycle == 257){
            ppu->v &= ~(((1<<5)-1)|(1<<10));
            ppu->v |= ppu->t&(((1<<5)-1)|(1<<10));
        }
    }else if(ppu->scanline == 240){
        /* Post-render scanline */
    }else if(ppu->scanline <= 260){
        if(ppu->scanline == 241 && ppu->cycle == 1){
            /* Set the VBlank flag and trigger NMI */
            ppu->vblank = 1;
            cpu->nmi_pin = 1;
        }
        /* Vertical blanking lines */
    }

    ppu->cycle++;
    if(ppu->cycle > 240){
        ppu->cycle = 0;
        ppu->scanline++;
        if(ppu->scanline > 261) ppu->scanline = 0;
    }

    /* Let the CPU run all 3 PPU cycles */
    if(ppu->cycles_since_cpu_cycle >= 3){
        mn_cpu_cycle(&emu->cpu, emu);
        ppu->cycles_since_cpu_cycle = 0;
    }

    ppu->cycles_since_cpu_cycle++;
}

unsigned char mn_ppu_read(MNPPU *ppu, unsigned short int reg) {
    switch(reg){
        case MN_PPU_CTRL:
            break;
        case MN_PPU_MASK:
            break;
        case MN_PPU_STATUS:
            return 0xFF;
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
            ppu->t &= (1<<10)|(1<<11);
            ppu->t |= (value&3)<<10;
            break;
        case MN_PPU_MASK:
            if(ppu->since_start < ppu->startup_time) break;
            break;
        case MN_PPU_STATUS:
            ppu->w = 0;
            break;
        case MN_PPU_OAMADDR:
            break;
        case MN_PPU_OAMDATA:
            break;
        case MN_PPU_PPUSCROLL:
            if(ppu->since_start < ppu->startup_time) break;
            if(ppu->w){
                /* 2nd write */
                ppu->t &= ~((7<<12)|(((1<<5)-1)<<5));
                ppu->t |= (value&7)<<12;
                ppu->t |= (value&((1<<5)-1))<<5;
                ppu->w = 0;
            }else{
                ppu->t &= ~7;
                ppu->t |= value>>3;
                ppu->x = value;
                ppu->w = 1;
            }
            break;
        case MN_PPU_PPUADDR:
            if(ppu->since_start < ppu->startup_time) break;
            if(ppu->w){
                /* 2nd write */
                ppu->t &= ~0xFF;
                ppu->t |= value;
                ppu->v = ppu->t;
                ppu->w = 0;
            }else{
                ppu->t &= ~(((1<<7)-1)<<8);
                ppu->t |= value&((1<<6)-1);
                ppu->w = 1;
            }
            break;
        case MN_PPU_PPUDATA:
            if(ppu->scanline < 240 || ppu->scanline == 261){
                /* The PPU is rendering */
                /* Perform coarse X increment and Y increment + trigger a load
                 * next value */
            }else{
                ppu->v += ppu->ctrl&MN_PPU_CTRL_INC ? 32 : 1;
            }
            break;
    }
}

void mn_ppu_free(MNPPU *ppu) {
    /* TODO */
    (void)ppu;
}
