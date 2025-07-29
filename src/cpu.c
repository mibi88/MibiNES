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

int mn_cpu_init(MNCPU *cpu) {
    cpu->pc = 0x8000;
    cpu->jammed = 0;

    cpu->cycle = 8;
    cpu->target_cycle = 0;

    return 0;
}

void mn_cpu_cycle(MNCPU *cpu, MNEmu *emu) {
    /* Emulated the 6502 as described at https://www.nesdev.org/6502_cpu.txt */
    unsigned char tmp;

    if(cpu->jammed) return;

    if(cpu->cycle == 1){
        cpu->t = emu->mapper.read(emu, &emu->mapper, cpu->pc);
        cpu->cycle = 2;
        cpu->target_cycle = 2;

        printf("c: %d, %c%c%c%c%c-%c%c %02x\n", cpu->cycle,
               cpu->p&MN_CPU_C ? 'C' : '-',
               cpu->p&MN_CPU_Z ? 'Z' : '-',
               cpu->p&MN_CPU_I ? 'I' : '-',
               cpu->p&MN_CPU_D ? 'D' : '-',
               cpu->p&MN_CPU_B ? 'B' : '-',
               cpu->p&MN_CPU_N ? 'N' : '-',
               cpu->p&MN_CPU_V ? 'V' : '-', cpu->opcode);

        return;
    }else if(cpu->cycle > cpu->target_cycle){
        cpu->opcode = emu->mapper.read(emu, &emu->mapper, cpu->pc);
        cpu->pc++;
        cpu->cycle = 1;

        printf("c: %d, %c%c%c%c%c-%c%c %02x\n", cpu->cycle,
               cpu->p&MN_CPU_C ? 'C' : '-',
               cpu->p&MN_CPU_Z ? 'Z' : '-',
               cpu->p&MN_CPU_I ? 'I' : '-',
               cpu->p&MN_CPU_D ? 'D' : '-',
               cpu->p&MN_CPU_B ? 'B' : '-',
               cpu->p&MN_CPU_N ? 'N' : '-',
               cpu->p&MN_CPU_V ? 'V' : '-', cpu->opcode);

        return;
    }

    /* XXX: Maybe it would be better to use a LUT. */
    /* TODO: Avoid having so much duplicated code. */
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
                    cpu->pc &= 0xFF;
                    cpu->pc |= emu->mapper.read(emu, &emu->mapper, 0xFFFF)<<8;
                    break;
            }
            break;

        case 0x40:
            /* RTI */
            switch(cpu->cycle){
                case 2:
                    cpu->target_cycle = 6;
                    break;
                case 3:
                    cpu->s++;
                    break;
                case 4:
                    cpu->p = emu->mapper.read(emu, &emu->mapper,
                                              0x0100+cpu->s);
                    cpu->s++;
                    break;
                case 5:
                    cpu->pc &= 0xFF00;
                    cpu->pc |= emu->mapper.read(emu, &emu->mapper,
                                                0x0100+cpu->s);
                    cpu->s++;
                    break;
                case 6:
                    cpu->pc &= 0xFF;
                    cpu->pc |= emu->mapper.read(emu, &emu->mapper,
                                                0x0100+cpu->s)<<8;
                    break;
            }
            break;

        case 0x60:
            /* RTS */
            switch(cpu->cycle){
                case 2:
                    cpu->target_cycle = 6;
                    break;
                case 3:
                    cpu->s++;
                    break;
                case 4:
                    cpu->pc &= 0xFF00;
                    cpu->pc |= emu->mapper.read(emu, &emu->mapper,
                                                0x0100+cpu->s);
                    cpu->s++;
                    break;
                case 5:
                    cpu->pc &= 0xFF;
                    cpu->pc |= emu->mapper.read(emu, &emu->mapper,
                                                0x0100+cpu->s)<<8;
                    break;
                case 6:
                    cpu->pc++;
            }
            break;

        case 0x48:
            /* PHA */
            switch(cpu->cycle){
                case 2:
                    cpu->target_cycle = 3;
                    break;
                case 3:
                    emu->mapper.write(emu, &emu->mapper, 0x0100+cpu->s,
                                      cpu->a);
                    cpu->s--;
                    break;
            }
            break;

        case 0x08:
            /* PHP */
            switch(cpu->cycle){
                case 2:
                    cpu->target_cycle = 3;
                    break;
                case 3:
                    emu->mapper.write(emu, &emu->mapper, 0x0100+cpu->s,
                                      cpu->p);
                    cpu->s--;
                    break;
            }
            break;

        case 0x68:
            /* PLA */
            switch(cpu->cycle){
                case 2:
                    cpu->target_cycle = 4;
                    break;
                case 3:
                    cpu->s++;
                    break;
                case 4:
                    cpu->a = emu->mapper.read(emu, &emu->mapper,
                                              0x0100+cpu->s);
                    break;
            }
            break;

        case 0x28:
            /* PLP */
            switch(cpu->cycle){
                case 2:
                    cpu->target_cycle = 4;
                    break;
                case 3:
                    cpu->s++;
                    break;
                case 4:
                    cpu->p = emu->mapper.read(emu, &emu->mapper,
                                              0x0100+cpu->s);
                    break;
            }
            break;

        case 0x20:
            /* JSR */
            switch(cpu->cycle){
                case 2:
                    cpu->target_cycle = 6;
                    cpu->pc++;
                    break;
                case 4:
                    emu->mapper.write(emu, &emu->mapper, 0x0100+cpu->s,
                                      cpu->pc>>8);
                    cpu->s--;
                    break;
                case 5:
                    emu->mapper.write(emu, &emu->mapper, 0x0100+cpu->s,
                                      cpu->pc);
                    cpu->s--;
                    break;
                case 6:
                    cpu->pc = cpu->t|(emu->mapper.read(emu, &emu->mapper,
                                                       cpu->pc)<<8);
                    break;
            }
            break;

        /* Official opcodes with implied or accumulator addressing */

        /* NOTE: The CPU cycle number is always equal or bigger to 2 when
         * reaching this switch. */

        case 0x0A:
            /* ASL */
            cpu->p &= ~MN_CPU_C;
            /* Set the carry flag to the old bit 7 */
            cpu->p |= (cpu->a>>7)&1;

            cpu->a <<= 1;

            if(!cpu->a) cpu->p |= MN_CPU_Z;
            else cpu->p &= ~MN_CPU_Z;
            if(cpu->a&(1<<7)) cpu->p |= MN_CPU_N;
            else cpu->p &= ~MN_CPU_N;
            break;

        case 0x18:
            /* CLC */
            cpu->p &= ~MN_CPU_C;
            break;

        case 0x2A:
            /* ROL */
            tmp = cpu->p&MN_CPU_C;
            cpu->p &= ~MN_CPU_C;
            /* Set the carry flag to the old bit 7 */
            cpu->p |= (cpu->a>>7)&1;

            cpu->a <<= 1;
            cpu->a |= tmp;

            if(!cpu->a) cpu->p |= MN_CPU_Z;
            else cpu->p &= ~MN_CPU_Z;
            if(cpu->a&(1<<7)) cpu->p |= MN_CPU_N;
            else cpu->p &= ~MN_CPU_N;
            break;

        case 0x38:
            /* SEC */
            cpu->p |= MN_CPU_C;
            break;

        case 0x4A:
            /* LSR */
            cpu->p &= ~MN_CPU_C;
            /* Set the carry flag to the old bit 0 */
            cpu->p |= cpu->a&1;

            cpu->a >>= 1;

            if(!cpu->a) cpu->p |= MN_CPU_Z;
            else cpu->p &= ~MN_CPU_Z;
            if(cpu->a&(1<<7)) cpu->p |= MN_CPU_N;
            else cpu->p &= ~MN_CPU_N;
            break;

        case 0x58:
            /* CLI */
            cpu->p &= ~MN_CPU_I;
            break;

        case 0x6A:
            /* ROR */
            tmp = (cpu->p&MN_CPU_C)<<7;
            cpu->p &= ~MN_CPU_C;
            /* Set the carry flag to the old bit 0 */
            cpu->p |= cpu->a&1;

            cpu->a >>= 1;
            cpu->a |= tmp;

            if(!cpu->a) cpu->p |= MN_CPU_Z;
            else cpu->p &= ~MN_CPU_Z;
            if(cpu->a&(1<<7)) cpu->p |= MN_CPU_N;
            else cpu->p &= ~MN_CPU_N;
            break;

        case 0x78:
            /* SEI */
            cpu->p |= MN_CPU_I;
            break;

        case 0x88:
            /* DEY */
            cpu->y--;
            if(!cpu->y) cpu->p |= MN_CPU_Z;
            else cpu->p &= ~MN_CPU_Z;
            if(cpu->y&(1<<7)) cpu->p |= MN_CPU_N;
            else cpu->p &= ~MN_CPU_N;
            break;

        case 0x8A:
            /* TXA */
            cpu->a = cpu->x;
            if(!cpu->a) cpu->p |= MN_CPU_Z;
            else cpu->p &= ~MN_CPU_Z;
            if(cpu->a&(1<<7)) cpu->p |= MN_CPU_N;
            else cpu->p &= ~MN_CPU_N;
            break;

        case 0x98:
            /* TYA */
            cpu->a = cpu->y;
            if(!cpu->a) cpu->p |= MN_CPU_Z;
            else cpu->p &= ~MN_CPU_Z;
            if(cpu->a&(1<<7)) cpu->p |= MN_CPU_N;
            else cpu->p &= ~MN_CPU_N;
            break;

        case 0x9A:
            /* TXS */
            cpu->s = cpu->x;
            break;

        case 0xA8:
            /* TAY */
            cpu->y = cpu->a;
            if(!cpu->y) cpu->p |= MN_CPU_Z;
            else cpu->p &= ~MN_CPU_Z;
            if(cpu->y&(1<<7)) cpu->p |= MN_CPU_N;
            else cpu->p &= ~MN_CPU_N;
            break;

        case 0xAA:
            /* TAX */
            cpu->x = cpu->a;
            if(!cpu->x) cpu->p |= MN_CPU_Z;
            else cpu->p &= ~MN_CPU_Z;
            if(cpu->x&(1<<7)) cpu->p |= MN_CPU_N;
            else cpu->p &= ~MN_CPU_N;
            break;

        case 0xB8:
            /* CLV */
            cpu->p &= ~MN_CPU_V;
            break;

        case 0xBA:
            /* TSX */
            cpu->x = cpu->s;
            if(!cpu->x) cpu->p |= MN_CPU_Z;
            else cpu->p &= ~MN_CPU_Z;
            if(cpu->x&(1<<7)) cpu->p |= MN_CPU_N;
            else cpu->p &= ~MN_CPU_N;
            break;

        case 0xC8:
            /* INY */
            cpu->y++;
            if(!cpu->y) cpu->p |= MN_CPU_Z;
            else cpu->p &= ~MN_CPU_Z;
            if(cpu->y&(1<<7)) cpu->p |= MN_CPU_N;
            else cpu->p &= ~MN_CPU_N;
            break;

        case 0xCA:
            /* DEX */
            cpu->x++;
            if(!cpu->x) cpu->p |= MN_CPU_Z;
            else cpu->p &= ~MN_CPU_Z;
            if(cpu->x&(1<<7)) cpu->p |= MN_CPU_N;
            else cpu->p &= ~MN_CPU_N;
            break;

        case 0xD8:
            /* CLD */
            cpu->p &= ~MN_CPU_D;
            break;

        case 0xE8:
            /* INX */
            cpu->x++;
            if(!cpu->x) cpu->p |= MN_CPU_Z;
            else cpu->p &= ~MN_CPU_Z;
            if(cpu->x&(1<<7)) cpu->p |= MN_CPU_N;
            else cpu->p &= ~MN_CPU_N;
            break;

        case 0xEA:
            /* NOP */
            break;

        case 0xF8:
            /* SED */
            cpu->p |= MN_CPU_D;
            break;

        /* Unofficial opcodes */

        case 0x1A:
        case 0x3A:
        case 0x5A:
        case 0x7A:
        case 0xDA:
        case 0xFA:
            /* NOP */
            break;

        case 0x02:
        case 0x12:
        case 0x22:
        case 0x32:
        case 0x42:
        case 0x52:
        case 0x62:
        case 0x72:
        case 0x92:
        case 0xB2:
        case 0xD2:
        case 0xF2:
            /* STP */
            /* TODO: Check if I'm emulating it accurately */
            cpu->jammed = 1;
            break;
        default:
            /* Unknown opcode, jam the CPU for now */
            cpu->jammed = 1;
    }

    printf("c: %d, %c%c%c%c%c-%c%c %02x\n", cpu->cycle,
           cpu->p&MN_CPU_C ? 'C' : '-',
           cpu->p&MN_CPU_Z ? 'Z' : '-',
           cpu->p&MN_CPU_I ? 'I' : '-',
           cpu->p&MN_CPU_D ? 'D' : '-',
           cpu->p&MN_CPU_B ? 'B' : '-',
           cpu->p&MN_CPU_N ? 'N' : '-',
           cpu->p&MN_CPU_V ? 'V' : '-', cpu->opcode);

    cpu->cycle++;
}

void mn_cpu_free(MNCPU *cpu) {
    /* TODO */
    (void)cpu;
}
