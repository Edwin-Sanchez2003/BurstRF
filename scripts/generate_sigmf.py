#!/usr/bin/env python3
"""
Generate SigMF recordings (.sigmf-meta + .sigmf-data pairs) for validating a
spectrogram tool.

Samples are generated and written in chunks, so recordings can be any length,
including multi-GiB files, without holding them in RAM.

Usage:
    python generate_sigmf_fixtures.py [output_dir] [--large]

    --large   also write the 2 GiB recording (needs 2 GiB free disk space
              and a few minutes of run time)

Requires: numpy
"""

import json
import os
import sys
from dataclasses import dataclass, field
from typing import Callable, Optional

import numpy as np

SIGMF_VERSION = "1.2.0"
CHUNK = 1 << 22          # samples generated and written per step (~32 MiB for cf32)

BYTES_PER_SAMPLE = {
    "cf32_le": 8,
    "cf32_be": 8,
    "cf64_le": 16,
    "ci16_le": 4,
    "ci8": 2,
    "cu8": 2,
    "rf32_le": 4,
}

# A signal is a function (start_sample, count) -> complex numpy array of length count.
# `start_sample` is the global sample index, so phase stays continuous across chunks.
SignalFn = Callable[[int, int], np.ndarray]


@dataclass
class Spec:
    name: str
    description: str
    datatype: str
    sample_rate: float
    signal: SignalFn
    total_samples: Optional[int] = None
    target_bytes: Optional[int] = None
    center_freq: Optional[float] = None
    annotations: list = field(default_factory=list)
    captures: Optional[list] = None
    author: str = "sigmf-fixture-generator"

    def n_samples(self) -> int:
        if self.total_samples is not None:
            return self.total_samples
        return self.target_bytes // BYTES_PER_SAMPLE[self.datatype]


# ---------------------------------------------------------------------------
# Encoding
# ---------------------------------------------------------------------------

def encode_samples(x: np.ndarray, dt: str) -> bytes:
    """Serialize a chunk of complex samples according to a SigMF datatype."""
    if dt == "cf32_le":
        return x.astype("<c8").tobytes()
    if dt == "cf32_be":
        return x.astype(">c8").tobytes()
    if dt == "cf64_le":
        return x.astype("<c16").tobytes()
    if dt == "ci16_le":
        out = np.empty(2 * len(x), dtype="<i2")
        out[0::2] = np.clip(np.round(x.real * 32767), -32768, 32767).astype("<i2")
        out[1::2] = np.clip(np.round(x.imag * 32767), -32768, 32767).astype("<i2")
        return out.tobytes()
    if dt == "ci8":
        out = np.empty(2 * len(x), dtype="i1")
        out[0::2] = np.clip(np.round(x.real * 127), -128, 127).astype("i1")
        out[1::2] = np.clip(np.round(x.imag * 127), -128, 127).astype("i1")
        return out.tobytes()
    if dt == "cu8":
        # Offset binary, as produced by RTL-SDR: 127.5 is zero.
        out = np.empty(2 * len(x), dtype="u1")
        out[0::2] = np.clip(np.round(x.real * 127 + 127.5), 0, 255).astype("u1")
        out[1::2] = np.clip(np.round(x.imag * 127 + 127.5), 0, 255).astype("u1")
        return out.tobytes()
    if dt == "rf32_le":
        return x.real.astype("<f4").tobytes()
    raise ValueError(f"Unsupported datatype: {dt}")


# ---------------------------------------------------------------------------
# Signal building blocks (all chunk-aware)
# ---------------------------------------------------------------------------

def _t(start: int, count: int, fs: float) -> np.ndarray:
    return (start + np.arange(count, dtype=np.float64)) / fs


def constant(value: complex) -> SignalFn:
    return lambda start, count: np.full(count, value, dtype=np.complex128)


def tone(fs: float, f: float, amp: float = 0.5) -> SignalFn:
    def fn(start, count):
        return amp * np.exp(2j * np.pi * f * _t(start, count, fs))
    return fn


def real_cosine(fs: float, f: float, amp: float = 0.7) -> SignalFn:
    def fn(start, count):
        return (amp * np.cos(2 * np.pi * f * _t(start, count, fs))).astype(np.complex128)
    return fn


def chirp(fs: float, f0: float, f1: float, total: int, amp: float = 0.5) -> SignalFn:
    """Linear chirp from f0 to f1 across `total` samples."""
    k = (f1 - f0) / (total / fs)

    def fn(start, count):
        t = _t(start, count, fs)
        return amp * np.exp(1j * 2 * np.pi * (f0 * t + 0.5 * k * t * t))
    return fn


