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

#include <emu.h>

#include <cpu.h>
#include <ppu.h>
#include <apu.h>

int mn_emu_init(MNEmu *emu, void draw_pixel(long int color),
                unsigned char *rom, unsigned char *palette, size_t size,
                int pal) {
    emu->pal = pal;

    if(mn_cpu_init(&emu->cpu)){
        return MN_EMU_E_CPU;
    }
    emu->cpu.irq_pin = 1; /* /IRQ is kept high */
    emu->cpu.nmi_pin = 1;
    if(mn_ppu_init(&emu->ppu, palette, draw_pixel)){
        return MN_EMU_E_PPU;
    }
    if(mn_apu_init(&emu->apu)){
        return MN_EMU_E_APU;
    }

    if(mn_mapper_find(&emu->mapper, rom, size)){
        return MN_EMU_E_MAPPER;
    }

    if(emu->mapper.init(emu, &emu->mapper, rom, size)){
        return MN_EMU_E_MAPPER;
    }

    return MN_EMU_E_NONE;
}

void mn_emu_step(MNEmu *emu) {
    mn_ppu_cycle(&emu->ppu, emu);
}

void mn_emu_cycle(MNEmu *emu) {
    /* TODO: Perform the right number of steps */
    (void)emu;
}

void mn_emu_pixel(MNEmu *emu) {
    mn_emu_step(emu);
    (void)emu;
}

void mn_emu_step_into(MNEmu *emu) {
    /* TODO: Perform the right number of steps */
    (void)emu;
}

void mn_emu_step_over(MNEmu *emu) {
    /* TODO: Perform the right number of steps */
    (void)emu;
}

void mn_emu_step_out(MNEmu *emu) {
    /* TODO: Perform the right number of steps */
    (void)emu;
}

void mn_emu_free(MNEmu *emu) {
    emu->mapper.free(emu, &emu->mapper);
    mn_cpu_free(&emu->cpu);
    mn_ppu_free(&emu->ppu);
    mn_apu_free(&emu->apu);
}
