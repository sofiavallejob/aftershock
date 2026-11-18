// Aftershock, a reverb made from earthquakes. (C) 2026 Sofia Vallejo Budziszewski, AGPL-3.0.
// Free to port, by humans. Bring your own ears. See PORTING.md.

#include "PresetManager.h"
#include "PluginProcessor.h"
#include "BinaryData.h"
#include "Theme.h"

namespace
{
const juce::StringArray stationKeys { "near", "regional", "tele" };
const juce::StringArray skipParams { "event", "station", "bypass", "output" };
}

PresetManager::PresetManager (AftershockProcessor& p) : proc (p)
{
    auto json = juce::JSON::parse (juce::String::fromUTF8 (BinaryData::factory_presets_json, BinaryData::factory_presets_jsonSize));
    if (auto* c = json["categories"].getArray()) for (auto& v : *c) cats.add (v.toString());
    if (auto* arr = json["presets"].getArray())
        for (auto& v : *arr) { Preset pr; if (fromJson (v, pr)) { pr.factory = true; factory.push_back (pr); } }
    cats.addIfNotAlreadyThere ("User");
    auto fav = userFolder().getChildFile ("favourites.txt");
    if (fav.existsAsFile()) favourites.addLines (fav.loadFileAsString());
    favourites.removeEmptyStrings();
    rescanUser();
}

juce::File PresetManager::userFolder()
{
    auto f = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile ("Aftershock").getChildFile ("Presets");
    f.createDirectory();
    return f;
}

bool PresetManager::fromJson (const juce::var& v, Preset& p)
{
    if (! v.isObject() || v["name"].toString().isEmpty()) return false;
    p.name = v["name"].toString();
    p.category = v["category"].toString();
    p.about = v["about"].toString();
    p.eventId = v["event"].toString();
    p.station = v["station"].toString();
    if (auto* o = v["params"].getDynamicObject()) p.params = o->getProperties();
    return true;
}

juce::var PresetManager::toJson (const Preset& p)
{
    auto* o = new juce::DynamicObject();
    o->setProperty ("format", "aftershock-preset");
    o->setProperty ("version", 1);
    o->setProperty ("name", p.name);
    o->setProperty ("category", p.category);
    o->setProperty ("about", p.about);
    o->setProperty ("event", p.eventId);
    o->setProperty ("station", p.station);
    auto* prm = new juce::DynamicObject();
    for (auto& nv : p.params) prm->setProperty (nv.name, nv.value);
    o->setProperty ("params", juce::var (prm));
    return juce::var (o);
}

void PresetManager::rescanUser()
{
    user.clear();
    for (auto& f : userFolder().findChildFiles (juce::File::findFiles, false, "*.aftershock"))
    {
        Preset p;
        if (fromJson (juce::JSON::parse (f), p)) { p.factory = false; p.file = f; if (p.category.isEmpty()) p.category = "User"; user.push_back (p); }
    }
    std::sort (user.begin(), user.end(), [] (auto& a, auto& b) { return a.name.compareNatural (b.name) < 0; });
}

std::vector<Preset> PresetManager::all()
{
    auto v = factory;
    v.insert (v.end(), user.begin(), user.end());
    return v;
}

const Preset* PresetManager::find (const juce::String& key)
{
    for (auto& p : factory) if (p.key() == key) return &p;
    for (auto& p : user) if (p.key() == key) return &p;
    return nullptr;
}

void PresetManager::load (const Preset& pr)
{
    auto& s = proc.apvts;
    proc.undoManager.beginNewTransaction ("Load preset " + pr.name);
    for (auto* prm : proc.getParameters())
        if (auto* r = dynamic_cast<juce::RangedAudioParameter*> (prm))
        {
            const auto id = r->getParameterID();
            if (skipParams.contains (id)) continue;
            float target = r->getDefaultValue();
            if (pr.params.contains (id)) target = r->convertTo0to1 ((float) (double) pr.params[id]);
            r->beginChangeGesture(); r->setValueNotifyingHost (target); r->endChangeGesture();
        }
    for (int i = 0; i < (int) proc.pack.events.size(); ++i)
        if (proc.pack.events[(size_t) i].id == pr.eventId)
            if (auto* r = s.getParameter ("event")) { r->beginChangeGesture(); r->setValueNotifyingHost (r->convertTo0to1 ((float) i)); r->endChangeGesture(); }
    const int st = stationKeys.indexOf (pr.station);
    if (st >= 0)
        if (auto* r = s.getParameter ("station")) { r->beginChangeGesture(); r->setValueNotifyingHost (r->convertTo0to1 ((float) st)); r->endChangeGesture(); }
    current = pr.name; currentCat = pr.category;
    ui::Settings::get().setQuietly ("lastPreset", pr.key());
    if (onChange) onChange();
}

