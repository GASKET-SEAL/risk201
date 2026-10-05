#include "../include/units.hpp"

namespace risc201 {

namespace {

using U64 = uint64_t;

inline bool bitAt(Word w, int i) { return ((w >> i) & 1u) != 0; }

AdderOut finish(Word a, Word b, Word sum, bool carry) {
    AdderOut r;
    r.sum = sum;
    r.carry = carry;
    r.overflow = (((~(a ^ b)) & (a ^ sum)) >> 31) != 0;
    return r;
}

AdderOut rippleAdd(Word a, Word b, bool cin) {
    Word sum = 0;
    bool c = cin;
    for (int i = 0; i < 32; ++i) {
        bool x = bitAt(a, i);
        bool y = bitAt(b, i);
        bool s = (x != y) != c;
        c = (x && y) || (c && (x != y));
        sum |= Word(s) << i;
    }
    return finish(a, b, sum, c);
}

AdderOut lookaheadAdd(Word a, Word b, bool cin) {
    const int groups = 8;
    Word gw = a & b;
    Word pw = a ^ b;
    bool g[groups][4], p[groups][4];
    bool gg[groups], gp[groups], gc[groups + 1];

    for (int j = 0; j < groups; ++j) {
        for (int t = 0; t < 4; ++t) {
            g[j][t] = bitAt(gw, 4 * j + t);
            p[j][t] = bitAt(pw, 4 * j + t);
        }
        gg[j] = g[j][3] || (p[j][3] && g[j][2]) || (p[j][3] && p[j][2] && g[j][1]) ||
                (p[j][3] && p[j][2] && p[j][1] && g[j][0]);
        gp[j] = p[j][3] && p[j][2] && p[j][1] && p[j][0];
    }

    gc[0] = cin;
    for (int j = 1; j <= groups; ++j) {
        bool all = true;
        for (int k = 0; k < j; ++k) all = all && gp[k];
        bool c = cin && all;
        for (int i = 0; i < j; ++i) {
            bool t = gg[i];
            for (int k = i + 1; k < j; ++k) t = t && gp[k];
            c = c || t;
        }
        gc[j] = c;
    }

    Word sum = 0;
    for (int j = 0; j < groups; ++j) {
        bool c[4];
        c[0] = gc[j];
        c[1] = g[j][0] || (p[j][0] && c[0]);
        c[2] = g[j][1] || (p[j][1] && g[j][0]) || (p[j][1] && p[j][0] && c[0]);
        c[3] = g[j][2] || (p[j][2] && g[j][1]) || (p[j][2] && p[j][1] && g[j][0]) ||
               (p[j][2] && p[j][1] && p[j][0] && c[0]);
        for (int t = 0; t < 4; ++t) sum |= Word(p[j][t] != c[t]) << (4 * j + t);
    }
    return finish(a, b, sum, gc[groups]);
}

AdderOut koggeStoneAdd(Word a, Word b, bool cin) {
    Word p = a ^ b;
    Word gen = (a & b) | (p & Word(cin));
    Word prop = p;
    for (int d = 1; d < 32; d <<= 1) {
        gen |= prop & (gen << d);
        prop &= (prop << d) | ((1u << d) - 1u);
    }
    Word carries = (gen << 1) | Word(cin);
    return finish(a, b, p ^ carries, ((gen >> 31) & 1u) != 0);
}

U64 add64(AdderKind k, U64 x, U64 y, bool cin) {
    AdderOut lo = adderAdd(k, Word(x), Word(y), cin);
    AdderOut hi = adderAdd(k, Word(x >> 32), Word(y >> 32), lo.carry);
    return (U64(hi.sum) << 32) | lo.sum;
}

U64 sub64(AdderKind k, U64 x, U64 y) { return add64(k, x, ~y, true); }

Word mulShiftAdd(AdderKind k, Word a, Word b) {
    Word acc = 0;
    for (int i = 0; i < 32; ++i)
        if (bitAt(b, i)) acc = adderAdd(k, acc, a << i).sum;
    return acc;
}

Word mulBooth4(AdderKind k, Word a, Word b) {
    Word acc = 0;
    bool prev = false;
    for (int i = 0; i < 16; ++i) {
        bool lo = bitAt(b, 2 * i);
        bool hi = bitAt(b, 2 * i + 1);
        int digit = int(lo) + int(prev) - 2 * int(hi);
        prev = hi;
        Word pp = a << (2 * i);
        switch (digit) {
        case 1:  acc = adderAdd(k, acc, pp).sum; break;
        case 2:  acc = adderAdd(k, acc, pp << 1).sum; break;
        case -1: acc = adderSub(k, acc, pp).sum; break;
        case -2: acc = adderSub(k, acc, pp << 1).sum; break;
        default: break;
        }
    }
    return acc;
}

DivOut divRestoring(AdderKind k, Word n, Word d) {
    Word q = 0;
    Word r = 0;
    for (int i = 31; i >= 0; --i) {
        r = (r << 1) | ((n >> i) & 1u);
        AdderOut t = adderSub(k, r, d);
        if (t.carry) {
            r = t.sum;
            q |= 1u << i;
        }
    }
    return DivOut{q, r};
}

DivOut divNonRestoring(AdderKind k, Word n, Word d) {
    U64 r = 0;
    Word q = 0;
    for (int i = 31; i >= 0; --i) {
        U64 shifted = (r << 1) | U64(bitAt(n, i));
        r = (r >> 63) ? add64(k, shifted, d, false) : sub64(k, shifted, d);
        if (!(r >> 63)) q |= 1u << i;
    }
    if (r >> 63) r = add64(k, r, d, false);
    return DivOut{q, Word(r)};
}

} // namespace

AdderOut adderAdd(AdderKind kind, Word a, Word b, bool cin) {
    switch (kind) {
    case AdderKind::Ripple:     return rippleAdd(a, b, cin);
    case AdderKind::Lookahead:  return lookaheadAdd(a, b, cin);
    case AdderKind::KoggeStone: return koggeStoneAdd(a, b, cin);
    }
    return rippleAdd(a, b, cin);
}

AdderOut adderSub(AdderKind kind, Word a, Word b) { return adderAdd(kind, a, ~b, true); }

Word negate(AdderKind kind, Word a) { return adderSub(kind, 0, a).sum; }

Word multiply(MulKind kind, AdderKind adder, Word a, Word b) {
    switch (kind) {
    case MulKind::ShiftAdd: return mulShiftAdd(adder, a, b);
    case MulKind::Booth4:   return mulBooth4(adder, a, b);
    }
    return mulShiftAdd(adder, a, b);
}

DivOut divideSigned(DivKind kind, AdderKind adder, Word a, Word b) {
    bool na = bitAt(a, 31);
    bool nb = bitAt(b, 31);
    Word ma = na ? negate(adder, a) : a;
    Word mb = nb ? negate(adder, b) : b;
    DivOut u = (kind == DivKind::Restoring) ? divRestoring(adder, ma, mb)
                                            : divNonRestoring(adder, ma, mb);
    DivOut out;
    out.quotient = (na != nb) ? negate(adder, u.quotient) : u.quotient;
    out.remainder = na ? negate(adder, u.remainder) : u.remainder;
    return out;
}

Word barrelShift(ShiftKind kind, Word value, Word amount) {
    Word sh = amount & 0x1Fu;
    Word fill = (kind == ShiftKind::ASR && bitAt(value, 31)) ? 0xFFFFFFFFu : 0u;
    for (int s = 0; s < 5; ++s) {
        if (!bitAt(sh, s)) continue;
        int n = 1 << s;
        switch (kind) {
        case ShiftKind::LSL: value = value << n; break;
        case ShiftKind::LSR: value = value >> n; break;
        case ShiftKind::ASR: value = (value >> n) | (fill << (32 - n)); break;
        }
    }
    return value;
}

const char* toString(AdderKind kind) {
    switch (kind) {
    case AdderKind::Ripple:     return "Ripple";
    case AdderKind::Lookahead:  return "Lookahead";
    case AdderKind::KoggeStone: return "KoggeStone";
    }
    return "?";
}

const char* toString(MulKind kind) {
    switch (kind) {
    case MulKind::ShiftAdd: return "ShiftAdd";
    case MulKind::Booth4:   return "Booth4";
    }
    return "?";
}

const char* toString(DivKind kind) {
    switch (kind) {
    case DivKind::Restoring:    return "Restoring";
    case DivKind::NonRestoring: return "NonRestoring";
    }
    return "?";
}

} // namespace risc201
