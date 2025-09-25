#!/usr/bin/env bash
set -euo pipefail

# Auto-detect model if not provided
if [[ -z "${MODEL_PATH:-}" ]]; then
  # Prefer Qwen/Qwen2/Qwen3 models if present
  CANDIDATE=$(ls -1 /models 2>/dev/null | grep -Ei 'qwen(2|3)?|qwen' | grep -Ei '\.gguf$' | head -n1 || true)
  if [[ -z "$CANDIDATE" ]]; then
    # fallback: first gguf
    CANDIDATE=$(ls -1 /models/*.gguf 2>/dev/null | head -n1 || true)
  else
    CANDIDATE="/models/$CANDIDATE"
  fi
  if [[ -z "$CANDIDATE" ]]; then
    echo "No GGUF model found in /models. Mount your model and/or set MODEL_PATH."
    exit 1
  fi
  export MODEL_PATH="$CANDIDATE"
fi

# Default threads to all CPUs if 0 or unset
if [[ -z "${THREADS:-}" || "${THREADS}" == "0" ]]; then
  THREADS=$(nproc)
fi

echo "Starting llama.cpp server"
echo "Model: $MODEL_PATH"
echo "Port:  ${SERVER_PORT:-8080} | Threads: ${THREADS} | ctx: ${CTX_SIZE:-4096} | ngl: ${NGL:-0}"

exec /usr/local/bin/server \
  -m "$MODEL_PATH" \
  -c "${CTX_SIZE:-4096}" \
  -ngl "${NGL:-0}" \
  -t "$THREADS" \
  --host 0.0.0.0 \
  --port "${SERVER_PORT:-8080}" \
  ${SERVER_EXTRA_ARGS:-}

