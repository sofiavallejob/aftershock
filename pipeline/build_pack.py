"""Build the Aftershock earthquake pack.

For every curated event:
  1. resolve the exact origin from USGS ComCat
  2. find one station per distance band (near / regional / teleseismic)
  3. download 3-component broadband data, remove the instrument response
     (ground velocity), rotate to Z/N/E, resample to 20 Hz
  4. quality-check: signal-to-noise, gaps, clipping
  5. measure the coda: per-band envelope decay -> apparent coda Q, per-band
     energy, usable coda length, early-arrival taps
  6. write everything into one binary pack plus a readable catalogue

Usage:  python build_pack.py OUTDIR [--only id1,id2] [--workers 4]
"""
import argparse, json, struct, sys, time, traceback, warnings
from concurrent.futures import ThreadPoolExecutor, as_completed
import numpy as np
from scipy.signal import butter, sosfiltfilt, hilbert, find_peaks
from obspy import UTCDateTime, Stream
from obspy.clients.fdsn import Client
from obspy.geodetics import gps2dist_azimuth, kilometer2degrees
from obspy.taup import TauPyModel
from events import EVENTS, SETTINGS, BANDS

warnings.filterwarnings("ignore")
FS = 20.0
PRE = 60.0                       # seconds of data kept before the origin
NB = 10
BAND_F = [0.1 * 80 ** (i / (NB - 1)) for i in range(NB)]   # 0.1 .. 8 Hz, log spaced
DATACENTRES = ["EARTHSCOPE", "ORFEUS", "GEOFON"]
GOOD_NETS = ["IU", "II", "G", "GE", "IC", "CU", "US", "CN", "AU", "GT", "MN", "CZ", "OE", "SL", "HU", "CR", "IV", "HL", "KO", "NZ", "JP", "PS", "IM", "BK", "CI", "NM"]
TAUP = TauPyModel("iasp91")


def log(*a):
    print(time.strftime("%H:%M:%S"), *a, flush=True)


def resolve_origin(t):
    c = Client("USGS")
    cat = c.get_events(starttime=UTCDateTime(t) - 180, endtime=UTCDateTime(t) + 180, minmagnitude=4.0)
    ev = max(cat, key=lambda e: (e.preferred_magnitude() or e.magnitudes[0]).mag)
    o = ev.preferred_origin() or ev.origins[0]
    m = ev.preferred_magnitude() or ev.magnitudes[0]
    return dict(time=str(o.time), lat=o.latitude, lon=o.longitude,
                depth=round((o.depth or 0) / 1000, 1), mag=round(m.mag, 1), magtype=m.magnitude_type,
                usgs_id=str(ev.resource_id).split("eventid=")[-1].split("&")[0])


def phase_times(depth, deg):
    arr = TAUP.get_travel_times(source_depth_in_km=max(depth, 0.1), distance_in_degree=deg,
                                phase_list=["P", "p", "Pn", "Pdiff", "S", "s", "Sn", "Sdiff", "SKS"])
    P = next((a.time for a in arr if a.name in ("P", "p", "Pn", "Pdiff")), None)
    S = next((a.time for a in arr if a.name in ("S", "s", "Sn", "Sdiff", "SKS")), None)
    return P, S


def candidates(o, band):
    t0 = UTCDateTime(o["time"])
    _, dmin, dmax = band
    out = []
    for dc in DATACENTRES:
        try:
            inv = Client(dc, timeout=60).get_stations(latitude=o["lat"], longitude=o["lon"], minradius=dmin,
                                                      maxradius=dmax, channel="BH?,HH?,HN?", level="channel",
                                                      starttime=t0, endtime=t0 + 60)
        except Exception:
            continue
        for net in inv:
            for sta in net:
                chans = {c.code for c in sta.channels}
                if not ({"BHZ", "HHZ"} & chans) and not (band[0] == "near" and "HNZ" in chans):
                    continue
                d, _, _ = gps2dist_azimuth(o["lat"], o["lon"], sta.latitude, sta.longitude)
                deg = kilometer2degrees(d / 1000)
                if deg < max(dmin, 0.15):
                    continue
                target = {"near": 1.2, "regional": 8.0, "tele": 35.0}[band[0]]
                pref = 0 if net.code in GOOD_NETS[:7] else (1 if net.code in GOOD_NETS else 2)
                out.append((pref, abs(deg - target), dc, net.code, sta.code, sta.latitude, sta.longitude, d / 1000, deg))
    out.sort()
    seen, uniq = set(), []
    for c in out:
        if (c[3], c[4]) not in seen:
            seen.add((c[3], c[4])); uniq.append(c)
    return uniq


