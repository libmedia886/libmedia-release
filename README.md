# libmedia release

This repository is the binary release mirror for `libmedia`.

## Contents

- `lib/libmedia.so`: core library with the embedded Ed25519 public key.
- `plugins/*.so`: external modules.
- `plugins/*.so.sig`: detached signatures matched to each plugin byte-for-byte.
- `include/`: public application and plugin headers.
- `examples/minimal/`: load one signed plugin.
- `examples/load_all/`: scan and load all signed plugins in the release directory.

No private key or PEM file is included in this release repository. The core library already contains the verification public key.

## Build the minimal example

```sh
cmake -S examples/minimal -B build/minimal
cmake --build build/minimal
(cd build/minimal && ./libmedia_minimal)
```

The plugin `.so` and its adjacent `.so.sig` must both be present. If either is missing, or if the plugin has been changed, `MEDIA_PLUGIN_Load()` rejects it.

## Build the load-all example

```sh
cmake -S examples/load_all -B build/load_all
cmake --build build/load_all
(cd build/load_all && ./libmedia_load_all)
```

The signing key is maintained separately. New releases must be signed before publication; do not modify or remove the `.sig` files.
