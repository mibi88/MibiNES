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
    int halted;

    unsigned int rdy : 1;

    unsigned char last_read;

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
    unsigned char primary_oam[256];
    unsigned char secondary_oam[32];

    unsigned char secondary_oam_pos;

    unsigned char y;

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

    unsigned int n : 6;
    unsigned int m : 2;
    unsigned int entries_read : 2;

    unsigned char oamaddr;

    unsigned char b;

    /* See https://github.com/emu-russia/breaks/blob/master/BreakingNESWiki_Dee
     * pL/PPU/fifo.md */
    struct {
        unsigned char low_bp;
        unsigned char high_bp;
        /* The down counter is initialized to the X position of the sprite and
         * counts down on each pixel. Once it reaches 0 the sprite starts
         * rendering. */
        short int down_counter;
        unsigned int palette : 2;
        unsigned int priority : 1;
    } sprite_fifo[8];

    unsigned int big_sprites : 1;

    unsigned char step;

    /* Pixel output is delayed 4 cycles further. */
    unsigned char pixel_out[4];

    unsigned char ext;

    unsigned char read_buffer;

    unsigned int even_frame : 1;

    unsigned int trigger_nmi : 1;

    unsigned int vblank : 1;
    unsigned int sprite0_hit : 1;
    unsigned int sprite_overflow : 1;

    unsigned int keep_vblank_clear : 1;

    unsigned char ctrl;
    unsigned char mask;

    unsigned int sprite0_loaded : 1;

    unsigned char *palette;

    void (*draw_pixel)(long int color);
} MNPPU;

typedef struct {
    /* TODO */

    size_t channel_num;
} MNAPU;

/* XXX: Is it a good idea to split DMA from the rest of the CPU? */
typedef struct {
    unsigned int cycle : 1;
    unsigned char value;

    unsigned char step;

    unsigned int aligned : 1;

    unsigned int do_oam_dma : 1;
    unsigned int do_dmc_dma : 1;

    unsigned char page;
} MNDMA;

typedef struct {
    unsigned int strobe : 1;
    unsigned char reg;

    int (*init)(void *_ctrl, void *_emu);
    unsigned char (*load_reg)(void *_ctrl, void *_emu);
    unsigned char (*shift_reg)(void *_ctrl, void *_emu);
    unsigned char (*read)(void *_ctrl, void *_emu);
    void (*free)(void *_ctrl, void *_emu);

    /* This function can take any number and types of arguments to keep things
     * flexible, in case I want to emulate other peripherals than the standard
     * NES controller. */
    unsigned char (*get_input)();

    void *data;
} MNCtrl;

typedef struct {
    MNCPU cpu;
    MNPPU ppu;
    MNAPU apu;
    MNDMA dma;

    MNCtrl ctrl1;
    MNCtrl ctrl2;

    MNMapper mapper;

    int pal;
} MNEmu;

enum {
    MN_EMU_E_NONE,
    MN_EMU_E_CPU,
    MN_EMU_E_PPU,
    MN_EMU_E_APU,
    MN_EMU_E_DMA,
    MN_EMU_E_MAPPER,
    MN_EMU_E_CTRL,

    MN_EMU_E_AMOUNT
};

int mn_emu_init(MNEmu *emu, void draw_pixel(long int color),
                unsigned char player1_input(), unsigned char player2_input(),
                MNCtrl ctrl1_type, MNCtrl ctrl2_type, unsigned char *rom,
                unsigned char *palette, size_t size, int pal);
void mn_emu_pixel(MNEmu *emu);
void mn_emu_frame(MNEmu *emu);
void mn_emu_free(MNEmu *emu);

#endif /* MN_EMU_H */
