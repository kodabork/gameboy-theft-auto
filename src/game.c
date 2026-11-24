/* New York game layer: player on foot / in cars, traffic, pedestrians, cops and
   wanted levels, weapons, pickups, missions and the chase camera. */
#include "engine.h"
#include "world.h"
#include "ground.h"
#include "game.h"
#include "assets.h"
#include "sound.h"
#include "minigames.h"

#define TICKS 16384                 /* dt units per second */
#define Q8(x) ((x) * 256)
#define MAX_CARS 60
#define MAX_PEDS 56
#define MAX_DROPS 32
#define MAX_FLASH 8
#define MAX_PICKUPS 180
#define NFIXED_PICKUPS 52

/* fixed car slots */
#define SLOT_TRAFFIC0 1
#define NTRAFFIC 20
#define SLOT_PARKED0 21
#define SLOT_PARKED_END 50
#define SLOT_POLICE0 50
#define NPOLICE 5
#define SLOT_RIVAL0 55
#define SLOT_MISSION 57
/* fixed ped slots */
#define PED_CIV_END 42      /* 0..41 street population (incl. beat cops) */
#define PED_CREW0 42        /* 42..44 cops that jump out of police cars */
#define PED_MISSION0 45     /* 45..54 job targets and gunmen */
#define PED_ACTOR 55        /* the person Nick talks to in a cutscene */

enum { AI_NONE, AI_TRAFFIC, AI_POLICE, AI_RIVAL };
enum { P_CIV, P_COP, P_TARGET, P_GUARD };
enum { S_WALK, S_FLEE, S_CHASE, S_IDLE };

typedef struct {
    int x, y;        /* q8 units */
    int head;        /* q8 angle */
    int speed;       /* q8 units/s (forward) */
    u16 model; u8 ai, dir, road, cross; s16 wait;
    int tx, ty;      /* AI target (units) */
    int vx, vy;      /* player physics velocity */
    u8 active, police, crew, cp;
    s16 stuck, rev;
    int kx, ky;      /* knock-back velocity from collisions (q8 units/s) */
    int spin;        /* yaw rate from collisions (q8 angle units/s) */
    s16 dmg;         /* damage 0..1000: smokes from 650, burns from 880, then explodes */
    int burn;        /* burning: ticks to the explosion; wreck: ticks of smouldering left */
} Car;
#define CAR_SMOKE 650
#define CAR_FIRE 880
#define IS_WRECK(c) ((c)->model == CAR_WRECK)

typedef struct {
    int x, y, head, phase, speed;
    u16 model; u8 bx, by, corner, cw;
    u8 active, down, pool, type, state, run, aim;
    s16 timer, hp, cd;
    int fx, fy;      /* threat / goal point (units) */
    int rvx, rvy, rvz, rh;   /* ragdoll: velocity (q8 units/s) and height above the ground (q8 units) */
    u16 rang; s16 rspin; u8 rag, rbounce;   /* tumble pitch (4096/turn), spin (per s), flying?, bounces */
} Ped;

typedef struct { int x, y, h, vx, vy, vh; s16 life; } Drop;
typedef struct { int x, y, h; s16 t; } Flash;
typedef struct { int x, y; u8 type, avail; s16 timer; } Pickup;

EWRAM_BSS static Car cars[MAX_CARS];
static int ncars;
EWRAM_BSS static Ped peds[MAX_PEDS];
EWRAM_BSS static Drop drops[MAX_DROPS];
EWRAM_BSS static Flash flashes[MAX_FLASH];
EWRAM_BSS static Pickup pickups[MAX_PICKUPS];

static struct {
    int in_car;
    int x, y, head, phase, moving, running;
    int hp, money;
    u8 weapon, owned;           /* owned bitmask: 1 pistol, 2 smg, 4 shotgun, 8 rifle, 16 rocket launcher */
    s16 ammo[6];
    int fire_cd, aim_t, hurt_cd, arrest_t;
    int dead_t; u8 dead_kind;
} pl;

static int wanted, wanted_cool;
static int nitro = 1000;
static int drifting, boosting;
static int cam_ang, cam_dist = M(7);
static int impact;
static u16 keys_prev;
static int prev_step;
static u32 frame_no;
static u8 near_cars[MAX_CARS]; static int n_near;   /* active cars within ~70 m of the player */
EWRAM_BSS GameHud g_hud;

#define AINL static inline __attribute__((always_inline))
AINL int angdiff(int a, int b) { return ((a - b + 2048) & 4095) - 2048; }
AINL int iabs(int v) { return v < 0 ? -v : v; }
AINL int dist_approx(int dx, int dy) { dx = iabs(dx); dy = iabs(dy); return dx > dy ? dx + (dy >> 1) : dy + (dx >> 1); }
AINL int plx(void) { return pl.x >> 8; }
AINL int ply(void) { return pl.y >> 8; }

static void msg(const char *a, const char *b, int secs) { g_hud.msg1 = a; g_hud.msg2 = b; g_hud.msg_t = secs * TICKS; }
static void big(const char *t, int color, int secs) { g_hud.big = t; g_hud.big_color = color; g_hud.big_t = secs * TICKS; }

/* ------------------------------------------------------------ traffic */
static void lane_point(int dir, int road, int cross, int *ox, int *oy)
{
    switch (dir) {
    case 0: *ox = ROAD_C(cross); *oy = ROAD_C(road) - LANE_OFS; break;
    case 2: *ox = ROAD_C(cross); *oy = ROAD_C(road) + LANE_OFS; break;
    case 1: *ox = ROAD_C(road) + LANE_OFS; *oy = ROAD_C(cross); break;
    default:*ox = ROAD_C(road) - LANE_OFS; *oy = ROAD_C(cross); break;
    }
}
static int dsign(int d) { return (d == 0 || d == 1) ? 1 : -1; }

static void ai_choose(Car *c)
{
    int d = c->dir, X = c->cross, R = c->road;
    int opts[4], n = 0;
    if (X + dsign(d) >= 0 && X + dsign(d) <= NB) { opts[n++] = d; opts[n++] = d; }
    for (int k = 1; k <= 3; k += 2) {
        int nd = (d + k) & 3;
        int nc = R + dsign(nd);
        if (nc >= 0 && nc <= NB) opts[n++] = nd;
    }
    if (n == 0) return;
    int nd = opts[rnd() % n];
    if (nd == d) c->cross = X + dsign(d);
    else { c->road = X; c->cross = R + dsign(nd); c->dir = nd; }
    lane_point(c->dir, c->road, c->cross, &c->tx, &c->ty);
}

static const int dang[4] = { 0, 1024, 2048, 3072 };

static void place_on_lane(Car *c, int dir, int road, int at, int past_m)
{
    int x, y; lane_point(dir, road, at, &x, &y);
    if (dir == 0 || dir == 2) x += dsign(dir) * M(past_m); else y += dsign(dir) * M(past_m);
    c->dir = dir; c->road = road;
    c->x = Q8(x); c->y = Q8(y);
    c->head = Q8(dang[dir]);
    c->cross = at + dsign(dir);
    if (c->cross < 0 || c->cross > NB) { c->dir = (dir + 2) & 3; c->head = Q8(dang[c->dir]); c->cross = at + dsign(c->dir); }
    lane_point(c->dir, c->road, c->cross, &c->tx, &c->ty);
}

static int clampi(int v, int lo, int hi) { return v < lo ? lo : v > hi ? hi : v; }

/* ---- coastal road traffic: follows the loop's waypoints in the right-hand lane */
#define ROAD_COAST 99
static void coast_target(Car *c)
{
    int k = c->cp, n = c->dir ? (k + NCOAST_WP - 1) % NCOAST_WP : (k + 1) % NCOAST_WP;
    int a = iatan2(coast_wp[n][1] - coast_wp[k][1], coast_wp[n][0] - coast_wp[k][0]);
    c->tx = coast_wp[n][0] + ((isin(a) * M(2.2)) >> 14);
    c->ty = coast_wp[n][1] - ((icos(a) * M(2.2)) >> 14);
}
static int spawn_coast_near(Car *c)
{
    for (int tries = 0; tries < 24; tries++) {
        int k = (int)(rnd() % NCOAST_WP);
        int d = dist_approx(coast_wp[k][0] - plx(), coast_wp[k][1] - ply());
        if (d < M(60) || d > M(130)) continue;
        c->cp = k; c->dir = rnd() & 1; c->road = ROAD_COAST;
        coast_target(c);
        int a = iatan2(c->ty - coast_wp[k][1], c->tx - coast_wp[k][0]);
        c->x = Q8(coast_wp[k][0] + ((isin(a) * M(2.2)) >> 14));
        c->y = Q8(coast_wp[k][1] - ((icos(a) * M(2.2)) >> 14));
        c->head = Q8(a);
        c->cp = c->dir ? (k + NCOAST_WP - 1) % NCOAST_WP : (k + 1) % NCOAST_WP;
        coast_target(c);
        return 1;
    }
    return 0;
}

/* put a traffic car on a road 60..130 m from the player */
static void spawn_traffic_near(Car *c)
{
    int pbx = plx() >> 10, pby = ply() >> 10;
    /* near the edge of the grid or out of town: some of the traffic uses the coastal road */
    int out = plx() < M(90) || ply() < M(90) || plx() > GRID_U - M(90) || ply() > GRID_U - M(90);
    if (out && (rnd() & 1) && spawn_coast_near(c)) {
        c->speed = Q8(M(9)); c->ai = AI_TRAFFIC; c->active = 1; c->police = 0; c->wait = 1; c->kx = c->ky = c->spin = 0; c->dmg = 0; c->burn = 0;
        return;
    }
    for (int tries = 0; tries < 12; tries++) {
        int dir = rnd() & 3;
        int road = clampi(((dir & 1) ? pbx : pby) + rndr(-1, 2), 0, NB);
        int at = clampi(((dir & 1) ? pby : pbx) + rndr(-2, 3), 1, NB - 1);
        place_on_lane(c, dir, road, at, 20);
        int d = dist_approx((c->x >> 8) - plx(), (c->y >> 8) - ply());
        if (d > M(60) && d < M(130)) break;
    }
    c->speed = Q8(M(6)); c->ai = AI_TRAFFIC; c->active = 1; c->police = 0; c->wait = 1; c->kx = c->ky = c->spin = 0; c->dmg = 0; c->burn = 0;
}

static int obstacle_ahead(int self, const Car *c)
{
    int hx = icos(c->head >> 8), hy = isin(c->head >> 8);
    int cx = c->x >> 8, cy = c->y >> 8;
    for (int k = -1; k < n_near; k++) {
        int i = k < 0 ? -1 : near_cars[k];
        if (i == self) continue;
        int ox, oy;
        if (i < 0) { if (pl.in_car >= 0) continue; ox = plx(); oy = ply(); }
        else { ox = cars[i].x >> 8; oy = cars[i].y >> 8; }
        int dx = ox - cx, dy = oy - cy;
        if (iabs(dx) > M(12) || iabs(dy) > M(12)) continue;
        int fwd = (dx * hx + dy * hy) >> 14;
        int lat = (dx * hy - dy * hx) >> 14;
        if (fwd > 0 && fwd < M(11) && lat > -M(1.5) && lat < M(1.5)) return 1;
    }
    return 0;
}

static void steer_to(Car *c, int tx, int ty, int rate, int dt, int *diff_out)
{
    int x = c->x >> 8, y = c->y >> 8;
    int want = iatan2(ty - y, tx - x);
    int diff = angdiff(want, c->head >> 8);
    *diff_out = diff;
    int mt = rate * dt / TICKS + 1;
    if (diff > mt) diff = mt; else if (diff < -mt) diff = -mt;
    if (c->speed < 0) diff = -diff;
    c->head = (c->head + Q8(diff)) & (Q8(4096) - 1);
}

static void approach_speed(Car *c, int target, int acc, int dec, int dt)
{
    if (c->speed < target) { c->speed += acc / 64 * dt / (TICKS / 64); if (c->speed > target) c->speed = target; }
    else { c->speed -= dec / 64 * dt / (TICKS / 64); if (c->speed < target) c->speed = target; }
}

static int tail_mode;
static void update_traffic(int i, int dt)
{
    Car *c = &cars[i];
    int x = c->x >> 8, y = c->y >> 8;
    int dx = c->tx - x, dy = c->ty - y;
    int coast = c->road == ROAD_COAST;
    if (coast) {
        if (dx * dx + dy * dy < M(5) * M(5)) {
            c->cp = c->dir ? (c->cp + NCOAST_WP - 1) % NCOAST_WP : (c->cp + 1) % NCOAST_WP;
            coast_target(c);
        }
    } else if (dx * dx + dy * dy < M(3) * M(3)) ai_choose(c);
    int diff; steer_to(c, c->tx, c->ty, 1100, dt, &diff);
    int target = coast ? Q8(M(12)) : Q8(M(9));
    if (i == SLOT_MISSION) target = tail_mode ? Q8(M(10)) : Q8(M(17));   /* a fleeing (or unaware, tailed) mission car */
    if (diff > 6 || diff < -6) target = Q8(M(5));
    int nearp = dist_approx(x - plx(), y - ply()) < M(70);
    if (nearp && c->wait > 0 && obstacle_ahead(i, c)) target = 0;
    if (target == 0 && c->speed == 0) { if (++c->wait > 50) c->wait = -25; }
    else if (c->wait < 0) c->wait++;
    else if (c->wait == 0) c->wait = 1;
    approach_speed(c, target, Q8(M(4)), Q8(M(12)), dt);
}

/* ------------------------------------------------------------ police */
static void crew_exit(Car *c);

static void update_police(int i, int dt)
{
    Car *c = &cars[i];
    int tx = plx(), ty = ply();
    if (pl.in_car >= 0) {          /* lead the target a little */
        tx += cars[pl.in_car].vx / 512; ty += cars[pl.in_car].vy / 512;
    }
    int d = dist_approx(tx - (c->x >> 8), ty - (c->y >> 8));
    if (c->rev > 0) {              /* backing out after getting stuck */
        c->rev -= dt >> 6;
        approach_speed(c, -Q8(M(6)), Q8(M(14)), Q8(M(20)), dt);
        int diff; steer_to(c, tx, ty, 900, dt, &diff);
        return;
    }
    int diff; steer_to(c, tx, ty, 1500, dt, &diff);
    int target = d > M(30) ? Q8(M(33)) : d > M(12) ? Q8(M(20)) : Q8(M(10));
    if (iabs(diff) > 700) target = Q8(M(7));
    approach_speed(c, target, Q8(M(13)), Q8(M(24)), dt);
    if (c->speed < Q8(M(1.5)) && d > M(8)) { if (++c->stuck > 45) { c->stuck = 0; c->rev = 256; } }
    else c->stuck = 0;
    int pspd = pl.in_car >= 0 ? iabs(cars[pl.in_car].speed) : 0;
    if (c->crew && d < M(14) && pspd < Q8(M(3))) crew_exit(c);
}

static int active_police(void)
{
    int n = 0;
    for (int i = 0; i < NPOLICE; i++) if (cars[SLOT_POLICE0 + i].active && cars[SLOT_POLICE0 + i].ai == AI_POLICE) n++;
    return n;
}

static void spawn_police(void)
{
    for (int i = 0; i < NPOLICE; i++) {
        Car *c = &cars[SLOT_POLICE0 + i];
        if (c->active) continue;
        spawn_traffic_near(c);
        c->model = CAR_POLICE; c->police = 1; c->ai = AI_POLICE; c->crew = 1; c->speed = Q8(M(15)); c->dmg = 0; c->burn = 0;
        c->stuck = 0; c->rev = 0;
        return;
    }
}

/* ------------------------------------------------------------ radio */
static const char *const station_name[NSTATIONS] = { "HARBOR FM", "KICK 104.5", "IRON 97", "LOW END 88", "SMOOTH 101" };
static const char *radio_label(void) { return g_snd.radio_on ? station_name[g_snd.station] : "RADIO OFF"; }
static void radio_show(void) { g_hud.radio_name = radio_label(); g_hud.radio_t = TICKS * 5 / 2; }
/* step through the stations and "off" (UP/DOWN in a car, or A on the pause menu) */
static void radio_step(int d)
{
    int cur = g_snd.radio_on ? g_snd.station : NSTATIONS;
    cur = (cur + d + NSTATIONS + 1) % (NSTATIONS + 1);
    if (cur == NSTATIONS) g_snd.radio_on = 0; else { g_snd.radio_on = 1; g_snd.station = cur; }
    g_snd.sfx |= SFX_TUNE;
    radio_show();
}

/* ------------------------------------------------------------ player car */
static void drive(Car *c, u16 keys, int dt)
{
    int a = c->head >> 8;
    int ca = icos(a), sa = isin(a);
    int fwd = ((c->vx >> 4) * ca + (c->vy >> 4) * sa) >> 10;
    int lat = ((c->vx >> 4) * sa - (c->vy >> 4) * ca) >> 10;
    int steer = (keys & KEY_LEFT) ? 1 : (keys & KEY_RIGHT) ? -1 : 0;
    int handbrake = (keys & KEY_B) && steer && fwd > Q8(M(9));
    boosting = (keys & KEY_L) && nitro > 0 && fwd > 0;
    int vmax = boosting ? Q8(M(80)) : Q8(M(62));
    if (keys & KEY_A) {
        int acc = Q8(M(12));
        if (fwd > 0) { int ratio = ((vmax - fwd) >> 8) * 256 / (vmax >> 8); if (ratio < 0) ratio = 0; acc = acc * ratio >> 8; }
        if (boosting) acc += Q8(M(14));
        fwd += acc / 64 * dt / (TICKS / 64);
    } else if (keys & KEY_B) {
        if (fwd > Q8(M(1))) fwd -= (handbrake ? Q8(M(9)) : Q8(M(28))) / 64 * dt / (TICKS / 64);
        else { fwd -= Q8(M(7)) / 64 * dt / (TICKS / 64); if (fwd < -Q8(M(12))) fwd = -Q8(M(12)); }
    } else {
        int drag = Q8(M(2.5)) / 64 * dt / (TICKS / 64) + fwd / 256;
        if (fwd > drag) fwd -= drag; else if (fwd < -drag) fwd += drag; else fwd = 0;
    }
    if (boosting) { nitro -= 260 * dt / TICKS + 1; if (nitro < 0) nitro = 0; }
    {   /* hills: gravity along the slope */
        int x = c->x >> 8, y = c->y >> 8, ox = (ca * M(1.5)) >> 14, oy = (sa * M(1.5)) >> 14;
        int dh = ground_h(x + ox, y + oy) - ground_h(x - ox, y - oy);        /* rise over 3 m */
        int ga = -Q8(M(9.8)) / M(3) * dh;
        if ((keys & (KEY_A | KEY_B)) || iabs(fwd) > Q8(M(1.5)))      /* parked: brake holds it */
            fwd += ga / 64 * dt / (TICKS / 64);
    }
    if (fwd > vmax) fwd -= (fwd - vmax) / 8;
    int grip = handbrake ? 2 : (fwd > Q8(M(40)) ? 7 : 10);
    int g = grip * dt * 256 / TICKS; if (g > 256) g = 256;
    lat -= (lat * g) >> 8;
    drifting = lat > Q8(M(3)) || lat < -Q8(M(3));
    nitro += (drifting ? 170 : 25) * dt / TICKS;
    if (nitro > 1000) nitro = 1000;
    if (steer) {
        int v = fwd >> 8, av = iabs(v);
        int R = M(4.5) + av / 2;
        int omega = v * 652 / R;
        if (handbrake || drifting) omega = omega * 3 / 2;
        if (av < M(1)) omega = 0;
        c->head = (c->head + steer * omega * dt / 64) & (Q8(4096) - 1);
    }
    c->vx = ((fwd >> 4) * ca + (lat >> 4) * sa) >> 10;
    c->vy = ((fwd >> 4) * sa - (lat >> 4) * ca) >> 10;
    c->speed = fwd;
    if (c->spin) {                                   /* spin from a crash, tyres scrub it off */
        c->head = (c->head + c->spin / 64 * dt / (TICKS / 64)) & (Q8(4096) - 1);
        int sd = 3 * dt; if (sd > TICKS) sd = TICKS;
        c->spin -= (c->spin >> 7) * (sd >> 7);
        if (c->spin > -256 && c->spin < 256) c->spin = 0;
    }
}

/* ---------------------------------------------------------- collisions */
/* Every car is an oriented box sized to its model. Crashes push cars apart along the
   contact normal, bounce them with an impulse, and spin them when hit off-centre. */
static void dims_of(int model, int *hl, int *hw)
{
    /* set index = offset / CAR_SET_FRAMES, by reciprocal multiply (Thumb code has no fast divide) */
    enum { RCP = ((1 << 22) + CAR_SET_FRAMES - 1) / CAR_SET_FRAMES };
    int off = model == CAR_WRECK ? CAR_SEDAN_SILVER - CAR_COUPE_BLUE : model - CAR_COUPE_BLUE;
    int k = off < 0 ? -1 : (int)(((unsigned)off * RCP) >> 22);
    if (k < 0 || k >= CAR_NSETS) k = 0;
    *hl = car_dims[k][0]; *hw = car_dims[k][1];
}

/* sprite frame for a car: 64 view angles at the two nearest elevation levels, 32 at the others.
   Cars are left/right symmetric, so only headings 0..n/2 are stored; the rest are drawn mirrored
   (bit 15 of the frame number = flip horizontally). */
static int car_frame(int model, int level, int rel)
{
    static const u8 off[4] = { 0, 33, 66, 83 };
    int n = level < 2 ? 64 : 32;
    int i = level < 2 ? (((rel + 32) >> 6) & 63) : (((rel + 64) >> 7) & 31);
    if (i <= n / 2) return model + off[level] + i;
    return (model + off[level] + (n - i)) | 0x8000;
}

static int is_player_car(const Car *c) { return pl.in_car >= 0 && c == &cars[pl.in_car]; }
static void car_damage(Car *c, int d, int by_player);

static void car_vel(const Car *c, int *vx, int *vy)
{
    if (is_player_car(c)) { *vx = c->vx; *vy = c->vy; return; }
    int a = c->head >> 8;
    *vx = (((c->speed >> 4) * icos(a)) >> 10) + c->kx;
    *vy = (((c->speed >> 4) * isin(a)) >> 10) + c->ky;
}

static void car_add_vel(Car *c, int dvx, int dvy)
{
    if (is_player_car(c)) { c->vx += dvx; c->vy += dvy; return; }
    int a = c->head >> 8, ca = icos(a), sa = isin(a);
    int f = ((dvx >> 4) * ca + (dvy >> 4) * sa) >> 10;     /* along the car: speed */
    c->speed += f;
    c->kx += dvx - (((f >> 4) * ca) >> 10);                 /* the rest: sideways shove */
    c->ky += dvy - (((f >> 4) * sa) >> 10);
}

static void add_spin(Car *c, int rx, int ry, int nx10, int ny10, int j)
{
    /* torque = r x (j n); calibrated so a hard corner hit gives ~half a turn per second */
    int rxn = rx * ny10 - ry * nx10;               /* units * q10 */
    int ds = ((rxn >> 4) * (j >> 8)) >> 1;
    if (ds > 900000) ds = 900000; else if (ds < -900000) ds = -900000;
    c->spin += ds;
}

static void move_car(Car *c, int dt)
{
    int player = is_player_car(c);
    int moving = player || c->speed || c->kx || c->ky || c->spin;
    if (!moving) return;
    if (!player) {
        int dist = (c->speed / 64) * dt / (TICKS / 64);
        int a = c->head >> 8;
        c->x += (dist * icos(a)) >> 14;
        c->y += (dist * isin(a)) >> 14;
        c->x += (c->kx / 64) * dt / (TICKS / 64);
        c->y += (c->ky / 64) * dt / (TICKS / 64);
        int dec = 6 * dt; if (dec > TICKS) dec = TICKS;           /* knock-back dies off quickly */
        c->kx -= (c->kx >> 7) * (dec >> 7); c->ky -= (c->ky >> 7) * (dec >> 7);
        if (c->kx > -64 && c->kx < 64) c->kx = 0;
        if (c->ky > -64 && c->ky < 64) c->ky = 0;
        c->head = (c->head + c->spin / 64 * dt / (TICKS / 64)) & (Q8(4096) - 1);
        int sd = 4 * dt; if (sd > TICKS) sd = TICKS;
        c->spin -= (c->spin >> 7) * (sd >> 7);
        if (c->spin > -256 && c->spin < 256) c->spin = 0;
    } else {
        c->x += (c->vx / 64) * dt / (TICKS / 64);
        c->y += (c->vy / 64) * dt / (TICKS / 64);
    }
    /* traffic far from the player stays in its lane; skip the building test */
    if (!player && c->ai == AI_TRAFFIC && dist_approx((c->x >> 8) - plx(), (c->y >> 8) - ply()) > M(80)) return;
    int hl, hw; dims_of(c->model, &hl, &hw);
    for (int it = 0; it < 2; it++) {
        int a = c->head >> 8, ca = icos(a), sa = isin(a);
        int x = c->x >> 8, y = c->y >> 8, nx, ny, qx, qy;
        int depth = car_vs_world(x, y, ca, sa, hl, hw, &nx, &ny, &qx, &qy);
        if (!depth) break;
        c->x += Q8((nx * (depth + 1)) >> 10); c->y += Q8((ny * (depth + 1)) >> 10);
        int vx, vy; car_vel(c, &vx, &vy);
        int vn = ((vx >> 2) * nx + (vy >> 2) * ny) >> 8;          /* q8 units/s, <0 = into the wall */
        if (vn >= 0) continue;
        int j = -vn * 13 / 10;                                    /* restitution 0.3 */
        int tvx = vx - ((vn >> 2) * nx >> 8), tvy = vy - ((vn >> 2) * ny >> 8);
        car_add_vel(c, ((j >> 2) * nx >> 8) - tvx / 6, ((j >> 2) * ny >> 8) - tvy / 6);
        add_spin(c, qx - x, qy - y, nx, ny, j / 2);
        { int v = -vn / 4096; if (v > 5) car_damage(c, (v - 5) * 7, player); }
        if (player) {
            int v = -vn / 4096; if (v > impact) impact = v;
            int a2 = c->head >> 8;
            c->speed = ((c->vx >> 4) * icos(a2) + (c->vy >> 4) * isin(a2)) >> 10;
        }
    }
}

