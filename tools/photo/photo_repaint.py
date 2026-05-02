#!/usr/bin/env python3
"""SDXL Turbo + (optional) ControlNet Tile photo-repaint of FC frames.

Usage:
  photo_repaint.py --in frame.png --out result.png \
      [--prompt "..."] [--strength 0.6] [--steps 4] [--cn-scale 0.7] [--seed 42]

Loads SDXL Turbo (1-4 steps) on MPS, optionally uses a Tile ControlNet to keep
the original composition. We pre-upscale the 256x240 input with nearest-neighbour
to 1024x1024 (letterbox) before feeding the diffusion pipeline, since the model
is trained at 512+ resolution.
"""
import argparse, os, sys, time, math
from pathlib import Path
from PIL import Image, ImageOps
import torch

DEFAULT_NEG = "blurry, lowres, ugly, deformed, watermark, text, bad anatomy"


def fit_canvas(img: Image.Image, side: int = 1024, fill=(0, 0, 0)) -> Image.Image:
    """Letterbox img into a side x side canvas using nearest-neighbour upscale."""
    w, h = img.size
    s = min(side / w, side / h)
    nw, nh = int(round(w * s)), int(round(h * s))
    img2 = img.resize((nw, nh), Image.NEAREST)
    canvas = Image.new("RGB", (side, side), fill)
    canvas.paste(img2, ((side - nw) // 2, (side - nh) // 2))
    return canvas


def load_pipe(use_controlnet: bool, dtype):
    from diffusers import (
        StableDiffusionXLImg2ImgPipeline,
        StableDiffusionXLControlNetImg2ImgPipeline,
        ControlNetModel,
        AutoencoderKL,
    )

    base = os.environ.get("SDXL_TURBO_DIR", os.path.expanduser("~/sdxl-turbo-local"))
    vae_dir = os.environ.get("SDXL_VAE_FP16_DIR", os.path.expanduser("~/sdxl-extras/vae-fp16-fix"))
    cn_dir_default = os.path.expanduser("~/sdxl-extras/cn-tile")
    load_kw = dict(torch_dtype=dtype)
    if os.path.isdir(base):
        load_kw["variant"] = "fp16"
        load_kw["local_files_only"] = True
    else:
        load_kw["variant"] = "fp16"

    vae = None
    if os.path.isdir(vae_dir):
        print(f"[photo] using fp16-fix VAE: {vae_dir}", flush=True)
        vae = AutoencoderKL.from_pretrained(vae_dir, torch_dtype=dtype)

    if use_controlnet:
        cn_repo = os.environ.get("CN_TILE_REPO", cn_dir_default)
        print(f"[photo] loading ControlNet: {cn_repo}", flush=True)
        cn = ControlNetModel.from_pretrained(cn_repo, torch_dtype=dtype)
        print(f"[photo] loading SDXL Turbo from {base} (img2img + cn)", flush=True)
        if vae is not None:
            load_kw["vae"] = vae
        pipe = StableDiffusionXLControlNetImg2ImgPipeline.from_pretrained(
            base, controlnet=cn, **load_kw,
        )
    else:
        print(f"[photo] loading SDXL Turbo from {base} (img2img)", flush=True)
        if vae is not None:
            load_kw["vae"] = vae
        pipe = StableDiffusionXLImg2ImgPipeline.from_pretrained(
            base, **load_kw,
        )
    pipe.set_progress_bar_config(disable=True)
    return pipe


def render(pipe, use_cn: bool, device: str, *, inp: str, out: str,
           prompt: str, neg: str = DEFAULT_NEG,
           strength: float = 0.55, steps: int = 4, guidance: float = 0.0,
           seed: int = 42, side: int = 1024, cn_scale: float = 0.6) -> float:
    src = Image.open(inp).convert("RGB")
    init = fit_canvas(src, side)
    gen = torch.Generator(device=device if device != "mps" else "cpu").manual_seed(seed)
    kwargs = dict(
        prompt=prompt, negative_prompt=neg, image=init,
        strength=strength,
        num_inference_steps=max(steps, max(2, int(math.ceil(1 / max(strength, 0.01))))),
        guidance_scale=guidance, generator=gen,
    )
    if use_cn:
        kwargs.update(control_image=init, controlnet_conditioning_scale=cn_scale)
    t0 = time.time()
    img = pipe(**kwargs).images[0]
    Path(out).parent.mkdir(parents=True, exist_ok=True)
    img.save(out)
    return time.time() - t0


def serve(socket_path: str, use_cn: bool, device: str, dtype):
    """Run a Unix-socket daemon. Each accepted connection sends one JSON request
    and receives one JSON reply, line-delimited.

    Request : {"in": "path", "out": "path", "prompt": "...", "strength": 0.55,
               "steps": 4, "cn_scale": 0.6, "seed": 42, "side": 1024}
    Reply   : {"ok": true, "out": "path", "ms": 1234}     on success
              {"ok": false, "error": "..."}              on failure
    """
    import json, socket, traceback

    t0 = time.time()
    pipe = load_pipe(use_cn, dtype).to(device)
    print(f"[photo-daemon] pipeline ready in {time.time()-t0:.1f}s "
          f"cn={use_cn} device={device}", flush=True)

    # Warm up (compiles MPS kernels) using a 1x1 black PNG.
    try:
        warm_in = "/tmp/.fcemu_photo_warm.png"
        Image.new("RGB", (16, 16), (0, 0, 0)).save(warm_in)
        warm_out = "/tmp/.fcemu_photo_warm_out.png"
        ms = render(pipe, use_cn, device, inp=warm_in, out=warm_out,
                    prompt="warmup", steps=2, strength=0.3)
        print(f"[photo-daemon] warmup {ms:.2f}s", flush=True)
    except Exception as e:
        print(f"[photo-daemon] warmup failed: {e}", flush=True)

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
                    conn.close(); continue
                req = json.loads(buf.decode("utf-8"))
                inp = req["in"]; out = req["out"]
                prompt = req.get("prompt") or "photorealistic, cinematic, ultra detailed, 35mm film"
                try:
                    ms = render(
                        pipe, use_cn, device, inp=inp, out=out, prompt=prompt,
                        neg=req.get("neg", DEFAULT_NEG),
                        strength=float(req.get("strength", 0.55)),
                        steps=int(req.get("steps", 4)),
                        guidance=float(req.get("guidance", 0.0)),
                        seed=int(req.get("seed", 42)),
                        side=int(req.get("side", 1024)),
                        cn_scale=float(req.get("cn_scale", 0.6)),
                    )
                    rep = {"ok": True, "out": out, "ms": int(ms * 1000)}
                    print(f"[photo-daemon] {os.path.basename(out)} {int(ms*1000)}ms",
                          flush=True)
                except Exception as e:
                    traceback.print_exc()
                    rep = {"ok": False, "error": f"{type(e).__name__}: {e}"}
                conn.sendall((json.dumps(rep) + "\n").encode("utf-8"))
            finally:
                try: conn.close()
                except Exception: pass
    finally:
        try: os.unlink(socket_path)
        except Exception: pass


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--in", dest="inp")
    ap.add_argument("--out", dest="out")
    ap.add_argument("--prompt", default="photorealistic 1980s Mario in the Mushroom Kingdom, cinematic, 35mm film, dramatic lighting")
    ap.add_argument("--neg", default=DEFAULT_NEG)
    ap.add_argument("--strength", type=float, default=0.55)
    ap.add_argument("--steps", type=int, default=4)
    ap.add_argument("--guidance", type=float, default=0.0)
    ap.add_argument("--seed", type=int, default=42)
    ap.add_argument("--side", type=int, default=1024)
    ap.add_argument("--no-cn", action="store_true", help="disable ControlNet Tile")
    ap.add_argument("--cn-scale", type=float, default=0.6)
    ap.add_argument("--device", default="mps")
    ap.add_argument("--daemon", action="store_true", help="run as unix-socket daemon")
    ap.add_argument("--socket", default="/tmp/fcemu_photo.sock")
    args = ap.parse_args()

    device = args.device
    dtype = torch.float16 if device != "cpu" else torch.float32
    use_cn = not args.no_cn

    if args.daemon:
        serve(args.socket, use_cn, device, dtype)
        return

    if not args.inp or not args.out:
        ap.error("--in and --out are required when not in --daemon mode")

    t0 = time.time()
    pipe = load_pipe(use_cn, dtype).to(device)
    print(f"[photo] pipeline ready in {time.time()-t0:.1f}s", flush=True)
    ms = render(pipe, use_cn, device,
                inp=args.inp, out=args.out, prompt=args.prompt, neg=args.neg,
                strength=args.strength, steps=args.steps, guidance=args.guidance,
                seed=args.seed, side=args.side, cn_scale=args.cn_scale)
    print(f"[photo] inference {ms:.2f}s  -> {args.out}", flush=True)


if __name__ == "__main__":
    main()
