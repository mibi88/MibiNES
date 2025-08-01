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

unsigned char *load_file(char *name, char *file, size_t *s) {
    FILE *fp;
    unsigned char *buffer;
    size_t size;

    fp = fopen(file, "rb");
    if(fp == NULL){
        fprintf(stderr, "%s: Failed to load \"%s\"!\n", name, file);

        return NULL;
    }

    fseek(fp, 0, SEEK_END);
    size = ftell(fp);
    rewind(fp);

    buffer = malloc(size);
    if(buffer == NULL){
        fprintf(stderr, "%s: Failed to allocate %lu bytes!\n", name, size);

        return NULL;
    }

    fread(buffer, 1, size, fp);

    *s = size;

    return buffer;
}

int main(int argc, char **argv) {
    unsigned char *rom;
    unsigned char *palette;
    size_t size;
    size_t palette_size;

    if(argc < 3){
        fprintf(stderr, "USAGE: %s [ROM] [PALETTE]\nA small NES emulator\n",
                argv[0]);

        return EXIT_FAILURE;
    }

    rom = load_file(argv[0], argv[1], &size);
    if(rom == NULL){
        return EXIT_FAILURE;
    }

    palette = load_file(argv[0], argv[2], &palette_size);
    if(palette == NULL){
        return EXIT_FAILURE;
    }
    if(palette_size < 0x600){
        fprintf(stderr, "%s: Bad palette size. Size must be 1536 bytes!\n",
                argv[0]);
    }

    if(mn_gui_init(rom, palette, size)){
        fprintf(stderr, "%s: Failed to initialize %s!\n", argv[0], argv[0]);

        free(rom);

        return EXIT_FAILURE;
    }

    mn_gui_run();

    mn_gui_free();

    free(rom);

    return EXIT_SUCCESS;
}
