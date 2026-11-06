# libmedia release

**English** | [简体中文](README.zh-CN.md)

Binary release mirror for `libmedia`.

## Contents

- `lib/libmedia.so`: core library with the embedded Ed25519 public key.
- `plugins/*.so`: external modules.
- `plugins/*.so.sig`: detached signatures matched to each plugin byte-for-byte.
- `include/`: public application and plugin headers.
- `examples/minimal/`: load one signed plugin.
- `examples/load_all/`: scan and load all signed plugins in the release directory.

No private key or PEM file is included in this release repository. The core library already contains the verification public key.

## Signature verification

Every plugin is loaded through `MEDIA_PLUGIN_LoadSelf()`, which verifies the plugin image against its adjacent `.sig` file before mapping it. A plugin is rejected when:

- the `.sig` file is missing;
- the plugin has been modified in any byte;
- the signature was produced with a different key.

The `.sig` files are produced with the release key and must be shipped alongside the plugins. Do not modify or remove them.

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

This example loads every plugin found in `../../plugins` and prints the number of plugins accepted. Note that a single process holds at most `MEDIA_PLUGIN_MAX_LOADED` (32) plugins at a time; plugins beyond that limit are reported as `too many plugins loaded`. An application should load only the modules it needs.

The signing key is maintained separately. New releases must be signed before publication.
