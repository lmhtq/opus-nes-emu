#!/usr/bin/env bash
# fcemu 一键引导脚本 — 检查 / 安装依赖 / 构建 / 设置 Photo Mode / 运行示例
#
# 用法:
#   ./bootstrap.sh                # 默认 = check + build (不自动装系统依赖)
#   ./bootstrap.sh check          # 仅检查依赖, 报告缺失
#   ./bootstrap.sh deps           # 尝试安装缺失的系统依赖 (需要 brew / apt)
#   ./bootstrap.sh build          # 用 CMake 构建 + 跑 ctest
#   ./bootstrap.sh photo          # 给 Photo Mode 装 Python venv
#   ./bootstrap.sh test           # 重跑测试
#   ./bootstrap.sh clean          # rm -rf build
#   ./bootstrap.sh all            # check + deps + build + photo
#   ./bootstrap.sh help           # 打印这段说明
#
# 任何时候都安全重入: 已经装好的步骤会被跳过。

set -u
set -o pipefail

# ---------------------------------------------------------------------------
# Config / paths
# ---------------------------------------------------------------------------
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${ROOT_DIR}/build"
PHOTO_DIR="${ROOT_DIR}/tools/photo"
JOBS="$(getconf _NPROCESSORS_ONLN 2>/dev/null || echo 4)"

# ---------------------------------------------------------------------------
# Output helpers (ASCII labels, no emoji)
# ---------------------------------------------------------------------------
if [ -t 1 ]; then
    C_RESET='\033[0m'; C_DIM='\033[2m'; C_RED='\033[31m'
    C_GREEN='\033[32m'; C_YELLOW='\033[33m'; C_CYAN='\033[36m'
else
    C_RESET=''; C_DIM=''; C_RED=''; C_GREEN=''; C_YELLOW=''; C_CYAN=''
fi
say()  { printf '%b\n' "$*"; }
ok()   { say "${C_GREEN}[ ok ]${C_RESET} $*"; }
warn() { say "${C_YELLOW}[warn]${C_RESET} $*"; }
miss() { say "${C_RED}[miss]${C_RESET} $*"; }
info() { say "${C_CYAN}[info]${C_RESET} $*"; }
step() { say ""; say "${C_CYAN}== $* ==${C_RESET}"; }

# ---------------------------------------------------------------------------
# Platform detection
# ---------------------------------------------------------------------------
OS="$(uname -s)"
case "$OS" in
    Darwin) PLATFORM="macos" ;;
    Linux)
        if command -v apt-get >/dev/null 2>&1;     then PLATFORM="linux-apt"
        elif command -v dnf >/dev/null 2>&1;       then PLATFORM="linux-dnf"
        elif command -v pacman >/dev/null 2>&1;    then PLATFORM="linux-pacman"
        else PLATFORM="linux-unknown"; fi ;;
    *) PLATFORM="$OS-unknown" ;;
esac

# ---------------------------------------------------------------------------
# Dependency checks. Each populates MISSING / MISSING_OPT arrays.
# ---------------------------------------------------------------------------
MISSING=()
MISSING_OPT=()

check_cmd() {
    local cmd="$1" label="$2" optional="${3:-no}"
    if command -v "$cmd" >/dev/null 2>&1; then
        local ver
        ver="$("$cmd" --version 2>&1 | head -n1 || true)"
        ok "${label} (${cmd}) — ${ver:-found}"
        return 0
    fi
    if [ "$optional" = "yes" ]; then
        warn "${label} (${cmd}) — not found (optional)"
        MISSING_OPT+=("$cmd")
    else
        miss "${label} (${cmd}) — not found"
        MISSING+=("$cmd")
    fi
    return 1
}

check_sdl2() {
    if command -v pkg-config >/dev/null 2>&1 && pkg-config --exists sdl2; then
        ok "SDL2 — $(pkg-config --modversion sdl2)"
        return 0
    fi
    if command -v sdl2-config >/dev/null 2>&1; then
        ok "SDL2 — $(sdl2-config --version)"
        return 0
    fi
    if [ "$PLATFORM" = "macos" ] && [ -d /opt/homebrew/Cellar/sdl2 ]; then
        ok "SDL2 — installed via Homebrew (no pkg-config in PATH)"
        return 0
    fi
    miss "SDL2 — not found (CMake's find_package(SDL2) will fail)"
    MISSING+=("sdl2")
}

