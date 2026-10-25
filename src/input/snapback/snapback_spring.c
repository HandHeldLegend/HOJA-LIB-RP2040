#include "input/snapback/snapback_spring.h"
#include "utilities/settings.h"

#include <math.h>
#include <stdbool.h>
#include <stdint.h>

// A free stick follows s[n+1] = a1 s[n] + a2 s[n-1] at the analog rate (2 kHz), with a1/a2 (Q16)
// for the direction it was released from: the stick's own from the app's tuning step, else these
// defaults (a ProGCC 3S left stick, 48 releases fitted: 51-65 Hz, damping 0.12-0.16). Octant 0 is
// +x, counting towards +y.
#define SPRING_Q 16
static const int32_t _spring_default_a1[8] = {126802, 125429, 125618, 124742, 124719, 125250, 126071, 126201};
static const int32_t _spring_default_a2[8] = {-62953, -62076, -62255, -61868, -61619, -62021, -62529, -62543};

// Releases from the edge. The stick is at the edge from here, released once it falls through the
// release line, and for 3 ms after the line a release stays within 45 counts per sample of the free
// spring (worst seen 229 at 3 ms); the thumb is 2-3x further off by then (fewest 353)
#define SPRING_EDGE         1600
#define SPRING_RELEASE      1200
#define SPRING_DECIDE       6
#define SPRING_DEFAULT_DECIDE_STEP  45
// A release crosses centre when the free spring says, a sample late at most; a flick the thumb throws
// across is 4-11 samples late (GCU 2 and ProGCC 3S recordings), as no thumb is as fast as the spring.
// A stick still on its own side 3 samples after the free spring crossed is the thumb, and passes.
#define SPRING_CROSS_MARGIN 3
// For 7 ms after the release line the free spring still says where a released stick must be, so the
// release check carries on through the hold: a hand that pauses near centre, or takes the stick
// somewhere the spring wouldn't, ends the hold as soon as it leaves the spring's path
#define SPRING_FREE_CHECK   14
// On the far side of a centre crossing, a free spring turns back within a quarter swing and gets no
// further out than its crossing speed over its frequency (theta, radians a sample). Real releases reach
// 1.21 and 1.16 of those (GCU 2, ProGCC 3S), turnaround rebounds well under; a hand that eases off after
// centre and then pushes on goes past both. Still going out after 1.4 quarter swings, or out past 1.5x
// the reach, ends a decision or hold: the turnaround check can't tell such a hand from a rebound in
// its first 2.5 ms, and the grab check doesn't see a steady push
#define SPRING_PEAK_MARGIN      1.5
#define SPRING_QUARTER_MARGIN   1.4

// Let go partway back (a turnaround): the stick crosses centre fast, then brakes against the spring
// (stiffness 0.052-0.47 per sample squared seen 2.5 ms on) where the thumb keeps going (-0.03-0.03).
// A crossing faster than 20 counts a sample (40 per ms) is held while its stiffness is measured.
#define SPRING_OUT          300
#define SPRING_DEFAULT_CROSS_SPEED  20
#define SPRING_CHECK        5
#define SPRING_DEFAULT_STIFF_DEN    25   // a spring if stiffness >= 1/25

// While holding, the thumb taking the stick back shows as the next sample missing the spring
#define SPRING_DEFAULT_GRAB_ERR     40
#define SPRING_GRAB_COUNT   3
// The rebound is over once the stick stays inside its deadzone for 10 ms (40-200 counts): with a
// small deadzone, ringing that had looked settled at 200 still showed
#define SPRING_SETTLED_MIN  40
#define SPRING_SETTLED_MAX  200
#define SPRING_SETTLE_COUNT 20
// A free spring is only still at the top of a swing, for a sample or two. Held still far out (moving
// under 1/64 of its distance a sample, 4 samples running) it is the thumb or hand, parked where it
// wants the stick: the grab check misses that, as a still stick is nearly what the spring predicts
#define SPRING_STILL_MIN    600
#define SPRING_STILL_DIV    64
#define SPRING_STILL_COUNT  4
// A spring only ever pulls the stick back towards centre. Speeding up away from centre (out past 300,
// 3 samples running) it is being pushed: a hand that eased off just after centre, which the crossing
// check can take for a spring braking
#define SPRING_PUSH_MIN     300
#define SPRING_PUSH_COUNT   3
#define SPRING_HOLD_MAX     200

typedef enum
{
    SPRING_IDLE,
    SPRING_AT_EDGE,
    SPRING_DECIDING,
    SPRING_CHECKING,
    SPRING_HOLDING,
} spring_state_t;

