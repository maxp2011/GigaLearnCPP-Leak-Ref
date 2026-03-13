# Building LibTorch from Source (RTX 5090 / Blackwell)

Use this when you cannot use the pre-built LibTorch (e.g. disk too small for the 3.5GB zip + ~12GB extract, or you need a custom CUDA arch). The build produces a `libtorch` folder you can point GigaLearn at.

**Warning:** Plan for **~20–30GB free** (clone + build artifacts). Build time is often 1–2 hours. If space is tight, free as much as you can and try; you can also build on another machine and copy the resulting `libtorch` to the server.

---

## 1. Free space: remove existing LibTorch zips and partial installs

On the server (e.g. `/workspace`):

```bash
cd /workspace
rm -f libtorch-cxx11-abi-shared-with-deps-2.5.1+cu124.zip
rm -f libtorch-cxx11-abi-shared-with-deps-2.7.1+cu128.zip
rm -rf libtorch
# Optional: remove the other GigaLearn tree if you only use GigaLearnCPP-Leak-Ref (frees ~8.6G):
# rm -rf /workspace/GigaLearnCPP-Leak
df -h .
```

---

## 2. Prerequisites

- **Disk:** At least **~20GB free** (clone ~2–3GB + build dir). More is safer.
- **CUDA:** Toolkit and driver (e.g. CUDA 12.x) installed; `nvcc --version` works.
- **Build:** `cmake`, `g++`, `python3`, `pip`, `git`:

```bash
sudo apt update
sudo apt install -y build-essential cmake git python3 python3-pip
pip3 install -r requirements.txt   # after cloning pytorch (step 3)
```

---

## 3. Clone PyTorch

Use a release tag so the build is reproducible (example: 2.7.1; change if you need another version):

```bash
cd /workspace
git clone --recursive --depth 1 --branch v2.7.1 https://github.com/pytorch/pytorch
cd pytorch
```

If you hit “no kernel image” with an older tag, try a newer tag or `main` (e.g. `--branch main` and no `--depth 1`).

---

## 4. Install Python build deps (required by setup.py)

```bash
pip3 install -r requirements.txt
# Optional: install ninja for faster builds
pip3 install ninja
```

---

## 5. Build with RTX 5090 (Blackwell) support

Blackwell is compute capability **12.0** (sm_120). Build LibTorch (and optionally the wheel):

```bash
export USE_CUDA=1
export USE_CUDNN=1
export TORCH_CUDA_ARCH_LIST="8.0;9.0;12.0"
# Build libtorch; BUILD_LIBTORCH_WHL=1 makes a libtorch-only wheel in dist/
BUILD_LIBTORCH_WHL=1 python3 setup.py bdist_wheel
```

- Use only `TORCH_CUDA_ARCH_LIST="12.0"` if this machine is 5090-only (slightly faster build).
- The wheel in `dist/` is named like `libtorch-*.whl`. Unzip it and use the `libtorch` folder inside (it has `lib/`, `include/`, `share/cmake/Torch/`).

---

## 6. Unpack LibTorch for GigaLearn

The `.whl` in `dist/` is a zip. Unzip it and copy the LibTorch tree to `/workspace/libtorch`:

```bash
cd /workspace/pytorch/dist
unzip -q libtorch-*.whl -d libtorch_unpacked
# Wheel layout varies; find the dir that has lib/, include/, share/cmake/Torch/
find libtorch_unpacked -type d -name "Torch" | head -1
# Then copy the parent of that Torch dir (the "libtorch" root) to /workspace:
# e.g. if share/cmake/Torch is at libtorch_unpacked/libtorch/share/cmake/Torch:
cp -r libtorch_unpacked/libtorch /workspace/libtorch
# Or move the root that contains lib/libtorch_cuda.so:
# cp -r /path/to/root_with_lib_include_share /workspace/libtorch
```

---

## 7. Point GigaLearn at your LibTorch

When building GigaLearnCPP-Leak-Ref:

```bash
cd /workspace/GigaLearnCPP-Leak-Ref
rm -rf build && mkdir build && cd build
export CMAKE_PREFIX_PATH="/workspace/libtorch"
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
```

Run with:

```bash
export LD_LIBRARY_PATH="/workspace/libtorch/lib:$LD_LIBRARY_PATH"
./build/GigaLearnBot --gpu
```

---

## 8. If you don’t have enough free space

- Build on another machine or volume with more space, then copy the resulting `libtorch` to the server (e.g. `scp` or `rsync`).
- Or use the **pre-built** cu128 LibTorch (see `README_UBUNTU.md`) on a disk with ~15GB free for the zip + extract.
