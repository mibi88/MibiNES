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

#include <dma.h>

/* TODO: Add support for DMC DMA */

int mn_dma_init(MNDMA *dma) {
    /* TODO: Pick a random cycle */
    dma->cycle = 0;

    dma->value = 0;
    dma->step = 0;

    dma->do_oam_dma = 0;
    dma->do_dmc_dma = 0;

    dma->aligned = 1;

    return 0;
}

void mn_dma_cycle(MNDMA *dma, MNEmu *emu) {
    if(dma->do_oam_dma){
        emu->cpu.rdy = 0;
        if(emu->cpu.halted && ((!dma->cycle && !dma->aligned) ||
                               dma->aligned)){
            switch(dma->cycle){
                case 0:
                    /* get cycle */
                    dma->value = emu->mapper.read(emu, &emu->mapper,
                                                  dma->page<<8|dma->step);
                    break;
                case 1:
                    /* put cycle */
                    emu->mapper.write(emu, &emu->mapper, 0x2004,
                                      dma->value);

                    /* Stop DMA once we copied 256 bytes */
                    dma->step++;
                    if(!dma->step){
                        dma->do_oam_dma = 0;
                        emu->cpu.rdy = 1;
                    }
                    break;
            }
            dma->aligned = 1;
        }
    }else{
        dma->aligned = 0;
        dma->step = 0;
    }

    dma->cycle = !dma->cycle;
}

void mn_dma_free(MNDMA *dma) {
    /* There is nothing to do here */
    (void)dma;
}