check_opengl() {
    case "$PLATFORM" in
        macos)
            ok "OpenGL — system framework (always available on macOS)"
            ;;
        linux-*)
            if command -v pkg-config >/dev/null 2>&1 && pkg-config --exists gl; then
                ok "OpenGL — libGL $(pkg-config --modversion gl)"
            else
                miss "OpenGL — libGL dev headers not found"
                MISSING+=("opengl")
            fi
            ;;
        *)
            warn "OpenGL — unknown platform, assuming available"
            ;;
    esac
}

check_ncnn() {
    if command -v pkg-config >/dev/null 2>&1 && pkg-config --exists ncnn 2>/dev/null; then
        ok "ncnn — $(pkg-config --modversion ncnn) (in-process AI upscaler enabled)"
        return 0
    fi
    if [ -f /opt/homebrew/include/ncnn/net.h ] || [ -f /usr/local/include/ncnn/net.h ]; then
        ok "ncnn — found (Homebrew, in-process AI upscaler enabled)"
        return 0
    fi
    warn "ncnn — not found (optional; without it, only CoreML / subprocess backends)"
    MISSING_OPT+=("ncnn")
}

check_libomp_mac() {
    [ "$PLATFORM" = "macos" ] || return 0
    if [ -f /opt/homebrew/opt/libomp/include/omp.h ] || \
       [ -f /usr/local/opt/libomp/include/omp.h ]; then
        ok "libomp — found (required by ncnn on macOS)"
    else
        warn "libomp — not found (only matters if you want ncnn)"
        MISSING_OPT+=("libomp")
    fi
}

check_python_for_photo() {
    if ! command -v python3 >/dev/null 2>&1; then
        warn "python3 — not found (only needed for Photo Mode)"
        MISSING_OPT+=("python3")
        return 1
    fi
    local ver
    ver="$(python3 -c 'import sys; print("%d.%d" % sys.version_info[:2])')"
    if ! python3 -m venv --help >/dev/null 2>&1; then
        warn "python3-venv — module missing (apt: install python3-venv)"
        MISSING_OPT+=("python3-venv")
        return 1
    fi
    ok "python3 ${ver} — venv available"
}