/* ------------------------------------------------------------ damage and explosions */
typedef struct { int x, y; s16 t; } Boom;
#define MAX_BOOMS 6
#define BOOM_LEN (TICKS * 7 / 10)
EWRAM_BSS static Boom booms[MAX_BOOMS];
static int mission, mstate;
static int cut_on, cut_t;                     /* in-engine cutscene: letterbox + scripted camera */
static void eject_driver(Car *c);
static void kill_ped(Ped *p, int vx, int vy, int by_player, int fling);
static void player_hurt(int dmg);
static void crime(int level);
static void msg(const char *a, const char *b, int secs);

static void car_damage(Car *c, int d, int by_player)
{
    if (d <= 0 || IS_WRECK(c) || !c->active) return;
    if (c - cars == SLOT_MISSION && mission == 0 && mstate == 1) d >>= 1;     /* the supercar is tougher */
    int before = c->dmg;
    c->dmg += d; if (c->dmg > 1000) c->dmg = 1000;
    if (before < CAR_FIRE && c->dmg >= CAR_FIRE) {             /* catches fire */
        c->burn = c->dmg >= 1000 ? TICKS / 2 : 5 * TICKS;
        /* your own car always gives you time to bail out: a crash alone never kills you */
        if (is_player_car(c)) { if (c->burn < 4 * TICKS) c->burn = 4 * TICKS; msg("YOUR CAR IS ON FIRE!", "GET OUT! (R)", 3); }
        else if (c->ai == AI_TRAFFIC) { c->ai = AI_NONE; c->speed = 0; eject_driver(c); }
        else if (c->ai == AI_POLICE || c->ai == AI_RIVAL) { c->ai = AI_NONE; }
        if (by_player && c->police) crime(2);
    }
}

static void explode_at(int x, int y, Car *src)
{
    for (int i = 0; i < MAX_BOOMS; i++) if (booms[i].t <= 0 || booms[i].t >= BOOM_LEN) { booms[i].x = x; booms[i].y = y; booms[i].t = 1; break; }
    g_snd.shot = 5; g_snd.shot_vol = 64; g_snd.crash = 64;
    int d = dist_approx(x - plx(), y - ply());
    if (d < M(28)) g_hud.hurt_t = TICKS / 4;
    for (int i = 0; i < ncars; i++) {                          /* blast: nearby cars catch it too */
        Car *c = &cars[i];
        if (!c->active || c == src || IS_WRECK(c)) continue;
        int dd = dist_approx((c->x >> 8) - x, (c->y >> 8) - y);
        if (dd < M(9)) {
            car_damage(c, 700 - dd * 700 / M(9), 0);
            int dx = (c->x >> 8) - x, dy = (c->y >> 8) - y;
            car_add_vel(c, dx * 40, dy * 40);
        }
    }
    for (int i = 0; i < MAX_PEDS; i++) {
        Ped *p = &peds[i];
        if (!p->active || p->down) continue;
        int dx = (p->x >> 8) - x, dy = (p->y >> 8) - y;
        if (dist_approx(dx, dy) < M(7)) kill_ped(p, dx * 48, dy * 48, 0, 4);
    }
    if (pl.in_car < 0 && d < M(7)) player_hurt(90 - d * 90 / M(7));
}

static void car_explode(Car *c)
{
    int was_player = is_player_car(c);
    c->model = CAR_WRECK; c->ai = AI_NONE; c->police = 0; c->speed = 0; c->burn = 20 * TICKS;
    c->vx = c->vx / 3; c->vy = c->vy / 3; c->kx = c->ky = 0;
    explode_at(c->x >> 8, c->y >> 8, c);
    if (was_player) {                         /* still inside when it blows: that's fatal */
        int a = c->head >> 8;
        pl.in_car = -1;
        pl.x = c->x + Q8((isin(a) * M(2)) >> 14); pl.y = c->y - Q8((icos(a) * M(2)) >> 14); pl.head = c->head;
        player_hurt(1000);
    }
}

static void update_damage(int dt)
{
    for (int i = 0; i < ncars; i++) {
        Car *c = &cars[i];
        if (!c->active) continue;
        if (IS_WRECK(c)) { if (c->burn > 0) c->burn -= dt; continue; }
        if (c->dmg >= CAR_FIRE) { c->burn -= dt; if (c->burn <= 0) car_explode(c); }
    }
    for (int i = 0; i < MAX_BOOMS; i++) if (booms[i].t > 0 && booms[i].t < BOOM_LEN) booms[i].t += dt;
}

/* separating-axis test between two oriented boxes; resolves overlap and exchanges impulse */
static void collide_pair(Car *A, Car *B)
{
    int ahl, ahw, bhl, bhw;
    dims_of(A->model, &ahl, &ahw); dims_of(B->model, &bhl, &bhw);
    int ax = A->x >> 8, ay = A->y >> 8, bx = B->x >> 8, by = B->y >> 8;
    int dx = bx - ax, dy = by - ay;
    int reach = ahl + bhl + M(0.5);
    if (dx > reach || dx < -reach || dy > reach || dy < -reach) return;
    int aa = A->head >> 8, ba = B->head >> 8;
    int axes[4][2] = { { icos(aa), isin(aa) }, { isin(aa), -icos(aa) }, { icos(ba), isin(ba) }, { isin(ba), -icos(ba) } };
    int best = 0x7fffffff, bnx = 0, bny = 0;
    for (int k = 0; k < 4; k++) {
        int kx = axes[k][0], ky = axes[k][1];
        int ra = (ahl * iabs((axes[0][0] * kx + axes[0][1] * ky) >> 14) + ahw * iabs((axes[1][0] * kx + axes[1][1] * ky) >> 14)) >> 14;
        int rb = (bhl * iabs((axes[2][0] * kx + axes[2][1] * ky) >> 14) + bhw * iabs((axes[3][0] * kx + axes[3][1] * ky) >> 14)) >> 14;
        int d = (dx * kx + dy * ky) >> 14;
        int ov = ra + rb - iabs(d);
        if (ov <= 0) return;                                       /* separating axis found */
        if (ov < best) { best = ov; bnx = d < 0 ? -kx : kx; bny = d < 0 ? -ky : ky; }
    }
    /* contact point: A's corner furthest toward B and B's corner furthest toward A */
    int cax = ax, cay = ay, cbx = bx, cby = by, pa = -0x7fffffff, pb = 0x7fffffff;
    for (int q = 0; q < 4; q++) {
        int sf = (q & 1) ? 1 : -1, sr = (q & 2) ? 1 : -1;
        int px = ax + ((sf * ahl * axes[0][0] + sr * ahw * axes[1][0]) >> 14);
        int py = ay + ((sf * ahl * axes[0][1] + sr * ahw * axes[1][1]) >> 14);
        int pr = ((px - ax) * bnx + (py - ay) * bny) >> 14;
        if (pr > pa) { pa = pr; cax = px; cay = py; }
        px = bx + ((sf * bhl * axes[2][0] + sr * bhw * axes[3][0]) >> 14);
        py = by + ((sf * bhl * axes[2][1] + sr * bhw * axes[3][1]) >> 14);
        pr = ((px - ax) * bnx + (py - ay) * bny) >> 14;
        if (pr < pb) { pb = pr; cbx = px; cby = py; }
    }
    int cx = (cax + cbx) >> 1, cy = (cay + cby) >> 1;
    /* push apart (half each) */
    int half = (best >> 1) + 1;
    int nx10 = bnx >> 4, ny10 = bny >> 4;
    A->x -= Q8((nx10 * half) >> 10); A->y -= Q8((ny10 * half) >> 10);
    B->x += Q8((nx10 * half) >> 10); B->y += Q8((ny10 * half) >> 10);
    /* impulse along the normal (equal masses, restitution 0.25) */
    int avx, avy, bvx, bvy;
    car_vel(A, &avx, &avy); car_vel(B, &bvx, &bvy);
    int vn = (((bvx - avx) >> 4) * bnx + ((bvy - avy) >> 4) * bny) >> 10;
    if (vn >= 0) return;
    int j = (-vn * 5 / 4) >> 1;
    car_add_vel(A, -((j >> 2) * nx10 >> 8), -((j >> 2) * ny10 >> 8));
    car_add_vel(B, (j >> 2) * nx10 >> 8, (j >> 2) * ny10 >> 8);
    add_spin(A, cx - ax, cy - ay, -nx10, -ny10, j);
    add_spin(B, cx - bx, cy - by, nx10, ny10, j);
    int pa_ = is_player_car(A), pb_ = is_player_car(B);
    { int v = -vn / 4096; if (v > 4) { car_damage(A, (v - 4) * 6, pa_ || pb_); car_damage(B, (v - 4) * 6, pa_ || pb_); } }
    if (pa_ || pb_) {
        int v = -vn / 4096; if (v > impact) impact = v;
        Car *o = pa_ ? B : A;
        if (o->police && o->ai == AI_POLICE && wanted == 0) wanted = 1;
        Car *p = pa_ ? A : B;
        int a2 = p->head >> 8;
        p->speed = ((p->vx >> 4) * icos(a2) + (p->vy >> 4) * isin(a2)) >> 10;
    }
}

static void car_car_collisions(void)
{
    /* cheap bounding-square reject first: most pairs are far apart */
    EWRAM_BSS static int px[MAX_CARS], py[MAX_CARS], pr[MAX_CARS];
    for (int a = 0; a < n_near; a++) {
        const Car *c = &cars[near_cars[a]];
        int hl, hw; dims_of(c->model, &hl, &hw);
        px[a] = c->x >> 8; py[a] = c->y >> 8; pr[a] = hl + M(0.25);
    }
    for (int a = 0; a < n_near; a++)
        for (int b = a + 1; b < n_near; b++) {
            int reach = pr[a] + pr[b], dx = px[b] - px[a], dy = py[b] - py[a];
            if (dx > reach || dx < -reach || dy > reach || dy < -reach) continue;
            collide_pair(&cars[near_cars[a]], &cars[near_cars[b]]);
        }
}

/* is point (x,y) inside car c grown by 'pad'? returns push-out direction + car speed */
static int point_vs_car(const Car *c, int x, int y, int pad, int *ox, int *oy)
{
    int hl, hw; dims_of(c->model, &hl, &hw);
    hl += pad; hw += pad;
    int a = c->head >> 8, ca = icos(a), sa = isin(a);
    int dx = x - (c->x >> 8), dy = y - (c->y >> 8);
    if (iabs(dx) > hl + hw || iabs(dy) > hl + hw) return 0;
    int lf = (dx * ca + dy * sa) >> 14, lr = (dx * sa - dy * ca) >> 14;
    int pf = hl - iabs(lf), pr = hw - iabs(lr);
    if (pf <= 0 || pr <= 0) return 0;
    if (pf < pr) { int sg = lf < 0 ? -1 : 1; lf = sg * hl; }
    else { int sg = lr < 0 ? -1 : 1; lr = sg * hw; }
    *ox = (c->x >> 8) + ((lf * ca + lr * sa) >> 14);
    *oy = (c->y >> 8) + ((lf * sa - lr * ca) >> 14);
    return 1;
}

/* ------------------------------------------------------------ effects */
static void spray_blood(int x, int y, int vx, int vy)
{
    for (int k = 0; k < 10; k++) {
        int i;
        for (i = 0; i < MAX_DROPS && drops[i].life > 0; i++) ;
        if (i == MAX_DROPS) return;
        Drop *d = &drops[i];
        d->x = Q8(x); d->y = Q8(y); d->h = Q8(M(1.0));
        d->vx = vx / 3 + (int)(rnd() % (u32)Q8(M(6))) - Q8(M(3));
        d->vy = vy / 3 + (int)(rnd() % (u32)Q8(M(6))) - Q8(M(3));
        d->vh = Q8(M(2)) + (int)(rnd() % (u32)Q8(M(4)));
        d->life = 1;
    }
}

static void update_drops(int dt)
{
    for (int i = 0; i < MAX_DROPS; i++) {
        Drop *d = &drops[i];
        if (d->life <= 0) continue;
        d->x += (d->vx / 64) * dt / (TICKS / 64);
        d->y += (d->vy / 64) * dt / (TICKS / 64);
        d->h += (d->vh / 64) * dt / (TICKS / 64);
        d->vh -= Q8(M(9.8)) / 64 * dt / (TICKS / 64);
        if (d->h <= 0) d->life = 0;
    }
    for (int i = 0; i < MAX_FLASH; i++) if (flashes[i].t > 0) flashes[i].t -= dt >> 6;
}

static void muzzle(int x, int y, int head, int h)
{
    for (int i = 0; i < MAX_FLASH; i++) if (flashes[i].t <= 0) {
        flashes[i].x = x + ((icos(head) * M(0.65)) >> 14);
        flashes[i].y = y + ((isin(head) * M(0.65)) >> 14);
        flashes[i].h = h; flashes[i].t = 20;
        return;
    }
}

/* ------------------------------------------------------------ wanted */
static int cop_nearby(int range)
{
    for (int i = 0; i < MAX_PEDS; i++) {
        Ped *p = &peds[i];
        if (!p->active || p->down || p->type != P_COP) continue;
        if (dist_approx((p->x >> 8) - plx(), (p->y >> 8) - ply()) < range) return 1;
    }
    for (int i = 0; i < NPOLICE; i++) {
        Car *c = &cars[SLOT_POLICE0 + i];
        if (c->active && c->police && dist_approx((c->x >> 8) - plx(), (c->y >> 8) - ply()) < range) return 1;
    }
    return 0;
}

static void crime(int level)
{
    int nw = level;
    if (wanted >= level) nw = wanted + (cop_nearby(M(45)) ? 1 : 0);
    if (nw > 5) nw = 5;
    if (nw > wanted) { wanted = nw; wanted_cool = 0; }
}

/* ------------------------------------------------------------ pedestrians */
static void ped_corner(const Ped *p, int k, int *x, int *y)
{
    int ox = p->bx * 1024, oy = p->by * 1024;
    int a = M(13.5), b = M(62.5);
    switch (k & 3) {
    case 0: *x = ox + a; *y = oy + a; break;
    case 1: *x = ox + b; *y = oy + a; break;
    case 2: *x = ox + b; *y = oy + b; break;
    default:*x = ox + a; *y = oy + b; break;
    }
}

static const u16 civ_models[6] = { PED_GREY, PED_RED, PED_NAVY, PED_SUIT, PED_GREEN, PED_WHITE };

/* drop a pedestrian onto a sidewalk near the player but out of sight-range */
static void place_ped_near(Ped *p, int min_d)
{
    int pbx = plx() >> 10, pby = ply() >> 10;
    for (int tries = 0; tries < 10; tries++) {
        p->bx = (u8)clampi(pbx + rndr(-2, 2), 0, NB - 1); p->by = (u8)clampi(pby + rndr(-2, 2), 0, NB - 1);
        p->corner = rnd() & 3; p->cw = rnd() & 1;
        int x0, y0, x1, y1;
        ped_corner(p, p->corner, &x0, &y0);
        ped_corner(p, p->corner + (p->cw ? 1 : 3), &x1, &y1);
        int t = rndr(0, 255);
        p->x = Q8(x0 + (x1 - x0) * t / 256); p->y = Q8(y0 + (y1 - y0) * t / 256);
        p->corner = (p->corner + (p->cw ? 1 : 3)) & 3;
        p->head = Q8(iatan2(y1 - y0, x1 - x0));
        if (dist_approx((p->x >> 8) - plx(), (p->y >> 8) - ply()) > min_d) break;
    }
    p->down = 0; p->pool = 0; p->state = S_WALK; p->hp = 100; p->run = 0; p->aim = 0; p->active = 1; p->rag = 0;
}

static void kill_ped(Ped *p, int vx, int vy, int by_player, int fling_m)
{
    spray_blood(p->x >> 8, p->y >> 8, vx, vy);
    g_snd.sfx |= SFX_YELP;
    {   /* ragdoll: launched with the car's speed (or knocked back by a shot), tumbling */
        int sp = dist_approx(vx, vy); if (sp < 1) sp = 1;
        int hs, up, spin;
        if (fling_m > 0) {
            hs = sp * 9 / 10; if (hs > Q8(M(17))) hs = Q8(M(17));
            up = Q8(M(1.6)) + sp / 5; if (up > Q8(M(7.0))) up = Q8(M(7.0));
            spin = sp / 9; if (spin < 2600) spin = 2600; if (spin > 11000) spin = 11000;
        } else { hs = Q8(M(1.4)); up = Q8(M(1.1)); spin = 1500; }                    /* shot: stagger and fall */
        int a = iatan2(vy, vx);
        p->rvx = (icos(a) >> 6) * (hs >> 8); p->rvy = (isin(a) >> 6) * (hs >> 8);   /* (q14 >> 6) * units = q8 */
        p->rvz = up; p->rh = Q8(M(0.05)); p->rang = 0; p->rspin = (s16)spin; p->rag = 1; p->rbounce = 0;
    }
    p->head = Q8(iatan2(vy, vx));
    p->down = 1; p->pool = 0; p->timer = 30 * 256; p->run = 0; p->aim = 0;
    g_snd.hit = 1;
    if (by_player) {
        if (p->type == P_COP) crime(wanted + 2 > 2 ? wanted + 2 : 2);
        else if (p->type == P_CIV) crime(1);
    }
}

/* ragdoll step: gravity, bounces off the road with damping, rebounds off walls, slides to rest
   and settles flat, then hands over to the lying-down pose */
static void update_ragdoll(Ped *p, int dt)
{
    const int G = Q8(M(9.8));
    int ox = p->x >> 8, oy = p->y >> 8;
    p->x += (p->rvx >> 6) * dt / (TICKS >> 6);
    p->y += (p->rvy >> 6) * dt / (TICKS >> 6);
    {   /* walls: push out and bounce back */
        int x = p->x >> 8, y = p->y >> 8;
        if (world_collide(&x, &y, M(0.35))) {
            if (x != (p->x >> 8)) p->rvx = -p->rvx * 4 / 10;
            if (y != (p->y >> 8)) p->rvy = -p->rvy * 4 / 10;
            p->x = Q8(x); p->y = Q8(y);
            p->rspin = (s16)(-p->rspin / 2);
            g_snd.hit = 1;
        }
    }
    (void)ox; (void)oy;
    p->rvz -= (G >> 6) * dt / (TICKS >> 6);
    p->rh += (p->rvz >> 6) * dt / (TICKS >> 6);
    if (p->rh <= 0) {
        p->rh = 0;
        if (p->rvz < -Q8(M(1.8)) && p->rbounce < 4) {        /* bounce, losing energy */
            p->rvz = -p->rvz * 35 / 100;
            p->rvx = p->rvx * 6 / 10; p->rvy = p->rvy * 6 / 10;
            p->rspin = (s16)(p->rspin * 6 / 10);
            p->rbounce++;
            g_snd.hit = 1;
        } else {                                               /* sliding along the road */
            p->rvz = 0;
            int sp = dist_approx(p->rvx >> 8, p->rvy >> 8);   /* units/s */
            int dec = M(10) * dt / TICKS + 1;
            if (sp <= dec) p->rvx = p->rvy = 0;
            else { p->rvx = p->rvx / sp * (sp - dec); p->rvy = p->rvy / sp * (sp - dec); }
            /* roll over to lie flat */
            int a = p->rang & 4095, target = (a < 2048) ? 1024 : 3072;
            int step = 5000 * dt / TICKS + 1, d = target - a;
            if (d > step) d = step; else if (d < -step) d = -step;
            p->rang = (u16)((a + d) & 4095); p->rspin = 0;
            if (!p->rvx && !p->rvy && p->rang == target) {     /* at rest: lie down for good */
                p->rag = 0; p->timer = 30 * 256; p->pool = 0;
                return;
            }
        }
    }
    p->rang = (u16)((p->rang + p->rspin * dt / TICKS) & 4095);
    if (p->rvx || p->rvy) p->head = Q8(iatan2(p->rvy, p->rvx));
}

static void panic(int x, int y, int range)
{
    for (int i = 0; i < MAX_PEDS; i++) {
        Ped *p = &peds[i];
        if (!p->active || p->down || p->type != P_CIV) continue;
        if (dist_approx((p->x >> 8) - x, (p->y >> 8) - y) > range) continue;
        p->state = S_FLEE; p->timer = 8 * 256; p->fx = x; p->fy = y;
    }
}

static void ped_move(Ped *p, int head_target, int speed, int dt, int collide)
{
    int diff = angdiff(head_target, p->head >> 8);
    int mt = 1000 * dt / TICKS + 1;              /* turn rate: ~100 degrees per second, no snapping */
    if (diff > mt) diff = mt; else if (diff < -mt) diff = -mt;
    p->head = (p->head + Q8(diff)) & (Q8(4096) - 1);
    int dist = (speed / 64) * dt / (TICKS / 64);
    int a = p->head >> 8;
    p->x += (dist * icos(a)) >> 14;
    p->y += (dist * isin(a)) >> 14;
    p->run = speed > Q8(M(3));
    p->phase += dist * 512 / (p->run ? Q8(M(2.6)) : Q8(M(1.4)));
    if (collide) {
        int x = p->x >> 8, y = p->y >> 8;
        if (world_collide(&x, &y, M(0.35))) { p->x = Q8(x); p->y = Q8(y); }
    }
}

static void player_hurt(int dmg);

static void ped_shoot_at_player(Ped *p, int dt, int d)
{
    p->aim = 1;
    int want = iatan2(ply() - (p->y >> 8), plx() - (p->x >> 8));
    p->head = Q8(want);
    p->cd -= dt >> 6;
    if (p->cd <= 0) {
        p->cd = 230 + (int)(rnd() % 80);
        muzzle(p->x >> 8, p->y >> 8, want, M(1.35));
        g_snd.shot = 1; g_snd.shot_vol = d < M(8) ? 56 : d < M(20) ? 40 : 26;
        int chance = d < M(10) ? 55 : 35;
        if (pl.in_car >= 0) chance -= 15;
        if ((int)(rnd() % 100) < chance) { if (pl.in_car >= 0) { car_damage(&cars[pl.in_car], 28, 0); player_hurt(2); } else player_hurt(7); }
    }
}

static void update_peds(int dt0)
{
    for (int i = 0; i < MAX_PEDS; i++) {
        Ped *p = &peds[i];
        if (!p->active) continue;
        int x = p->x >> 8, y = p->y >> 8;
        int dpl = dist_approx(x - plx(), y - ply());
        int dt = dt0;
        if (dpl > M(45) && p->state == S_WALK && !p->down) {      /* distant walkers: quarter rate */
            if (((frame_no + i) & 3) != 0) continue;
            dt = dt0 * 4;
        }
        if (p->down && p->rag) {             /* ragdoll flight, bounces and slide */
            update_ragdoll(p, dt0);
            continue;
        }
        if (p->down) {
            p->timer -= dt >> 6;
            int age = 30 * 256 - p->timer;
            p->pool = (u8)(age < 256 ? 0 : age < 2 * 256 ? 1 : age < 4 * 256 ? 2 : 3);
            if (p->timer <= 0 || (dpl > M(110) && age > 512)) {
                if (i < PED_CIV_END) place_ped_near(p, M(50)); else p->active = 0;
            }
            continue;
        }
        p->aim = 0;
        if (i == PED_ACTOR && !cut_on && dpl > M(60)) { p->active = 0; continue; }
        /* keep the street population around the player */
        if (i < PED_CIV_END && p->state != S_CHASE && dpl > M(95)) { place_ped_near(p, M(50)); continue; }
        switch (p->state) {
        case S_WALK: {
            int tx, ty; ped_corner(p, p->corner, &tx, &ty);
            int dx = tx - x, dy = ty - y;
            if (dx * dx + dy * dy < M(1) * M(1)) { p->corner = (p->corner + (p->cw ? 1 : 3)) & 3; break; }
            ped_move(p, iatan2(dy, dx), p->speed, dt, 0);
            if (p->type == P_COP && wanted > 0 && dpl < M(55)) p->state = S_CHASE;
            if (p->type == P_GUARD && dpl < M(24)) p->state = S_CHASE;
            if (p->type == P_TARGET && dpl < M(12)) { p->state = S_FLEE; p->timer = 20 * 256; p->fx = plx(); p->fy = ply(); }
            break; }
        case S_IDLE:
            if (p->type == P_GUARD && dpl < M(24)) p->state = S_CHASE;
            if (p->type == P_TARGET && dpl < M(12)) { p->state = S_FLEE; p->timer = 20 * 256; p->fx = plx(); p->fy = ply(); }
            break;
        case S_FLEE: {
            p->timer -= dt >> 6;
            int away = iatan2(y - p->fy, x - p->fx);
            ped_move(p, away, Q8(M(5.2)), dt, 1);
            if (p->timer <= 0) {
                p->state = (p->type == P_TARGET) ? S_IDLE : S_WALK;
                /* re-route on the nearest block */
                p->bx = (u8)clampi((x - M(13)) >> 10, 0, NB - 1); p->by = (u8)clampi((y - M(13)) >> 10, 0, NB - 1);
            }
            break; }
        case S_CHASE: {
            int armed = (p->type == P_GUARD) || (p->type == P_COP && wanted >= 2);
            if (p->type == P_COP && wanted == 0) { p->state = S_WALK; break; }
            if (pl.dead_t > 0) { p->state = S_IDLE; break; }
            if (armed && dpl < M(18) && dpl > M(3)) { ped_shoot_at_player(p, dt, dpl); break; }
            if (dpl > M(1.4)) ped_move(p, iatan2(ply() - y, plx() - x), Q8(M(5.0)), dt, 1);
            if (p->type == P_COP && wanted == 1 && dpl < M(1.8) && pl.in_car < 0) pl.arrest_t += dt;
            if (p->type == P_GUARD && dpl > M(40)) p->state = S_IDLE;
            if (i >= PED_CREW0 && i < PED_MISSION0 && dpl > M(120)) p->active = 0;
            break; }
        }
        /* cars: slow ones shove pedestrians aside, fast ones knock them down */
        if (dpl > M(70)) continue;
        x = p->x >> 8; y = p->y >> 8;
        for (int k = 0; k < n_near; k++) {
            int c = near_cars[k];
            int ox, oy;
            int qx = (cars[c].x >> 8) - x, qy = (cars[c].y >> 8) - y;
            if (qx > M(4.5) || qx < -M(4.5) || qy > M(4.5) || qy < -M(4.5)) continue;   /* far apart: skip the full test */
            if (!point_vs_car(&cars[c], x, y, M(0.3), &ox, &oy)) continue;
            int vx, vy; car_vel(&cars[c], &vx, &vy);
            int sp = dist_approx(vx, vy);
            if (sp > Q8(M(4))) {
                int fling = sp / 4096 / 3 + 1; if (fling > 6) fling = 6;
                kill_ped(p, vx, vy, pl.in_car == c, fling);
                g_snd.crash = 26;
                if (pl.in_car == c) { cars[c].vx = cars[c].vx * 7 / 8; cars[c].vy = cars[c].vy * 7 / 8; }
                break;
            }
            p->x = Q8(ox); p->y = Q8(oy);
        }
    }
    update_drops(dt0);
}

