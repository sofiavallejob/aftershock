// Aftershock, a reverb made from earthquakes. (C) 2026 Sofia Vallejo Budziszewski, AGPL-3.0.
// Free to port, by humans. Bring your own ears. See PORTING.md.

#include "Views.h"
#include "BinaryData.h"

namespace ui
{
static constexpr double deg = juce::MathConstants<double>::pi / 180.0;

std::unique_ptr<juce::AccessibilityHandler> imageHandler (juce::Component& c, std::function<juce::String()>)
{
    return std::make_unique<juce::AccessibilityHandler> (c, juce::AccessibilityRole::image);
}


// ---------------------------------------------------------------- land
const Land& Land::get()
{
    static Land L = []
    {
        Land l;
        auto json = juce::JSON::parse (juce::String::fromUTF8 (BinaryData::land_json, BinaryData::land_jsonSize));
        if (auto* arcs = json.getArray())
            for (auto& a : *arcs)
                if (auto* pts = a.getArray())
                {
                    std::vector<juce::Point<float>> ring;
                    for (int i = 0; i + 1 < pts->size(); i += 2)
                        ring.push_back ({ (float) (double) (*pts)[i], (float) (double) (*pts)[i + 1] });
                    l.rings.push_back (std::move (ring));
                }
        auto inside = [&] (float x, float y)
        {
            for (auto& r : l.rings)
            {
                bool c = false;
                for (size_t i = 0, j = r.size() - 1; i < r.size(); j = i++)
                    if ((r[i].y > y) != (r[j].y > y) && x < (r[j].x - r[i].x) * (y - r[i].y) / (r[j].y - r[i].y) + r[i].x) c = ! c;
                if (c) return true;
            }
            return false;
        };
        for (float lat = -60.0f; lat <= 80.0f; lat += 4.2f)
        {
            const float step = 4.2f / (float) juce::jmax (0.25, std::cos (lat * deg));
            for (float lon = -180.0f + std::fmod (lat * 7.0f + 360.0f, step); lon < 180.0f; lon += step)
                if (inside (lon, lat)) l.dots.push_back ({ lon, lat });
        }
        return l;
    }();
    return L;
}

const aftershock::Event* DataView::event() const
{
    return juce::isPositiveAndBelow (sel.event, (int) proc.pack.events.size()) ? &proc.pack.events[(size_t) sel.event] : nullptr;
}

const aftershock::Station* DataView::station() const
{
    auto* e = event();
    return e != nullptr && juce::isPositiveAndBelow (sel.station, (int) e->stations.size()) ? &e->stations[(size_t) sel.station] : nullptr;
}

// great-circle point between two places, t in 0..1
static std::pair<double, double> greatCircle (double la1, double lo1, double la2, double lo2, double t)
{
    auto v = [] (double la, double lo) { return std::array<double, 3> { std::cos (la * deg) * std::cos (lo * deg), std::cos (la * deg) * std::sin (lo * deg), std::sin (la * deg) }; };
    auto A = v (la1, lo1), B = v (la2, lo2);
    const double d = std::acos (juce::jlimit (-1.0, 1.0, A[0] * B[0] + A[1] * B[1] + A[2] * B[2]));
    if (d < 1e-9) return { la1, lo1 };
    const double s1 = std::sin ((1 - t) * d) / std::sin (d), s2 = std::sin (t * d) / std::sin (d);
    const double x = s1 * A[0] + s2 * B[0], y = s1 * A[1] + s2 * B[1], z = s1 * A[2] + s2 * B[2];
    return { std::atan2 (z, std::sqrt (x * x + y * y)) / deg, std::atan2 (y, x) / deg };
}

// ---------------------------------------------------------------- globe
GlobeView::GlobeView (AftershockProcessor& p) : DataView (p)
{
    setTitle ("Globe");
    Land::get();
    lastTime = juce::Time::getMillisecondCounterHiRes();
}

void GlobeView::updateTimer()
{
    const bool run = isShowing() && ! Settings::get().reduceMotion();
    if (run && ! isTimerRunning()) { lastTime = juce::Time::getMillisecondCounterHiRes(); startTimerHz (30); }
    if (! run) stopTimer();
    repaint();
}

void GlobeView::timerCallback()
{
    const double now = juce::Time::getMillisecondCounterHiRes();
    if (! dragging) lon0 = std::fmod (lon0 + (now - lastTime) * 0.001 * 360.0 / 48.0, 360.0);
    lastTime = now;
    repaint();
}

void GlobeView::selectionChanged()
{
    if (auto* e = event())
    {
        juce::String d = "Globe showing all " + juce::String ((int) proc.pack.events.size()) + " earthquakes. Selected: " + e->name;
        if (auto* s = station()) d << ", recorded at station " << s->label() << ", " << juce::roundToInt (s->distKm) << " kilometres away.";
        setDescription (d);
        if (Settings::get().reduceMotion()) lon0 = -e->lon;   // turn the selection to face the listener
    }
}

bool GlobeView::project (double lat, double lon, juce::Point<float>& out, float& depth) const
{
    auto b = getLocalBounds().toFloat();
    const float R = juce::jmin (b.getWidth(), b.getHeight()) * 0.5f - 8.0f;
    const auto c = b.getCentre();
    const double la = lat * deg, dl = (lon + lon0) * deg, t = tilt * deg;
    const double x = std::cos (la) * std::sin (dl);
    const double y = std::cos (t) * std::sin (la) - std::sin (t) * std::cos (la) * std::cos (dl);
    const double z = std::sin (t) * std::sin (la) + std::cos (t) * std::cos (la) * std::cos (dl);
    out = { c.x + R * (float) x, c.y - R * (float) y };
    depth = (float) z;
    return z > 0.0;
}

int GlobeView::hit (juce::Point<float> pt) const
{
    int best = -1; float bd = 10;
    for (int i = 0; i < (int) proc.pack.events.size(); ++i)
    {
        juce::Point<float> q; float z;
        if (project (proc.pack.events[(size_t) i].lat, proc.pack.events[(size_t) i].lon, q, z) && q.getDistanceFrom (pt) < bd) { bd = q.getDistanceFrom (pt); best = i; }
    }
    return best;
}

void GlobeView::mouseDown (const juce::MouseEvent&) { dragging = true; moved = false; dragLon = lon0; }
void GlobeView::mouseDrag (const juce::MouseEvent& e)
{
    if (e.getDistanceFromDragStart() > 3) moved = true;
    lon0 = dragLon + e.getDistanceFromDragStartX() * 0.5;
    tilt = juce::jlimit (-60.0, 60.0, tilt + 0.0);
    repaint();
}
void GlobeView::mouseUp (const juce::MouseEvent& e)
{
    dragging = false;
    lastTime = juce::Time::getMillisecondCounterHiRes();
    if (! moved) { const int h = hit (e.position); if (h >= 0 && onPickEvent) onPickEvent (h); }
}
void GlobeView::mouseMove (const juce::MouseEvent& e)
{
    const int h = hit (e.position);
    if (h != hover) { hover = h; setMouseCursor (h >= 0 ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::DraggingHandCursor); repaint(); }
}

void GlobeView::paint (juce::Graphics& g)
{
    const auto p = Settings::get().palette();
    auto b = getLocalBounds().toFloat();
    const float R = juce::jmin (b.getWidth(), b.getHeight()) * 0.5f - 8.0f;
    const auto c = b.getCentre();
    auto disc = juce::Rectangle<float> (R * 2, R * 2).withCentre (c);

    // body and halo
    if (! p.highContrast)
    {
        juce::ColourGradient halo (p[peri].withAlpha (0.22f), c.x, c.y, p[peri].withAlpha (0.0f), c.x + R * 1.25f, c.y, true);
        g.setGradientFill (halo); g.fillEllipse (disc.expanded (R * 0.22f));
    }
    juce::ColourGradient body (p.globe1, c.x - R * 0.28f, c.y - R * 0.4f, p.globe3, c.x + R, c.y + R, true);
    body.addColour (0.55, p.globe2);
    g.setGradientFill (body); g.fillEllipse (disc);
    g.setColour (p[peri].withAlpha (p.highContrast ? 0.8f : 0.3f)); g.drawEllipse (disc, 1.0f);

    // land dots, fading toward the limb
    for (auto& d : Land::get().dots)
    {
        juce::Point<float> q; float z;
        if (! project (d.y, d.x, q, z)) continue;
        g.setColour (p.dot.withMultipliedAlpha (0.35f + 0.65f * z));
        g.fillEllipse (q.x - 1.5f, q.y - 1.5f, 3.0f, 3.0f);
    }

    auto* ev = event();
    auto* st = station();
    // path from the earthquake to the station
    if (ev != nullptr && st != nullptr)
    {
        juce::Path path; bool on = false;
        for (int k = 0; k <= 48; ++k)
        {
            auto [la, lo] = greatCircle (ev->lat, ev->lon, st->lat, st->lon, k / 48.0);
            juce::Point<float> q; float z;
            if (project (la, lo, q, z)) { if (! on) path.startNewSubPath (q); else path.lineTo (q); on = true; }
            else on = false;
        }
        juce::Path dashed; const float dl[] = { 3, 3 };
        juce::PathStrokeType (1.4f).createDashedStroke (dashed, path, dl, 2);
        g.setColour (p[lime]); g.fillPath (dashed);
        juce::Point<float> q; float z;
        if (project (st->lat, st->lon, q, z)) { g.setColour (p.t1); g.drawEllipse (q.x - 3.5f, q.y - 3.5f, 7, 7, 1.5f); }
    }

    // earthquakes
    for (int i = 0; i < (int) proc.pack.events.size(); ++i)
    {
        const auto& e = proc.pack.events[(size_t) i];
        juce::Point<float> q; float z;
        if (! project (e.lat, e.lon, q, z)) continue;
        const auto col = p[settingAccent (e.setting)];
        if (i == sel.event)
        {
            juce::ColourGradient gl (p[lime].withAlpha (0.55f), q.x, q.y, p[lime].withAlpha (0.0f), q.x + 14, q.y, true);
            g.setGradientFill (gl); g.fillEllipse (q.x - 14, q.y - 14, 28, 28);
            g.setColour (p[lime].withAlpha (0.35f)); g.fillEllipse (q.x - 6, q.y - 6, 12, 12);
            g.setColour (p[lime]); g.drawEllipse (q.x - 6, q.y - 6, 12, 12, 2.0f);
        }
        else
        {
            g.setColour (col.withAlpha (0.25f * z)); g.fillEllipse (q.x - 6, q.y - 6, 12, 12);
            g.setColour (col); g.fillEllipse (q.x - 3.5f, q.y - 3.5f, 7, 7);
            if (i == hover) { g.setColour (p.t1); g.drawEllipse (q.x - 6, q.y - 6, 12, 12, 1.2f); }
        }
    }

    // light and limb shading
    if (! p.highContrast)
    {
        juce::ColourGradient hi (juce::Colours::white.withAlpha (0.10f), c.x - R * 0.32f, c.y - R * 0.44f, juce::Colours::white.withAlpha (0.0f), c.x + R * 0.3f, c.y + R * 0.2f, true);
        g.setGradientFill (hi); g.fillEllipse (disc);
        juce::ColourGradient limb (juce::Colours::black.withAlpha (0.0f), c.x, c.y, juce::Colours::black.withAlpha (0.22f), c.x + R, c.y, true);
        limb.addColour (0.62, juce::Colours::black.withAlpha (0.0f));
        g.setGradientFill (limb); g.fillEllipse (disc);
    }

    if (hover >= 0)
    {
        const auto& e = proc.pack.events[(size_t) hover];
        const juce::String txt = e.name + "  M" + juce::String (e.mag, 1) + "  " + e.time.substring (0, 4);
        const float tw = juce::GlyphArrangement::getStringWidth (font (12.0f), txt) + 18;
        auto box = juce::Rectangle<float> (tw, 22).withCentre ({ c.x, b.getBottom() - 14 });
        g.setColour (p.card.withAlpha (0.95f)); g.fillRoundedRectangle (box, 6);
        g.setColour (p.t1); g.setFont (font (12.0f)); g.drawText (txt, box, juce::Justification::centred);
    }
}

std::unique_ptr<juce::AccessibilityHandler> GlobeView::createAccessibilityHandler() { return imageHandler (*this, {}); }

// ---------------------------------------------------------------- flat map
juce::Rectangle<float> MapView::plotArea() const { return getLocalBounds().toFloat().withTrimmedBottom (40); }

juce::Point<float> MapView::project (double lat, double lon) const
{
    auto a = plotArea();
    const float sc = juce::jmin (a.getWidth() / 360.0f, a.getHeight() / 142.0f);
    const float w = 360.0f * sc, h = 142.0f * sc;
    const float x0 = a.getCentreX() - w * 0.5f, y0 = a.getCentreY() - h * 0.5f;
    return { x0 + (float) (lon + 180.0) * sc, y0 + (float) (84.0 - lat) * sc };
}

int MapView::hit (juce::Point<float> pt) const
{
    int best = -1; float bd = 10;
    for (int i = 0; i < (int) proc.pack.events.size(); ++i)
    {
        const float d = project (proc.pack.events[(size_t) i].lat, proc.pack.events[(size_t) i].lon).getDistanceFrom (pt);
        if (d < bd) { bd = d; best = i; }
    }
    return best;
}

void MapView::mouseMove (const juce::MouseEvent& e)
{
    const int h = hit (e.position);
    if (h != hover) { hover = h; setMouseCursor (h >= 0 ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::NormalCursor); repaint(); }
}

void MapView::mouseUp (const juce::MouseEvent& e)
{
    const int h = hit (e.position);
    if (h >= 0 && onPickEvent) onPickEvent (h);
}

void MapView::selectionChanged()
{
    if (auto* e = event()) setDescription ("World map of all earthquakes, coloured by tectonic setting. Selected: " + e->name);
}

void MapView::paint (juce::Graphics& g)
{
    const auto p = Settings::get().palette();
    auto area = plotArea();

    juce::Path land;
    for (auto& r : Land::get().rings)
    {
        float mn = 999, mx = -999, mnLat = 999;
        for (auto& pt : r) { mn = juce::jmin (mn, pt.x); mx = juce::jmax (mx, pt.x); mnLat = juce::jmin (mnLat, pt.y); }
        if (mx - mn > 300 || mnLat < -58) continue;
        bool first = true;
        for (auto& pt : r) { auto q = project (juce::jmin (84.0f, pt.y), pt.x); if (first) land.startNewSubPath (q); else land.lineTo (q); first = false; }
        land.closeSubPath();
    }
    g.setColour (p.ink.withAlpha (p.highContrast ? 0.3f : 0.13f)); g.fillPath (land);
    g.setColour (p.ink.withAlpha (0.18f)); g.strokePath (land, juce::PathStrokeType (0.5f));

    auto* ev = event();
    auto* st = station();
    if (ev != nullptr && st != nullptr)
    {
        juce::Path path; float px = 0;
        for (int k = 0; k <= 64; ++k)
        {
            auto [la, lo] = greatCircle (ev->lat, ev->lon, st->lat, st->lon, k / 64.0);
            auto q = project (la, lo);
            if (k == 0 || std::abs (q.x - px) > area.getWidth() * 0.5f) path.startNewSubPath (q); else path.lineTo (q);
            px = q.x;
        }
        juce::Path dashed; const float dl[] = { 3, 3 };
        juce::PathStrokeType (1.3f).createDashedStroke (dashed, path, dl, 2);
        g.setColour (p[lime]); g.fillPath (dashed);
        auto q = project (st->lat, st->lon);
        g.setColour (p.t1); g.drawEllipse (q.x - 3.5f, q.y - 3.5f, 7, 7, 1.4f);
        g.setColour (p.limeText); g.setFont (font (10.0f));
        g.drawText (st->label(), juce::Rectangle<float> (q.x - 64, q.y - 18, 58, 12), juce::Justification::centredRight);
    }

    for (int i = 0; i < (int) proc.pack.events.size(); ++i)
    {
        const auto& e = proc.pack.events[(size_t) i];
        auto q = project (e.lat, e.lon);
        const float r = 2.6f + (float) juce::jmax (0.0, e.mag - 6.0) * 0.9f;
        if (i == sel.event)
        {
            g.setColour (p[lime].withAlpha (0.3f)); g.fillEllipse (q.x - 7, q.y - 7, 14, 14);
            g.setColour (p[lime]); g.drawEllipse (q.x - 7, q.y - 7, 14, 14, 1.8f);
        }
        else
        {
            g.setColour (p[settingAccent (e.setting)]); g.fillEllipse (q.x - r, q.y - r, r * 2, r * 2);
            if (i == hover) { g.setColour (p.t1); g.drawEllipse (q.x - r - 3, q.y - r - 3, r * 2 + 6, r * 2 + 6, 1.2f); }
        }
    }

    // legend
    auto leg = getLocalBounds().toFloat().removeFromBottom (34);
    g.setFont (font (10.5f));
    float x = leg.getX(), y = leg.getY() + 2;
    for (auto& [key, label] : proc.pack.settings)
    {
        const float w = juce::GlyphArrangement::getStringWidth (font (10.5f), label) + 26;
        if (x + w > leg.getRight()) { x = leg.getX(); y += 15; }
        g.setColour (p[settingAccent (key)]); g.fillEllipse (x, y + 3, 7, 7);
        g.setColour (p.t2); g.drawText (label, juce::Rectangle<float> (x + 11, y, w, 13), juce::Justification::centredLeft);
        x += w;
    }

    if (hover >= 0)
    {
        const auto& e = proc.pack.events[(size_t) hover];
        const juce::String txt = e.name + "  M" + juce::String (e.mag, 1) + "  " + e.time.substring (0, 4);
        auto q = project (e.lat, e.lon);
        const float tw = juce::GlyphArrangement::getStringWidth (font (12.0f), txt) + 18;
        auto box = juce::Rectangle<float> (tw, 22).withPosition (juce::jlimit (area.getX(), area.getRight() - tw, q.x + 10), juce::jmax (area.getY(), q.y - 28));
        g.setColour (p.card.withAlpha (0.96f)); g.fillRoundedRectangle (box, 6);
        g.setColour (p.border()); g.drawRoundedRectangle (box, 6, 1);
        g.setColour (p.t1); g.setFont (font (12.0f)); g.drawText (txt, box, juce::Justification::centred);
    }
}

std::unique_ptr<juce::AccessibilityHandler> MapView::createAccessibilityHandler() { return imageHandler (*this, {}); }

// ---------------------------------------------------------------- signal (simple view)
static void drawPhases (juce::Graphics& g, const aftershock::Trace& tr, juce::Rectangle<float> plot, std::function<float (float)> X, const Palette& p, bool labels)
{
    const std::pair<const char*, float> ph[] = { { "P", tr.P }, { "S", tr.S }, { "surface", tr.R } };
    float labelEnd = -1e9f;
    for (auto& [name, t] : ph)
    {
        if (t <= tr.t0 || t >= tr.endTime()) continue;
        const float x = X (t);
        juce::Path l; l.startNewSubPath (x, plot.getY()); l.lineTo (x, plot.getBottom());
        juce::Path dashed; const float dl[] = { 3, 3 };
        juce::PathStrokeType (1.0f).createDashedStroke (dashed, l, dl, 2);
        g.setColour (p.t3.withAlpha (0.8f)); g.fillPath (dashed);
        if (labels && x > labelEnd + 4)
        {
            g.setColour (p.t2); g.setFont (font (10.0f));
            const float w = juce::GlyphArrangement::getStringWidth (font (10.0f), name) + 2;
            g.drawText (name, juce::Rectangle<float> (x + 3, plot.getY(), w, 12), juce::Justification::centredLeft);
            labelEnd = x + 3 + w;
        }
    }
}

void SignalView::selectionChanged()
{
    if (auto* s = station())
    {
        trace = proc.pack.readTrace (*s);
        setDescription ("Seismogram, north component at " + s->label() + ". P wave at " + juce::String (juce::roundToInt (trace.P))
                        + " seconds. The part from " + juce::String (juce::roundToInt (sel.w0)) + " to " + juce::String (juce::roundToInt (sel.w1))
                        + " seconds is squeezed into the reverb.");
    }
}

void SignalView::paint (juce::Graphics& g)
{
    const auto p = Settings::get().palette();
    auto b = getLocalBounds().toFloat();
    g.setColour (p.wellColour()); g.fillRoundedRectangle (b, 12.0f);
    b = b.reduced (14, 10);
    auto* s = station();
    if (s == nullptr || trace.zne[1].empty()) return;

    auto head = b.removeFromTop (16);
    g.setFont (font (10.5f).withExtraKerningFactor (0.06f)); g.setColour (p.t3);
    const auto* ev = event();
    g.drawText ("GROUND VELOCITY  " + s->label() + "  " + juce::String (juce::roundToInt (s->distKm)) + " KM", head, juce::Justification::centredLeft);
    g.setColour (p.limeText); g.drawText (juce::String (juce::CharPointer_UTF8 ("becomes the reverb \xe2\x86\x92")), head, juce::Justification::centredRight);
    juce::ignoreUnused (ev);

    auto axis = b.removeFromBottom (14);
    auto bars = b.removeFromBottom (40); b.removeFromBottom (4);
    auto plot = b.withTrimmedTop (4);
    const float t0 = trace.t0, t1 = trace.endTime();
    auto X = [&] (float t) { return plot.getX() + (t - t0) / (t1 - t0) * plot.getWidth(); };

    // reverb window
    const float wx0 = X (sel.w0), wx1 = X (sel.w1);
    juce::ColourGradient wg (p[lime].withAlpha (0.20f * juce::jmax (0.4f, p.tint)), wx0, 0, p[lime].withAlpha (0.0f), wx1, 0, false);
    wg.addColour (0.5, p[lime].withAlpha (0.07f));
    g.setGradientFill (wg); g.fillRect (juce::Rectangle<float>::leftTopRightBottom (wx0, plot.getY(), wx1, bars.getBottom()));
    g.setColour (p[lime]); g.fillRect (wx0 - 0.75f, plot.getY(), 1.5f, bars.getBottom() - plot.getY());
    drawPhases (g, trace, plot, X, p, true);

    const auto& d = trace.zne[1];
    float mx = 1e-20f; for (auto v : d) mx = juce::jmax (mx, std::abs (v));
    const int W = (int) plot.getWidth();
    juce::RectangleList<float> rl;
    const float mid = plot.getCentreY() + 4;
    for (int x = 0; x < W; ++x)
    {
        const size_t a = (size_t) ((double) x / W * (double) d.size()), z = std::min (d.size(), (size_t) ((double) (x + 1) / W * (double) d.size()) + 1);
        float lo = 0, hi = 0;
        for (size_t i = a; i < z; ++i) { lo = juce::jmin (lo, d[i]); hi = juce::jmax (hi, d[i]); }
        const float y0 = mid - hi / mx * plot.getHeight() * 0.44f, y1 = mid - lo / mx * plot.getHeight() * 0.44f;
        rl.add (plot.getX() + (float) x, y0, 1.0f, juce::jmax (0.8f, y1 - y0));
    }
    g.setColour (p.t1); g.fillRectList (rl);

    // energy strip
    const int nb = 40; const float bw = bars.getWidth() / nb;
    std::vector<float> e ((size_t) nb, 0.0f); float em = 1e-20f;
    for (int i = 0; i < nb; ++i)
    {
        const size_t a = d.size() * (size_t) i / nb, z = d.size() * (size_t) (i + 1) / nb;
        for (size_t k = a; k < z; ++k) e[(size_t) i] = juce::jmax (e[(size_t) i], std::abs (d[k]));
        em = juce::jmax (em, e[(size_t) i]);
    }
    for (int i = 0; i < nb; ++i)
    {
        const float h = juce::jmax (2.0f, std::sqrt (e[(size_t) i] / em) * bars.getHeight());
        auto r = juce::Rectangle<float> (bars.getX() + (float) i * bw + 1, bars.getBottom() - h, bw - 2, h);
        juce::ColourGradient cg (p[teal].withAlpha (0.9f), 0, bars.getY(), p[teal].withAlpha (0.25f), 0, bars.getBottom(), false);
        g.setGradientFill (cg); g.fillRoundedRectangle (r, 1.5f);
    }

    g.setColour (p.t3); g.setFont (font (10.0f));
    for (float t = 0; t < t1; t += (t1 > 1200 ? 600.0f : 300.0f))
        if (X (t) < axis.getRight() - 140) g.drawText (juce::String ((int) t) + " s", juce::Rectangle<float> (X (t) - 2, axis.getY(), 60, 14), juce::Justification::centredLeft);
    g.drawText (juce::String (juce::roundToInt (sel.w1 - sel.w0)) + juce::String (juce::CharPointer_UTF8 (" s of ground motion \xe2\x86\x92 "))
                + juce::String (proc.apvts.getRawParameterValue ("length")->load(), 1) + " s", axis, juce::Justification::centredRight);
}

std::unique_ptr<juce::AccessibilityHandler> SignalView::createAccessibilityHandler() { return imageHandler (*this, {}); }

// ---------------------------------------------------------------- three components
void WavesView::selectionChanged()
{
    if (auto* s = station())
    {
        trace = proc.pack.readTrace (*s);
        setDescription ("Ground velocity at " + s->label() + ": vertical, north and east. P at " + juce::String (juce::roundToInt (trace.P))
                        + " s, S at " + juce::String (juce::roundToInt (trace.S)) + " s.");
    }
}

void WavesView::paint (juce::Graphics& g)
{
    const auto p = Settings::get().palette();
    auto b = getLocalBounds().toFloat();
    if (trace.zne[0].empty()) return;
    auto axis = b.removeFromBottom (14);
    auto plot = b.withTrimmedLeft (18);
    const float t0 = trace.t0, t1 = trace.endTime();
    auto X = [&] (float t) { return plot.getX() + (t - t0) / (t1 - t0) * plot.getWidth(); };

    const float wx0 = X (sel.w0), wx1 = X (sel.w1);
    juce::ColourGradient wg (p[lime].withAlpha (0.14f * juce::jmax (0.4f, p.tint)), wx0, 0, p[lime].withAlpha (0.0f), wx1, 0, false);
    g.setGradientFill (wg); g.fillRect (juce::Rectangle<float>::leftTopRightBottom (wx0, plot.getY(), wx1, plot.getBottom()));
    g.setColour (p[lime]); g.fillRect (wx0 - 0.75f, plot.getY(), 1.5f, plot.getHeight());
    drawPhases (g, trace, plot, X, p, false);

    float mx = 1e-20f;
    for (auto& c : trace.zne) for (auto v : c) mx = juce::jmax (mx, std::abs (v));
    const char* names[] = { "Z", "N", "E" };
    const Accent cols[] = { teal, lav, orange };
    const float lh = plot.getHeight() / 3.0f;
    for (int c = 0; c < 3; ++c)
    {
        const auto& d = trace.zne[(size_t) c];
        const float mid = plot.getY() + lh * ((float) c + 0.5f);
        g.setColour (p.ink.withAlpha (0.1f)); g.drawHorizontalLine ((int) mid, plot.getX(), plot.getRight());
        const int W = (int) plot.getWidth();
        juce::RectangleList<float> rl;
        for (int x = 0; x < W; ++x)
        {
            const size_t a = (size_t) ((double) x / W * (double) d.size()), z = std::min (d.size(), (size_t) ((double) (x + 1) / W * (double) d.size()) + 1);
            float lo = 0, hi = 0;
            for (size_t i = a; i < z; ++i) { lo = juce::jmin (lo, d[i]); hi = juce::jmax (hi, d[i]); }
            const float y0 = mid - hi / mx * lh * 0.48f, y1 = mid - lo / mx * lh * 0.48f;
            rl.add (plot.getX() + (float) x, y0, 1.0f, juce::jmax (0.8f, y1 - y0));
        }
        g.setColour (p[cols[c]]); g.fillRectList (rl);
        g.setColour (p.t3); g.setFont (font (10.0f));
        g.drawText (names[c], juce::Rectangle<float> (b.getX(), mid - 7, 14, 14), juce::Justification::centredLeft);
    }
    g.setColour (p.t3); g.setFont (font (10.0f));
    for (float t = 0; t < t1; t += (t1 > 1200 ? 600.0f : 300.0f))
        if (X (t) < axis.getRight() - 30) g.drawText (juce::String ((int) t) + " s", juce::Rectangle<float> (X (t) - 2, axis.getY(), 60, 14), juce::Justification::centredLeft);
}

std::unique_ptr<juce::AccessibilityHandler> WavesView::createAccessibilityHandler() { return imageHandler (*this, {}); }

// ---------------------------------------------------------------- IR spectrogram
void IRView::setIR (const juce::AudioBuffer<float>& ir, double rate)
{
    const int N = 1024, H = 64, W = 96;
    seconds = ir.getNumSamples() / (float) rate;
    cols.assign ((size_t) W, std::vector<float> ((size_t) H, -200.0f));
    if (ir.getNumSamples() >= N)
    {
        juce::dsp::FFT fft (10);
        juce::dsp::WindowingFunction<float> win ((size_t) N, juce::dsp::WindowingFunction<float>::hann);
        std::vector<float> buf ((size_t) N * 2);
        const int hop = juce::jmax (1, (ir.getNumSamples() - N) / W);
        const double fmin = 60, fmax = juce::jmin (18000.0, rate / 2);
        for (int x = 0; x < W; ++x)
        {
            std::fill (buf.begin(), buf.end(), 0.0f);
            for (int i = 0; i < N && x * hop + i < ir.getNumSamples(); ++i)
                buf[(size_t) i] = 0.5f * (ir.getSample (0, x * hop + i) + ir.getSample (1, x * hop + i));
            win.multiplyWithWindowingTable (buf.data(), (size_t) N);
            fft.performFrequencyOnlyForwardTransform (buf.data());
            for (int y = 0; y < H; ++y)
            {
                const double f0 = fmin * std::pow (fmax / fmin, 1.0 - (double) (y + 1) / H), f1 = fmin * std::pow (fmax / fmin, 1.0 - (double) y / H);
                const int b0 = juce::jlimit (1, N / 2 - 1, (int) (f0 / rate * N)), b1 = juce::jlimit (b0 + 1, N / 2, (int) (f1 / rate * N) + 1);
                float acc = 0; for (int k = b0; k < b1; ++k) acc += buf[(size_t) k] * buf[(size_t) k];
                cols[(size_t) x][(size_t) y] = 10.0f * std::log10 (acc / (float) (b1 - b0) + 1e-18f);
            }
        }
    }
    recolour();
}

void IRView::recolour()
{
    const auto p = Settings::get().palette();
    const int W = (int) cols.size(), H = W > 0 ? (int) cols[0].size() : 0;
    if (W == 0) return;
    float gmax = -200; for (auto& c : cols) for (auto v : c) gmax = juce::jmax (gmax, v);
    const juce::Colour stops[] = { p.globe3, p[peri].withMultipliedBrightness (0.55f), p[teal], p[green], p[yellow], p[yellow].brighter (0.6f) };
    img = juce::Image (juce::Image::RGB, W, H, true);
    for (int x = 0; x < W; ++x)
        for (int y = 0; y < H; ++y)
        {
            float v = juce::jlimit (0.0f, 0.999f, (cols[(size_t) x][(size_t) y] - gmax + 60.0f) / 60.0f);
            v = std::floor (v * 7.0f) / 7.0f;   // stepped, pixel look
            const float s = v * 5.0f; const int i = (int) s;
            img.setPixelAt (x, y, stops[i].interpolatedWith (stops[juce::jmin (5, i + 1)], s - (float) i));
        }
    setDescription ("Impulse response spectrogram, 60 hertz to 18 kilohertz, " + juce::String (seconds, 1) + " seconds long.");
    repaint();
}

void IRView::paint (juce::Graphics& g)
{
    const auto p = Settings::get().palette();
    auto b = getLocalBounds().toFloat();
    auto axis = b.removeFromBottom (14);
    if (img.isValid())
    {
        juce::Graphics::ScopedSaveState s (g);
        juce::Path clip; clip.addRoundedRectangle (b, 8.0f);
        g.reduceClipRegion (clip);
        g.setImageResamplingQuality (juce::Graphics::lowResamplingQuality);
        g.drawImage (img, b, juce::RectanglePlacement::stretchToFit);
    }
    g.setColour (p.t3); g.setFont (font (10.0f));
    g.drawText ("0 s", axis, juce::Justification::centredLeft);
    g.drawText ("60 Hz to 18 kHz, log", axis, juce::Justification::centred);
    g.drawText (juce::String (seconds, 1) + " s", axis, juce::Justification::centredRight);
}

std::unique_ptr<juce::AccessibilityHandler> IRView::createAccessibilityHandler() { return imageHandler (*this, {}); }

// ---------------------------------------------------------------- coda fingerprint
void FingerprintView::paint (juce::Graphics& g)
{
    const auto p = Settings::get().palette();
    auto* st = station();
    if (st == nullptr || st->bands.empty()) return;
    auto b = getLocalBounds().toFloat();
    auto axis = b.removeFromBottom (14);
    auto top = b.removeFromTop (b.getHeight() * 0.48f);
    auto bot = b.withTrimmedTop (4);
    const int nb = (int) st->bands.size();

    g.setFont (font (10.0f)); g.setColour (p.t3);
    g.drawText ("band level", top.removeFromTop (14), juce::Justification::centredLeft);
    g.drawText ("seismic RT60", bot.removeFromTop (14), juce::Justification::centredLeft);
    const float cw = top.getWidth() / (float) nb;
    for (int i = 0; i < nb; ++i)
    {
        const float lvl = juce::jlimit (0.02f, 1.0f, 1.0f + st->bands[(size_t) i].energyDb / 80.0f);
        auto cell = juce::Rectangle<float> (top.getX() + (float) i * cw, top.getY(), cw, top.getHeight()).reduced (1.5f, 0);
        auto bar = cell.withTrimmedTop (cell.getHeight() * (1.0f - lvl));
        juce::ColourGradient cg (p[lime], 0, bar.getY(), p[lime].withAlpha (0.35f), 0, bar.getBottom(), false);
        g.setGradientFill (cg); g.fillRoundedRectangle (bar, 2.0f);
    }

    float lo = 1e9f, hi = 0;
    for (auto& band : st->bands) if (band.rt60 > 0) { lo = juce::jmin (lo, band.rt60); hi = juce::jmax (hi, band.rt60); }
    if (hi <= 0) return;
    const float l0 = std::log10 (lo * 0.7f), l1 = std::log10 (hi * 1.4f);
    auto Y = [&] (float rt) { return bot.getBottom() - 4 - (std::log10 (rt) - l0) / (l1 - l0) * (bot.getHeight() - 8); };
    g.setColour (p.t3); g.setFont (font (9.5f));
    g.drawText (juce::String (juce::roundToInt (hi)) + " s", juce::Rectangle<float> (bot.getRight() - 50, Y (hi) - 13, 50, 12), juce::Justification::centredRight);
    g.drawText (juce::String (juce::roundToInt (lo)) + " s", juce::Rectangle<float> (bot.getRight() - 50, Y (lo) + 2, 50, 12), juce::Justification::centredRight);
    juce::Path path; bool started = false;
    for (int i = 0; i < nb; ++i)
    {
        const auto& band = st->bands[(size_t) i];
        const float x = bot.getX() + ((float) i + 0.5f) * cw;
        if (band.rt60 <= 0) { started = false; continue; }
        const float y = Y (band.rt60);
        if (! started) path.startNewSubPath (x, y); else path.lineTo (x, y);
        started = true;
    }
    g.setColour (p[teal].withAlpha (0.8f)); g.strokePath (path, juce::PathStrokeType (1.3f));
    for (int i = 0; i < nb; ++i)
    {
        const auto& band = st->bands[(size_t) i];
        if (band.rt60 <= 0) continue;
        g.setColour (p[teal].withAlpha (band.fitDb >= 10 ? 1.0f : 0.5f));
        g.fillEllipse (juce::Rectangle<float> (6, 6).withCentre ({ bot.getX() + ((float) i + 0.5f) * cw, Y (band.rt60) }));
    }
    g.setColour (p.t3); g.setFont (font (9.5f));
    for (int i = 0; i < nb; i += 3)
    {
        const float hz = st->bands[(size_t) i].hz;
        g.drawText (hz < 1 ? juce::String (hz, 1) : juce::String (juce::roundToInt (hz)), juce::Rectangle<float> (axis.getX() + (float) i * cw, axis.getY(), cw, 14), juce::Justification::centred);
    }
    g.drawText ("Hz", axis, juce::Justification::centredRight);
}

std::unique_ptr<juce::AccessibilityHandler> FingerprintView::createAccessibilityHandler() { return imageHandler (*this, {}); }
} // namespace ui
