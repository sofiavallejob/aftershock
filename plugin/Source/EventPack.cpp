// Aftershock, a reverb made from earthquakes. (C) 2026 Sofia Vallejo Budziszewski, AGPL-3.0.
// Free to port, by humans. Bring your own ears. See PORTING.md.

#include "EventPack.h"
#include "BinaryData.h"

namespace aftershock
{
static float num (const juce::var& v, float fallback)
{
    return v.isVoid() || v.isUndefined() ? fallback : (float) (double) v;
}

bool EventPack::load (const void* data, size_t size)
{
    juce::MemoryBlock raw;
    auto* bytes = static_cast<const juce::uint8*> (data);

    if (size > 2 && bytes[0] == 0x1f && bytes[1] == 0x8b)
    {
        juce::MemoryInputStream in (data, size, false);
        juce::GZIPDecompressorInputStream gz (&in, false, juce::GZIPDecompressorInputStream::gzipFormat);
        juce::MemoryOutputStream out (raw, false);
        out.writeFromInputStream (gz, -1);
    }
    else
        raw.append (data, size);

    if (raw.getSize() < 12 || std::memcmp (raw.getData(), "AFTSHK01", 8) != 0)
        return false;

    juce::uint32 headerLen = 0;
    std::memcpy (&headerLen, static_cast<const char*> (raw.getData()) + 8, 4);
    headerLen = juce::ByteOrder::swapIfBigEndian (headerLen);
    if (12 + (size_t) headerLen > raw.getSize())
        return false;

    auto json = juce::JSON::parse (juce::String::fromUTF8 (static_cast<const char*> (raw.getData()) + 12, (int) headerLen));
    if (! json.isObject())
        return false;

    blob.replaceAll (static_cast<const char*> (raw.getData()) + 12 + headerLen, raw.getSize() - 12 - headerLen);

    for (auto& f : *json["bands_hz"].getArray())
        bandsHz.push_back ((float) (double) f);

    if (auto* obj = json["settings"].getDynamicObject())
        for (auto& p : obj->getProperties())
            settings.emplace_back (p.name.toString(), p.value.toString());

    credits = json["credits"].toString();

    for (auto& ev : *json["events"].getArray())
    {
        Event e;
        e.id = ev["id"]; e.name = ev["name"]; e.setting = ev["setting"]; e.time = ev["time"];
        e.magType = ev["magtype"]; e.usgsId = ev["usgs_id"];
        e.lat = ev["lat"]; e.lon = ev["lon"]; e.depthKm = ev["depth"]; e.mag = ev["mag"];

        for (auto& s : *ev["stations"].getArray())
        {
            Station st;
            st.net = s["net"]; st.sta = s["sta"]; st.channel = s["channel"]; st.dataCentre = s["datacentre"]; st.band = s["band"];
            st.lat = s["lat"]; st.lon = s["lon"]; st.distKm = s["dist_km"]; st.distDeg = s["dist_deg"];
            st.P = num (s["P"], 0); st.S = num (s["S"], -1); st.R = num (s["R"], -1);
            st.codaEnd = num (s["coda_end"], 0); st.snr = num (s["snr"], 0);
            st.t0 = num (s["t0"], -30); st.fs = num (s["fs"], 20);
            st.n = s["n"]; st.offset = (juce::int64) (double) s["offset"]; st.scale = s["scale"];

            for (auto& b : *s["bands"].getArray())
                st.bands.push_back ({ num (b["f"], 0), num (b["rt60"], -1), num (b["fit_db"], 0),
                                      num (b["energy_db"], -60), num (b["q_app"], -1) });

            if (auto* taps = s["taps"].getArray())
                for (auto& t : *taps)
                    st.taps.push_back ({ num (t[0], 0), num (t[1], 0), num (t[2], 0.5f) });

            e.stations.push_back (std::move (st));
        }

        if (! e.stations.empty())
            events.push_back (std::move (e));
    }

    return ! events.empty();
}

juce::String EventPack::settingLabel (const juce::String& key) const
{
    for (auto& s : settings)
        if (s.first == key) return s.second;
    return key;
}

int EventPack::settingIndex (const juce::String& key) const
{
    for (size_t i = 0; i < settings.size(); ++i)
        if (settings[i].first == key) return (int) i;
    return 0;
}

Trace EventPack::readTrace (const Station& st) const
{
    Trace t;
    t.fs = st.fs; t.t0 = st.t0; t.P = st.P; t.S = st.S; t.R = st.R;
    const auto bytesNeeded = (size_t) st.n * 3 * sizeof (juce::int16);
    if (st.offset < 0 || (size_t) st.offset + bytesNeeded > blob.getSize())
        return t;

    auto* p = reinterpret_cast<const juce::int16*> (static_cast<const char*> (blob.getData()) + st.offset);
    for (auto& c : t.zne) c.resize ((size_t) st.n);
    for (int i = 0; i < st.n; ++i)
        for (int c = 0; c < 3; ++c)
            t.zne[(size_t) c][(size_t) i] = (float) (juce::int16) juce::ByteOrder::swapIfBigEndian ((juce::uint16) p[i * 3 + c])
                                            * (float) st.scale;
    return t;
}

const EventPack& EventPack::get()
{
    static EventPack pack = []
    {
        EventPack p;
        p.load (BinaryData::aftershock_pack_bin_gz, (size_t) BinaryData::aftershock_pack_bin_gzSize);
        return p;
    }();
    return pack;
}
} // namespace aftershock
