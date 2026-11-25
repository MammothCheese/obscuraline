/* SPDX-License-Identifier: GPL-3.0-only */
/* Every leg driven by the autopilot, with every part on the train (the synth breaks' cruise included): prints each
 * leg's "travel A>B: ... N s to spare" line. tests/retime.py turns them into lines.txt's times (autopilot + 2 s).
 *
 *   gcc -O2 -DDINGHY_HOST -Dmain=host_main -DAPP_VER='"dev"' -Ifirmware/hal -Ifirmware/src -Ifirmware/gen -o build/host/legtime tests/legtime.c
 *   DENSHA_AUTOPLAY=1 build/host/legtime */
#include "../firmware/src/obscura.c"
#undef main

int main(void)
{
    static chapter_t ch;
    static int32_t prm[2];
    uint32_t l, c;
    for (c = 0; c < NCHAP; c++) if (CHAP[c].type == MECH_TRAVEL) { ch = CHAP[c]; break; }
    if (getenv("LEG_ENODEN")) {                 /* LEG_ENODEN=1: the Enoden's stretches instead (every part but the synth) */
        for (l = 0; l < NENLEGS; l++) {
            cur_ch = &ch; prm[0] = 0; ch.prm = prm;
            tr_rain_pct = 0;
            tr_mode = TRM_ENODEN; tr_force = &EN_LEGS[l]; tr_names = EN_STATION; tr_features = (uint8_t)(0xFFu & ~(F_SYNTH | F_VOICE));
            memset(&in, 0, sizeof in);
            fm1_ms = 1000;
            tr_enter();
            while (!tr.done && fm1_ms < 2000000u) { fm1_ms += 10; tr_update(); }
            tr_leave();
        }
        return 0;
    }
    for (l = 0; l < NTRLEGS; l++) {
        if (TR_LEGS[l].ride) continue;          /* (a ride is a rhythm game, not a drive) */
        prm[0] = (int32_t)l; ch.prm = prm; cur_ch = &ch;
        tr_rain_pct = getenv("LEG_RAIN") ? 100 : 0;
        tr_mode = TRM_STORY; tr_force = 0; tr_names = TR_STATION; tr_features = 0xFF;
        memset(&in, 0, sizeof in);
        fm1_ms = 1000;
        tr_enter();
        while (!tr.done && fm1_ms < 2000000u) { fm1_ms += 10; tr_update(); }
        if (tr_white) printf("%6u ms  travel %s>%s: into the light\n", fm1_ms, TR_STATION[tr.L->st_from], TR_STATION[tr.L->st_to]);
        tr_leave();
    }
    return 0;
}
