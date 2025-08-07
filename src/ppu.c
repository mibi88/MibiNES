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
#include <dma.h>

#include <stdio.h>

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

#define MN_PPU_BG_COARSE_X_INC() \
    { \
        register unsigned char x = ((ppu->v&((1<<6)-1))+1); \
        ppu->v = ppu->v&~((1<<6)-1); \
        ppu->v |= x&((1<<6)-1); \
        /* Switch nametable */ \
        if(x&(1<<6)) ppu->v ^= 0x400; \
    }

#if 0
#define MN_PPU_BG_Y_INC() \
    { \
        /* See https://www.nesdev.org/wiki/PPU_scrolling#Y_increment */ \
        if((ppu->v&(7<<12)) != (7<<12)){ \
            ppu->v += (1<<12); \
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
#else

#define MN_PPU_BG_Y_INC() \
    { \
        /* See https://www.nesdev.org/wiki/PPU_scrolling#Y_increment
         * This is the exact same algorithm that's on the NesDev wiki, to debug
         * my code above. */ \
        register unsigned char y; \
        if((ppu->v&0x7000) != 0x7000){ \
            ppu->v += 0x1000; \
        }else{ \
            ppu->v &= ~0x7000; \
            y = (ppu->v&0x03E0)>>5; \
            if(y == 29){ \
                y = 0; \
                ppu->v ^= 0x0800; \
            }else if(y == 31){ \
                y = 0; \
            }else{ \
                y += 1; \
            } \
            ppu->v = (ppu->v&~0x03E0)|(y<<5); \
        } \
    }
#endif

#define MN_PPU_BG_FILL_SHIFT_REGS() \
    { \
        /* The bitplanes go into the high 8-bit of two 16-bit shift
         * registers. */ \
        ppu->low_shift &= ~0xFF; \
        ppu->high_shift &= ~0xFF; \
        ppu->low_shift |= ppu->low_bp; \
        ppu->high_shift |= ppu->high_bp; \
 \
        ppu->attr_latch1 = ppu->attr>>((ppu->v&1)<<1)>>(((ppu->v>>5)&1)<<2); \
        ppu->attr_latch2 = ppu->attr>>((ppu->v&1)<<1)>>(((ppu->v>>5)&1)<<2)>> \
                           1; \
    }

#define MN_PPU_BG_FETCHES_DONE() \
    { \
        /* Reload the shift registers */ \
        MN_PPU_BG_FILL_SHIFT_REGS(); \
 \
        /* Increment coarse X in v */ \
        MN_PPU_BG_COARSE_X_INC(); \
    }

#define MN_PPU_BG_FETCH(step) \
    { \
        switch((step)&7){ \
            case 0: \
                ppu->addr = 0x2000|(ppu->v&0x0FFF); \
                ppu->video_mem_bus = ppu->addr; \
                break; \
            case 1: \
                ppu->tile_id = (ppu->video_mem_bus = emu->mapper. \
                                vram_read(emu, &emu->mapper, \
                                          ppu->addr)); \
                break; \
            case 2: \
                /* Address calculated as described at
                 * https://www.nesdev.org/wiki/PPU_scrolling#Tile_and_attribute
                 * _fetching */ \
                ppu->addr = (0x2000+32*30)|(ppu->v&0x0C00)| \
                            ((ppu->v>>4)&0x38)|((ppu->v>>2)&7); \
                ppu->video_mem_bus = ppu->addr; \
                break; \
            case 3: \
                ppu->attr = (ppu->video_mem_bus = emu->mapper. \
                             vram_read(emu, &emu->mapper, ppu->addr)); \
                break; \
            case 4: \
                ppu->addr = (ppu->tile_id<<4)|(ppu->v>>12); \
                ppu->video_mem_bus = ppu->addr; \
                break; \
            case 5: \
                ppu->low_bp = (ppu->video_mem_bus = emu->mapper. \
                               vram_read(emu, &emu->mapper, \
                                         ppu->addr)); \
                break; \
            case 6: \
                ppu->addr = (ppu->tile_id<<4)|(1<<3)|(ppu->v>>12); \
                ppu->video_mem_bus = ppu->addr; \
                break; \
            case 7: \
                ppu->high_bp = (ppu->video_mem_bus = emu->mapper. \
                                vram_read(emu, &emu->mapper, \
                                          ppu->addr)); \
                MN_PPU_BG_FETCHES_DONE(); \
                break; \
        } \
    }

#define MN_PPU_BG_SHIFT() \
    { \
        /* Shift the shift registers */ \
        ppu->low_shift <<= 1; \
        ppu->low_shift |= 1; \
        ppu->high_shift <<= 1; \
        ppu->high_shift |= 1; \
 \
        ppu->attr1_shift <<= 1; \
        ppu->attr1_shift |= ppu->attr_latch1; \
 \
        ppu->attr2_shift <<= 1; \
        ppu->attr2_shift |= ppu->attr_latch2; \
    }

#define MN_PPU_BG_GET_PIXEL() \
    { \
        register unsigned char color; \
        register unsigned char palette; \
 \
        color = ((ppu->low_shift>>(ppu->x+8))&1)|(((ppu->high_shift>> \
                 (ppu->x+8))&1)<<1); \
 \
        /* Use the universal background color if color == 0 */ \
        if(!color) palette = 0; \
        palette = ((ppu->attr1_shift>>ppu->x)&1)| \
                  (((ppu->attr2_shift>>ppu->x)&1)<<1); \
 \
        pixel = color|(palette<<2); \
    }

#define MN_PPU_DRAW_PIXEL(pixel) \
    { \
        idx = emu->mapper.vram_read(emu, &emu->mapper, \
                                    0x3F00+((pixel)>>2)*4+((pixel)&3)); \
        /* The two upper bytes are not stored */ \
        idx &= 0x3F; \
 \
        if(ppu->mask&MN_PPU_MASK_GRAYSCALE) idx &= 0x30; \
 \
        ppu->draw_pixel((ppu->palette[0x40*(ppu->mask>>5)+idx*3]<<16)| \
                        (ppu->palette[0x40*(ppu->mask>>5)+idx*3+1]<<8)| \
                        ppu->palette[0x40*(ppu->mask>>5)+idx*3+2]); \
    }

#define MN_PPU_INC_CYCLE() \
    { \
        ppu->cycle++; \
        if(ppu->cycle > 340){ \
            ppu->cycle = 0; \
            ppu->scanline++; \
            if(ppu->scanline > 261){ \
                ppu->scanline = 0; \
                ppu->even_frame = !ppu->even_frame; \
            } \
        } \
    }

unsigned char mn_ppu_bg(MNPPU *ppu, MNEmu *emu);
unsigned char mn_ppu_sprites(MNPPU *ppu, MNEmu *emu);

/*
 * Border region:
 * 16 pixels left
 * 11 pixels right
 * 2 pixels  down
 *
 */
void mn_ppu_cycle(MNPPU *ppu, MNEmu *emu) {
    MNCPU *cpu = &emu->cpu;
    unsigned char bg_pixel;
    unsigned char sprite_pixel;
    unsigned char idx;

    unsigned char pixel;

    if(ppu->scanline == 261 && ppu->cycle == 340 && !ppu->even_frame &&
       (ppu->mask&MN_PPU_MASK_RENDER)){
        /* Skip the last cycle of the pre-render scanline on an odd frames */
        MN_PPU_INC_CYCLE();
    }

    if(ppu->scanline <= 239 || ppu->scanline == 261){
        if(ppu->cycle >= 257 && ppu->cycle <= 320){
            /* OAMADDR is repeatedly set to 0 during these cycles */
            ppu->oamaddr = 0;
        }

        if(ppu->cycle == 1){
#if 0
            printf("l: %08b%08b h: %08b%08b v: %07b%08b\n", ppu->low_shift>>8,
                   ppu->low_shift&0xFF, ppu->high_shift>>8,
                   ppu->high_shift&0xFF, (ppu->v>>8)&0x7F, ppu->v&0xFF);
#endif
        }

        if(ppu->mask&MN_PPU_MASK_RENDER){
            /*printf("sv: %07b%08b %u %u\n", (ppu->t>>8)&((1<<8)-1),
                     ppu->t&0xFF, ppu->cycle, ppu->scanline);*/

            /* Visible scanlines or pre-render scanline */

            bg_pixel = mn_ppu_bg(ppu, emu);

            if(ppu->cycle == 257){
                /* Copy some bits of t to v */

                ppu->v &= ~(((1<<5)-1)|(1<<10));
                ppu->v |= ppu->t&(((1<<5)-1)|(1<<10));
            }

            if(ppu->scanline == 261){
                /* Pre-render scanline only code */

                if(ppu->cycle == 1){
                    /* Clear flags */
                    ppu->vblank = 0;
                    ppu->sprite0_hit = 0;
                    ppu->sprite_overflow = 0;
                    cpu->nmi_pin = 1;
                }

                if(ppu->cycle >= 280 && ppu->cycle <= 304){
                    /* The PPU repeatedly copies these bits in these cycles of
                     * the pre-render scanline. */
                    ppu->v &= ~((((1<<4)-1)<<11)|((1<<5)-1)<<5);
                    ppu->v |= cpu->t&((((1<<4)-1)<<11)|((1<<5)-1)<<5);
                }
            }else{
                sprite_pixel = mn_ppu_sprites(ppu, emu);

                if(ppu->cycle >= 1 && ppu->cycle <= 256){

#if 0
                    if(!(ppu->mask&MN_PPU_MASK_BACKGROUND)) bg_pixel = 0;
                    if(!(ppu->mask&MN_PPU_MASK_SPRITES)) sprite_pixel = 0;
#endif

                    pixel = sprite_pixel;

                    /* Select the right pixel and output it */
#if 0
                    if(((sprite_pixel&(1<<4)) && (bg_pixel&2)) ||
                       !(sprite_pixel&2)){
                        pixel = bg_pixel;
                    }
#endif
                    MN_PPU_DRAW_PIXEL(pixel);
                }
            }
        }else{
            if(ppu->cycle >= 1 && ppu->cycle <= 256){
                if(ppu->scanline != 261){
                    /* Produce a pixel */

                    /* TODO */
                    MN_PPU_DRAW_PIXEL(0);
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
        }
        /* Vertical blanking lines */
    }

    if(ppu->ctrl&MN_PPU_CTRL_NMI && ppu->vblank) cpu->nmi_pin = 0;

    MN_PPU_INC_CYCLE();

    if(ppu->since_start < ppu->startup_time){
        ppu->since_start++;
    }

    /* Let the CPU run all 3 PPU cycles */
    if(ppu->cycles_since_cpu_cycle >= 3){
        mn_cpu_cycle(&emu->cpu, emu);
        mn_dma_cycle(&emu->dma, emu);
        ppu->cycles_since_cpu_cycle = 0;
    }

    ppu->cycles_since_cpu_cycle++;
}

unsigned char mn_ppu_bg(MNPPU *ppu, MNEmu *emu) {
    unsigned char pixel = 0;

    /* Memory fetches */
    if(ppu->cycle >= 321 && ppu->cycle <= 336){
        MN_PPU_BG_FETCH(ppu->cycle-321);
        MN_PPU_BG_SHIFT();
    }else if(ppu->cycle >= 1 && ppu->cycle <= 256){
        MN_PPU_BG_FETCH(ppu->cycle-1);
    }

    /*printf("v: %06b%08b t: %06b%08b\n", ppu->v>>8, ppu->v&0xFF, ppu->t>>8,
           ppu->t&0xFF);*/

    if(ppu->cycle >= 337 && ppu->cycle <= 340){
        /* Dummy nametable fetches */
        switch((ppu->cycle-337)&3){
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

    if(ppu->cycle == 0){
        /* Cycle 0 is an IDLE cycle */
    }else if(ppu->cycle >= 1 && ppu->cycle <= 256){
        if(ppu->scanline != 261){
            /* Produce a background pixel */
            MN_PPU_BG_GET_PIXEL();

            /*printf("%08b%08b %08b%08b\n", ppu->low_shift>>8,
                     ppu->low_shift&0xFF, ppu->high_shift>>8,
                     ppu->high_shift&0xFF);*/
        }
        if(ppu->cycle >= 2){
            /* Shift registers shift for the first time at cycle 2 */
            MN_PPU_BG_SHIFT();
        }

        if(ppu->cycle == 256){
            /* Increment Y */

            MN_PPU_BG_Y_INC();
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
    return pixel;
}

#define MN_PPU_OAM_WRITE(b) \
    { \
        /* If secondary OAM is full, read instead (useless, so it isn't
         * emulated) */ \
        if(ppu->secondary_oam_pos < 32){ \
            ppu->secondary_oam[ppu->secondary_oam_pos] = b; \
        } \
    }

#define MN_PPU_OAM_IN_RANGE(y) ((y) <= ppu->scanline && \
                            (y)+(ppu->big_sprites ? 16 : 8) > ppu->scanline)

#define MN_PPU_OAM_FETCH(step) \
    { \
        register unsigned char v_flip, h_flip; \
        register unsigned char attr; \
        register unsigned char y; \
        register unsigned char pos = ((step)&~7)>>1; \
        switch((step)&7){ \
            case 0: \
                ppu->addr = 0x2000|(ppu->v&0x0FFF); \
                ppu->video_mem_bus = ppu->addr; \
                break; \
            case 1: \
                ppu->tile_id = (ppu->video_mem_bus = emu->mapper. \
                                vram_read(emu, &emu->mapper, \
                                          ppu->addr)); \
                break; \
            case 2: \
                /* Address calculated as described at
                 * https://www.nesdev.org/wiki/PPU_scrolling#Tile_and_attribute
                 * _fetching */ \
                ppu->addr = (0x2000+32*30)|(ppu->v&0x0C00)| \
                            ((ppu->v>>4)&0x38)|((ppu->v>>2)&7); \
                ppu->video_mem_bus = ppu->addr; \
                break; \
            case 3: \
                ppu->attr = (ppu->video_mem_bus = emu->mapper. \
                             vram_read(emu, &emu->mapper, ppu->addr)); \
                break; \
            case 4: \
                /* XXX: On which dots do the attributes from secondary OAM fill
                 * the sprite FIFO a.k.a. the motion picture buffer? */ \
                y = ppu->secondary_oam[pos]; \
                ppu->tile_id = ppu->secondary_oam[pos+1]; \
                attr = ppu->secondary_oam[pos+2]; \
                ppu->sprite_fifo[(step)>>3].palette = attr&2; \
                ppu->sprite_fifo[(step)>>3].priority = attr>>5; \
                ppu->sprite_fifo[(step)>>3].down_counter = \
                    ppu->secondary_oam[pos+3]; \
                v_flip = attr>>7; \
                ppu->addr = ((ppu->tile_id<<4)|(v_flip ? \
                              8-(((ppu->scanline-1)-y)&7) : \
                              (((ppu->scanline-1)-y)&7)))/*+ \
                            ((ppu->scanline-1)-y > 16 ? \
                             (ppu->big_sprites^v_flip)*16 : \
                             (ppu->big_sprites^v_flip^1)*16)*/; \
                ppu->video_mem_bus = ppu->addr; \
                break; \
            case 5: \
                h_flip = (ppu->secondary_oam[pos+2]>>6)&1; \
                attr = (ppu->video_mem_bus = emu->mapper. \
                               vram_read(emu, &emu->mapper, \
                                         ppu->addr)); \
                if(h_flip){ \
                    attr = (attr>>7)|((attr>>6)&1)<<1|((attr>>5)&1)<<2| \
                           ((attr>>4)&1)<<3|((attr>>3)&1)<<4| \
                           ((attr>>2)&1)<<5|((attr>>1)&1)<<6|(attr&1)<<7; \
                } \
                /* XXX: Is this accurate? */ \
                if(ppu->secondary_oam_pos < pos) attr = 0; \
                ppu->sprite_fifo[(step)>>3].low_bp = attr; \
                break; \
            case 6: \
                y = ppu->secondary_oam[pos]; \
                attr = ppu->secondary_oam[pos+2]; \
                v_flip = attr>>7; \
                ppu->addr = ((ppu->tile_id<<4)|(1<<3)|(v_flip ? \
                              8-(((ppu->scanline-1)-y)&7) : \
                              (((ppu->scanline-1)-y)&7)))/*+ \
                            ((ppu->scanline-1)-y > 16 ? \
                             (ppu->big_sprites^v_flip)*16 : \
                             (ppu->big_sprites^v_flip^1)*16)*/; \
                ppu->video_mem_bus = ppu->addr; \
                break; \
            case 7: \
                h_flip = (ppu->secondary_oam[pos+2]>>6)&1; \
                attr = (ppu->video_mem_bus = emu->mapper. \
                                vram_read(emu, &emu->mapper, \
                                          ppu->addr)); \
                if(h_flip){ \
                    attr = (attr>>7)|((attr>>6)&1)<<1|((attr>>5)&1)<<2| \
                           ((attr>>4)&1)<<3|((attr>>3)&1)<<4| \
                           ((attr>>2)&1)<<5|((attr>>1)&1)<<6|(attr&1)<<7; \
                } \
                /* XXX: Is this accurate? */ \
                if(ppu->secondary_oam_pos < pos) attr = 0; \
                ppu->sprite_fifo[(step)>>3].high_bp = attr; \
                break; \
        } \
    }

unsigned char mn_ppu_sprites(MNPPU *ppu, MNEmu *emu) {
    register unsigned char read = 0;
    unsigned char sprite_pixel = 0;
    unsigned char i;
    unsigned char inc = 0;

    if(ppu->cycle == 0){
#if 0
        unsigned char i, n;
        puts("Secondary OAM dump:");
        for(i=0;i<32;i+=4){
            for(n=0;n<4;n++){
                printf("%02x ", ppu->secondary_oam[i+n]);
            }
            puts("");
        }
#endif
    }

    if(ppu->cycle >= 1 && ppu->cycle <= 64){
        ppu->secondary_oam[(ppu->cycle-1)>>1] = 0xFF;
    }else if(ppu->cycle <= 256){
        if(ppu->cycle == 65){
            /* TODO: Use OAMADDR for all primary oam accesses when performing
             * sprite evaluation instead of n and m */
            ppu->n = 0;
            ppu->m = 0;
            ppu->secondary_oam_pos = 0;
            ppu->step = 0;
#if 0
            puts("SPRITE EVALUATION");
#endif
        }
        if(ppu->cycle&1){
            /* Data is read from primary OAM */
            ppu->b = ppu->primary_oam[ppu->n*4+ppu->m];
        }else{
            /* Data is written to secondary OAM */

            if(ppu->step == 0){
                /* Step 1 */
                if(!ppu->m){
                    ppu->y = ppu->b;
                    if(MN_PPU_OAM_IN_RANGE(ppu->y)){
                        inc = 1;
#if 0
                        printf("%u %u %u %u: Y in range\n", ppu->step+1,
                               ppu->secondary_oam_pos, ppu->n, ppu->m);
#endif
                    }else{
                        ppu->step++;
#if 0
                        printf("%u %u %u %u: Y not in range\n", ppu->step+1,
                               ppu->secondary_oam_pos, ppu->n, ppu->m);
#endif
                    }
                }else if(MN_PPU_OAM_IN_RANGE(ppu->y)){
                    /* The Y coordinate is in range, so we copy the other
                     * bytes */
#if 0
                    printf("%u %u %u %u: Sprite copy\n", ppu->step+1,
                           ppu->secondary_oam_pos, ppu->n, ppu->m);
#endif
                    if(ppu->m == 3) ppu->step++;
                    inc = 1;
                }
                ppu->m++;
            }
            if(ppu->step == 1){
                    /* Step 2 */
                    ppu->n++;
                    if(!ppu->n){
                        /* n has overflowed back to 0, all sprites got
                         * evaluated */
#if 0
                        printf("%u %u %u %u: All sprites got evaluated\n",
                               ppu->step+1, ppu->secondary_oam_pos, ppu->n,
                               ppu->m);
#endif
                        ppu->step = 3;
                    }else{
                        if(ppu->secondary_oam_pos >= 32){
                            /* 8 sprites have been found */
                            ppu->m = 0;
                            ppu->entries_read = 0;
                            ppu->step++;
#if 0
                            printf("%u %u %u %u: Secondary oam is full\n",
                                   ppu->step+1, ppu->secondary_oam_pos, ppu->n,
                                   ppu->m);
#endif
                        }else{
                            ppu->m = 0;
                            ppu->step = 0;
#if 0
                            printf("%u %u %u %u: Continue\n",
                                   ppu->step+1, ppu->secondary_oam_pos, ppu->n,
                                   ppu->m);
#endif
                        }
                    }
            }
            if(ppu->step == 2){
                /* Step 3 */
                if(MN_PPU_OAM_IN_RANGE(ppu->b)){
                    ppu->sprite_overflow = 1;
                    ppu->entries_read = 0;
                    read = 1;
                }else{
                    ppu->n++;
                    ppu->m++;
                    if(!ppu->n){
                        ppu->m = 0;
                        ppu->step++;
                    }
                }
                if(ppu->entries_read || read){
                    ppu->m++;
                    if(!ppu->m) ppu->n++;
                    ppu->entries_read++;
                }
            }
            if(ppu->step == 3){
                /* Step 4 */
                /* Fail to write OAM[n][0] */
            }

            MN_PPU_OAM_WRITE(ppu->b);
            if(inc && ppu->secondary_oam_pos < 32){
                ppu->secondary_oam_pos++;
            }
        }
    }else if(ppu->cycle <= 320){
        /* Sprite fetches */
        MN_PPU_OAM_FETCH(ppu->cycle-257);
    }else if(ppu->cycle <= 340 || !ppu->cycle){
        /* Background pipeline initialization */
    }

    if(ppu->cycle >= 1 && ppu->cycle <= 256){
        /* Produce a pixel */

        for(i=0;i<8;i++){
            if(ppu->sprite_fifo[i].down_counter){
                /* Decrease the counter */
                ppu->sprite_fifo[i].down_counter--;
            }else{
                /* Get a sprite pixel */
                sprite_pixel = (ppu->sprite_fifo[i].low_bp>>7)|
                               (ppu->sprite_fifo[i].high_bp>>7)>>1|
                               ppu->sprite_fifo[i].palette<<2|
                               ppu->sprite_fifo[i].priority<<4;

                /* Shift the shift registers */
                ppu->sprite_fifo[i].low_bp <<= 1;
                ppu->sprite_fifo[i].high_bp <<= 1;
            }
        }
    }
    return sprite_pixel;
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
            ppu->io_bus = ppu->primary_oam[ppu->oamaddr];
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
               (ppu->mask&MN_PPU_MASK_RENDER)){
                /* The PPU is rendering */
                /* Perform coarse X increment and Y increment + trigger a load
                 * next value */
                MN_PPU_BG_COARSE_X_INC();
                MN_PPU_BG_Y_INC();
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
            ppu->t &= ~(3<<10);
            ppu->t |= (value&3)<<10;
            ppu->ctrl = value;
            ppu->big_sprites = value>>6;
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
            /* TODO: Emulate corruption */
            ppu->oamaddr = value;
            break;
        case MN_PPU_OAMDATA:
            ppu->primary_oam[ppu->oamaddr] = value;
            ppu->oamaddr++;
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
                ppu->t |= value;
                ppu->v = ppu->t;
                ppu->w = 0;
            }else{
                ppu->t &= 0xFF;
                ppu->t |= (value&((1<<6)-1))<<8;
                ppu->w = 1;
            }
            break;
        case MN_PPU_PPUDATA:
#if 0
            printf("%04x = %02x\n", ppu->v&((1<<15)-1), value);
#endif
            emu->mapper.vram_write(emu, &emu->mapper, ppu->v&((1<<15)-1),
                                   value);
            if((ppu->scanline < 240 || ppu->scanline == 261) &&
               (ppu->mask&MN_PPU_MASK_RENDER)){
                /* The PPU is rendering */
                /* Perform coarse X increment and Y increment + trigger a load
                 * next value */
                MN_PPU_BG_COARSE_X_INC();
                MN_PPU_BG_Y_INC();
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
