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

#if MN_CPU_DEBUG
/* NOTE: For debugging only */
#include <stdio.h>
#endif

int mn_cpu_init(MNCPU *cpu) {
    cpu->pc = 0;
    cpu->jammed = 0;
    cpu->halted = 0;

    /* TODO: Properly emulate the power on and reset sequences. */
    cpu->s = 0xFD;
    cpu->a = 0;
    cpu->x = 0;
    cpu->y = 0;
    cpu->p = MN_CPU_I;

    cpu->cycle = 8;
    cpu->target_cycle = 0;

    cpu->rdy = 1;

    cpu->irq_pin = 0;
    cpu->nmi_pin = 0;
    cpu->nmi_pin_last = 0;

    cpu->should_nmi = 0;
    cpu->should_irq = 0;
    cpu->nmi_detected = 0;
    cpu->irq_detected = 0;

    cpu->execute_int_next = 0;
    cpu->execute_int = 0;

    cpu->opcode_loaded = 0;

    cpu->skip_and = 0;

    return 0;
}


#if MN_CPU_DEBUG && MN_CPU_REPORT_POLLING
/* TODO: Choose another define name */
#undef MN_CPU_REPORT_POLLING
#define MN_CPU_REPORT_POLLING puts("poll");
#else
#undef MN_CPU_REPORT_POLLING
#define MN_CPU_REPORT_POLLING
#endif

static unsigned char mn_cpu_read(MNEmu *emu, unsigned short int addr) {
    register MNCPU *cpu = &emu->cpu;

    /* Halt the CPU on a read if RDY is low */
    if(!cpu->rdy) cpu->halted = 1;
    return cpu->last_read = emu->mapper.read(emu, &emu->mapper, addr);
}

#define MN_CPU_READ(addr) mn_cpu_read(emu, addr)
#define MN_CPU_WRITE(addr, value) emu->mapper.write(emu, &emu->mapper, addr, \
                                                    value)

#define MN_CPU_INTPOLL() \
    { \
        MN_CPU_REPORT_POLLING \
        if(cpu->should_nmi || (cpu->should_irq && !(cpu->p&MN_CPU_I))){ \
            cpu->execute_int_next = 1; \
        } \
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
        if((reg) >= (value)) cpu->p |= MN_CPU_C; \
        else cpu->p &= ~MN_CPU_C; \
        if((reg) == (value)) cpu->p |= MN_CPU_Z; \
        else cpu->p &= ~MN_CPU_Z; \
        if(((reg)-(value))&(1<<7)) cpu->p |= MN_CPU_N; \
        else cpu->p &= ~MN_CPU_N; \
    }

#define MN_CPU_ADC(value) \
    { \
        register unsigned char a; \
 \
        a = cpu->a; \
        cpu->a = (result = cpu->a+(value)+(cpu->p&MN_CPU_C)); \
 \
        if(result&(~0xFF)) cpu->p |= MN_CPU_C; \
        else cpu->p &= ~MN_CPU_C; \
 \
        if((result^a)&(result^(value))&(1<<7)){ \
            cpu->p |= MN_CPU_V; \
        }else{ \
            cpu->p &= ~MN_CPU_V; \
        } \
 \
        MN_CPU_UPDATE_NZ(cpu->a); \
    }

#define MN_CPU_SBC(value) \
    { \
        register unsigned char a; \
 \
        a = cpu->a; \
        cpu->a = (result = cpu->a-(value)-((cpu->p&MN_CPU_C)^1)); \
 \
        if(!(result&(~0xFF))) cpu->p |= MN_CPU_C; \
        else cpu->p &= ~MN_CPU_C; \
 \
        if((result^a)&(result^(~value))&(1<<7)){ \
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
        cpu->p |= (var>>7)&1; \
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
        cpu->p |= var&1; \
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
        cpu->p |= var&1; \
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
                tmp = MN_CPU_READ(cpu->pc); \
                cpu->tmp = cpu->t|(tmp<<8); \
                cpu->pc++; \
                break; \
            case 4: \
                tmp = MN_CPU_READ(cpu->tmp); \
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
                tmp = MN_CPU_READ(cpu->pc); \
                cpu->tmp = cpu->t|(tmp<<8); \
                cpu->pc++; \
                break; \
            case 4: \
                cpu->t = MN_CPU_READ(cpu->tmp); \
                break; \
            case 5: \
                MN_CPU_WRITE(cpu->tmp, cpu->t); \
 \
                /* Perform the operation on it */ \
                op; \
                break; \
            case 6: \
                MN_CPU_WRITE(cpu->tmp, cpu->t); \
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
                tmp = MN_CPU_READ(cpu->pc); \
                cpu->tmp = cpu->t|(tmp<<8); \
                cpu->pc++; \
                break; \
            case 4: \
                op; \
                MN_CPU_WRITE(cpu->tmp, tmp); \
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
                tmp = MN_CPU_READ(cpu->t); \
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
                cpu->t = MN_CPU_READ(cpu->tmp); \
                break; \
            case 4: \
                MN_CPU_WRITE(cpu->tmp, cpu->t); \
                op; \
                break; \
            case 5: \
                MN_CPU_WRITE(cpu->tmp, cpu->t); \
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
                MN_CPU_WRITE(cpu->t, tmp); \
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
                MN_CPU_READ(cpu->t); \
                cpu->t += i; \
                break; \
            case 4: \
                tmp = MN_CPU_READ(cpu->t); \
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
                MN_CPU_READ(cpu->t); \
                cpu->t += cpu->x; \
                break; \
            case 4: \
                cpu->tmp = cpu->t; \
                cpu->t = MN_CPU_READ(cpu->tmp); \
                break; \
            case 5: \
                MN_CPU_WRITE(cpu->tmp, cpu->t); \
                op; \
                break; \
            case 6: \
                MN_CPU_WRITE(cpu->tmp, cpu->t); \
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
                MN_CPU_READ(cpu->t); \
                cpu->t += i; \
                break; \
            case 4: \
                op; \
                MN_CPU_WRITE(cpu->t, tmp); \
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
                cpu->t = MN_CPU_READ(cpu->pc); \
                cpu->tmp += i; \
                tmp = cpu->tmp>>8; \
                cpu->tmp = (cpu->tmp&0xFF)|(cpu->t<<8); \
                cpu->t = tmp; \
                cpu->pc++; \
                break; \
            case 4: \
                tmp = MN_CPU_READ(cpu->tmp); \
                if(cpu->t){ \
                    cpu->tmp += cpu->t<<8; \
                    cpu->target_cycle++; \
                }else{ \
                    op; \
                } \
                break; \
            case 5: \
                /* Only executed if a page boundary had been crossed */ \
                tmp = MN_CPU_READ(cpu->tmp); \
                op; \
                break; \
        } \
    }

#define MN_CPU_ABSI_RMW(i, op) \
    { \
        switch(cpu->cycle){ \
            case 1: \
                cpu->target_cycle = 7; \
                break; \
            case 2: \
                cpu->pc++; \
                break; \
            case 3: \
                cpu->tmp = cpu->t; \
                cpu->t = MN_CPU_READ(cpu->pc); \
                cpu->tmp += i; \
                tmp = cpu->tmp>>8; \
                cpu->tmp = (cpu->tmp&0xFF)|(cpu->t<<8); \
                cpu->t = tmp; \
                cpu->pc++; \
                break; \
            case 4: \
                MN_CPU_READ(cpu->tmp); \
                cpu->tmp += cpu->t<<8; \
                break; \
            case 5: \
                cpu->t = MN_CPU_READ(cpu->tmp); \
                break; \
            case 6: \
                MN_CPU_WRITE(cpu->tmp, cpu->t); \
                op; \
                break; \
            case 7: \
                MN_CPU_WRITE(cpu->tmp, cpu->t); \
                break; \
        } \
    }

#define MN_CPU_ABSI_STORE(i, op) \
    { \
        switch(cpu->cycle){ \
            case 1: \
                cpu->target_cycle = 5; \
                break; \
            case 2: \
                cpu->pc++; \
                break; \
            case 3: \
                cpu->tmp = cpu->t; \
                cpu->t = MN_CPU_READ(cpu->pc); \
                cpu->tmp += i; \
                tmp = cpu->tmp>>8; \
                cpu->tmp = (cpu->tmp&0xFF)|(cpu->t<<8); \
                cpu->t = tmp; \
                cpu->pc++; \
                break; \
            case 4: \
                MN_CPU_READ(cpu->tmp); \
                cpu->tmp += cpu->t<<8; \
                break; \
            case 5: \
                op; \
                MN_CPU_WRITE(cpu->tmp, tmp); \
                break; \
        } \
    }

