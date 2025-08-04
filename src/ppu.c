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

    ppu->keep_vblank_clear = 0;

    return 0;
}

#define MN_PPU_COARSE_X_INC() \
    { \
        register unsigned char x = ((ppu->v&((1<<6)-1))+1); \
        ppu->v = ppu->v&~((1<<6)-1); \
        ppu->v |= x&((1<<6)-1); \
        /* Switch nametable */ \
        if(x&(1<<6)) ppu->v ^= 0x400; \
    }

#define MN_PPU_Y_INC() \
    { \
        /* See https://www.nesdev.org/wiki/PPU_scrolling#Y_increment */ \
        if((ppu->v&(7<<12)) != (7<<12)){ \
            ppu->v += 0x7000; \
        }else{ \
            ppu->v &= ~(7<<12); \
            if((ppu->v&(31<<5)) == (29<<5)){ \
                ppu->v &= ~(31<<5); \
                ppu->v ^= 0x800; \
            }else if((ppu->v&(31<<5)) == (31<<5)){ \
                ppu->v &= ~(31<<5); \
            }else{ \
                ppu->v += 1<<5; \
            } \
        } \
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
    unsigned char color;
    unsigned char palette;
    unsigned char bg_pixel;
    unsigned char idx;

    if(ppu->scanline <= 239 || ppu->scanline == 261){
        if((ppu->mask&MN_PPU_MASK_BACKGROUND) ||
           (ppu->mask&MN_PPU_MASK_SPRITES)){
            /* Visible scanlines or pre-render scanline */
            if(ppu->cycle >= 1 && ppu->cycle <= 256){
                if(!((ppu->cycle-1)&3) && ppu->cycle > 1){
                    /* Reload the shift registers */

                    /* The bitplanes go into the high 8-bit of two 16-bit shift
                     * registers. */
                    ppu->low_shift &= 0xFF;
                    ppu->high_shift &= 0xFF;
                    ppu->low_shift |= ppu->low_bp<<8;
                    ppu->high_shift |= ppu->high_bp<<8;

                    ppu->attr_latch1 = ((ppu->attr>>(((ppu->v>>5)&1)<<2))+
                                        (ppu->v&1))&1;
                    ppu->attr_latch2 = (((ppu->attr>>(((ppu->v>>5)&1)<<2))+
                                        (ppu->v&1))<<1)&1;

                    /* Increment coarse X in v */
                    MN_PPU_COARSE_X_INC();
                }

                if(ppu->scanline != 261){
                    /* Produce a background pixel */
                    color = ((ppu->low_shift>>ppu->x)&1)|(((ppu->high_shift>>
                             ppu->x)&1)<<1);
                    palette = (ppu->attr1_shift&1)|((ppu->attr2_shift&1)<<1);
                    idx = emu->mapper.vram_read(emu, &emu->mapper,
                                                0x3F00+palette*4+color);

                    if(!color){
                        bg_pixel = 0;
                    }else{
                        bg_pixel = 0;
                    }

                    bg_pixel |= idx;

#if 0
                    printf("%u %u %02x %02x %02x %02x\n", ppu->scanline,
                           ppu->cycle, color, palette, idx, bg_pixel);
#endif

                    /* The upper two bits of the color are not stored */
                    bg_pixel &= 0x3F;

                    ppu->draw_pixel((ppu->palette[bg_pixel*3]<<16)|
                                    (ppu->palette[bg_pixel*3+1]<<8)|
                                    ppu->palette[bg_pixel*3+2]);
                }

                /* Shift the shift registers */
                ppu->low_shift <<= 1;
                ppu->low_shift |= 1;
                ppu->high_shift <<= 1;
                ppu->high_shift |= 1;

                ppu->attr1_shift <<= 1;
                ppu->attr1_shift |= ppu->attr_latch1;

                ppu->attr2_shift <<= 1;
                ppu->attr2_shift |= ppu->attr_latch2;

                switch((ppu->cycle-1)&3){
                    case 0:
                        ppu->addr = 0x2000|(ppu->v&0x0FFF);
                        ppu->video_mem_bus = ppu->addr;
                        break;
                    case 1:
                        ppu->tile_id = (ppu->video_mem_bus = emu->mapper.
                                        vram_read(emu, &emu->mapper,
                                                  ppu->addr));
                        break;
                    case 2:
                        /* Address calculated as described at
                         * https://www.nesdev.org/wiki/PPU_scrolling#Tile_and_a
                         * ttribute_fetching */
                        ppu->addr = (0x2000+32*30)|(ppu->v&0x0C00)|
                                    ((ppu->v>>4)&0x38)|((ppu->v>>2)&7);
                        ppu->video_mem_bus = ppu->addr;
                        break;
                    case 3:
                        ppu->attr = (ppu->video_mem_bus = emu->mapper.
                                     vram_read(emu, &emu->mapper, ppu->addr));
                        break;
                    case 4:
                        ppu->addr = ppu->tile_id*16+(ppu->v>>12);
                        ppu->video_mem_bus = ppu->addr;
                        break;
                    case 5:
                        ppu->low_bp = (ppu->video_mem_bus = emu->mapper.
                                       vram_read(emu, &emu->mapper,
                                                 ppu->addr));
                        break;
                    case 6:
                        ppu->addr = ppu->tile_id*16+8+(ppu->v>>12);
                        ppu->video_mem_bus = ppu->addr;
                        break;
                    case 7:
                        ppu->high_bp = (ppu->video_mem_bus = emu->mapper.
                                        vram_read(emu, &emu->mapper,
                                                  ppu->addr));
                        break;
                }
            }else if(ppu->cycle >= 321 && ppu->cycle <= 336){
                if(ppu->cycle == 321+8){
                    /* XXX: Is this correct? */
                    MN_PPU_COARSE_X_INC();
                }
                switch((ppu->cycle-321)&3){
                    case 0:
                        ppu->addr = 0x2000|(ppu->v&0x0FFF);
                        ppu->video_mem_bus = ppu->addr;
                        break;
                    case 1:
                        ppu->tile_id = (ppu->video_mem_bus = emu->mapper.
                                        vram_read(emu, &emu->mapper,
                                                  ppu->addr));
                        break;
                    case 2:
                        /* Address calculated as described at
                         * https://www.nesdev.org/wiki/PPU_scrolling#Tile_and_a
                         * ttribute_fetching */
                        ppu->addr = (0x2000+32*30)|(ppu->v&0x0C00)|
                                    ((ppu->v>>4)&0x38)|((ppu->v>>2)&7);
                        ppu->video_mem_bus = ppu->addr;
                        break;
                    case 3:
                        ppu->attr = (ppu->video_mem_bus = emu->mapper.
                                     vram_read(emu, &emu->mapper, ppu->addr));
                        break;
                    case 4:
                        ppu->addr = ppu->tile_id*16+(ppu->v>>12);
                        ppu->video_mem_bus = ppu->addr;
                        break;
                    case 5:
                        ppu->low_bp = (ppu->video_mem_bus = emu->mapper.
                                       vram_read(emu, &emu->mapper,
                                                 ppu->addr));
                        break;
                    case 6:
                        ppu->addr = ppu->tile_id*16+8+(ppu->v>>12);
                        ppu->video_mem_bus = ppu->addr;
                        break;
                    case 7:
                        ppu->high_bp = (ppu->video_mem_bus = emu->mapper.
                                        vram_read(emu, &emu->mapper,
                                                  ppu->addr));
                        break;
                }
            }else if(ppu->cycle >= 337){
                switch((ppu->cycle-321)&3){
                    case 0:
                        ppu->addr = 0x2000|(ppu->v&0x0FFF);
                        ppu->video_mem_bus = ppu->addr;
                        break;
                    case 1:
                        ppu->tile_id = (ppu->video_mem_bus = emu->mapper.
                                        vram_read(emu, &emu->mapper,
                                                  ppu->addr));
                        break;
                }
            }

            if(ppu->scanline == 261){
                if(ppu->cycle == 1){
                    ppu->vblank = 0;
                    ppu->sprite0_hit = 0;
                    ppu->sprite_overflow = 0;
                    cpu->nmi_pin = 0;
                }
                if(ppu->cycle >= 280 && ppu->cycle <= 304){
                    /* The PPU repeatedly copies these bits in these cycles of
                     * the pre-render scanline. */
                    ppu->v &= ~((((1<<4)-1)<<11)|((1<<5)-1)<<5);
                    ppu->v |= cpu->t&((((1<<4)-1)<<11)|((1<<5)-1)<<5);
                }
            }
            if(ppu->cycle == 0){
                /* Cycle 0 is an IDLE cycle */
            }else if(ppu->cycle <= 256){
                if(ppu->cycle == 256){
                    /* Increment Y */
                    MN_PPU_Y_INC();
                }
            }else if(ppu->cycle == 257){
                ppu->v &= ~(((1<<5)-1)|(1<<10));
                ppu->v |= ppu->t&(((1<<5)-1)|(1<<10));
            }
        }else{
            if(ppu->cycle >= 1 && ppu->cycle <= 256){
                if(ppu->scanline != 261){
                    /* Produce a background pixel */
                    bg_pixel = (ppu->video_mem_bus = emu->mapper.
                                        vram_read(emu, &emu->mapper,
                                                  ppu->v));

                    bg_pixel &= 0x3F;

                    ppu->draw_pixel((ppu->palette[bg_pixel*3]<<16)|
                                    (ppu->palette[bg_pixel*3+1]<<8)|
                                    ppu->palette[bg_pixel*3+2]);
                }
            }
        }
    }else if(ppu->scanline == 240){
        /* Post-render scanline */
    }else if(ppu->scanline <= 260){
        if(ppu->scanline == 241 && ppu->cycle == 1){
            /* Set the VBlank flag and trigger NMI */
            if(!ppu->keep_vblank_clear) ppu->vblank = 1;
            ppu->keep_vblank_clear = 0;
            cpu->nmi_pin = 1;
        }
        /* Vertical blanking lines */
    }

    ppu->cycle++;
    if(ppu->cycle > 341){
        ppu->cycle = 0;
        ppu->scanline++;
        if(ppu->scanline > 261) ppu->scanline = 0;
    }

    if(ppu->since_start < ppu->startup_time){
        ppu->since_start++;
    }

    /* Let the CPU run all 3 PPU cycles */
    if(ppu->cycles_since_cpu_cycle >= 3){
        mn_cpu_cycle(&emu->cpu, emu);
        ppu->cycles_since_cpu_cycle = 0;
    }

    ppu->cycles_since_cpu_cycle++;
}

