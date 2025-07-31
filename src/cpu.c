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
    cpu->pc = 0;
    cpu->jammed = 0;

    cpu->cycle = 8;
    cpu->target_cycle = 0;

    cpu->irq_pin = 0;
    cpu->nmi_pin = 0;
    cpu->nmi_pin_last = 0;

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

#define MN_CPU_IMP(op) \
    { \
        switch(cpu->cycle){ \
            case 1: \
                cpu->target_cycle = 2; \
                break; \
            case 2: \
                op; \
                break; \
        } \
    }

#define MN_CPU_IMM(op) \
    { \
        switch(cpu->cycle){ \
            case 1: \
                cpu->target_cycle = 2; \
                break; \
            case 2: \
                op; \
                cpu->pc++; \
                break; \
        } \
    }

#define MN_CPU_ABS_READ(op) \
    { \
        switch(cpu->cycle){ \
            case 1: \
                cpu->target_cycle = 4; \
                break; \
            case 2: \
                cpu->pc++; \
                break; \
            case 3: \
                tmp = emu->mapper.read(emu, &emu->mapper, cpu->pc); \
                cpu->tmp = cpu->t|(tmp<<8); \
                cpu->pc++; \
                break; \
            case 4: \
                tmp = emu->mapper.read(emu, &emu->mapper, cpu->tmp); \
 \
                op; \
 \
                break; \
        } \
    }

