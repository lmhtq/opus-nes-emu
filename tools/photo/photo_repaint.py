#!/usr/bin/env python3
"""fcemu Photo Mode — cloud image-generation API client.

Repaints the current FC frame (256x240 PNG) through a hosted image-edit model
and writes a high-resolution PNG back to disk. Talks to fcemu over the same
UNIX-socket / line-JSON protocol the local SDXL daemon used to speak, so the
C++ side (launch_photo_repaint) is unchanged.

Supported providers (chosen via --backend or FCEMU_PHOTO_BACKEND, or per-
request via JSON "backend" field):
  - ark       Doubao SeedEdit / Seedream  (火山方舟 Ark)        env: ARK_API_KEY
  - openai    OpenAI Images edit (gpt-image-1)                 env: OPENAI_API_KEY

Adding a new provider is a ~40-line class that implements `repaint()`.

Daemon protocol (line-delimited JSON, one request → one reply):
  request : {"in": "...", "out": "...", "prompt": "...",
             "backend": "ark|openai" (optional, defaults to daemon's),
             "size": "1024x1024" (optional),
             "seed": 42 (optional),
             "guidance_scale": 5.5 (optional, ark only),
             "neg": "..." (optional, ignored by providers that don't take it)}
  reply   : {"ok": true,  "out": "...", "ms": 1234}
            {"ok": false, "error": "..."}
"""
import argparse
import base64
import io
import json
import os
import socket
import sys
import time
import traceback
from pathlib import Path
from typing import Optional

import requests
from PIL import Image


# ---------------------------------------------------------------------------
# Image helpers
# ---------------------------------------------------------------------------

def upscale_nearest(img: Image.Image, target_long: int = 1024) -> Image.Image:
    """NEAREST upscale so the longer side equals target_long. Keeps hard pixel
    edges intact — the API sees crisp pixel art, not a blurred 256x240."""
    w, h = img.size
    if max(w, h) >= target_long:
        return img
    s = target_long / max(w, h)
    return img.resize((int(round(w * s)), int(round(h * s))), Image.NEAREST)


