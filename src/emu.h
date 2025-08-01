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
    unsigned short int tmp, tmp2;

    int jammed;

    unsigned int irq_pin : 1;
    unsigned int nmi_pin : 1;
    unsigned int nmi_pin_last : 1;
    unsigned int should_nmi : 1;
    unsigned int should_irq : 1;
    unsigned int nmi_detected : 1;
    unsigned int irq_detected : 1;

    unsigned int execute_int_next : 1;
    unsigned int execute_int: 1;

    unsigned int is_irq : 1;
} MNCPU;

typedef struct {
    unsigned char io_bus;
    unsigned char video_mem_bus;
    unsigned int addr : 16;

    unsigned char cycles_since_cpu_cycle;

    unsigned short int scanline;
    unsigned short int cycle;

    unsigned short int since_start;

    unsigned short int startup_time;

    /* Internal registers */
    unsigned int v : 15; /* Current vram address */
    unsigned int t : 15; /* Temorary vram address */
    unsigned int x : 3;  /* Fine X scroll */
    unsigned int w : 1;  /* First or second write toggle */

    unsigned char tile_id;
    unsigned char attr;
    /* The low and high bitplanes for that tile ID */
    unsigned char low_bp, high_bp;

    unsigned int low_shift : 16;
    unsigned int high_shift : 16;

    unsigned int attr_latch1 : 1;
    unsigned int attr_latch2 : 1;

    unsigned int attr1_shift : 8;
    unsigned int attr2_shift : 8;

    /* Pixel output is delayed 4 cycles further. */
    unsigned char pixel_out[4];

    unsigned char ext;

    unsigned int even_frame : 1;

    unsigned int trigger_nmi : 1;

    unsigned int vblank : 1;
    unsigned int sprite0_hit : 1;
    unsigned int sprite_overflow : 1;

    unsigned int keep_vblank_clear : 1;

    unsigned char ctrl;
    unsigned char mask;

    unsigned char *palette;

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
                unsigned char *rom, unsigned char *palette, size_t size,
                int pal);
void mn_emu_pixel(MNEmu *emu);
void mn_emu_frame(MNEmu *emu);
void mn_emu_free(MNEmu *emu);

#endif /* MN_EMU_H */