static void crew_exit(Car *c)
{
    c->crew = 0; c->ai = AI_NONE; c->speed = 0;
    int a = c->head >> 8;
    for (int k = 0; k < 2; k++) {
        Ped *p = &peds[PED_CREW0 + k + (peds[PED_CREW0].active ? 1 : 0)];
        if (p->active) continue;
        int side = k ? 1 : -1;
        p->x = c->x + Q8(side * ((-isin(a) * M(1.7)) >> 14));
        p->y = c->y + Q8(side * ((icos(a) * M(1.7)) >> 14));
        p->model = PED_COP; p->type = P_COP; p->state = S_CHASE; p->active = 1; p->down = 0; p->rag = 0;
        p->hp = 100; p->cd = 128; p->speed = Q8(M(5)); p->head = c->head;
    }
}

/* ------------------------------------------------------------ player */
static void player_hurt(int dmg)
{
    if (pl.dead_t > 0) return;
    pl.hp -= dmg;
    g_hud.hurt_t = TICKS / 6;
    if (pl.hp <= 0) {
        pl.hp = 0; pl.dead_t = 3 * TICKS; pl.dead_kind = 0;
        big("WASTED", 0, 3); g_snd.sfx |= SFX_FAIL;
        if (pl.in_car < 0) {
            Ped fake; fake.x = pl.x; fake.y = pl.y;
            spray_blood(plx(), ply(), 0, 0); (void)fake;
        }
    }
}

static void walk(u16 keys, int dt)
{
    int turn = (keys & KEY_LEFT) ? 1 : (keys & KEY_RIGHT) ? -1 : 0;
    pl.head = (pl.head + turn * Q8(900) * dt / TICKS) & (Q8(4096) - 1);
    int sp = 0;
    if (keys & KEY_UP) sp = (keys & KEY_B) ? Q8(M(5.5)) : Q8(M(1.9));
    else if (keys & KEY_DOWN) sp = -Q8(M(1.2));
    pl.moving = sp != 0;
    pl.running = sp > Q8(M(3));
    int dist = (sp / 64) * dt / (TICKS / 64);
    int a = pl.head >> 8;
    pl.x += (dist * icos(a)) >> 14;
    pl.y += (dist * isin(a)) >> 14;
    int adist = iabs(dist);
    if (sp >= 0) pl.phase += adist * 512 / (pl.running ? Q8(M(2.6)) : Q8(M(1.4)));
    else pl.phase -= adist * 512 / Q8(M(1.2));
    int x = plx(), y = ply();
    if (world_collide(&x, &y, M(0.4))) { pl.x = Q8(x); pl.y = Q8(y); }
    if (pl.hurt_cd > 0) pl.hurt_cd -= dt;
    for (int k = 0; k < n_near; k++) {
        Car *c = &cars[near_cars[k]];
        int ox, oy;
        if (!point_vs_car(c, plx(), ply(), M(0.4), &ox, &oy)) continue;
        pl.x = Q8(ox); pl.y = Q8(oy);
        int vx, vy; car_vel(c, &vx, &vy);
        if (dist_approx(vx, vy) > Q8(M(6)) && pl.hurt_cd <= 0) { player_hurt(20); pl.hurt_cd = TICKS; g_snd.hit = 1; }
    }
}


/* weapons: 0 fists, 1 pistol, 2 SMG, 3 shotgun, 4 rifle */
#define NWEAP 6
#define W_ROCKET 5
static const char *const weapon_names[NWEAP] = { "FISTS", "PISTOL", "SMG", "SHOTGUN", "RIFLE", "ROCKETS" };
static const s16 w_rate[NWEAP]  = { 0, TICKS * 3 / 10, TICKS / 11, TICKS * 9 / 10, TICKS * 2 / 5, TICKS * 6 / 5 };
static const s16 w_range[NWEAP] = { 0, M(32), M(26), M(15), M(55), M(80) };
static const u8 w_dmg[NWEAP]    = { 0, 50, 22, 70, 100, 0 };
static const u8 w_auto[NWEAP]   = { 0, 0, 1, 0, 0, 0 };

/* rockets: real projectiles. A direct hit sets the car ablaze at once; the blast does the rest */
#define MAX_ROCKETS 3
typedef struct { int x, y, vx, vy; s16 life, h; u8 active; } Rocket;      /* x,y q8 units; v q8 units/s */
EWRAM_BSS static Rocket rockets[MAX_ROCKETS];
static int point_vs_car(const Car *c, int x, int y, int pad, int *ox, int *oy);
static void rocket_fire(int head)
{
    for (int i = 0; i < MAX_ROCKETS; i++) {
        Rocket *r = &rockets[i];
        if (r->active) continue;
        int c = icos(head), sn = isin(head);
        r->x = pl.x + Q8((c * M(1.2)) >> 14); r->y = pl.y + Q8((sn * M(1.2)) >> 14);
        r->vx = (c * M(42)) >> 6; r->vy = (sn * M(42)) >> 6;              /* 42 m/s, q8 (no int overflow) */
        r->life = (s16)(TICKS * 2 >> 6); r->h = M(1.3); r->active = 1;
        return;
    }
}
static void car_damage(Car *c, int d, int by_player);
static void explode_at(int x, int y, Car *src);
static void crime(int level);
static int cop_nearby(int range);
static void update_rockets(int dt)
{
    for (int i = 0; i < MAX_ROCKETS; i++) {
        Rocket *r = &rockets[i];
        if (!r->active) continue;
        r->life -= dt >> 6;
        /* move in steps of about 0.6 m so it can't skip through a car */
        int dist = dist_approx(r->vx >> 8, r->vy >> 8) * dt / TICKS;          /* units this frame */
        int n = dist / M(0.6) + 1, hit = r->life <= 0;
        Car *hc = 0;
        for (int k = 0; k < n && !hit; k++) {
            r->x += (r->vx >> 6) * dt / (TICKS >> 6) / n; r->y += (r->vy >> 6) * dt / (TICKS >> 6) / n;
            int x = r->x >> 8, y = r->y >> 8;
            if (world_point_blocked(x, y, M(0.2))) { hit = 1; break; }
            for (int j = 0; j < ncars; j++) {
                Car *c = &cars[j];
                if (!c->active || j == pl.in_car) continue;
                int ox, oy;
                if (point_vs_car(c, x, y, M(0.2), &ox, &oy)) { hit = 1; hc = c; break; }
            }
            for (int j = 0; j < MAX_PEDS && !hit; j++) {
                const Ped *p = &peds[j];
                if (p->active && !p->down && dist_approx((p->x >> 8) - x, (p->y >> 8) - y) < M(0.7)) hit = 1;
            }
        }
        if (!hit) continue;
        r->active = 0;
        if (hc && !IS_WRECK(hc)) car_damage(hc, 1000, 1);          /* direct hit: burning, about to go up */
        explode_at(r->x >> 8, r->y >> 8, hc);
        if (cop_nearby(M(45))) crime(2);
    }
}

static void hit_ped(Ped *p, int dmg, int aimang)
{
    p->hp -= dmg;
    int vx = (icos(aimang) >> 4) * 256, vy = (isin(aimang) >> 4) * 256;
    if (dmg >= 70) { vx += vx >> 1; vy += vy >> 1; }
    if (p->hp <= 0) kill_ped(p, vx, vy, 1, 0);
    else {
        spray_blood(p->x >> 8, p->y >> 8, vx / 4, vy / 4);
        if (p->type == P_CIV) { p->state = S_FLEE; p->timer = 10 * 256; p->fx = plx(); p->fy = ply(); }
        else if (p->type == P_COP) { p->state = S_CHASE; crime(2); }
        else if (p->type == P_TARGET) { p->state = S_FLEE; p->timer = 20 * 256; p->fx = plx(); p->fy = ply(); }
        else p->state = S_CHASE;
    }
}

static void player_fire(u16 keys, u16 pressed, int dt)
{
    if (pl.fire_cd > 0) pl.fire_cd -= dt;
    if (pl.aim_t > 0) pl.aim_t -= dt;
    if (pressed & KEY_L) {                      /* cycle owned weapons */
        for (int k = 1; k <= NWEAP; k++) {
            int w = (pl.weapon + k) % NWEAP;
            if (w == 0 || (((pl.owned >> (w - 1)) & 1) && pl.ammo[w] > 0)) { if (w != pl.weapon) g_snd.sfx |= SFX_SWAP; pl.weapon = (u8)w; break; }
        }
    }
    int w = pl.weapon;
    if (w == 0 || !(keys & KEY_A) || pl.fire_cd > 0) return;
    if (!w_auto[w] && !(pressed & KEY_A)) return;          /* semi-auto: one shot per press */
    if (pl.ammo[w] <= 0) { pl.weapon = 0; g_snd.sfx |= SFX_DRY; return; }
    pl.fire_cd = w_rate[w];
    pl.ammo[w]--;
    pl.aim_t = TICKS / 2;
    if (w == W_ROCKET) {                                        /* aim at the nearest car ahead, else straight on */
        int ph = pl.head >> 8, aim = ph, bd = M(80);
        for (int i = 0; i < ncars; i++) {
            const Car *c = &cars[i];
            if (!c->active || IS_WRECK(c)) continue;
            int dx = (c->x >> 8) - plx(), dy = (c->y >> 8) - ply(), d = dist_approx(dx, dy);
            if (d >= bd || d < M(3)) continue;
            int a = iatan2(dy, dx);
            if (iabs(angdiff(a, ph)) > 400) continue;
            bd = d; aim = a;
        }
        pl.head = Q8(aim);
        rocket_fire(aim);
        muzzle(plx(), ply(), aim, M(1.35));
        g_snd.shot = 6; g_snd.shot_vol = 64; g_snd.sfx |= SFX_LAUNCH;
        panic(plx(), ply(), M(50));
        return;
    }
    int range = w_range[w];
    int ph = pl.head >> 8;
    /* candidates in front, best first (auto-aim) */
    int cand[3] = { -1, -1, -1 }, cs[3] = { 0x7fffffff, 0x7fffffff, 0x7fffffff };
    int cone = w == 3 ? 380 : 560;
    for (int i = 0; i < MAX_PEDS; i++) {
        Ped *p = &peds[i];
        if (!p->active || p->down) continue;
        int dx = (p->x >> 8) - plx(), dy = (p->y >> 8) - ply();
        int d = dist_approx(dx, dy);
        if (d > range) continue;
        int ad = iabs(angdiff(iatan2(dy, dx), ph));
        if (ad > cone) continue;
        int score = d + ad * 4;
        for (int k = 0; k < 3; k++) if (score < cs[k]) {
            for (int m = 2; m > k; m--) { cs[m] = cs[m - 1]; cand[m] = cand[m - 1]; }
            cs[k] = score; cand[k] = i; break;
        }
    }
    int aimang = ph;
    if (cand[0] >= 0) {
        Ped *p = &peds[cand[0]];
        aimang = iatan2((p->y >> 8) - ply(), (p->x >> 8) - plx());
        pl.head = Q8(aimang);
        int nt = w == 3 ? 3 : 1;                       /* the shotgun's spread can drop a small group */
        for (int k = 0; k < nt && cand[k] >= 0; k++) {
            Ped *q = &peds[cand[k]];
            int d = dist_approx((q->x >> 8) - plx(), (q->y >> 8) - ply());
            int chance = w == 4 ? 95 : d < M(12) ? 90 : 65;
            int dmg = w_dmg[w];
            if (w == 3) { dmg = d < M(6) ? 100 : d < M(10) ? 70 : 40; if (k) chance = 70; }
            if ((int)(rnd() % 100) < chance) hit_ped(q, dmg, aimang);
        }
    }
    else {                                              /* nobody in the sights: shoot the nearest car ahead */
        static const s16 car_dmg[NWEAP] = { 0, 70, 30, 160, 130, 0 };
        int bc = -1, bd = 0x7fffffff;
        for (int i = 0; i < ncars; i++) {
            Car *c = &cars[i];
            if (!c->active || IS_WRECK(c)) continue;
            int dx = (c->x >> 8) - plx(), dy = (c->y >> 8) - ply();
            int d = dist_approx(dx, dy);
            if (d > range || d < M(1.5)) continue;
            if (iabs(angdiff(iatan2(dy, dx), ph)) > 300) continue;
            if (d < bd) { bd = d; bc = i; }
        }
        if (bc >= 0) {
            Car *c = &cars[bc];
            aimang = iatan2((c->y >> 8) - ply(), (c->x >> 8) - plx());
            pl.head = Q8(aimang);
            car_damage(c, car_dmg[w], 1);
            if (c->police) crime(2); else if (c->ai == AI_TRAFFIC && cop_nearby(M(40))) crime(1);
            g_snd.hit = 1; g_snd.sfx |= SFX_RICOCHET;              /* bullet on sheet metal */
        } else if ((rnd() & 3) == 0) g_snd.sfx |= SFX_RICOCHET;   /* a miss whines off a wall */
    }
    muzzle(plx(), ply(), aimang, M(1.35));
    g_snd.shot = w; g_snd.shot_vol = 64;
    if (w == 3) g_snd.sfx |= SFX_PUMP; else if (w == 4) g_snd.sfx |= SFX_BOLT; else if (w == 1) g_snd.sfx |= SFX_SHELL;
    panic(plx(), ply(), w == 1 ? M(35) : M(50));
    if (cop_nearby(M(40))) crime(1);
}

/* a carjacked driver is pulled out on the driver's side and runs (or sometimes squares up) */
static void eject_driver(Car *c)
{
    int slot = -1, far = -1;
    for (int i = 0; i < PED_CIV_END; i++) {
        if (!peds[i].active) { slot = i; break; }
        int d = dist_approx((peds[i].x >> 8) - plx(), (peds[i].y >> 8) - ply());
        if (d > far) { far = d; slot = i; }
    }
    if (slot < 0) return;
    Ped *p = &peds[slot];
    int a = c->head >> 8;
    int ex = (c->x >> 8) + ((isin(a) * M(1.9)) >> 14), ey = (c->y >> 8) - ((icos(a) * M(1.9)) >> 14);
    if (world_point_blocked(ex, ey, M(0.4))) { ex = (c->x >> 8) - ((isin(a) * M(1.9)) >> 14); ey = (c->y >> 8) + ((icos(a) * M(1.9)) >> 14); }
    p->x = Q8(ex); p->y = Q8(ey); p->head = Q8(a + 1024);
    p->active = 1; p->down = 0; p->hp = 100; p->cd = 0; p->rag = 0;
    p->type = P_CIV; p->model = civ_models[rnd() % 6];
    p->speed = Q8(M(1.3)); p->phase = rnd() & 511;
    p->bx = ex >> 10; p->by = ey >> 10;
    p->state = S_FLEE; p->timer = 8 * 256; p->fx = plx(); p->fy = ply();
    static const char *const shouts[4] = { "HEY! THAT'S MY CAR!", "NOT AGAIN!", "SOMEBODY CALL THE COPS!", "TAKE IT, JUST DON'T HURT ME!" };
    msg(shouts[rnd() & 3], "", 2);
}

static void toggle_car(void)
{
    if (pl.in_car >= 0) {
        Car *c = &cars[pl.in_car];
        if ((c->speed > Q8(M(3)) || c->speed < -Q8(M(3))) && c->dmg < CAR_FIRE) return;   /* bail out of a burning car at any speed */
        int a = c->head >> 8;
        for (int side = 1; side >= -1; side -= 2) {
            int ex = (c->x >> 8) + side * ((-isin(a) * M(2.0)) >> 14);
            int ey = (c->y >> 8) + side * ((icos(a) * M(2.0)) >> 14);
            if (!world_point_blocked(ex, ey, M(0.4))) {
                c->speed = 0; c->ai = AI_NONE; c->vx = c->vy = 0;
                pl.x = Q8(ex); pl.y = Q8(ey); pl.head = c->head;
                pl.in_car = -1;
                g_snd.door = 1;
                return;
            }
        }
    } else {
        int best = -1, bd = M(3.5) * M(3.5);
        for (int i = 0; i < ncars; i++) {
            if (!cars[i].active || IS_WRECK(&cars[i])) continue;
            int dx = (cars[i].x - pl.x) >> 8, dy = (cars[i].y - pl.y) >> 8;
            int d2 = dx * dx + dy * dy;
            if (d2 < bd) { bd = d2; best = i; }
        }
        if (best >= 0) {
            Car *c = &cars[best];
            if (c->ai == AI_TRAFFIC && cop_nearby(M(30))) crime(1);     /* carjacking in front of cops */
            if (c->police) crime(2);
            if (c->ai == AI_TRAFFIC || c->ai == AI_RIVAL) eject_driver(c);
            pl.in_car = best; c->ai = AI_NONE; c->speed = 0; c->vx = c->kx; c->vy = c->ky; c->kx = c->ky = 0; c->crew = 0;
            g_snd.door = 1; g_snd.sfx |= SFX_START;
            radio_show();
        }
    }
}


/* ------------------------------------------------------------ cats */
/* A few street cats near the player. They live on the sidewalk loop around a block (like the
   pedestrians, so they never walk into walls): wander, sit, bolt from runners and cars, and
   meow when you walk up to them. Nothing ever hurts them. */
#define MAX_CATS 5
#define CAT_A M(14.3)                    /* sidewalk loop: square from (A, A) to (B, B) in the block */
#define CAT_B M(61.7)
#define CAT_SIDE (CAT_B - CAT_A)
enum { CAT_WALK, CAT_SIT, CAT_RUN };
typedef struct { int s; int x, y, head, phase; s16 timer, meow_cd; u8 active, bx, by, state, coat; s8 dir; } StreetCat;
EWRAM_BSS static StreetCat street_cats[MAX_CATS];

static void cat_place(StreetCat *c)
{
    int s = c->s >> 8, side = s / CAT_SIDE, t = s - side * CAT_SIDE;
    int ox = c->bx * 1024, oy = c->by * 1024, x, y, h;
    switch (side & 3) {
    case 0: x = CAT_A + t; y = CAT_A; h = 0; break;
    case 1: x = CAT_B; y = CAT_A + t; h = 1024; break;
    case 2: x = CAT_B - t; y = CAT_B; h = 2048; break;
    default: x = CAT_A; y = CAT_B - t; h = 3072; break;
    }
    c->x = ox + x; c->y = oy + y;
    if (c->state != CAT_SIT) c->head = c->dir > 0 ? h : (h + 2048) & 4095;
}

static void cat_spawn(StreetCat *c)
{
    int pbx = plx() >> 10, pby = ply() >> 10;
    for (int tries = 0; tries < 8; tries++) {
        c->bx = (u8)clampi(pbx + (int)(rnd() % 3) - 1, 0, NB - 1); c->by = (u8)clampi(pby + (int)(rnd() % 3) - 1, 0, NB - 1);
        c->s = (int)(rnd() % (4 * CAT_SIDE)) << 8;
        c->dir = (rnd() & 1) ? 1 : -1; c->state = CAT_SIT; c->timer = (s16)(rnd() % 6 + 2) * 64;
        c->head = (int)(rnd() & 4095);
        cat_place(c);
        if (dist_approx(c->x - plx(), c->y - ply()) > M(22)) break;
    }
    c->coat = (u8)(rnd() % 3); c->phase = 0; c->meow_cd = 0; c->active = 1;
}

static void update_cats(int dt)
{
    const int in_town = plx() > 0 && ply() > 0 && plx() < NB * 1024 && ply() < NB * 1024;
    for (int i = 0; i < MAX_CATS; i++) {
        StreetCat *c = &street_cats[i];
        if (!c->active || dist_approx(c->x - plx(), c->y - ply()) > M(90)) {
            if (in_town) cat_spawn(c); else c->active = 0;
            continue;
        }
        int ticks = dt >> 8;                                    /* timers count 1/64 s */
        if (c->meow_cd > 0) c->meow_cd -= ticks;
        /* threats: a car moving nearby, or the player running at it */
        int pd = dist_approx(c->x - plx(), c->y - ply());
        int threat = 0, tx = 0, ty = 0;
        if (pl.in_car < 0 && pd < M(4) && pl.running) { threat = 1; tx = plx(); ty = ply(); }
        for (int k = 0; k < n_near && !threat; k++) {
            const Car *v = &cars[near_cars[k]];
            int vx, vy; car_vel(v, &vx, &vy);
            if (dist_approx(vx >> 8, vy >> 8) < M(2)) continue;
            if (dist_approx((v->x >> 8) - c->x, (v->y >> 8) - c->y) < M(8)) { threat = 1; tx = v->x >> 8; ty = v->y >> 8; }
        }
        if (threat && c->state != CAT_RUN) {
            /* bolt along the sidewalk, whichever way leads away */
            int s0 = c->s, best = 1, bd = -1;
            for (int d = -1; d <= 1; d += 2) {
                c->s = s0 + d * Q8(M(2)); c->dir = (s8)d; cat_place(c);
                int dd = dist_approx(c->x - tx, c->y - ty);
                if (dd > bd) { bd = dd; best = d; }
            }
            c->s = s0; c->dir = (s8)best; c->state = CAT_RUN; c->timer = 100 + (int)(rnd() % 60);
        } else if (!threat && pl.in_car < 0 && pd < M(2.5) && !pl.running && c->state != CAT_RUN) {
            if (c->meow_cd <= 0) { g_snd.sfx |= SFX_MEOW; c->meow_cd = 64 * 6; }   /* friendly mew, then sits and looks at you */
            c->state = CAT_SIT; c->timer = 64 * 3;
            c->head = iatan2(ply() - c->y, plx() - c->x);
        }
        c->timer -= ticks;
        if (c->timer <= 0) {
            if (c->state == CAT_SIT) { c->state = CAT_WALK; c->dir = (rnd() & 1) ? 1 : -1; c->timer = (s16)(64 * (3 + rnd() % 6)); }
            else { c->state = CAT_SIT; c->timer = (s16)(64 * (2 + rnd() % 7)); }
        }
        if (c->state != CAT_SIT) {
            int speed = c->state == CAT_RUN ? Q8(M(4.5)) : Q8(M(0.6));      /* q8 units per second */
            int step = (speed >> 6) * dt / (TICKS >> 6);
            c->s += c->dir * step;
            const int loop = Q8(4 * CAT_SIDE);
            if (c->s < 0) c->s += loop; else if (c->s >= loop) c->s -= loop;
            c->phase += step;
            cat_place(c);
        }
    }
}

/* ------------------------------------------------------------ pickups */
static void setup_pickups(void)
{
    /* block x, block y, type (0 pistol 1 SMG 2 health 3 shotgun 4 rifle); 9,y = out of town */
    static const s16 pos[NFIXED_PICKUPS][3] = {
        {3,3,0}, {5,4,2}, {2,5,1}, {4,6,0}, {6,3,1}, {1,2,2}, {6,6,2}, {2,2,0}, {5,1,1}, {3,6,2},
        {0,0,3}, {7,7,3}, {4,1,3}, {1,6,4}, {6,0,4}, {7,4,0}, {0,4,1}, {3,7,1}, {7,1,2}, {4,4,4},
        {0,7,0}, {5,6,3}, {2,0,2}, {6,5,0},
        /* v0.20: a second set, mostly guns; these sit on the far side of their block */
        {1,5,5}, {6,2,5}, {3,0,5}, {5,7,5},      /* rocket launchers; the first is a block from the Kings' corner */
        {6,4,0}, {3,1,1}, {0,1,3}, {4,5,4}, {5,7,0}, {2,1,1}, {3,2,3}, {1,3,4}, {0,6,1}, {1,0,4}, {4,2,0}, {6,7,3}, {5,5,1}, {7,6,4}, {1,4,3}, {1,5,0}, {4,0,1}, {3,4,4}, {4,3,2}, {7,3,2}, {2,7,0}, {3,5,3}, {2,4,1}, {4,7,2} };
    for (int i = 0; i < NFIXED_PICKUPS; i++) {
        int x = pos[i][0] * 1024 + M(13.5), y = pos[i][1] * 1024 + M(30 + (i * 7) % 25);
        if (i >= 24) { x = pos[i][0] * 1024 + M(28 + (i * 11) % 24); y = pos[i][1] * 1024 + M(13.5); }   /* the cross street's sidewalk */
        for (int k = 0; k < 8 && world_point_blocked(x, y, M(0.6)); k++) { x += M(2); y += M(2); }   /* never inside a wall */
        pickups[i].x = x; pickups[i].y = y;
        pickups[i].type = (u8)pos[i][2]; pickups[i].avail = 1; pickups[i].timer = 0;
    }
    /* the rest of the city: about one block in three outside the Docks, weapons to suit the district */
    static const u8 dist_types[5][8] = {
        { 0, 1, 2, 0, 3, 2, 4, 1 },     /* docks (unused) */
        { 1, 2, 1, 0, 2, 4, 1, 3 },     /* downtown: SMGs and health */
        { 3, 4, 3, 2, 4, 1, 5, 3 },     /* industrial: shotguns and rifles */
        { 0, 2, 0, 2, 1, 0, 3, 2 },     /* the Heights: pistols and health */
        { 4, 5, 4, 2, 3, 4, 5, 2 },     /* Gull Island: rifles and rockets */
    };
    u32 h = 0x9E3779B9u;
    int n = NFIXED_PICKUPS;
    for (int by = 0; by < NB && n < MAX_PICKUPS; by++)
    for (int bx = 0; bx < NB && n < MAX_PICKUPS; bx++) {
        int d = city_district[by * NB + bx];
        h = h * 1664525u + 1013904223u;
        if ((bx < 8 && by < 8) || d == DI_RIVER || BLK_KIND(bx, by) == BK_WATER || (h >> 28) % 3) continue;
        int x = bx * 1024 + M(13.5), y = by * 1024 + M(20 + (h >> 8) % 36);
        if ((h >> 20) & 1) { x = bx * 1024 + M(20 + (h >> 12) % 36); y = by * 1024 + M(13.5); }   /* the cross street */
        for (int k = 0; k < 8 && world_point_blocked(x, y, M(0.6)); k++) { x += M(2); y += M(2); }
        pickups[n].x = x; pickups[n].y = y;
        pickups[n].type = dist_types[d < 5 ? d : 0][(h >> 4) & 7]; pickups[n].avail = 1; pickups[n].timer = 0;
        n++;
    }
    for (; n < MAX_PICKUPS; n++) pickups[n].avail = 0, pickups[n].timer = 0x7fff, pickups[n].x = pickups[n].y = -M(500);
}