#define MN_CPU_RELATIVE(branch) \
    { \
        switch(cpu->cycle) { \
            case 1: \
                cpu->target_cycle = 3; \
                MN_CPU_INTPOLL(); \
                break; \
            case 2: \
                cpu->pc++; \
                break; \
            case 3: \
                tmp = MN_CPU_READ(cpu->pc); \
                if(branch){ \
                    if(cpu->t&(1<<7)){ \
                        cpu->tmp = cpu->pc-(256-cpu->t); \
                    }else{ \
                        cpu->tmp = cpu->pc+cpu->t; \
                    } \
                    cpu->pc = (cpu->tmp&0xFF)|(cpu->pc&0xFF00); \
                    if(cpu->pc != cpu->tmp){ \
                        MN_CPU_INTPOLL(); \
                    } \
                    cpu->target_cycle++; \
                }else{ \
                    cpu->opcode = tmp; \
                    cpu->opcode_loaded = 1; \
                } \
                break; \
            case 4: \
                tmp = MN_CPU_READ(cpu->pc); \
                if(cpu->pc != cpu->tmp) { \
                    cpu->pc = cpu->tmp; \
                }else{ \
                    cpu->opcode = tmp; \
                    cpu->opcode_loaded = 1; \
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
                MN_CPU_READ(cpu->t); \
                cpu->t += cpu->x; \
                break; \
            case 4: \
                cpu->tmp = MN_CPU_READ(cpu->t); \
                break; \
            case 5: \
                cpu->tmp |= MN_CPU_READ((cpu->t+1)&0xFF)<<8; \
                break; \
            case 6: \
                tmp = MN_CPU_READ(cpu->tmp); \
                op; \
                break; \
        } \
    }

#define MN_CPU_IDXIND_RMW(op) \
    { \
        switch(cpu->cycle){ \
            case 1: \
                cpu->target_cycle = 8; \
                break; \
            case 2: \
                cpu->pc++; \
                break; \
            case 3: \
                MN_CPU_READ(cpu->t); \
                cpu->t += cpu->x; \
                break; \
            case 4: \
                cpu->tmp = MN_CPU_READ(cpu->t); \
                break; \
            case 5: \
                cpu->tmp |= MN_CPU_READ((cpu->t+1)&0xFF)<<8; \
                break; \
            case 6: \
                cpu->t = MN_CPU_READ(cpu->tmp); \
                break; \
            case 7: \
                MN_CPU_WRITE(cpu->tmp, cpu->t); \
                op; \
                break; \
            case 8: \
                MN_CPU_WRITE(cpu->tmp, cpu->t); \
                break; \
        } \
    }

#define MN_CPU_IDXIND_STORE(op) \
    { \
        switch(cpu->cycle){ \
            case 1: \
                cpu->target_cycle = 6; \
                break; \
            case 2: \
                cpu->pc++; \
                break; \
            case 3: \
                MN_CPU_READ(cpu->t); \
                cpu->t += cpu->x; \
                break; \
            case 4: \
                cpu->tmp = MN_CPU_READ(cpu->t); \
                break; \
            case 5: \
                cpu->tmp |= MN_CPU_READ((cpu->t+1)&0xFF)<<8; \
                break; \
            case 6: \
                op; \
                MN_CPU_WRITE(cpu->tmp, tmp); \
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
                cpu->tmp = MN_CPU_READ(cpu->t); \
                break; \
            case 4: \
                cpu->tmp |= MN_CPU_READ((cpu->t+1)&0xFF)<<8; \
                cpu->tmp2 = cpu->tmp+cpu->y; \
                cpu->tmp = (cpu->tmp2&0xFF)|(cpu->tmp&0xFF00); \
                break; \
            case 5: \
                tmp = MN_CPU_READ(cpu->tmp); \
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
                tmp = MN_CPU_READ(cpu->tmp); \
                op; \
                break; \
        } \
    }

#define MN_CPU_INDIDX_RMW(op) \
    { \
        switch(cpu->cycle){ \
            case 1: \
                cpu->target_cycle = 8; \
                break; \
            case 2: \
                cpu->pc++; \
                break; \
            case 3: \
                cpu->tmp = MN_CPU_READ(cpu->t); \
                break; \
            case 4: \
                cpu->tmp |= MN_CPU_READ((cpu->t+1)&0xFF)<<8; \
                cpu->tmp2 = cpu->tmp+cpu->y; \
                cpu->tmp = (cpu->tmp2&0xFF)|(cpu->tmp&0xFF00); \
                break; \
            case 5: \
                MN_CPU_READ(cpu->tmp); \
                cpu->tmp = cpu->tmp2; \
                break; \
            case 6: \
                cpu->t = MN_CPU_READ(cpu->tmp); \
                break; \
            case 7: \
                MN_CPU_WRITE(cpu->tmp, cpu->t); \
                op; \
                break; \
            case 8: \
                MN_CPU_WRITE(cpu->tmp, cpu->t); \
                break; \
        } \
    }

#define MN_CPU_INDIDX_STORE(op) \
    { \
        switch(cpu->cycle){ \
            case 1: \
                cpu->target_cycle = 6; \
                break; \
            case 2: \
                cpu->pc++; \
                break; \
            case 3: \
                cpu->tmp = MN_CPU_READ(cpu->t); \
                break; \
            case 4: \
                cpu->tmp |= MN_CPU_READ((cpu->t+1)&0xFF)<<8; \
                cpu->tmp2 = cpu->tmp+cpu->y; \
                cpu->tmp = (cpu->tmp2&0xFF)|(cpu->tmp&0xFF00); \
                break; \
            case 5: \
                tmp = MN_CPU_READ(cpu->tmp); \
                cpu->tmp = cpu->tmp2; \
                break; \
            case 6: \
                op; \
                MN_CPU_WRITE(cpu->tmp, tmp); \
                break; \
        } \
    }


#define MN_CPU_INDIDX_SH(op) \
    { \
        switch(cpu->cycle){ \
            case 1: \
                cpu->target_cycle = 6; \
                break; \
            case 2: \
                cpu->pc++; \
                break; \
            case 3: \
                cpu->tmp = MN_CPU_READ(cpu->t); \
                break; \
            case 4: \
                cpu->skip_and = ~cpu->rdy; \
                cpu->tmp |= MN_CPU_READ((cpu->t+1)&0xFF)<<8; \
                cpu->tmp2 = cpu->tmp+cpu->y; \
                cpu->tmp = (cpu->tmp2&0xFF)|(cpu->tmp&0xFF00); \
                break; \
            case 5: \
                tmp = MN_CPU_READ(cpu->tmp); \
                cpu->t = cpu->tmp2-cpu->tmp; \
                cpu->tmp = cpu->tmp2; \
                break; \
            case 6: \
                op; \
                if(cpu->t && !cpu->skip_and) cpu->tmp &= tmp<<8|0xFF; \
                MN_CPU_WRITE(cpu->tmp, tmp); \
                break; \
        } \
    }

#define MN_CPU_ABSI_SH(i, op) \
    { \
        switch(cpu->cycle){ \
            case 1: \
                cpu->target_cycle = 5; \
                break; \
            case 2: \
                cpu->pc++; \
                break; \
            case 3: \
                cpu->skip_and = ~cpu->rdy; \
                cpu->tmp = cpu->t; \
                cpu->t = MN_CPU_READ(cpu->pc); \
                cpu->tmp += i; \
                tmp = cpu->tmp>>8; \
                cpu->tmp = (cpu->tmp&0xFF)|(cpu->t<<8); \
                cpu->t = tmp; \
                cpu->pc++; \
                break; \
            case 4: \
                MN_CPU_READ(cpu->tmp); \
                cpu->tmp += cpu->t<<8; \
                break; \
            case 5: \
                op; \
                if(cpu->t && !cpu->skip_and) cpu->tmp &= tmp<<8|0xFF; \
                MN_CPU_WRITE(cpu->tmp, tmp); \
                break; \
        } \
    }

#define MN_CPU_OP_INFO() \
    { \
        printf("%c%c%c%c%c1%c%c (p: %02x) op: %02x pc: %04x a: %02x x: %02x " \
               "y: %02x s: %02x\n", \
               cpu->p&MN_CPU_C ? 'C' : '-', \
               cpu->p&MN_CPU_Z ? 'Z' : '-', \
               cpu->p&MN_CPU_I ? 'I' : '-', \
               cpu->p&MN_CPU_D ? 'D' : '-', \
               cpu->p&MN_CPU_B ? 'B' : '-', \
               cpu->p&MN_CPU_V ? 'V' : '-', \
               cpu->p&MN_CPU_N ? 'N' : '-', cpu->p|(1<<5), cpu->opcode, \
               cpu->pc, cpu->a, cpu->x, cpu->y, cpu->s); \
    }

#define MN_CPU_OP(hex, op) \
    void mn_cpu_opcode_##hex(MNCPU *cpu, MNEmu *emu) { \
        unsigned char tmp; \
        unsigned short int result; \
 \
        (void)cpu; \
        (void)emu; \
 \
        /* TODO: Define tmp and result in op's scope instead. */ \
        (void)tmp; \
        (void)result; \
 \
        op; \
    }

MN_CPU_OP(00, {
    /* BRK */
    switch(cpu->cycle){
        case 1:
            cpu->target_cycle = 7;
            break;
        case 2:
            cpu->pc++;
            break;
        case 3:
            MN_CPU_WRITE(0x0100+cpu->s, cpu->pc>>8);
            cpu->s--;
            cpu->p |= MN_CPU_B;
            break;
        case 4:
            MN_CPU_WRITE(0x0100+cpu->s, cpu->pc);
            cpu->s--;
            cpu->is_irq = 1;
            if(cpu->should_nmi){
                cpu->is_irq = 0;
                cpu->should_nmi = 0;
            }
            break;
        case 5:
            MN_CPU_WRITE(0x0100+cpu->s, cpu->p);
            cpu->s--;
            break;
        case 6:
            cpu->pc &= 0xFF00;
            cpu->pc |= MN_CPU_READ(cpu->is_irq ? 0xFFFE : 0xFFFA);
            break;
        case 7:
            cpu->pc &= 0xFF;
            cpu->pc |= MN_CPU_READ(cpu->is_irq ? 0xFFFF : 0xFFFB)<<8;
            if(!cpu->is_irq) cpu->should_nmi = 0;
            break;
    }
})

MN_CPU_OP(40, {
    /* RTI */
    switch(cpu->cycle){
        case 1:
            cpu->target_cycle = 6;
            break;
        case 3:
            cpu->s++;
            break;
        case 4:
            cpu->p = MN_CPU_READ(0x0100+cpu->s);
            cpu->s++;
            break;
        case 5:
            cpu->pc &= 0xFF00;
            cpu->pc |= MN_CPU_READ(0x0100+cpu->s);
            cpu->s++;
            break;
        case 6:
            cpu->pc &= 0xFF;
            cpu->pc |= MN_CPU_READ(0x0100+cpu->s)<<8;
            break;
    }
})

MN_CPU_OP(60, {
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
            cpu->pc |= MN_CPU_READ(0x0100+cpu->s);
            cpu->s++;
            break;
        case 5:
            cpu->pc &= 0xFF;
            cpu->pc |= MN_CPU_READ(0x0100+cpu->s)<<8;
            break;
        case 6:
            cpu->pc++;
    }
})