def bp(x, lo, hi):
    sos = butter(3, [lo, min(hi, FS / 2 * 0.95)], btype="band", fs=FS, output="sos")
    return sosfiltfilt(sos, x)


def envelope(x, smooth_s):
    e = np.abs(hilbert(x))
    k = max(1, int(smooth_s * FS))
    return np.convolve(e, np.ones(k) / k, "same")


def fetch_station(o, cand, band_name):
    pref, _, dc, net, sta, slat, slon, dkm, deg = cand
    t0 = UTCDateTime(o["time"])
    P, S = phase_times(o["depth"], deg)
    if P is None:
        raise ValueError("no P")
    if S is None:
        S = P * 1.8
    dur = min(2400.0, max(900.0, S * 3.5 + 120))
    c = Client(dc, timeout=120)
    st = None
    order = ("BH?", "HH?", "HN?") if band_name == "near" else ("BH?", "HH?")
    st, last_err = None, "no data"
    for ch in order:
        try:
            st = c.get_waveforms(net, sta, "*", ch, t0 - PRE - 60, t0 + dur + 60)
            if len(st) < 3:
                continue
            return _process(o, st, c, net, sta, slat, slon, dkm, deg, P, S, dur, band_name, dc)
        except ValueError as e:
            last_err = str(e)
        except Exception:
            pass
    raise ValueError(last_err)


