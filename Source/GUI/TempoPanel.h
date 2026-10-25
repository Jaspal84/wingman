#pragma once
#include "../PluginProcessor.h"
#include "Theme.h"
namespace wingman {
class TempoPanel final:public juce::Component,private juce::Timer {
    WingmanProcessor& processor;
    juce::ComboBox source;
    juce::TextButton analyse{"Re-analyse"},cancel{"Cancel"},apply{"Apply Detected Tempo"},half{"Half-time"},twice{"Double-time"},tap{"Tap Tempo"},detectKey{"Detect Key / Chords"};
    juce::Label result,status,keyResult;
    TapTempo taps;
    void timerCallback()override{
        auto r=processor.tempoSnapshot();if(processor.tempoSourceTrack()!=source.getSelectedId()-1)r={};auto job=processor.tempoAnalysisStatus();
        result.setText(r.valid?juce::String(r.bpm,1)+" BPM | periodicity confidence "+juce::String(r.confidence*100,0)+"% | alternatives "+juce::String(r.halfTime,1)+" / "+juce::String(r.doubleTime,1):"No analyzed tempo available",juce::dontSendNotification);
        const auto harmony=processor.analysisSnapshot();const juce::StringArray keys{"C","C#","D","Eb","E","F","F#","G","Ab","A","Bb","B"};
        keyResult.setText(harmony.regions.empty()?"No analyzed key available":"Last analyzed key: "+keys[harmony.keyRoot]+(harmony.minor?" minor":" major")+" | heuristic margin "+juce::String(harmony.keyConfidence*100,0)+"%",juce::dontSendNotification);
        if(job.running||job.message!=lastMessage){lastMessage=job.message;status.setText(job.message,juce::dontSendNotification);}apply.setEnabled(r.valid&&!job.running&&!processor.hostSync());half.setEnabled(r.valid&&r.halfTime>=40&&!processor.hostSync());twice.setEnabled(r.valid&&r.doubleTime<=240&&!processor.hostSync());tap.setEnabled(!processor.hostSync());analyse.setEnabled(!job.running);cancel.setEnabled(job.running);repaint();
    }
    juce::String lastMessage;
public:
    explicit TempoPanel(WingmanProcessor& p):processor(p){
        source.addItemList({"Vocals","Other","Bass","Drums","Guitar"},1);source.setSelectedId(processor.tempoSourceTrack()>=0?processor.tempoSourceTrack()+1:4);
        for(auto* c:std::initializer_list<juce::Component*>{&source,&analyse,&cancel,&apply,&half,&twice,&tap,&detectKey,&result,&status,&keyResult})addAndMakeVisible(c);
        analyse.onClick=[this]{auto r=processor.startTempoAnalysis(source.getSelectedId()-1);if(r.failed())status.setText(r.getErrorMessage(),juce::dontSendNotification);};cancel.onClick=[this]{processor.cancelTempoAnalysis();};
        detectKey.onClick=[this]{auto r=processor.startAnalysis(source.getSelectedId()-1);status.setText(r.wasOk()?"Key/chord analysis started":r.getErrorMessage(),juce::dontSendNotification);};
        apply.onClick=[this]{auto r=processor.applyDetectedTempo();status.setText(r.wasOk()?"Detected tempo applied":r.getErrorMessage(),juce::dontSendNotification);};
        half.onClick=[this]{auto r=processor.tempoSnapshot();if(r.valid)processor.setTempo(r.halfTime);};twice.onClick=[this]{auto r=processor.tempoSnapshot();if(r.valid)processor.setTempo(r.doubleTime);};
        tap.onClick=[this]{if(auto bpm=taps.tap(juce::Time::getMillisecondCounterHiRes()/1000)){processor.setTempo(*bpm);status.setText("Manual tap tempo: "+juce::String(*bpm,1)+" BPM",juce::dontSendNotification);}};result.setFont(juce::FontOptions(23.0f,juce::Font::bold));timerCallback();startTimerHz(5);
    }
    void resized()override{auto area=getLocalBounds().reduced(24);area.removeFromTop(46);auto row=area.removeFromTop(36);source.setBounds(row.removeFromLeft(220));row.removeFromLeft(12);analyse.setBounds(row.removeFromLeft(180));row.removeFromLeft(12);cancel.setBounds(row.removeFromLeft(100));row.removeFromLeft(12);detectKey.setBounds(row.removeFromLeft(200));area.removeFromTop(20);result.setBounds(area.removeFromTop(50));auto controls=area.removeFromTop(38);for(auto* c:{&apply,&half,&twice,&tap})c->setBounds(controls.removeFromLeft(area.getWidth()/4).reduced(3));status.setBounds(area.removeFromTop(40));keyResult.setBounds(area.removeFromTop(32));}
    void paint(juce::Graphics& g)override{
        g.setColour(panel);g.fillRoundedRectangle(getLocalBounds().toFloat(),12);g.setColour(juce::Colours::white);g.setFont(juce::FontOptions(22.0f,juce::Font::bold));g.drawText("TEMPO & ANALYSIS",24,10,getWidth()-48,30,juce::Justification::centredLeft);
        auto r=processor.tempoSnapshot();auto bounds=getLocalBounds().reduced(24).withTrimmedTop(300).toFloat();g.setColour(background);g.fillRoundedRectangle(bounds,6);g.setColour(trackColours[4]);
        auto clip=processor.trackSnapshot(source.getSelectedId()-1).clip;
        if(clip){const double duration=clip->audio.getNumSamples()/clip->sampleRate;for(int x=0;x<static_cast<int>(bounds.getWidth());++x){const auto at=static_cast<int>(x/bounds.getWidth()*clip->audio.getNumSamples());const auto sample=clip->audio.getSample(0,std::min(at,clip->audio.getNumSamples()-1));g.drawVerticalLine(static_cast<int>(bounds.getX())+x,bounds.getCentreY()-std::abs(sample)*bounds.getHeight()*0.4f,bounds.getCentreY()+std::abs(sample)*bounds.getHeight()*0.4f);}if(r.valid&&processor.tempoSourceTrack()==source.getSelectedId()-1){g.setColour(juce::Colours::yellow.withAlpha(0.7f));for(double beat:r.beats)g.drawVerticalLine(static_cast<int>(bounds.getX()+beat/duration*bounds.getWidth()),bounds.getY(),bounds.getBottom());}}
        else{g.setColour(juce::Colours::lightgrey);g.drawText("Import or record audio, then analyze. Confidence is heuristic, not a calibrated probability.",bounds,juce::Justification::centred);}
    }
};
}
