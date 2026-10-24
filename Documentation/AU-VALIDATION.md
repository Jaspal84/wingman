# macOS installation and AU discovery

Preserved identity: bundle `com.wingmanaudio.wingman`, subtype `Wgm1`, manufacturer `Wgma`. Wingman accepts both audio and MIDI, so its explicit AU type is `kAudioUnitType_MusicEffect` (`aumf`). JUCE already defaulted to this category; the old hard-coded `aufx` validation command was inconsistent with that default. Do not change the identifier to work around a discovery failure.

On an Apple Silicon Mac with Xcode Command Line Tools, CMake and Python 3:

```sh
./Scripts/build-macos.sh
python3 Scripts/macos_au.py inspect 'build-macos/Wingman_artefacts/Release/AU/Wingman Pro.component'
./Scripts/install-macos.sh 'build-macos/Wingman_artefacts/Release/AU/Wingman Pro.component' --adhoc
./Scripts/validate-au.sh "$HOME/Library/Audio/Plug-Ins/Components/Wingman Pro.component" 2>&1 | tee auval.log
# Preserve auval's exit code when piping: use bash set -o pipefail first.
```

Installation stages a user-level copy, optionally ad-hoc signs that copy, verifies signing before replacing the destination, and retains the old bundle in a uniquely named backup. Ad-hoc signing is for local development, not notarized distribution. Existing `Wingman.component` bundles with the same identifiers should be moved to a backup outside the Components folder before installing `Wingman Pro.component`; duplicate registered identities can make discovery ambiguous. Do not remove unknown/user-modified bundles blindly.

Validation reads `AudioComponents` and `CFBundleExecutable` from the actual bundle, checks plist syntax, signature and architecture, then runs `auval -v <actual type> <actual subtype> <actual manufacturer>`. Capture its real output and exit status. `inspect` can be fixture-tested on Linux; it is not AU validation.

If `FATAL ERROR: didn't find the component` remains:

1. Confirm the bundle exists in the current user's `~/Library/Audio/Plug-Ins/Components` and the printed plist codes match the installed copy.
2. Use `auval -a` to inspect registered units; check for duplicates, arm64 compatibility and signing failures.
3. Close/reopen Logic/MainStage. If registration remains stale, log out/in and rescan in Logic Pro > Settings > Plug-in Manager. Do not indiscriminately delete all AU caches or kill unrelated audio applications.
4. Record macOS version, native/Rosetta mode, actual codes, architecture and signature output along with `auval` logs for diagnosis.

Linux status: scripts and plist fixtures tested; no compiled AU, actual `auval`, Logic, MainStage or CoreAudio validation has occurred. The user's reported discovery error cannot be declared fixed without those results.
