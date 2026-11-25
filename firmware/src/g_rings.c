/* SPDX-License-Identifier: GPL-3.0-only */
/* Chapter X: THE LABYRINTH. A brass ball, gravity, and concentric rings.
 *
 * The ball rests on top of the outer ring. Each ring has a GAP, and BARRIERS: one on its inner face, reaching
 * into the channel inside it, one on its outer face, reaching into the channel outside it. K1..K4 turn the
 * rings, outer to inner. The ball always rolls down to the lowest place it can reach in its channel. Gravity
 * is gravity: in the upper half of a channel the ball lies on the inner ring and drops inward through its gap; in
 * the lower half it lies on the outer ring, and falls back out through that ring's gap (out of the outer ring: back
 * to the start). A barrier stops it, and a turning ring's barrier pushes
 * it along, uphill if need be. Bring it to the centre. Turning a ring moves its barriers in both channels it
 * borders, so which way you turn matters. Gap and barrier places are drawn each time. The key lights (C D E F)
 * show a ring whose gap is right under the ball. The rings are geared: each knob turns its ring and the next ring
 * in the other way (the inner knob only its own), so they are set in order, outer first, each correcting the next. */

#define RG_N 4                                  /* rings */
#define RG_STEP 1820                            /* a detent: 10 degrees */
static const int32_t RG_R[RG_N] = {80, 61, 42, 24};   /* ring centre radii (the ball on top stays under the header) */
#define RG_CY 124
#define RG_W 5                                  /* ring half thickness */
#define RG_GAP 2600                             /* half width of a gap (angle) */

static struct {
    int32_t n;
    uint32_t rot[RG_N], tgt[RG_N];              /* drawn and wanted rotation */
    uint32_t gap[RG_N], bin[RG_N], bout[RG_N];  /* on each ring: its gap, inner barrier, outer barrier (ring angles) */
    int32_t lvl;                                /* -1 on top of the outer ring, 0..n-2 in a channel, n-1 at the centre */
    int32_t phi, om;                            /* angle (16-bit, as an int for arithmetic), angular speed */
    int32_t fall, fdir;                         /* 0..256: falling; +1 inward, -1 outward */
    uint32_t fin_t;
} rg;

static int32_t rg_wrap(int32_t a) { a &= 0xFFFF; return a >= 32768 ? a - 65536 : a; }
static int32_t rg_chan_r(int32_t l) { return l < 0 ? RG_R[0] + RG_W + 5 : (RG_R[l] + RG_R[l + 1]) / 2; }

