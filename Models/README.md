# Model manager and offline inference

No model weights are bundled or downloaded automatically. Model Manager configures the runtime/model paths, verifies local installation, requests explicit download/license consent, starts background work and reports errors rather than showing fake separated sources.

Evaluated backend: official Demucs **4.0.1**, source tag `ef66d254cd6d558e207eeff2c4b8d053db2e77dd`, MIT license: https://github.com/facebookresearch/demucs/blob/v4.0.1/LICENSE . Native PyTorch checkpoint HTDemucs `955717e8`, four sources (`drums`, `bass`, `other`, `vocals`), 44.1 kHz stereo. Official source URL:

https://dl.fbaipublicfiles.com/demucs/hybrid_transformer/955717e8-8726e21a.th

Verified checkpoint size: **84,141,911 bytes** (80.2 MiB). Full pinned SHA-256:

`8726e21a993978c7ba086d3872e7608d7d5bfca646ca4aca459ffda844faa8b4`

The first official TLS-verified download matched the upstream SHA-256 prefix `8726e21a`. Its complete digest is now pinned in the worker: both downloads and loads must match it. A manifest cannot choose an alternative digest sharing that prefix. Offline loads rehash the actual checkpoint. The pinned release's MIT license notice is included in DEMUCS-LICENSE.txt and new model installations; its digest is also checked when present. The release README/license is the license provenance; the short filename prefix is not a full independent signature.

Runtime validated for import on Linux: CPython 3.12, torch **2.5.1+cpu**, torchaudio **2.5.1+cpu**, Demucs **4.0.1**. A native macOS arm64 PyTorch CPU runtime is the initial proposed M1 path. ONNX/CoreML compatibility and M1 performance have **not** been established. The checkpoint is not an arbitrary ONNX graph.

Use Python 3.12 and `Scripts/setup-inference.sh /absolute/runtime/path` to install the pinned runtime in a new dedicated virtual environment (the script preserves existing environments), then explicitly consent to model download and its MIT terms:

```sh
python Scripts/stem_worker.py install --models /path/to/models --allow-download --accept-mit-license
python Scripts/stem_worker.py check --models /path/to/models
python Scripts/stem_worker.py separate --models /path/to/models --input song.wav --output /path/to/empty-output --status /path/to/status.json --cancel /path/to/cancel.flag
```

Use the Python executable from the dedicated runtime, not an unrelated system Python. The worker is offline during `check` and `separate`; it only loads the local verified model. It normalizes/resamples input, runs sequential overlapping chunks on two CPU threads, reconstructs synchronized output and writes four 24-bit WAVs plus measured wall time and peak process RSS. Input is capped at 120 seconds; only one worker should run at once. Cancellation is checked between model chunks; a supervising process can terminate a worker for immediate cancellation. Partial exports are removed on handled failure. HTDemucs internally retains its training-window requirement even with shorter outer segments, so 8-GB M1 suitability cannot be asserted before a native memory benchmark.

Linux result: actual HTDemucs inference now passes on the upstream real 20-second stereo fixture. Four synchronized, finite, nonempty/distinct 24-bit stems are exported; the embedded plugin worker, lane import/export and active-inference cancellation also pass. A 6-second 48-kHz mono input passes conversion/resampling checks. See Documentation/INFERENCE-VALIDATION.md for measured time/RSS and quality limits. The fixture's vocal/bass outputs are quiet; no isolated-source SDR or perceptual score is claimed. M1, CoreML/ONNX and actual AU/Logic/MainStage behavior remain untested. No checkpoint/audio assets are redistributed in Git.

Installations are versioned under `htdemucs-v4.0.1-955717e8` inside the selected model directory; checkpoint and manifest are published together by directory rename. The worker also accepts legacy flat verified directories. The GUI supervisor owns private staging and cancellation; successful four-file output is validated before replacing all lanes together. Python-worker failure tests and actual subprocess missing-model protocol tests do not count as trained-model inference acceptance.

The exact Mac clone/update/build/install/auval/inference commands are in Documentation/MACBOOK-M1-TESTING.md. The native CPU runtime pins are proposed but have not been executed on macOS.