MN_CPU_OP(48, {
    /* PHA */
    switch(cpu->cycle){
        case 1:
            cpu->target_cycle = 3;
            break;
        case 3:
            MN_CPU_WRITE(0x0100+cpu->s, cpu->a);
            cpu->s--;
            break;
    }
})

MN_CPU_OP(08, {
    /* PHP */
    switch(cpu->cycle){
        case 1:
            cpu->target_cycle = 3;
            break;
        case 3:
            MN_CPU_WRITE(0x0100+cpu->s, cpu->p|(1<<5)|MN_CPU_B);
            cpu->s--;
            break;
    }
})

MN_CPU_OP(68, {
    /* PLA */
    switch(cpu->cycle){
        case 1:
            cpu->target_cycle = 4;
            break;
        case 3:
            cpu->s++;
            break;
        case 4:
            cpu->a = MN_CPU_READ(0x0100+cpu->s);

            MN_CPU_UPDATE_NZ(cpu->a);
            break;
    }
})

MN_CPU_OP(28, {
    /* PLP */
    switch(cpu->cycle){
        case 1:
            cpu->target_cycle = 4;
            break;
        case 3:
            cpu->s++;
            break;
        case 4:
            cpu->p = MN_CPU_READ(0x0100+cpu->s)&~MN_CPU_B;
            break;
    }
})

MN_CPU_OP(20, {
    /* JSR */
    switch(cpu->cycle){
        case 1:
            cpu->target_cycle = 6;
            break;
        case 2:
            cpu->pc++;
            break;
        case 4:
            MN_CPU_WRITE(0x0100+cpu->s, cpu->pc>>8);
            cpu->s--;
            break;
        case 5:
            MN_CPU_WRITE(0x0100+cpu->s, cpu->pc);
            cpu->s--;
            break;
        case 6:
            cpu->pc = cpu->t|(MN_CPU_READ(cpu->pc)<<8);
            break;
    }
})

/* Official opcodes with implied or accumulator addressing */

/* NOTE: The CPU cycle number is always equal or bigger to 2 when
 * reaching this switch. */

MN_CPU_OP(0A, {
    /* ASL */
    MN_CPU_IMP({
        MN_CPU_ASL(cpu->a);
    });
})

MN_CPU_OP(18, {
    /* CLC */
    MN_CPU_IMP({
        cpu->p &= ~MN_CPU_C;
    });
})

MN_CPU_OP(2A, {
    /* ROL */
    MN_CPU_IMP({
        MN_CPU_ROL(cpu->a);
    });
})

MN_CPU_OP(38, {
    /* SEC */
    MN_CPU_IMP({
        cpu->p |= MN_CPU_C;
    });
})

MN_CPU_OP(4A, {
    /* LSR */
    MN_CPU_IMP({
        MN_CPU_LSR(cpu->a);
    });
})

MN_CPU_OP(58, {
    /* CLI */
    MN_CPU_IMP({
        cpu->p &= ~MN_CPU_I;
    });
})

MN_CPU_OP(6A, {
    /* ROR */
    MN_CPU_IMP({
        MN_CPU_ROR(cpu->a);
    });
})

MN_CPU_OP(78, {
    /* SEI */
    MN_CPU_IMP({
        cpu->p |= MN_CPU_I;
    });
})

MN_CPU_OP(88, {
    /* DEY */
    MN_CPU_IMP({
        cpu->y--;

        MN_CPU_UPDATE_NZ(cpu->y);
    });
})

MN_CPU_OP(8A, {
    /* TXA */
    MN_CPU_IMP({
        cpu->a = cpu->x;

        MN_CPU_UPDATE_NZ(cpu->a);
    });
})

MN_CPU_OP(98, {
    /* TYA */
    MN_CPU_IMP({
        cpu->a = cpu->y;

        MN_CPU_UPDATE_NZ(cpu->a);
    });
})

MN_CPU_OP(9A, {
    /* TXS */
    MN_CPU_IMP({
        cpu->s = cpu->x;
    });
})

MN_CPU_OP(A8, {
    /* TAY */
    MN_CPU_IMP({
        cpu->y = cpu->a;

        MN_CPU_UPDATE_NZ(cpu->y);
    });
})

MN_CPU_OP(AA, {
    /* TAX */
    MN_CPU_IMP({
        cpu->x = cpu->a;

        MN_CPU_UPDATE_NZ(cpu->x);
    });
})

MN_CPU_OP(B8, {
    /* CLV */
    MN_CPU_IMP({
        cpu->p &= ~MN_CPU_V;
    });
})

MN_CPU_OP(BA, {
    /* TSX */
    MN_CPU_IMP({
        cpu->x = cpu->s;

        MN_CPU_UPDATE_NZ(cpu->x);
    });
})

MN_CPU_OP(C8, {
    /* INY */
    MN_CPU_IMP({
        cpu->y++;

        MN_CPU_UPDATE_NZ(cpu->y);
    });
})

MN_CPU_OP(CA, {
    /* DEX */
    MN_CPU_IMP({
        cpu->x--;

        MN_CPU_UPDATE_NZ(cpu->x);
    });
})

MN_CPU_OP(D8, {
    /* CLD */
    MN_CPU_IMP({
        cpu->p &= ~MN_CPU_D;
    });
})

MN_CPU_OP(E8, {
    /* INX */
    MN_CPU_IMP({
        cpu->x++;

        MN_CPU_UPDATE_NZ(cpu->x);
    });
})

MN_CPU_OP(EA, {
    /* NOP */
    MN_CPU_IMP({
        /* Do nothing */
    });
})

MN_CPU_OP(F8, {
    /* SED */
    MN_CPU_IMP({
        cpu->p |= MN_CPU_D;
    });
})

/* Opcodes with immediate addressing */

MN_CPU_OP(09, {
    /* ORA */
    MN_CPU_IMM({
        cpu->a |= cpu->t;

        MN_CPU_UPDATE_NZ(cpu->a);
    });
})

MN_CPU_OP(29, {
    /* AND */
    MN_CPU_IMM({
        cpu->a &= cpu->t;

        MN_CPU_UPDATE_NZ(cpu->a);
    });
})

MN_CPU_OP(49, {
    /* EOR */
    MN_CPU_IMM({
        cpu->a ^= cpu->t;

        MN_CPU_UPDATE_NZ(cpu->a);
    });
})

MN_CPU_OP(69, {
    /* ADC */
    MN_CPU_IMM({
        MN_CPU_ADC(cpu->t);
    });
})

MN_CPU_OP(A0, {
    /* LDY */
    MN_CPU_IMM({
        cpu->y = cpu->t;

        MN_CPU_UPDATE_NZ(cpu->y);
    });
})

MN_CPU_OP(A2, {
    /* LDX */
    MN_CPU_IMM({
        cpu->x = cpu->t;

        MN_CPU_UPDATE_NZ(cpu->x);
    });
})

MN_CPU_OP(A9, {
    /* LDA */
    MN_CPU_IMM({
        cpu->a = cpu->t;

        MN_CPU_UPDATE_NZ(cpu->a);
    });
})

MN_CPU_OP(C0, {
    /* CPY */
    MN_CPU_IMM({
        MN_CPU_CMP(cpu->y, cpu->t);
    });
})

MN_CPU_OP(C9, {
    /* CMP */
    MN_CPU_IMM({
        MN_CPU_CMP(cpu->a, cpu->t);
    });
})

MN_CPU_OP(E0, {
    /* CPX */
    MN_CPU_IMM({
        MN_CPU_CMP(cpu->x, cpu->t);
    });
})

/* NOTE: According to No More Secrets, it [$EB] is the same [as $E9], said
 * Fiskbit on the NesDev Discord. */
MN_CPU_OP(E9_EB, {
    /* SBC */
    MN_CPU_IMM({
        MN_CPU_SBC(cpu->t);
    });
})

/* Absolute addressing */

MN_CPU_OP(4C, {
    /* JMP */
    switch(cpu->cycle){
        case 1:
            cpu->target_cycle = 3;
            break;
        case 2:
            cpu->pc++;
            break;
        case 3:
            tmp = MN_CPU_READ(cpu->pc);
            cpu->pc = cpu->t|(tmp<<8);
            break;
    }
})

/* Absolute addressing - read instructions */

MN_CPU_OP(0D, {
    /* ORA */
    MN_CPU_ABS_READ({
        cpu->a |= tmp;

        MN_CPU_UPDATE_NZ(cpu->a);
    });
})

MN_CPU_OP(2C, {
    /* BIT */
    MN_CPU_ABS_READ({
        MN_CPU_BIT(tmp);
    });
})

MN_CPU_OP(2D, {
    /* AND */
    MN_CPU_ABS_READ({
        cpu->a &= tmp;

        MN_CPU_UPDATE_NZ(cpu->a);
    });
})

MN_CPU_OP(4D, {
    /* EOR */
    MN_CPU_ABS_READ({
        cpu->a ^= tmp;

        MN_CPU_UPDATE_NZ(cpu->a);
    });
})

MN_CPU_OP(6D, {
    /* ADC */
    MN_CPU_ABS_READ({
        MN_CPU_ADC(tmp);
    });
})

MN_CPU_OP(AC, {
    /* LDY */
    MN_CPU_ABS_READ({
        cpu->y = tmp;

        MN_CPU_UPDATE_NZ(cpu->y);
    });
})

MN_CPU_OP(AD, {
    /* LDA */
    MN_CPU_ABS_READ({
        cpu->a = tmp;

        MN_CPU_UPDATE_NZ(cpu->a);
    });
})

MN_CPU_OP(AE, {
    /* LDX */
    MN_CPU_ABS_READ({
        cpu->x = tmp;

        MN_CPU_UPDATE_NZ(cpu->x);
    });
})

MN_CPU_OP(CC, {
    /* CPY */
    MN_CPU_ABS_READ({
        MN_CPU_CMP(cpu->y, tmp);
    });
})