static void rg_enter(void)
{
    int32_t k;
    rg.n = PRM(P_RINGS_RINGS);
    for (k = 0; k < RG_N; k++) {
        rg.gap[k] = rnd() & 0xFFFFu;
        rg.bin[k] = (rg.gap[k] + 9000u + rndn(24000)) & 0xFFFFu;    /* never on the gap itself */
        rg.bout[k] = (rg.gap[k] + 33000u + rndn(20000)) & 0xFFFFu;
        rg.rot[k] = rg.tgt[k] = (uint32_t)(rndn(36) * RG_STEP);
    }
    if (iabs(rg_wrap((int32_t)(rg.gap[0] + rg.rot[0]) - 49152)) < 12000) rg.rot[0] = rg.tgt[0] = rg.rot[0] + 30000u;   /* not open at the start */
    rg.lvl = -1; rg.phi = 49152; rg.om = 0; rg.fall = 0; rg.fdir = 1; rg.fin_t = 0;
}
/* the barriers in the ball's channel l: ring l's inner one, ring l+1's outer one (screen angles) */
static void rg_barriers(int32_t l, int32_t *b, int32_t *nb, int32_t *ring)
{
    *nb = 0;
    if (l < 0) return;
    b[*nb] = (int32_t)((rg.bin[l] + rg.rot[l]) & 0xFFFFu); ring[(*nb)++] = l;
    if (l + 1 < rg.n) { b[*nb] = (int32_t)((rg.bout[l + 1] + rg.rot[l + 1]) & 0xFFFFu); ring[(*nb)++] = l + 1; }
}
static int rg_update(void)
{
    static const uint8_t KN[RG_N] = {ENC_KNOB1, ENC_KNOB2, ENC_KNOB3, ENC_KNOB4};
    int32_t k, b[2], nb, ring[2], before[RG_N];
    led_btn_mask = BIT(BTN_EDIT);
    for (k = 0; k < rg.n; k++) {
        int32_t e = in.enc[KN[k]];
        if (e) {                                /* a ring, and (geared to it) the next ring in, the other way */
            rg.tgt[k] += (uint32_t)(e * RG_STEP);
            if (k + 1 < rg.n) rg.tgt[k + 1] -= (uint32_t)(e * RG_STEP);
            sfx_click(0, 40, (uint8_t)(60 + k * 40)); sfx_thunk(0, 56 + k * 4, 22, 128);
        }
    }
    /* the rings ease toward their turns; a moving barrier pushes the ball ahead of it */
    for (k = 0; k < rg.n; k++) {
        int32_t d = rg_wrap((int32_t)(rg.tgt[k] - rg.rot[k]));
        before[k] = (int32_t)rg.rot[k];
        if (d) rg.rot[k] += (uint32_t)(d / 3 + (d > 0) - (d < 0));
    }
    if (rg.fall) {                              /* falling, in or out: a short drop, then rolling again */
        rg.fall += 40;
        if (rg.fall >= 256) {
            rg.fall = 0; rg.lvl += rg.fdir;
            rg.om = (int32_t)rndn(81) - 40;      /* (it never lands balanced on a crest) */
            sfx_thunk(0, 64 - rg.lvl * 4, 50, 128);
            if (rg.fdir > 0) sfx_tine(76 + rg.lvl * 5, 70, au_time(), 128, 0);
            if (rg.lvl < 0) { rg.phi = 49152; rg.om = 0; sfx_buzz(0); }   /* out of the outer ring: back on top */
            if (rg.lvl >= rg.n - 1) return 1;   /* the centre */
        }
        return 0;
    }
    if (rg.lvl >= rg.n - 1) return 1;
    if (rg.lvl < 0) {                           /* on top of the outer ring: it drops in when the gap comes under it */
        if (iabs(rg_wrap((int32_t)((rg.gap[0] + rg.rot[0]) & 0xFFFFu) - rg.phi)) < RG_GAP - 600) { rg.fall = 1; rg.fdir = 1; sfx_click(0, 60, 128); }
        return 0;
    }
    /* gravity along the channel (bottom = 16384), damping (om in 1/16ths: no dead spot at the crest) */
    rg.om += (mo_cos((uint32_t)rg.phi) * 60) >> 11;
    rg.om -= rg.om / 16;
    rg.phi = (rg.phi + rg.om / 32) & 0xFFFF;
    /* barriers: the ball cannot pass one; a turning one pushes it */
    rg_barriers(rg.lvl, b, &nb, ring);
    for (k = 0; k < nb; k++) {
        int32_t r = rg_chan_r(rg.lvl), c = 7 * 65536 / (6 * r) + 300;   /* contact distance as an angle */
        int32_t d = rg_wrap(rg.phi - b[k]), moved = rg_wrap((int32_t)rg.rot[ring[k]] - before[ring[k]]);
        int32_t dprev = rg_wrap(rg.phi - (b[k] - moved));
        if (iabs(d) < c || (iabs(dprev) < 16384 && ((d > 0) != (dprev > 0)) && iabs(d) < 16384)) {
            int32_t side = dprev >= 0 ? 1 : -1;
            rg.phi = (b[k] + side * c) & 0xFFFF;
            if ((rg.om > 0 && side < 0) || (rg.om < 0 && side > 0)) { if (iabs(rg.om) > 4160) sfx_click(0, 30, 128); rg.om = -rg.om / 4; }
        }
    }
    /* a gap under the ball: in the upper half it lies on the inner ring, and drops in through its gap; in the lower
     * half it lies on the outer ring, and falls back out through that one's */
    if (mo_sin((uint32_t)rg.phi) < -6000) {
        if (iabs(rg_wrap((int32_t)((rg.gap[rg.lvl + 1] + rg.rot[rg.lvl + 1]) & 0xFFFFu) - rg.phi)) < RG_GAP - 700) { rg.fall = 1; rg.fdir = 1; }
    } else if (mo_sin((uint32_t)rg.phi) > 6000) {
        if (iabs(rg_wrap((int32_t)((rg.gap[rg.lvl] + rg.rot[rg.lvl]) & 0xFFFFu) - rg.phi)) < RG_GAP - 700) { rg.fall = 1; rg.fdir = -1; }
    }
    return 0;
}

