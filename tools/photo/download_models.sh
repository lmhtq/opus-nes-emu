#!/usr/bin/env bash
# Download the SDXL Turbo + fp16-fix VAE + ControlNet Tile weights used by
# fcemu Photo Mode. Total disk usage ≈ 8.6 GiB.
#
# 国内默认走 hf-mirror.com（更稳）。要走官方 huggingface.co 就 export
# HF_MIRROR=https://huggingface.co
set -euo pipefail

HF_MIRROR=${HF_MIRROR:-https://hf-mirror.com}
BASE_DIR=${SDXL_TURBO_DIR:-$HOME/sdxl-turbo-local}
EXTRA_DIR=${SDXL_EXTRAS_DIR:-$HOME/sdxl-extras}

echo "[photo-download] mirror=$HF_MIRROR"
echo "[photo-download] base=$BASE_DIR  extras=$EXTRA_DIR"
mkdir -p "$BASE_DIR"/{unet,text_encoder,text_encoder_2,vae,scheduler,tokenizer,tokenizer_2}
mkdir -p "$EXTRA_DIR"/{vae-fp16-fix,cn-tile}

dl() {
  local url=$1 out=$2
  if [ -s "$out" ]; then
    echo "  exists: $out ($(du -h "$out" | cut -f1))"
    return
  fi
  echo "  downloading $(basename "$out")..."
  curl -L --retry 10 --retry-delay 2 -C - -fSso "$out" "$url"
}

# 1. SDXL Turbo (fp16) — stabilityai/sdxl-turbo
SDXL=stabilityai/sdxl-turbo
dl "$HF_MIRROR/$SDXL/resolve/main/model_index.json"                                "$BASE_DIR/model_index.json"
for f in scheduler/scheduler_config.json \
         tokenizer/{merges.txt,special_tokens_map.json,tokenizer_config.json,vocab.json} \
         tokenizer_2/{merges.txt,special_tokens_map.json,tokenizer_config.json,vocab.json} \
         text_encoder/config.json \
         text_encoder_2/config.json \
         unet/config.json \
         vae/config.json ; do
  dl "$HF_MIRROR/$SDXL/resolve/main/$f" "$BASE_DIR/$f"
done
dl "$HF_MIRROR/$SDXL/resolve/main/text_encoder/model.fp16.safetensors"               "$BASE_DIR/text_encoder/model.fp16.safetensors"
dl "$HF_MIRROR/$SDXL/resolve/main/text_encoder_2/model.fp16.safetensors"             "$BASE_DIR/text_encoder_2/model.fp16.safetensors"
dl "$HF_MIRROR/$SDXL/resolve/main/unet/diffusion_pytorch_model.fp16.safetensors"     "$BASE_DIR/unet/diffusion_pytorch_model.fp16.safetensors"
dl "$HF_MIRROR/$SDXL/resolve/main/vae/diffusion_pytorch_model.fp16.safetensors"      "$BASE_DIR/vae/diffusion_pytorch_model.fp16.safetensors"

# 2. fp16-fix VAE — madebyollin/sdxl-vae-fp16-fix (消除 SDXL 原 VAE 在 fp16 NaN)
VAE=madebyollin/sdxl-vae-fp16-fix
dl "$HF_MIRROR/$VAE/resolve/main/config.json"                              "$EXTRA_DIR/vae-fp16-fix/config.json"
dl "$HF_MIRROR/$VAE/resolve/main/diffusion_pytorch_model.safetensors"      "$EXTRA_DIR/vae-fp16-fix/diffusion_pytorch_model.safetensors"

# 3. ControlNet Tile SDXL — xinsir/controlnet-tile-sdxl-1.0
CN=xinsir/controlnet-tile-sdxl-1.0
dl "$HF_MIRROR/$CN/resolve/main/config.json"                               "$EXTRA_DIR/cn-tile/config.json"
dl "$HF_MIRROR/$CN/resolve/main/diffusion_pytorch_model.safetensors"       "$EXTRA_DIR/cn-tile/diffusion_pytorch_model.safetensors"

echo ""
echo "[photo-download] done."
du -sh "$BASE_DIR" "$EXTRA_DIR"
