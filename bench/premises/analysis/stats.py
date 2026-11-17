"""Small statistics helpers (no numpy): medians, exact order-statistic CIs, bootstrap."""
import math
import random


def median(v):
    s = sorted(v)
    n = len(s)
    if n == 0:
        return float("nan")
    return s[n // 2] if n % 2 else 0.5 * (s[n // 2 - 1] + s[n // 2])


def binom_cdf(k, n, p=0.5):
    return sum(math.comb(n, i) * p ** i * (1 - p) ** (n - i) for i in range(0, k + 1))


def median_ci(v, conf=0.95):
    """Distribution-free CI for the median: order statistics x(l), x(u) (1-based) with
    P(x(l) <= median <= x(u)) >= conf; returns (lo, hi, coverage)."""
    s = sorted(v)
    n = len(s)
    if n < 6:
        return (s[0], s[-1], 1 - 2 * 0.5 ** n)
    best = None
    for l in range(1, n // 2 + 1):
        u = n + 1 - l
        cov = binom_cdf(u - 1, n) - binom_cdf(l - 1, n)
        if cov >= conf:
            best = (s[l - 1], s[u - 1], cov)
        else:
            break
    return best


T975 = {1: 12.706, 2: 4.303, 3: 3.182, 4: 2.776, 5: 2.571, 6: 2.447, 7: 2.365, 8: 2.306, 9: 2.262,
        10: 2.228, 11: 2.201, 12: 2.179, 13: 2.160, 14: 2.145, 15: 2.131, 16: 2.120, 17: 2.110,
        18: 2.101, 19: 2.093, 20: 2.086, 25: 2.060, 30: 2.042}


def t975(df):
    if df >= 30:
        return 1.96 if df > 120 else 2.0
    return T975.get(df) or T975[max(k for k in T975 if k <= df)]


def geo_ci(ratios, conf=0.95):
    """Geometric mean with a t interval in log space."""
    logs = [math.log(r) for r in ratios]
    n = len(logs)
    m = sum(logs) / n
    if n < 2:
        return math.exp(m), math.exp(m), math.exp(m)
    sd = math.sqrt(sum((x - m) ** 2 for x in logs) / (n - 1))
    h = t975(n - 1) * sd / math.sqrt(n)
    return math.exp(m), math.exp(m - h), math.exp(m + h)


def bootstrap(stat, groups, b=2000, seed=1):
    """Percentile bootstrap of stat(sample) over resampled index lists of len(groups)."""
    rng = random.Random(seed)
    n = len(groups)
    out = []
    for _ in range(b):
        idx = [rng.randrange(n) for _ in range(n)]
        val = stat([groups[i] for i in idx])
        if val is not None and not math.isnan(val):
            out.append(val)
    out.sort()
    if not out:
        return (float("nan"), float("nan"), 0)
    lo = out[int(0.025 * len(out))]
    hi = out[min(len(out) - 1, int(0.975 * len(out)))]
    return lo, hi, len(out) / b


def verdict(med, lo, hi, win=0.95, loss=1.05):
    v = "win" if med < win else ("loss" if med > loss else "tie")
    if v == "win":
        sure = hi < win
    elif v == "loss":
        sure = lo > loss
    else:
        sure = lo >= win and hi <= loss
    return v, sure