def _process(o, st, c, net, sta, slat, slon, dkm, deg, P, S, dur, band_name, dc):
    t0 = UTCDateTime(o["time"])
    st.merge(fill_value=None)
    loc = sorted({tr.stats.location for tr in st})[0]
    st = st.select(location=loc)
    if len(st) != 3 or any(np.ma.is_masked(tr.data) for tr in st):
        raise ValueError("gaps")
    inv = c.get_stations(network=net, station=sta, location=loc or "--", channel=st[0].stats.channel[:2] + "?",
                         level="response", starttime=t0 - PRE - 70, endtime=t0 + dur + 70)
    sr = st[0].stats.sampling_rate
    raw_max = max(np.abs(tr.data).max() for tr in st)
    if raw_max >= 0.9 * 2 ** 23:
        raise ValueError("near clipping")
    # sensor step / mass drift: the late median must stay near the pre-event median
    for tr in st:
        d = tr.data.astype(np.float64); k = int(20 * tr.stats.sampling_rate)
        pre = d[:k]; late = d[-3 * k:]
        if abs(np.median(late) - np.median(pre)) > 50 * (np.std(pre) + 1):
            raise ValueError("sensor step")
    st.detrend("demean"); st.detrend("linear"); st.taper(0.02)
    st.remove_response(inventory=inv, output="VEL", pre_filt=(0.02, 0.04, 8.5, 9.5), zero_mean=True)
    comps = "".join(sorted({tr.stats.channel[-1] for tr in st}))
    if comps != "ENZ":
        st._rotate_to_zne(inv, components=({"12Z": "Z12", "23Z": "Z23"}.get(comps, "Z12"),))
    if len(st.select(component="N")) != 1 or len(st.select(component="E")) != 1:
        raise ValueError("rotation")
    st.filter("lowpass", freq=8.0, corners=6, zerophase=True)
    st.resample(FS, no_filter=True)
    st.trim(t0 - PRE, t0 + dur, pad=True, fill_value=0)
    Z, N, E = (st.select(component=k)[0].data.astype(np.float64) for k in "ZNE")
    n = min(len(Z), len(N), len(E)); Z, N, E = Z[:n], N[:n], E[:n]
    tt = np.arange(n) / FS - PRE

    # ---- quality: nothing may arrive before P (catches leakage from processing)
    # (0.5-5 Hz, above the ocean microseism, 20 s windows)
    if P - 22 > -PRE + 23:
        vb = np.sqrt(bp(Z, 0.5, 5.0) ** 2 + bp(N, 0.5, 5.0) ** 2 + bp(E, 0.5, 5.0) ** 2)
        early = np.sqrt(np.mean(vb[(tt > -PRE + 3) & (tt < -PRE + 23)] ** 2))
        late = np.sqrt(np.mean(vb[(tt > P - 22) & (tt < P - 2)] ** 2))
        if late > 4 * early + 1e-15:
            raise ValueError("energy before P")

    # ---- quality: SNR in a band that survives distance
    hi = 5.0 if band_name != "tele" else 2.0
    v = np.sqrt(bp(Z, 0.5, hi) ** 2 + bp(N, 0.5, hi) ** 2 + bp(E, 0.5, hi) ** 2)
    noise_w = (tt > max(-PRE + 2, P - 40)) & (tt < P - 3)
    sig_w = (tt > P) & (tt < P + 150)
    noise = np.sqrt(np.mean(v[noise_w] ** 2)) if noise_w.sum() > 20 else np.inf
    sig = envelope(v, 5)[sig_w].max()
    snr = sig / noise
    if not np.isfinite(snr) or snr < 8:
        raise ValueError(f"snr {snr:.1f}")

    # ---- usable coda end: smoothed envelope stays above 2x noise
    env = envelope(v, 5)
    above = np.where((env > 2 * noise) & (tt > S))[0]
    t_end = tt[above[-1]] if len(above) else S + 60
    t_end = float(min(tt[-1], max(t_end + 15, S + 60)))
    keep = tt <= t_end
    Z, N, E, tt = Z[keep], N[keep], E[keep], tt[keep]

    # ---- per-band coda analysis

    bands = []
    for f in BAND_F:
        lo, hi_ = f / 2 ** (1 / 3), f * 2 ** (1 / 3)
        if hi_ >= FS / 2 * 0.95:
            hi_ = FS / 2 * 0.94
        x = np.sqrt(sum(bp(c_, lo, hi_) ** 2 for c_ in (Z, N, E)))
        e = envelope(x, max(2.0, 3.0 / f))
        nw = (tt > -PRE + 2) & (tt < P - 3)
        nb = np.sqrt(np.mean(e[nw] ** 2)) if nw.sum() > 20 else 1e-30
        energy = float(np.sum(e[(tt >= P)] ** 2) / FS)
        # seismic RT60: decay slope from -5 dB to -25 dB below the post-S peak
        # (T20 as in room acoustics), shortened to -15 dB when noise is in the way
        rt, span = None, None
        i_pk = np.searchsorted(tt, S - 2) + int(np.argmax(e[tt >= S - 2]))
        db = 20 * np.log10(np.maximum(e, 1e-30) / e[i_pk])
        floor = 20 * np.log10(max(nb, 1e-30) / e[i_pk]) + 3
        for lo_db in (-25, -15, -10):
            if lo_db <= floor:
                continue
            after = db[i_pk:]
            a = np.argmax(after <= -5); b_ = np.argmax(after <= lo_db)
            if after[a] > -5 or after[b_] > lo_db or b_ - a < FS * 3:
                continue
            ts = tt[i_pk + a:i_pk + b_]; ys = db[i_pk + a:i_pk + b_]
            slope = np.polyfit(ts, ys, 1)[0]
            if slope < 0:
                rt, span = float(-60 / slope), -lo_db - 5
                break
        q_app = None if rt is None else round(0.4548 * f * rt, 1)   # 20log10(e)*pi = 27.29 dB per (f t / Q)
        bands.append(dict(f=round(f, 3), rt60=None if rt is None else round(rt, 1), fit_db=span, q_app=q_app,
                          energy=energy, snr=round(float(e[tt >= P].max() / max(nb, 1e-30)), 1)))
    emax = max(b["energy"] for b in bands) or 1
    for b in bands:
        b["energy_db"] = round(10 * np.log10(max(b["energy"], 1e-30) / emax), 2); del b["energy"]

    # ---- early-arrival taps: 8 strongest energy bursts between P and S + 30 % of S-P
    i0, i1 = np.searchsorted(tt, P - 1), np.searchsorted(tt, S + 0.3 * (S - P))
    seg = np.sqrt(Z[i0:i1] ** 2 + N[i0:i1] ** 2 + E[i0:i1] ** 2)
    taps = []
    if len(seg) > 16:
        es = np.convolve(seg, np.ones(10) / 10, "same")
        pk, _ = find_peaks(es, distance=max(1, len(es) // 24))
        pk = np.sort(pk[np.argsort(es[pk])[::-1][:8]])
        for p_ in pk:
            nn, ee = N[i0 + p_], E[i0 + p_]
            taps.append([round(p_ / len(es), 4), round(float(es[p_] / es[pk].max()), 3), round(float(ee * ee / (nn * nn + ee * ee + 1e-30)), 3)])

    R = dkm / 3.6 if o["depth"] < 70 else None
    meta = dict(net=net, sta=sta, loc=loc, channel=st[0].stats.channel[:2], datacentre=dc, lat=slat, lon=slon, dist_km=round(dkm, 1), dist_deg=round(deg, 2),
                band=band_name, fs=FS, t0=-PRE, n=int(len(Z)), P=round(P, 2), S=round(S, 2),
                R=None if R is None else round(R, 1), coda_end=round(t_end, 1), snr=round(float(snr), 1),
                sr_orig=sr, clipped=bool(raw_max >= 2 ** 23 - 2), bands=bands, taps=taps)
    return meta, np.vstack([Z, N, E])


def process_event(ev):
    eid, name, t, setting = ev
    o = resolve_origin(t)
    out = dict(id=eid, name=name, setting=setting, **o, stations=[])
    data = []
    for band in BANDS:
        cands = candidates(o, band)
        got = False; rej = []
        for cand in cands[:(40 if band[0] == "near" else 28)]:
            try:
                meta, arr = fetch_station(o, cand, band[0])
                if meta["clipped"]:
                    continue
                out["stations"].append(meta); data.append(arr); got = True
                log(f"  {eid:14s} {band[0]:8s} {meta['net']}.{meta['sta']:6s} {meta['dist_km']:7.0f} km  snr {meta['snr']:6.1f}  coda {meta['coda_end']:6.0f} s")
                break
            except Exception as e:
                rej.append(f"{cand[3]}.{cand[4]}:{str(e)[:40]}")
                continue
        if not got:
            log(f"  {eid:14s} {band[0]:8s} no usable station ({len(cands)} candidates) {rej[:5]}")
    return out, data


def write_pack(outdir, events, arrays):
    import os
    os.makedirs(outdir, exist_ok=True)
    blob = bytearray()
    for ev, arrs in zip(events, arrays):
        for st, a in zip(ev["stations"], arrs):
            peak = float(np.abs(a).max()) or 1.0
            q = np.round(a / peak * 32767).astype("<i2")
            st["offset"] = len(blob); st["scale"] = peak / 32767  # m/s per count
            blob += q.T.tobytes()                                  # interleaved Z N E
    header = dict(format="aftershock-pack", version=1, fs=FS, bands_hz=[round(f, 3) for f in BAND_F],
                  settings=SETTINGS, events=events,
                  credits=("Waveforms: FDSN data centres EarthScope (formerly IRIS DMC), ORFEUS and GEOFON. "
                           "Each station lists its FDSN network code; see https://www.fdsn.org/networks/ for network "
                           "citations and DOIs. Origins: USGS ComCat. Travel times: iasp91 via ObsPy TauP."))
    hj = json.dumps(header, separators=(",", ":")).encode()
    with open(f"{outdir}/aftershock_pack.bin", "wb") as f:
        f.write(b"AFTSHK01"); f.write(struct.pack("<I", len(hj))); f.write(hj); f.write(blob)
    json.dump(header, open(f"{outdir}/catalog.json", "w"), indent=1)
    return len(blob)


if __name__ == "__main__":
    ap = argparse.ArgumentParser()
    ap.add_argument("outdir"); ap.add_argument("--only", default=""); ap.add_argument("--workers", type=int, default=4)
    a = ap.parse_args()
    evs = [e for e in EVENTS if not a.only or e[0] in a.only.split(",")]
    results = {}
    with ThreadPoolExecutor(a.workers) as ex:
        futs = {ex.submit(process_event, e): e for e in evs}
        for f in as_completed(futs):
            e = futs[f]
            try:
                results[e[0]] = f.result()
                log(f"done {e[0]}: {len(results[e[0]][0]['stations'])} stations")
            except Exception as err:
                log(f"FAILED {e[0]}: {err}")
    order = [e[0] for e in evs if e[0] in results and results[e[0]][0]["stations"]]
    n = write_pack(a.outdir, [results[k][0] for k in order], [results[k][1] for k in order])
    log(f"pack: {len(order)} events, {sum(len(results[k][0]['stations']) for k in order)} traces, {n/1e6:.1f} MB")