check_rom_dir() {
    if [ -d "${ROOT_DIR}/roms" ] && ls "${ROOT_DIR}/roms"/*.nes >/dev/null 2>&1; then
        local n
        n="$(ls "${ROOT_DIR}/roms"/*.nes 2>/dev/null | wc -l | tr -d ' ')"
        ok "roms/ — ${n} .nes file(s) present"
    else
        warn "roms/ — empty (place a .nes file here for the run examples below)"
    fi
}

do_check() {
    step "Detect platform"
    info "OS=${OS}  PLATFORM=${PLATFORM}  JOBS=${JOBS}"

    step "Required tools"
    check_cmd cmake "CMake (>= 3.16)"
    check_cmd make  "make"
    if command -v c++ >/dev/null 2>&1; then
        ok "C++ compiler — $(c++ --version 2>&1 | head -n1)"
    else
        check_cmd clang++ "Clang"
        check_cmd g++     "GCC"
    fi
    check_cmd git "git"
    check_cmd pkg-config "pkg-config" yes

    step "Required libraries"
    check_sdl2
    check_opengl

    step "Optional (AI upscale, REQ-116)"
    check_ncnn
    check_libomp_mac

    step "Optional (Photo Mode, REQ-117)"
    check_python_for_photo
    check_cmd curl "curl" yes

    step "Project state"
    check_rom_dir

    say ""
    if [ ${#MISSING[@]} -eq 0 ]; then
        ok "All required dependencies satisfied."
    else
        miss "Missing required: ${MISSING[*]}"
        info "Run:  ./bootstrap.sh deps    # to attempt auto-install"
        return 1
    fi
    if [ ${#MISSING_OPT[@]} -gt 0 ]; then
        warn "Missing optional: ${MISSING_OPT[*]}"
    fi
}

# ---------------------------------------------------------------------------
# Install deps (best-effort, asks confirmation before running brew/apt)
# ---------------------------------------------------------------------------
confirm() {
    local prompt="$1"
    if [ "${FCEMU_BOOTSTRAP_YES:-}" = "1" ]; then return 0; fi
    printf '%b ' "${C_YELLOW}>>${C_RESET} ${prompt} [y/N]"
    read -r ans
    [[ "$ans" =~ ^[Yy]$ ]]
}

do_deps() {
    do_check || true   # populate MISSING / MISSING_OPT but don't bail

    if [ ${#MISSING[@]} -eq 0 ] && [ ${#MISSING_OPT[@]} -eq 0 ]; then
        ok "Nothing to install."
        return 0
    fi

    case "$PLATFORM" in
        macos)
            if ! command -v brew >/dev/null 2>&1; then
                miss "Homebrew not installed. See https://brew.sh"
                return 1
            fi
            local pkgs=()
            local seen=" ${MISSING[*]} ${MISSING_OPT[*]} "
            [[ $seen == *" cmake "*    ]] && pkgs+=(cmake)
            [[ $seen == *" git "*      ]] && pkgs+=(git)
            [[ $seen == *" sdl2 "*     ]] && pkgs+=(sdl2)
            [[ $seen == *" ncnn "*     ]] && pkgs+=(ncnn)
            [[ $seen == *" libomp "*   ]] && pkgs+=(libomp)
            [[ $seen == *" python3 "*  ]] && pkgs+=(python@3.11)
            [[ $seen == *" pkg-config "* ]] && pkgs+=(pkg-config)
            if [ ${#pkgs[@]} -eq 0 ]; then
                info "No Homebrew-installable items needed."
                return 0
            fi
            say ""
            info "Will run: brew install ${pkgs[*]}"
            confirm "Proceed?" || { warn "Skipped."; return 0; }
            brew install "${pkgs[@]}"
            ;;
        linux-apt)
            local pkgs=()
            local seen=" ${MISSING[*]} ${MISSING_OPT[*]} "
            [[ $seen == *" cmake "*       ]] && pkgs+=(cmake)
            [[ $seen == *" make "*        ]] && pkgs+=(build-essential)
            [[ $seen == *" git "*         ]] && pkgs+=(git)
            [[ $seen == *" sdl2 "*        ]] && pkgs+=(libsdl2-dev)
            [[ $seen == *" opengl "*      ]] && pkgs+=(libgl1-mesa-dev)
            [[ $seen == *" pkg-config "*  ]] && pkgs+=(pkg-config)
            [[ $seen == *" python3 "*     ]] && pkgs+=(python3 python3-venv)
            [[ $seen == *" python3-venv "* ]] && pkgs+=(python3-venv)
            [[ $seen == *" curl "*        ]] && pkgs+=(curl)
            if [ ${#pkgs[@]} -eq 0 ]; then
                info "No apt-installable items needed."
                return 0
            fi
            say ""
            info "Will run: sudo apt-get update && sudo apt-get install -y ${pkgs[*]}"
            confirm "Proceed?" || { warn "Skipped."; return 0; }
            sudo apt-get update
            sudo apt-get install -y "${pkgs[@]}"
            ;;
        *)
            warn "Auto-install not supported on ${PLATFORM}."
            info  "Install these manually: ${MISSING[*]} ${MISSING_OPT[*]}"
            return 1
            ;;
    esac
}

# ---------------------------------------------------------------------------
# Build
# ---------------------------------------------------------------------------
do_build() {
    step "Configure & build (cmake -S . -B build)"
    cmake -S "${ROOT_DIR}" -B "${BUILD_DIR}"
    cmake --build "${BUILD_DIR}" -j "${JOBS}"
    if [ -x "${BUILD_DIR}/fcemu" ]; then
        ok "Built: ${BUILD_DIR}/fcemu"
    else
        miss "fcemu binary not produced — check the cmake/make output above."
        return 1
    fi
}

do_test() {
    if [ ! -d "${BUILD_DIR}" ]; then
        warn "No build/ — running build first."
        do_build
    fi
    step "ctest"
    ( cd "${BUILD_DIR}" && ctest --output-on-failure )
}

do_clean() {
    step "Clean"
    rm -rf "${BUILD_DIR}"
    ok "Removed ${BUILD_DIR}"
}

# ---------------------------------------------------------------------------
# Photo Mode: set up the slim venv (requests + Pillow). No model downloads.
# ---------------------------------------------------------------------------
do_photo() {
    step "Photo Mode venv (tools/photo/.venv)"
    if ! command -v python3 >/dev/null 2>&1; then
        miss "python3 not available — run ./bootstrap.sh deps first."
        return 1
    fi
    if ! python3 -m venv --help >/dev/null 2>&1; then
        miss "python3-venv missing — apt install python3-venv (Debian/Ubuntu)."
        return 1
    fi
    if [ ! -d "${PHOTO_DIR}/.venv" ]; then
        python3 -m venv "${PHOTO_DIR}/.venv"
        ok "Created ${PHOTO_DIR}/.venv"
    else
        info "${PHOTO_DIR}/.venv already exists"
    fi
    "${PHOTO_DIR}/.venv/bin/pip" install -q -U pip
    "${PHOTO_DIR}/.venv/bin/pip" install -q -r "${PHOTO_DIR}/requirements.txt"
    ok "Installed: $("${PHOTO_DIR}/.venv/bin/pip" freeze | tr '\n' ' ')"

    say ""
    info "API key check:"
    if [ -n "${ARK_API_KEY:-}" ]; then
        ok "ARK_API_KEY is set (豆包 SeedEdit ready)"
    else
        warn "ARK_API_KEY not set — export it to use Ark backend"
    fi
    if [ -n "${OPENAI_API_KEY:-}" ]; then
        ok "OPENAI_API_KEY is set (OpenAI gpt-image-1 ready)"
    else
        warn "OPENAI_API_KEY not set — export it to use OpenAI backend"
    fi
    if [ -z "${ARK_API_KEY:-}" ] && [ -z "${OPENAI_API_KEY:-}" ]; then
        warn "No image-API key configured yet. Photo Mode daemon won't start."
    fi
}

# ---------------------------------------------------------------------------
# Help / usage examples
# ---------------------------------------------------------------------------
do_examples() {
    cat <<EOF

${C_CYAN}== Usage examples ==${C_RESET}

  ${C_DIM}# Plain run${C_RESET}
  ./build/fcemu roms/your-game.nes

  ${C_DIM}# AI 实时超分 (REQ-116). 三档可选: fast / medium / quality${C_RESET}
  ./build/fcemu --ai-mode=fast    roms/your-game.nes
  ./build/fcemu --ai-mode=quality roms/your-game.nes
  ./build/fcemu --ai-upscale=realesrgan-x4plus roms/your-game.nes

  ${C_DIM}# Headless 冒烟测试 (无 SDL 窗口)${C_RESET}
  ./build/fcemu_headless roms/your-game.nes

  ${C_DIM}# Photo Mode daemon (REQ-117), 一次启动常驻${C_RESET}
  export ARK_API_KEY=your-key
  ${C_DIM}# 可选: 默认 doubao-seedream-4-0-250828, 升级到 5.0 加这一行${C_RESET}
  export ARK_IMAGE_MODEL=doubao-seedream-5-0-260128
  nohup tools/photo/.venv/bin/python tools/photo/photo_repaint.py \\
      --daemon --backend=ark > ~/Desktop/fcemu-photo/daemon.log 2>&1 &
  ${C_DIM}# 然后游戏中按 P，照片落到 ~/Desktop/fcemu-photo/${C_RESET}

  ${C_DIM}# 命令行单帧调试 Photo Mode (单帧也能用 --model 临时换)${C_RESET}
  tools/photo/.venv/bin/python tools/photo/photo_repaint.py \\
      --backend=ark --model=doubao-seedream-5-0-260128 \\
      --in /tmp/frame.png --out /tmp/out.png \\
      --prompt "photorealistic 1980s Mario, 35mm film"

  ${C_DIM}# 跑测试${C_RESET}
  ./bootstrap.sh test
EOF
}

do_help() {
    # Print only the leading comment block (everything up to the first blank line).
    awk 'NR==1{next} /^$/{exit} {sub(/^# ?/,""); print}' "$0"
}

# ---------------------------------------------------------------------------
# Dispatcher
# ---------------------------------------------------------------------------
cmd="${1:-default}"
case "$cmd" in
    check)         do_check ;;
    deps)          do_deps ;;
    build)         do_check && do_build && do_examples ;;
    test)          do_test ;;
    clean)         do_clean ;;
    photo)         do_photo ;;
    all)           do_check; do_deps; do_build && do_photo; do_examples ;;
    default|"")    do_check && do_build && do_examples ;;
    help|-h|--help) do_help ;;
    *) miss "Unknown command: $cmd"; do_help; exit 2 ;;
esac
