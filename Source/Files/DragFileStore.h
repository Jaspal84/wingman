#pragma once
#include <juce_core/juce_core.h>
#include <filesystem>
namespace wingman {
// External hosts may read asynchronously. Never remove files on drag completion.
class DragFileStore {
public:
    static constexpr juce::int64 retentionMilliseconds=7LL*24*60*60*1000;
    static juce::File root(){return juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("Wingman-DragFiles");}
    static juce::Result createDirectory(juce::File& directory){
        const auto parent=root();
        if(parent.isSymbolicLink())return juce::Result::fail("Drag cache must not be a symbolic link");
        if(parent.createDirectory().failed())return juce::Result::fail("Cannot create drag cache; use Export instead");
        std::error_code error;
        std::filesystem::permissions(parent.getFullPathName().toStdString(),std::filesystem::perms::owner_all,std::filesystem::perm_options::replace,error);
        if(error)return juce::Result::fail("Cannot protect drag cache permissions");
        directory=parent.getChildFile(juce::Uuid().toString());
        if(directory.createDirectory().failed())return juce::Result::fail("Cannot create unique drag directory");
        return juce::Result::ok();
    }
    // Called only on a background thread. Bound each pass and leave recent files,
    // unrelated directories and symlinks alone. Expiry uses directory mtime.
    static int cleanupExpired(const juce::File& parent,juce::Time now,int maxRemovals=128){
        if(!parent.isDirectory() || parent.isSymbolicLink())return 0;
        int removed=0,visited=0;
        for(const auto& entry:juce::RangedDirectoryIterator(parent,false,"*",juce::File::findDirectories)){
            if(++visited>4096 || removed>=maxRemovals)break;
            const auto file=entry.getFile();const auto name=file.getFileName();
            if(file.isSymbolicLink() || name.length()!=32 || !name.containsOnly("0123456789abcdef"))continue;
            if(now.toMilliseconds()-file.getLastModificationTime().toMilliseconds()<=retentionMilliseconds)continue;
            if(file.deleteRecursively())++removed;
        }
        return removed;
    }
};
}
