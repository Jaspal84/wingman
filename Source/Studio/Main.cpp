#include <juce_audio_utils/juce_audio_utils.h>
#include <cstdio>
#include "Studio/Project.h"
#include "GUI/Theme.h"
#include "Threading/BackgroundThread.h"

namespace wingman {
// Standalone-only initial arrangement surface. Existing AU and project schema stay intact.
class Studio final : public juce::AudioAppComponent, public juce::FileDragAndDropTarget, private juce::Timer {
    Theme theme;
    ArrangementPlaybackEngine engine;
    std::array<Track,8> tracks;
    std::array<juce::File,8> sources;
    std::array<juce::TextButton,8> imports, mutes, solos;
    std::array<juce::Slider,8> gains, pans;
    const std::array<juce::String,8> names{"Kick","Snare","Hi-Hat","Bass","Chords","Melody","Vocals","FX"};
    juce::TextButton play{"Play"}, stop{"Stop"}, loop{"Loop"}, devices{"Audio device"}, bounce{"Export WAV"};
    juce::TextButton saveProject{"Save project"},openProject{"Open project"};
    juce::Label status;
    std::unique_ptr<juce::FileChooser> chooser;
    BackgroundThread worker;
    bool busy=false;
    double deviceRate=44100;
    void publish(){engine.publish(tracks,loop.getToggleState());repaint();}
    void chooseImport(int lane){
        if(busy)return;
        chooser=std::make_unique<juce::FileChooser>("Import into "+names[static_cast<std::size_t>(lane)],juce::File{},"*.wav;*.aif;*.aiff;*.flac");
        chooser->launchAsync(juce::FileBrowserComponent::openMode|juce::FileBrowserComponent::canSelectFiles,
            [safe=juce::Component::SafePointer<Studio>(this),lane](const auto& dialog){if(safe&&dialog.getResult().existsAsFile())safe->load(dialog.getResult(),lane);});
    }
    void load(juce::File file,int lane){
        if(busy)return;
        busy=true;status.setText("Decoding "+file.getFileName(),juce::dontSendNotification);
        worker.start([safe=juce::Component::SafePointer<Studio>(this),file,lane](CancellationToken token){
            std::shared_ptr<const Clip> clip;
            const auto result=studio::decode(file,clip);auto error=result.getErrorMessage();
            if(token.stop_requested())return;
            juce::MessageManager::callAsync([safe,lane,clip,error,file]{if(!safe)return;safe->busy=false;if(clip){safe->tracks[static_cast<std::size_t>(lane)].clip=clip;safe->sources[static_cast<std::size_t>(lane)]=file;safe->publish();safe->status.setText("Imported "+clip->name,juce::dontSendNotification);}else safe->status.setText(error,juce::dontSendNotification);});
        });
    }
    void projectDialog(bool saving){
        if(busy)return;
        chooser=std::make_unique<juce::FileChooser>(saving?"Save project (references source files)":"Open project",juce::File{},"*.jaspro");
        chooser->launchAsync((saving?juce::FileBrowserComponent::saveMode|juce::FileBrowserComponent::warnAboutOverwriting:juce::FileBrowserComponent::openMode)|juce::FileBrowserComponent::canSelectFiles,
            [safe=juce::Component::SafePointer<Studio>(this),saving](const auto& dialog){if(!safe||safe->busy||dialog.getResult()==juce::File{})return;auto file=saving?dialog.getResult().withFileExtension("jaspro"):dialog.getResult();safe->busy=true;safe->status.setText(saving?"Saving project...":"Loading project...",juce::dontSendNotification);
                const studio::Project snapshot{safe->tracks,safe->sources,safe->loop.getToggleState()};
                safe->worker.start([safe,saving,file,snapshot](CancellationToken token){auto next=std::make_shared<studio::Project>();auto result=saving?studio::save(file,snapshot):studio::load(file,*next,[&]{return token.stop_requested();});if(token.stop_requested())return;
                    juce::MessageManager::callAsync([safe,saving,next,result]{if(!safe)return;safe->busy=false;if(result.wasOk()&&!saving){safe->engine.playing=false;safe->engine.seek(0);safe->tracks=next->tracks;safe->sources=next->sources;safe->loop.setToggleState(next->loop,juce::dontSendNotification);for(std::size_t i=0;i<8;++i){safe->gains[i].setValue(next->tracks[i].gain,juce::dontSendNotification);safe->pans[i].setValue(next->tracks[i].pan,juce::dontSendNotification);safe->mutes[i].setToggleState(next->tracks[i].mute,juce::dontSendNotification);safe->solos[i].setToggleState(next->tracks[i].solo,juce::dontSendNotification);}safe->publish();}safe->status.setText(result.wasOk()?(saving?"Project saved (source audio stays in its original location)":"Project loaded"):result.getErrorMessage(),juce::dontSendNotification);});
                });
            });
    }
    void exportMix(){
        if(busy)return;
        bool loaded=false;for(const auto& t:tracks)loaded|=bool(t.clip);
        if(!loaded){status.setText("Import audio before exporting",juce::dontSendNotification);return;}
        chooser=std::make_unique<juce::FileChooser>("Export 24-bit stereo mix",juce::File::getSpecialLocation(juce::File::userDocumentsDirectory).getChildFile("Jas-Pro-Mix.wav"),"*.wav");
        chooser->launchAsync(juce::FileBrowserComponent::saveMode|juce::FileBrowserComponent::canSelectFiles|juce::FileBrowserComponent::warnAboutOverwriting,
            [safe=juce::Component::SafePointer<Studio>(this)](const auto& dialog){if(!safe||safe->busy||dialog.getResult()==juce::File{})return;safe->render(dialog.getResult().withFileExtension("wav"));});
    }
    void render(juce::File destination){
        busy=true;status.setText("Rendering stereo mix...",juce::dontSendNotification);
        const auto snapshot=tracks;
        worker.start([safe=juce::Component::SafePointer<Studio>(this),snapshot,destination](CancellationToken token){
            const auto result=studio::renderMix(destination,snapshot,[&]{return token.stop_requested();});const bool ok=result.wasOk();
            juce::MessageManager::callAsync([safe,ok]{if(safe){safe->busy=false;safe->status.setText(ok?"Exported 48 kHz / 24-bit stereo WAV":"Export failed or cancelled",juce::dontSendNotification);}});
        });
    }
public:
    explicit Studio(bool enableDevice=true){
        setLookAndFeel(&theme);
        for(auto* button:{&play,&stop,&loop,&devices,&bounce,&saveProject,&openProject})addAndMakeVisible(button);
        saveProject.onClick=[this]{projectDialog(true);};openProject.onClick=[this]{projectDialog(false);};
        loop.setClickingTogglesState(true);loop.onClick=[this]{publish();};
        play.onClick=[this]{bool loaded=false;double end=0;for(const auto& t:tracks)if(t.clip){loaded=true;end=std::max(end,t.clip->audio.getNumSamples()/t.clip->sampleRate);}if(loaded){if(engine.position()>=end)engine.seek(0);engine.playing=true;}else status.setText("Import audio first",juce::dontSendNotification);};
        stop.onClick=[this]{engine.playing=false;engine.seek(0);};
        bounce.onClick=[this]{exportMix();};
        devices.onClick=[this]{juce::DialogWindow::LaunchOptions options;options.dialogTitle="Audio output";options.dialogBackgroundColour=panel;options.content.setOwned(new juce::AudioDeviceSelectorComponent(deviceManager,0,0,2,2,false,false,true,false));options.content->setSize(500,430);options.launchAsync();};
        for(std::size_t i=0;i<8;++i){
            imports[i].setButtonText("Import");imports[i].onClick=[this,i]{chooseImport(static_cast<int>(i));};
            for(auto* b:{&mutes[i],&solos[i]}){b->setClickingTogglesState(true);addAndMakeVisible(b);}
            mutes[i].setButtonText("M");solos[i].setButtonText("S");
            mutes[i].onClick=[this,i]{tracks[i].mute=mutes[i].getToggleState();publish();};
            solos[i].onClick=[this,i]{tracks[i].solo=solos[i].getToggleState();publish();};
            gains[i].setRange(0,1,.01);gains[i].setValue(.8);gains[i].setTooltip("Volume");
            pans[i].setRange(-1,1,.01);pans[i].setValue(0);pans[i].setTooltip("Stereo pan");
            gains[i].onValueChange=[this,i]{tracks[i].gain=static_cast<float>(gains[i].getValue());publish();};
            pans[i].onValueChange=[this,i]{tracks[i].pan=static_cast<float>(pans[i].getValue());publish();};
            for(auto* s:{&gains[i],&pans[i]}){s->setSliderStyle(juce::Slider::LinearHorizontal);s->setTextBoxStyle(juce::Slider::NoTextBox,false,0,0);addAndMakeVisible(s);}
            addAndMakeVisible(imports[i]);
        }
        addAndMakeVisible(status);status.setText("Eight-track playback | Import local WAV, AIFF or FLAC | 30 seconds per lane",juce::dontSendNotification);
        setSize(1200,760);if(enableDevice)setAudioChannels(0,2);startTimerHz(20);
    }
    ~Studio()override{stopTimer();shutdownAudio();worker.request_stop();worker.join();setLookAndFeel(nullptr);}
    void prepareToPlay(int,double rate)override{deviceRate=rate;}
    void releaseResources()override{}
    void getNextAudioBlock(const juce::AudioSourceChannelInfo& info)override{info.clearActiveBufferRegion();juce::AudioBuffer<float> view(info.buffer->getArrayOfWritePointers(),info.buffer->getNumChannels(),info.startSample,info.numSamples);engine.begin();engine.render(view,deviceRate);}
    bool isInterestedInFileDrag(const juce::StringArray& files)override{return files.size()==1&&juce::File(files[0]).hasFileExtension("wav;aif;aiff;flac");}
    void filesDropped(const juce::StringArray& files,int,int y)override{if(busy||!isInterestedInFileDrag(files))return;const int row=(getHeight()-150)/8;const int lane=(y-112)/row;if(y>=112&&lane>=0&&lane<8)load(juce::File(files[0]),lane);}
    void mouseDown(const juce::MouseEvent& event)override{if(event.x<310||event.y<112||event.y>=getHeight()-38)return;double length=0;for(const auto& track:tracks)if(track.clip)length=std::max(length,track.clip->audio.getNumSamples()/track.clip->sampleRate);engine.seek(juce::jlimit(0.0,length,static_cast<double>(event.x-310)/std::max(1,getWidth()-330)*std::max(1.0,length)));}
    void resized()override{
        int x=20;for(auto* b:{&play,&stop,&loop,&devices,&bounce,&saveProject,&openProject}){const int width=(getWidth()-40)/7;b->setBounds(x,58,width-8,34);x+=width;}
        const int row=(getHeight()-150)/8;
        for(int i=0;i<8;++i){const int y=112+i*row;imports[static_cast<std::size_t>(i)].setBounds(110,y+8,70,28);mutes[static_cast<std::size_t>(i)].setBounds(188,y+8,32,28);solos[static_cast<std::size_t>(i)].setBounds(224,y+8,32,28);gains[static_cast<std::size_t>(i)].setBounds(110,y+40,90,20);pans[static_cast<std::size_t>(i)].setBounds(205,y+40,90,20);}
        status.setBounds(20,getHeight()-30,getWidth()-40,25);
    }
    void paint(juce::Graphics& g)override{
        g.fillAll(background);g.setColour(juce::Colours::white);g.setFont(25);g.drawText("Jas PRO Ultimate Loop Studio",20,10,getWidth()-40,36,juce::Justification::centredLeft);
        const int row=(getHeight()-150)/8;double length=1;for(const auto& t:tracks)if(t.clip)length=std::max(length,t.clip->audio.getNumSamples()/t.clip->sampleRate);
        for(std::size_t i=0;i<8;++i){const int y=112+static_cast<int>(i)*row;g.setColour(panel);g.fillRoundedRectangle(10.0f,static_cast<float>(y),static_cast<float>(getWidth()-20),static_cast<float>(row-4),6);g.setColour(juce::Colours::white);g.setFont(17);g.drawText(names[i],20,y,88,row-4,juce::Justification::centredLeft);
            auto area=juce::Rectangle<int>(310,y+5,getWidth()-330,row-14);g.setColour(juce::Colour(0xff47d8c0));
            if(auto clip=tracks[i].clip){for(int x=0;x<area.getWidth();++x){const double seconds=static_cast<double>(x)/area.getWidth()*length;auto index=static_cast<std::size_t>(seconds*clip->sampleRate/Clip::peakHop);float peak=index<clip->peaks.size()?clip->peaks[index]:0;float height=std::min(1.0f,peak)*static_cast<float>(area.getHeight())*.45f;g.drawVerticalLine(area.getX()+x,static_cast<float>(area.getCentreY())-height,static_cast<float>(area.getCentreY())+height);}}
            g.setColour(juce::Colours::white);int cursor=area.getX()+static_cast<int>(engine.position()/length*area.getWidth());g.drawVerticalLine(cursor,static_cast<float>(area.getY()),static_cast<float>(area.getBottom()));
        }
    }
    void timerCallback()override{repaint();}
};
class Application final:public juce::JUCEApplication {
    class Window final:public juce::DocumentWindow {
    public:Window():DocumentWindow("Jas PRO Ultimate Loop Studio",background,allButtons){setUsingNativeTitleBar(true);setContentOwned(new Studio,true);setResizable(true,true);setResizeLimits(1000,650,2400,1500);centreWithSize(1200,760);setVisible(true);}void closeButtonPressed()override{juce::JUCEApplication::getInstance()->systemRequestedQuit();}
    };
    std::unique_ptr<Window> window;
public:
    const juce::String getApplicationName()override{return "Jas PRO Ultimate Loop Studio";}
    const juce::String getApplicationVersion()override{return "0.1.0";}
    void initialise(const juce::String& arguments)override{
        if(arguments.contains("--self-test")){
            struct TestLog final:juce::Logger {bool asserted=false;void logMessage(const juce::String& message)override{asserted|=message.containsIgnoreCase("Assertion failure");std::fputs(message.toRawUTF8(),stderr);std::fputs("\n",stderr);}} log;
            juce::Logger::setCurrentLogger(&log);bool ok=true;
            {Studio studio(false);
            for(auto size:{juce::Point<int>(1000,650),juce::Point<int>(1200,760),juce::Point<int>(2400,1500)}){studio.setSize(size.x,size.y);for(auto* child:studio.getChildren())if(child->isVisible())ok=ok&&studio.getLocalBounds().contains(child->getBounds());ok=ok&&studio.createComponentSnapshot(studio.getLocalBounds()).isValid();}
            }
            juce::Logger::setCurrentLogger(nullptr);
            setApplicationReturnValue(ok&&!log.asserted?0:1);quit();return;
        }
        window=std::make_unique<Window>();
    }
    void shutdown()override{window.reset();}
};
}
START_JUCE_APPLICATION(wingman::Application)