static void update_pickups(int dt)
{
    for (int i = 0; i < MAX_PICKUPS; i++) {
        Pickup *k = &pickups[i];
        if (!k->avail) { k->timer -= dt >> 6; if (k->timer <= 0) k->avail = 1; continue; }
        if (pl.in_car >= 0 || pl.dead_t > 0) continue;
        if (dist_approx(k->x - plx(), k->y - ply()) > M(1.6)) continue;
        k->avail = 0; k->timer = 60 * 256;
        if (k->type == 0) { pl.owned |= 1; pl.ammo[1] += 36; pl.weapon = 1; msg("PISTOL", "", 2); }
        else if (k->type == 1) { pl.owned |= 2; pl.ammo[2] += 120; pl.weapon = 2; msg("SMG", "", 2); }
        else if (k->type == 3) { pl.owned |= 4; pl.ammo[3] += 16; pl.weapon = 3; msg("SHOTGUN", "", 2); }
        else if (k->type == 4) { pl.owned |= 8; pl.ammo[4] += 30; pl.weapon = 4; msg("RIFLE", "", 2); }
        else if (k->type == 5) { pl.owned |= 16; pl.ammo[5] += 5; pl.weapon = 5; msg("ROCKET LAUNCHER", "5 ROCKETS", 2); }
        else { pl.hp = 100; msg("HEALTH", "", 2); }
        g_snd.sfx |= SFX_PICKUP;
    }
}

/* ------------------------------------------------------------ story */
/* Act One of STORY.md. Nick (Mykyta) Melnyk, a Ukrainian army sergeant, flies to New York when his
   uncle Bohdan stops answering his phone. Bohdan runs a car shop on the waterfront; Moran's men
   have put him in hospital, and Victor Moran says Bohdan owes him $10,000 or the shop is his.
   Nick works it off: cars for Marta (Bohdan's shop foreman), product for Jinx, and Moran's dirty
   work, some of it ordered by Detective Shaw. Jinx steals Moran's ledger, which proves the debts
   are invented to seize waterfront property. Nick takes Shaw's cash run apart, and Rudy comes out
   of hiding: Moran sank the Lucky Marie, the tug of Bohdan's old friend Sal, for refusing him.
   It ends at the old pier, and Moran's phone points at Councilman Pryce (Act Two). */
typedef struct { const char *who, *l1, *l2; } Page;
static const Page *dlg; static int dlg_n, dlg_i, dlg_then;      /* dlg_then: 1 = start mission */
#define NPAGES(a) ((int)(sizeof(a) / sizeof((a)[0])))

static const Page st_intro[] = {
    { "", "NEW YORK. DOWN ON THE WATERFRONT,", "THE DOCKS RUN ON FAVOURS AND CASH." },
    { "NICK", "TEN YEARS IN THE UKRAINIAN ARMY.", "NOW MY UNCLE STOPS ANSWERING." },
    { "MARTA", "YOU'RE BOHDAN'S NEPHEW? NICK?", "I'M MARTA. I RUN HIS SHOP FLOOR." },
    { "MARTA", "HE'S IN THE HOSPITAL. MORAN'S MEN", "BROKE BOTH HIS ARMS." },
    { "MARTA", "MORAN SAYS BOHDAN OWES HIM", "$10,000. OR HE TAKES THE SHOP." },
    { "NICK", "MY UNCLE NEVER BORROWED A DIME.", 0 },
    { "MARTA", "TELL MORAN. TILL BOHDAN'S BACK,", "THE SHOP HAS WORK FOR YOU." },
    { "MARTA", "THE KIND WE DON'T ADVERTISE.", "FOLLOW THE YELLOW MARKER." },
};
static const Page st_m0[] = {
    { "MARTA", "A RICH KID LEFT A YELLOW SUPERCAR", "ON THE STREET. I HAVE A BUYER." },
    { "NICK", "I DROVE TANKS, NOT SUPERCARS.", 0 },
    { "MARTA", "THEN LEARN FAST. BRING IT TO THE", "SHOP. AND DON'T WRECK IT." } };
static const Page st_m1[] = {
    { "JINX", "YOU'RE BOHDAN'S NEPHEW? I'M JINX.", "I MOVE PRODUCT FOR WHOEVER PAYS." },
    { "JINX", "A PACKAGE IS WAITING AT THE DOCKS.", "PICK IT UP. BRING IT TO THE CLUB." },
    { "NICK", "WHAT'S IN IT?", 0 },
    { "JINX", "THE KIND OF STUFF THAT GETS YOU", "TEN YEARS. SO DON'T GET CAUGHT." } };
static const Page st_m2[] = {
    { "MORAN", "NICK MELNYK. VICTOR MORAN.", "BOHDAN'S DEBT IS YOURS NOW." },
    { "MORAN", "MY BOOKKEEPER IS TALKING TO COPS.", "THE MAN IN THE SUIT. SHUT HIM UP." },
    { "NICK", "AND THAT COMES OFF THE DEBT?", 0 },
    { "MORAN", "$1,500 OF IT. TELL BOHDAN TO GET", "WELL SOON. HOSPITALS AREN'T SAFE." } };
static const Page st_m3[] = {
    { "MORAN", "THE EASTSIDE KINGS ARE SELLING", "ON MY CORNER. SEND THEM A MESSAGE." },
    { "MORAN", "THREE CARS, PARKED LIKE THEY OWN", "THE STREET. BURN ALL THREE." },
    { "NICK", "THEY'LL SHOOT BACK.", 0 },
    { "MORAN", "THAT'S WHY I'M SENDING YOU.", 0 } };
static const Page st_m4[] = {
    { "SHAW", "DETECTIVE SHAW. I WORK FOR MORAN", "WHEN THE CITY ISN'T PAYING ME." },
    { "SHAW", "TWO OF MY OFFICERS ARE DIGGING", "INTO HIS BUSINESS. HONEST ONES." },
    { "NICK", "YOU WANT ME TO KILL COPS?", "YOUR OWN COPS?" },
    { "SHAW", "YOUR UNCLE'S IN ROOM 12, RIGHT?", "OLD MILL. THEN LOSE THE HEAT." } };
static const Page st_m5[] = {
    { "MORAN", "A VAN FULL OF MY PRODUCT HAS TO", "REACH THE WEST COAST ROAD." },
    { "MORAN", "90 SECONDS. THE BUYER DOESN'T WAIT,", "AND NEITHER DO I." } };
static const Page st_m6[] = {
    { "JINX", "NICK! MORAN'S MEN ARE COMING.", "THEY KNOW I TOOK HIS LEDGER." },
    { "JINX", "BOHDAN NEVER OWED HIM. NOBODY DID.", "MORAN INVENTS DEBTS TO TAKE LAND." },
    { "NICK", "THE SHOP. THE HOMES BY THE WATER.", 0 },
    { "JINX", "DEAL WITH HIS GUYS, THEN MEET ME", "AT THE SAFEHOUSE. LOSE THE COPS." } };
static const Page st_m7[] = {
    { "MARTA", "THE LEDGER NAMES SHAW. TONIGHT HE", "MOVES MORAN'S CASH IN A BLACK SUV." },
    { "MARTA", "STAY ON HIS BUMPER UNTIL HE BAILS.", "THEN MAKE SURE HE STAYS DOWN." },
    { "NICK", "MORAN WILL KNOW IT WAS ME.", 0 },
    { "MARTA", "GOOD. LET HIM BE SCARED FOR ONCE.", 0 } };
static const Page st_m8[] = {
    { "RUDY", "I'M RUDY. I CREWED SAL'S TUG,", "THE LUCKY MARIE. BOHDAN'S FRIEND." },
    { "RUDY", "SAL WOULDN'T RUN MORAN'S CARGO.", "SO MORAN'S MEN SANK HER. WITH HIM." },
    { "RUDY", "YOUR UNCLE WOULD HAVE BEEN NEXT.", 0 },
    { "NICK", "WHERE IS MORAN?", 0 },
    { "RUDY", "THE OLD PIER, NORTHWEST. HE'S GOT", "GUNS WITH HIM. COPS ON THE WAY." },
    { "NICK", "THEN HE PAYS. IN FULL.", 0 } };
static const Page st_end[] = {
    { "RUDY", "IT'S OVER. MORAN'S GONE. BOHDAN", "CAN COME HOME. SAL CAN REST." },
    { "NICK", "MORAN'S PHONE KEPT RINGING.", "ONE NAME. 'PRYCE'." },
    { "RUDY", "COUNCILMAN PRYCE? HE WANTS THE", "WHOLE WATERFRONT FOR A CASINO." },
    { "NICK", "THEN THIS ISN'T OVER.", 0 },
    { "", "END OF ACT ONE.", "THE CITY'S YOURS. FOR NOW." } };

static const Page *const mission_intro[] = { st_m0, st_m1, st_m2, st_m3, st_m4, st_m5, st_m6, st_m7, st_m8 };
static const u8 mission_intro_n[] = { NPAGES(st_m0), NPAGES(st_m1), NPAGES(st_m2), NPAGES(st_m3), NPAGES(st_m4),
                                      NPAGES(st_m5), NPAGES(st_m6), NPAGES(st_m7), NPAGES(st_m8) };
static const char *const mission_title[] = { "HOT WHEELS", "CANDY RUN", "THE BOOKKEEPER", "WRONG CORNER", "DIRTY BLUE",
                                             "SPECIAL DELIVERY", "THE LEDGER", "CASH RUN", "LAST CALL" };
/* who gives each job, and what they wear */
static const u16 giver_model[] = { PED_RED, PED_GREEN, PED_SUIT, PED_SUIT, PED_NAVY, PED_SUIT, PED_GREEN, PED_RED, PED_WHITE };

static int gstate = 0;                        /* GS_* */
/* car: 0 = saved on foot, else car set + 1 in the low byte and heading (256 per turn) in the high byte */
typedef struct { u32 magic; u16 ver, mission; s32 money; s16 hp; u8 owned, weapon; s16 ammo[5]; u16 car; s32 x, y; u32 sum; } SaveData;
static int save_read(SaveData *d);
static void save_write(void);
static int menu_sel, menu_note, title_t, has_save;
enum { GS_TITLE, GS_PLAY, GS_PAUSE, GS_DIALOG, GS_SHOP, GS_MINI };

/* ------------------------------------------------------------ shops */
/* Walk-in shops: a door marker on the east sidewalk of a block. Inside is a 2D room (interiors.c)
   with a buy menu. Any shop can be held up at gunpoint in free roam (never during a job). */
enum { SHOP_GUN, SHOP_STORE, SHOP_PAWN, SHOP_CARS, SHOP_BOWL, SHOP_POOL };
typedef struct { s16 x, y; u8 type, bx, by; int cd; } Shop;        /* cd: ticks until it can be robbed again */
#define MAX_SHOPS 28
EWRAM_BSS static Shop shops[MAX_SHOPS];
static int nshops, shop_cur = -1, shop_away = 1, shop_rob, shop_take, shop_goal, shop_loose;
static const char *shop_note; static int shop_note_t;
static const char *const shop_names[6] = { "NY GUNS", "CORNER MART", "PAWN & LOAN", "MOTOR SALES", "HARBOR LANES", "EIGHT BALL CLUB" };
static int mini_bet;
enum { K_GUN, K_HEAL, K_SELL, K_CAR, K_ROB, K_PLAY };
typedef struct { const char *label; s16 price; u8 kind, arg; u16 amt; } ShopItem;
static const ShopItem gun_items[] = {
    { "PISTOL + 36", 200, K_GUN, 1, 36 }, { "SMG + 120", 600, K_GUN, 2, 120 }, { "SHOTGUN + 16", 800, K_GUN, 3, 16 },
    { "RIFLE + 30", 1200, K_GUN, 4, 30 }, { "ROCKETS X5", 3000, K_GUN, 5, 5 } };
static const ShopItem store_items[] = {
    { "SNACK  +20 HP", 15, K_HEAL, 20, 0 }, { "HOT MEAL  +50 HP", 40, K_HEAL, 50, 0 }, { "FIRST AID KIT", 90, K_HEAL, 100, 0 } };
static const ShopItem pawn_items[] = {
    { "USED PISTOL + 24", 120, K_GUN, 1, 24 }, { "USED SHOTGUN + 8", 450, K_GUN, 3, 8 }, { "SELL YOUR GUNS", 0, K_SELL, 0, 0 } };
static const ShopItem car_items[] = {
    { "HATCHBACK", 1500, K_CAR, 0, CAR_HATCH_WHITE }, { "PICKUP", 3000, K_CAR, 0, CAR_PICKUP_BLUE },
    { "MUSCLE CAR", 6000, K_CAR, 0, CAR_MUSCLE_ORANGE }, { "SPORTS COUPE", 12000, K_CAR, 0, CAR_COUPE_BLACK },
    { "SUPERCAR", 25000, K_CAR, 0, CAR_SUPER_RED } };
static const ShopItem bowl_items[] = { { "BOWL A GAME", 100, K_PLAY, MINI_BOWL, 0 }, { "HIGH STAKES GAME", 500, K_PLAY, MINI_BOWL, 0 } };
static const ShopItem pool_items[] = { { "PLAY 8-BALL", 100, K_PLAY, MINI_POOL, 0 }, { "HIGH STAKES 8-BALL", 500, K_PLAY, MINI_POOL, 0 } };
static const ShopItem rob_item = { "ROB THE PLACE", 0, K_ROB, 0, 0 };
static const ShopItem *const shop_items[6] = { gun_items, store_items, pawn_items, car_items, bowl_items, pool_items };
static const u8 shop_nitems[6] = { 5, 3, 3, 5, 2, 2 };
static const s16 resale[6] = { 0, 60, 180, 240, 360, 900 };

static void setup_shops(void)
{
    static const u8 tab[][3] = {      /* block x, block y, type */
        {6,4,SHOP_POOL}, {4,10,SHOP_BOWL},                                                                       /* games */
        {3,3,SHOP_GUN}, {1,4,SHOP_STORE}, {5,6,SHOP_STORE}, {2,6,SHOP_PAWN}, {7,3,SHOP_CARS},                       /* Docks */
        {10,2,SHOP_STORE}, {13,6,SHOP_STORE}, {16,3,SHOP_GUN}, {11,8,SHOP_PAWN}, {18,5,SHOP_CARS}, {15,9,SHOP_STORE}, /* Downtown */
        {12,12,SHOP_GUN}, {17,11,SHOP_STORE}, {10,16,SHOP_PAWN}, {13,17,SHOP_CARS}, {11,13,SHOP_STORE},          /* Industrial */
        {2,11,SHOP_STORE}, {5,15,SHOP_STORE}, {3,18,SHOP_PAWN}, {6,13,SHOP_GUN}, {1,16,SHOP_STORE},              /* the Heights */
        {17,17,SHOP_STORE}, {18,18,SHOP_GUN} };                                                                  /* Gull Island */
    nshops = 0;
    for (unsigned i = 0; i < sizeof tab / sizeof tab[0] && nshops < MAX_SHOPS; i++) {
        int bx = tab[i][0], by = tab[i][1];
        if (BLK_KIND(bx, by) != BK_BUILT) continue;
        Shop *h = &shops[nshops++];
        h->bx = (u8)bx; h->by = (u8)by; h->type = tab[i][2]; h->cd = 0;
        h->x = (s16)(bx * 1024 + M(62.5)); h->y = (s16)(by * 1024 + M(30));
    }
}

static int shop_can_rob(void)
{
    return shop_cur >= 0 && shops[shop_cur].cd <= 0 && mstate != 1 && pl.weapon >= 1 && pl.ammo[pl.weapon] > 0;
}
static int shop_menu(const ShopItem **out)       /* this shop's menu, plus the hold-up when allowed */
{
    int t = shops[shop_cur].type, n = shop_nitems[t];
    for (int i = 0; i < n; i++) out[i] = &shop_items[t][i];
    if (shop_can_rob()) out[n++] = &rob_item;
    return n;
}
static void shop_say(const char *t) { shop_note = t; shop_note_t = 2 * TICKS; }

static void shop_leave(void)
{
    Shop *h = &shops[shop_cur];
    if (shop_rob) {
        pl.money += shop_take;
        h->cd = 240 * TICKS;                    /* four minutes before it has cash again */
        int w = shop_take < 700 ? 2 : 3;
        if (w > wanted) { wanted = w; wanted_cool = 0; }
        static char buf[24]; buf[0] = '+'; buf[1] = '$';
        { char tmp[12]; int n = 0, v = shop_take; do { tmp[n++] = (char)('0' + v % 10); v /= 10; } while (v); int k = 2; while (n) buf[k++] = tmp[--n]; buf[k] = 0; }
        big(buf, 1, 3);
        g_snd.sfx |= SFX_CASH;
    }
    pl.x = Q8(h->x + M(1.8)); pl.y = Q8(h->y); pl.head = 0; pl.moving = 0;
    gstate = GS_PLAY; shop_cur = -1; shop_rob = 0; g_hud.shop = 0;
}

static void buy_car(int model)
{
    Shop *h = &shops[shop_cur];
    int slot = SLOT_PARKED0;
    for (int k = SLOT_PARKED0; k < SLOT_PARKED_END; k++) if (!cars[k].active) { slot = k; break; }
    Car *c = &cars[slot];
    c->x = Q8(ROAD_C(h->bx + 1) - M(4.9)); c->y = Q8(h->y);
    c->head = Q8(3072); c->model = (u16)model; c->ai = AI_NONE; c->speed = 0; c->vx = c->vy = 0; c->active = 1; c->police = 0;
    c->kx = c->ky = c->spin = 0; c->dmg = 0; c->burn = 0;
}

static void shop_update(u16 keys, u16 pressed, int dt)
{
    Shop *h = &shops[shop_cur];
    if (shop_note_t > 0) shop_note_t -= dt;
    g_hud.shop = 1 + h->type; g_hud.shop_name = shop_names[h->type];
    g_hud.shop_note = shop_note_t > 0 ? shop_note : 0;
    g_snd.engine_vol = 0; g_snd.skid = 0; g_snd.horn = 0; g_snd.siren = 0; g_snd.nitro = 0;
    if (shop_rob) {                                          /* the hold-up */
        g_hud.shop_pose = 1; g_hud.menu_n = 0; g_hud.shop_cash = shop_take;
        if (shop_rob == 1) {
            if (keys & KEY_A) {
                shop_loose = 0;
                shop_take += shop_goal * dt / (6 * TICKS) + 1;
                if (shop_take >= shop_goal) { shop_take = shop_goal; shop_rob = 2; shop_say("THAT'S ALL OF IT. RUN!"); }
            } else if ((shop_loose += dt) > TICKS * 3 / 4) {
                shop_rob = 2;
                if (h->type == SHOP_GUN) {                   /* gun-shop clerks keep a shotgun under the counter */
                    pl.hp -= 35; if (pl.hp < 5) pl.hp = 5; g_hud.hurt_t = TICKS / 2;
                    g_snd.shot = 3; g_snd.shot_vol = 0; shop_say("HE HAD A SHOTGUN. GET OUT!");
                } else shop_say("HE HIT THE ALARM. RUN!");
            }
            g_hud.shop_line = shop_rob == 1 ? "HOLD A: KEEP HIM COVERED" : "B: RUN FOR IT";
        } else g_hud.shop_line = "B: RUN FOR IT";
        if (pressed & KEY_B) shop_leave();
        return;
    }
    g_hud.shop_pose = 0; g_hud.shop_line = "A: BUY   B: LEAVE";
    const ShopItem *items[8];
    int n = shop_menu(items);
    if (menu_sel >= n) menu_sel = n - 1;
    if ((pressed & KEY_UP) && menu_sel > 0) { menu_sel--; g_snd.step = 1; }
    if ((pressed & KEY_DOWN) && menu_sel < n - 1) { menu_sel++; g_snd.step = 1; }
    EWRAM_BSS static char lbl[8][28];
    for (int i = 0; i < n; i++) {
        const ShopItem *it = items[i];
        char *q = lbl[i]; const char *a = it->label;
        while (*a) *q++ = *a++;
        if (it->price) {
            int len = (int)(q - lbl[i]);
            while (len < 19) { *q++ = ' '; len++; }
            *q++ = '$';
            char tmp[8]; int m = 0, v = it->price; do { tmp[m++] = (char)('0' + v % 10); v /= 10; } while (v);
            while (m) *q++ = tmp[--m];
        }
        *q = 0;
        g_hud.menu_item[i] = lbl[i];
    }
    g_hud.menu_n = n; g_hud.menu_sel = menu_sel;
    if (pressed & KEY_B) { shop_leave(); return; }
    if (!(pressed & KEY_A)) return;
    const ShopItem *it = items[menu_sel];
    if (it->kind == K_ROB) {
        shop_rob = 1; shop_take = 0; shop_loose = 0;
        static const s16 goal[6] = { 1200, 450, 900, 2000, 700, 600 };
        shop_goal = goal[h->type] + (int)(rnd() % (u32)(goal[h->type] / 2));
        shop_say("HANDS UP! EMPTY THE REGISTER!");
        g_snd.sfx |= SFX_PUMP;
        return;
    }
    if (it->kind == K_PLAY) {
        if (pl.money < it->price) { shop_say("YOU CAN'T COVER THE BET"); g_snd.sfx |= SFX_DRY; return; }
        pl.money -= it->price; mini_bet = it->price;
        gstate = GS_MINI; mini_start(it->arg); g_snd.sfx |= SFX_CASH;
        return;
    }
    if (it->kind == K_SELL) {
        int v = 0;
        for (int w = 1; w <= 5; w++) if ((pl.owned >> (w - 1)) & 1) v += resale[w];
        if (!v) { shop_say("YOU'VE GOT NOTHING TO SELL"); return; }
        pl.money += v; pl.owned = 0; pl.weapon = 0;
        for (int w = 1; w <= 5; w++) pl.ammo[w] = 0;
        g_snd.sfx |= SFX_CASH; shop_say("SOLD. NO QUESTIONS ASKED.");
        return;
    }
    if (pl.money < it->price) { shop_say("NOT ENOUGH CASH"); g_snd.sfx |= SFX_DRY; return; }
    if (it->kind == K_HEAL && pl.hp >= 100) { shop_say("YOU'RE ALREADY FINE"); return; }
    pl.money -= it->price;
    g_snd.sfx |= SFX_CASH;
    switch (it->kind) {
    case K_GUN:
        pl.owned |= (u8)(1 << (it->arg - 1)); pl.ammo[it->arg] += it->amt; pl.weapon = it->arg;
        g_snd.sfx |= SFX_SWAP; shop_say("THANKS. STAY SAFE OUT THERE.");
        break;
    case K_HEAL:
        pl.hp += it->arg; if (pl.hp > 100) pl.hp = 100;
        shop_say("ENJOY.");
        break;
    case K_CAR:
        buy_car(it->amt);
        msg("YOUR NEW CAR IS", "PARKED OUT FRONT", 4);
        shop_leave();
        break;
    }
}

static void dialog(const Page *pg, int n, int then)
{
    dlg = pg; dlg_n = n; dlg_i = 0; dlg_then = then; gstate = GS_DIALOG; cut_on = 0;
}

/* a cutscene: the speaker stands facing Nick and the camera cuts between them */
static void cutscene(const Page *pg, int n, int then, int model)
{
    Ped *a = &peds[PED_ACTOR];
    int ex = plx(), ey = ply();
    int gap = pl.in_car >= 0 ? M(3.4) : M(2.2);
    int base = (pl.in_car >= 0 ? (cars[pl.in_car].head >> 8) + 1024 : (pl.head >> 8));   /* in a car: by the driver's door */
    int ax = ex, ay = ey;
    for (int k = 0; k < 4; k++) {
        int ang = (base + k * 1024) & 4095;
        ax = ex + ((icos(ang) * gap) >> 14); ay = ey + ((isin(ang) * gap) >> 14);
        if (!world_point_blocked(ax, ay, M(0.5))) break;
    }
    int face = iatan2(ey - ay, ex - ax);
    a->active = 1; a->down = 0; a->hp = 100; a->cd = 0; a->type = P_CIV; a->model = (u16)model; a->state = S_IDLE;
    a->x = Q8(ax); a->y = Q8(ay); a->head = Q8(face); a->speed = Q8(M(1.4)); a->run = 0; a->aim = 0; a->phase = 0;
    a->bx = (u8)clampi(ax >> 10, 0, NB - 1); a->by = (u8)clampi(ay >> 10, 0, NB - 1);
    a->corner = (u8)(rnd() & 3); a->cw = (u8)(rnd() & 1);
    if (pl.in_car < 0) { pl.head = Q8((face + 2048) & 4095); pl.moving = 0; }
    dialog(pg, n, then);
    cut_on = 1; cut_t = 0;
}
static int is_hero(const char *w) { return w && w[0] == 'N' && w[1] == 'I'; }

