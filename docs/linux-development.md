# Linux Development

This is the setup checklist for developing and testing Flux on Linux, with the Vulkan and KMS paths enabled.

## Target

- Distro: Arch Linux.
- Kernel: use a current Arch kernel; KMS work assumes modern DRM APIs and working modesetting.
- GPU stack: Mesa for AMD/Intel, or the proprietary NVIDIA stack if that is the target hardware.
- Vulkan: Vulkan 1.3 is required. Flux also requires `dynamicRendering` and `synchronization2`.

## Packages

Base build tools:

```sh
sudo pacman -S --needed base-devel cmake ninja pkgconf git clang
```

Graphics and text dependencies:

```sh
sudo pacman -S --needed \
  wayland wayland-protocols libxkbcommon \
  vulkan-headers vulkan-icd-loader vulkan-tools glslang \
  mesa libdrm libinput systemd-libs \
  freetype2 fontconfig harfbuzz zlib libwebp
```

Driver packages:

```sh
# AMD
sudo pacman -S --needed vulkan-radeon mesa-utils

# Intel
sudo pacman -S --needed vulkan-intel mesa-utils

# NVIDIA
sudo pacman -S --needed nvidia nvidia-utils
```

Verify Vulkan before building Flux:

```sh
vulkaninfo --summary
```

The selected physical device must report Vulkan 1.3. If Flux rejects the device, the error should now say whether the problem is API version, `dynamicRendering`, `synchronization2`, a missing extension, or queue/present support.

## Configure

Wayland build:

```sh
cmake -S . -B build-linux-wayland -G Ninja \
  -DFLUX_PLATFORM=LINUX_WAYLAND \
  -DFLUX_BUILD_TESTS=ON \
  -DFLUX_BUILD_EXAMPLES=ON
cmake --build build-linux-wayland
ctest --test-dir build-linux-wayland --output-on-failure
```

KMS build:

```sh
cmake -S . -B build-linux-kms -G Ninja \
  -DFLUX_PLATFORM=LINUX_KMS \
  -DFLUX_BUILD_TESTS=ON \
  -DFLUX_BUILD_EXAMPLES=OFF
cmake --build build-linux-kms
```

## KMS Smoke

KMS needs DRM master. Run from a real TTY, not inside a desktop session that already owns DRM master. If needed, switch to a spare VT with `Ctrl+Alt+F3`, log in, then run:

```sh
./build-linux-kms/kms-vulkan-smoke
```

Expected result: the selected output presents a solid blue frame briefly and the program exits. If this fails, fix the Linux graphics stack before debugging higher-level rendering.

Useful checks:

```sh
ls -l /dev/dri
loginctl session-status
vulkaninfo --summary
```

For KMS debugging:

```sh
FLUX_DEBUG_KMS=1 ./build-linux-kms/kms-vulkan-smoke
```

## Common Failures

- `Missing required Vulkan instance extension: VK_KHR_display`: the ICD or driver does not expose direct display support.
- `Vulkan 1.3 required`: update Mesa/driver or select a different GPU.
- `missing Vulkan 1.3 feature(s): dynamicRendering, synchronization2`: the device or driver cannot run the Flux Vulkan backend.
- `no graphics queue family can present to this surface`: the selected Vulkan device cannot present to the KMS/Wayland surface.
- `drmModeGetResources failed` or no connectors: run from a real TTY, check permissions, and confirm another compositor is not holding DRM master.
