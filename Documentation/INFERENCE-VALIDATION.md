# Genuine HTDemucs validation — Linux, 2026-10-24

**Passed:** official model access/download, full-digest verification, real trained-model execution, four-stem export/validation, the plugin's embedded-worker/import/export path, active-inference cancellation and all automated suites.

**Not tested:** macOS AU/Logic/MainStage, physical audio hardware, native M1 performance, CoreML/ONNX, or source/perceptual accuracy on a broad music corpus. Follow [the exact Mac Terminal runbook](MACBOOK-M1-TESTING.md).

## Checkpoint and license

- Model: HTDemucs four-source `955717e8`, Demucs **4.0.1**; inspected class HTDemucs, 41,984,456 parameters, stereo 44.1 kHz, trained segment 7.8 seconds.
- Official URL: https://dl.fbaipublicfiles.com/demucs/hybrid_transformer/955717e8-8726e21a.th
- HTTP result: **200**; **84,141,911 bytes** downloaded. Environment-status metadata still showed an older allowlist; actual HTTPS access/download is the evidence of working access.
- Full SHA-256: `8726e21a993978c7ba086d3872e7608d7d5bfca646ca4aca459ffda844faa8b4`.
- Initial official TLS download matched upstream's published prefix `8726e21a`. The measured full digest is now pinned for both download and load, independently of manifest contents. A fresh installation using the full pin, license notice and atomic directory publication also passed.
- License provenance: the pinned Demucs release README and [MIT notice](https://github.com/facebookresearch/demucs/blob/ef66d254cd6d558e207eeff2c4b8d053db2e77dd/LICENSE). A copy is retained in Models/DEMUCS-LICENSE.txt and new model installations. Notice SHA-256: `cf9b17822d1fcd4ff32ccbe14183386fb3adf6f2ff92dc184130823f7fc28173`. No independently published full model digest was assumed: the full pin records the verified official download, while the upstream filename supplies only the prefix.
- Runtime: CPython **3.12.14**, torch/torchaudio **2.5.1+cpu**, Demucs **4.0.1**. Checkpoint hash and declared MIT metadata are checked before loading Python-serialized model weights. Unexpected metadata, altered notices and corrupted checkpoints are rejected.

## Real test input and method

The input is the official Demucs repository's `test.mp3` at release commit `ef66d254cd6d558e207eeff2c4b8d053db2e77dd`, decoded to 24-bit WAV with soundfile. This is an upstream music file, not a synthetic waveform generated to imitate separation.

- Source: https://github.com/facebookresearch/demucs/blob/ef66d254cd6d558e207eeff2c4b8d053db2e77dd/test.mp3
- Git blob verified against that checkout: `668604d876880d39913ef73bcf9df4b4308a9d70`.
- MP3 SHA-256: `aa8d8e51ca1b65e4e04f0be52c7e63b38c17aaaeded9193bc6ce0acf6d6a6d9a`.
- Decoded WAV SHA-256: `d364b64f5fce7759b75d88acf94c50a03e534d7b689aae597f72fef200a03dd1`.
- **20.000 seconds, 882,000 frames, two channels, 44.1 kHz**.

The sample and derived WAVs remain outside Git for local evaluation. This report does not grant production redistribution rights to the audio. For Mac/quality benchmarks use your own or appropriately licensed recordings and retain their provenance.

Actual `demucs.apply.apply_model` calls run the verified trained checkpoint in an external CPU process: two PyTorch intra-op threads, eval/inference mode, no randomized shifts, sequential four-second outer chunks, 25% overlap and weighted overlap-add reconstruction. HTDemucs internally retains its 7.8-second trained context. These are not EQ-generated or duplicated stems.

## Measured execution

Hardware: Linux x86_64 Intel Xeon Platinum 8573C, five CPUs available; details in Validation/linux-test-summary.json. No GPU/CoreML execution occurred. Each process measures monotonic elapsed time and Unix process peak RSS; Linux KiB is converted to bytes. Download time is excluded. End-to-end separation includes checksum, runtime/model initialization, audio preparation, model execution, overlap reconstruction and export. `inference_seconds` times synchronous CPU apply_model calls only.

| Actual run | Input | Separation total | Model-only time | Peak worker RSS |
| --- | --- | ---: | ---: | ---: |
| First standalone worker | 20 s stereo / 44.1 kHz | 48.1455 s | Not separately instrumented; inference+export 45.2230 s | 1,547,268,096 bytes |
| Conversion test | 6 s mono / 48 kHz | 18.0267 s | Not separately instrumented; inference+export 16.0657 s | 1,515,425,792 bytes |
| Final timed embedded plugin worker | 20 s stereo / 44.1 kHz | **43.3313 s** | **40.0638 s** | **1,502,093,312 bytes (1.40 GiB)** |

The final timed run's inference-plus-export interval is 40.2556 seconds. A prior embedded run also completed in 41.2067 seconds with 1,614,053,376 bytes peak RSS. Run-to-run scheduling/cache variation exists; these are observations, not a latency guarantee or M1 extrapolation. The final total/audio-duration ratio is 2.17; no real-time-separation claim is supported. Worker RSS does not include the parent plugin/DAW combined memory.

## Export and signal checks

The preserved native four-stem set passes Scripts/validate_stems.py:

- Vocals/drums/bass/other are each **882,000-frame stereo 44.1-kHz PCM_24 WAV**, exactly 20 seconds.
- All decoded samples are finite; each file contains nonzero signal; no pair contains identical samples.
- No saturated PCM samples in this fixture.

| Stem | RMS | Absolute peak |
| --- | ---: | ---: |
| Vocals | 0.000152387 | 0.00487542 |
| Drums | 0.088528116 | 0.64180028 |
| Bass | 0.000292999 | 0.01046562 |
| Other | 0.140946127 | 0.78429198 |

Mixture-sum residual RMS: **0.00425926**; mixture reconstruction SNR: **31.9811 dB**. Pairwise correlations and per-file SHA-256 values are retained in Validation/linux-stereo-quality.json.

**Quality limitation:** vocals and bass are very quiet relative to the mixture. This fixture therefore provides limited evidence for those categories. No isolated source references or listening assessment were available; **source SDR/SI-SDR, leakage/semantic correctness and perceptual quality are not established**. Reconstruction SNR is not source SDR and file validity alone is not a musical-quality score.

The 6-second mono/48-kHz input also succeeds: channel conversion plus resampling produces four stereo **264,600-frame, 44.1-kHz** stems. Structural checks pass; mixture reconstruction SNR is 29.8810 dB. It is a different input condition, not an equivalent latency comparison.

The real plugin integration runs the embedded worker, validates/atomically imports all four clips, then exports each lane through the actual plugin mixer. All four plugin exports are **960,000-frame stereo 48-kHz PCM_24**, finite/nonzero, with default 0.8 gain and existing linear resampling. Their statistics are retained separately. These are distinct from the worker's native 44.1-kHz exports.

## Cancellation and regression evidence

The opt-in integration test starts another trained-model run, waits until a genuine inference chunk completes, then cancels the owned child process. The final run stopped in **101.108 ms**, removed owned temporary directories and preserved every previously imported clip. This tests the supervisor's forced-child cancellation; it is not merely a pre-model cancellation flag test.

**8/8 CTest suites passed**, with real-model environment selectors enabled for integration:

- Stem validator: 7 tests.
- AU metadata fixtures: 6 tests, not auval.
- Model worker: 10 consent/checksum/license/status/cancellation tests.
- DSP: 15 checks.
- Harmony: 108 checks, synthetic held chords 96/96 and keys 24/24.
- Generation: 32,708 per-note/constraint/reproducibility checks.
- Transport/threading: 13 checks.
- Integration: 249 checks, including real embedded inference/import/export/cancellation.

Linux VST3 and Standalone rebuild successfully. Normal tests explicitly report when the real-inference block is not requested. No weights are silently downloaded by tests. A full Mac/Logic/M1 result is still unrun; the remaining review is OUTSTANDING-REVIEW.md.

## Reproduce in the prepared cloud snapshot

```sh
source /workspace/wingman-env.sh
/workspace/wingman-ai/bin/python Scripts/stem_worker.py check --models /workspace/wingman-models
/workspace/wingman-ai/bin/python Scripts/validate_stems.py \
  /workspace/wingman-validation/inputs/demucs-test.wav \
  /workspace/wingman-validation/first-run --report /tmp/stem-validation.json
WINGMAN_REAL_MODELS=/workspace/wingman-models \
WINGMAN_REAL_PYTHON=/workspace/wingman-ai/bin/python \
WINGMAN_REAL_INPUT=/workspace/wingman-validation/inputs/demucs-test.wav \
ctest --test-dir build -V
```

Optional `WINGMAN_REAL_EXPORTS` names a new directory to preserve plugin lane exports; tests refuse to overwrite existing output files. Input must be 8–120 seconds for the cancellation regression. See MACBOOK-M1-TESTING.md for fresh checkout, native build, installation, compiled-ID auval and real inference on the Mac.