/* ------------------------------------------------------------ missions */
#define NACT1 9                    /* Act One: the hand-written jobs below */
#define NMISSIONS 40               /* Acts Two to Four come from story_acts.h */
static int mission, mstate, mtimer;          /* mstate: 0 marker shown, 1 active */
static int phase;                            /* step within the current job */
EWRAM_BSS static int mission_start_x[NACT1], mission_start_y[NACT1];
static int story_flags;                      /* 1: Jinx spared, 2: Pryce taken alive */
static int garage_x, garage_y, drop_x, drop_y, safe_x, safe_y, pier_x, pier_y;
static int dock_x, dock_y, club_x, club_y, corner_x, corner_y, mill_x, mill_y;
static int time_left, pressure, chase_phase;
static const int hosp_bx = 2, hosp_by = 3, station_bx = 6, station_by = 6;
/* what the current job wants shown: one ground marker and up to 6 radar targets */
static int mk_x, mk_y, mk_col = -1;
static int nmb; EWRAM_BSS static int mb_x[6], mb_y[6], mb_c[6];
static void mblip(int x, int y, int c) { if (nmb < 6) { mb_x[nmb] = x; mb_y[nmb] = y; mb_c[nmb++] = c; } }
static const int gang_slot[3] = { SLOT_RIVAL0, SLOT_RIVAL0 + 1, SLOT_MISSION };

static void setup_missions(void)
{
    static const s16 st[NACT1][2] = {     /* metres */
        { 3 * 64 + 32, 4 * 64 + 13 }, { 5 * 64 + 13, 4 * 64 + 40 }, { 2 * 64 + 40, 2 * 64 + 13 }, { 4 * 64 + 40, 6 * 64 + 62 },
        { 1 * 64 + 40, 3 * 64 + 13 }, { 3 * 64 + 13, 1 * 64 + 40 }, { 7 * 64 + 40, 4 * 64 + 13 }, { 4 * 64 + 13, 2 * 64 + 40 },
        { 0 * 64 + 40, 0 * 64 + 13 } };   /* the van (5) and Shaw's SUV (7) start beside their markers */
    for (int i = 0; i < NACT1; i++) { mission_start_x[i] = M(1) * st[i][0]; mission_start_y[i] = M(1) * st[i][1]; }
    mission_start_y[3] = 6 * 1024 + M(62.5);
    garage_x = ROAD_C(1) + M(4.5); garage_y = 6 * 1024 + M(40);
    drop_x = -M(44); drop_y = ROAD_C(2);                            /* west connector, by the coast */
    safe_x = 6 * 1024 + M(40); safe_y = 1 * 1024 + M(13.5);
    dock_x = 7 * 1024 + M(40); dock_y = 6 * 1024 + M(13.5);         /* the docks, south-east */
    club_x = 1 * 1024 + M(40); club_y = 2 * 1024 + M(13.5);         /* the club, north-west */
    corner_x = ROAD_C(2); corner_y = 6 * 1024 + M(28);              /* the Kings' corner */
    mill_x = ROAD_C(5); mill_y = 1 * 1024 + M(30);                  /* the old mill */
    /* the pier: the coast point furthest north-west */
    int best = 0x7fffffff, bi = 0;
    for (int k = 0; k < NCOAST_WP; k++) { int v = coast_wp[k][0] + coast_wp[k][1]; if (v < best) { best = v; bi = k; } }
    pier_x = coast_wp[bi][0]; pier_y = coast_wp[bi][1];
    mission = 0; mstate = 0; time_left = -1; phase = 0;
}

static void story_clear(void);
static void story_after(int done);
static void reward(int money, const char *text)
{
    story_clear();
    pl.money += money;
    g_snd.sfx |= SFX_PASS;
    big(text, 1, 4);
    time_left = -1;
    int done = mission;
    mission++; mstate = 0; phase = 0;
    if (done == NACT1 - 1) {                      /* the end of Act One */
        wanted = 0; wanted_cool = 0;
        for (int i = 0; i < NPOLICE; i++) cars[SLOT_POLICE0 + i].active = 0;
        cutscene(st_end, NPAGES(st_end), 0, PED_WHITE);
    }
    story_after(done);
    save_write(); has_save = 1;                   /* autosave after every job */
}

static void story_after(int done);
static void mission_fail(const char *why)
{
    story_clear();
    big(why, 0, 3);
    g_snd.sfx |= SFX_FAIL;
    mstate = 0; time_left = -1; phase = 0;
    for (int i = 0; i < 3; i++) { int s = gang_slot[i]; if (cars[s].active && pl.in_car != s) cars[s].active = 0; }
    for (int i = PED_MISSION0; i < PED_ACTOR; i++) peds[i].active = 0;
}

static void mission_ped(int k, int type, int model, int x, int y, int hp)
{
    Ped *p = &peds[PED_MISSION0 + k];
    p->active = 1; p->down = 0; p->hp = hp; p->cd = 160; p->rag = 0;
    p->type = type; p->model = model; p->state = S_IDLE;
    p->x = Q8(x); p->y = Q8(y); p->head = Q8(rnd() & 4095); p->speed = Q8(M(1.4));
    p->bx = (u8)clampi(x >> 10, 0, NB - 1); p->by = (u8)clampi(y >> 10, 0, NB - 1);
}
static int mission_peds_down(int k0, int k1)          /* are mission peds k0..k1-1 all down? */
{
    for (int k = k0; k < k1; k++) { Ped *p = &peds[PED_MISSION0 + k]; if (p->active && !p->down) return 0; }
    return 1;
}
static void park_car(int slot, int model, int x, int y, int head, int police)
{
    Car *c = &cars[slot];
    c->active = 1; c->model = (u16)model; c->ai = AI_NONE; c->speed = 0; c->vx = c->vy = 0; c->kx = c->ky = c->spin = 0;
    c->police = (u8)police; c->dmg = 0; c->burn = 0; c->x = Q8(x); c->y = Q8(y); c->head = Q8(head);
}

static void story_start(void);
static void start_mission(void)
{
    mstate = 1; mtimer = 0; time_left = -1; phase = 0;
    if (mission >= NACT1) { story_start(); return; }
    Car *mc = &cars[SLOT_MISSION];
    switch (mission) {
    case 0:
        park_car(SLOT_MISSION, CAR_SUPER_YELLOW, ROAD_C(6) + M(4.9), 2 * 1024 + M(36), 1024, 0);
        msg("STEAL THE YELLOW SUPERCAR", "AND BRING IT TO THE GARAGE", 5);
        break;
    case 1:                                   /* candy run: collect at the docks, deliver to the club */
        mission_ped(0, P_CIV, PED_GREY, dock_x, dock_y, 60);
        msg("PICK UP THE PACKAGE", "AT THE DOCKS", 5);
        break;
    case 2: {
        pl.owned |= 1; if (pl.ammo[1] < 48) pl.ammo[1] = 48; pl.weapon = 1;
        int tx = 6 * 1024 + M(30), ty = 5 * 1024 + M(13.5);
        mission_ped(0, P_TARGET, PED_SUIT, tx, ty, 60);
        mission_ped(1, P_GUARD, PED_GREY, tx - M(2), ty + M(1.5), 100);
        mission_ped(2, P_GUARD, PED_GREY, tx + M(2), ty + M(1.5), 100);
        mission_ped(3, P_GUARD, PED_NAVY, tx + M(5), ty - M(1), 100);
        msg("TAKE OUT THE MAN IN THE SUIT.", "WATCH HIS BODYGUARDS. (A: FIRE)", 5);
        break; }
    case 3:                                   /* wrong corner: burn the Kings' three cars */
        pl.owned |= 1; if (pl.ammo[1] < 90) pl.ammo[1] = 90;
        /* parked nose to tail: one blast can set the next one burning */
        park_car(gang_slot[0], CAR_MUSCLE_ORANGE, corner_x + M(4.9), corner_y - M(6.5), 1024, 0);
        park_car(gang_slot[1], CAR_TUNER_GREEN, corner_x + M(4.9), corner_y, 1024, 0);
        park_car(gang_slot[2], CAR_HATCH5_GREEN, corner_x + M(4.9), corner_y + M(6.5), 1024, 0);
        for (int k = 0; k < 3; k++) mission_ped(k, P_GUARD, PED_GREEN, corner_x + M(8.5), corner_y + (k - 1) * M(7), 90);
        msg("DESTROY THE KINGS' THREE CARS", "SHOOT THEM OR RAM THEM", 5);
        break;
    case 4:                                   /* dirty blue: two honest cops at the old mill */
        pl.owned |= 1; if (pl.ammo[1] < 60) pl.ammo[1] = 60;
        park_car(SLOT_RIVAL0, CAR_POLICE, mill_x + M(4.9), mill_y, 1024, 1);
        mission_ped(0, P_GUARD, PED_COP, mill_x + M(8.5), mill_y - M(2), 110);
        mission_ped(1, P_GUARD, PED_COP, mill_x + M(8.5), mill_y + M(3), 110);
        msg("KILL THE TWO OFFICERS", "AT THE OLD MILL", 5);
        break;
    case 5:
        park_car(SLOT_MISSION, CAR_VAN_WHITE, ROAD_C(3) + M(4.9), 1 * 1024 + M(30), 1024, 0);
        time_left = 90 * TICKS;
        msg("GET IN THE VAN AND TAKE IT", "TO THE WEST COAST ROAD", 5);
        break;
    case 6: {                                 /* the ledger: Moran's men hit Jinx */
        pl.owned |= 1; if (pl.ammo[1] < 60) pl.ammo[1] = 60;
        int sx = plx(), sy = ply();
        static const s8 off[3][2] = { { 14, 4 }, { -12, 9 }, { 5, -14 } };
        for (int k = 0; k < 3; k++) {
            int x = sx + M(1) * off[k][0], y = sy + M(1) * off[k][1];
            if (world_point_blocked(x, y, M(0.5))) { x = sx + M(1) * off[k][0] / 2; y = sy; }
            mission_ped(k, P_GUARD, k == 1 ? PED_NAVY : PED_GREY, x, y, 100);
            peds[PED_MISSION0 + k].state = S_CHASE;
        }
        msg("MORAN'S MEN ARE HERE!", "TAKE THEM OUT", 4);
        break; }
    case 7:
        park_car(SLOT_MISSION, CAR_SUV_BLACK, 0, 0, 0, 0);
        place_on_lane(mc, 0, 2, 4, 30);
        mc->ai = AI_TRAFFIC; mc->speed = Q8(M(10)); mc->wait = 1;
        pressure = 0; chase_phase = 0;
        msg("CATCH SHAW'S SUV AND STAY", "ON HIS BUMPER UNTIL HE BAILS", 5);
        break;
    case 8:
        pl.owned |= 1; if (pl.ammo[1] < 80) pl.ammo[1] = 80;
        mission_ped(0, P_TARGET, PED_SUIT, pier_x, pier_y, 160);
        mission_ped(1, P_GUARD, PED_GREY, pier_x + M(3), pier_y + M(2), 120);
        mission_ped(2, P_GUARD, PED_NAVY, pier_x - M(2), pier_y + M(3), 120);
        mission_ped(3, P_GUARD, PED_GREY, pier_x + M(1), pier_y - M(4), 120);
        wanted = 2; wanted_cool = 0;
        msg("TAKE MORAN DOWN AT THE OLD PIER", "IN THE NORTHWEST", 5);
        break;
    }
}

static void story_update(int dt);
static void update_mission(int dt)
{
    mk_col = -1; nmb = 0;
    if (mission >= NMISSIONS || pl.dead_t > 0) return;
    if (mission >= NACT1) { story_update(dt); return; }
    if (mstate == 0) {
        mk_x = mission_start_x[mission]; mk_y = mission_start_y[mission]; mk_col = 0;
        mblip(mk_x, mk_y, 1);
        if (dist_approx(mission_start_x[mission] - plx(), mission_start_y[mission] - ply()) < M(3.5) && wanted == 0)
            cutscene(mission_intro[mission], mission_intro_n[mission], 1, giver_model[mission]);
        return;
    }
    mtimer += dt;
    if (time_left >= 0) {
        time_left -= dt;
        if (time_left <= 0) { time_left = -1; mission_fail("OUT OF TIME"); return; }
    }
    Car *mc = &cars[SLOT_MISSION];
    Ped *t = &peds[PED_MISSION0];
    switch (mission) {
    case 0:
        if (IS_WRECK(mc)) { mission_fail("THE SUPERCAR IS WRECKED"); break; }
        if (pl.in_car == SLOT_MISSION) { mk_x = garage_x; mk_y = garage_y; mk_col = 1; mblip(garage_x, garage_y, 2); }
        else mblip(mc->x >> 8, mc->y >> 8, 2);
        if (pl.in_car == SLOT_MISSION && dist_approx(garage_x - plx(), garage_y - ply()) < M(7) && iabs(mc->speed) < Q8(M(6)))
            reward(500, "JOB DONE  $500");
        break;
    case 1:
        if (phase == 0) {
            mk_x = dock_x; mk_y = dock_y; mk_col = 1; mblip(dock_x, dock_y, 2);
            if (dist_approx(dock_x - plx(), dock_y - ply()) < (pl.in_car >= 0 ? M(7) : M(3))) {
                phase = 1; time_left = 100 * TICKS; g_snd.door = 1;
                t->state = S_WALK;                                   /* the dealer strolls off */
                if (wanted < 1) { wanted = 1; wanted_cool = 0; }
                msg("THE COPS SAW THE HANDOFF!", "GET THE PACKAGE TO THE CLUB", 4);
            }
        } else {
            mk_x = club_x; mk_y = club_y; mk_col = 1; mblip(club_x, club_y, 2);
            if (dist_approx(club_x - plx(), club_y - ply()) < (pl.in_car >= 0 ? M(7) : M(3))) reward(800, "DELIVERED  $800");
        }
        break;
    case 2:
        mblip(t->x >> 8, t->y >> 8, 3);
        if (t->down) { crime(2); reward(1500, "HIT DONE  $1500"); }
        else if (dist_approx((t->x >> 8) - (6 * 1024 + M(30)), (t->y >> 8) - (5 * 1024 + M(13.5))) > M(160)) mission_fail("THE TARGET GOT AWAY");
        break;
    case 3: {
        int left = 0;
        for (int k = 0; k < 3; k++) {
            Car *g = &cars[gang_slot[k]];
            if (g->active && !IS_WRECK(g)) { left++; mblip(g->x >> 8, g->y >> 8, 3); }
        }
        if (left == 0) { crime(2); reward(1800, "MESSAGE SENT  $1800"); }
        else if (phase != 4 - left) { phase = 4 - left; if (phase > 1) msg(left == 1 ? "ONE CAR LEFT" : "TWO CARS LEFT", 0, 2); }
        break; }
    case 4:
        if (phase == 0) {
            for (int k = 0; k < 2; k++) { Ped *c = &peds[PED_MISSION0 + k]; if (c->active && !c->down) mblip(c->x >> 8, c->y >> 8, 3); }
            if (mission_peds_down(0, 2)) {
                phase = 1; wanted = 3; wanted_cool = 0;
                msg("OFFICERS DOWN. EVERY COP IN TOWN", "IS COMING. LOSE THE HEAT!", 5);
            }
        } else if (wanted == 0) reward(2200, "HEAT LOST  $2200");
        break;
    case 5:
        if (!mc->active || IS_WRECK(mc)) { mission_fail("THE VAN IS WRECKED"); break; }
        if (pl.in_car == SLOT_MISSION) { mk_x = drop_x; mk_y = drop_y; mk_col = 1; mblip(drop_x, drop_y, 2); }
        else mblip(mc->x >> 8, mc->y >> 8, 2);
        if (pl.in_car == SLOT_MISSION && dist_approx(drop_x - plx(), drop_y - ply()) < M(9) && iabs(mc->speed) < Q8(M(8))) {
            mc->ai = AI_NONE; reward(2500, "DELIVERED  $2500");
        }
        break;
    case 6:
        if (phase == 0) {
            for (int k = 0; k < 3; k++) { Ped *g = &peds[PED_MISSION0 + k]; if (g->active && !g->down) mblip(g->x >> 8, g->y >> 8, 3); }
            if (mission_peds_down(0, 3)) {
                phase = 1; if (wanted < 2) { wanted = 2; wanted_cool = 0; }
                msg("THE GUNFIRE BROUGHT THE COPS.", "LOSE THEM, THEN GET TO THE SAFEHOUSE", 5);
            }
        } else {
            if (wanted == 0) { mk_x = safe_x; mk_y = safe_y; mk_col = 1; mblip(safe_x, safe_y, 2); }
            if (wanted == 0 && dist_approx(safe_x - plx(), safe_y - ply()) < M(5)) reward(3000, "SAFE  $3000");
        }
        break;
    case 7:
        if (chase_phase == 0) {
            mblip(mc->x >> 8, mc->y >> 8, 3);
            if (IS_WRECK(mc)) { crime(2); reward(3500, "SHAW'S DONE  $3500"); break; }
            int d = dist_approx((mc->x >> 8) - plx(), (mc->y >> 8) - ply());
            if (pl.in_car >= 0 && d < M(9)) pressure += dt; else if (pressure > 0) pressure -= dt / 4;
            if (d > M(230)) { mission_fail("SHAW GOT AWAY"); break; }
            if (pressure >= 4 * TICKS) {      /* he bails out and runs */
                chase_phase = 1; mc->ai = AI_NONE; mc->speed = 0; mc->vx = mc->vy = 0;
                int a = mc->head >> 8;
                mission_ped(0, P_TARGET, PED_NAVY, (mc->x >> 8) + ((isin(a) * M(2)) >> 14), (mc->y >> 8) - ((icos(a) * M(2)) >> 14), 80);
                t->state = S_FLEE; t->timer = 30 * 256; t->fx = plx(); t->fy = ply();
                msg("SHAW BAILED!", "DON'T LET HIM GET AWAY", 3);
            }
        } else {
            mblip(t->x >> 8, t->y >> 8, 3);
            if (t->down) { crime(2); reward(3500, "SHAW'S DONE  $3500"); }
            else if (dist_approx((t->x >> 8) - plx(), (t->y >> 8) - ply()) > M(150)) mission_fail("SHAW GOT AWAY");
        }
        break;
    case 8:
        mblip(t->x >> 8, t->y >> 8, 3);
        if (t->down) { crime(1); reward(10000, "MORAN IS DONE"); }
        else if (dist_approx((t->x >> 8) - pier_x, (t->y >> 8) - pier_y) > M(200)) mission_fail("MORAN GOT AWAY");
        break;
    }
}


/* ------------------------------------------------------------ Acts Two to Four: scripted missions */
/* Each mission is a list of steps (story_acts.h). One step runs at a time: phase = step index,
   ssub = 0 on the step's first frame (set-up), then 1. Coordinates are in metres. */
enum { OP_GO, OP_CAR, OP_DROP, OP_KILL, OP_WRECK, OP_CHASE, OP_TAIL, OP_DEFEND, OP_HUNT, OP_HEAT, OP_SURVIVE,
       OP_SPOT, OP_SAY, OP_GIVE, OP_BOSS, OP_CHOICE, OP_PICKUP };
enum { F_FOOT = 1, F_CAR = 2, F_MCAR = 4, F_BAIL = 8, F_LOSE = 16, F_HERE = 32, F_FLEE = 64, F_KEEP = 128 };
typedef struct { u8 op, n, flags, arg; u16 model; s16 x, y, t; const char *m1, *m2; } Step;
typedef struct { const char *title; u16 giver; s16 sx, sy; const Page *intro; u8 nintro, nsteps; const Step *steps; u16 pay; const Page *after; u8 nafter; } MissionDef;
#define NOWHERE (-32000)
static int ssub, stimer, scount, swave, sx_dyn, sy_dyn, spress;
static const Step *cur_step(void);

static void story_clear(void)
{
    tail_mode = 0; spress = 0;
    for (int i = 0; i < 3; i++) { Car *c = &cars[gang_slot[i]]; if (c->ai == AI_RIVAL) c->ai = AI_NONE; }
}
static int story_pressure(void)
{
    const Step *st = cur_step();
    if (!st || mstate != 1) return -1;
    if (st->op == OP_TAIL) return stimer * 100 / (st->t * TICKS);
    if (st->op == OP_CHASE && (st->flags & F_BAIL)) return spress * 100 / (4 * TICKS);
    if (st->op == OP_SPOT) return stimer * 100 / (2 * TICKS);
    if (st->op == OP_SURVIVE) return stimer * 100 / (st->t * TICKS);
    return -1;
}
/* a free spot near (x, y): tries a ring of points until one is not inside a building or the water */
static void free_spot(int *x, int *y, int r)
{
    for (int k = 0; k < 12; k++) {
        int a = k * 341, px = *x + ((icos(a) * r) >> 14), py = *y + ((isin(a) * r) >> 14);
        if (!world_point_blocked(px, py, M(0.6))) { *x = px; *y = py; return; }
    }
}
static void spawn_gunmen(int k0, int n, int model, int x, int y, int hp, int chase, int type)
{
    for (int k = 0; k < n && k0 + k < 9; k++) {
        int px = x, py = y;
        free_spot(&px, &py, M(3 + 2 * k));
        mission_ped(k0 + k, type, model, px, py, hp ? hp : 100);
        if (chase) peds[PED_MISSION0 + k0 + k].state = S_CHASE;
    }
}
static void car_on_road(Car *c, int x, int y)       /* a moving car on the nearest east-west road */
{
    int road = clampi((y + M(32)) >> 10, 0, NB), at = clampi(x >> 10, 0, NB - 1);
    place_on_lane(c, 0, road, at, 10);
}
static void blip_car(Car *c, int col) { if (c->active) mblip(c->x >> 8, c->y >> 8, col); }
static int near_pt(int x, int y, int r) { return dist_approx(x - plx(), y - ply()) < r; }

#include "story_acts.h"

static const MissionDef *story_def(void) { return mission >= NACT1 && mission < NMISSIONS ? &story_defs[mission - NACT1] : 0; }
static const Step *cur_step(void) { const MissionDef *d = story_def(); return d && phase < d->nsteps ? &d->steps[phase] : 0; }
static const char *mission_name(int m) { return m < NACT1 ? mission_title[m] : story_defs[m - NACT1].title; }

static void story_start(void)
{
    const MissionDef *d = story_def();
    phase = 0; ssub = 0;
    msg(d->title, 0, 3);
}
static void step_done(void)
{
    const MissionDef *d = story_def();
    phase++; ssub = 0; time_left = -1; tail_mode = 0; spress = 0;
    if (phase >= d->nsteps) {
        static char buf[24]; char *q = buf; const char *a = "JOB DONE  $";
        while (*a) *q++ = *a++;
        { char t[8]; int n = 0, v = d->pay; do { t[n++] = (char)('0' + v % 10); v /= 10; } while (v); while (n) *q++ = t[--n]; *q = 0; }
        int had_after = d->after != 0;
        const Page *ap = d->after; int an = d->nafter;
        if (mission == NMISSIONS - 2 && (story_flags & 2) && d[0].after) { ap = st_end_arrest; an = NPAGES(st_end_arrest); }
        reward(d->pay, buf);
        if (had_after) cutscene(ap, an, 0, d->giver);
    }
}

