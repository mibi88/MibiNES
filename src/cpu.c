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

#define MN_CPU_UPDATE_NZ(reg) \
    { \
        if(!reg) cpu->p |= MN_CPU_Z; \
        else cpu->p &= ~MN_CPU_Z; \
        if(reg&(1<<7)) cpu->p |= MN_CPU_N; \
        else cpu->p &= ~MN_CPU_N; \
    }

#define MN_CPU_CMP(reg, value) \
    { \
        if(reg >= value) cpu->p |= MN_CPU_C; \
        else cpu->p &= ~MN_CPU_C; \
        if(reg == value) cpu->p |= MN_CPU_Z; \
        else cpu->p &= ~MN_CPU_Z; \
        if((reg-value)&(1<<7)) cpu->p |= MN_CPU_N; \
        else cpu->p &= ~MN_CPU_N; \
    }

#define MN_CPU_ADC(value) \
    { \
        tmp = cpu->a; \
        cpu->a = (result = cpu->a+(value)+(cpu->p&MN_CPU_C)); \
 \
        if(result&(~0xFF)) cpu->p |= MN_CPU_C; \
        else cpu->p &= ~MN_CPU_C; \
 \
        if((result^tmp)&(result^(value))&(1<<7)){ \
            cpu->p |= MN_CPU_V; \
        }else{ \
            cpu->p &= ~MN_CPU_V; \
        } \
 \
        MN_CPU_UPDATE_NZ(cpu->a); \
    }

#define MN_CPU_ASL(var) \
    { \
        cpu->p &= ~MN_CPU_C; \
        /* Set the carry flag to the old bit 7 */ \
        cpu->p |= (var>>7)&1; \
 \
        var <<= 1; \
 \
        MN_CPU_UPDATE_NZ(var); \
    }

#define MN_CPU_ROL(var) \
    { \
        tmp = cpu->p&MN_CPU_C; \
        cpu->p &= ~MN_CPU_C; \
        /* Set the carry flag to the old bit 7 */ \
        cpu->p |= (cpu->a>>7)&1; \
 \
        var <<= 1; \
        var |= tmp; \
 \
        MN_CPU_UPDATE_NZ(var); \
    }

#define MN_CPU_LSR(var) \
    { \
        cpu->p &= ~MN_CPU_C; \
        /* Set the carry flag to the old bit 0 */ \
        cpu->p |= cpu->a&1; \
 \
        var >>= 1; \
 \
        MN_CPU_UPDATE_NZ(var); \
    }

#define MN_CPU_ROR(var) \
    { \
        tmp = (cpu->p&MN_CPU_C)<<7; \
        cpu->p &= ~MN_CPU_C; \
        /* Set the carry flag to the old bit 0 */ \
        cpu->p |= cpu->a&1; \
 \
        var >>= 1; \
        var |= tmp; \
 \
        MN_CPU_UPDATE_NZ(var); \
    }

#define MN_CPU_BIT(value) \
    { \
        if(!(value&cpu->a)) cpu->p |= MN_CPU_Z; \
        else cpu->p &= ~MN_CPU_Z; \
 \
        /* Set V to the bit 6 of the memory value and N to the bit
         * 7 of the memory value. */ \
        cpu->p &= (1<<6)-1; \
        cpu->p |= value&((1<<6)|(1<<7)); \
    }

#define MN_CPU_ABS_RMW(op) \
    { \
        switch(cpu->cycle){ \
            case 2: \
                cpu->target_cycle = 6; \
                cpu->pc++; \
                break; \
            case 3: \
                tmp = emu->mapper.read(emu, &emu->mapper, cpu->pc); \
                cpu->tmp = cpu->t|(tmp<<8); \
                cpu->pc++; \
                break; \
            case 4: \
                cpu->t = emu->mapper.read(emu, &emu->mapper, cpu->tmp); \
                break; \
            case 5: \
                emu->mapper.write(emu, &emu->mapper, cpu->tmp, cpu->t); \
 \
                /* Perform the operation on it */ \
                op; \
                break; \
            case 6: \
                emu->mapper.write(emu, &emu->mapper, cpu->tmp, cpu->t); \
                break; \
        } \
    }

