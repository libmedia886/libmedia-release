# How this release was built

This file records the exact build inputs of the binaries in `lib/` and `plugins/`, so a
rebuild produces the same artefact. It is not needed to *use* the release.

## Source

| Part | Repository | Commit |
| --- | --- | --- |
| core (`lib/libmedia.so`) | `rktohi` | `82efce0` |
| 37 plugins | one repository per plugin | each repository's `master` at merge time (2026-11-14) |

Version string baked into the core is `0.9.0`.

## Configure flags — all of these matter

```sh
cmake -S rktohi -B build \
      -DBUILD_TESTING=OFF \
      -DMEDIA_TEST_SIGNING=OFF \
      -DMEDIA_PLUGIN_PUBLIC_KEY_RAW_FILE=/path/to/public-key.raw \
      -DCMAKE_SKIP_RPATH=ON \
      -DCMAKE_SKIP_BUILD_RPATH=ON \
      -DCMAKE_BUILD_WITH_INSTALL_RPATH=OFF
```

* `BUILD_TESTING=OFF` and `MEDIA_TEST_SIGNING=OFF` are what keep the version string at
  `0.9.0`. With testing on it becomes `0.9.0+testkey`.
* `MEDIA_PLUGIN_PUBLIC_KEY_RAW_FILE` must point at the 32-byte **raw** Ed25519 public key
  shipped as `lib/public-key.raw`. `MEDIA_TEST_SIGNING=OFF` makes the configure step fail
  if the file is missing or is not exactly 32 bytes.
* The three `RPATH` flags are **not optional.** 14 of the 37 plugin repositories do not set
  `SKIP_BUILD_RPATH TRUE` / `INSTALL_RPATH ""` in their own `CMakeLists.txt`, so a plain
  `cmake` run embeds the build machine's path — `/userdata/.../build/rktohi:` — into their
  `.so`, including a trailing empty element that puts the current working directory on the
  loader search path. With the flags above the released files carry no `RPATH`/`RUNPATH` at
  all. Check before publishing:

  ```sh
  for f in lib/*.so plugins/*.so; do readelf -d "$f" | grep -Eq 'RPATH|RUNPATH' && echo "LEAK $f"; done
  ```

  (Fixing this in the 14 repositories instead would make the flags unnecessary; that change
  has not been made as of this release.)

## Post-build steps

1. **Strip.** Each plugin target strips itself in its own `CMakeLists.txt`, and the core does
   the same under the `MEDIA_STRIP_ARTIFACTS` option (on by default). A released file must
   have no `.symtab`:

   ```sh
   readelf -SW lib/libmedia.so plugins/*.so | grep -c '\.symtab'   # expect 0
   ```

2. **Sign.** Every released `.so` needs a detached signature next to it with the `.so.sig`
   suffix (note: `libmedia_tpg.so.sig`, not `libmedia_tpg.sig`). The signature is the raw
   64-byte Ed25519 signature over the file, produced by the signer in
   `rktohi/tests/tools/media_test_sign.c`:

   ```sh
   media_test_sign <release-private-key.pem> lib/libmedia.so plugins/*.so
   ```

   `lib/libmedia.so` needs its signature too — `MEDIA_SYS_Init()` refuses to start without
   `lib/libmedia.so.sig`. The private key is not in this repository and must not be.

3. **Verify the result** with the same 32-byte raw key:

   ```sh
   for f in lib/*.so plugins/*.so; do test -f "$f.sig" || echo "MISSING $f.sig"; done
   ```

   A mismatch between the build key and `lib/public-key.raw` shows up at load time as
   `-1013 signature does not match`, not at sign time — check both together.

## Notes

* `include/media_api.h` and `include/media_plugin.h` are the only public headers. Internal
  headers are not part of the release.
* Exported symbols are `MEDIA_*` from the core and from each plugin, plus the single
  per-plugin entry point `media_plugin_entry` that the loader calls.