static void story_update(int dt)
{
    const MissionDef *d = story_def();
    if (mstate == 0) {
        mk_x = M(1) * d->sx; mk_y = M(1) * d->sy; mk_col = 0; mblip(mk_x, mk_y, 1);
        if (near_pt(mk_x, mk_y, M(3.5)) && wanted == 0) cutscene(d->intro, d->nintro, 1, d->giver);
        return;
    }
    mtimer += dt;
    if (time_left >= 0) { time_left -= dt; if (time_left <= 0) { time_left = -1; mission_fail("OUT OF TIME"); return; } }
    const Step *st = cur_step();
    if (!st) return;
    Car *mc = &cars[SLOT_MISSION];
    int X = st->x == NOWHERE ? sx_dyn : M(1) * st->x, Y = st->x == NOWHERE ? sy_dyn : M(1) * st->y;
    int first = ssub == 0;
    if (first) {
        ssub = 1; stimer = 0; scount = 0; swave = 0;
        if (st->m1) msg(st->m1, st->m2, 5);
        if (st->t > 0 && (st->op == OP_GO || st->op == OP_DROP || st->op == OP_PICKUP)) time_left = st->t * TICKS;
    }
    switch (st->op) {
    case OP_GO: {
        int r = (st->flags & F_FOOT) ? M(2.5) : M(7);
        mk_x = X; mk_y = Y; mk_col = 1; mblip(X, Y, 2);
        if ((st->flags & F_MCAR) && (!mc->active || IS_WRECK(mc))) { mission_fail("THE CAR IS WRECKED"); break; }
        int ok = near_pt(X, Y, r);
        if (st->flags & F_FOOT) ok = ok && pl.in_car < 0;
        if (st->flags & F_CAR) ok = ok && pl.in_car >= 0 && iabs(cars[pl.in_car].speed) < Q8(M(8));
        if (st->flags & F_MCAR) ok = ok && pl.in_car == SLOT_MISSION && iabs(mc->speed) < Q8(M(8));
        if ((st->flags & F_LOSE) && wanted > 0) { ok = 0; if (near_pt(X, Y, r)) msg("LOSE THE COPS FIRST", 0, 1); }
        if ((st->flags & (F_CAR | F_MCAR)) && pl.in_car < 0 && near_pt(X, Y, r) && g_hud.msg_t <= 0) msg("YOU NEED TO BE IN A CAR", 0, 2);
        if (ok) step_done();
        break; }
    case OP_CAR:                                   /* get in the job's car */
        if (first && !(st->flags & F_KEEP)) park_car(SLOT_MISSION, st->model, X, Y, st->arg * 1024, 0);
        if (!mc->active || IS_WRECK(mc)) { mission_fail("THE CAR IS WRECKED"); break; }
        if (pl.in_car == SLOT_MISSION) { mc->ai = AI_NONE; step_done(); break; }
        blip_car(mc, 2);
        break;
    case OP_DROP:
        if (!mc->active || IS_WRECK(mc)) { mission_fail("THE CAR IS WRECKED"); break; }
        if (pl.in_car == SLOT_MISSION) { mk_x = X; mk_y = Y; mk_col = 1; mblip(X, Y, 2); }
        else { blip_car(mc, 2); if (first) msg("GET BACK IN THE CAR", 0, 2); }
        if ((st->flags & F_LOSE) && wanted > 0) break;
        if (pl.in_car == SLOT_MISSION && near_pt(X, Y, M(8)) && iabs(mc->speed) < Q8(M(7))) step_done();
        break;
    case OP_KILL: case OP_BOSS: {
        if (first) {
            int type = (st->flags & F_FLEE) ? P_TARGET : P_GUARD;
            if (st->op == OP_BOSS) {
                mission_ped(0, P_GUARD, st->model, X, Y, st->t ? st->t : 600);
                spawn_gunmen(1, st->n, st->arg ? PED_COP : PED_GREY, X, Y, 110, 0, P_GUARD);
            } else spawn_gunmen(0, st->n, st->model, X, Y, st->t, st->flags & F_HERE, type);
        }
        int left = 0, cnt = st->op == OP_BOSS ? st->n + 1 : st->n;
        for (int k = 0; k < cnt && k < 9; k++) {
            Ped *g = &peds[PED_MISSION0 + k];
            if (g->active && !g->down) { left++; mblip(g->x >> 8, g->y >> 8, 3); }
        }
        if ((st->flags & F_FLEE) && left && dist_approx((peds[PED_MISSION0].x >> 8) - plx(), (peds[PED_MISSION0].y >> 8) - ply()) > M(170)) { mission_fail("HE GOT AWAY"); break; }
        if (!left) { crime(2); if (cnt > 0) { sx_dyn = peds[PED_MISSION0].x >> 8; sy_dyn = peds[PED_MISSION0].y >> 8; } step_done(); }
        break; }
    case OP_WRECK: {
        if (first) {
            for (int k = 0; k < st->n && k < 3; k++)
                park_car(gang_slot[k], st->model, X + M(4.9), Y + (k - 1) * M(7), 1024, 0);
            if (st->arg) spawn_gunmen(0, st->arg, st->t ? PED_COP : PED_GREEN, X + M(8.5), Y, 100, 0, P_GUARD);
        }
        int left = 0;
        for (int k = 0; k < st->n && k < 3; k++) { Car *g = &cars[gang_slot[k]]; if (g->active && !IS_WRECK(g)) { left++; blip_car(g, 3); } }
        for (int k = 0; k < st->arg; k++) { Ped *g = &peds[PED_MISSION0 + k]; if (g->active && !g->down) mblip(g->x >> 8, g->y >> 8, 3); }
        if (!left) { crime(2); step_done(); }
        break; }
    case OP_CHASE:                                 /* a car runs: wreck it, or (F_BAIL) ride its bumper until the driver gives up */
        if (first) {
            park_car(SLOT_MISSION, st->model, 0, 0, 0, 0);
            car_on_road(mc, X, Y);
            mc->ai = AI_TRAFFIC; mc->speed = Q8(M(10)); mc->wait = 1;
        }
        blip_car(mc, 3);
        if (!mc->active || IS_WRECK(mc)) { crime(2); sx_dyn = mc->x >> 8; sy_dyn = mc->y >> 8; step_done(); break; }
        {
            int dd = dist_approx((mc->x >> 8) - plx(), (mc->y >> 8) - ply());
            if (dd < M(120)) swave = 1;                     /* caught up: from now on they can get away */
            if (swave && dd > M(st->t ? st->t : 240)) { mission_fail("THEY GOT AWAY"); break; }
            if (st->flags & F_BAIL) {
                if (pl.in_car >= 0 && dd < M(9)) spress += dt; else if (spress > 0) spress -= dt / 4;
                if (spress >= 4 * TICKS) {
                    mc->ai = AI_NONE; mc->speed = 0; mc->vx = mc->vy = 0;
                    sx_dyn = mc->x >> 8; sy_dyn = mc->y >> 8;
                    msg("THE DRIVER BAILED!", 0, 3);
                    step_done();
                }
            }
        }
        break;
    case OP_TAIL:                                  /* follow without being seen: not closer than 15 m, not further than 110 m */
        if (first) {
            park_car(SLOT_MISSION, st->model, 0, 0, 0, 0);
            car_on_road(mc, X, Y);
            mc->ai = AI_TRAFFIC; mc->speed = Q8(M(8)); mc->wait = 1; tail_mode = 1;
        }
        blip_car(mc, 3);
        if (!mc->active || IS_WRECK(mc)) { mission_fail("YOU WRECKED THE CAR YOU WERE TAILING"); break; }
        {
            int dd = dist_approx((mc->x >> 8) - plx(), (mc->y >> 8) - ply());
            if (dd < M(90)) swave = 1;                      /* found them: the tail starts now */
            if (!swave) break;
            if (dd > M(110)) { mission_fail("YOU LOST THEM"); break; }
            if (dd < M(15)) { scount += dt; if (scount > 2 * TICKS) { mission_fail("YOU WERE SPOTTED"); break; } if (scount < dt * 2) msg("TOO CLOSE! BACK OFF", 0, 2); }
            else scount = 0;
            stimer += dt;
            if (stimer >= st->t * TICKS) {
                tail_mode = 0; mc->ai = AI_NONE; mc->speed = 0; mc->vx = mc->vy = 0;
                sx_dyn = mc->x >> 8; sy_dyn = mc->y >> 8;
                step_done();
            }
        }
        break;
    case OP_DEFEND:                                /* waves of gunmen come for a spot; stay and fight */
        mk_x = X; mk_y = Y; mk_col = 1;
        if (!near_pt(X, Y, M(70))) { mission_fail("YOU LEFT THEM UNPROTECTED"); break; }
        if (first || mission_peds_down(0, 3)) {
            if (!first) swave++;
            if (swave >= st->n) { crime(2); step_done(); break; }
            int a = (int)(rnd() & 4095);
            spawn_gunmen(0, 3, st->model, X + ((icos(a) * M(26)) >> 14), Y + ((isin(a) * M(26)) >> 14), 100, 1, P_GUARD);
            if (!first) msg(swave == st->n - 1 ? "LAST WAVE!" : "MORE ARE COMING", 0, 2);
        }
        for (int k = 0; k < 3; k++) { Ped *g = &peds[PED_MISSION0 + k]; if (g->active && !g->down) mblip(g->x >> 8, g->y >> 8, 3); }
        break;
    case OP_HUNT: {                                /* cars come after you: wreck them, or (F_LOSE) shake them */
        if (first)
            for (int k = 0; k < st->n && k < 2; k++) {
                Car *h = &cars[gang_slot[k]];
                park_car(gang_slot[k], st->model, 0, 0, 0, 0);
                spawn_traffic_near(h);
                h->model = st->model; h->ai = AI_RIVAL; h->police = 0; h->crew = 0; h->speed = Q8(M(15)); h->stuck = 0; h->rev = 0;
            }
        int left = 0;
        for (int k = 0; k < st->n && k < 2; k++) {
            Car *h = &cars[gang_slot[k]];
            if (!h->active || IS_WRECK(h)) continue;
            if ((st->flags & F_LOSE) && dist_approx((h->x >> 8) - plx(), (h->y >> 8) - ply()) > M(170)) { h->ai = AI_NONE; h->active = 0; continue; }
            left++; blip_car(h, 3);
        }
        if (!left) step_done();
        break; }
    case OP_HEAT:
        if (first && wanted < st->n) { wanted = st->n; wanted_cool = 0; }
        if (wanted == 0) step_done();
        break;
    case OP_SURVIVE:
        if (wanted < st->n) { wanted = st->n; wanted_cool = 0; }
        if (first) time_left = -1;
        stimer += dt;
        if (stimer >= st->t * TICKS) step_done();
        break;
    case OP_SPOT:                                  /* stand on the spot (on foot) for two seconds */
        mk_x = X; mk_y = Y; mk_col = 1; mblip(X, Y, 2);
        if (pl.in_car < 0 && near_pt(X, Y, M(2.2))) { stimer += dt; if (stimer >= 2 * TICKS) { g_snd.sfx |= SFX_PICKUP; msg(st->m2 ? st->m2 : "GOT IT", 0, 2); step_done(); } }
        else stimer = 0;
        break;
    case OP_SAY:
        step_done();
        dialog(story_says[st->n].pg, story_says[st->n].n, 0);
        break;
    case OP_GIVE:
        pl.owned |= (u8)(1 << (st->n - 1)); if (pl.ammo[st->n] < st->t) pl.ammo[st->n] = st->t; pl.weapon = st->n;
        g_snd.sfx |= SFX_SWAP;
        step_done();
        break;
    case OP_CHOICE: {                              /* kill them, or walk up to them */
        Ped *t = &peds[PED_MISSION0];
        if (first) { mission_ped(0, P_CIV, st->model, X, Y, 100); t->state = S_IDLE; }
        mblip(t->x >> 8, t->y >> 8, 3);
        if (t->down) { story_flags &= ~st->n; step_done(); break; }
        if (pl.in_car < 0 && dist_approx((t->x >> 8) - plx(), (t->y >> 8) - ply()) < M(1.8)) {
            story_flags |= st->n; t->active = 0; step_done();
        }
        t->state = S_IDLE;
        break; }
    case OP_PICKUP: {                              /* a friend waits at the spot: pull up next to them */
        Ped *f = &peds[PED_MISSION0 + 9];
        if (first) { mission_ped(9, P_CIV, st->model, X, Y, 100); f->state = S_IDLE; }
        if (f->down) { mission_fail("THEY'RE DEAD"); break; }
        mk_x = X; mk_y = Y; mk_col = 1; mblip(X, Y, 2);
        f->state = S_IDLE;
        if (pl.in_car >= 0 && near_pt(X, Y, M(8)) && iabs(cars[pl.in_car].speed) < Q8(M(3))) {
            f->active = 0; g_snd.door = 1; step_done();
        } else if (pl.in_car < 0 && near_pt(X, Y, M(6)) && g_hud.msg_t <= 0) msg("COME BACK IN A CAR", 0, 2);
        break; }
    }
}

static void story_after(int done)
{
    /* act titles when a new act begins */
    if (done == NACT1 - 1) big("ACT TWO: PAPER", 1, 5);
    else if (mission == 20) big("ACT THREE: IRON", 1, 5);
    else if (mission == 31) big("ACT FOUR: TIDE", 1, 5);
    else if (mission == NMISSIONS) big("THE END", 1, 8);
}

/* ------------------------------------------------------------ respawn */
static void respawn_player(void)
{
    int arrested = pl.dead_kind == 1;
    if (pl.in_car >= 0) { cars[pl.in_car].ai = AI_NONE; pl.in_car = -1; }
    int bx = arrested ? station_bx : hosp_bx, by = arrested ? station_by : hosp_by;
    pl.x = Q8(bx * 1024 + M(13.5)); pl.y = Q8(by * 1024 + M(36)); pl.head = Q8(0);
    cam_ang = pl.head;
    pl.hp = 100;
    if (arrested) { pl.owned = 0; pl.weapon = 0; pl.ammo[1] = pl.ammo[2] = pl.ammo[3] = pl.ammo[4] = pl.ammo[5] = 0; pl.money -= pl.money / 10; }
    else { pl.money -= 100; if (pl.money < 0) pl.money = 0; }
    wanted = 0; wanted_cool = 0;
    for (int i = 0; i < NPOLICE; i++) cars[SLOT_POLICE0 + i].active = 0;
    for (int i = PED_CREW0; i < PED_MISSION0; i++) peds[i].active = 0;
    if (mstate == 1) mission_fail(arrested ? "BUSTED" : "WASTED");
    big(0, 0, 0);
}

/* ------------------------------------------------------------ init */
/* Parked cars: a fixed pool of slots that follows the player. A parked car that is left far
   behind (and nobody is using) is moved to a free kerb 80-140 m ahead of you, out of sight. */
static const u16 park_models[20] = { CAR_SEDAN_SILVER, CAR_COUPE_BLACK, CAR_SUV_BLACK, CAR_MUSCLE_ORANGE, CAR_HATCH_WHITE,
                                     CAR_SEDAN_BLUE, CAR_MUSCLE_SILVER, CAR_HATCH_LIME, CAR_SUPER_RED, CAR_HATCH5_RED,
                                     CAR_PICKUP_WHITE, CAR_PICKUP_BLUE, CAR_VAN_WHITE, CAR_HATCH5_GREEN,
                                     CAR_WAGON_BROWN, CAR_TUNER_GREEN, CAR_LUXURY_BLACK, CAR_MINIVAN_SILVER, CAR_YACHT_MAROON, CAR_BOXTRUCK_WHITE };
static int park_spot(Car *c, int dmin, int dmax)
{
    int pbx = plx() >> 10, pby = ply() >> 10;
    for (int tries = 0; tries < 10; tries++) {
        int axis = rnd() & 1;
        int road = clampi((axis ? pbx : pby) + rndr(-2, 3), 0, NB);
        int seg = clampi((axis ? pby : pbx) + rndr(-2, 2), 0, NB - 1);
        int side = (rnd() & 1) ? 1 : -1;
        if ((road == 0 && side < 0) || (road == NB && side > 0)) side = -side;
        int along = seg * 1024 + M(rndr(22, 52)), off = ROAD_C(road) + side * M(4.9);
        int x = axis ? off : along, y = axis ? along : off;
        int d = dist_approx(x - plx(), y - ply());
        if (d < dmin || d > dmax) continue;
        int busy = 0;
        for (int k = 0; k < MAX_CARS && !busy; k++)
            if (cars[k].active && &cars[k] != c && dist_approx((cars[k].x >> 8) - x, (cars[k].y >> 8) - y) < M(7)) busy = 1;
        if (busy) continue;
        c->x = Q8(x); c->y = Q8(y);
        if (axis == 0) c->head = Q8(side < 0 ? 0 : 2048); else c->head = Q8(side > 0 ? 1024 : 3072);
        c->model = park_models[rnd() % 20]; c->ai = AI_NONE; c->speed = 0; c->vx = c->vy = 0; c->active = 1; c->police = 0;
        c->kx = c->ky = c->spin = 0; c->dmg = 0; c->burn = 0;
        return 1;
    }
    return 0;
}
#define PARKED_NEAR 12
static void spawn_parked(void)
{
    for (int n = SLOT_PARKED0; n < SLOT_PARKED_END; n++)
        if (n >= SLOT_PARKED0 + PARKED_NEAR || !park_spot(&cars[n], M(12), M(150))) cars[n].active = 0;
}
static void recycle_parked(void)
{
    static int rr;
    int i = SLOT_PARKED0 + rr;
    rr = (rr + 1) % (SLOT_PARKED_END - SLOT_PARKED0);
    Car *c = &cars[i];
    if (pl.in_car == i) return;
    if (c->active && (c->ai != AI_NONE || c->burn > 0 || dist_approx((c->x >> 8) - plx(), (c->y >> 8) - ply()) < M(160))) return;
    /* keep about as many parked cars around you as the old small map had (it spread 29 over the whole city) */
    int n = 0;
    for (int k = SLOT_PARKED0; k < SLOT_PARKED_END; k++) n += cars[k].active && cars[k].ai == AI_NONE;
    if (c->active) { c->active = 0; n--; }
    if (n < PARKED_NEAR) park_spot(c, M(80), M(140));
}

typedef struct { s16 x, y; s16 head; } Lamp;
#define MAX_LAMPS ((NB + 1) * NB * 4)
EWRAM_BSS static Lamp lamps[MAX_LAMPS];
static int nlamps;

static void spawn_lamps(void)
{
    nlamps = 0;
    for (int road = 0; road <= NB; road++)
    for (int seg = 0; seg < NB; seg++)
    for (int axis = 0; axis < 2; axis++)
    for (int side = -1; side <= 1; side += 2) {
        if ((road == 0 && side < 0) || (road == NB && side > 0)) continue;
        if (nlamps >= MAX_LAMPS) return;
        Lamp *l = &lamps[nlamps++];
        int along = seg * 1024 + M(side > 0 ? 30 : 46);
        int off = ROAD_C(road) + side * M(6.7);
        if (axis == 0) { l->x = along; l->y = off; l->head = side > 0 ? 3072 : 1024; }
        else           { l->x = off; l->y = along; l->head = side > 0 ? 2048 : 0; }
    }
}
/* lamps sorted into city blocks, so a frame only looks at the blocks around the camera */
EWRAM_BSS static u16 lamp_start[NBLK], lamp_count[NBLK];
static int lamp_block(const Lamp *l) { return clampi(l->y >> 10, 0, NB - 1) * NB + clampi(l->x >> 10, 0, NB - 1); }
static void bucket_lamps(void)
{
    EWRAM_BSS static Lamp tmp[MAX_LAMPS];
    EWRAM_BSS static u16 fill[NBLK];
    for (int b = 0; b < NBLK; b++) lamp_count[b] = 0;
    for (int i = 0; i < nlamps; i++) lamp_count[lamp_block(&lamps[i])]++;
    int at = 0;
    for (int b = 0; b < NBLK; b++) { lamp_start[b] = (u16)at; at += lamp_count[b]; }
    for (int b = 0; b < NBLK; b++) fill[b] = lamp_start[b];
    for (int i = 0; i < nlamps; i++) tmp[fill[lamp_block(&lamps[i])]++] = lamps[i];
    for (int i = 0; i < nlamps; i++) lamps[i] = tmp[i];
}

/* ------------------------------------------------------------ street props */
typedef struct { s16 x, y; u16 frame; s16 head; s16 h; } Prop;     /* head < 0: no view angles */
#define MAX_PROPS (NBLK * 12)
EWRAM_BSS static Prop props[MAX_PROPS];
static int nprops;
EWRAM_BSS static u16 prop_start[NBLK], prop_count[NBLK];
#define MAX_ROOF 1100
EWRAM_BSS static u16 roof_start[NBLK + 1];
EWRAM_BSS static Prop roofs[MAX_ROOF];
static int nroofs;

static void add_prop(int x, int y, int frame, int head, int h)
{
    if (nprops >= MAX_PROPS) return;
    Prop *p = &props[nprops++];
    p->x = (s16)x; p->y = (s16)y; p->frame = (u16)frame; p->head = (s16)head; p->h = (s16)h;
}

static void spawn_props(void)
{
    nprops = 0;
    for (int by = 0; by < NB; by++)
    for (int bx = 0; bx < NB; bx++) {
        int bi = by * NB + bx;
        prop_start[bi] = (u16)nprops;
        int ox = bx * 1024, oy = by * 1024;
        int park = BLK_KIND(bx, by) == BK_PARK, water = BLK_KIND(bx, by) == BK_WATER;
        /* traffic lights on two diagonal corners of each intersection */
        add_prop(ox + M(12.5), oy + M(12.5), PROP_TLIGHT, 2048, 0);
        add_prop(ox + M(61.8), oy + M(61.8), PROP_TLIGHT, 0, 0);
        /* corner furniture */
        add_prop(ox + M(13.5), oy + M(17), PROP_HYDRANT, -1, 0);
        add_prop(ox + M(13.6), oy + M(13.6), PROP_TRASH, -1, 0);
        if (!park && !water) add_prop(ox + M(62.6), oy + M(40), PROP_TRASH, -1, 0);
        add_prop(ox + M(40), oy + M(13.5), PROP_BENCH, 1024, 0);       /* faces the road */
        /* street trees in the tree pits on alternating blocks */
        if ((((bx + by) & 1) == 0 || park) && !water)
            for (int k = 0; k < 3; k++) {
                int a = M(20 + k * 16);
                add_prop(ox + M(13), oy + a, PROP_STREETTREE, -1, 0);
                add_prop(ox + a, oy + M(13), PROP_STREETTREE, -1, 0);
            }
        prop_count[bi] = (u16)(nprops - prop_start[bi]);
    }
    /* wooden water towers on older, shorter buildings */
    nroofs = 0;
    for (int bk = 0; bk < NBLK; bk++) {
    roof_start[bk] = (u16)nroofs;
    for (int i = blk_start[bk]; i < blk_start[bk + 1] && nroofs < MAX_ROOF; i++) {
        const Building *b = &g_world.b[i];
        if (b->mat == MAT_GLASS || b->h > M(36) || (i * 2654435761u >> 30) == 0) continue;
        if (b->x1 - b->x0 < M(9) || b->y1 - b->y0 < M(9)) continue;
        Prop *p = &roofs[nroofs++];
        p->x = (s16)(b->x0 + (b->x1 - b->x0) * (3 + (i & 3)) / 10);
        p->y = (s16)(b->y0 + (b->y1 - b->y0) * (6 - (i & 1)) / 10);
        p->frame = PROP_WATERTOWER; p->head = -1; p->h = (s16)(b->h + b->base - ground_h(p->x, p->y));
    }
    }
    roof_start[NBLK] = (u16)nroofs;
}

void game_init(void)
{
    { SaveData d; has_save = save_read(&d); }
    gstate = GS_TITLE; menu_sel = has_save ? 1 : 0;
    world_generate();
    for (int i = 0; i < MAX_CARS; i++) cars[i].active = 0;
    ncars = MAX_CARS;
    pl.x = Q8(ROAD_C(3) + M(18)); pl.y = Q8(ROAD_C(4) - LANE_OFS);
    Car *p = &cars[0];
    p->model = CAR_HATCH_LIME; p->ai = AI_NONE; p->speed = 0; p->vx = p->vy = 0; p->active = 1;
    p->x = pl.x; p->y = pl.y; p->head = 0;
    static const u16 tm[16] = { CAR_TAXI, CAR_HATCH_WHITE, CAR_PICKUP_WHITE, CAR_SUV_BLACK, CAR_SEDAN_BLUE, CAR_VAN_WHITE, CAR_TAXI, CAR_HATCH5_GREEN,
                                CAR_WAGON_BROWN, CAR_MINIVAN_SILVER, CAR_BOXTRUCK_WHITE, CAR_LUXURY_BLACK, CAR_TUNER_GREEN, CAR_YACHT_MAROON, CAR_AMBULANCE, CAR_SEDAN_SILVER };
    for (int i = 0; i < NTRAFFIC; i++) {
        Car *c = &cars[SLOT_TRAFFIC0 + i];
        spawn_traffic_near(c);
        c->model = tm[i & 15];
    }
    /* a few cars on the starting street */
    for (int i = 0; i < 3; i++) {
        Car *c = &cars[SLOT_TRAFFIC0 + i];
        place_on_lane(c, i == 1 ? 2 : 0, 4, 3 + (i == 1), 40 + i * 30);
    }
    spawn_parked();
    for (int i = 0; i < MAX_PEDS; i++) peds[i].active = 0;
    for (int i = 0; i < PED_CIV_END; i++) {
        Ped *q = &peds[i];
        place_ped_near(q, M(4));
        q->type = (i % 10 == 9) ? P_COP : P_CIV;
        q->model = q->type == P_COP ? PED_COP : civ_models[i % 6];
        q->speed = Q8(M(1.2)) + (int)(rnd() % (u32)Q8(M(0.5)));
        q->phase = rnd() & 511;
    }
    spawn_lamps(); bucket_lamps();
    spawn_props();
    setup_shops();
    setup_pickups();
    setup_missions();
    pl.in_car = 0; pl.head = 0; pl.hp = 100; pl.money = 0; pl.weapon = 0; pl.owned = 0;
    cam_ang = p->head;
    msg("WALK TO THE YELLOW MARKER", "TO START A JOB.  R: EXIT CAR", 6);
}

/* ------------------------------------------------------------ update */
/* ------------------------------------------------------------ save game (SRAM) */
#define SAVE_MAGIC 0x31415447u               /* "GTA1" */
#ifdef HOST
static u8 sram_sim[512];
#define SRAM_B(i) sram_sim[i]
#else
#define SRAM_B(i) (((volatile u8 *)0x0E000000)[i])
#endif
static u32 save_sum(const SaveData *d)
{
    const u8 *b = (const u8 *)d; u32 h = 2166136261u;
    for (unsigned i = 0; i < sizeof *d - 4; i++) h = (h ^ b[i]) * 16777619u;
    return h;
}
static int save_read(SaveData *d)
{
    u8 *b = (u8 *)d;
    for (unsigned i = 0; i < sizeof *d; i++) b[i] = SRAM_B(i);
    return d->magic == SAVE_MAGIC && d->sum == save_sum(d) && (d->mission & 255) <= NMISSIONS;
}
static void save_write(void)
{
    SaveData d;
    { u8 *z = (u8 *)&d; for (unsigned i = 0; i < sizeof d; i++) z[i] = 0; }   /* no stray padding bytes in the checksum */
    d.magic = SAVE_MAGIC; d.ver = 1; d.mission = (u16)(mission | (story_flags << 8)); d.money = pl.money; d.hp = (s16)pl.hp;
    d.owned = pl.owned; d.weapon = pl.weapon;
    for (int i = 1; i < 5; i++) d.ammo[i] = pl.ammo[i];
    d.ammo[0] = pl.ammo[5];                       /* slot 0 (fists) was unused: it holds the rockets */
    d.x = plx(); d.y = ply();
    if (pl.in_car >= 0 && !IS_WRECK(&cars[pl.in_car])) {
        const Car *c = &cars[pl.in_car];
        enum { RCP = ((1 << 22) + CAR_SET_FRAMES - 1) / CAR_SET_FRAMES };
        int set = (int)(((unsigned)(c->model - CAR_COUPE_BLUE) * RCP) >> 22);
        if (set >= 0 && set < CAR_NSETS) d.car = (u16)((set + 1) | (((c->head >> 8) >> 4) << 8));
    }
    d.sum = save_sum(&d);
    const u8 *b = (const u8 *)&d;
    for (unsigned i = 0; i < sizeof d; i++) SRAM_B(i) = b[i];
}