static void rg_draw(uint32_t won)
{
    static const uint16_t COL[RG_N] = {C_CYAN, C_ROSE, C_AMBER, C_GOLD};
    int32_t cx = 120, cy = RG_CY, k, j;
    if (won && !rg.fin_t) { rg.fin_t = fm1_ms; sfx_motif(au_time() + ms_frames(300)); }
    /* the brick vault, the door's dark interior */
    g_cab_wall(cx, cy, 0);
    g_disc(cx, cy, 97, C_ROSE);
    g_disc(cx, cy, 95, RGB(30, 14, 54));
    g_disc(cx, cy, 93, RGB(8, 4, 18));
    if (won) g_led(cx, cy, 60, mix565(RGB(8, 4, 18), C_CYAN_D, 160));
    for (k = 0; k < rg.n; k++) {
        int32_t r = RG_R[k];
        uint32_t gp = (rg.gap[k] + rg.rot[k]) & 0xFFFFu;
        g_ring(cx, cy, r - RG_W, r + RG_W, C_BRASS_D);
        g_ring(cx, cy, r - RG_W + 1, r + RG_W - 1, C_BRASS);
        for (j = 0; j < 24; j++) {              /* rivets turning with the ring: its motion is visible */
            uint32_t a = rg.rot[k] + (uint32_t)j * 2731u;
            px_set(px16(cx, r * 16, a) >> 4, py16(cy, r * 16, a) >> 4, C_BRASS_DD);
        }
        {   /* the gap: a cut in the ring, rimmed in the ring's colour */
            int32_t q[8] = {px16(cx, (r - RG_W - 1) * 16, gp - RG_GAP), py16(cy, (r - RG_W - 1) * 16, gp - RG_GAP), px16(cx, (r + RG_W + 1) * 16, gp - RG_GAP), py16(cy, (r + RG_W + 1) * 16, gp - RG_GAP),
                            px16(cx, (r + RG_W + 1) * 16, gp + RG_GAP), py16(cy, (r + RG_W + 1) * 16, gp + RG_GAP), px16(cx, (r - RG_W - 1) * 16, gp + RG_GAP), py16(cy, (r - RG_W - 1) * 16, gp + RG_GAP)};
            g_quad16(q, RGB(8, 4, 18));
            g_led(px16(cx, r * 16, gp - RG_GAP) >> 4, py16(cy, r * 16, gp - RG_GAP) >> 4, 1, COL[k]);
            g_led(px16(cx, r * 16, gp + RG_GAP) >> 4, py16(cy, r * 16, gp + RG_GAP) >> 4, 1, COL[k]);
        }
        /* barriers: a bar from the ring's inner face inward, and from its outer face outward */
        if (k + 1 < rg.n) {
            uint32_t a = (rg.bin[k] + rg.rot[k]) & 0xFFFFu;
            g_thick16(px16(cx, (r - RG_W) * 16, a) + 8, py16(cy, (r - RG_W) * 16, a) + 8, px16(cx, (RG_R[k + 1] + RG_W + 1) * 16, a) + 8, py16(cy, (RG_R[k + 1] + RG_W + 1) * 16, a) + 8, 56, COL[k]);
        }
        if (k > 0) {
            uint32_t a = (rg.bout[k] + rg.rot[k]) & 0xFFFFu;
            g_thick16(px16(cx, (r + RG_W) * 16, a) + 8, py16(cy, (r + RG_W) * 16, a) + 8, px16(cx, (RG_R[k - 1] - RG_W - 1) * 16, a) + 8, py16(cy, (RG_R[k - 1] - RG_W - 1) * 16, a) + 8, 56, COL[k]);
        }
    }
    g_disc(cx, cy, RG_R[rg.n - 1] - RG_W - 1, won ? C_GOLD : RGB(30, 16, 52));
    if (won) g_led(cx, cy, 8, C_GLOW);
    else g_sigil(cx, cy, 13, C_AMBER);         /* the mark again: the door is the same machine's */
    {   /* the ball */
        int32_t r0 = rg_chan_r(rg.lvl), r1 = rg.fdir < 0 ? rg_chan_r(rg.lvl - 1) : rg.lvl + 1 < rg.n - 1 ? rg_chan_r(rg.lvl + 1) : 0, r = r0 + (r1 - r0) * rg.fall / 256;
        int32_t bx, by;
        if (won || rg.lvl >= rg.n - 1) r = 0;
        bx = px16(cx, r * 16, (uint32_t)rg.phi) >> 4; by = py16(cy, r * 16, (uint32_t)rg.phi) >> 4;
        g_disc(bx, by, 5, C_STEEL_L);
        g_disc(bx - 1, by - 2, 2, C_CREAM);
    }
    for (k = 0; k < rg.n; k++) {                /* knob legends, coloured like their rings */
        char t[3] = {'K', (char)('1' + k), 0};
        g_text(6, 30 + k * 16, &FONT_S, t, COL[k]);
    }
    for (k = 0; k < RG_N; k++) {                /* key lights: a ring whose gap is right under the ball */
        static const uint8_t KK[RG_N] = {7, 9, 11, 12};
        int on = k < rg.n && iabs(rg_wrap((int32_t)((rg.gap[k] + rg.rot[k]) & 0xFFFFu) - rg.phi)) < RG_GAP;
        fm1_led_key_glow(KEY_LED(KK[k]), k < rg.n);
        fm1_led_key(KEY_LED(KK[k]), on && !won);
    }
}
static void rg_leave(void) { ui_keys_clear(); }
static const game_t G_RINGS = {BIT(BTN_EDIT), 1800, rg_enter, rg_update, rg_draw, rg_leave};
