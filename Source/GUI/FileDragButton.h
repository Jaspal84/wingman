#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "Files/DragFileStore.h"
namespace wingman {
// Native external dragging must begin during a held mouse gesture, not onClick.
class FileDragButton final : public juce::TextButton {
    bool attempted=false;
public:
    std::function<juce::Result(juce::File&)> prepare;
    std::function<void(const juce::String&)> report;
    FileDragButton(){setButtonText("Drag");setTooltip("Hold and drag to a DAW; use Export if unsupported");}
    void mouseDown(const juce::MouseEvent& e) override {attempted=false;juce::TextButton::mouseDown(e);}
    void mouseDrag(const juce::MouseEvent& e) override {
        juce::TextButton::mouseDrag(e);
        if(attempted || e.getDistanceFromDragStart()<6 || !prepare)return;
        attempted=true;juce::File file;auto result=prepare(file);
        if(result.failed()){if(report)report(result.getErrorMessage());return;}
        // Refresh retention on every gesture, including re-dragging an older stem.
        if(file.getParentDirectory().getParentDirectory()==DragFileStore::root())
            file.getParentDirectory().setLastModificationTime(juce::Time::getCurrentTime());
        const bool started=juce::DragAndDropContainer::performExternalDragDropOfFiles({file.getFullPathName()},false,this);
        if(report)report(started?"File drag started; host import requires verification":"External drag unavailable; use Export instead");
        // Do not delete here or in the completion callback. Logic may read later.
    }
};
}
