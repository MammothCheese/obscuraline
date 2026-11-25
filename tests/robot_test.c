/* SPDX-License-Identifier: GPL-3.0-only */
/* The daily robots, tested the way a player solves them: thousands of robots of every model, each solved with
 * the methods in docs/GUIDE.md using only what the player can see (the needles, the lit relays, the meters'
 * warmth, the voice box's answers), pressing the real controls through the game's own update.
 *
 *   cc -O2 -DDINGHY_HOST -Dmain=host_main -Ifirmware/hal -Ifirmware/src -Ifirmware/gen -o build/host/robot_test tests/robot_test.c
 *   build/host/robot_test [count]
 * Also the gear boards: every board drawn must be solved by its own stored solution. */
#include "../firmware/src/obscura.c"
#undef main

static void turn(uint32_t enc, int32_t d) { memset(&in, 0, sizeof in); in.enc[enc] = d; rb_update(); }
static void press_btn(uint32_t b) { memset(&in, 0, sizeof in); in.press = 1u << b; rb_update(); }
static void play_keys(const uint8_t *keys, int32_t n)
{
    int32_t i;
    for (i = 0; i < n; i++) { memset(&in, 0, sizeof in); in.kdown = 1u << keys[i]; rb_update(); }
}
static int lit(int i) { return (int)(rb.relay >> i & 1u); }

static int solve_gauges(void)                   /* in order: each later dial never disturbs an earlier gauge */
{
    static const uint8_t CK[3] = {ENC_SELECT, ENC_PRESETS, ENC_ALGORITHM};
    int i;
    for (i = 0; i < 3; i++) turn(CK[i], rb.t[i] - rb.g[i]);
    return (rb.done & RB_CAL) != 0;
}
static int solve_relays(void)                   /* chase left to right; the last column decides; chase again */
{
    int pass, c, i;
    for (pass = 0; pass < 2; pass++) {
        for (c = 0; c < 5; c++) {
            if (!lit(c)) press_btn(RB_BTN[c + 1]);
            if (!lit(6 + c)) press_btn(RB_BTN[6 + c + 1]);
        }
        if (pass == 0) {
            int glo = !lit(5), rec = !lit(11);
            if (glo) press_btn(RB_BTN[0]);      /* FX */
            if (rec) press_btn(RB_BTN[6]);      /* HOME */
        }
    }
    /* the meters, by warmth alone: a step one way; colder? the other way; on until it is green */
    {
        static const uint8_t MK[4] = {ENC_KNOB1, ENC_KNOB2, ENC_KNOB3, ENC_KNOB4};
        for (i = 0; i < 4; i++) {
            int dir = 1, guard = 80;
            int32_t d0 = iabs(rb.m[i] - rb.h[i]);
            while (rb.m[i] != rb.h[i] && guard--) {
                turn(MK[i], dir);
                if (iabs(rb.m[i] - rb.h[i]) > d0 || (rb.m[i] == 0 || rb.m[i] == 30)) dir = -dir;
                d0 = iabs(rb.m[i] - rb.h[i]);
            }
        }
    }
    return (rb.done & RB_REL) != 0;
}
/* the voice box: guess only codes consistent with every answer so far (the answers as the screen shows them) */
static int32_t answer(const uint8_t *code, const uint8_t *g, int32_t n, uint8_t *fb)
{
    int32_t j, m, right = 0;
    for (j = 0; j < n; j++) {
        fb[j] = 0;
        if (g[j] == code[j]) { fb[j] = 2; right++; continue; }
        for (m = 0; m < n; m++) if (code[m] == g[j]) fb[j] = 1;
    }
    return right;
}
static int solve_voice(int32_t *guesses)
{
    static uint8_t cand[2600][5];
    int32_t nc = 0, n = rb.clen, a, b, c, d, e, k;
    for (a = 0; a < 7; a++) for (b = 0; b < 7; b++) for (c = 0; c < 7; c++) for (d = 0; d < (n > 3 ? 7 : 1); d++) for (e = 0; e < (n > 4 ? 7 : 1); e++) {
        uint8_t v[5] = {(uint8_t)a, (uint8_t)b, (uint8_t)c, (uint8_t)d, (uint8_t)e};
        int i, j, ok = 1;
        for (i = 0; i < n; i++) for (j = i + 1; j < n; j++) if (v[i] == v[j]) ok = 0;
        if (ok && nc < 2600) { memcpy(cand[nc], v, 5); nc++; }
    }
    for (k = 0; k < 20 && !(rb.done & RB_VOX); k++) {
        uint8_t keys[5], fb[5];
        int32_t i, w = 0;
        for (i = 0; i < n; i++) keys[i] = RB_VKEY[cand[0][i]];
        play_keys(keys, n);
        (*guesses)++;
        if (rb.done & RB_VOX) break;
        memcpy(fb, rb.histfb[rb.nhist - 1], 5);  /* what the screen shows */
        {
            uint8_t gs[5];
            memcpy(gs, cand[0], 5);                 /* (the guess, kept apart: the list is filtered in place) */
        for (i = 0; i < nc; i++) {              /* keep the codes that would have answered the same */
            uint8_t f2[5];
            int32_t j, same = 1;
            answer(cand[i], gs, n, f2);
            for (j = 0; j < n; j++) if (f2[j] != fb[j]) same = 0;
            if (same) memcpy(cand[w++], cand[i], 5);
        }
        }
        nc = w;
        if (!nc) return 0;
    }
    return (rb.done & RB_VOX) != 0;
}