MN_CPU_OP(CD, {
    /* CMP */
    MN_CPU_ABS_READ({
        MN_CPU_CMP(cpu->a, tmp);
    });
})

MN_CPU_OP(EC, {
    /* CPX */
    MN_CPU_ABS_READ({
        MN_CPU_CMP(cpu->x, tmp);
    });
})

MN_CPU_OP(ED, {
    /* SBC */
    MN_CPU_ABS_READ({
        MN_CPU_SBC(tmp);
        break;
    });
})

/* Absolute addressing - read-modify-write (RMW) instructions */

MN_CPU_OP(0E, {
    /* ASL */
    MN_CPU_ABS_RMW({
        MN_CPU_ASL(cpu->t);
    });
})

MN_CPU_OP(2E, {
    /* ROL */
    MN_CPU_ABS_RMW({
        MN_CPU_ROL(cpu->t);
    });
})

MN_CPU_OP(4E, {
    /* LSR */
    MN_CPU_ABS_RMW({
        MN_CPU_LSR(cpu->t);
    });
})

MN_CPU_OP(6E, {
    /* ROR */
    MN_CPU_ABS_RMW({
        MN_CPU_ROR(cpu->t);
    });
})

MN_CPU_OP(CE, {
    /* DEC */
    MN_CPU_ABS_RMW({
        cpu->t--;

        MN_CPU_UPDATE_NZ(cpu->t);
    });
})

MN_CPU_OP(EE, {
    /* INC */
    MN_CPU_ABS_RMW({
        cpu->t++;

        MN_CPU_UPDATE_NZ(cpu->t);
    });
})

/* Absolute addressing - write instructions */

MN_CPU_OP(8C, {
    /* STY */
    MN_CPU_ABS_STORE({
        tmp = cpu->y;
    });
})

MN_CPU_OP(8D, {
    /* STA */
    MN_CPU_ABS_STORE({
        tmp = cpu->a;
    });
})

MN_CPU_OP(8E, {
    /* STX */
    MN_CPU_ABS_STORE({
        tmp = cpu->x;
    });
})

/* Zeropage addressing */

/* Zeropage addressing - read instructions */

MN_CPU_OP(05, {
    /* ORA */
    MN_CPU_ZP_READ({
        cpu->a |= tmp;

        MN_CPU_UPDATE_NZ(cpu->a);
    });
})

MN_CPU_OP(24, {
    /* BIT */
    MN_CPU_ZP_READ({
        MN_CPU_BIT(tmp);
    });
})

MN_CPU_OP(25, {
    /* AND */
    MN_CPU_ZP_READ({
        cpu->a &= tmp;

        MN_CPU_UPDATE_NZ(cpu->a);
    });
})

MN_CPU_OP(45, {
    /* EOR */
    MN_CPU_ZP_READ({
        cpu->a ^= tmp;

        MN_CPU_UPDATE_NZ(cpu->a);
    });
})

MN_CPU_OP(65, {
    /* ADC */
    MN_CPU_ZP_READ({
        MN_CPU_ADC(tmp);
    });
})

MN_CPU_OP(A4, {
    /* LDY */
    MN_CPU_ZP_READ({
        cpu->y = tmp;

        MN_CPU_UPDATE_NZ(cpu->y);
    });
})

MN_CPU_OP(A5, {
    /* LDA */
    MN_CPU_ZP_READ({
        cpu->a = tmp;

        MN_CPU_UPDATE_NZ(cpu->a);
    });
})

MN_CPU_OP(A6, {
    /* LDX */
    MN_CPU_ZP_READ({
        cpu->x = tmp;

        MN_CPU_UPDATE_NZ(cpu->x);
    });
})

MN_CPU_OP(C4, {
    /* CPY */
    MN_CPU_ZP_READ({
        MN_CPU_CMP(cpu->y, tmp);
    });
})

MN_CPU_OP(C5, {
    /* CMP */
    MN_CPU_ZP_READ({
        MN_CPU_CMP(cpu->a, tmp);
    });
})

MN_CPU_OP(E4, {
    /* CPX */
    MN_CPU_ZP_READ({
        MN_CPU_CMP(cpu->x, tmp);
    });
})

MN_CPU_OP(E5, {
    /* SBC */
    MN_CPU_ZP_READ({
        MN_CPU_SBC(tmp);
    });
})

/* Zeropage addressing - RMW instructions */

MN_CPU_OP(06, {
    /* ASL */
    MN_CPU_ZP_RMW({
        MN_CPU_ASL(cpu->t);
    });
})

MN_CPU_OP(26, {
    /* ROL */
    MN_CPU_ZP_RMW({
        MN_CPU_ROL(cpu->t);
    });
})

MN_CPU_OP(46, {
    /* LSR */
    MN_CPU_ZP_RMW({
        MN_CPU_LSR(cpu->t);
    });
})

MN_CPU_OP(66, {
    /* ROR */
    MN_CPU_ZP_RMW({
        MN_CPU_ROR(cpu->t);
    });
})

MN_CPU_OP(C6, {
    /* DEC */
    MN_CPU_ZP_RMW({
        cpu->t--;

        MN_CPU_UPDATE_NZ(cpu->t);
    });
})

MN_CPU_OP(E6, {
    /* INC */
    MN_CPU_ZP_RMW({
        cpu->t++;

        MN_CPU_UPDATE_NZ(cpu->t);
    });
})

/* Zeropage addressing - write instructions */

MN_CPU_OP(84, {
    /* STY */
    MN_CPU_ZP_STORE({
        tmp = cpu->y;
    });
})

MN_CPU_OP(85, {
    /* STA */
    MN_CPU_ZP_STORE({
        tmp = cpu->a;
    });
})

MN_CPU_OP(86, {
    /* STX */
    MN_CPU_ZP_STORE({
        tmp = cpu->x;
    });
})

/* Indexed zeropage addressing */

/* Indexed zeropage addressing - read instructions */

/* Indexed with X */

MN_CPU_OP(15, {
    /* ORA */
    MN_CPU_ZPI_READ(cpu->x, {
        cpu->a |= tmp;

        MN_CPU_UPDATE_NZ(cpu->a);
    });
})

MN_CPU_OP(35, {
    /* AND */
    MN_CPU_ZPI_READ(cpu->x, {
        cpu->a &= tmp;

        MN_CPU_UPDATE_NZ(cpu->a);
    });
})

MN_CPU_OP(55, {
    /* EOR */
    MN_CPU_ZPI_READ(cpu->x, {
        cpu->a ^= tmp;

        MN_CPU_UPDATE_NZ(cpu->a);
    });
})

MN_CPU_OP(75, {
    /* ADC */
    MN_CPU_ZPI_READ(cpu->x, {
        MN_CPU_ADC(tmp);
    });
})

MN_CPU_OP(B4, {
    /* LDY */
    MN_CPU_ZPI_READ(cpu->x, {
        cpu->y = tmp;

        MN_CPU_UPDATE_NZ(cpu->y);
    });
})

MN_CPU_OP(B5, {
    /* LDA */
    MN_CPU_ZPI_READ(cpu->x, {
        cpu->a = tmp;

        MN_CPU_UPDATE_NZ(cpu->a);
    });
})

MN_CPU_OP(D5, {
    /* CMP */
    MN_CPU_ZPI_READ(cpu->x, {
        MN_CPU_CMP(cpu->a, tmp);
    });
})

MN_CPU_OP(F5, {
    /* SBC */
    MN_CPU_ZPI_READ(cpu->x, {
        MN_CPU_SBC(tmp);
    });
})

/* Indexed with Y */

MN_CPU_OP(B6, {
    /* LDX */
    MN_CPU_ZPI_READ(cpu->y, {
        cpu->x = tmp;

        MN_CPU_UPDATE_NZ(cpu->x);
    });
})

/* Indexed zeropage addressing - RMW instructions */

MN_CPU_OP(16, {
    /* ASL */
    MN_CPU_ZPI_RMW({
        MN_CPU_ASL(cpu->t);
    });
})

MN_CPU_OP(36, {
    /* ROL */
    MN_CPU_ZPI_RMW({
        MN_CPU_ROL(cpu->t);
    });
})

MN_CPU_OP(56, {
    /* LSR */
    MN_CPU_ZPI_RMW({
        MN_CPU_LSR(cpu->t);
    });
})

MN_CPU_OP(76, {
    /* ROR */
    MN_CPU_ZPI_RMW({
        MN_CPU_ROR(cpu->t);
    });
})

MN_CPU_OP(D6, {
    /* DEC */
    MN_CPU_ZPI_RMW({
        cpu->t--;

        MN_CPU_UPDATE_NZ(cpu->t);
    });
})

MN_CPU_OP(F6, {
    /* INC */
    MN_CPU_ZPI_RMW({
        cpu->t++;

        MN_CPU_UPDATE_NZ(cpu->t);
    });
})

/* Indexed zeropage addressing - write instructions */

/* Indexed with X */

MN_CPU_OP(94, {
    /* STY */
    MN_CPU_ZPI_STORE(cpu->x, {
        tmp = cpu->y;
    });
})

MN_CPU_OP(95, {
    /* STA */
    MN_CPU_ZPI_STORE(cpu->x, {
        tmp = cpu->a;
    });
})

/* Indexed with Y */

MN_CPU_OP(96, {
    /* STX */
    MN_CPU_ZPI_STORE(cpu->y, {
        tmp = cpu->x;
    });
})

/* Absolute indexed addressing */

/* Absolute indexed addressing - read instructions */

/* Indexed with X */

MN_CPU_OP(BC, {
    /* LDY */
    MN_CPU_ABSI_READ(cpu->x, {
        cpu->y = tmp;

        MN_CPU_UPDATE_NZ(cpu->y);
    });
})

/* Indexed with X or Y */

MN_CPU_OP(19_1D, {
    /* ORA */
    MN_CPU_ABSI_READ(cpu->opcode&(1<<2) ? cpu->x : cpu->y, {
        cpu->a |= tmp;

        MN_CPU_UPDATE_NZ(cpu->a);
    });
})

