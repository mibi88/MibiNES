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

#include <nesctrl.h>

static int mn_nesctrl_init(void *_ctrl, void *_emu) {
    /* There is nothing to do here */
    (void)_emu;
    (void)_ctrl;

    return 0;
}

static unsigned char mn_nesctrl_load_reg(void *_ctrl, void *_emu) {
    MNCtrl *ctrl = _ctrl;
    (void)_emu;

    ctrl->reg = ctrl->get_input();

    return ctrl->reg;
}

static unsigned char mn_nesctrl_shift_reg(void *_ctrl, void *_emu) {
    MNCtrl *ctrl = _ctrl;
    (void)_emu;

    ctrl->reg >>= 1;
    ctrl->reg |= 1<<7;

    return ctrl->reg;
}

static unsigned char mn_nesctrl_read(void *_ctrl, void *_emu) {
    MNCtrl *ctrl = _ctrl;
    (void)_emu;

    return ctrl->reg&1;
}

static void mn_nesctrl_free(void *_emu, void *_ctrl) {
    /* There is nothing to do here */
    (void)_emu;
    (void)_ctrl;
}

MNCtrl mn_nesctrl = {
    0,
    0,

    mn_nesctrl_init,
    mn_nesctrl_load_reg,
    mn_nesctrl_shift_reg,
    mn_nesctrl_read,
    mn_nesctrl_free,

    NULL,

    NULL
};