#define MN_CPU_ABS_RMW(op) \
    { \
        switch(cpu->cycle){ \
            case 1: \
                cpu->target_cycle = 6; \
                break; \
            case 2: \
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
            case 1: \
                cpu->target_cycle = 4; \
                break; \
            case 2: \
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
            case 1: \
                cpu->target_cycle = 3; \
                break; \
            case 2: \
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
            case 1: \
                cpu->target_cycle = 5; \
                break; \
            case 2: \
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

#define MN_CPU_ZP_STORE(op) \
    { \
        switch(cpu->cycle){ \
            case 1: \
                cpu->target_cycle = 3; \
                break; \
            case 2: \
                cpu->pc++; \
                break; \
            case 3: \
                op; \
                emu->mapper.write(emu, &emu->mapper, cpu->t, tmp); \
                break; \
        } \
    }

#define MN_CPU_ZPI_READ(i, op) \
    { \
        switch(cpu->cycle){ \
            case 1: \
                cpu->target_cycle = 4; \
                break; \
            case 2: \
                cpu->pc++; \
                break; \
            case 3: \
                emu->mapper.read(emu, &emu->mapper, cpu->t); \
                cpu->t += i; \
                break; \
            case 4: \
                tmp = emu->mapper.read(emu, &emu->mapper, cpu->t); \
                op; \
                break; \
        } \
    }

#define MN_CPU_ZPI_RMW(op) \
    { \
        switch(cpu->cycle){ \
            case 1: \
                cpu->target_cycle = 6; \
                break; \
            case 2: \
                cpu->pc++; \
                break; \
            case 3: \
                emu->mapper.read(emu, &emu->mapper, cpu->t); \
                cpu->t += cpu->x; \
                break; \
            case 4: \
                cpu->tmp = cpu->t; \
                cpu->t = emu->mapper.read(emu, &emu->mapper, cpu->tmp); \
                break; \
            case 5: \
                emu->mapper.write(emu, &emu->mapper, cpu->tmp, cpu->t); \
                op; \
                break; \
            case 6: \
                emu->mapper.write(emu, &emu->mapper, cpu->tmp, cpu->t); \
                break; \
        } \
    }

#define MN_CPU_ZPI_STORE(i, op) \
    { \
        switch(cpu->cycle){ \
            case 1: \
                cpu->target_cycle = 4; \
                break; \
            case 2: \
                cpu->pc++; \
                break; \
            case 3: \
                emu->mapper.read(emu, &emu->mapper, cpu->t); \
                cpu->t += i; \
                break; \
            case 4: \
                op; \
                emu->mapper.write(emu, &emu->mapper, cpu->tmp, tmp); \
                break; \
        } \
    }

#define MN_CPU_ABSI_READ(i, op) \
    { \
        switch(cpu->cycle){ \
            case 1: \
                cpu->target_cycle = 4; \
                break; \
            case 2: \
                cpu->pc++; \
                break; \
            case 3: \
                cpu->tmp = cpu->t; \
                cpu->t = emu->mapper.read(emu, &emu->mapper, cpu->pc); \
                cpu->tmp += i; \
                tmp = cpu->tmp>>8; \
                cpu->tmp = (cpu->tmp&0xFF)|(cpu->t<<8); \
                cpu->t = tmp; \
                cpu->pc++; \
                break; \
            case 4: \
                tmp = emu->mapper.read(emu, &emu->mapper, cpu->tmp); \
                if(cpu->t){ \
                    cpu->tmp += cpu->t<<8; \
                    cpu->target_cycle++; \
                }else{ \
                    op; \
                } \
                break; \
            case 5: \
                /* Only executed if a page boundary had been crossed */ \
                tmp = emu->mapper.read(emu, &emu->mapper, cpu->tmp); \
                op; \
                break; \
        } \
    }

#define MN_CPU_ABSI_RMW(op) \
    { \
        switch(cpu->cycle){ \
            case 1: \
                cpu->target_cycle = 4; \
                break; \
            case 2: \
                cpu->pc++; \
                break; \
            case 3: \
                cpu->tmp = cpu->t; \
                cpu->t = emu->mapper.read(emu, &emu->mapper, cpu->pc); \
                cpu->tmp += cpu->x; \
                tmp = cpu->tmp>>8; \
                cpu->tmp = (cpu->tmp&0xFF)|(cpu->t<<8); \
                cpu->t = tmp; \
                cpu->pc++; \
                break; \
            case 4: \
                emu->mapper.read(emu, &emu->mapper, cpu->tmp); \
                cpu->tmp += cpu->t<<8; \
                break; \
            case 5: \
                cpu->t = emu->mapper.read(emu, &emu->mapper, cpu->tmp); \
                break; \
            case 6: \
                emu->mapper.write(emu, &emu->mapper, cpu->tmp, cpu->t); \
                op; \
                break; \
            case 7: \
                emu->mapper.write(emu, &emu->mapper, cpu->tmp, cpu->t); \
                break; \
        } \
    }

#define MN_CPU_ABSI_STORE(i, op) \
    { \
        switch(cpu->cycle){ \
            case 1: \
                cpu->target_cycle = 4; \
                break; \
            case 2: \
                cpu->pc++; \
                break; \
            case 3: \
                cpu->tmp = cpu->t; \
                cpu->t = emu->mapper.read(emu, &emu->mapper, cpu->pc); \
                cpu->tmp += i; \
                tmp = cpu->tmp>>8; \
                cpu->tmp = (cpu->tmp&0xFF)|(cpu->t<<8); \
                cpu->t = tmp; \
                cpu->pc++; \
                break; \
            case 4: \
                emu->mapper.read(emu, &emu->mapper, cpu->tmp); \
                cpu->tmp += cpu->t<<8; \
                break; \
            case 5: \
                op; \
                emu->mapper.write(emu, &emu->mapper, cpu->tmp, tmp); \
                break; \
        } \
    }

#define MN_CPU_RELATIVE(branch) \
    { \
        switch(cpu->cycle) { \
            case 1: \
                cpu->target_cycle = 3; \
                break; \
            case 2: \
                cpu->pc++; \
                break; \
            case 3: \
                tmp = emu->mapper.read(emu, &emu->mapper, cpu->pc); \
                if(branch){ \
                    if(cpu->t&(1<<7)){ \
                        cpu->tmp = cpu->pc-(256-cpu->t); \
                    }else{ \
                        cpu->tmp = cpu->pc+cpu->t+1; \
                    } \
                    cpu->pc = (cpu->tmp&0xFF)|(cpu->pc&0xFF00); \
                    cpu->target_cycle++; \
                }else{ \
                    cpu->opcode = tmp; \
                    cpu->cycle = 0; /* It will be incremented to 1 after the
                                     * switch */ \
                    cpu->pc++; \
                } \
                break; \
            case 4: \
                tmp = emu->mapper.read(emu, &emu->mapper, cpu->pc); \
                if(cpu->pc != cpu->tmp) { \
                    cpu->pc = cpu->tmp; \
                }else{ \
                    cpu->opcode = tmp; \
                    cpu->cycle = 0; /* It will be incremented to 1 after the
                                     * switch */ \
                    cpu->pc++; \
                } \
                break; \
        } \
    }

#define MN_CPU_IDXIND_READ(op) \
    { \
        switch(cpu->cycle){ \
            case 1: \
                cpu->target_cycle = 6; \
                break; \
            case 2: \
                cpu->pc++; \
                break; \
            case 3: \
                emu->mapper.read(emu, &emu->mapper, cpu->t); \
                cpu->t += cpu->x; \
                break; \
            case 4: \
                cpu->tmp = emu->mapper.read(emu, &emu->mapper, cpu->t); \
                break; \
            case 5: \
                cpu->tmp |= emu->mapper.read(emu, &emu->mapper, \
                                             (cpu->t+1)&0xFF)<<8; \
                break; \
            case 6: \
                tmp = emu->mapper.read(emu, &emu->mapper, cpu->tmp); \
                op; \
                break; \
        } \
    }

#define MN_CPU_IDXIND_RMW(op) \
    { \
        switch(cpu->cycle){ \
            case 1: \
                cpu->target_cycle = 6; \
                break; \
            case 2: \
                cpu->pc++; \
                break; \
            case 3: \
                emu->mapper.read(emu, &emu->mapper, cpu->t); \
                cpu->t += cpu->x; \
                break; \
            case 4: \
                cpu->tmp = emu->mapper.read(emu, &emu->mapper, cpu->t); \
                break; \
            case 5: \
                cpu->tmp |= emu->mapper.read(emu, &emu->mapper, \
                                             (cpu->t+1)&0xFF)<<8; \
                break; \
            case 6: \
                cpu->t = emu->mapper.read(emu, &emu->mapper, cpu->tmp); \
                break; \
            case 7: \
                emu->mapper.write(emu, &emu->mapper, cpu->tmp, cpu->t); \
                op; \
                break; \
            case 8: \
                emu->mapper.write(emu, &emu->mapper, cpu->tmp, cpu->t); \
                break; \
        } \
    }

#define MN_CPU_IDXIND_WRITE(op) \
    { \
        switch(cpu->cycle){ \
            case 1: \
                cpu->target_cycle = 6; \
                break; \
            case 2: \
                cpu->pc++; \
                break; \
            case 3: \
                emu->mapper.read(emu, &emu->mapper, cpu->t); \
                cpu->t += cpu->x; \
                break; \
            case 4: \
                cpu->tmp = emu->mapper.read(emu, &emu->mapper, cpu->t); \
                break; \
            case 5: \
                cpu->tmp |= emu->mapper.read(emu, &emu->mapper, \
                                             (cpu->t+1)&0xFF)<<8; \
                break; \
            case 6: \
                op; \
                emu->mapper.write(emu, &emu->mapper, cpu->tmp, cpu->t); \
                break; \
        } \
    }

#define MN_CPU_INDIDX_READ(op) \
    { \
        switch(cpu->cycle){ \
            case 1: \
                cpu->target_cycle = 5; \
                break; \
            case 2: \
                cpu->pc++; \
                break; \
            case 3: \
                cpu->tmp = emu->mapper.read(emu, &emu->mapper, cpu->t); \
                break; \
            case 4: \
                cpu->tmp |= emu->mapper.read(emu, &emu->mapper, \
                                             (cpu->t+1)&0xFF)<<8; \
                cpu->tmp2 = cpu->tmp+cpu->y; \
                cpu->tmp = (cpu->tmp2&0xFF)|(cpu->tmp&0xFF00); \
                break; \
            case 5: \
                tmp = emu->mapper.read(emu, &emu->mapper, cpu->tmp); \
                if(cpu->tmp != cpu->tmp2){ \
                    cpu->tmp = cpu->tmp2; \
                    cpu->target_cycle++; \
                }else{ \
                    op; \
                } \
                break; \
            case 6: \
                /* This cycle is only executed if the effective address was
                 * fixed. */ \
                tmp = emu->mapper.read(emu, &emu->mapper, cpu->tmp); \
                op; \
                break; \
        } \
    }

#define MN_CPU_INDIDX_WRITE(op) \
    { \
        switch(cpu->cycle){ \
            case 1: \
                cpu->target_cycle = 6; \
                break; \
            case 2: \
                cpu->pc++; \
                break; \
            case 3: \
                cpu->tmp = emu->mapper.read(emu, &emu->mapper, cpu->t); \
                break; \
            case 4: \
                cpu->tmp |= emu->mapper.read(emu, &emu->mapper, \
                                             (cpu->t+1)&0xFF)<<8; \
                cpu->tmp2 = cpu->tmp+cpu->y; \
                cpu->tmp = (cpu->tmp2&0xFF)|(cpu->tmp&0xFF00); \
                break; \
            case 5: \
                tmp = emu->mapper.read(emu, &emu->mapper, cpu->tmp); \
                cpu->tmp = cpu->tmp2; \
                break; \
            case 6: \
                /* This cycle is only executed if the effective address was
                 * fixed. */ \
                op; \
                emu->mapper.write(emu, &emu->mapper, cpu->tmp, tmp); \
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

    if(cpu->cycle == 2){
        cpu->t = emu->mapper.read(emu, &emu->mapper, cpu->pc);
    }else if(cpu->cycle > cpu->target_cycle){
        cpu->opcode = emu->mapper.read(emu, &emu->mapper, cpu->pc);
        cpu->pc++;
        cpu->cycle = 1;
        cpu->target_cycle = 2;
    }

    /* XXX: Maybe it would be better to use a LUT. */
    /* TODO: Avoid having so much duplicated code. */
    switch(cpu->opcode){
        case 0x00:
            /* BRK */
            switch(cpu->cycle){
                case 1:
                    cpu->target_cycle = 7;
                    break;
                case 2:
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
                case 1:
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
                case 1:
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
                case 1:
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
                case 1:
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
                case 1:
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
                case 1:
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
                case 1:
                    cpu->target_cycle = 6;
                    break;
                case 2:
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
            MN_CPU_IMP({
                MN_CPU_ASL(cpu->a);
            });
            break;

        case 0x18:
            /* CLC */
            MN_CPU_IMP({
                cpu->p &= ~MN_CPU_C;
            });
            break;

        case 0x2A:
            /* ROL */
            MN_CPU_IMP({
                MN_CPU_ROL(cpu->a);
            });
            break;

        case 0x38:
            /* SEC */
            MN_CPU_IMP({
                cpu->p |= MN_CPU_C;
            });
            break;

        case 0x4A:
            /* LSR */
            MN_CPU_IMP({
                MN_CPU_LSR(cpu->a);
            });
            break;

        case 0x58:
            /* CLI */
            MN_CPU_IMP({
                cpu->p &= ~MN_CPU_I;
            });
            break;

        case 0x6A:
            /* ROR */
            MN_CPU_IMP({
                MN_CPU_ROR(cpu->a);
            });
            break;

        case 0x78:
            /* SEI */
            MN_CPU_IMP({
                cpu->p |= MN_CPU_I;
            });
            break;

        case 0x88:
            /* DEY */
            MN_CPU_IMP({
                cpu->y--;

                MN_CPU_UPDATE_NZ(cpu->y);
            });
            break;

        case 0x8A:
            /* TXA */
            MN_CPU_IMP({
                cpu->a = cpu->x;

                MN_CPU_UPDATE_NZ(cpu->a);
            });
            break;

        case 0x98:
            /* TYA */
            MN_CPU_IMP({
                cpu->a = cpu->y;

                MN_CPU_UPDATE_NZ(cpu->a);
            });
            break;

        case 0x9A:
            /* TXS */
            MN_CPU_IMP({
                cpu->s = cpu->x;
            });
            break;

        case 0xA8:
            /* TAY */
            MN_CPU_IMP({
                cpu->y = cpu->a;

                MN_CPU_UPDATE_NZ(cpu->y);
            });
            break;

        case 0xAA:
            /* TAX */
            MN_CPU_IMP({
                cpu->x = cpu->a;

                MN_CPU_UPDATE_NZ(cpu->x);
            });
            break;

        case 0xB8:
            /* CLV */
            MN_CPU_IMP({
                cpu->p &= ~MN_CPU_V;
            });
            break;

        case 0xBA:
            /* TSX */
            MN_CPU_IMP({
                cpu->x = cpu->s;

                MN_CPU_UPDATE_NZ(cpu->x);
            });
            break;

        case 0xC8:
            /* INY */
            MN_CPU_IMP({
                cpu->y++;

                MN_CPU_UPDATE_NZ(cpu->y);
            });
            break;

        case 0xCA:
            /* DEX */
            MN_CPU_IMP({
                cpu->x++;

                MN_CPU_UPDATE_NZ(cpu->x);
            });
            break;

        case 0xD8:
            /* CLD */
            MN_CPU_IMP({
                cpu->p &= ~MN_CPU_D;
            });
            break;

        case 0xE8:
            /* INX */
            MN_CPU_IMP({
                cpu->x++;

                MN_CPU_UPDATE_NZ(cpu->x);
            });
            break;

        case 0xEA:
            /* NOP */
            MN_CPU_IMP({
                /* Do nothing */
            });
            break;

        case 0xF8:
            /* SED */
            MN_CPU_IMP({
                cpu->p |= MN_CPU_D;
            });
            break;

        /* Opcodes with immediate addressing */

        case 0x09:
            /* ORA */
            MN_CPU_IMM({
                cpu->a |= cpu->t;

                MN_CPU_UPDATE_NZ(cpu->a);
            });
            break;

        case 0x29:
            /* AND */
            MN_CPU_IMM({
                cpu->a &= cpu->t;

                MN_CPU_UPDATE_NZ(cpu->a);
            });
            break;

        case 0x49:
            /* EOR */
            MN_CPU_IMM({
                cpu->a ^= cpu->t;

                MN_CPU_UPDATE_NZ(cpu->a);
            });
            break;

        case 0x69:
            /* ADC */
            MN_CPU_IMM({
                MN_CPU_ADC(cpu->t);
            });
            break;

        case 0xA0:
            /* LDY */
            MN_CPU_IMM({
                cpu->y = cpu->t;

                MN_CPU_UPDATE_NZ(cpu->y);
            });
            break;

        case 0xA2:
            /* LDX */
            MN_CPU_IMM({
                cpu->x = cpu->t;

                MN_CPU_UPDATE_NZ(cpu->x);
            });
            break;

        case 0xA9:
            /* LDA */
            MN_CPU_IMM({
                cpu->a = cpu->t;

                MN_CPU_UPDATE_NZ(cpu->a);
            });
            break;

        case 0xC0:
            /* CPY */
            MN_CPU_IMM({
                MN_CPU_CMP(cpu->y, cpu->t);
            });
            break;

        case 0xC9:
            /* CMP */
            MN_CPU_IMM({
                MN_CPU_CMP(cpu->a, cpu->t);
            });
            break;

        case 0xE0:
            /* CPX */
            MN_CPU_IMM({
                MN_CPU_CMP(cpu->x, cpu->t);
            });
            break;

        case 0xEB: /* Unofficial opcode */
            /* NOTE: According to No More Secrets, it is the same [as $E9],
             *  said Fiskbit on the NesDev Discord. */
        case 0xE9:
            /* SBC */
            MN_CPU_IMM({
                MN_CPU_ADC(~cpu->t);
            });
            break;

        /* Absolute addressing */

        case 0x4C:
            /* JMP */
            switch(cpu->cycle){
                case 1:
                    cpu->target_cycle = 3;
                    break;
                case 2:
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
            MN_CPU_ABS_READ({
                cpu->a |= tmp;

                MN_CPU_UPDATE_NZ(cpu->a);
            });
            break;

        case 0x2C:
            /* BIT */
            MN_CPU_ABS_READ({
                MN_CPU_BIT(tmp);
            });
            break;

        case 0x2D:
            /* AND */
            MN_CPU_ABS_READ({
                cpu->a &= tmp;

                MN_CPU_UPDATE_NZ(cpu->a);
            });
            break;

        case 0x4D:
            /* EOR */
            MN_CPU_ABS_READ({
                cpu->a ^= tmp;

                MN_CPU_UPDATE_NZ(cpu->a);
            });
            break;

        case 0x6D:
            /* ADC */
            MN_CPU_ABS_READ({
                MN_CPU_ADC(tmp);
            });
            break;

        case 0xAC:
            /* LDY */
            MN_CPU_ABS_READ({
                cpu->y = tmp;

                MN_CPU_UPDATE_NZ(cpu->y);
            });
            break;

        case 0xAD:
            /* LDA */
            MN_CPU_ABS_READ({
                cpu->a = tmp;

                MN_CPU_UPDATE_NZ(cpu->a);
            });
            break;

        case 0xAE:
            /* LDX */
            MN_CPU_ABS_READ({
                cpu->x = tmp;

                MN_CPU_UPDATE_NZ(cpu->x);
            });
            break;

        case 0xCC:
            /* CPY */
            MN_CPU_ABS_READ({
                MN_CPU_CMP(cpu->y, tmp);
            });
            break;

        case 0xCD:
            /* CMP */
            MN_CPU_ABS_READ({
                MN_CPU_CMP(cpu->a, tmp);
            });
            break;

        case 0xEC:
            /* CPX */
            MN_CPU_ABS_READ({
                MN_CPU_CMP(cpu->x, tmp);
            });
            break;

        case 0xED:
            /* SBC */
            MN_CPU_ABS_READ({
                MN_CPU_ADC(~tmp);
                break;
            });
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

        case 0x84:
            /* STY */
            MN_CPU_ZP_STORE({
                tmp = cpu->y;
            });
            break;

        case 0x85:
            /* STA */
            MN_CPU_ZP_STORE({
                tmp = cpu->a;
            });
            break;

        case 0x86:
            /* STX */
            MN_CPU_ZP_STORE({
                tmp = cpu->x;
            });
            break;

        /* Indexed zeropage addressing */

        /* Indexed zeropage addressing - read instructions */

        /* Indexed with X */

        case 0x15:
            /* ORA */
            MN_CPU_ZPI_READ(cpu->x, {
                cpu->a |= tmp;

                MN_CPU_UPDATE_NZ(cpu->a);
            });
            break;

        case 0x35:
            /* AND */
            MN_CPU_ZPI_READ(cpu->x, {
                cpu->a &= tmp;

                MN_CPU_UPDATE_NZ(cpu->a);
            });
            break;

        case 0x55:
            /* EOR */
            MN_CPU_ZPI_READ(cpu->x, {
                cpu->a ^= tmp;

                MN_CPU_UPDATE_NZ(cpu->a);
            });
            break;

        case 0x75:
            /* ADC */
            MN_CPU_ZPI_READ(cpu->x, {
                MN_CPU_ADC(tmp);
            });
            break;

        case 0xB4:
            /* LDY */
            MN_CPU_ZPI_READ(cpu->x, {
                cpu->y = tmp;

                MN_CPU_UPDATE_NZ(cpu->y);
            });
            break;

        case 0xB5:
            /* LDA */
            MN_CPU_ZPI_READ(cpu->x, {
                cpu->a = tmp;

                MN_CPU_UPDATE_NZ(cpu->a);
            });
            break;

        case 0xD5:
            /* CMP */
            MN_CPU_ZPI_READ(cpu->x, {
                MN_CPU_CMP(cpu->a, tmp);
            });
            break;

        case 0xF5:
            /* SBC */
            MN_CPU_ZPI_READ(cpu->x, {
                MN_CPU_ADC(~tmp);
            });
            break;

        /* Indexed with Y */

        case 0xB6:
            /* LDX */
            MN_CPU_ZPI_READ(cpu->y, {
                cpu->x = tmp;

                MN_CPU_UPDATE_NZ(cpu->x);
            });
            break;

        /* Indexed zeropage addressing - RMW instructions */

        case 0x16:
            /* ASL */
            MN_CPU_ZPI_RMW({
                MN_CPU_ASL(cpu->t);
            });
            break;

        case 0x36:
            /* ROL */
            MN_CPU_ZPI_RMW({
                MN_CPU_ROL(cpu->t);
            });
            break;

        case 0x56:
            /* LSR */
            MN_CPU_ZPI_RMW({
                MN_CPU_LSR(cpu->t);
            });
            break;

        case 0x76:
            /* ROR */
            MN_CPU_ZPI_RMW({
                MN_CPU_ROR(cpu->t);
            });
            break;

        case 0xD6:
            /* DEC */
            MN_CPU_ZPI_RMW({
                cpu->t--;

                MN_CPU_UPDATE_NZ(cpu->t);
            });
            break;

        case 0xF6:
            /* INC */
            MN_CPU_ZPI_RMW({
                cpu->t++;

                MN_CPU_UPDATE_NZ(cpu->t);
            });
            break;

        /* Indexed zeropage addressing - write instructions */

        /* Indexed with X */

        case 0x94:
            /* STY */
            MN_CPU_ZPI_STORE(cpu->x, {
                tmp = cpu->y;
            });
            break;

        case 0x95:
            /* STA */
            MN_CPU_ZPI_STORE(cpu->x, {
                tmp = cpu->a;
            });
            break;

        /* Indexed with Y */

        case 0x96:
            /* STX */
            MN_CPU_ZPI_STORE(cpu->y, {
                tmp = cpu->x;
            });
            break;

        /* Absolute indexed addressing */

        /* Absolute indexed addressing - read instructions */

        /* Indexed with X */

        case 0xBC:
            /* LDY */
            MN_CPU_ABSI_READ(cpu->x, {
                cpu->y = tmp;

                MN_CPU_UPDATE_NZ(cpu->y);
            });
            break;

        /* Indexed with X or Y */

        case 0x19:
        case 0x1D:
            /* ORA */
            MN_CPU_ABSI_READ(cpu->opcode&(1<<2) ? cpu->x : cpu->y, {
                cpu->a |= tmp;

                MN_CPU_UPDATE_NZ(cpu->a);
            });
            break;

        case 0x39:
        case 0x3D:
            /* AND */
            MN_CPU_ABSI_READ(cpu->opcode&(1<<2) ? cpu->x : cpu->y, {
                cpu->a &= tmp;

                MN_CPU_UPDATE_NZ(cpu->a);
            });
            break;

        case 0x59:
        case 0x5D:
            /* EOR */
            MN_CPU_ABSI_READ(cpu->opcode&(1<<2) ? cpu->x : cpu->y, {
                cpu->a ^= tmp;

                MN_CPU_UPDATE_NZ(cpu->a);
            });
            break;

        case 0x79:
        case 0x7D:
            /* ADC */
            MN_CPU_ABSI_READ(cpu->opcode&(1<<2) ? cpu->x : cpu->y, {
                MN_CPU_ADC(tmp);
            });
            break;

        case 0xB9:
        case 0xBD:
            /* LDA */
            MN_CPU_ABSI_READ(cpu->opcode&(1<<2) ? cpu->x : cpu->y, {
                cpu->a = tmp;

                MN_CPU_UPDATE_NZ(cpu->a);
            });
            break;

        case 0xD9:
        case 0xDD:
            /* CMP */
            MN_CPU_ABSI_READ(cpu->opcode&(1<<2) ? cpu->x : cpu->y, {
                MN_CPU_CMP(cpu->a, tmp);
            });
            break;

        case 0xF9:
        case 0xFD:
            /* SBC */
            MN_CPU_ABSI_READ(cpu->opcode&(1<<2) ? cpu->x : cpu->y, {
                MN_CPU_ADC(~tmp);
            });
            break;

        /* Indexed with Y */

        case 0xBE:
            /* LDX */
            MN_CPU_ABSI_READ(cpu->x, {
                cpu->x = tmp;

                MN_CPU_UPDATE_NZ(cpu->x);
            });
            break;

        /* Absolute addressing - RMW instructions */

        case 0x1E:
            /* ASL */
            MN_CPU_ABSI_RMW({
                MN_CPU_ASL(cpu->t);
            });
            break;

        case 0x3E:
            /* ROL */
            MN_CPU_ABSI_RMW({
                MN_CPU_ROL(cpu->t);
            });
            break;

        case 0x5E:
            /* LSR */
            MN_CPU_ABSI_RMW({
                MN_CPU_LSR(cpu->t);
            });
            break;

        case 0x7E:
            /* ROR */
            MN_CPU_ABSI_RMW({
                MN_CPU_ROR(cpu->t);
            });
            break;

        case 0xDE:
            /* DEC */
            MN_CPU_ABSI_RMW({
                cpu->t--;

                MN_CPU_UPDATE_NZ(cpu->t);
            });
            break;

        case 0xFE:
            /* INC */
            MN_CPU_ABSI_RMW({
                cpu->t++;

                MN_CPU_UPDATE_NZ(cpu->t);
            });
            break;

        /* Absolute addressing - write instructions */

        case 0x99:
        case 0x9D:
            /* STA */
            MN_CPU_ABSI_STORE(cpu->opcode&(1<<2) ? cpu->x : cpu->y, {
                tmp = cpu->a;
            });
            break;

        /* Relative addressing */

        case 0x10:
            /* BPL */
            MN_CPU_RELATIVE(!(cpu->p&MN_CPU_N));
            break;

        case 0x30:
            /* BMI */
            MN_CPU_RELATIVE(cpu->p&MN_CPU_N);
            break;

        case 0x50:
            /* BVC */
            MN_CPU_RELATIVE(!(cpu->p&MN_CPU_V));
            break;

        case 0x70:
            /* BVS */
            MN_CPU_RELATIVE(cpu->p&MN_CPU_V);
            break;

        case 0x90:
            /* BCC */
            MN_CPU_RELATIVE(!(cpu->p&MN_CPU_C));
            break;

        case 0xB0:
            /* BCS */
            MN_CPU_RELATIVE(cpu->p&MN_CPU_C);
            break;

        case 0xD0:
            /* BNE */
            MN_CPU_RELATIVE(!(cpu->p&MN_CPU_Z));
            break;

        case 0xF0:
            /* BEQ */
            MN_CPU_RELATIVE(cpu->p&MN_CPU_Z);
            break;

        /* Indexed indirect addressing */

        /* Indexed indirect addressing - read instructions */

        case 0x01:
            /* ORA */
            MN_CPU_IDXIND_READ({
                cpu->a |= tmp;

                MN_CPU_UPDATE_NZ(cpu->a);
            });
            break;

        case 0x21:
            /* AND */
            MN_CPU_IDXIND_READ({
                cpu->a &= tmp;

                MN_CPU_UPDATE_NZ(cpu->a);
            });
            break;

        case 0x41:
            /* EOR */
            MN_CPU_IDXIND_READ({
                cpu->a ^= tmp;

                MN_CPU_UPDATE_NZ(cpu->a);
            });
            break;

        case 0x61:
            /* ADC */
            MN_CPU_IDXIND_READ({
                MN_CPU_ADC(tmp);
            });
            break;

        case 0xA1:
            /* LDA */
            MN_CPU_IDXIND_READ({
                cpu->a = tmp;

                MN_CPU_UPDATE_NZ(cpu->a);
            });
            break;

        case 0xC1:
            /* CMP */
            MN_CPU_IDXIND_READ({
                MN_CPU_CMP(cpu->a, tmp);
            });
            break;

        case 0xE1:
            /* SBC */
            MN_CPU_IDXIND_READ({
                MN_CPU_ADC(~tmp);
            });
            break;

        /* Indexed indirect addressing - Write instructions */

        case 0x81:
            /* STA */
            MN_CPU_IDXIND_WRITE({
                cpu->t = cpu->a;
            });
            break;

        /* Indirect indexed addressing */

        /* Indirect indexed addressing - Read instructions */

        case 0x11:
            /* ORA */
            MN_CPU_INDIDX_READ({
                cpu->a |= tmp;

                MN_CPU_UPDATE_NZ(cpu->a);
            });
            break;

        case 0x31:
            /* AND */
            MN_CPU_INDIDX_READ({
                cpu->a &= tmp;

                MN_CPU_UPDATE_NZ(cpu->a);
            });
            break;

        case 0x51:
            /* EOR */
            MN_CPU_INDIDX_READ({
                cpu->a ^= tmp;

                MN_CPU_UPDATE_NZ(cpu->a);
            });
            break;

        case 0x71:
            /* ADC */
            MN_CPU_INDIDX_READ({
                MN_CPU_ADC(tmp);
            });
            break;

        case 0xB1:
            /* LDA */
            MN_CPU_INDIDX_READ({
                cpu->a = tmp;

                MN_CPU_UPDATE_NZ(cpu->a);
            });
            break;

        case 0xD1:
            /* CMP */
            MN_CPU_INDIDX_READ({
                MN_CPU_CMP(cpu->a, tmp);
            });
            break;

        case 0xF1:
            /* SBC */
            MN_CPU_INDIDX_READ({
                MN_CPU_ADC(~tmp);
            });
            break;

        /* Indirect indexed addressing - write instructions */

        case 0x91:
            /* STA */
            MN_CPU_INDIDX_WRITE({
                tmp = cpu->a;
            });
            break;

        /* Indirect absolute addressing */

        case 0x6C:
            /* JMP */
            switch(cpu->cycle){
                case 1:
                    cpu->target_cycle = 5;
                    break;
                case 2:
                    cpu->pc++;
                    break;
                case 3:
                    cpu->tmp = emu->mapper.read(emu, &emu->mapper, cpu->pc)<<8;
                    cpu->tmp |= cpu->t;
                    cpu->pc++;
                    break;
                case 4:
                    cpu->t = emu->mapper.read(emu, &emu->mapper, cpu->pc);
                    break;
                case 5:
                    cpu->pc = emu->mapper.read(emu, &emu->mapper,
                                               cpu->tmp)<<8;
                    cpu->pc |= cpu->t;
                    break;
            }
            break;

        /* Unofficial opcodes */

        /* Implied addressing */

        case 0x1A:
        case 0x3A:
        case 0x5A:
        case 0x7A:
        case 0xDA:
        case 0xFA:
            /* NOP */
            MN_CPU_IMP({
                /* Do nothing */
            });
            break;

        /* Immediate addressing */

        case 0x80:
        case 0x82:
        case 0x89:
        case 0xC2:
            /* NOP */
            MN_CPU_IMM({
                /* Do nothing */
            });
            break;

        case 0xAB:
            /* LAX */
            MN_CPU_IMP({
                cpu->a = cpu->t;
                cpu->x = cpu->a;

                MN_CPU_UPDATE_NZ(cpu->a);
            });
            break;

        /* Absolute addressing */

        case 0x0C:
            /* NOP */
            switch(cpu->cycle){
                case 1:
                    cpu->target_cycle = 4;
                    break;
                case 2:
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
                case 1:
                    cpu->target_cycle = 4;
                    break;
                case 2:
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

        case 0x87:
            /* SAX */
            /* NOTE: Apparently it is unstable on the NES */
            MN_CPU_ZP_STORE({
                tmp = cpu->a&cpu->x;
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

    printf("c: %d, %c%c%c%c%c-%c%c op: %02x pc: %u\n", cpu->cycle,
           cpu->p&MN_CPU_C ? 'C' : '-',
           cpu->p&MN_CPU_Z ? 'Z' : '-',
           cpu->p&MN_CPU_I ? 'I' : '-',
           cpu->p&MN_CPU_D ? 'D' : '-',
           cpu->p&MN_CPU_B ? 'B' : '-',
           cpu->p&MN_CPU_N ? 'N' : '-',
           cpu->p&MN_CPU_V ? 'V' : '-', cpu->opcode, cpu->pc);

    if((cpu->opcode&31) != 16 && cpu->cycle == cpu->target_cycle-1){
        printf("poll, %u\n", cpu->target_cycle);
    }

    cpu->cycle++;
}

void mn_cpu_free(MNCPU *cpu) {
    /* TODO */
    (void)cpu;
}
