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

#include <mapper.h>

#include <mappers/nrom.h>

#define MN_MAPPER_AMOUNT 1

static MNMapper *mn_mapper_list[MN_MAPPER_AMOUNT] = {
    &mn_mapper_nrom
};

int mn_mapper_find(MNMapper *mapper, unsigned char *rom, size_t size) {
    int id;

    if(size < 16){
        return MN_MAPPER_E_SIZE;
    }

    id = rom[6]>>4;
    id |= rom[7]&0xF0;

    if((rom[7]&((1<<2)|(1<<3))) == (1<<3)){
        /* NES 2.0 */

        /* TODO: Support NES 2.0 */
    }

    if(id >= MN_MAPPER_AMOUNT) return MN_MAPPER_E_UNKNOWN;

    if(mn_mapper_list[id] != NULL){
        *mapper = *mn_mapper_list[id];

        return MN_MAPPER_E_NONE;
    }

    return MN_MAPPER_E_UNKNOWN;
}

unsigned long int mn_mapper_rand(unsigned long int *seed) {
    /* Algorithm "xor" from p. 4 of Marsaglia, "Xorshift RNGs" */
    *seed ^= *seed << 13;
	*seed ^= *seed >> 17;
	*seed ^= *seed << 5;

	/* It's made for 32-bit integers */
	*seed &= 0xFFFFFFFF;
    return *seed;
}

void mn_mapper_ram_init(unsigned char *buffer, size_t size) {
    size_t i;

    /* TODO: Use the current time instead */
    unsigned long int seed = 1;

    for(i=0;i<size;i++){
        buffer[i] = mn_mapper_rand(&seed);
    }
}
