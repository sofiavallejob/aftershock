// Aftershock, a reverb made from earthquakes. (C) 2026 Sofia Vallejo Budziszewski, AGPL-3.0.
// Free to port, by humans. Bring your own ears. See PORTING.md.

#pragma once
#include <juce_audio_processors/juce_audio_processors.h>

class AftershockProcessor;

// Factory presets (compiled in from presets/factory_presets.json) and user presets
// (one small JSON file each, *.aftershock, in the user's preset folder).
struct Preset
{
    juce::String name, category, about, eventId, station;
    juce::NamedValueSet params;
    bool factory = true;
    juce::File file;
    juce::String key() const { return (factory ? "f:" : "u:") + name; }
};

class PresetManager
{
public:
    explicit PresetManager (AftershockProcessor&);

    const juce::StringArray& categories() const { return cats; }
    std::vector<Preset> all();                     // factory first, then user presets
    void rescanUser();

    void load (const Preset&);
    void loadFactory (int index);
    void loadInit();
    void step (int delta);                         // previous / next in the full list
    juce::Result saveUser (const juce::String& name, const juce::String& category, const juce::String& about);
    juce::Result importFile (const juce::File&);
    juce::Result deleteUser (const Preset&);

    static juce::File userFolder();
    juce::String currentName() const { return current; }
    juce::String currentCategory() const { return currentCat; }
    void setCurrentName (const juce::String& n);
    int numFactory() const { return (int) factory.size(); }
    int currentFactoryIndex() const;
    juce::String factoryName (int i) const { return juce::isPositiveAndBelow (i, (int) factory.size()) ? factory[(size_t) i].name : juce::String(); }
    const Preset* find (const juce::String& key);

    // favourites, stored next to the user presets
    bool isFavourite (const Preset& p) const { return favourites.contains (p.key()); }
    void toggleFavourite (const Preset&);

    juce::String describe (const Preset&) const;   // "Petrinja 2020 · near · 179 km"

    std::function<void()> onChange;

    static juce::var toJson (const Preset&);
    static bool fromJson (const juce::var&, Preset&);

private:
    Preset capture (const juce::String& name, const juce::String& category, const juce::String& about) const;
    void saveFavourites();

    AftershockProcessor& proc;
    std::vector<Preset> factory, user;
    juce::StringArray cats, favourites;
    juce::String current { "Init" }, currentCat;
};