typedef struct
{
    spring_state_t state;
    uint8_t  octant;
    uint8_t  step;
    uint8_t  grab;
    uint8_t  settle;
    uint8_t  still;
    uint8_t  push;
    uint16_t d_prev;             // the last two samples' distances
    uint16_t d_prev2;
    uint8_t  layer;              // what started the decision or hold: 1 a release, 2 a crossing
    uint8_t  free_crossed;       // samples since the free spring crossed centre, 0xFF before
    bool     crossed;            // the stick has crossed centre since the release line
    uint16_t hold;
    int32_t  edge_x, edge_y;     // where it left the edge
    int32_t  x1, y1, x2, y2;     // the last two samples
    int32_t  fx1, fy1, fx2, fy2; // the free spring since the release line
    int32_t  out_x, out_y;       // the direction it was last out in, for the crossing layer
    int64_t  p1, p2;             // the last two positions along it
    int64_t  q1;                 // the last position along the release direction
    int64_t  l1_fall;            // a release's centre crossing: its speed, samples since, turned back
    uint8_t  l1_n;
    bool     l1_crossed;
    bool     l1_turned;
    int64_t  x_fall;             // the same for a crossing check
    uint8_t  x_n;
    uint8_t  x_octant;
    bool     x_turned;
    int64_t  stiff_num, stiff_den;
} spring_axis_s;

static spring_axis_s _spring[2] = {0};

// The tuning in use, per stick. Settings load on the other core, so each stick has two copies: the
// new one is filled while the other is in use, then becomes live in one write.
typedef struct
{
    int32_t a1[8];
    int32_t a2[8];
    int32_t decide_step;
    int32_t grab_err;
    int32_t stiff_den;
    int32_t cross_speed;
    int32_t settled;
    uint16_t peak_q8[8];         // the furthest out a free spring gets, per unit crossing speed (Q8)
    uint8_t  quarter_n[8];       // the longest it keeps going out after crossing, samples
} spring_params_s;

static spring_params_s _spring_params[2][2];
static volatile uint8_t _spring_live[2] = {0, 0};
static volatile bool _spring_loaded = false;
static volatile bool _spring_tuned[2] = {false, false};

// A tuning the filter can trust: a stable, ringing spring for every octant and sane thresholds
static bool _spring_cfg_valid(const snapbackSpringCfg_s *c)
{
    if (c->version != SNAPBACK_SPRING_CFG_VERSION)
        return false;
    for (int o = 0; o < 8; o++)
    {
        // Between about 30 and 200 Hz, and losing energy (pole inside the unit circle)
        if (c->a1[o] < 98304 || c->a1[o] > 131072)
            return false;
        if (c->a2[o] > -52429 || c->a2[o] <= -65536)
            return false;
    }
    if (c->decide_step < 10u || c->decide_step > 300u)
        return false;
    if (c->grab_err < 10u || c->grab_err > 300u)
        return false;
    if (c->stiff_den < 5u || c->stiff_den > 200u)
        return false;
    if (c->cross_speed < 5u || c->cross_speed > 200u)
        return false;
    return true;
}

static void _spring_cfg_defaults(snapbackSpringCfg_s *c)
{
    c->version = SNAPBACK_SPRING_CFG_VERSION;
    c->flags = 0;
    for (int o = 0; o < 8; o++)
    {
        c->a1[o] = _spring_default_a1[o];
        c->a2[o] = _spring_default_a2[o];
    }
    c->decide_step = SPRING_DEFAULT_DECIDE_STEP;
    c->grab_err = SPRING_DEFAULT_GRAB_ERR;
    c->stiff_den = SPRING_DEFAULT_STIFF_DEN;
    c->cross_speed = SPRING_DEFAULT_CROSS_SPEED;
}