MN_CPU_OP(39_3D, {
    /* AND */
    MN_CPU_ABSI_READ(cpu->opcode&(1<<2) ? cpu->x : cpu->y, {
        cpu->a &= tmp;

        MN_CPU_UPDATE_NZ(cpu->a);
    });
})

MN_CPU_OP(59_5D, {
    /* EOR */
    MN_CPU_ABSI_READ(cpu->opcode&(1<<2) ? cpu->x : cpu->y, {
        cpu->a ^= tmp;

        MN_CPU_UPDATE_NZ(cpu->a);
    });
})

MN_CPU_OP(79_7D, {
    /* ADC */
    MN_CPU_ABSI_READ(cpu->opcode&(1<<2) ? cpu->x : cpu->y, {
        MN_CPU_ADC(tmp);
    });
})

MN_CPU_OP(B9_BD, {
    /* LDA */
    MN_CPU_ABSI_READ(cpu->opcode&(1<<2) ? cpu->x : cpu->y, {
        cpu->a = tmp;

        MN_CPU_UPDATE_NZ(cpu->a);
    });
})

MN_CPU_OP(D9_DD, {
    /* CMP */
    MN_CPU_ABSI_READ(cpu->opcode&(1<<2) ? cpu->x : cpu->y, {
        MN_CPU_CMP(cpu->a, tmp);
    });
})

MN_CPU_OP(F9_FD, {
    /* SBC */
    MN_CPU_ABSI_READ(cpu->opcode&(1<<2) ? cpu->x : cpu->y, {
        MN_CPU_SBC(tmp);
    });
})

/* Indexed with Y */

MN_CPU_OP(BE, {
    /* LDX */
    MN_CPU_ABSI_READ(cpu->y, {
        cpu->x = tmp;

        MN_CPU_UPDATE_NZ(cpu->x);
    });
})

/* Absolute addressing - RMW instructions */

MN_CPU_OP(1E, {
    /* ASL */
    MN_CPU_ABSI_RMW(cpu->x, {
        MN_CPU_ASL(cpu->t);
    });
})

MN_CPU_OP(3E, {
    /* ROL */
    MN_CPU_ABSI_RMW(cpu->x, {
        MN_CPU_ROL(cpu->t);
    });
})

MN_CPU_OP(5E, {
    /* LSR */
    MN_CPU_ABSI_RMW(cpu->x, {
        MN_CPU_LSR(cpu->t);
    });
})

MN_CPU_OP(7E, {
    /* ROR */
    MN_CPU_ABSI_RMW(cpu->x, {
        MN_CPU_ROR(cpu->t);
    });
})

MN_CPU_OP(DE, {
    /* DEC */
    MN_CPU_ABSI_RMW(cpu->x, {
        cpu->t--;

        MN_CPU_UPDATE_NZ(cpu->t);
    });
})

MN_CPU_OP(FE, {
    /* INC */
    MN_CPU_ABSI_RMW(cpu->x, {
        cpu->t++;

        MN_CPU_UPDATE_NZ(cpu->t);
    });
})

/* Absolute addressing - write instructions */

MN_CPU_OP(99_9D, {
    /* STA */
    MN_CPU_ABSI_STORE(cpu->opcode&(1<<2) ? cpu->x : cpu->y, {
        tmp = cpu->a;
    });
})

/* Relative addressing */

MN_CPU_OP(10, {
    /* BPL */
    MN_CPU_RELATIVE(!(cpu->p&MN_CPU_N));
})

MN_CPU_OP(30, {
    /* BMI */
    MN_CPU_RELATIVE(cpu->p&MN_CPU_N);
})

MN_CPU_OP(50, {
    /* BVC */
    MN_CPU_RELATIVE(!(cpu->p&MN_CPU_V));
})

MN_CPU_OP(70, {
    /* BVS */
    MN_CPU_RELATIVE(cpu->p&MN_CPU_V);
})

MN_CPU_OP(90, {
    /* BCC */
    MN_CPU_RELATIVE(!(cpu->p&MN_CPU_C));
})

MN_CPU_OP(B0, {
    /* BCS */
    MN_CPU_RELATIVE(cpu->p&MN_CPU_C);
})

MN_CPU_OP(D0, {
    /* BNE */
    MN_CPU_RELATIVE(!(cpu->p&MN_CPU_Z));
})

MN_CPU_OP(F0, {
    /* BEQ */
    MN_CPU_RELATIVE(cpu->p&MN_CPU_Z);
})

/* Indexed indirect addressing */

/* Indexed indirect addressing - read instructions */

MN_CPU_OP(01, {
    /* ORA */
    MN_CPU_IDXIND_READ({
        cpu->a |= tmp;

        MN_CPU_UPDATE_NZ(cpu->a);
    });
})

MN_CPU_OP(21, {
    /* AND */
    MN_CPU_IDXIND_READ({
        cpu->a &= tmp;

        MN_CPU_UPDATE_NZ(cpu->a);
    });
})

MN_CPU_OP(41, {
    /* EOR */
    MN_CPU_IDXIND_READ({
        cpu->a ^= tmp;

        MN_CPU_UPDATE_NZ(cpu->a);
    });
})

MN_CPU_OP(61, {
    /* ADC */
    MN_CPU_IDXIND_READ({
        MN_CPU_ADC(tmp);
    });
})

MN_CPU_OP(A1, {
    /* LDA */
    MN_CPU_IDXIND_READ({
        cpu->a = tmp;

        MN_CPU_UPDATE_NZ(cpu->a);
    });
})

MN_CPU_OP(C1, {
    /* CMP */
    MN_CPU_IDXIND_READ({
        MN_CPU_CMP(cpu->a, tmp);
    });
})

MN_CPU_OP(E1, {
    /* SBC */
    MN_CPU_IDXIND_READ({
        MN_CPU_SBC(tmp);
    });
})

/* Indexed indirect addressing - Write instructions */

MN_CPU_OP(81, {
    /* STA */
    MN_CPU_IDXIND_STORE({
        tmp = cpu->a;
    });
})

/* Indirect indexed addressing */

/* Indirect indexed addressing - Read instructions */

MN_CPU_OP(11, {
    /* ORA */
    MN_CPU_INDIDX_READ({
        cpu->a |= tmp;

        MN_CPU_UPDATE_NZ(cpu->a);
    });
})

MN_CPU_OP(31, {
    /* AND */
    MN_CPU_INDIDX_READ({
        cpu->a &= tmp;

        MN_CPU_UPDATE_NZ(cpu->a);
    });
})

MN_CPU_OP(51, {
    /* EOR */
    MN_CPU_INDIDX_READ({
        cpu->a ^= tmp;

        MN_CPU_UPDATE_NZ(cpu->a);
    });
})

MN_CPU_OP(71, {
    /* ADC */
    MN_CPU_INDIDX_READ({
        MN_CPU_ADC(tmp);
    });
})

MN_CPU_OP(B1, {
    /* LDA */
    MN_CPU_INDIDX_READ({
        cpu->a = tmp;

        MN_CPU_UPDATE_NZ(cpu->a);
    });
})

MN_CPU_OP(D1, {
    /* CMP */
    MN_CPU_INDIDX_READ({
        MN_CPU_CMP(cpu->a, tmp);
    });
})

MN_CPU_OP(F1, {
    /* SBC */
    MN_CPU_INDIDX_READ({
        MN_CPU_SBC(tmp);
    });
})

/* Indirect indexed addressing - write instructions */

MN_CPU_OP(91, {
    /* STA */
    MN_CPU_INDIDX_STORE({
        tmp = cpu->a;
    });
})

/* Indirect absolute addressing */

MN_CPU_OP(6C, {
    /* JMP */
    switch(cpu->cycle){
        case 1:
            cpu->target_cycle = 5;
            break;
        case 2:
            cpu->pc++;
            break;
        case 3:
            cpu->tmp = MN_CPU_READ(cpu->pc)<<8;
            cpu->tmp |= cpu->t;
            cpu->pc++;
            break;
        case 4:
            cpu->t = MN_CPU_READ(cpu->tmp);
            break;
        case 5:
            cpu->pc = MN_CPU_READ((cpu->tmp&0xFF00)|
                                  ((cpu->tmp+1)&0xFF))<<8;
            cpu->pc |= cpu->t;
            break;
    }
})

/* Unofficial opcodes */

/* Implied addressing */

MN_CPU_OP(1A_NOP, {
    /* NOP */
    MN_CPU_IMP({
        /* Do nothing */
    });
})

/* Immediate addressing */

MN_CPU_OP(80_NOP, {
    /* NOP */
    MN_CPU_IMM({
        /* Do nothing */
    });
})

MN_CPU_OP(AB, {
    /* LAX */
    MN_CPU_IMM({
        cpu->a = cpu->t;
        cpu->x = cpu->a;

        MN_CPU_UPDATE_NZ(cpu->a);
    });
})

MN_CPU_OP(0B_2B, {
    /* ANC */
    MN_CPU_IMM({
        cpu->a &= cpu->t;

        MN_CPU_UPDATE_NZ(cpu->a);
        cpu->p &= ~MN_CPU_C;
        cpu->p |= (cpu->p>>7)&1;
    });
})

MN_CPU_OP(4B, {
    /* ALR */
    MN_CPU_IMM({
        cpu->a &= cpu->t;

        MN_CPU_UPDATE_NZ(cpu->a);
        MN_CPU_LSR(cpu->a);
    });
})

MN_CPU_OP(6B, {
    /* ARR */
    /* TODO: Fix this opcode, the overflow flag is incorrect after
     * execution */
    MN_CPU_IMM({
        cpu->a &= cpu->t;
        tmp = cpu->a;
        MN_CPU_ROR(cpu->a);

        cpu->p &= ~(1<<7);
        cpu->p |= cpu->a&(1<<7);

        if(cpu->a) cpu->p &= ~MN_CPU_Z;
        else cpu->p |= MN_CPU_Z;

        cpu->p &= ~MN_CPU_V;
        cpu->p |= (tmp^(cpu->a<<1))&(1<<6);
    });
})