def sawtooth_chirp(fs: float, f0: float, f1: float, period: int, amp: float = 0.7) -> SignalFn:
    """Chirp f0 -> f1 that restarts every `period` samples."""
    k = (f1 - f0) / (period / fs)

    def fn(start, count):
        idx = (start + np.arange(count)) % period
        t = idx / fs
        return amp * np.exp(1j * 2 * np.pi * (f0 * t + 0.5 * k * t * t))
    return fn


def bursts(fs: float, spans: list, amp: float = 0.9) -> SignalFn:
    """Tone bursts. spans = [(start_sample, stop_sample, freq_hz), ...]."""
    def fn(start, count):
        x = np.zeros(count, dtype=np.complex128)
        stop = start + count
        for a, b, f in spans:
            lo, hi = max(a, start), min(b, stop)
            if lo < hi:
                idx = np.arange(lo, hi, dtype=np.float64)
                x[lo - start:hi - start] = amp * np.exp(2j * np.pi * f * idx / fs)
        return x
    return fn


def comb(fs: float, freqs, phases, amp: float = 0.05) -> SignalFn:
    freqs = np.asarray(freqs, dtype=np.float64)
    phases = np.asarray(phases, dtype=np.float64)

    def fn(start, count):
        t = _t(start, count, fs)
        x = np.zeros(count, dtype=np.complex128)
        for f, p in zip(freqs, phases):
            x += amp * np.exp(1j * (2 * np.pi * f * t + p))
        return x
    return fn


def hop(fs: float, seg: int, freqs, amp: float = 0.6) -> SignalFn:
    """A different tone in each consecutive segment of `seg` samples."""
    freqs = np.asarray(freqs, dtype=np.float64)

    def fn(start, count):
        idx = start + np.arange(count)
        seg_i = idx // seg
        local_t = (idx % seg) / fs
        return amp * np.exp(2j * np.pi * freqs[seg_i] * local_t)
    return fn


def _hash_pm1(idx: np.ndarray, salt: int) -> np.ndarray:
    """Deterministic pseudo-random +1/-1 per integer index (splitmix-style hash).
    Depends only on the index, so symbols are identical no matter how the
    work is chunked."""
    x = idx.astype(np.uint64) + np.uint64(salt)
    x ^= x >> np.uint64(33)
    x *= np.uint64(0xFF51AFD7ED558CCD)
    x ^= x >> np.uint64(33)
    return (x & np.uint64(1)).astype(np.float64) * 2.0 - 1.0


def psk_baseband(fs: float, sym_rate: float, qpsk: bool) -> SignalFn:
    sps = int(round(fs / sym_rate))

    def fn(start, count):
        sym_idx = np.arange(start, start + count, dtype=np.int64) // sps
        i = _hash_pm1(sym_idx, 0)
        if not qpsk:
            return i.astype(np.complex128)
        q = _hash_pm1(sym_idx, 0x9E3779B97F4A7C15)
        return (i + 1j * q) / np.sqrt(2)
    return fn


def modulated(baseband: SignalFn, carrier: SignalFn, amp: float) -> SignalFn:
    def fn(start, count):
        return amp * baseband(start, count) * carrier(start, count)
    return fn


def noise(rng: np.random.Generator, sigma: float) -> SignalFn:
    def fn(start, count):
        return sigma * (rng.standard_normal(count) + 1j * rng.standard_normal(count)) / np.sqrt(2)
    return fn


def add(*fns: SignalFn) -> SignalFn:
    def fn(start, count):
        out = fns[0](start, count)
        for f in fns[1:]:
            out = out + f(start, count)
        return out
    return fn


# ---------------------------------------------------------------------------
# Writing
# ---------------------------------------------------------------------------

def build_meta(spec: Spec) -> dict:
    global_block = {
        "core:datatype": spec.datatype,
        "core:sample_rate": spec.sample_rate,
        "core:version": SIGMF_VERSION,
        "core:description": spec.description,
        "core:author": spec.author,
        "core:recorder": "sigmf-fixture-generator",
    }

    if spec.captures is not None:
        captures = spec.captures
    else:
        cap = {"core:sample_start": 0}
        if spec.center_freq is not None:
            cap["core:frequency"] = spec.center_freq
        captures = [cap]

    return {
        "global": global_block,
        "captures": captures,
        "annotations": spec.annotations,
    }


def write_spec(spec: Spec, out_dir: str) -> tuple[int, int]:
    n = spec.n_samples()
    base = os.path.join(out_dir, spec.name)

    with open(base + ".sigmf-data", "wb") as f:
        for start in range(0, n, CHUNK):
            count = min(CHUNK, n - start)
            f.write(encode_samples(spec.signal(start, count), spec.datatype))

    with open(base + ".sigmf-meta", "w") as f:
        json.dump(build_meta(spec), f, indent=2)
        f.write("\n")

    return n, os.path.getsize(base + ".sigmf-data")


# ---------------------------------------------------------------------------
# Recording definitions
# ---------------------------------------------------------------------------

