#pragma once
#include "Generation/Patterns.h"
#include "Theme.h"
#include <juce_gui_extra/juce_gui_extra.h>
namespace wingman {
class GeneratorPanel final : public juce::Component {
    Theme theme;
    PatternSettings settings;
    juce::PropertyPanel properties;
    juce::Slider phrase;
    juce::TextButton vary{"Vary four-bar phrase"};
    juce::TextButton apply{"Generate / regenerate"};
    juce::Label status;
    std::function<juce::Result(const PatternSettings&)> callback;
    std::function<juce::Result(int,int)> variation;
    std::vector<juce::Value> bindings;
public:
    GeneratorPanel(PatternSettings initial,std::function<juce::Result(const PatternSettings&)> action,std::function<juce::Result(int,int)> varyAction)
        : settings(initial),callback(std::move(action)),variation(std::move(varyAction)) {
        setLookAndFeel(&theme);
        addAndMakeVisible(phrase); addAndMakeVisible(vary);
        phrase.setRange(1,initial.bars,1); phrase.setValue(1); phrase.setTooltip("First bar of the phrase to vary (1-based)");
        vary.onClick=[this]{const int first=static_cast<int>(phrase.getValue())-1; auto result=variation(first,std::min(4,settings.bars-first)); status.setText(result.wasOk()?"Phrase varied; other bars retained":result.getErrorMessage(),juce::dontSendNotification);};
        addAndMakeVisible(properties); addAndMakeVisible(apply); addAndMakeVisible(status);
        juce::Array<juce::PropertyComponent*> fields;
        auto value = [this](juce::var initialValue) { bindings.emplace_back(initialValue); return bindings.back(); };
        fields.add(new juce::TextPropertyComponent(value(juce::String(settings.seed)),"Seed",10,false));
        fields.add(new juce::SliderPropertyComponent(value(settings.variation),"Variation",0,99,1));
        fields.add(new juce::ChoicePropertyComponent(value(settings.bars),"Bars",{"4","8","16","32"},{4,8,16,32}));
        fields.add(new juce::ChoicePropertyComponent(value(static_cast<int>(settings.style)),"Style",{"Punjabi pop","Bhangra-inspired","Bollywood","Hip-hop","Trap","House","Funk","Acoustic pop"},{0,1,2,3,4,5,6,7}));
        fields.add(new juce::SliderPropertyComponent(value(settings.octave),"Bass octave",-2,2,1));
        fields.add(new juce::SliderPropertyComponent(value(settings.velocity),"Velocity",1,127,1));
        fields.add(new juce::SliderPropertyComponent(value(settings.lowest),"Melody lowest note",0,127,1));
        fields.add(new juce::SliderPropertyComponent(value(settings.highest),"Melody highest note",0,127,1));
        fields.add(new juce::ChoicePropertyComponent(value(settings.keyRoot),"Scale root",{"C","C#","D","Eb","E","F","F#","G","Ab","A","Bb","B"},{0,1,2,3,4,5,6,7,8,9,10,11}));
        fields.add(new juce::BooleanPropertyComponent(value(settings.minor),"Scale mode","Natural minor (off = major)"));
        const std::array<juce::String,12> labels{"Density","Swing","Humanization","Note length","Complexity","Root emphasis","Syncopation","Kick density","Snare variation","Fill amount","Repetition","Ghost notes"};
        const std::array<double,11> values{settings.density,settings.swing,settings.humanize,settings.noteLength,settings.complexity,settings.rootEmphasis,settings.syncopation,settings.kickDensity,settings.snareVariation,settings.fill,settings.repetition};
        for(int i=0;i<11;++i) fields.add(new juce::SliderPropertyComponent(value(values[static_cast<std::size_t>(i)]),labels[static_cast<std::size_t>(i)],0,1,0.01));
        fields.add(new juce::BooleanPropertyComponent(value(settings.ghostNotes),"Ghost notes","Enable quiet snare notes"));
        fields.add(new juce::ChoicePropertyComponent(value(settings.hatSubdivision),"Hi-hat per beat",{"2","4","8"},{2,4,8}));
        const juce::StringArray instruments{"Kick","Snare","Clap","Closed hat","Open hat","Percussion","Low tom","Mid tom","High tom"};
        for(int i=0;i<9;++i) fields.add(new juce::SliderPropertyComponent(value(settings.drumMap[static_cast<std::size_t>(i)]),instruments[i]+" MIDI note",0,127,1));
        properties.addProperties(fields);
        apply.onClick=[this] {
            int i=0; settings.seed=static_cast<std::uint32_t>(bindings[static_cast<std::size_t>(i++)].getValue().toString().getLargeIntValue());
            settings.variation=bindings[static_cast<std::size_t>(i++)].getValue(); settings.bars=bindings[static_cast<std::size_t>(i++)].getValue();
            settings.style=static_cast<Style>(static_cast<int>(bindings[static_cast<std::size_t>(i++)].getValue()));
            settings.octave=bindings[static_cast<std::size_t>(i++)].getValue(); settings.velocity=bindings[static_cast<std::size_t>(i++)].getValue();
            settings.lowest=bindings[static_cast<std::size_t>(i++)].getValue(); settings.highest=bindings[static_cast<std::size_t>(i++)].getValue();
            settings.keyRoot=bindings[static_cast<std::size_t>(i++)].getValue(); settings.minor=bindings[static_cast<std::size_t>(i++)].getValue();
            for(double* field : {&settings.density,&settings.swing,&settings.humanize,&settings.noteLength,&settings.complexity,&settings.rootEmphasis,&settings.syncopation,&settings.kickDensity,&settings.snareVariation,&settings.fill,&settings.repetition}) *field=bindings[static_cast<std::size_t>(i++)].getValue();
            settings.ghostNotes=bindings[static_cast<std::size_t>(i++)].getValue(); settings.hatSubdivision=bindings[static_cast<std::size_t>(i++)].getValue();
            for(auto& note : settings.drumMap) note=bindings[static_cast<std::size_t>(i++)].getValue();
            const auto result=callback(settings); status.setText(result.wasOk()?"Generated. Close this window to audition/export.":result.getErrorMessage(),juce::dontSendNotification);
        };
        setSize(560,640);
    }
    ~GeneratorPanel() override {setLookAndFeel(nullptr);}
    void resized() override { properties.setBounds(getLocalBounds().reduced(12).withTrimmedBottom(118)); phrase.setBounds(16,getHeight()-110,200,30); vary.setBounds(224,getHeight()-110,getWidth()-240,30); apply.setBounds(16,getHeight()-72,getWidth()-32,32); status.setBounds(16,getHeight()-38,getWidth()-32,30); }
};
}