MN_CPU_OP(8B, {
    /* XAA */
    /* TODO: Make it a bit broken to be closer to the expected
     * behaviour :D */
    MN_CPU_IMM({
        cpu->a = cpu->x;
        cpu->a &= cpu->t;
        MN_CPU_UPDATE_NZ(cpu->a);
    });
})

MN_CPU_OP(CB, {
    /* AXS */
    MN_CPU_IMM({
        MN_CPU_CMP(cpu->a&cpu->x, cpu->t);
        cpu->x = (cpu->a&cpu->x)-cpu->t;
        MN_CPU_UPDATE_NZ(cpu->x);
    });
})

/* Absolute addressing */

MN_CPU_OP(0C_3C, {
    /* NOP */
    MN_CPU_ABS_READ({
        /* Do nothing */
    });
})

MN_CPU_OP(AF, {
    /* LAX */
    MN_CPU_ABS_READ({
        /* XXX: Is LAX performing only one or two reads? */
        cpu->a = tmp;
        cpu->x = cpu->a;

        MN_CPU_UPDATE_NZ(cpu->a);
    });
})

MN_CPU_OP(8F, {
    /* SAX */
    /* NOTE: Apparently it is unstable on the NES */
    MN_CPU_ABS_STORE({
        tmp = cpu->a&cpu->x;
    });
})

MN_CPU_OP(0F, {
    /* SLO */
    MN_CPU_ABS_RMW({
        MN_CPU_ASL(cpu->t);
        cpu->a |= cpu->t;

        MN_CPU_UPDATE_NZ(cpu->a);
    });
})

MN_CPU_OP(2F, {
    /* RLA */
    MN_CPU_ABS_RMW({
        MN_CPU_ROL(cpu->t);
        cpu->a &= cpu->t;

        MN_CPU_UPDATE_NZ(cpu->a);
    });
})

MN_CPU_OP(4F, {
    /* SRE */
    MN_CPU_ABS_RMW({
        MN_CPU_LSR(cpu->t);
        cpu->a ^= cpu->t;

        MN_CPU_UPDATE_NZ(cpu->a);
    });
})

MN_CPU_OP(6F, {
    /* RRA */
    MN_CPU_ABS_RMW({
        MN_CPU_ROR(cpu->t);
        MN_CPU_ADC(cpu->t);
    });
})

MN_CPU_OP(CF, {
    /* DCP */
    MN_CPU_ABS_RMW({
        cpu->t--;
        MN_CPU_CMP(cpu->a, cpu->t);
    });
})

MN_CPU_OP(EF, {
    /* ISC */
    MN_CPU_ABS_RMW({
        cpu->t++;
        MN_CPU_SBC(cpu->t);
    });
})

/* Indexed absolute addressing */

/* With X */

MN_CPU_OP(1C_NOP, {
    /* NOP */
    MN_CPU_ABSI_RMW(cpu->x, {
        /* Do nothing */
    });
})

MN_CPU_OP(1F, {
    /* SLO */
    MN_CPU_ABSI_RMW(cpu->x, {
        MN_CPU_ASL(cpu->t);
        cpu->a |= cpu->t;

        MN_CPU_UPDATE_NZ(cpu->a);
    });
})

MN_CPU_OP(3F, {
    /* RLA */
    MN_CPU_ABSI_RMW(cpu->x, {
        MN_CPU_ROL(cpu->t);
        cpu->a &= cpu->t;

        MN_CPU_UPDATE_NZ(cpu->a);
    });
})

MN_CPU_OP(5F, {
    /* SRE */
    MN_CPU_ABSI_RMW(cpu->x, {
        MN_CPU_LSR(cpu->t);
        cpu->a ^= cpu->t;

        MN_CPU_UPDATE_NZ(cpu->a);
    });
})

MN_CPU_OP(7F, {
    /* RRA */
    MN_CPU_ABSI_RMW(cpu->x, {
        MN_CPU_ROR(cpu->t);
        MN_CPU_ADC(cpu->t);
    });
})

MN_CPU_OP(9C, {
    /* SHY */
    MN_CPU_ABSI_SH(cpu->x, {
        tmp = cpu->y;
        if(!cpu->skip_and) tmp &= ((cpu->tmp>>8)+1);
    });
})

MN_CPU_OP(DF, {
    /* DCP */
    MN_CPU_ABSI_RMW(cpu->x, {
        cpu->t--;
        MN_CPU_CMP(cpu->a, cpu->t);
    });
})

MN_CPU_OP(FF, {
    /* ISC */
    MN_CPU_ABSI_RMW(cpu->x, {
        cpu->t++;
        MN_CPU_SBC(cpu->t);
    });
})

/* With Y */

MN_CPU_OP(1B, {
    /* SLO */
    MN_CPU_ABSI_RMW(cpu->y, {
        MN_CPU_ASL(cpu->t);
        cpu->a |= cpu->t;

        MN_CPU_UPDATE_NZ(cpu->a);
    });
})

MN_CPU_OP(3B, {
    /* RLA */
    MN_CPU_ABSI_RMW(cpu->y, {
        MN_CPU_ROL(cpu->t);
        cpu->a &= cpu->t;

        MN_CPU_UPDATE_NZ(cpu->a);
    });
})

MN_CPU_OP(5B, {
    /* SRE */
    MN_CPU_ABSI_RMW(cpu->y, {
        MN_CPU_LSR(cpu->t);
        cpu->a ^= cpu->t;

        MN_CPU_UPDATE_NZ(cpu->a);
    });
})

MN_CPU_OP(7B, {
    /* RRA */
    MN_CPU_ABSI_RMW(cpu->y, {
        MN_CPU_ROR(cpu->t);
        MN_CPU_ADC(cpu->t);
    });
})

MN_CPU_OP(9B, {
    /* TAS */
    MN_CPU_ABSI_SH(cpu->y, {
        cpu->s = cpu->a&cpu->x;
        tmp = cpu->s;
        if(!cpu->skip_and) tmp &= ((cpu->tmp>>8)+1);
    });
})

MN_CPU_OP(9F, {
    /* AHX */
    MN_CPU_ABSI_SH(cpu->y, {
        tmp = cpu->a&cpu->x;
        if(!cpu->skip_and) tmp &= ((cpu->tmp>>8)+1);
    });
})

MN_CPU_OP(9E, {
    /* SHX */
    MN_CPU_ABSI_SH(cpu->y, {
        tmp = cpu->x;
        if(!cpu->skip_and) tmp &= ((cpu->tmp>>8)+1);
    });
})

MN_CPU_OP(BB, {
    /* LAS */
    MN_CPU_ABSI_READ(cpu->y, {
        cpu->a = tmp&cpu->s;
        cpu->x = cpu->a;
        cpu->s = cpu->a;

        MN_CPU_UPDATE_NZ(cpu->a);
    });
})

MN_CPU_OP(BF, {
    /* LAX */
    MN_CPU_ABSI_READ(cpu->y, {
        cpu->a = tmp;
        cpu->x = cpu->a;

        MN_CPU_UPDATE_NZ(cpu->a);
    });
})

MN_CPU_OP(DB, {
    /* DCP */
    MN_CPU_ABSI_RMW(cpu->y, {
        cpu->t--;
        MN_CPU_CMP(cpu->a, cpu->t);
    });
})

MN_CPU_OP(FB, {
    /* ISC */
    MN_CPU_ABSI_RMW(cpu->y, {
        cpu->t++;
        MN_CPU_SBC(cpu->t);
    });
})

/* Zeropage addressing */

MN_CPU_OP(04_NOP, {
    /* NOP */
    MN_CPU_ZP_READ({
        /* Do nothing */
    });
})

MN_CPU_OP(A7, {
    /* LAX */
    MN_CPU_ZP_READ({
        cpu->a = tmp;
        cpu->x = tmp;

        MN_CPU_UPDATE_NZ(cpu->a);
    });
})

MN_CPU_OP(87, {
    /* SAX */
    /* NOTE: Apparently it is unstable on the NES */
    MN_CPU_ZP_STORE({
        tmp = cpu->a&cpu->x;
    });
})

MN_CPU_OP(07, {
    /* SLO */
    MN_CPU_ZP_RMW({
        MN_CPU_ASL(cpu->t);
        cpu->a |= cpu->t;

        MN_CPU_UPDATE_NZ(cpu->a);
    });
})

MN_CPU_OP(27, {
    /* RLA */
    MN_CPU_ZP_RMW({
        MN_CPU_ROL(cpu->t);
        cpu->a &= cpu->t;

        MN_CPU_UPDATE_NZ(cpu->a);
    });
})

MN_CPU_OP(47, {
    /* SRE */
    MN_CPU_ZP_RMW({
        MN_CPU_LSR(cpu->t);
        cpu->a ^= cpu->t;

        MN_CPU_UPDATE_NZ(cpu->a);
    });
})

MN_CPU_OP(67, {
    /* RRA */
    MN_CPU_ZP_RMW({
        MN_CPU_ROR(cpu->t);
        MN_CPU_ADC(cpu->t);
    });
})

MN_CPU_OP(C7, {
    /* DCP */
    MN_CPU_ZP_RMW({
        cpu->t--;
        MN_CPU_CMP(cpu->a, cpu->t);
    });
})

MN_CPU_OP(E7, {
    /* ISC */
    MN_CPU_ZP_RMW({
        cpu->t++;
        MN_CPU_SBC(cpu->t);
    });
})

/* Indexed zeropage addressing */

/* With X */

MN_CPU_OP(14_NOP, {
    /* NOP */
    MN_CPU_ZPI_READ(cpu->x, {
        /* Do nothing */
    });
})

MN_CPU_OP(17, {
    /* SLO */
    MN_CPU_ZPI_RMW({
        MN_CPU_ASL(cpu->t);
        cpu->a |= cpu->t;

        MN_CPU_UPDATE_NZ(cpu->a);
    });
})

