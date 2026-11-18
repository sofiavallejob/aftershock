// Aftershock, a reverb made from earthquakes. (C) 2026 Sofia Vallejo Budziszewski, AGPL-3.0.
// Free to port, by humans. Bring your own ears. See PORTING.md.

// aftershock_snapshot: renders the editor in several states to PNG files (for docs and checks).
#include "PluginEditor.h"

int main (int argc, char* argv[])
{
    juce::ScopedJuceInitialiser_GUI init;
    juce::File out (argc > 1 ? juce::String (argv[1]) : juce::String ("shots"));
    out.createDirectory();
    auto& s = ui::Settings::get();
    for (auto k : { "theme", "scale", "textSize", "vision", "highContrast", "reduceMotion", "help", "advanced", "lastPreset" }) s.remove (k);
    s.set ("reduceMotion", true);

    AftershockProcessor proc;
    proc.setPlayConfigDetails (2, 2, 48000, 512);
    proc.prepareToPlay (48000, 512);
    for (int i = 0; i < 300 && proc.viewVersion.load() == 0; ++i) juce::Thread::sleep (10);
    proc.presets.loadFactory (0);
    for (int i = 0; i < 300 && proc.rendering.load() == false && proc.viewVersion.load() < 2; ++i) juce::Thread::sleep (10);
    juce::Thread::sleep (600);

    auto shoot = [&] (const juce::String& name, std::function<void (AftershockEditor&)> setup)
    {
        std::unique_ptr<AftershockEditor> ed (dynamic_cast<AftershockEditor*> (proc.createEditor()));
        ed->setVisible (true);

        if (setup) setup (*ed);
        for (int i = 0; i < 20; ++i) juce::MessageManager::getInstance()->runDispatchLoopUntil (20);
        auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
        juce::PNGImageFormat png;
        auto f = out.getChildFile (name + ".png"); f.deleteFile();
        juce::FileOutputStream os (f);
        png.writeImageToStream (img, os);
        std::cout << name << " " << ed->getWidth() << "x" << ed->getHeight() << "\n";

    };
    auto findChild = [] (juce::Component& root, const juce::String& title) -> juce::Component*
    {
        std::function<juce::Component* (juce::Component&)> rec = [&] (juce::Component& c) -> juce::Component*
        {
            for (auto* ch : c.getChildren()) { if (ch->getTitle() == title) return ch; if (auto* r = rec (*ch)) return r; }
            return nullptr;
        };
        return rec (root);
    };
    auto click = [&] (AftershockEditor& e, const juce::String& title) { if (auto* b = dynamic_cast<juce::Button*> (findChild (e, title))) b->triggerClick(); };

    shoot ("simple_dark", {});
    s.set ("theme", 1); shoot ("simple_light", {});
    s.set ("theme", 0); s.set ("highContrast", true); s.set ("vision", 1); shoot ("simple_hc", {});
    s.remove ("highContrast"); s.remove ("vision");
    shoot ("settings", [&] (AftershockEditor& e) { click (e, "Settings"); });
    shoot ("about", [&] (AftershockEditor& e) { click (e, "About Aftershock"); });
    shoot ("howitworks", [&] (AftershockEditor& e) { click (e, "About Aftershock"); click (e, "How it works"); });
    shoot ("presets", [&] (AftershockEditor& e) { click (e, "Preset"); });
    s.set ("advanced", true);
    shoot ("advanced_dark", {});
    s.set ("theme", 1); shoot ("advanced_light", {});
    s.remove ("theme"); s.remove ("advanced"); s.remove ("reduceMotion");
    return 0;
}
