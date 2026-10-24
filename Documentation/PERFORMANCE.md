# Performance evidence and M1 report status

No Apple Silicon machine is attached. No M1, CoreML, CoreAudio or Logic measurements have been made. Real Linux HTDemucs time/RSS measurements are now available in INFERENCE-VALIDATION.md and Validation/*.json. These supersede the earlier model-host blocker. No real-time-separation claim is made.

The separate harmonic-test measurement in `linux-harmony-benchmark.json` is a Debug C++ harmonic-test command on an x86_64 Intel Xeon Platinum 8573C, five CPUs available to this process. The command is single-process/single-analysis-thread; other cloud build activity may affect scheduling. It analyses held synthetic chords at 22.05 kHz (96 cases, 2 seconds each, +15-cent tuning) and synthetic major/minor cadences (24 cases, 10 seconds each). FFT size 8192, hop 2205 samples.

Observed: **10.6801 s wall time, 10.6652 s CPU time, 10,354,688 bytes peak child RSS**. This is a test-dataset benchmark, not a real music track or stem-inference benchmark. It must not be extrapolated to M1. Peak RSS covers this command, not the JUCE GUI, embedded project audio or PyTorch.

Reproduce on the available machine:

```sh
python3 Scripts/benchmark.py --output /tmp/harmony-benchmark.json -- build/wingman_harmony_tests
```

Method: a fresh Python launcher measures `perf_counter` wall time, child `getrusage` CPU time and maximum RSS; Linux KiB is converted to bytes, macOS already reports bytes. The command's exit code is preserved, and platform/CPU/affinity are recorded with the result. See the JSON for actual raw values, not rounded prose.

For required native M1 acceptance after installing the verified model/runtime:

1. Record Mac model, 8/16-GB memory, macOS, native arm64 architecture, Python/PyTorch/Demucs versions, model full hash and power mode.
2. Use an explicitly licensed stereo reference recording, retain sample rate/duration and obtain reference source labels for quality metrics when possible.
3. Run one CPU separation at a time. The worker records end-to-end processing time, peak process RSS, output sample rate/frame count and stem statistics in benchmark.json. Keep that report and validate four synchronized files, nonempty signal on the reference music and no NaN samples.
4. Test cancellation during inference and verify process/staging cleanup. Test missing/corrupted models and memory failures.
5. Only evaluate an ONNX/CoreML backend after a compatible model conversion is proven and equivalent source quality is checked. Current checkpoints are native PyTorch; a CoreML speedup cannot be assumed.

The model worker bounds input to 120 seconds, uses two CPU threads, sequential overlapping chunks and no queued parallel model requests. HTDemucs retains its training-window padding internally, so shorter outer chunks do not prove that activation memory fits an 8-GB M1. Recording and embedded clip state also consume memory. Native memory-pressure testing remains required.

The macOS build helper defaults to two parallel build jobs to reduce peak compilation pressure on an 8-GB M1; `CMAKE_BUILD_PARALLEL_LEVEL` can override this. This is a conservative build setting, not a measured M1 optimization.

For exact native repository/build/AU/inference commands, use MACBOOK-M1-TESTING.md. Worker RSS excludes the parent plugin/DAW and cannot establish 8-GB M1 suitability.
