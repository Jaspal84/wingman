#!/usr/bin/env bash
set -euo pipefail
if [[ $# -ne 1 ]]; then
  echo 'Usage: WINGMAN_PYTHON=python3.12 ./Scripts/setup-inference.sh /absolute/path/to/runtime' >&2
  exit 2
fi
cd "$(dirname "$0")/.."
wingman_runtime="$1"
"${WINGMAN_PYTHON:-python3.12}" -c 'import sys; assert sys.version_info[:2] == (3,12), "Use Python 3.12 for this pinned worker"'
if [[ -e "$wingman_runtime" ]]; then
  echo 'Choose a new runtime directory; existing environments are preserved.' >&2
  exit 1
fi
"${WINGMAN_PYTHON:-python3.12}" -m venv "$wingman_runtime"
"$wingman_runtime/bin/python" -m pip install 'pip==24.3.1'
if [[ "$(uname -s)" == Linux ]]; then
  "$wingman_runtime/bin/python" -m pip install 'torch==2.5.1+cpu' 'torchaudio==2.5.1+cpu' --index-url https://download.pytorch.org/whl/cpu
  "$wingman_runtime/bin/python" -m pip install -r Scripts/worker-requirements-linux.lock
elif [[ "$(uname -s)" == Darwin ]]; then
  "$wingman_runtime/bin/python" -m pip install -r Scripts/worker-requirements-macos.lock
else
  echo 'Supported worker platforms: Linux and macOS.' >&2
  exit 1
fi
"$wingman_runtime/bin/python" -c 'import torch,torchaudio,demucs; print(torch.__version__,torchaudio.__version__,demucs.__version__)'
echo 'Runtime installed. No model weights were downloaded. Configure this Python in Model Manager.'
