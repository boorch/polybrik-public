#!/usr/bin/env python3
"""Audio checks (numpy only). See README.md for the expected numbers.

  analyze.py pitch FILE [--start S] [--dur S]
  analyze.py onsets FILE [--threshold DB]
  analyze.py compare A B [--max-offset N]
"""
import argparse
import sys
import wave

import numpy as np


def read_wav(path):
    """16-bit PCM WAV -> (rate, mono float32 in [-1, 1]); channels are averaged."""
    with wave.open(path, "rb") as w:
        if w.getsampwidth() != 2:
            sys.exit(f"{path}: only 16-bit PCM is supported")
        rate, ch = w.getframerate(), w.getnchannels()
        pcm = np.frombuffer(w.readframes(w.getnframes()), dtype="<i2")
    x = pcm.reshape(-1, ch).astype(np.float32).mean(axis=1) / 32768.0
    return rate, x


def pitch(args):
    rate, x = read_wav(args.file)
    a = int(args.start * rate)
    b = len(x) if args.dur is None else min(len(x), a + int(args.dur * rate))
    seg = x[a:b]
    if len(seg) < 64:
        sys.exit("segment too short")
    seg = seg * np.hanning(len(seg))
    n = 1 << (int(len(seg) - 1).bit_length() + 2)  # 4x zero padding
    mag = np.abs(np.fft.rfft(seg, n))
    hz_per_bin = rate / n
    mag[: int(20 / hz_per_bin) + 1] = 0  # ignore DC and sub-20 Hz leakage

    def peak(m):
        k = int(np.argmax(m))
        if 0 < k < len(m) - 1 and m[k] > 0:
            l, c, r = np.log(m[k - 1] + 1e-12), np.log(m[k] + 1e-12), np.log(m[k + 1] + 1e-12)
            d = 0.5 * (l - r) / (l - 2 * c + r) if (l - 2 * c + r) != 0 else 0.0
        else:
            d = 0.0
        return (k + d) * hz_per_bin, 20 * np.log10(m[k] / (len(seg) / 4) + 1e-12)

    f1, db1 = peak(mag)
    print(f"segment {a / rate:.3f}s..{b / rate:.3f}s ({b - a} samples @ {rate} Hz)")
    if db1 < -60.0:
        print(f"silent segment (strongest peak {db1:.1f} dB, below -60 dB), no pitch")
        return
    m2 = mag.copy()
    k1 = int(round(f1 / hz_per_bin))
    w = max(8, int(0.03 * f1 / hz_per_bin))  # mask +-3% around the first peak
    m2[max(0, k1 - w) : k1 + w + 1] = 0
    f2, db2 = peak(m2)
    print(f"peak1 {f1:.3f} Hz {db1:.1f} dB")
    print(f"peak2 {f2:.3f} Hz {db2:.1f} dB")


def onsets(args):
    rate, x = read_wav(args.file)
    # 0.5 ms blocks: a retrigger of the same sample dips to zero for only 1 to 2 ms (the old
    # voice stops, the new one fades in), which 5 ms blocks average away.
    hop = max(1, int(0.0005 * rate))
    nblk = len(x) // hop
    rms = np.sqrt((x[: nblk * hop].reshape(nblk, hop) ** 2).mean(axis=1))
    peak = rms.max()
    if peak <= 0:
        print("silent file, no onsets")
        return
    env = 20 * np.log10(rms / peak + 1e-12)
    # Hysteresis: an onset is a rise above the threshold after the envelope sat 15 dB below it.
    # The block RMS of a sine ripples by up to 14 dB at 220 Hz with 0.5 ms blocks, so a plain
    # crossing fires repeatedly on a slow release ramp; a retrigger gap goes to zero and re-arms.
    above = env > args.threshold
    refractory = int(0.05 * rate / hop)  # 50 ms
    times, last, armed = [], -refractory, True
    for i in range(nblk):
        if env[i] < args.threshold - 15:
            armed = True
        elif above[i] and armed and i - last >= refractory:
            times.append(i * hop / rate)
            last = i
            armed = False
    print(f"{len(times)} onsets (threshold {args.threshold} dB rel. peak, 0.5 ms blocks)")
    for i, t in enumerate(times):
        gap = f"  +{t - times[i - 1]:.4f} s" if i else ""
        print(f"  {t:.4f} s{gap}")


def compare(args):
    ra, a = read_wav(args.a)
    rb, b = read_wav(args.b)
    if ra != rb:
        sys.exit(f"rates differ: {ra} vs {rb}")
    n = args.max_offset
    # cross-correlation by FFT; lag>0 means b is late (b[i+lag] ~ a[i])
    size = 1 << int(len(a) + len(b) - 1).bit_length()
    fa, fb = np.fft.rfft(a, size), np.fft.rfft(b, size)
    xc = np.fft.irfft(fb * np.conj(fa), size)
    lags = np.arange(-n, n + 1)
    best = int(lags[np.argmax(xc[lags % size])])
    # align: drop `best` samples from the late one
    aa, bb = (a, b[best:]) if best >= 0 else (a[-best:], b)
    m = min(len(aa), len(bb))
    aa, bb = aa[:m], bb[:m]
    diff = bb - aa
    rms_a = np.sqrt((aa**2).mean())
    rms_d = np.sqrt((diff**2).mean())
    db = 20 * np.log10(rms_d / rms_a) if rms_a > 0 and rms_d > 0 else float("-inf")
    print(f"offset {best} samples ({best / ra * 1000:.3f} ms, B relative to A)")
    print(f"aligned length {m} samples ({m / ra:.4f} s); A {len(a)} B {len(b)}")
    print(f"max abs diff {np.abs(diff).max() * 32768:.1f} (16-bit units)")
    print(f"rms diff {db:.1f} dB rel. A")
    if m < 0.9 * max(len(a), len(b)):
        print(f"WARNING: aligned window covers only {100 * m / max(len(a), len(b)):.0f}% of the longer file")
    if db > -20.0:
        print("WARNING: B does not match A (rms diff above -20 dB), the offset is not meaningful")


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = p.add_subparsers(dest="cmd", required=True)
    s = sub.add_parser("pitch")
    s.add_argument("file")
    s.add_argument("--start", type=float, default=0.0)
    s.add_argument("--dur", type=float, default=None)
    s.set_defaults(fn=pitch)
    s = sub.add_parser("onsets")
    s.add_argument("file")
    s.add_argument("--threshold", type=float, default=-20.0)
    s.set_defaults(fn=onsets)
    s = sub.add_parser("compare")
    s.add_argument("a")
    s.add_argument("b")
    s.add_argument("--max-offset", type=int, default=4410)
    s.set_defaults(fn=compare)
    args = p.parse_args()
    args.fn(args)


if __name__ == "__main__":
    main()