unsigned char mn_ppu_read(MNPPU *ppu, MNEmu *emu, unsigned short int reg) {
    unsigned char v;

    switch(reg){
        case MN_PPU_CTRL:
            break;
        case MN_PPU_MASK:
            break;
        case MN_PPU_STATUS:
            ppu->io_bus &= (1<<5)-1;
            ppu->io_bus |= (ppu->vblank<<7)|(ppu->sprite0_hit<<6)|
                           (ppu->sprite_overflow<<5);
            ppu->vblank = 0;
            if(ppu->scanline == 241 && ppu->cycle == 0){
                ppu->keep_vblank_clear = 1;
            }
            ppu->w = 0;
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
            /* XXX: Is this correct */
            v = ppu->io_bus;
            ppu->io_bus = emu->mapper.vram_read(emu, &emu->mapper,
                                                ppu->v&((1<<15)-1));
            if((ppu->scanline < 240 || ppu->scanline == 261) &&
               ((ppu->mask&MN_PPU_MASK_BACKGROUND) ||
                (ppu->mask&MN_PPU_MASK_SPRITES))){
                /* The PPU is rendering */
                /* Perform coarse X increment and Y increment + trigger a load
                 * next value */
                MN_PPU_COARSE_X_INC();
                MN_PPU_Y_INC();
                /* XXX: Is this a "load next value" as written in the wiki? */
                ppu->io_bus = emu->mapper.vram_read(emu, &emu->mapper,
                                                    ppu->v&((1<<15)-1));
            }else{
                ppu->v += ppu->ctrl&MN_PPU_CTRL_INC ? 32 : 1;
            }
            return v;
            break;
    }
    return ppu->io_bus;
}

