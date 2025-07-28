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

#include <stdio.h>
#include <stdlib.h>

#include <gui.h>

int main(int argc, char **argv) {
    FILE *fp;

    unsigned char *rom;
    size_t size;

    if(argc < 2){
        fprintf(stderr, "USAGE: %s [ROM]\nA small NES emulator\n", argv[0]);

        return EXIT_FAILURE;
    }

    fp = fopen(argv[1], "rb");
    if(fp == NULL){
        fprintf(stderr, "%s: Failed to load \"%s\"!\n", argv[0], argv[1]);

        return EXIT_FAILURE;
    }

    fseek(fp, 0, SEEK_END);
    size = ftell(fp);
    rewind(fp);

    rom = malloc(size);
    if(rom == NULL){
        fprintf(stderr, "%s: Failed to allocate %lu bytes!\n", argv[0], size);

        return EXIT_FAILURE;
    }

    fread(rom, 1, size, fp);

    if(mn_gui_init(rom, size)){
        fprintf(stderr, "%s: Failed to initialize %s!\n", argv[0], argv[0]);

        free(rom);

        return EXIT_FAILURE;
    }

    mn_gui_run();

    mn_gui_free();

    free(rom);

    return EXIT_SUCCESS;
}
