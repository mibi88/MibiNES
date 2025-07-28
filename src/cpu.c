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

#include <cpu.h>

#include <emu.h>

int mn_cpu_init(MNCPU *cpu) {
    cpu->pc = 0x8000;
    cpu->jammed = 0;

    cpu->cycle = 1;
    cpu->target_cycle = 0;

    return 0;
}

/* TODO: Put all structs that read RAM in emu.h to avoid using void
 * pointers. */
void mn_cpu_cycle(MNCPU *cpu, void *_emu) {
    /* Emulated the 6502 as described at https://www.nesdev.org/6502_cpu.txt */

    MNEmu *emu = _emu;

    if(cpu->jammed) return;

    if(cpu->cycle > cpu->target_cycle){
        cpu->opcode = emu->mapper.read(emu, &emu->mapper, cpu->pc);
        cpu->pc++;
        cpu->t = emu->mapper.read(emu, &emu->mapper, cpu->pc);
        cpu->cycle = 2;
    }

    switch(cpu->opcode){
        case 0x00:
            /* BRK */
            switch(cpu->cycle){
                case 2:
                    cpu->target_cycle = 7;
                    cpu->pc++;
                    break;
                case 3:
                    emu->mapper.write(emu, &emu->mapper, 0x0100+cpu->s,
                                      cpu->pc>>8);
                    cpu->s--;
                    cpu->p |= MN_CPU_B;
                    break;
                case 4:
                    emu->mapper.write(emu, &emu->mapper, 0x0100+cpu->s,
                                      cpu->pc);
                    cpu->s--;
                    break;
                case 5:
                    emu->mapper.write(emu, &emu->mapper, 0x0100+cpu->s,
                                      cpu->p);
                    cpu->s--;
                    break;
                case 6:
                    cpu->pc &= 0xFF00;
                    cpu->pc |= emu->mapper.read(emu, &emu->mapper, 0xFFFE);
                    break;
                case 7:
                    cpu->pc |= emu->mapper.read(emu, &emu->mapper, 0xFFFF)<<8;
                    break;
            }
            break;
        default:
            /* Unknown opcode, jam the CPU for now */
            cpu->jammed = 1;
    }

    cpu->cycle++;
}

void mn_cpu_free(MNCPU *cpu) {
    /* TODO */
    (void)cpu;
}
