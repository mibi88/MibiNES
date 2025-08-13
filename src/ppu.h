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

#ifndef MN_PPU_H
#define MN_PPU_H

#include <emu.h>

#define MN_PPU_DEBUG_FETCH 0
#define MN_PPU_DEBUG_PIXEL 0
#define MN_PPU_DEBUG_SPRITE_EVAL 0

enum {
    MN_PPU_CTRL,
    MN_PPU_MASK,
    MN_PPU_STATUS,
    MN_PPU_OAMADDR,
    MN_PPU_OAMDATA,
    MN_PPU_PPUSCROLL,
    MN_PPU_PPUADDR,
    MN_PPU_PPUDATA
};

enum {
    /* TODO: Add masks for all other flags */
    MN_PPU_CTRL_INC = 1<<2,
    MN_PPU_CTRL_NMI = 1<<7
};

enum {
    /* TODO: Add masks for all other flags */
    MN_PPU_MASK_GRAYSCALE = 1,
    MN_PPU_MASK_BACKGROUND = 1<<3,
    MN_PPU_MASK_SPRITES = 1<<4,
    MN_PPU_MASK_RENDER = 3<<3  /* The ppu renders if both the 3rd and the 4th
                                * bit are on. If both are off a backdrop color
                                * is shown. */
};

int mn_ppu_init(MNPPU *ppu, unsigned char *palette,
                void draw_pixel(long int color));
void mn_ppu_cycle(MNPPU *ppu, MNEmu *emu);
unsigned char mn_ppu_read(MNPPU *ppu, MNEmu *emu, unsigned short int reg);
void mn_ppu_write(MNPPU *ppu, MNEmu *emu, unsigned short int reg,
                  unsigned char value);
void mn_ppu_free(MNPPU *ppu);

#endif /* MN_PPU_H */