def letterbox_square(img: Image.Image, side: int = 1024,
                     fill=(0, 0, 0)) -> Image.Image:
    """NEAREST upscale + letterbox into side x side. Used by providers that
    want a square input (OpenAI gpt-image-1)."""
    w, h = img.size
    s = min(side / w, side / h)
    nw, nh = int(round(w * s)), int(round(h * s))
    img2 = img.resize((nw, nh), Image.NEAREST)
    canvas = Image.new("RGB", (side, side), fill)
    canvas.paste(img2, ((side - nw) // 2, (side - nh) // 2))
    return canvas


def encode_png_bytes(img: Image.Image) -> bytes:
    buf = io.BytesIO()
    img.save(buf, format="PNG")
    return buf.getvalue()


def write_output(data: bytes, out_path: str) -> None:
    Path(out_path).parent.mkdir(parents=True, exist_ok=True)
    with open(out_path, "wb") as f:
        f.write(data)


# ---------------------------------------------------------------------------
# Provider base
# ---------------------------------------------------------------------------

DEFAULT_PROMPT = ("photorealistic, cinematic, ultra detailed, 35mm film, "
                  "dramatic lighting")


class Provider:
    name = "abstract"

    def repaint(self, in_path: str, out_path: str, *,
                prompt: str, **opts) -> int:
        """Run one repaint. Return elapsed milliseconds. Raise on failure."""
        raise NotImplementedError


# ---------------------------------------------------------------------------
# Doubao SeedEdit / Seedream  (Volcano Ark)
# ---------------------------------------------------------------------------

class ArkProvider(Provider):
    """Volcano Ark image-to-image / text-to-image.

    The same `images/generations` endpoint serves multiple model families:
      - Seedream 4.x  (multimodal t2i+i2i, recommended)   doubao-seedream-4-*
      - SeedEdit 3.0  (i2i only, older)                   doubao-seededit-3-0-i2i-*
      - Seedream 3.0  (pure t2i)                          doubao-seedream-3-0-t2i-*

    Default is Seedream 4.0. Override via (in order of priority):
      1) per-request JSON  {"model": "..."}
      2) CLI flag          --model=<id>
      3) env var           ARK_IMAGE_MODEL=<id>

    Different model families accept slightly different params; we only send
    fields that all current Ark image models tolerate, and forward
    guidance_scale only when the caller explicitly asks for it.
    """
    name = "ark"

    # Seedream 4.0 is the current stable Ark image model that supports both
    # t2i and i2i. Set ARK_IMAGE_MODEL to switch to 4.5 / 5 / SeedEdit / etc.
    DEFAULT_MODEL = "doubao-seedream-4-0-250828"
    ENDPOINT = "https://ark.cn-beijing.volces.com/api/v3/images/generations"

    def __init__(self, model: Optional[str] = None):
        self.api_key = os.environ.get("ARK_API_KEY", "").strip()
        if not self.api_key:
            raise RuntimeError("ARK_API_KEY not set")
        self.model = (model or os.environ.get("ARK_IMAGE_MODEL")
                      or self.DEFAULT_MODEL)
        self.session = requests.Session()

    def repaint(self, in_path, out_path, *, prompt, **opts):
        t0 = time.time()
        src = Image.open(in_path).convert("RGB")
        upscaled = upscale_nearest(src, opts.get("target_long", 1024))
        b64 = base64.b64encode(encode_png_bytes(upscaled)).decode("ascii")

        model = opts.get("model") or self.model
        body = {
            "model": model,
            "prompt": prompt or DEFAULT_PROMPT,
            "image": f"data:image/png;base64,{b64}",
            "response_format": "url",
            "size": opts.get("size") or "2K",
            "seed": int(opts.get("seed", 42)),
            "watermark": bool(opts.get("watermark", False)),
        }
        # guidance_scale: only SeedEdit 3.x accepts it; omit by default so
        # newer Seedream models don't reject the request.
        if opts.get("guidance_scale") is not None:
            body["guidance_scale"] = float(opts["guidance_scale"])

        headers = {
            "Authorization": f"Bearer {self.api_key}",
            "Content-Type": "application/json",
        }
        r = self.session.post(self.ENDPOINT, headers=headers,
                              json=body, timeout=180)
        if r.status_code == 404:
            raise RuntimeError(
                f"Ark HTTP 404 — model '{model}' not found or no access. "
                f"Override with ARK_IMAGE_MODEL=<id>, --model=<id>, or per-"
                f"request JSON {{\"model\":\"...\"}}. Response: {r.text[:200]}"
            )
        if r.status_code >= 400:
            raise RuntimeError(f"Ark HTTP {r.status_code}: {r.text[:400]}")
        data = r.json()
        items = data.get("data") or []
        if not items:
            raise RuntimeError(f"Ark response missing data: {data}")
        item = items[0]
        if "url" in item:
            img_bytes = self.session.get(item["url"], timeout=60).content
        elif "b64_json" in item:
            img_bytes = base64.b64decode(item["b64_json"])
        else:
            raise RuntimeError(f"Ark response has no url/b64_json: {item}")
        write_output(img_bytes, out_path)
        return int((time.time() - t0) * 1000)


# ---------------------------------------------------------------------------
# OpenAI Images (gpt-image-1, edit endpoint)
# ---------------------------------------------------------------------------

class OpenAIProvider(Provider):
    """OpenAI image edit. gpt-image-1 always returns b64_json. We send the
    image as a square PNG via multipart, matching the model's preferred input.
    """
    name = "openai"

    DEFAULT_MODEL = "gpt-image-1"
    ENDPOINT = "https://api.openai.com/v1/images/edits"

    def __init__(self, model: Optional[str] = None):
        self.api_key = os.environ.get("OPENAI_API_KEY", "").strip()
        if not self.api_key:
            raise RuntimeError("OPENAI_API_KEY not set")
        self.model = (model or os.environ.get("OPENAI_IMAGE_MODEL")
                      or self.DEFAULT_MODEL)
        self.session = requests.Session()

    def repaint(self, in_path, out_path, *, prompt, **opts):
        t0 = time.time()
        src = Image.open(in_path).convert("RGB")
        side = int(opts.get("side", 1024))
        squared = letterbox_square(src, side=side)
        png_bytes = encode_png_bytes(squared)

        size = opts.get("size", f"{side}x{side}")
        files = {"image": ("frame.png", png_bytes, "image/png")}
        data = {
            "model": self.model,
            "prompt": prompt or DEFAULT_PROMPT,
            "size": size,
            "n": "1",
        }
        headers = {"Authorization": f"Bearer {self.api_key}"}
        r = self.session.post(self.ENDPOINT, headers=headers,
                              data=data, files=files, timeout=180)
        if r.status_code >= 400:
            raise RuntimeError(f"OpenAI HTTP {r.status_code}: {r.text[:400]}")
        payload = r.json()
        items = payload.get("data") or []
        if not items:
            raise RuntimeError(f"OpenAI response missing data: {payload}")
        item = items[0]
        if "b64_json" in item:
            img_bytes = base64.b64decode(item["b64_json"])
        elif "url" in item:
            img_bytes = self.session.get(item["url"], timeout=60).content
        else:
            raise RuntimeError(f"OpenAI response has no b64/url: {item}")
        write_output(img_bytes, out_path)
        return int((time.time() - t0) * 1000)


# ---------------------------------------------------------------------------
# Provider registry
# ---------------------------------------------------------------------------

PROVIDERS = {
    "ark": ArkProvider,
    "openai": OpenAIProvider,
}


def make_provider(name: str, *, model: Optional[str] = None) -> Provider:
    cls = PROVIDERS.get(name)
    if cls is None:
        raise RuntimeError(f"unknown backend: {name} "
                           f"(known: {', '.join(PROVIDERS)})")
    return cls(model=model)


# ---------------------------------------------------------------------------
# Daemon
# ---------------------------------------------------------------------------

def serve(socket_path: str, default_backend: str,
          default_model: Optional[str] = None) -> None:
    # Eager-construct the default provider so missing API keys fail fast at
    # daemon startup (not on first P press).
    providers: dict[str, Provider] = {
        default_backend: make_provider(default_backend, model=default_model)
    }
    print(f"[photo-daemon] default backend={default_backend} "
          f"model={getattr(providers[default_backend], 'model', '?')}",
          flush=True)

    if os.path.exists(socket_path):
        os.unlink(socket_path)
    srv = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
    srv.bind(socket_path)
    os.chmod(socket_path, 0o600)
    srv.listen(4)
    print(f"[photo-daemon] listening on {socket_path}", flush=True)

    try:
        while True:
            conn, _ = srv.accept()
            try:
                buf = b""
                while not buf.endswith(b"\n"):
                    chunk = conn.recv(4096)
                    if not chunk:
                        break
                    buf += chunk
                    if len(buf) > 1024 * 64:
                        break
                if not buf:
                    conn.close()
                    continue
                req = json.loads(buf.decode("utf-8"))
                backend = req.get("backend") or default_backend
                if backend not in providers:
                    providers[backend] = make_provider(backend)
                provider = providers[backend]
                inp = req["in"]
                out = req["out"]
                prompt = req.get("prompt") or DEFAULT_PROMPT
                try:
                    ms = provider.repaint(
                        inp, out,
                        prompt=prompt,
                        model=req.get("model"),
                        seed=req.get("seed", 42),
                        size=req.get("size"),
                        side=req.get("side", 1024),
                        guidance_scale=req.get("guidance_scale"),
                        target_long=req.get("target_long", 1024),
                    )
                    rep = {"ok": True, "out": out, "ms": ms,
                           "backend": backend}
                    print(f"[photo-daemon] {backend} "
                          f"{os.path.basename(out)} {ms}ms", flush=True)
                except Exception as e:
                    traceback.print_exc()
                    rep = {"ok": False, "error": f"{type(e).__name__}: {e}",
                           "backend": backend}
                conn.sendall((json.dumps(rep) + "\n").encode("utf-8"))
            finally:
                try:
                    conn.close()
                except Exception:
                    pass
    finally:
        try:
            os.unlink(socket_path)
        except Exception:
            pass


# ---------------------------------------------------------------------------
# CLI entry
# ---------------------------------------------------------------------------

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--in", dest="inp")
    ap.add_argument("--out", dest="out")
    ap.add_argument("--prompt", default=DEFAULT_PROMPT)
    ap.add_argument("--backend",
                    default=os.environ.get("FCEMU_PHOTO_BACKEND", "ark"),
                    choices=sorted(PROVIDERS.keys()))
    ap.add_argument("--model", default=None,
                    help="Override the provider's default model ID "
                         "(e.g. doubao-seedream-4-5-xxxxxx). Falls back to "
                         "ARK_IMAGE_MODEL / OPENAI_IMAGE_MODEL env, then to "
                         "the provider's hardcoded default.")
    ap.add_argument("--size", default=None,
                    help='Output size like "1024x1024" or "2K". Provider-specific.')
    ap.add_argument("--seed", type=int, default=42)
    ap.add_argument("--guidance", type=float, default=None,
                    help="guidance_scale (SeedEdit 3.x only; omit by default)")
    ap.add_argument("--daemon", action="store_true",
                    help="run as unix-socket daemon")
    ap.add_argument("--socket", default="/tmp/fcemu_photo.sock")
    args = ap.parse_args()

    if args.daemon:
        serve(args.socket, args.backend, default_model=args.model)
        return

    if not args.inp or not args.out:
        ap.error("--in and --out are required when not in --daemon mode")

    provider = make_provider(args.backend, model=args.model)
    ms = provider.repaint(
        args.inp, args.out,
        prompt=args.prompt,
        seed=args.seed,
        size=args.size,
        guidance_scale=args.guidance,
    )
    print(f"[photo] {args.backend} {ms}ms -> {args.out}", flush=True)


if __name__ == "__main__":
    main()