void mn_ppu_write(MNPPU *ppu, MNEmu *emu, unsigned short int reg,
                  unsigned char value) {
    ppu->io_bus = value;

    switch(reg){
        case MN_PPU_CTRL:
            if(ppu->since_start < ppu->startup_time) break;
            ppu->t &= (1<<10)|(1<<11);
            ppu->t |= (value&3)<<10;
            ppu->ctrl = value;
            break;
        case MN_PPU_MASK:
            if(ppu->since_start < ppu->startup_time) break;
            /* TODO: Only toggle rendering after 3-4 dots */
            /* TODO: Take the bugs described at https://www.nesdev.org/wiki/PPU
             * _registers#Rendering_control into account. */
            ppu->mask = value;
            break;
        case MN_PPU_STATUS:
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
                printf("t4: %04x\n", ppu->t);
                ppu->t |= value;
                printf("t5: %04x\n", ppu->t);
                ppu->v = ppu->t;
                printf("v: %04x\n", ppu->v);
                ppu->w = 0;
            }else{
                printf("t1: %04x\n", ppu->t);
                ppu->t &= 0xFF;
                printf("t2: %04x\n", ppu->t);
                ppu->t |= (value&((1<<6)-1))<<8;
                printf("t3: %04x\n", ppu->t);
                ppu->w = 1;
            }
            break;
        case MN_PPU_PPUDATA:
#if 0
            printf("%04x %02x\n", ppu->v&((1<<15)-1), value);
#endif
            emu->mapper.vram_write(emu, &emu->mapper, ppu->v&((1<<15)-1),
                                   value);
            if((ppu->scanline < 240 || ppu->scanline == 261) &&
               ((ppu->mask&MN_PPU_MASK_BACKGROUND) ||
                (ppu->mask&MN_PPU_MASK_SPRITES))){
                /* The PPU is rendering */
                /* Perform coarse X increment and Y increment + trigger a load
                 * next value */
                MN_PPU_COARSE_X_INC();
                MN_PPU_Y_INC();
                /* XXX: Is this a "load next value" as written in the wiki? */
                ppu->io_bus = emu->mapper.vram_read(emu, &emu->mapper,
                                                    ppu->v&((1<<15)-1));
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