/* ------------------------------------------------------------ title / pause / dialog */
static void new_game_state(void)
{
    if (pl.in_car >= 0) cars[pl.in_car].ai = AI_NONE;
    pl.in_car = 0; pl.hp = 100; pl.money = 0; pl.owned = 0; pl.weapon = 0; pl.dead_t = 0;
    for (int i = 0; i < 6; i++) pl.ammo[i] = 0;
    for (int i = 0; i < MAX_ROCKETS; i++) rockets[i].active = 0;
    Car *p = &cars[0];
    p->model = CAR_HATCH_LIME; p->ai = AI_NONE; p->speed = 0; p->vx = p->vy = 0; p->active = 1; p->kx = p->ky = p->spin = 0; p->dmg = 0; p->burn = 0;
    p->x = Q8(ROAD_C(3) + M(18)); p->y = Q8(ROAD_C(4) - LANE_OFS); p->head = 0;
    pl.x = p->x; pl.y = p->y; pl.head = 0; cam_ang = 0;
    wanted = 0; wanted_cool = 0;
    for (int i = 0; i < NPOLICE; i++) cars[SLOT_POLICE0 + i].active = 0;
    for (int i = PED_CREW0; i < MAX_PEDS; i++) peds[i].active = 0;
    cars[SLOT_MISSION].active = 0; cars[SLOT_RIVAL0].active = cars[SLOT_RIVAL0 + 1].active = 0;
    setup_missions(); setup_pickups();
    for (int i = 0; i < MAX_CATS; i++) street_cats[i].active = 0;
    g_hud.msg_t = 0; g_hud.big_t = 0; g_hud.countdown = 0; cut_on = 0;
}
static void load_game(const SaveData *d)
{
    new_game_state();
    mission = d->mission & 255; story_flags = d->mission >> 8; pl.money = d->money; pl.hp = d->hp > 0 ? d->hp : 100;
    pl.owned = d->owned; pl.weapon = d->weapon;
    for (int i = 1; i < 5; i++) pl.ammo[i] = d->ammo[i];
    pl.ammo[0] = 0; pl.ammo[5] = d->ammo[0];
    /* back where the game was saved: in the car you saved in, or on foot with no free car */
    pl.x = Q8(d->x); pl.y = Q8(d->y);
    int set = (d->car & 255) - 1;
    if (set >= 0 && set < CAR_NSETS) {
        Car *c = &cars[0];
        c->model = (u16)(CAR_COUPE_BLUE + set * CAR_SET_FRAMES); c->police = 0;
        c->x = pl.x; c->y = pl.y; c->head = Q8((d->car >> 8) << 4); cam_ang = c->head;
        pl.in_car = 0; pl.head = c->head;
    } else { pl.in_car = -1; cars[0].active = 0; }
}

static void meta_update(u16 keys, u16 pressed, int dt)
{
    g_snd.engine_vol = 0; g_snd.skid = 0; g_snd.horn = 0; g_snd.siren = 0; g_snd.nitro = 0;
    int up = pressed & KEY_UP, down = pressed & KEY_DOWN, ok = pressed & (KEY_A | KEY_START);
    g_hud.dlg_l1 = 0; g_hud.menu_n = 0; g_hud.title = 0; g_hud.shop = 0;
    g_hud.cutscene = gstate == GS_DIALOG && cut_on;
    g_hud.mini = 0;
    if (gstate == GS_SHOP) { shop_update(keys, pressed, dt); return; }
    if (gstate == GS_MINI) {                            /* bowling or pool; back to the counter when it ends */
        g_hud.mini = 1;
        int r = mini_update(keys, pressed, dt);
        if (r) {
            gstate = GS_SHOP; g_hud.mini = 0;
            if (r == 1) { pl.money += mini_bet * 2; g_snd.sfx |= SFX_PASS; shop_say("YOU WON THE BET!"); }
            else shop_say("BETTER LUCK NEXT TIME");
        }
        return;
    }
    if (gstate == GS_TITLE) {
        title_t += dt;
        g_hud.title = 1;
        static const char *items[2] = { "NEW GAME", "CONTINUE" };
        int n = has_save ? 2 : 1;
        for (int i = 0; i < n; i++) g_hud.menu_item[i] = items[i];
        g_hud.menu_n = n; g_hud.menu_title = 0;
        if (up && menu_sel > 0) { menu_sel--; g_snd.step = 1; }
        if (down && menu_sel < n - 1) { menu_sel++; g_snd.step = 1; }
        g_hud.menu_sel = menu_sel;
        if (ok) {
            g_snd.door = 1;
            if (menu_sel == 1) {
                SaveData d;
                if (save_read(&d)) { load_game(&d); gstate = GS_PLAY; msg("GAME LOADED", mission < NMISSIONS ? mission_name(mission) : "FREE ROAM", 3); }
            } else { new_game_state(); cutscene(st_intro, NPAGES(st_intro), 0, PED_RED); }
        }
        return;
    }
    if (gstate == GS_PAUSE) {
        static const char *items[4] = { "RESUME", "SAVE GAME", "", "QUIT TO TITLE" };
        for (int i = 0; i < 4; i++) g_hud.menu_item[i] = items[i];
        static char rbuf[24];
        { const char *a = "RADIO: ", *b = g_snd.radio_on ? station_name[g_snd.station] : "OFF"; int n = 0; while (*a) rbuf[n++] = *a++; while (*b && n < 23) rbuf[n++] = *b++; rbuf[n] = 0; }
        g_hud.menu_item[2] = rbuf;
        g_hud.menu_n = 4; g_hud.menu_title = "PAUSED";
        g_hud.menu_note = menu_note == 1 ? "GAME SAVED" : menu_note == 2 ? "CAN'T SAVE DURING A JOB OR CHASE" : 0;
        if (up && menu_sel > 0) { menu_sel--; g_snd.step = 1; }
        if (down && menu_sel < 3) { menu_sel++; g_snd.step = 1; }
        g_hud.menu_sel = menu_sel;
        if (pressed & KEY_START) { gstate = GS_PLAY; return; }
        if (pressed & KEY_B) { gstate = GS_PLAY; return; }
        if (pressed & KEY_A) {
            g_snd.door = 1;
            switch (menu_sel) {
            case 0: gstate = GS_PLAY; break;
            case 1: if (mstate == 1 || wanted > 0 || pl.dead_t > 0) menu_note = 2; else { save_write(); has_save = 1; menu_note = 1; } break;
            case 2: radio_step(1); g_hud.radio_t = 0; break;
            case 3: gstate = GS_TITLE; menu_sel = has_save ? 1 : 0; g_snd.radio_on = 0; break;
            }
        }
        return;
    }
    if (gstate == GS_DIALOG) {
        cut_t += dt;
        const Page *pg = &dlg[dlg_i];
        g_hud.dlg_who = pg->who; g_hud.dlg_l1 = pg->l1; g_hud.dlg_l2 = pg->l2;
        if (pressed & (KEY_A | KEY_START | KEY_B)) {
            if (++dlg_i >= dlg_n) {
                gstate = GS_PLAY; g_hud.dlg_l1 = 0;
                if (cut_on) { cut_on = 0; peds[PED_ACTOR].state = S_WALK; }
                if (dlg_then == 1) start_mission();
                else if (mission < NMISSIONS && mstate == 0 && mission == 0 && title_t >= 0) msg(mission_title[0], "GO TO THE YELLOW MARKER", 4);
            }
        }
    }
}

int game_title_active(void) { return gstate == GS_TITLE; }

void game_update(u16 keys, int dt)
{
    if (dt > TICKS / 8) dt = TICKS / 8;
    frame_no++;
    u16 pressed = keys & ~keys_prev;
    keys_prev = keys;
    if (g_hud.msg_t > 0) g_hud.msg_t -= dt;
    if (g_hud.radio_t > 0) g_hud.radio_t -= dt;
    if (g_hud.big_t > 0) g_hud.big_t -= dt;
    if (g_hud.hurt_t > 0) g_hud.hurt_t -= dt;
    if (pl.dead_t > 0) {
        pl.dead_t -= dt;
        keys = 0; pressed = 0;
        if (pl.dead_t <= 0) respawn_player();
    }
    if (gstate != GS_PLAY) { meta_update(keys, pressed, dt); return; }
    if ((pressed & KEY_START) && !(keys & KEY_SELECT)) { gstate = GS_PAUSE; menu_sel = 0; menu_note = 0; return; }
    g_hud.menu_n = 0; g_hud.dlg_l1 = 0; g_hud.title = 0; g_hud.shop = 0; g_hud.mini = 0;
    if (pressed & KEY_R) toggle_car();
    for (int i = 0; i < nshops; i++) if (shops[i].cd > 0) shops[i].cd -= dt;
    if (pl.in_car < 0 && pl.dead_t <= 0) {                 /* walk into a shop door */
        int near = -1;
        for (int i = 0; i < nshops; i++)
            if (iabs(shops[i].x - plx()) < M(2.5) && iabs(shops[i].y - ply()) < M(2.5)) { near = i; break; }
        if (near < 0) shop_away = 1;
        else if (shop_away && dist_approx(shops[near].x - plx(), shops[near].y - ply()) < M(1.3)) {
            shop_away = 0;
            if (wanted > 0) msg("THE DOOR IS LOCKED.", "LOSE THE COPS FIRST", 3);
            else { shop_cur = near; gstate = GS_SHOP; menu_sel = 0; shop_rob = 0; shop_note_t = 0; g_snd.door = 1; return; }
        }
    }
    impact = 0;
    int pspeed_before = pl.in_car >= 0 ? cars[pl.in_car].speed : 0;
    int pbx = plx(), pby = ply();
    n_near = 0;
    for (int i = 0; i < ncars; i++)
        if (cars[i].active && dist_approx((cars[i].x >> 8) - pbx, (cars[i].y >> 8) - pby) < M(70)) near_cars[n_near++] = (u8)i;
    EWRAM_BSS static int mdt[MAX_CARS];
    for (int i = 0; i < ncars; i++) mdt[i] = 0;
    for (int i = 0; i < ncars; i++) {
        Car *c = &cars[i];
        if (!c->active) continue;
        if (i == pl.in_car) drive(c, keys, dt);
        else if (c->ai == AI_TRAFFIC) {
            int far = dist_approx((c->x >> 8) - pbx, (c->y >> 8) - pby) > M(80);
            if (far && ((frame_no + i) & 1)) continue;          /* distant traffic: half rate */
            int dts = far ? dt * 2 : dt;
            if (i >= SLOT_TRAFFIC0 && i < SLOT_TRAFFIC0 + NTRAFFIC && dist_approx((c->x >> 8) - pbx, (c->y >> 8) - pby) > M(150)) {
                u16 m = c->model; spawn_traffic_near(c);
                static const u16 fresh[10] = { CAR_TAXI, CAR_HATCH_WHITE, CAR_SEDAN_BLUE, CAR_PICKUP_WHITE, CAR_HATCH5_RED, CAR_SUV_WHITE,
                                               CAR_WAGON_BROWN, CAR_MINIVAN_SILVER, CAR_LUXURY_BLACK, CAR_BOXTRUCK_WHITE };
                c->model = m == CAR_WRECK ? fresh[rnd() % 10] : m;
            }
            update_traffic(i, dts);
            mdt[i] = dts;
            continue;
        }
        else if (c->ai == AI_POLICE || c->ai == AI_RIVAL) update_police(i, dt);   /* rival: a hunter car rams you */
        else {
            int drag = Q8(M(8)) * dt / TICKS;
            if (c->speed > drag) c->speed -= drag; else if (c->speed < -drag) c->speed += drag; else c->speed = 0;
            if (i >= SLOT_POLICE0 && i < SLOT_POLICE0 + NPOLICE && (c->police || IS_WRECK(c)) && (wanted == 0 || IS_WRECK(c)) &&
                dist_approx((c->x >> 8) - pbx, (c->y >> 8) - pby) > M(120)) c->active = 0;
        }
        mdt[i] = dt;
    }
    /* move in sub-steps short enough that a fast car can't jump through another one between checks */
    {
        /* each car gets its own number of sub-steps (parked and slow cars need one); the fastest
           car sets how many collision passes run, and slower cars move on a spread of those passes */
        EWRAM_BSS static u8 nst[MAX_CARS];
        EWRAM_BSS static s16 acc_[MAX_CARS];
        for (int i = 0; i < ncars; i++) { nst[i] = 1; acc_[i] = 0; }
        int n = 1;
        for (int k = 0; k < n_near; k++) {
            int i = near_cars[k];
            Car *c = &cars[i];
            int vx, vy; car_vel(c, &vx, &vy);
            int v = dist_approx(vx >> 8, vy >> 8);                 /* units per second */
            int step_len = v * mdt[i] >> 14;                       /* TICKS = 16384: units moved this frame */
            int ni = ((step_len * 7282) >> 16) + 1;                /* / M(0.6) */
            if (ni > 8) ni = 8;
            nst[i] = (u8)ni;
            if (ni > n) n = ni;
        }
        for (int st = 0; st < n; st++) {
            for (int i = 0; i < ncars; i++) {
                if (!mdt[i] || !cars[i].active) continue;
                int ni = nst[i];
                acc_[i] += ni;
                if (acc_[i] < n) continue;
                acc_[i] -= n;
                int part = ni == 1 ? mdt[i] : mdt[i] / ni;
                if (st == n - 1) part = mdt[i] - part * (ni - 1);
                if (part > 0) move_car(&cars[i], part);
            }
            car_car_collisions();
        }
    }
    update_peds(dt);
    update_damage(dt);
    if (pl.in_car < 0) {
        if (pl.dead_t <= 0) { walk(keys, dt); player_fire(keys, pressed, dt); }
        else pl.moving = 0;
    } else {
        pl.x = cars[pl.in_car].x; pl.y = cars[pl.in_car].y;       /* crashes never hurt the driver */
    }
    update_pickups(dt);
    update_rockets(dt);
    update_cats(dt);
    recycle_parked();
    {   /* Bohdan's shop (Marta runs it) fixes your ride: stop in front of it */
        static int in_garage;
        int near = pl.in_car >= 0 && dist_approx(garage_x - plx(), garage_y - ply()) < M(7) && !(mission == 0 && mstate == 1);
        if (near && !in_garage && iabs(cars[pl.in_car].speed) < Q8(M(3))) {
            Car *c = &cars[pl.in_car];
            in_garage = 1;
            if (c->dmg > 0 && !IS_WRECK(c)) {
                int cost = c->dmg >= CAR_FIRE ? 300 : 100;
                if (pl.money >= cost) { pl.money -= cost; c->dmg = 0; c->burn = 0; msg("MARTA FIXED YOUR CAR", cost == 300 ? "-$300" : "-$100", 2); g_snd.sfx |= SFX_CASH; }
                else msg("REPAIRS COST $100", "COME BACK WITH CASH", 2);
            }
        }
        if (!near) in_garage = 0;
    }

    /* wanted level: police respond, and it cools off when no cop is around */
    if (wanted > 0) {
        if (active_police() < (wanted < 4 ? wanted : 4) && (frame_no & 31) == 0) spawn_police();
        if (cop_nearby(M(55))) wanted_cool = 0;
        else {
            wanted_cool += dt;
            if (wanted_cool > 12 * TICKS) { wanted--; wanted_cool = 0; }
        }
        if (pl.arrest_t > TICKS * 6 / 5 && pl.dead_t <= 0) { pl.dead_t = 3 * TICKS; pl.dead_kind = 1; big("BUSTED", 0, 3); g_snd.sfx |= SFX_FAIL; }
    } else {
        for (int i = 0; i < NPOLICE; i++) {
            Car *c = &cars[SLOT_POLICE0 + i];
            if (c->active && c->ai == AI_POLICE) { c->ai = AI_NONE; }
        }
    }
    pl.arrest_t = pl.arrest_t > 0 ? pl.arrest_t - dt / 2 : 0;
    update_mission(dt);

    /* chase camera */
    int target = (pl.in_car >= 0) ? cars[pl.in_car].head : pl.head;
    if (pl.in_car >= 0) {
        Car *c = &cars[pl.in_car];
        if (c->speed > Q8(M(5))) {
            int vdir = Q8(iatan2(c->vy, c->vx));
            target = (target + angdiff(vdir >> 8, target >> 8) * 128) & (Q8(4096) - 1);
        }
    }
    int diff = angdiff(target >> 8, cam_ang >> 8);
    int k = 5 * dt;
    if (k > TICKS) k = TICKS;
    cam_ang = (cam_ang + (Q8(diff) * (k >> 4)) / (TICKS >> 4)) & (Q8(4096) - 1);

    /* ---- sound + HUD */
    int siren = 0;
    for (int i = 0; i < NPOLICE; i++) {
        Car *c = &cars[SLOT_POLICE0 + i];
        if (!c->active || !c->police || wanted == 0) continue;
        int d = dist_approx((c->x >> 8) - pbx, (c->y >> 8) - pby);
        int v = d < M(80) ? 40 - d * 40 / M(80) : 0;
        if (v > siren) siren = v;
    }
    g_snd.siren = siren;
    if (pl.in_car >= 0) {
        Car *c = &cars[pl.in_car];
        int v = c->speed / 4096;
        int av = iabs(v);
        g_hud.speed_mph = av * 9 / 4; g_hud.in_car = 1;
        static const int gear_lo[5] = { 0, 9, 17, 25, 99 };
        int g = 0; while (av >= gear_lo[g + 1]) g++;
        int span = gear_lo[g + 1] - gear_lo[g]; if (g == 3) span = 12;
        int rpm = 900 + (av - gear_lo[g]) * 4800 / span;
        if (rpm > 6500) rpm = 6500;
        if ((keys & KEY_A) && av < 2) rpm = 2600;
        g_snd.engine_rpm = rpm;
        g_snd.engine_vol = (keys & (KEY_A | KEY_B)) ? 56 : 36;
        g_snd.horn = (keys & KEY_SELECT) ? 1 : 0;
        if (pressed & KEY_UP) radio_step(1);
        if (pressed & KEY_DOWN) radio_step(-1);
        g_snd.nitro = boosting;
        g_hud.nitro = nitro;
        int skid = 0;
        if (drifting) skid = 34 + av / 3;
        if ((keys & KEY_B) && v > 12) skid = 40;
        g_snd.skid = skid > 48 ? 48 : skid;
        int dv = iabs(pspeed_before - c->speed) / 4096;
        if (impact > dv) dv = impact;
        if (dv > 5 && !(keys & KEY_B)) g_snd.crash = dv > 20 ? 64 : dv * 3;
        if (dv > 16) g_snd.sfx |= SFX_GLASS;                    /* hard hit: breaking glass */
    } else {
        g_hud.speed_mph = 0; g_hud.in_car = 0; g_snd.horn = 0; g_snd.skid = 0; g_snd.nitro = 0;
        int best = 0x7fffffff, bi = -1;
        for (int i = 0; i < ncars; i++) {
            if (!cars[i].active || !cars[i].speed) continue;
            int d = dist_approx((cars[i].x >> 8) - pbx, (cars[i].y >> 8) - pby);
            if (d < best) { best = d; bi = i; }
        }
        if (bi >= 0 && best < M(40)) {
            g_snd.engine_rpm = 1400 + iabs(cars[bi].speed / 4096) * 120;
            g_snd.engine_vol = 30 - best * 30 / M(40);
        } else g_snd.engine_vol = 0;
        int st = pl.phase >> 8;
        if (pl.moving && st != prev_step) g_snd.step = 1;
        prev_step = st;
    }
    g_hud.near_car = 0;
    if (pl.in_car < 0)
        for (int i = 0; i < ncars; i++) {
            if (!cars[i].active || IS_WRECK(&cars[i])) continue;
            int dx = (cars[i].x - pl.x) >> 8, dy = (cars[i].y - pl.y) >> 8;
            if (dx * dx + dy * dy < M(3.5) * M(3.5)) g_hud.near_car = 1;
        }
    g_hud.hp = pl.hp; g_hud.money = pl.money; g_hud.wanted = wanted;
    g_hud.wanted_flash = wanted > 0 && wanted_cool > 4 * TICKS;
    g_hud.weapon = pl.in_car >= 0 ? 0 : pl.weapon;
    g_hud.weapon_name = weapon_names[g_hud.weapon];
    g_hud.ammo = pl.ammo[pl.weapon];
    g_hud.race_total = 0;
    g_hud.car_hp = pl.in_car >= 0 ? 100 - cars[pl.in_car].dmg / 10 : -1;
    g_hud.timer_s = time_left >= 0 ? (time_left + TICKS - 1) / TICKS : -1;
    g_hud.pressure = (mission == 7 && mstate == 1 && chase_phase == 0) ? pressure * 100 / (4 * TICKS) : story_pressure();
    /* radar */
    g_hud.px = plx(); g_hud.py = ply(); g_hud.pang = cam_ang >> 8;
    int nb = 0;
    for (int i = 0; i < nmb; i++) { g_hud.bx[nb] = mb_x[i]; g_hud.by[nb] = mb_y[i]; g_hud.bc[nb++] = mb_c[i]; }
    for (int i = 0; i < NPOLICE && nb < 12; i++) {
        Car *c = &cars[SLOT_POLICE0 + i];
        if (c->active && c->police && wanted) { g_hud.bx[nb] = c->x >> 8; g_hud.by[nb] = c->y >> 8; g_hud.bc[nb++] = 4; }
    }
    g_hud.nblips = nb;
    /* pickups within radar range (about 70 m): drawn as icons, guns and health */
    int np = 0;
    for (int i = 0; i < MAX_PICKUPS && np < MAX_PK_BLIPS; i++) {
        if (!pickups[i].avail) continue;
        int dx = pickups[i].x - g_hud.px, dy = pickups[i].y - g_hud.py;
        if (iabs(dx) > M(72) || iabs(dy) > M(72)) continue;
        g_hud.pk_x[np] = (s16)pickups[i].x; g_hud.pk_y[np] = (s16)pickups[i].y; g_hud.pk_t[np++] = pickups[i].type;
    }
    for (int i = 0; i < nshops && np < MAX_PK_BLIPS; i++) {   /* shops: storefront icons */
        int dx = shops[i].x - g_hud.px, dy = shops[i].y - g_hud.py;
        if (iabs(dx) > M(72) || iabs(dy) > M(72)) continue;
        g_hud.pk_x[np] = shops[i].x; g_hud.pk_y[np] = shops[i].y; g_hud.pk_t[np++] = (u8)(10 + shops[i].type);
    }
    g_hud.npk = np;
}

/* ---------------------------------------------------------------- scene */
static Sprite *sp; static int nsp;
static Camera *cm;
static int cam_gh, cam_gh_init, cam_rel_h = M(2.6);

static int view_lim = 0x7fffffff;
static int view_dist(int x, int y, int *rel_view)
{
    int dx = x - cm->x, dy = y - cm->y;
    if (iabs(dx) > view_lim || iabs(dy) > view_lim) return -1;     /* cheap reject first */
    int z = (dx * cm->c + dy * cm->s) >> 14;
    if (z < M(1)) return -1;
    int d = dist_approx(dx, dy);
    if (d > view_lim) return -1;
    /* also reject things well outside the left/right edges of the view */
    int xs = (dx * cm->s - dy * cm->c) >> 14;
    if (iabs(xs) * FX > z * 95 + M(3) * FX) return -1;
    if (rel_view) *rel_view = iatan2(dy, dx);                    /* only when the caller needs the angle */
    return d;
}

static void push_h(int x, int y, int frame, int h)
{
    if (nsp >= MAX_SPRITES) return;
    h = (h == SPR_DECAL ? 0 : h) + ground_h(x, y);
    sp[nsp].x = x; sp[nsp].y = y; sp[nsp].frame = (u16)frame; sp[nsp].h = (s16)h; nsp++;
}
static void push(int x, int y, int frame) { push_h(x, y, frame, 0); }

/* frame within a person set: idle / walk / run / aim, 18 per view direction */
/* frame within a person set from the view-relative heading (4096 per turn): 16 directions, of which
   0..8 are stored and 9..15 drawn mirrored (bit 15); walk and run cycles have PED_CYCLE frames */
static int ped_frame(int base, int rel, int moving, int running, int aiming, int phase)
{
    int d = ((rel + 128) >> 8) & 15, flip = 0;
    if (d > 8) { d = 16 - d; flip = 0x8000; }
    int cyc = ((phase & 511) * PED_CYCLE) >> 9;
    int k = 0;
    if (aiming) k = 1 + 2 * PED_CYCLE;
    else if (running) k = 1 + PED_CYCLE + cyc;
    else if (moving) k = 1 + cyc;
    return (base + d * PED_DIR_FRAMES + k) | flip;
}
/* ragdoll in flight: 3 stored views (behind, side, front) x 8 pitch angles; the other side mirrored */
static int ped_tumble_frame(int base, int rel, int ang)
{
    int v = ((rel + 512) >> 10) & 3, flip = 0;
    if (v == 3) { v = 1; flip = 0x8000; }
    return (base + PED_TUMBLE + v * 8 + ((ang >> 9) & 7)) | flip;
}

/* camera elevation level for a car at this distance: 16, 11, 7 or 4 degrees */
static int elev_level(int d)
{
    int dh = cam_rel_h - M(0.55);
    if (dh * 100 > d * 24) return 0;
    if (dh * 1000 > d * 158) return 1;
    if (dh * 1000 > d * 96) return 2;
    return 3;
}