void snapback_spring_load(void)
{
    for (int stick = 0; stick < 2; stick++)
    {
        snapbackSpringCfg_s *c = (stick == 0) ? &analog_config->l_spring : &analog_config->r_spring;
        // Nothing stored (or not usable): the defaults, written back so the app reads what's in use
        if (!_spring_cfg_valid(c))
            _spring_cfg_defaults(c);

        const uint8_t next = (uint8_t)(_spring_loaded ? (1u - _spring_live[stick]) : 0u);
        spring_params_s *p = &_spring_params[stick][next];
        for (int o = 0; o < 8; o++)
        {
            p->a1[o] = c->a1[o];
            p->a2[o] = c->a2[o];
        }
        p->decide_step = c->decide_step;
        p->grab_err = c->grab_err;
        p->stiff_den = c->stiff_den;
        p->cross_speed = c->cross_speed;
        const int32_t dz = (stick == 0) ? analog_config->l_deadzone : analog_config->r_deadzone;
        p->settled = (dz < SPRING_SETTLED_MIN) ? SPRING_SETTLED_MIN : ((dz > SPRING_SETTLED_MAX) ? SPRING_SETTLED_MAX : dz);
        for (int o = 0; o < 8; o++)
        {
            // Worked out here, once, so the per-sample tests stay integer
            const double a1 = c->a1[o] / 65536.0;
            const double a2 = c->a2[o] / 65536.0;
            double ratio = a1 / (2.0 * sqrt(-a2));
            ratio = (ratio > 1.0) ? 1.0 : ((ratio < -1.0) ? -1.0 : ratio);
            double theta = acos(ratio);
            theta = (theta < 0.01) ? 0.01 : theta;
            p->peak_q8[o] = (uint16_t)(256.0 * SPRING_PEAK_MARGIN / theta);
            p->quarter_n[o] = (uint8_t)ceil(SPRING_QUARTER_MARGIN * (M_PI / 2.0) / theta);
        }
        __atomic_store_n(&_spring_live[stick], next, __ATOMIC_RELEASE);
        _spring_tuned[stick] = (c->flags & SNAPBACK_SPRING_FLAG_TUNED) != 0;
    }
    _spring_loaded = true;
}

bool snapback_spring_tuned(uint8_t stick)
{
    return _spring_loaded && _spring_tuned[(stick > 1) ? 1 : stick];
}

// The octant of (x, y): within 22.5 degrees of an axis (tan 22.5 = 0.4142) or a diagonal
static uint8_t _spring_octant(int32_t x, int32_t y)
{
    const int32_t ax = (x < 0) ? -x : x;
    const int32_t ay = (y < 0) ? -y : y;
    if (ay * 10000 <= ax * 4142)
        return (x >= 0) ? 0 : 4;
    if (ax * 10000 <= ay * 4142)
        return (y >= 0) ? 2 : 6;
    if (x >= 0)
        return (y >= 0) ? 1 : 7;
    return (y >= 0) ? 3 : 5;
}

static inline int32_t _spring_next(const spring_params_s *k, uint8_t octant, int32_t s1, int32_t s2)
{
    return (int32_t)(((int64_t)k->a1[octant] * s1 + (int64_t)k->a2[octant] * s2) >> SPRING_Q);
}

static inline int64_t _spring_sq(int32_t dx, int32_t dy)
{
    return (int64_t)dx * dx + (int64_t)dy * dy;
}

// Past a centre crossing (q along the direction it came from, so the far side is negative): out further
// than a free spring could get for its crossing speed, or still going out when it would have turned back
static bool _spring_hand(const spring_params_s *k, uint8_t octant, int64_t q, int64_t q_prev, int64_t fall,
                         uint8_t n, bool turned)
{
    const bool beyond = (-q * 256) > (fall * k->peak_q8[octant]);
    const bool late = !turned && (q < 0) && (q < q_prev) && (n > k->quarter_n[octant]);
    return beyond || late;
}

static void _spring_hold_start(spring_axis_s *s)
{
    s->state = SPRING_HOLDING;
    s->hold = 0;
    s->grab = 0;
    s->settle = 0;
    s->still = 0;
    s->push = 0;
}