def build_specs(large: bool) -> list[Spec]:
    FS = 2.0e6
    N = 262144
    rng = np.random.default_rng(0xC0FFEE)

    comb_freqs = np.arange(-800e3, 801e3, 100e3)
    comb_phases = rng.uniform(0, 2 * np.pi, len(comb_freqs))

    burst_spans = [(20000, 45000, -300e3), (90000, 100000, 150e3), (180000, 230000, 600e3)]
    burst_annots = [{
        "core:sample_start": a,
        "core:sample_count": b - a,
        "core:freq_lower_edge": 868e6 + f - 5e3,
        "core:freq_upper_edge": 868e6 + f + 5e3,
        "core:label": f"burst {f / 1e3:+.0f}kHz",
        "core:comment": "synthetic burst",
    } for a, b, f in burst_spans]

    specs = [
        Spec(
            name="01_single_tone_cf32",
            description="Single complex tone at +250 kHz offset from center.",
            datatype="cf32_le", sample_rate=FS,
            signal=tone(FS, 250e3, amp=0.8),
            total_samples=N, center_freq=433.92e6,
            annotations=[{
                "core:sample_start": 0,
                "core:sample_count": N,
                "core:freq_lower_edge": 433.92e6 + 250e3 - 1e3,
                "core:freq_upper_edge": 433.92e6 + 250e3 + 1e3,
                "core:label": "tone +250kHz",
            }],
        ),
        Spec(
            name="02_negative_tone_cf32_be",
            description="Complex tone at -500 kHz, stored big-endian.",
            datatype="cf32_be", sample_rate=FS,
            signal=tone(FS, -500e3, amp=0.6),
            total_samples=N, center_freq=915.0e6,
        ),
        Spec(
            name="03_linear_chirp_cf64",
            description="Linear chirp from -900 kHz to +900 kHz over the record.",
            datatype="cf64_le", sample_rate=FS,
            signal=chirp(FS, -900e3, 900e3, N, amp=0.7),
            total_samples=N, center_freq=2.4e9,
        ),
        Spec(
            name="04_sawtooth_chirp_cf32",
            description="Repeating sawtooth chirp, period 16.4 ms.",
            datatype="cf32_le", sample_rate=FS,
            signal=sawtooth_chirp(FS, -800e3, 800e3, period=32768, amp=0.7),
            total_samples=N, center_freq=1.09e9,
        ),
        Spec(
            name="05_bursts_annotated_ci16",
            description="Three tone bursts at different offsets, annotated.",
            datatype="ci16_le", sample_rate=FS,
            signal=bursts(FS, burst_spans, amp=0.9),
            total_samples=N, center_freq=868.0e6,
            annotations=burst_annots,
        ),
        Spec(
            name="06_two_tones_noise_ci8",
            description="Two tones with a noise floor, quantized to signed 8-bit I/Q.",
            datatype="ci8", sample_rate=FS,
            signal=add(tone(FS, -400e3, 0.4), tone(FS, 380e3, 0.3), noise(rng, 0.05)),
            total_samples=N, center_freq=144.39e6,
        ),
        Spec(
            name="07_bpsk_cu8",
            description="BPSK at 100 ksym/s, carrier offset +200 kHz, unsigned 8-bit I/Q.",
            datatype="cu8", sample_rate=FS,
            signal=add(
                modulated(psk_baseband(FS, 100e3, qpsk=False), tone(FS, 200e3, 1.0), 0.6),
                noise(rng, 0.02),
            ),
            total_samples=N, center_freq=1.09e9,
        ),
        Spec(
            name="08_qpsk_cf32",
            description="QPSK at 250 ksym/s, carrier offset -650 kHz.",
            datatype="cf32_le", sample_rate=FS,
            signal=add(
                modulated(psk_baseband(FS, 250e3, qpsk=True), tone(FS, -650e3, 1.0), 0.7),
                noise(rng, 0.03),
            ),
            total_samples=N, center_freq=2.44e9,
        ),
        Spec(
            name="09_carrier_comb_cf32",
            description="17 equally spaced carriers spanning the sample rate.",
            datatype="cf32_le", sample_rate=FS,
            signal=comb(FS, comb_freqs, comb_phases, amp=0.05),
            total_samples=N, center_freq=100.0e6,
        ),
        Spec(
            name="10_near_nyquist_cf32",
            description="Tone at +0.45 * fs, checks display at the band edge.",
            datatype="cf32_le", sample_rate=FS,
            signal=tone(FS, 0.45 * FS, amp=0.8),
            total_samples=N, center_freq=3.0e9,
        ),
        Spec(
            name="11_dc_only_cf32",
            description="Constant DC offset with tiny noise; energy only in the DC bin.",
            datatype="cf32_le", sample_rate=FS,
            signal=add(constant(0.5 + 0.2j), noise(rng, 0.001)),
            total_samples=N, center_freq=433.0e6,
        ),
        Spec(
            name="12_white_noise_cf32",
            description="Complex Gaussian white noise, unit-ish variance.",
            datatype="cf32_le", sample_rate=FS,
            signal=noise(rng, 0.3),
            total_samples=N, center_freq=500.0e6,
        ),
        Spec(
            name="13_real_sinusoid_rf32",
            description="Real-valued sinusoid at fs/8, stored as rf32_le.",
            datatype="rf32_le", sample_rate=FS,
            signal=real_cosine(FS, FS / 8, amp=0.7),
            total_samples=N, center_freq=None,
        ),
        Spec(
            name="14_multi_capture_hop_cf32",
            description="Three captures at different center frequencies (frequency hop).",
            datatype="cf32_le", sample_rate=FS,
            signal=hop(FS, 65536, [100e3, 200e3, 300e3], amp=0.6),
            total_samples=3 * 65536, center_freq=None,
            captures=[
                {"core:sample_start": i * 65536, "core:frequency": f,
                 "core:datetime": f"2026-11-27T12:0{i}:00Z"}
                for i, f in enumerate([433.0e6, 868.0e6, 2.4e9])
            ],
        ),
        Spec(
            name="15_tiny_64_samples_cf32",
            description="Very short record (64 samples) to test small-FFT and padding behaviour.",
            datatype="cf32_le", sample_rate=FS,
            signal=tone(FS, 50e3, amp=0.9),
            total_samples=64, center_freq=1.0e9,
        ),
        Spec(
            name="16_high_rate_chirp_cf32",
            description="61.44 MSPS chirp, checks frequency axis labeling at high rates.",
            datatype="cf32_le", sample_rate=61.44e6,
            signal=chirp(61.44e6, -20e6, 20e6, 1 << 18, amp=0.5),
            total_samples=1 << 18, center_freq=5.8e9,
        ),
        Spec(
            name="17_mixed_scene_cf32",
            description="Chirp, a short burst, and noise: a mixed synthetic scene.",
            datatype="cf32_le", sample_rate=FS,
            signal=add(
                chirp(FS, -700e3, 300e3, N, amp=0.4),
                bursts(FS, [(120000, 160000, 500e3)], amp=0.8),
                noise(rng, 0.05),
            ),
            total_samples=N, center_freq=1.5e9,
            annotations=[
                {"core:sample_start": 0, "core:sample_count": N,
                 "core:freq_lower_edge": 1.5e9 - 700e3,
                 "core:freq_upper_edge": 1.5e9 + 300e3,
                 "core:label": "chirp"},
                {"core:sample_start": 120000, "core:sample_count": 40000,
                 "core:freq_lower_edge": 1.5e9 + 500e3 - 10e3,
                 "core:freq_upper_edge": 1.5e9 + 500e3 + 10e3,
                 "core:label": "burst"},
            ],
        ),

        # --- Size range -----------------------------------------------------
        Spec(
            name="20_small_1k_cf32",
            description="Small recording: 1,000 samples.",
            datatype="cf32_le", sample_rate=FS,
            signal=tone(FS, 1e6, amp=0.8),
            total_samples=1000, center_freq=915e6,
        ),
        Spec(
            name="21_medium_1MiB_ci16",
            description="1 MiB recording of ci16 samples.",
            datatype="ci16_le", sample_rate=FS,
            signal=add(tone(FS, 500e3, 0.6), noise(rng, 0.02)),
            target_bytes=1 << 20, center_freq=915e6,
        ),
    ]

    if large:
        FS_BIG = 10.0e6
        big_total = (2 * 1024**3) // BYTES_PER_SAMPLE["cf32_le"]   # 268,435,456
        specs.append(Spec(
            name="22_large_2GiB_chirp_cf32",
            description="2 GiB chirp with noise, for large-file testing.",
            datatype="cf32_le", sample_rate=FS_BIG,
            signal=add(
                chirp(FS_BIG, -4e6, 4e6, big_total, amp=0.5),
                noise(rng, 0.02),
            ),
            total_samples=big_total, center_freq=2.4e9,
        ))

    return specs


# ---------------------------------------------------------------------------
# Entry point
# ---------------------------------------------------------------------------

def main(argv: list[str]) -> int:
    large = "--large" in argv
    positional = [a for a in argv[1:] if not a.startswith("--")]
    out_dir = positional[0] if positional else "sigmf_fixtures"
    os.makedirs(out_dir, exist_ok=True)

    specs = build_specs(large=large)
    for spec in specs:
        n, nbytes = write_spec(spec, out_dir)
        print(f"{spec.name:32s} {spec.datatype:10s} "
              f"{n:>12,d} samples {nbytes:>14,d} bytes")

    print(f"\nWrote {len(specs)} recordings to {os.path.abspath(out_dir)}")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))