void game_build_scene(Camera *cam, Sprite *out, int *nout)
{
    int tx, ty, camh, want;
    if (pl.in_car >= 0) {
        tx = cars[pl.in_car].x >> 8; ty = cars[pl.in_car].y >> 8; camh = M(2.6);
        int v = cars[pl.in_car].speed / 4096; if (v < 0) v = 0; if (v > 70) v = 70;
        want = M(6.4) + v * M(2) / 70;
    } else { tx = plx(); ty = ply(); camh = M(2.1); want = M(3.8); }
    int a = cam_ang >> 8;
    int c = icos(a), s = isin(a);
    int d = want;
    int glim = ground_h(tx, ty) + camh - M(0.7);       /* a bank behind pulls the camera in too */
    for (int t = M(1); t <= want; t += M(0.5)) {
        int px = tx - ((c * t) >> 14), py = ty - ((s * t) >> 14);
        if (world_point_blocked(px, py, M(0.6)) || ground_h(px, py) > glim) { d = t - M(0.5); break; }
    }
    if (d < M(1.5)) d = M(1.5);
    cam_dist += (d - cam_dist) / 3;
    if (cam_dist > d) cam_dist = d;
    cam->x = tx - ((c * cam_dist) >> 14);
    cam->y = ty - ((s * cam_dist) >> 14);
    {   /* ride over the terrain: stay above the ground at both the target and the camera */
        int gt = ground_h(tx, ty), gc = ground_h(cam->x, cam->y);
        int g = gt;
        if (gc + M(1.2) > g + camh) g = gc + M(1.2) - camh;        /* never dip under a bank */
        if (!cam_gh_init || iabs(g - cam_gh) > M(8)) { cam_gh = g; cam_gh_init = 1; }
        cam_gh += (g - cam_gh) / 2;
        cam->h = cam_gh + camh; cam->gh = gc;
        cam_rel_h = cam->h - gt;
    }
    cam->c = c; cam->s = s;
    if (gstate == GS_DIALOG && cut_on && peds[PED_ACTOR].active) {
        /* cutscene: an establishing two-shot, then over-the-shoulder cuts to whoever is talking */
        const Ped *ap = &peds[PED_ACTOR];
        int ax = ap->x >> 8, ay = ap->y >> 8, ex = plx(), ey = ply();
        int a2 = iatan2(ay - ey, ax - ex);                       /* Nick -> speaker */
        const char *who = dlg[dlg_i].who;
        int shot = (dlg_i == 0 || !who || !who[0]) ? 0 : is_hero(who) ? 2 : 1;
        int cx = 0, cy = 0, ok = 0;
        if (shot) {
            int fx = shot == 1 ? ex : ax, fy = shot == 1 ? ey : ay;     /* listener: the camera stands behind them */
            int tx2 = shot == 1 ? ax : ex, ty2 = shot == 1 ? ay : ey;   /* speaker */
            int dir = shot == 1 ? a2 : (a2 + 2048) & 4095;
            int back = M(2.8);
            int side = (dir + 1024) & 4095;
            cx = fx - ((icos(dir) * back) >> 14) + ((icos(side) * M(1.6)) >> 14);
            cy = fy - ((isin(dir) * back) >> 14) + ((isin(side) * M(1.6)) >> 14);
            if (shot == 1 && pl.in_car >= 0) {                       /* Nick's car would block the view: swing round it */
                for (int k = 0; k < 2; k++) {
                    int look = (a2 + (k ? -480 : 480)) & 4095;
                    cx = ax - ((icos(look) * M(5.0)) >> 14); cy = ay - ((isin(look) * M(5.0)) >> 14);
                    if (!world_point_blocked(cx, cy, M(0.4))) break;
                }
            }
            ok = !world_point_blocked(cx, cy, M(0.4));
            if (ok) { int la = iatan2(ty2 - cy, tx2 - cx); cam->c = icos(la); cam->s = isin(la); }
        }
        if (!ok) {                                               /* two-shot from the side, slowly pushing in */
            int mx = (ax + ex) >> 1, my = (ay + ey) >> 1;
            int push_in = cut_t / (TICKS / 12); if (push_in > M(1.0)) push_in = M(1.0);
            int d2 = M(5.8) - push_in;
            for (int k = 0; k < 2; k++) {
                int look = (a2 + (k ? -1024 : 1024)) & 4095;
                cx = mx - ((icos(look) * d2) >> 14); cy = my - ((isin(look) * d2) >> 14);
                cam->c = icos(look); cam->s = isin(look);
                if (!world_point_blocked(cx, cy, M(0.4))) break;
            }
        }
        cam->x = cx; cam->y = cy;
        cam->h = M(0.85); cam->gh = 0; cam_rel_h = cam->h;     /* low, so both stand above the subtitles */
    }
    if (gstate == GS_TITLE) {               /* slow orbit over the city */
        int a = (title_t >> 7) & 4095;
        int cx = 4 * 1024 + M(6), cy = 4 * 1024 + M(6);
        cam->c = icos(a + 2048 + 300); cam->s = isin(a + 2048 + 300);
        cam->x = cx + ((icos(a) * M(70)) >> 14); cam->y = cy + ((isin(a) * M(70)) >> 14);
        cam->h = M(14); cam->gh = 0; cam_rel_h = M(14);
    }

    sp = out; nsp = 0; cm = cam;
    int view;
    view_lim = 0x7fffffff;
    if (pl.in_car < 0) {
        int dd = view_dist(plx(), ply(), &view);
        if (dd >= 0) {
            int rel = ((pl.head >> 8) - view) & 4095;
            int dir = ((rel + 256) >> 9) & 7;
            if (pl.dead_t > 0 && pl.dead_kind == 0) push(plx(), ply(), PED_GREY + PED_DOWN + dir);
            else if (pl.aim_t > 0 && pl.weapon >= 2) {     /* two-handed stance for the SMG, shotgun, rifle, rocket launcher */
                int d = ((rel + 128) >> 8) & 15, flip = 0;
                if (d > 8) { d = 16 - d; flip = 0x8000; }
                push(plx(), ply(), (PED_PLAYER + PED_STANCE + d * PED_NSTANCE + pl.weapon - 2) | flip);
            }
            else push(plx(), ply(), ped_frame(PED_PLAYER, rel, pl.moving, pl.running, pl.aim_t > 0, pl.phase));
        }
    }
    view_lim = M(110);
    for (int i = 0; i < ncars; i++) {
        if (!cars[i].active) continue;
        int x = cars[i].x >> 8, y = cars[i].y >> 8;
        int dd = view_dist(x, y, &view);
        if (dd < 0 || dd > M(110)) continue;
        int rel = ((cars[i].head >> 8) - view) & 4095;
        push(x, y, car_frame(cars[i].model, elev_level(dd), rel));
        if (cars[i].police && wanted > 0 && dd < M(90))
            push_h(x, y, FX_SIREN + ((frame_no >> 2) & 1), M(1.65));
    }
    for (int i = 0; i < MAX_PEDS; i++) {
        Ped *p = &peds[i];
        if (!p->active) continue;
        int x = p->x >> 8, y = p->y >> 8;
        view_lim = M(50);
        int dd = view_dist(x, y, &view);
        if (dd < 0) continue;
        int rel = ((p->head >> 8) - view) & 4095;
        int dir = ((rel + 256) >> 9) & 7;
        int base = p->model + (dd > M(13) ? PED_BLOCK : 0);
        if (p->rag) {                                           /* tumbling through the air */
            int c = icos(p->rang); if (c < 0) c = -c;
            int hc = (p->rh >> 8) + M(0.12) + ((c * M(0.83)) >> 14);              /* pelvis height */
            push_h(x, y, ped_tumble_frame(base, rel, p->rang), hc - M(1.0));
        } else if (p->down) {
            int el = dd < M(10) ? 0 : dd < M(25) ? 1 : 2;
            int hx = (icos(p->head >> 8) * M(0.55)) >> 14, hy = (isin(p->head >> 8) * M(0.55)) >> 14;
            push_h(x + hx, y + hy, BLOOD_POOL + el * 4 + p->pool, SPR_DECAL);
            push(x, y, base + PED_DOWN + dir);
        } else {
            int moving = p->state != S_IDLE && !p->aim;
            push(x, y, ped_frame(base, rel, moving, p->run, p->aim, p->phase));
        }
    }
    view_lim = M(40);
    for (int i = 0; i < MAX_CATS; i++) {                        /* cats: 8 directions x (sit, walk 0..3) */
        const StreetCat *c = &street_cats[i];
        if (!c->active) continue;
        if (view_dist(c->x, c->y, &view) < 0) continue;
        int dir = ((((c->head - view) & 4095) + 256) >> 9) & 7;
        static const u16 coat_base[3] = { CAT_TABBY, CAT_BLACK, CAT_GREY };
        int f = c->state == CAT_SIT ? 0 : 1 + ((c->phase / Q8(M(0.09))) & 3);
        push(c->x, c->y, coat_base[c->coat] + dir * 5 + f);
    }
    view_lim = M(50);
    for (int i = 0; i < MAX_ROCKETS; i++) {
        const Rocket *r = &rockets[i];
        if (!r->active) continue;
        int x = r->x >> 8, y = r->y >> 8;
        push_h(x, y, FX_ROCKET, r->h);
        push_h(x - (r->vx >> 13), y - (r->vy >> 13), FX_SMOKE, r->h - M(0.3));   /* smoke trail, ~1.3 m behind */
    }
    for (int i = 0; i < MAX_DROPS; i++)
        if (drops[i].life > 0) push_h(drops[i].x >> 8, drops[i].y >> 8, BLOOD_DROP, drops[i].h >> 8);
    for (int i = 0; i < MAX_FLASH; i++)
        if (flashes[i].t > 0) push_h(flashes[i].x, flashes[i].y, FX_FLASH, flashes[i].h);
    /* mission markers and checkpoints */
    if (mk_col >= 0 && pl.dead_t <= 0 && !cut_on && view_dist(mk_x, mk_y, 0) >= 0) push(mk_x, mk_y, FX_MARKER + mk_col);
    for (int i = 0; i < nshops && !cut_on; i++) {           /* shop doors */
        if (iabs(shops[i].x - cm->x) > M(45) || iabs(shops[i].y - cm->y) > M(45)) continue;
        if (view_dist(shops[i].x, shops[i].y, 0) >= 0) push(shops[i].x, shops[i].y, FX_MARKER + 1);
    }
    for (int i = 0; i < ncars; i++) {                           /* smoke and fire on damaged cars */
        Car *c = &cars[i];
        if (!c->active || (c->dmg < CAR_SMOKE && !(IS_WRECK(c) && c->burn > 0))) continue;
        int cx = c->x >> 8, cy = c->y >> 8;
        int dd = view_dist(cx, cy, 0);
        if (dd < 0 || dd > M(60)) continue;
        int a = c->head >> 8, hl, hw; dims_of(c->model, &hl, &hw);
        int fx = cx + ((icos(a) * (hl * 2 / 3)) >> 14), fy = cy + ((isin(a) * (hl * 2 / 3)) >> 14);   /* engine bay */
        if (IS_WRECK(c)) fx = cx, fy = cy;
        int ph = (int)(frame_no + i * 3);
        if (c->dmg >= CAR_FIRE || IS_WRECK(c)) {
            /* flames sit on the camera side of the body so they draw in front of it */
            int tx = cm->x - cx, ty = cm->y - cy, l = dist_approx(tx, ty);
            int ex = cx, ey = cy;
            if (l > 0) { ex += tx * (hw + M(0.3)) / l; ey += ty * (hw + M(0.3)) / l; }
            if (!IS_WRECK(c) || c->burn > 12 * TICKS) push_h(ex, ey, FX_FIRE + (ph & 3), M(0.6));
            push_h(fx, fy, FX_SMOKE + 2 + ((ph >> 2) & 1), M(2.2) + ((ph & 7) << 2));
        } else push_h(fx, fy, FX_SMOKE + ((ph >> 1) & 3), M(1.0) + ((ph & 7) << 3));
    }
    for (int i = 0; i < MAX_BOOMS; i++) {
        if (booms[i].t <= 0 || booms[i].t >= BOOM_LEN) continue;
        if (view_dist(booms[i].x, booms[i].y, 0) < 0) continue;
        push_h(booms[i].x, booms[i].y, FX_EXPL + booms[i].t * 6 / BOOM_LEN, M(0.5) + booms[i].t * M(2) / BOOM_LEN);
    }
    for (int i = 0; i < MAX_PICKUPS; i++) {
        if (!pickups[i].avail) continue;
        int dd = view_dist(pickups[i].x, pickups[i].y, 0);
        if (dd < 0 || dd > M(50)) continue;
        int bob = (isin((int)(frame_no * 40 + i * 300)) * 3) >> 14;
        push_h(pickups[i].x, pickups[i].y, FX_PICKUP + pickups[i].type, M(0.5) + bob);
    }
    {   /* street props from the blocks around the camera */
        int cbx = cm->x >> 10, cby = cm->y >> 10;
        view_lim = M(55);
        for (int j = cby - 1; j <= cby + 1; j++)
        for (int i = cbx - 1; i <= cbx + 1; i++) {
            if ((unsigned)i >= NB || (unsigned)j >= NB) continue;
            int bi = j * NB + i;
            for (int k = 0; k < prop_count[bi]; k++) {
                const Prop *p = &props[prop_start[bi] + k];
                int dd = view_dist(p->x, p->y, p->head >= 0 ? &view : 0);
                if (dd < 0) continue;
                int f = p->frame;
                if (p->head >= 0) {
                    int rel = (p->head - view) & 4095;
                    f += (f == PROP_BENCH) ? (((rel + 512) >> 10) & 3) : (((rel + 256) >> 9) & 7);
                }
                push_h(p->x, p->y, f, p->h);
            }
        }
        view_lim = M(170);
        int rb0 = clampi(cbx - 3, 0, NB - 1), rb1 = clampi(cbx + 3, 0, NB - 1);
        for (int j = clampi(cby - 3, 0, NB - 1); j <= clampi(cby + 3, 0, NB - 1); j++)
        for (int i = roof_start[j * NB + rb0], ie = roof_start[j * NB + rb1 + 1]; i < ie; i++) {
            int dx = roofs[i].x - cm->x, dy = roofs[i].y - cm->y;
            if (iabs(dx) > M(170) || iabs(dy) > M(170)) continue;
            if (view_dist(roofs[i].x, roofs[i].y, 0) < 0) continue;
            push_h(roofs[i].x, roofs[i].y, roofs[i].frame, roofs[i].h);
        }
    }
    view_lim = M(100);
    {   /* park trees: the blocks within 120 m */
        int cbx = cm->x >> 10, cby = cm->y >> 10;
        int tb0 = clampi(cbx - 2, 0, NB - 1), tb1 = clampi(cbx + 2, 0, NB - 1);
        for (int j = clampi(cby - 2, 0, NB - 1); j <= clampi(cby + 2, 0, NB - 1); j++)
        for (int i = tree_start[j * NB + tb0], ie = tree_start[j * NB + tb1 + 1]; i < ie; i++) {
            int dd = view_dist(g_trees[i].x, g_trees[i].y, 0);
            if (dd < 0 || dd > M(120)) continue;
            push(g_trees[i].x, g_trees[i].y, TREE);
        }
    }
    view_lim = M(70);
    {   /* lamps within 70 m: only the 3x3 blocks around the camera (a block is 64 m) */
        int cbx = cm->x >> 10, cby = cm->y >> 10;
        for (int bj = cby - 1; bj <= cby + 1; bj++)
        for (int bi = cbx - 1; bi <= cbx + 1; bi++) {
            if ((unsigned)bi >= NB || (unsigned)bj >= NB) continue;
            int b = bj * NB + bi;
            for (int i = lamp_start[b], e = lamp_start[b] + lamp_count[b]; i < e; i++) {
                int ldx = lamps[i].x - cm->x, ldy = lamps[i].y - cm->y;
                if (iabs(ldx) > M(70) || iabs(ldy) > M(70)) continue;
                int dd = view_dist(lamps[i].x, lamps[i].y, &view);
                if (dd < 0) continue;
                int rel = (lamps[i].head - view) & 4095;
                push(lamps[i].x, lamps[i].y, LAMP + (((rel + 256) >> 9) & 7));
            }
        }
    }
    /* out of town: coastal street lights and the wooded hills */
    if (cm->x < M(150) || cm->y < M(150) || cm->x > GRID_U - M(150) || cm->y > GRID_U - M(150)) {
        for (int i = 0; i < NCOAST_LAMPS; i++) {
            int lx = coast_lamps[i][0], ly = coast_lamps[i][1];
            if (iabs(lx - cm->x) > M(70) || iabs(ly - cm->y) > M(70)) continue;
            int dd = view_dist(lx, ly, &view);
            if (dd < 0) continue;
            int rel = (coast_lamps[i][2] - view) & 4095;
            push(lx, ly, LAMP + (((rel + 256) >> 9) & 7));
        }
        view_lim = M(120);
        for (int i = 0; i < NRING_TREES; i++) {
            int x = ring_trees[i][0], y = ring_trees[i][1];
            if (iabs(x - cm->x) > M(120) || iabs(y - cm->y) > M(120)) continue;
            if (view_dist(x, y, 0) < 0) continue;
            push(x, y, TREE);
        }
    }
    *nout = nsp;
}

#ifdef HOST
#include <stdio.h>
void game_debug(void)
{
    for (int i = 0; i < 3; i++)
        printf("car%-2d pos %5d,%5d speed %d dmg %d burn %d model %d\n", i, cars[i].x >> 8, cars[i].y >> 8, cars[i].speed / 4096, cars[i].dmg, cars[i].burn, cars[i].model);
    int act = 0; for (int i = 0; i < MAX_PEDS; i++) act += peds[i].active;
    printf("peds active %d  wanted %d  hp %d  police %d\n", act, wanted, pl.hp, active_police());
    printf("pl %d,%d g=%d in_car %d hp %d dead %d\n", plx(), ply(), ground_h(plx(), ply()), pl.in_car, pl.hp, pl.dead_t);
    for (int i = 0; i < MAX_CATS; i++) if (street_cats[i].active) printf("cat%d %5d,%5d state %d coat %d d=%d\n", i, street_cats[i].x, street_cats[i].y, street_cats[i].state, street_cats[i].coat, dist_approx(street_cats[i].x - plx(), street_cats[i].y - ply()) / 16);
    for (int i = 0; i < MAX_ROCKETS; i++) if (rockets[i].active) printf("rocket%d %d,%d\n", i, rockets[i].x >> 8, rockets[i].y >> 8);
    printf("weapon %d ammo %d owned %d\n", pl.weapon, pl.ammo[pl.weapon], pl.owned);
    for (int i = 0; i < MAX_PEDS; i++) if (peds[i].rag) printf("ragdoll%d h %.2fm speed %.1fm/s up %.1fm/s pitch %d bounces %d\n", i, peds[i].rh / 4096.0, dist_approx(peds[i].rvx >> 8, peds[i].rvy >> 8) / 16.0, peds[i].rvz / 4096.0, peds[i].rang, peds[i].rbounce);
    if (pl.in_car >= 0) { Car *c = &cars[pl.in_car]; printf("player car %d,%d g=%d camgh=%d head=%d\n", c->x >> 8, c->y >> 8, ground_h(c->x >> 8, c->y >> 8), cam_gh, c->head >> 8); }
}
void game_debug_ragdoll(void)              /* test: the nearest standing ped is hit by a car doing ~55 km/h */
{
    int bi = -1, bd = 0x7fffffff;
    for (int i = 0; i < PED_CIV_END; i++) { Ped *p = &peds[i]; if (!p->active || p->down) continue;
        int d = dist_approx((p->x >> 8) - plx(), (p->y >> 8) - ply()); if (d < bd) { bd = d; bi = i; } }
    if (bi < 0) return;
    Ped *p = &peds[bi];
    pl.x = p->x - Q8((icos(cam_ang >> 8) * M(6)) >> 14); pl.y = p->y - Q8((isin(cam_ang >> 8) * M(6)) >> 14);
    if (pl.in_car >= 0) { cars[pl.in_car].x = pl.x; cars[pl.in_car].y = pl.y; cars[pl.in_car].head = cam_ang; }
    int a = cam_ang >> 8;
    kill_ped(p, (icos(a) >> 4) * Q8(M(15)) >> 10, (isin(a) >> 4) * Q8(M(15)) >> 10, 0, 5);
}
void game_debug_near_cat(void)              /* test: stand 4 m from the nearest cat, facing it */
{
    int bi = -1, bd = 0x7fffffff;
    for (int i = 0; i < MAX_CATS; i++) { int d = dist_approx(street_cats[i].x - plx(), street_cats[i].y - ply()); if (street_cats[i].active && d < bd) { bd = d; bi = i; } }
    if (bi < 0 || pl.in_car >= 0) return;
    StreetCat *c = &street_cats[bi];
    int a = (c->head + 1024) & 4095;                       /* beside it, on the sidewalk line */
    pl.x = Q8(c->x - ((icos(c->head) * M(3)) >> 14) + ((icos(a) * M(1.6)) >> 14)); pl.y = Q8(c->y - ((isin(c->head) * M(3)) >> 14) + ((isin(a) * M(1.6)) >> 14));
    pl.head = Q8(c->head); cam_ang = pl.head;
}
void game_debug_knock(int ahead_m, int age)
{
    Car *c = &cars[pl.in_car >= 0 ? pl.in_car : 0];
    int a = c->head >> 8;
    Ped *p = &peds[0];
    p->x = c->x + ((icos(a) * Q8(M(ahead_m))) >> 14);
    p->y = c->y + ((isin(a) * Q8(M(ahead_m))) >> 14);
    p->down = 1; p->rag = 0; p->timer = 30 * 256 - age; p->pool = 3; p->head = Q8(700);
    Ped *q = &peds[1]; q->x = p->x + Q8(M(3)); q->y = p->y - Q8(M(3.5)); q->down = 0;
}
void game_debug_play(void) { new_game_state(); gstate = GS_PLAY; }
void game_debug_burn_self(void) { if (pl.in_car >= 0) car_damage(&cars[pl.in_car], 1000, 0); }   /* test: set your own car on fire */
void game_debug_burn(int d)              /* damage the car nearest the player (not the player's) */
{
    int bi = -1, bd = 0x7fffffff;
    for (int i = 0; i < ncars; i++) {
        if (!cars[i].active || i == pl.in_car || IS_WRECK(&cars[i])) continue;
        int dd = dist_approx((cars[i].x >> 8) - plx(), (cars[i].y >> 8) - ply());
        if (dd < bd) { bd = dd; bi = i; }
    }
    if (bi >= 0) car_damage(&cars[bi], d, 1);
}
void game_debug_tp_mission(int m)
{
    if (pl.in_car >= 0) { cars[pl.in_car].ai = AI_NONE; pl.in_car = -1; }
    mission = m; mstate = 0;
    pl.x = Q8(mission_start_x[m] - M(2)); pl.y = Q8(mission_start_y[m]); pl.head = 0; cam_ang = 0;
}
void game_debug_coast(int k)
{
    int i = pl.in_car >= 0 ? pl.in_car : 0;
    Car *c = &cars[i];
    k %= NCOAST_WP;
    int nx = coast_wp[(k + 1) % NCOAST_WP][0], ny = coast_wp[(k + 1) % NCOAST_WP][1];
    int a = iatan2(ny - coast_wp[k][1], nx - coast_wp[k][0]);
    int ox = (isin(a) * M(2)) >> 14, oy = -(icos(a) * M(2)) >> 14;     /* right-hand lane */
    c->x = Q8(coast_wp[k][0] + ox); c->y = Q8(coast_wp[k][1] + oy);
    c->head = Q8(a); c->vx = c->vy = 0; c->speed = 0; cam_ang = Q8(a);
}
void game_debug_street(int n)          /* n = road*4 + dir: start of a long street */
{
    int i = pl.in_car >= 0 ? pl.in_car : 0;
    Car *c = &cars[i];
    int road = n >> 2, dir = n & 3;
    place_on_lane(c, dir, road, dir == 0 || dir == 1 ? 0 : NB, 8);
    c->vx = c->vy = 0; c->speed = 0; cam_ang = c->head; c->ai = AI_NONE;
}
void game_debug_block(int bx, int by, int dir)   /* drop the player's car on road by at cross street bx */
{
    int i = pl.in_car >= 0 ? pl.in_car : 0;
    Car *c = &cars[i];
    place_on_lane(c, dir, by, bx, 20);
    c->vx = c->vy = 0; c->speed = 0; cam_ang = c->head; c->ai = AI_NONE;
    if (pl.in_car < 0) { pl.x = c->x; pl.y = c->y; }
}
void game_debug_shop(int i)            /* stand 3 m in front of shop i's door, facing it */
{
    if (pl.in_car >= 0) { cars[pl.in_car].ai = AI_NONE; pl.in_car = -1; }
    i %= nshops;
    pl.x = Q8(shops[i].x + M(3)); pl.y = Q8(shops[i].y); pl.head = Q8(2048); cam_ang = pl.head; wanted = 0; pl.money += 5000;
}
void game_debug_mission(int m)         /* jump to mission m, standing next to its marker */
{
    mission = m; mstate = 0; phase = 0; wanted = 0;
    if (pl.in_car >= 0) { cars[pl.in_car].ai = AI_NONE; pl.in_car = -1; }
    int x = m < NACT1 ? mission_start_x[m] : M(1) * story_defs[m - NACT1].sx, y = m < NACT1 ? mission_start_y[m] : M(1) * story_defs[m - NACT1].sy;
    pl.x = Q8(x - M(5)); pl.y = Q8(y); pl.head = 0; cam_ang = 0;
}
void game_debug_set(int w, int give_guns)
{
    wanted = w;
    if (give_guns) { pl.owned = 31; pl.ammo[1] = 50; pl.ammo[2] = 200; pl.ammo[3] = 20; pl.ammo[4] = 40; pl.ammo[5] = 10; pl.weapon = 2; }
}
#endif