void snapback_spring_process(analog_axis_s *input, analog_axis_s *output)
{
    // Until the settings have loaded there is no spring to compare with
    if (!_spring_loaded)
    {
        *output = *input;
        return;
    }

    const uint8_t stick = (input->axis_idx > 1) ? 1u : input->axis_idx;
    spring_axis_s *s = &_spring[stick];
    const spring_params_s *k = &_spring_params[stick][__atomic_load_n(&_spring_live[stick], __ATOMIC_ACQUIRE)];
    const int32_t x = input->x;
    const int32_t y = input->y;
    const uint16_t d = input->distance;

    // Everything is worked out every sample, whatever the state, so the cost never changes: the
    // spring one step on from the last two samples, the free spring running on from the release line,
    // and the position, acceleration and speed along the direction it was last out in
    const int32_t px = _spring_next(k, s->octant, s->x1, s->x2);
    const int32_t py = _spring_next(k, s->octant, s->y1, s->y2);
    const int32_t fx = _spring_next(k, s->octant, s->fx1, s->fx2);
    const int32_t fy = _spring_next(k, s->octant, s->fy1, s->fy2);
    const int64_t step_err = _spring_sq(x - px, y - py);
    const int64_t free_err = _spring_sq(x - fx, y - fy);
    const bool past_centre = ((int64_t)x * s->edge_x + (int64_t)y * s->edge_y) < 0;

    // Along the out direction everything is scaled by its length; the comparisons allow for that
    int64_t p0 = (int64_t)x * s->out_x + (int64_t)y * s->out_y;
    const int64_t acc = p0 - 2 * s->p1 + s->p2;
    const int64_t out_sq = (int64_t)s->out_x * s->out_x + (int64_t)s->out_y * s->out_y;
    const int64_t fall = s->p1 - p0;
    const bool crossing = (out_sq > 0) && (s->p1 >= 0) && (p0 < 0) &&
                          (fall * fall >= (int64_t)k->cross_speed * k->cross_speed * out_sq);

    bool neutral = false;

    // After a release line, a stick that stays on its own side well after the free spring crossed is
    // being thrown across by the thumb: it passes now instead of waiting for the grab check. Its
    // crossing still gets the crossing check: a turnaround eased back slowly and then let go looks the
    // same up to here, and only its stiffness tells it from the thumb
    if (((s->state == SPRING_DECIDING) || (s->state == SPRING_HOLDING)) && (s->layer == 1) && !s->crossed)
    {
        s->crossed = past_centre;
        if (s->free_crossed == 0xFFu)
        {
            if (((int64_t)fx * s->edge_x + (int64_t)fy * s->edge_y) < 0)
                s->free_crossed = 0;
        }
        else
        {
            s->free_crossed++;
        }
        if (!s->crossed && (s->free_crossed != 0xFFu) && (s->free_crossed >= SPRING_CROSS_MARGIN))
        {
            s->state = SPRING_IDLE;
        }
    }

    // The far side of a centre crossing (see SPRING_PEAK_MARGIN): a hand there ends the decision or
    // hold, and this sample passes as it is
    const int64_t q0 = (int64_t)x * s->edge_x + (int64_t)y * s->edge_y;
    bool hand = false;
    if (((s->state == SPRING_DECIDING) || (s->state == SPRING_HOLDING)) && (s->layer == 1))
    {
        if (!s->l1_crossed)
        {
            if ((s->q1 >= 0) && (q0 < 0))
            {
                s->l1_crossed = true;
                s->l1_fall = s->q1 - q0;
                s->l1_n = 0;
                s->l1_turned = false;
            }
        }
        else
        {
            s->l1_n++;
            if ((q0 < 0) && (q0 > s->q1))
                s->l1_turned = true;
            if (_spring_hand(k, s->octant, q0, s->q1, s->l1_fall, s->l1_n, s->l1_turned))
            {
                s->state = (d >= SPRING_EDGE) ? SPRING_AT_EDGE : SPRING_IDLE;
                hand = true;
            }
        }
    }
    else if ((s->state == SPRING_CHECKING) || ((s->state == SPRING_HOLDING) && (s->layer == 2)))
    {
        s->x_n++;
        if ((p0 < 0) && (p0 > s->p1))
            s->x_turned = true;
        if (_spring_hand(k, s->x_octant, p0, s->p1, s->x_fall, s->x_n, s->x_turned))
        {
            s->state = SPRING_IDLE;
            hand = true;
        }
    }

    if (!hand)
    {
        switch (s->state)
        {
            case SPRING_IDLE:
                if (d >= SPRING_EDGE)
                {
                    s->state = SPRING_AT_EDGE;
                }
                else if (crossing)
                {
                    s->state = SPRING_CHECKING;
                    s->step = 0;
                    s->stiff_num = 0;
                    s->stiff_den = 0;
                    s->x_fall = fall;
                    s->x_n = 0;
                    s->x_octant = _spring_octant(s->out_x, s->out_y);
                    s->x_turned = false;
                    // This sample is already past centre: hold it with the rest of the check
                    neutral = true;
                }
                break;

            case SPRING_AT_EDGE:
                if (d >= SPRING_EDGE)
                {
                    s->edge_x = x;
                    s->edge_y = y;
                }
                else if (d < SPRING_RELEASE)
                {
                    // Released, or the thumb pulling back: the free spring starts from here
                    s->octant = _spring_octant(s->edge_x, s->edge_y);
                    s->fx2 = s->x1;
                    s->fy2 = s->y1;
                    s->fx1 = x;
                    s->fy1 = y;
                    s->step = 0;
                    s->layer = 1;
                    s->free_crossed = 0xFFu;
                    s->crossed = false;
                    s->l1_crossed = false;
                    s->state = SPRING_DECIDING;
                }
                break;

            case SPRING_DECIDING:
            {
                s->step++;
                s->fx2 = s->fx1;
                s->fy2 = s->fy1;
                s->fx1 = fx;
                s->fy1 = fy;
                const int32_t limit = k->decide_step * s->step;
                if (s->step >= 2 && free_err > (int64_t)limit * limit)
                {
                    // Not the spring: the thumb has it
                    s->state = SPRING_IDLE;
                }
                else
                {
                    if (s->step >= SPRING_DECIDE)
                        _spring_hold_start(s);
                    // Until it's known, a swing past centre isn't shown
                    neutral = past_centre || (s->state == SPRING_HOLDING);
                }
                break;
            }

            case SPRING_CHECKING:
                // The acceleration is the previous sample's, so its position goes with it
                s->step++;
                if (s->step >= 2)
                {
                    s->stiff_num += acc * s->p1;
                    s->stiff_den += s->p1 * s->p1;
                }
                if (s->step >= SPRING_CHECK)
                {
                    if (s->stiff_den > 0 && -s->stiff_num * k->stiff_den >= s->stiff_den)
                    {
                        s->octant = _spring_octant(s->out_x, s->out_y);
                        _spring_hold_start(s);
                        s->layer = 2;
                    }
                    else
                    {
                        s->state = SPRING_IDLE;
                    }
                }
                neutral = (s->state == SPRING_CHECKING) || (s->state == SPRING_HOLDING);
                break;

            case SPRING_HOLDING:
            {
                const bool free_check = (s->layer == 1) && (s->step < SPRING_FREE_CHECK);
                const int32_t free_limit = k->decide_step * (s->step + 1);
                if (free_check && (free_err > (int64_t)free_limit * free_limit))
                {
                    // Off the free spring's path: the thumb or hand has it
                    s->state = (d >= SPRING_EDGE) ? SPRING_AT_EDGE : SPRING_IDLE;
                    break;
                }
                // Keep the free spring running for the release check and the throw check above
                if ((s->layer == 1) && (!s->crossed || free_check))
                {
                    s->fx2 = s->fx1;
                    s->fy2 = s->fy1;
                    s->fx1 = fx;
                    s->fy1 = fy;
                }
                if (free_check)
                    s->step++;
                s->hold++;
                s->grab = (step_err > (int64_t)k->grab_err * k->grab_err) ? (uint8_t)(s->grab + 1u) : 0u;
                s->settle = (d < k->settled) ? (uint8_t)(s->settle + 1u) : 0u;
                {
                    const int32_t moved = (int32_t)d - (int32_t)s->d_prev;
                    const bool still = (d >= SPRING_STILL_MIN) && ((moved < 0 ? -moved : moved) * SPRING_STILL_DIV <= (int32_t)d);
                    s->still = still ? (uint8_t)(s->still + 1u) : 0u;
                    const int32_t speeding_out = (int32_t)d - 2 * (int32_t)s->d_prev + (int32_t)s->d_prev2;
                    const bool push = (d >= SPRING_PUSH_MIN) && (moved > 0) && (speeding_out > 0);
                    s->push = push ? (uint8_t)(s->push + 1u) : 0u;
                }
                if ((s->push >= SPRING_PUSH_COUNT) || (s->still >= SPRING_STILL_COUNT) || (s->grab >= SPRING_GRAB_COUNT))
                    s->state = (d >= SPRING_EDGE) ? SPRING_AT_EDGE : SPRING_IDLE;
                else if (s->settle >= SPRING_SETTLE_COUNT || s->hold >= SPRING_HOLD_MAX)
                    s->state = SPRING_IDLE;
                else
                    neutral = true;
                break;
            }
        }

    }
    // A thumb move that ends at the edge is watched again for the next release
    if ((s->state == SPRING_IDLE) && (d >= SPRING_EDGE))
        s->state = SPRING_AT_EDGE;

    // The out direction follows the stick while nothing is being decided or held
    if (((s->state == SPRING_IDLE) || (s->state == SPRING_AT_EDGE)) && (d > SPRING_OUT))
    {
        s->out_x = x;
        s->out_y = y;
        p0 = (int64_t)x * x + (int64_t)y * y;
        s->p1 = p0;
        s->p2 = p0;
    }
    else
    {
        s->p2 = s->p1;
        s->p1 = p0;
    }

    s->q1 = (int64_t)x * s->edge_x + (int64_t)y * s->edge_y;
    s->d_prev2 = s->d_prev;
    s->d_prev = d;
    s->x2 = s->x1;
    s->y2 = s->y1;
    s->x1 = x;
    s->y1 = y;

    *output = *input;
    if (neutral)
    {
        output->x = 0;
        output->y = 0;
        output->distance = 0;
    }
}