MN_CPU_OP(37, {
    /* RLA */
    MN_CPU_ZPI_RMW({
        MN_CPU_ROL(cpu->t);
        cpu->a &= cpu->t;

        MN_CPU_UPDATE_NZ(cpu->a);
    });
})

MN_CPU_OP(57, {
    /* SRE */
    MN_CPU_ZPI_RMW({
        MN_CPU_LSR(cpu->t);
        cpu->a ^= cpu->t;

        MN_CPU_UPDATE_NZ(cpu->a);
    });
})

MN_CPU_OP(77, {
    /* RRA */
    MN_CPU_ZPI_RMW({
        MN_CPU_ROR(cpu->t);
        MN_CPU_ADC(cpu->t);
    });
})

MN_CPU_OP(D7, {
    /* DCP */
    MN_CPU_ZPI_RMW({
        cpu->t--;
        MN_CPU_CMP(cpu->a, cpu->t);
    });
})

MN_CPU_OP(F7, {
    /* ISC */
    MN_CPU_ZPI_RMW({
        cpu->t++;
        MN_CPU_SBC(cpu->t);
    });
})

/* With Y */

MN_CPU_OP(97, {
    /* SAX */
    /* NOTE: Apparently it is unstable on the NES */
    MN_CPU_ZPI_STORE(cpu->y, {
        tmp = cpu->a&cpu->x;
    });
})

MN_CPU_OP(B7, {
    /* LAX */
    MN_CPU_ZPI_READ(cpu->y, {
        cpu->a = tmp;
        cpu->x = cpu->a;

        MN_CPU_UPDATE_NZ(cpu->a);
    });
})

/* Indexed indirect addressing */

MN_CPU_OP(03, {
    /* SLO */
    MN_CPU_IDXIND_RMW({
        MN_CPU_ASL(cpu->t);
        cpu->a |= cpu->t;

        MN_CPU_UPDATE_NZ(cpu->a);
    });
})

MN_CPU_OP(23, {
    /* RLA */
    MN_CPU_IDXIND_RMW({
        MN_CPU_ROL(cpu->t);
        cpu->a &= cpu->t;

        MN_CPU_UPDATE_NZ(cpu->a);
    });
})

MN_CPU_OP(43, {
    /* SRE */
    MN_CPU_IDXIND_RMW({
        MN_CPU_LSR(cpu->t);
        cpu->a ^= cpu->t;

        MN_CPU_UPDATE_NZ(cpu->a);
    });
})

MN_CPU_OP(63, {
    /* RRA */
    MN_CPU_IDXIND_RMW({
        MN_CPU_ROR(cpu->t);
        MN_CPU_ADC(cpu->t);
    });
})

MN_CPU_OP(83, {
    /* SAX */
    /* NOTE: Apparently it is unstable on the NES */
    MN_CPU_IDXIND_STORE({
        tmp = cpu->a&cpu->x;
    });
})

MN_CPU_OP(A3, {
    /* LAX */
    MN_CPU_IDXIND_READ({
        cpu->a = tmp;
        cpu->x = cpu->a;

        MN_CPU_UPDATE_NZ(cpu->a);
    });
})

MN_CPU_OP(C3, {
    /* DCP */
    MN_CPU_IDXIND_RMW({
        cpu->t--;
        MN_CPU_CMP(cpu->a, cpu->t);
    });
})

MN_CPU_OP(E3, {
    /* ISC */
    MN_CPU_IDXIND_RMW({
        cpu->t++;
        MN_CPU_SBC(cpu->t);
    });
})

/* Indirect indexed addressing */

MN_CPU_OP(13, {
    /* SLO */
    MN_CPU_INDIDX_RMW({
        MN_CPU_ASL(cpu->t);
        cpu->a |= cpu->t;

        MN_CPU_UPDATE_NZ(cpu->a);
    });
})

MN_CPU_OP(33, {
    /* RLA */
    MN_CPU_INDIDX_RMW({
        MN_CPU_ROL(cpu->t);
        cpu->a &= cpu->t;

        MN_CPU_UPDATE_NZ(cpu->a);
    });
})

MN_CPU_OP(53, {
    /* SRE */
    MN_CPU_INDIDX_RMW({
        MN_CPU_LSR(cpu->t);
        cpu->a ^= cpu->t;

        MN_CPU_UPDATE_NZ(cpu->a);
    });
})

MN_CPU_OP(73, {
    /* RRA */
    MN_CPU_INDIDX_RMW({
        MN_CPU_ROR(cpu->t);
        MN_CPU_ADC(cpu->t);
    });
})

MN_CPU_OP(93, {
    /* AHX */
    MN_CPU_INDIDX_SH({
        tmp = cpu->a&cpu->x;
        if(!cpu->skip_and) tmp &= ((cpu->tmp>>8)+1);
    });
})

MN_CPU_OP(B3, {
    /* LAX */
    MN_CPU_INDIDX_READ({
        cpu->a = tmp;
        cpu->x = cpu->a;

        MN_CPU_UPDATE_NZ(cpu->a);
    });
})

MN_CPU_OP(D3, {
    /* DCP */
    MN_CPU_INDIDX_RMW({
        cpu->t--;
        MN_CPU_CMP(cpu->a, cpu->t);
    });
})

MN_CPU_OP(F3, {
    /* ISC */
    MN_CPU_INDIDX_RMW({
        cpu->t++;
        MN_CPU_SBC(cpu->t);
    });
})

/* STP */

MN_CPU_OP(X2, {
    /* STP */
    /* TODO: Check if I'm emulating it accurately */
    cpu->jammed = 1;
})

