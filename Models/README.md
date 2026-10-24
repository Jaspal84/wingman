# Model manager and offline inference

No model weights are bundled or downloaded automatically. Model Manager configures the runtime/model paths, verifies local installation, requests explicit download/license consent, starts background work and reports errors rather than showing fake separated sources.

Evaluated backend: official Demucs **4.0.1**, source tag `ef66d254cd6d558e207eeff2c4b8d053db2e77dd`, MIT license: https://github.com/facebookresearch/demucs/blob/v4.0.1/LICENSE . Native PyTorch checkpoint HTDemucs `955717e8`, four sources (`drums`, `bass`, `other`, `vocals`), 44.1 kHz stereo. Official source URL:

https://dl.fbaipublicfiles.com/demucs/hybrid_transformer/955717e8-8726e21a.th

Approximate download: 80 MB (actual size must be measured on download). Upstream publishes the SHA-256 prefix `8726e21a` in its pinned filename. The installer validates that prefix over a TLS-verified official download, then records the complete SHA-256 in `wingman-model.json`; offline use revalidates that full digest. Supply `--sha256 <trusted full digest>` for independent full-digest pinning when available. The short upstream prefix is not a full independent signature. Do not substitute a different file or disable verification. Treat model weights as executable trusted material because the upstream checkpoint uses Python serialization.

Runtime validated for import on Linux: CPython 3.12, torch **2.5.1+cpu**, torchaudio **2.5.1+cpu**, Demucs **4.0.1**. A native macOS arm64 PyTorch CPU runtime is the initial proposed M1 path. ONNX/CoreML compatibility and M1 performance have **not** been established. The checkpoint is not an arbitrary ONNX graph.

Use Python 3.12 and `Scripts/setup-inference.sh /absolute/runtime/path` to install the pinned runtime in a new dedicated virtual environment (the script preserves existing environments), then explicitly consent to model download and its MIT terms:

```sh
python Scripts/stem_worker.py install --models /path/to/models --allow-download --accept-mit-license
python Scripts/stem_worker.py check --models /path/to/models
python Scripts/stem_worker.py separate --models /path/to/models --input song.wav --output /path/to/empty-output --status /path/to/status.json --cancel /path/to/cancel.flag
```

Use the Python executable from the dedicated runtime, not an unrelated system Python. The worker is offline during `check` and `separate`; it only loads the local verified model. It normalizes/resamples input, runs sequential overlapping chunks on two CPU threads, reconstructs synchronized output and writes four 24-bit WAVs plus measured wall time and peak process RSS. Input is capped at 120 seconds; only one worker should run at once. Cancellation is checked between model chunks; a supervising process can terminate a worker for immediate cancellation. Partial exports are removed on handled failure. HTDemucs internally retains its training-window requirement even with shorter outer segments, so 8-GB M1 suitability cannot be asserted before a native memory benchmark.

Current blockers: the official weight host returned HTTP 403 in this environment and the allowlist addition awaits application through environment settings. Actual model inference, synchronized real-recording acceptance, source quality, peak RAM and M1 timing remain **unverified**. The repository contains a worker implementation, not evidence of a successfully running separator. No model redistribution or separation-quality claim is made.

Installations are versioned under `htdemucs-v4.0.1-955717e8` inside the selected model directory; checkpoint and manifest are published together by directory rename. The worker also accepts legacy flat verified directories. The GUI supervisor owns private staging and cancellation; successful four-file output is validated before replacing all lanes together. Python-worker failure tests and actual subprocess missing-model protocol tests do not count as trained-model inference acceptance.