void PresetManager::loadFactory (int i)
{
    if (juce::isPositiveAndBelow (i, (int) factory.size())) load (factory[(size_t) i]);
}

void PresetManager::loadInit()
{
    Preset p; p.name = "Init"; p.eventId = "petrinja2020"; p.station = "near";
    load (p);
}

void PresetManager::step (int d)
{
    auto v = all();
    if (v.empty()) return;
    int idx = -1;
    for (int i = 0; i < (int) v.size(); ++i) if (v[(size_t) i].name == current) idx = i;
    idx = (idx + d + (int) v.size()) % (int) v.size();
    load (v[(size_t) idx]);
}

int PresetManager::currentFactoryIndex() const
{
    for (int i = 0; i < (int) factory.size(); ++i) if (factory[(size_t) i].name == current) return i;
    return -1;
}

void PresetManager::setCurrentName (const juce::String& n)
{
    current = n.isEmpty() ? juce::String ("Init") : n;
    currentCat = {};
    for (auto& p : factory) if (p.name == current) currentCat = p.category;
    for (auto& p : user) if (p.name == current) currentCat = p.category;
}

Preset PresetManager::capture (const juce::String& name, const juce::String& category, const juce::String& about) const
{
    Preset p; p.name = name; p.category = category.isEmpty() ? juce::String ("User") : category; p.about = about; p.factory = false;
    const int e = proc.currentEventIndex();
    if (juce::isPositiveAndBelow (e, (int) proc.pack.events.size())) p.eventId = proc.pack.events[(size_t) e].id;
    p.station = stationKeys[(int) proc.apvts.getRawParameterValue ("station")->load()];
    for (auto* prm : proc.getParameters())
        if (auto* r = dynamic_cast<juce::RangedAudioParameter*> (prm))
            if (! skipParams.contains (r->getParameterID()))
                p.params.set (r->getParameterID(), (double) r->convertFrom0to1 (r->getValue()));
    return p;
}

juce::Result PresetManager::saveUser (const juce::String& name, const juce::String& category, const juce::String& about)
{
    const auto clean = juce::File::createLegalFileName (name.trim());
    if (clean.isEmpty()) return juce::Result::fail ("Give the preset a name.");
    auto p = capture (name.trim(), category, about);
    auto f = userFolder().getChildFile (clean + ".aftershock");
    if (! f.replaceWithText (juce::JSON::toString (toJson (p))))
        return juce::Result::fail ("Could not write " + f.getFullPathName());
    rescanUser();
    current = p.name; currentCat = p.category;
    if (onChange) onChange();
    return juce::Result::ok();
}

juce::Result PresetManager::importFile (const juce::File& f)
{
    Preset p;
    if (! fromJson (juce::JSON::parse (f), p)) return juce::Result::fail (f.getFileName() + " is not an Aftershock preset.");
    auto dest = userFolder().getChildFile (f.getFileName());
    if (dest != f && ! f.copyFileTo (dest)) return juce::Result::fail ("Could not copy the preset.");
    rescanUser();
    if (onChange) onChange();
    return juce::Result::ok();
}

juce::Result PresetManager::deleteUser (const Preset& p)
{
    if (p.factory || ! p.file.existsAsFile()) return juce::Result::fail ("Factory presets cannot be deleted.");
    p.file.moveToTrash();
    rescanUser();
    if (onChange) onChange();
    return juce::Result::ok();
}

void PresetManager::toggleFavourite (const Preset& p)
{
    if (favourites.contains (p.key())) favourites.removeString (p.key()); else favourites.add (p.key());
    saveFavourites();
    if (onChange) onChange();
}

void PresetManager::saveFavourites() { userFolder().getChildFile ("favourites.txt").replaceWithText (favourites.joinIntoString ("\n")); }

juce::String PresetManager::describe (const Preset& p) const
{
    for (auto& e : proc.pack.events)
        if (e.id == p.eventId)
        {
            juce::String s = e.name.upToFirstOccurrenceOf (",", false, false) + " " + e.time.substring (0, 4);
            for (auto& st : e.stations)
                if (st.band == p.station)
                    return s + " · " + (p.station == "tele" ? juce::String ("far") : p.station) + " · " + juce::String (juce::roundToInt (st.distKm)) + " km";
            return s;
        }
    return {};
}
