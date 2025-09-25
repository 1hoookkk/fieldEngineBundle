llama.cpp Docker (Qwen3 model)
================================

This folder runs the llama.cpp server in Docker and mounts your local model directory so you can serve a Qwen3 (or other GGUF) model.

Quick start
-----------

1) Put your GGUF model file in your host downloads folder (default is `C:/downloads` on Windows).

2) From this folder (`llama-docker/`), build and start:

   - Build (CUDA by default): `docker compose build`
   - Run (with GPU access):   `docker compose up`

   The server listens on `http://localhost:8080`.

3) Test the API (OpenAI-compatible):

   curl example:
   ```bash
   curl http://localhost:8080/v1/chat/completions \
     -H "Content-Type: application/json" \
     -d '{
       "model": "qwen3",
       "messages": [
         {"role":"user","content":"Hello!"}
       ]
     }'
   ```

Configuration
-------------

Edit `.env` to adjust:

- HOST_MODEL_DIR: host folder with GGUF files (default `C:/downloads`).
- MODEL_PATH: set to an explicit file inside `/models` (e.g. `/models/qwen3-*.gguf`). Leave empty to auto-pick the first `.gguf` file.
- CTX_SIZE, THREADS, NGL, SERVER_PORT: performance and port settings. For CUDA, `NGL` > 0 offloads that many layers to GPU.
- SERVER_EXTRA_ARGS: pass extra llama.cpp server flags.
- BUILD_TARGET: `cuda` (default) or `cpu` to choose build target.
- GPUS: `all` to allow GPU access; leave unset if building/running CPU only.

GPU prerequisites
-----------------

- NVIDIA driver and CUDA-capable GPU
- NVIDIA Container Toolkit installed and configured for Docker
- Docker Desktop + WSL2 on Windows if applicable

If you prefer CPU-only:

- Set `BUILD_TARGET=cpu` and `NGL=0` in `.env`
- Optionally remove or unset `GPUS` in `.env`

Notes
-----

- This setup includes both CPU and CUDA builds. Default is CUDA with cuBLAS.
- Qwen models: Ensure your model is in GGUF format. If you have a safetensors or PyTorch checkpoint, it must be converted first.