static void (*opcode_lut[256])(MNCPU *cpu, MNEmu *emu) = {
    mn_cpu_opcode_00,
    mn_cpu_opcode_01,
    mn_cpu_opcode_X2,
    mn_cpu_opcode_03,
    mn_cpu_opcode_04_NOP,
    mn_cpu_opcode_05,
    mn_cpu_opcode_06,
    mn_cpu_opcode_07,
    mn_cpu_opcode_08,
    mn_cpu_opcode_09,
    mn_cpu_opcode_0A,
    mn_cpu_opcode_0B_2B,
    mn_cpu_opcode_0C_3C,
    mn_cpu_opcode_0D,
    mn_cpu_opcode_0E,
    mn_cpu_opcode_0F,
    mn_cpu_opcode_10,
    mn_cpu_opcode_11,
    mn_cpu_opcode_X2,
    mn_cpu_opcode_13,
    mn_cpu_opcode_14_NOP,
    mn_cpu_opcode_15,
    mn_cpu_opcode_16,
    mn_cpu_opcode_17,
    mn_cpu_opcode_18,
    mn_cpu_opcode_19_1D,
    mn_cpu_opcode_1A_NOP,
    mn_cpu_opcode_1B,
    mn_cpu_opcode_1C_NOP,
    mn_cpu_opcode_19_1D,
    mn_cpu_opcode_1E,
    mn_cpu_opcode_1F,
    mn_cpu_opcode_20,
    mn_cpu_opcode_21,
    mn_cpu_opcode_X2,
    mn_cpu_opcode_23,
    mn_cpu_opcode_24,
    mn_cpu_opcode_25,
    mn_cpu_opcode_26,
    mn_cpu_opcode_27,
    mn_cpu_opcode_28,
    mn_cpu_opcode_29,
    mn_cpu_opcode_2A,
    mn_cpu_opcode_0B_2B,
    mn_cpu_opcode_2C,
    mn_cpu_opcode_2D,
    mn_cpu_opcode_2E,
    mn_cpu_opcode_2F,
    mn_cpu_opcode_30,
    mn_cpu_opcode_31,
    mn_cpu_opcode_X2,
    mn_cpu_opcode_33,
    mn_cpu_opcode_14_NOP,
    mn_cpu_opcode_35,
    mn_cpu_opcode_36,
    mn_cpu_opcode_37,
    mn_cpu_opcode_38,
    mn_cpu_opcode_39_3D,
    mn_cpu_opcode_1A_NOP,
    mn_cpu_opcode_3B,
    mn_cpu_opcode_0C_3C,
    mn_cpu_opcode_39_3D,
    mn_cpu_opcode_3E,
    mn_cpu_opcode_3F,
    mn_cpu_opcode_40,
    mn_cpu_opcode_41,
    mn_cpu_opcode_X2,
    mn_cpu_opcode_43,
    mn_cpu_opcode_04_NOP,
    mn_cpu_opcode_45,
    mn_cpu_opcode_46,
    mn_cpu_opcode_47,
    mn_cpu_opcode_48,
    mn_cpu_opcode_49,
    mn_cpu_opcode_4A,
    mn_cpu_opcode_4B,
    mn_cpu_opcode_4C,
    mn_cpu_opcode_4D,
    mn_cpu_opcode_4E,
    mn_cpu_opcode_4F,
    mn_cpu_opcode_50,
    mn_cpu_opcode_51,
    mn_cpu_opcode_X2,
    mn_cpu_opcode_53,
    mn_cpu_opcode_14_NOP,
    mn_cpu_opcode_55,
    mn_cpu_opcode_56,
    mn_cpu_opcode_57,
    mn_cpu_opcode_58,
    mn_cpu_opcode_59_5D,
    mn_cpu_opcode_1A_NOP,
    mn_cpu_opcode_5B,
    mn_cpu_opcode_1C_NOP,
    mn_cpu_opcode_59_5D,
    mn_cpu_opcode_5E,
    mn_cpu_opcode_5F,
    mn_cpu_opcode_60,
    mn_cpu_opcode_61,
    mn_cpu_opcode_X2,
    mn_cpu_opcode_63,
    mn_cpu_opcode_04_NOP,
    mn_cpu_opcode_65,
    mn_cpu_opcode_66,
    mn_cpu_opcode_67,
    mn_cpu_opcode_68,
    mn_cpu_opcode_69,
    mn_cpu_opcode_6A,
    mn_cpu_opcode_6B,
    mn_cpu_opcode_6C,
    mn_cpu_opcode_6D,
    mn_cpu_opcode_6E,
    mn_cpu_opcode_6F,
    mn_cpu_opcode_70,
    mn_cpu_opcode_71,
    mn_cpu_opcode_X2,
    mn_cpu_opcode_73,
    mn_cpu_opcode_14_NOP,
    mn_cpu_opcode_75,
    mn_cpu_opcode_76,
    mn_cpu_opcode_77,
    mn_cpu_opcode_78,
    mn_cpu_opcode_79_7D,
    mn_cpu_opcode_1A_NOP,
    mn_cpu_opcode_7B,
    mn_cpu_opcode_1C_NOP,
    mn_cpu_opcode_79_7D,
    mn_cpu_opcode_7E,
    mn_cpu_opcode_7F,
    mn_cpu_opcode_80_NOP,
    mn_cpu_opcode_81,
    mn_cpu_opcode_80_NOP,
    mn_cpu_opcode_83,
    mn_cpu_opcode_84,
    mn_cpu_opcode_85,
    mn_cpu_opcode_86,
    mn_cpu_opcode_87,
    mn_cpu_opcode_88,
    mn_cpu_opcode_80_NOP,
    mn_cpu_opcode_8A,
    mn_cpu_opcode_8B,
    mn_cpu_opcode_8C,
    mn_cpu_opcode_8D,
    mn_cpu_opcode_8E,
    mn_cpu_opcode_8F,
    mn_cpu_opcode_90,
    mn_cpu_opcode_91,
    mn_cpu_opcode_X2,
    mn_cpu_opcode_93,
    mn_cpu_opcode_94,
    mn_cpu_opcode_95,
    mn_cpu_opcode_96,
    mn_cpu_opcode_97,
    mn_cpu_opcode_98,
    mn_cpu_opcode_99_9D,
    mn_cpu_opcode_9A,
    mn_cpu_opcode_9B,
    mn_cpu_opcode_9C,
    mn_cpu_opcode_99_9D,
    mn_cpu_opcode_9E,
    mn_cpu_opcode_9F,
    mn_cpu_opcode_A0,
    mn_cpu_opcode_A1,
    mn_cpu_opcode_A2,
    mn_cpu_opcode_A3,
    mn_cpu_opcode_A4,
    mn_cpu_opcode_A5,
    mn_cpu_opcode_A6,
    mn_cpu_opcode_A7,
    mn_cpu_opcode_A8,
    mn_cpu_opcode_A9,
    mn_cpu_opcode_AA,
    mn_cpu_opcode_AB,
    mn_cpu_opcode_AC,
    mn_cpu_opcode_AD,
    mn_cpu_opcode_AE,
    mn_cpu_opcode_AF,
    mn_cpu_opcode_B0,
    mn_cpu_opcode_B1,
    mn_cpu_opcode_X2,
    mn_cpu_opcode_B3,
    mn_cpu_opcode_B4,
    mn_cpu_opcode_B5,
    mn_cpu_opcode_B6,
    mn_cpu_opcode_B7,
    mn_cpu_opcode_B8,
    mn_cpu_opcode_B9_BD,
    mn_cpu_opcode_BA,
    mn_cpu_opcode_BB,
    mn_cpu_opcode_BC,
    mn_cpu_opcode_B9_BD,
    mn_cpu_opcode_BE,
    mn_cpu_opcode_BF,
    mn_cpu_opcode_C0,
    mn_cpu_opcode_C1,
    mn_cpu_opcode_80_NOP,
    mn_cpu_opcode_C3,
    mn_cpu_opcode_C4,
    mn_cpu_opcode_C5,
    mn_cpu_opcode_C6,
    mn_cpu_opcode_C7,
    mn_cpu_opcode_C8,
    mn_cpu_opcode_C9,
    mn_cpu_opcode_CA,
    mn_cpu_opcode_CB,
    mn_cpu_opcode_CC,
    mn_cpu_opcode_CD,
    mn_cpu_opcode_CE,
    mn_cpu_opcode_CF,
    mn_cpu_opcode_D0,
    mn_cpu_opcode_D1,
    mn_cpu_opcode_X2,
    mn_cpu_opcode_D3,
    mn_cpu_opcode_14_NOP,
    mn_cpu_opcode_D5,
    mn_cpu_opcode_D6,
    mn_cpu_opcode_D7,
    mn_cpu_opcode_D8,
    mn_cpu_opcode_D9_DD,
    mn_cpu_opcode_1A_NOP,
    mn_cpu_opcode_DB,
    mn_cpu_opcode_1C_NOP,
    mn_cpu_opcode_D9_DD,
    mn_cpu_opcode_DE,
    mn_cpu_opcode_DF,
    mn_cpu_opcode_E0,
    mn_cpu_opcode_E1,
    mn_cpu_opcode_80_NOP,
    mn_cpu_opcode_E3,
    mn_cpu_opcode_E4,
    mn_cpu_opcode_E5,
    mn_cpu_opcode_E6,
    mn_cpu_opcode_E7,
    mn_cpu_opcode_E8,
    mn_cpu_opcode_E9_EB,
    mn_cpu_opcode_EA,
    mn_cpu_opcode_E9_EB,
    mn_cpu_opcode_EC,
    mn_cpu_opcode_ED,
    mn_cpu_opcode_EE,
    mn_cpu_opcode_EF,
    mn_cpu_opcode_F0,
    mn_cpu_opcode_F1,
    mn_cpu_opcode_X2,
    mn_cpu_opcode_F3,
    mn_cpu_opcode_14_NOP,
    mn_cpu_opcode_F5,
    mn_cpu_opcode_F6,
    mn_cpu_opcode_F7,
    mn_cpu_opcode_F8,
    mn_cpu_opcode_F9_FD,
    mn_cpu_opcode_1A_NOP,
    mn_cpu_opcode_FB,
    mn_cpu_opcode_1C_NOP,
    mn_cpu_opcode_F9_FD,
    mn_cpu_opcode_FE,
    mn_cpu_opcode_FF
};

void mn_cpu_cycle(MNCPU *cpu, MNEmu *emu) {
    /* Emulated the 6502 as described at https://www.nesdev.org/6502_cpu.txt
     *
     * I also used:
     * https://www.nesdev.org/wiki/CPU_unofficial_opcodes
     * http://www.6502.org/users/obelisk/6502/reference.html
     * https://www.oxyron.de/html/opcodes02.html
     * https://www.nesdev.org/wiki/Instruction_reference#ADC
     */

    if(cpu->jammed) return;
    if(cpu->halted){
        MN_CPU_READ(cpu->last_read);

        /* XXX: Is this accurate? */
        if(cpu->rdy) cpu->halted = 0;
        return;
    }

    /* The internal signals are raised during phi 1 of each cycle */
    if(cpu->nmi_detected) cpu->should_nmi = 1;
    if(cpu->irq_detected) cpu->should_irq = 1;

    if(cpu->cycle == 2){
        cpu->t = MN_CPU_READ(cpu->pc);
    }else if(cpu->cycle > cpu->target_cycle){
        cpu->opcode = MN_CPU_READ(cpu->pc);
OPCODE_LOADED:
#if MN_CPU_DEBUG && !MN_CPU_CYCLE_DETAIL
        MN_CPU_OP_INFO();
#endif
        cpu->cycle = 1;
        cpu->target_cycle = 2;
        if(cpu->execute_int_next){
            cpu->execute_int = 1;
            cpu->execute_int_next = 0;
#if MN_CPU_DEBUG
            puts("INT");
#endif
        }else{
            cpu->pc++;
        }
    }

    if(cpu->execute_int){
        switch(cpu->cycle){
            case 1:
                cpu->target_cycle = 7;
                /* BRK is forced into the opcode register */
                cpu->opcode = 0x00;
                break;
            case 2:
                /* NOTE: PC increment is not performed on interrupt, only on
                 * BRK */
                break;
            case 3:
                MN_CPU_WRITE(0x0100+cpu->s, cpu->pc>>8);
                cpu->s--;
                break;
            case 4:
                MN_CPU_WRITE(0x0100+cpu->s, cpu->pc);
                cpu->s--;
                cpu->is_irq = 1;
                if(cpu->should_nmi){
                    cpu->is_irq = 0;
                    cpu->should_nmi = 0;
                }
                break;
            case 5:
                MN_CPU_WRITE(0x0100+cpu->s, cpu->p);
                cpu->s--;
                break;
            case 6:
                cpu->pc &= 0xFF00;
                cpu->pc |= MN_CPU_READ(cpu->is_irq ? 0xFFFE : 0xFFFA);
                break;
            case 7:
                cpu->pc &= 0xFF;
                cpu->pc |= MN_CPU_READ(cpu->is_irq ? 0xFFFF : 0xFFFB)<<8;
                if(!cpu->is_irq) cpu->should_nmi = 0;
                cpu->execute_int = 0;
                break;
        }
        cpu->cycle++;
        return;
    }

    /* TODO: Avoid having so much duplicated code. */
    opcode_lut[cpu->opcode](cpu, emu);

    if(cpu->opcode_loaded){
        cpu->opcode_loaded = 0;
        goto OPCODE_LOADED;
    }

#if MN_CPU_DEBUG && MN_CPU_CYCLE_DETAIL
    printf("c: %d ", cpu->cycle);
    MN_CPU_OP_INFO();
#endif

    if((cpu->opcode&31) != 16 && cpu->opcode &&
       cpu->cycle == cpu->target_cycle-1){
        MN_CPU_INTPOLL();
    }

    /* The edge detector and level detector polling is performed on phi 2 of
     * each cycle */
    cpu->nmi_detected = (cpu->nmi_pin^cpu->nmi_pin_last) && !cpu->nmi_pin;
    cpu->irq_detected = !cpu->irq_pin;

    /* The internal signal when an IRQ is detected is only high for that
     * cycle */
    cpu->should_irq = 0;

    cpu->cycle++;

    cpu->nmi_pin_last = cpu->nmi_pin;
}

void mn_cpu_free(MNCPU *cpu) {
    /* TODO */
    (void)cpu;
}