int main(int argc, char **argv)
{
    int32_t count = argc > 1 ? atoi(argv[1]) : 3000, r, fails = 0, guesses = 0, worst = 0, lay;
    cur_ch = &WORKSHOP_CH;
    for (lay = 1; lay <= 7; lay++)
        for (r = 0; r < count / 7; r++) {
            int ok = 1;
            int32_t g = 0;
            rng_s ^= (uint32_t)(r * 2654435761u + lay * 977u); if (!rng_s) rng_s = 1;
            rb_force_layout = lay;
            rb_enter();
            if (rb.parts & RB_CAL) ok &= solve_gauges();
            if (rb.parts & RB_REL) ok &= solve_relays();
            if (rb.parts & RB_VOX) { ok &= solve_voice(&g); guesses += g; if (g > worst) worst = g; }
            memset(&in, 0, sizeof in);
            if (!rb_update()) ok = 0;           /* the robot itself says it is mended */
            if (!ok) { fails++; if (fails < 5) printf("FAIL: layout %d robot %d (parts %u done %u)\n", lay, r, rb.parts, rb.done); }
        }
    printf("robots: %d tried, %d failed; voice box: %.2f guesses on average, %d at worst\n",
           (count / 7) * 7, fails, (double)guesses / (double)((count / 7) * 4), worst);
    /* the gear boards: each drawn board, its own solution in place, must turn every hand the right way */
    {
        int32_t bf = 0, nb = 0, h, ch2;
        static chapter_t fake;
        static int32_t prm[6];
        static const mo_list_t lst[1] = {{0, 0}};
        for (h = 1; h <= 2; h++) for (ch2 = 1; ch2 <= 2; ch2++) for (r = 0; r < 300; r++) {
            int32_t i, k, ok = 1;
            prm[P_GEARBOARD_HANDS] = h; prm[P_GEARBOARD_CHAIN] = ch2;
            prm[P_GEARBOARD_RUSTED_LO] = 2; prm[P_GEARBOARD_RUSTED_HI] = 4; prm[P_GEARBOARD_SPARES_LO] = 0; prm[P_GEARBOARD_SPARES_HI] = 2;
            fake.prm = prm; fake.lst = lst; fake.type = MECH_GEARBOARD;
            cur_ch = &fake;
            rng_s ^= (uint32_t)(r * 40503u + h * 7u + ch2 * 131u); if (!rng_s) rng_s = 1;
            gb_enter();
            for (i = 0; i < GB_N; i++) if (gb.sol[i] && !gb.fixed[i]) gb.g[i] = gb.sol[i];
            gb_drive();
            for (k = 0; k < gb.nhands; k++) if (gb.dir[gb.hand[k]] != gb.want[gb.hand[k]]) ok = 0;
            if (gb.jam) ok = 0;
            nb++;
            if (!ok) bf++;
        }
        printf("gear boards: %d drawn, %d not solved by their own solution\n", nb, bf);
        return fails || bf;
    }
}
