#pragma once
#include "Theme.h"
#include "AI/ModelManager/ModelTypes.h"
namespace wingman {
class ModelPanel final : public juce::Component,private juce::Timer {
    Theme theme;
    juce::String localError;
    juce::TextEditor python,models;
    juce::Label pythonLabel,modelsLabel,explanation,status;
    juce::TextButton pickPython{"Choose Python"},pickModels{"Choose models"},check{"Verify local model"},install{"Install official model"},separate{"Separate WAV"},cancel{"Cancel"};
    juce::ToggleButton consent{"I accept MIT weights terms and consent to the ~80 MB download"};
    std::unique_ptr<juce::FileChooser> chooser;
    std::function<void(const juce::String&,const juce::String&)> configure;
    std::function<juce::Result(const juce::String&,const juce::File&,bool)> start;
    std::function<void()> stop;
    std::function<ModelJobInfo()> read;
    void timerCallback() override {
        const auto job=read();
        status.setText((!localError.isEmpty()&&!job.running ? localError : job.message)+(job.running?" ("+juce::String(job.progress*100,0)+"%)":""),juce::dontSendNotification);
        check.setEnabled(!job.running);install.setEnabled(!job.running);separate.setEnabled(!job.running);cancel.setEnabled(job.running);
    }
    void run(const juce::String& action,const juce::File& input={}) {
        python.setText(python.getText().trim(),false);models.setText(models.getText().trim(),false);
        configure(python.getText(),models.getText());
        localError.clear();
        const auto result=start(action,input,consent.getToggleState());
        if(result.failed()){localError=result.getErrorMessage();status.setText(localError,juce::dontSendNotification);}
    }
public:
    ModelPanel(std::pair<juce::String,juce::String> config,
        std::function<void(const juce::String&,const juce::String&)> setConfig,
        std::function<juce::Result(const juce::String&,const juce::File&,bool)> startAction,
        std::function<void()> cancelAction,std::function<ModelJobInfo()> readStatus)
        :configure(std::move(setConfig)),start(std::move(startAction)),stop(std::move(cancelAction)),read(std::move(readStatus)) {
        setLookAndFeel(&theme);
        for(auto* c:std::initializer_list<juce::Component*>{&python,&models,&pythonLabel,&modelsLabel,&explanation,&status,&pickPython,&pickModels,&check,&install,&separate,&cancel,&consent})addAndMakeVisible(c);
        python.setText(config.first);models.setText(config.second);
        pythonLabel.setText("Pinned Python runtime executable",juce::dontSendNotification);modelsLabel.setText("Local HTDemucs model directory",juce::dontSendNotification);
        explanation.setText("Demucs 4.0.1 | torch/torchaudio 2.5.1 | CPU worker\nFour stems: vocals, drums, bass, other. MIT upstream weights.\nNo silent downloads. Checksum verified before loading.\nCore ML/ONNX and M1 performance are not validated.",juce::dontSendNotification);
        explanation.setJustificationType(juce::Justification::topLeft);
        pickPython.onClick=[this]{
            chooser=std::make_unique<juce::FileChooser>("Choose inference Python executable");
            chooser->launchAsync(juce::FileBrowserComponent::openMode|juce::FileBrowserComponent::canSelectFiles,[safe=juce::Component::SafePointer<ModelPanel>(this)](const auto& fc){if(safe&&fc.getResult()!=juce::File{})safe->python.setText(fc.getResult().getFullPathName());});
        };
        pickModels.onClick=[this]{
            chooser=std::make_unique<juce::FileChooser>("Choose installed model directory");
            chooser->launchAsync(juce::FileBrowserComponent::openMode|juce::FileBrowserComponent::canSelectDirectories,[safe=juce::Component::SafePointer<ModelPanel>(this)](const auto& fc){if(safe&&fc.getResult()!=juce::File{})safe->models.setText(fc.getResult().getFullPathName());});
        };
        check.onClick=[this]{run("check");};install.onClick=[this]{if(!consent.getToggleState()){localError="Enable download/license consent first";status.setText(localError,juce::dontSendNotification);return;}run("install");};
        separate.onClick=[this]{
            chooser=std::make_unique<juce::FileChooser>("Choose a WAV mix to separate",juce::File{},"*.wav");
            chooser->launchAsync(juce::FileBrowserComponent::openMode|juce::FileBrowserComponent::canSelectFiles,[safe=juce::Component::SafePointer<ModelPanel>(this)](const auto& fc){if(safe&&fc.getResult()!=juce::File{})safe->run("separate",fc.getResult());});
        };
        cancel.setEnabled(false); cancel.onClick=[this]{stop();};setSize(640,440);startTimerHz(5);
    }
    ~ModelPanel()override{stopTimer();setLookAndFeel(nullptr);}
    void resized()override{
        auto area=getLocalBounds().reduced(16);
        explanation.setBounds(area.removeFromTop(100));pythonLabel.setBounds(area.removeFromTop(22));
        auto row=area.removeFromTop(30);pickPython.setBounds(row.removeFromRight(130));python.setBounds(row.reduced(0,1));
        area.removeFromTop(8);modelsLabel.setBounds(area.removeFromTop(22));row=area.removeFromTop(30);pickModels.setBounds(row.removeFromRight(130));models.setBounds(row.reduced(0,1));
        consent.setBounds(area.removeFromTop(34));row=area.removeFromTop(34);check.setBounds(row.removeFromLeft(row.getWidth()/2).reduced(2));install.setBounds(row.reduced(2));
        area.removeFromTop(8);row=area.removeFromTop(34);separate.setBounds(row.removeFromLeft(row.getWidth()/2).reduced(2));cancel.setBounds(row.reduced(2));status.setBounds(area);
    }
};
}