#define MN_CPU_ABS_STORE(op) \
    { \
        switch(cpu->cycle){ \
            case 2: \
                cpu->target_cycle = 4; \
                cpu->pc++; \
                break; \
            case 3: \
                tmp = emu->mapper.read(emu, &emu->mapper, cpu->pc); \
                cpu->tmp = cpu->t|(tmp<<8); \
                cpu->pc++; \
                break; \
            case 4: \
                op; \
                emu->mapper.write(emu, &emu->mapper, cpu->tmp, tmp); \
                break; \
        } \
    }

#define MN_CPU_ZP_READ(op) \
    { \
        switch(cpu->cycle){ \
            case 2: \
                cpu->target_cycle = 3; \
                cpu->pc++; \
                break; \
            case 3: \
                tmp = emu->mapper.read(emu, &emu->mapper, cpu->t); \
                op; \
                break; \
        } \
    }

#define MN_CPU_ZP_RMW(op) \
    { \
        switch(cpu->cycle){ \
            case 2: \
                cpu->target_cycle = 5; \
                cpu->pc++; \
                cpu->tmp = cpu->t; \
                break; \
            case 3: \
                cpu->t = emu->mapper.read(emu, &emu->mapper, cpu->tmp); \
                break; \
            case 4: \
                emu->mapper.write(emu, &emu->mapper, cpu->tmp, cpu->t); \
                op; \
                break; \
            case 5: \
                emu->mapper.write(emu, &emu->mapper, cpu->tmp, cpu->t); \
                break; \
        } \
    }

void mn_cpu_cycle(MNCPU *cpu, MNEmu *emu) {
    /* Emulated the 6502 as described at https://www.nesdev.org/6502_cpu.txt
     *
     * I also used:
     * https://www.nesdev.org/wiki/CPU_unofficial_opcodes
     * http://www.6502.org/users/obelisk/6502/reference.html
     * https://www.oxyron.de/html/opcodes02.html
     * https://www.nesdev.org/wiki/Instruction_reference#ADC
     */
    unsigned char tmp;
    unsigned short int result;

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

            MN_CPU_ASL(cpu->a);
            break;

        case 0x18:
            /* CLC */
            cpu->p &= ~MN_CPU_C;
            break;

        case 0x2A:
            /* ROL */

            MN_CPU_ROL(cpu->a);
            break;

        case 0x38:
            /* SEC */
            cpu->p |= MN_CPU_C;
            break;

        case 0x4A:
            /* LSR */
            MN_CPU_LSR(cpu->a);
            break;

        case 0x58:
            /* CLI */
            cpu->p &= ~MN_CPU_I;
            break;

        case 0x6A:
            /* ROR */
            MN_CPU_ROR(cpu->a);
            break;

        case 0x78:
            /* SEI */
            cpu->p |= MN_CPU_I;
            break;

        case 0x88:
            /* DEY */
            cpu->y--;

            MN_CPU_UPDATE_NZ(cpu->y);
            break;

        case 0x8A:
            /* TXA */
            cpu->a = cpu->x;

            MN_CPU_UPDATE_NZ(cpu->a);
            break;

        case 0x98:
            /* TYA */
            cpu->a = cpu->y;

            MN_CPU_UPDATE_NZ(cpu->a);
            break;

        case 0x9A:
            /* TXS */
            cpu->s = cpu->x;
            break;

        case 0xA8:
            /* TAY */
            cpu->y = cpu->a;

            MN_CPU_UPDATE_NZ(cpu->y);
            break;

        case 0xAA:
            /* TAX */
            cpu->x = cpu->a;

            MN_CPU_UPDATE_NZ(cpu->x);
            break;

        case 0xB8:
            /* CLV */
            cpu->p &= ~MN_CPU_V;
            break;

        case 0xBA:
            /* TSX */
            cpu->x = cpu->s;

            MN_CPU_UPDATE_NZ(cpu->x);
            break;

        case 0xC8:
            /* INY */
            cpu->y++;

            MN_CPU_UPDATE_NZ(cpu->y);
            break;

        case 0xCA:
            /* DEX */
            cpu->x++;

            MN_CPU_UPDATE_NZ(cpu->x);
            break;

        case 0xD8:
            /* CLD */
            cpu->p &= ~MN_CPU_D;
            break;

        case 0xE8:
            /* INX */
            cpu->x++;

            MN_CPU_UPDATE_NZ(cpu->x);
            break;

        case 0xEA:
            /* NOP */
            break;

        case 0xF8:
            /* SED */
            cpu->p |= MN_CPU_D;
            break;

        /* Opcodes with immediate addressing */

        case 0x09:
            /* ORA */
            cpu->a |= cpu->t;

            MN_CPU_UPDATE_NZ(cpu->a);

            cpu->pc++;
            break;

        case 0x29:
            /* AND */
            cpu->a &= cpu->t;

            MN_CPU_UPDATE_NZ(cpu->a);

            cpu->pc++;
            break;

        case 0x49:
            /* EOR */
            cpu->a ^= cpu->t;

            MN_CPU_UPDATE_NZ(cpu->a);

            cpu->pc++;
            break;

        case 0x69:
            /* ADC */

            MN_CPU_ADC(cpu->t);

            cpu->pc++;
            break;

        case 0xA0:
            /* LDY */
            cpu->y = cpu->t;

            MN_CPU_UPDATE_NZ(cpu->y);

            cpu->pc++;
            break;

        case 0xA2:
            /* LDX */
            cpu->x = cpu->t;

            MN_CPU_UPDATE_NZ(cpu->x);

            cpu->pc++;
            break;

        case 0xA9:
            /* LDA */
            cpu->a = cpu->t;

            MN_CPU_UPDATE_NZ(cpu->a);

            cpu->pc++;
            break;

        case 0xC0:
            /* CPY */
            MN_CPU_CMP(cpu->y, cpu->t);

            cpu->pc++;
            break;

        case 0xC9:
            /* CMP */
            MN_CPU_CMP(cpu->a, cpu->t);

            cpu->pc++;
            break;

        case 0xE0:
            /* CPX */
            MN_CPU_CMP(cpu->x, cpu->t);

            cpu->pc++;
            break;

        case 0xEB: /* Unofficial opcode */
            /* NOTE: According to No More Secrets, it is the same [as $E9],
             *  said Fiskbit on the NesDev Discord. */
        case 0xE9:
            /* SBC */

            MN_CPU_ADC(~cpu->t);
            break;

        /* Absolute addressing */

        case 0x4C:
            /* JMP */
            switch(cpu->cycle){
                case 2:
                    cpu->target_cycle = 3;
                    cpu->pc++;
                    break;
                case 3:
                    tmp = emu->mapper.read(emu, &emu->mapper, cpu->pc);
                    cpu->pc = cpu->t|(tmp<<8);
                    break;
            }
            break;

        /* Absolute addressing - read instructions */

        case 0x0D:
            /* ORA */
            switch(cpu->cycle){
                case 2:
                    cpu->target_cycle = 4;
                    cpu->pc++;
                    break;
                case 3:
                    tmp = emu->mapper.read(emu, &emu->mapper, cpu->pc);
                    cpu->tmp = cpu->t|(tmp<<8);
                    cpu->pc++;
                    break;
                case 4:
                    cpu->a |= emu->mapper.read(emu, &emu->mapper, cpu->tmp);

                    MN_CPU_UPDATE_NZ(cpu->a);
                    break;
            }
            break;

        case 0x2C:
            /* BIT */
            switch(cpu->cycle){
                case 2:
                    cpu->target_cycle = 4;
                    cpu->pc++;
                    break;
                case 3:
                    tmp = emu->mapper.read(emu, &emu->mapper, cpu->pc);
                    cpu->tmp = cpu->t|(tmp<<8);
                    cpu->pc++;
                    break;
                case 4:
                    tmp = emu->mapper.read(emu, &emu->mapper, cpu->tmp);

                    MN_CPU_BIT(tmp);

                    break;
            }
            break;

        case 0x2D:
            /* AND */
            switch(cpu->cycle){
                case 2:
                    cpu->target_cycle = 4;
                    cpu->pc++;
                    break;
                case 3:
                    tmp = emu->mapper.read(emu, &emu->mapper, cpu->pc);
                    cpu->tmp = cpu->t|(tmp<<8);
                    cpu->pc++;
                    break;
                case 4:
                    cpu->a &= emu->mapper.read(emu, &emu->mapper, cpu->tmp);

                    MN_CPU_UPDATE_NZ(cpu->a);
                    break;
            }
            break;

        case 0x4D:
            /* EOR */
            switch(cpu->cycle){
                case 2:
                    cpu->target_cycle = 4;
                    cpu->pc++;
                    break;
                case 3:
                    tmp = emu->mapper.read(emu, &emu->mapper, cpu->pc);
                    cpu->tmp = cpu->t|(tmp<<8);
                    cpu->pc++;
                    break;
                case 4:
                    cpu->a ^= emu->mapper.read(emu, &emu->mapper, cpu->tmp);

                    MN_CPU_UPDATE_NZ(cpu->a);
                    break;
            }
            break;

        case 0x6D:
            /* ADC */
            switch(cpu->cycle){
                case 2:
                    cpu->target_cycle = 4;
                    cpu->pc++;
                    break;
                case 3:
                    tmp = emu->mapper.read(emu, &emu->mapper, cpu->pc);
                    cpu->tmp = cpu->t|(tmp<<8);
                    cpu->pc++;
                    break;
                case 4:
                    tmp = emu->mapper.read(emu, &emu->mapper, cpu->tmp);

                    MN_CPU_ADC(tmp);
                    break;
            }
            break;

        case 0xAC:
            /* LDY */
            switch(cpu->cycle){
                case 2:
                    cpu->target_cycle = 4;
                    cpu->pc++;
                    break;
                case 3:
                    tmp = emu->mapper.read(emu, &emu->mapper, cpu->pc);
                    cpu->tmp = cpu->t|(tmp<<8);
                    cpu->pc++;
                    break;
                case 4:
                    cpu->y = emu->mapper.read(emu, &emu->mapper, cpu->tmp);

                    MN_CPU_UPDATE_NZ(cpu->y);
                    break;
            }
            break;

        case 0xAD:
            /* LDA */
            switch(cpu->cycle){
                case 2:
                    cpu->target_cycle = 4;
                    cpu->pc++;
                    break;
                case 3:
                    tmp = emu->mapper.read(emu, &emu->mapper, cpu->pc);
                    cpu->tmp = cpu->t|(tmp<<8);
                    cpu->pc++;
                    break;
                case 4:
                    cpu->a = emu->mapper.read(emu, &emu->mapper, cpu->tmp);

                    MN_CPU_UPDATE_NZ(cpu->a);
                    break;
            }
            break;

        case 0xAE:
            /* LDX */
            switch(cpu->cycle){
                case 2:
                    cpu->target_cycle = 4;
                    cpu->pc++;
                    break;
                case 3:
                    tmp = emu->mapper.read(emu, &emu->mapper, cpu->pc);
                    cpu->tmp = cpu->t|(tmp<<8);
                    cpu->pc++;
                    break;
                case 4:
                    cpu->x = emu->mapper.read(emu, &emu->mapper, cpu->tmp);

                    MN_CPU_UPDATE_NZ(cpu->x);
                    break;
            }
            break;

        case 0xCC:
            /* CPY */
            switch(cpu->cycle){
                case 2:
                    cpu->target_cycle = 4;
                    cpu->pc++;
                    break;
                case 3:
                    tmp = emu->mapper.read(emu, &emu->mapper, cpu->pc);
                    cpu->tmp = cpu->t|(tmp<<8);
                    cpu->pc++;
                    break;
                case 4:
                    tmp = emu->mapper.read(emu, &emu->mapper, cpu->tmp);

                    MN_CPU_CMP(cpu->y, tmp);
                    break;
            }
            break;

        case 0xCD:
            /* CMP */
            switch(cpu->cycle){
                case 2:
                    cpu->target_cycle = 4;
                    cpu->pc++;
                    break;
                case 3:
                    tmp = emu->mapper.read(emu, &emu->mapper, cpu->pc);
                    cpu->tmp = cpu->t|(tmp<<8);
                    cpu->pc++;
                    break;
                case 4:
                    tmp = emu->mapper.read(emu, &emu->mapper, cpu->tmp);

                    MN_CPU_CMP(cpu->a, tmp);
                    break;
            }
            break;

        case 0xEC:
            /* CPX */
            switch(cpu->cycle){
                case 2:
                    cpu->target_cycle = 4;
                    cpu->pc++;
                    break;
                case 3:
                    tmp = emu->mapper.read(emu, &emu->mapper, cpu->pc);
                    cpu->tmp = cpu->t|(tmp<<8);
                    cpu->pc++;
                    break;
                case 4:
                    tmp = emu->mapper.read(emu, &emu->mapper, cpu->tmp);

                    MN_CPU_CMP(cpu->x, tmp);
                    break;
            }
            break;

        case 0xED:
            /* SBC */
            switch(cpu->cycle){
                case 2:
                    cpu->target_cycle = 4;
                    cpu->pc++;
                    break;
                case 3:
                    tmp = emu->mapper.read(emu, &emu->mapper, cpu->pc);
                    cpu->tmp = cpu->t|(tmp<<8);
                    cpu->pc++;
                    break;
                case 4:
                    tmp = emu->mapper.read(emu, &emu->mapper, cpu->tmp);

                    MN_CPU_ADC(~tmp);
                    break;
            }
            break;

        /* Absolute addressing - read-modify-write (RMW) instructions */

        case 0x0E:
            /* ASL */
            MN_CPU_ABS_RMW({
                MN_CPU_ASL(cpu->t);
            });
            break;

        case 0x2E:
            /* ROL */
            MN_CPU_ABS_RMW({
                MN_CPU_ROL(cpu->t);
            });
            break;

        case 0x4E:
            /* LSR */
            MN_CPU_ABS_RMW({
                MN_CPU_LSR(cpu->t);
            });
            break;

        case 0x6E:
            /* ROR */
            MN_CPU_ABS_RMW({
                MN_CPU_ROR(cpu->t);
            });
            break;

        case 0xCE:
            /* DEC */
            MN_CPU_ABS_RMW({
                cpu->t--;

                MN_CPU_UPDATE_NZ(cpu->t);
            });
            break;

        case 0xEE:
            /* INC */
            MN_CPU_ABS_RMW({
                cpu->t++;

                MN_CPU_UPDATE_NZ(cpu->t);
            });
            break;

        /* Absolute addressing - write instructions */

        case 0x8C:
            /* STY */
            MN_CPU_ABS_STORE({
                tmp = cpu->y;
            });
            break;

        case 0x8D:
            /* STA */
            MN_CPU_ABS_STORE({
                tmp = cpu->a;
            });
            break;

        case 0x8E:
            /* STX */
            MN_CPU_ABS_STORE({
                tmp = cpu->x;
            });
            break;

        /* Zeropage addressing */

        /* Zeropage addressing - read instructions */

        case 0x05:
            /* ORA */
            MN_CPU_ZP_READ({
                cpu->a |= tmp;

                MN_CPU_UPDATE_NZ(cpu->a);
            });
            break;

        case 0x24:
            /* BIT */
            MN_CPU_ZP_READ({
                MN_CPU_BIT(tmp);
            });
            break;

        case 0x25:
            /* AND */
            MN_CPU_ZP_READ({
                cpu->a &= tmp;

                MN_CPU_UPDATE_NZ(cpu->a);
            });
            break;

        case 0x45:
            /* EOR */
            MN_CPU_ZP_READ({
                cpu->a ^= tmp;

                MN_CPU_UPDATE_NZ(cpu->a);
            });
            break;

        case 0x65:
            /* ADC */
            MN_CPU_ZP_READ({
                MN_CPU_ADC(tmp);
            });
            break;

        case 0xA4:
            /* LDY */
            MN_CPU_ZP_READ({
                cpu->y = tmp;

                MN_CPU_UPDATE_NZ(cpu->y);
            });
            break;

        case 0xA5:
            /* LDA */
            MN_CPU_ZP_READ({
                cpu->a = tmp;

                MN_CPU_UPDATE_NZ(cpu->a);
            });
            break;

        case 0xA6:
            /* LDX */
            MN_CPU_ZP_READ({
                cpu->x = tmp;

                MN_CPU_UPDATE_NZ(cpu->x);
            });
            break;

        case 0xC4:
            /* CPY */
            MN_CPU_ZP_READ({
                MN_CPU_CMP(cpu->y, tmp);
            });
            break;

        case 0xC5:
            /* CMP */
            MN_CPU_ZP_READ({
                MN_CPU_CMP(cpu->a, tmp);
            });
            break;

        case 0xE4:
            /* CPX */
            MN_CPU_ZP_READ({
                MN_CPU_CMP(cpu->x, tmp);
            });
            break;

        case 0xE5:
            /* SBC */
            MN_CPU_ZP_READ({
                MN_CPU_ADC(~tmp);
            });
            break;

        /* Zeropage addressing - RMW instructions */

        case 0x06:
            /* ASL */
            MN_CPU_ZP_RMW({
                MN_CPU_ASL(cpu->t);
            });
            break;

        case 0x26:
            /* ROL */
            MN_CPU_ZP_RMW({
                MN_CPU_ROL(cpu->t);
            });
            break;

        case 0x46:
            /* LSR */
            MN_CPU_ZP_RMW({
                MN_CPU_LSR(cpu->t);
            });
            break;

        case 0x66:
            /* ROR */
            MN_CPU_ZP_RMW({
                MN_CPU_ROR(cpu->t);
            });
            break;

        case 0xC6:
            /* DEC */
            MN_CPU_ZP_RMW({
                cpu->t--;

                MN_CPU_UPDATE_NZ(cpu->t);
            });
            break;

        case 0xE6:
            /* INC */
            MN_CPU_ZP_RMW({
                cpu->t++;

                MN_CPU_UPDATE_NZ(cpu->t);
            });
            break;

        /* Zeropage addressing - write instructions */

        /* Unofficial opcodes */

        /* Implied addressing */

        case 0x1A:
        case 0x3A:
        case 0x5A:
        case 0x7A:
        case 0xDA:
        case 0xFA:
            /* NOP */
            break;

        /* Immediate addressing */

        case 0x80:
        case 0x82:
        case 0x89:
        case 0xC2:
            /* NOP */
            cpu->pc++;
            break;

        case 0xAB:
            /* LAX */
            cpu->a = cpu->t;
            cpu->x = cpu->a;

            MN_CPU_UPDATE_NZ(cpu->a);

            cpu->pc++;
            break;

        /* Absolute addressing */

        case 0x0C:
            /* NOP */
            switch(cpu->cycle){
                case 2:
                    cpu->target_cycle = 4;
                    cpu->pc++;
                    break;
                case 3:
                    tmp = emu->mapper.read(emu, &emu->mapper, cpu->pc);
                    cpu->tmp = cpu->t|(tmp<<8);
                    cpu->pc++;
                    break;
                case 4:
                    emu->mapper.read(emu, &emu->mapper, cpu->tmp);
                    break;
            }
            break;

        case 0xAF:
            /* LAX */
            switch(cpu->cycle){
                case 2:
                    cpu->target_cycle = 4;
                    cpu->pc++;
                    break;
                case 3:
                    tmp = emu->mapper.read(emu, &emu->mapper, cpu->pc);
                    cpu->tmp = cpu->t|(tmp<<8);
                    cpu->pc++;
                    break;
                case 4:
                    /* XXX: Is LAX performing only one or two reads? */
                    cpu->a = emu->mapper.read(emu, &emu->mapper, cpu->tmp);
                    cpu->x = cpu->a;

                    MN_CPU_UPDATE_NZ(cpu->a);
                    break;
            }
            break;

        case 0x8F:
            /* SAX */
            /* NOTE: Apparently it is unstable on the NES */
            MN_CPU_ABS_STORE({
                tmp = cpu->a&cpu->x;
            });
            break;

        /* Zeropage addressing */
        case 0xA7:
            /* LAX */
            MN_CPU_ZP_READ({
                cpu->a = tmp;
                cpu->x = tmp;

                MN_CPU_UPDATE_NZ(cpu->a);
            });
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
