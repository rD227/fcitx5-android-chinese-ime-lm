# Sources

- chinese-ime-lm/reference: https://github.com/metasequoiaime/chinese-ime-lm
  commit `b97e99c` (full commit recorded in `vendor/chinese-ime-lm/UPSTREAM_COMMIT`).
  Apache-2.0, with the upstream NOTICE preserved.
- Rime internal headers: https://github.com/rime/librime, tag `1.16.1`,
  commit `de4700e9f6b75b109910613df907965e3cbe0567`, BSD-3-Clause.
  Includes the header changes from fcitx5-android/prebuilder commit
  `eb156443de3b387089e51f9bd19df4e3ddce1732`, `patches/librime.patch`;
  notably the extra `Engine::switcher_` changes internal class offsets.
  The full patch is preserved in `vendor/librime/prebuilder-librime.patch`.
  Pinned arm64 `librime.a` SHA-256:
  `27c157d9080fabf5ab0261d3fdb40e02ba0c177c3db24196f7c5217e6be72db8`.
  Local integration also adds two read-only, nonvirtual accessors to
  `src/rime/algo/syllabifier.h` for the corrector and delimiters. They do not
  change its layout. The patch is preserved in
  `vendor/librime/neural-syllabifier-accessors.patch`.
  `correction-distance.cpp` wraps the exact Android/libc++ symbol for
  `Syllabifier::BuildSyllableGraph` through the linker, calls the pinned original,
  then carries the enhanced corrector's distances into graph credibility.
  Both the CMake symbol and adapter must be reviewed on every ABI/library update.
- `vendor/librime/src/darts.h`: unmodified `include/darts.h` from that same
  pinned Rime commit. Darts-clone, BSD-2-Clause; its notice is preserved in
  `vendor/librime/DARTS_COPYING.md` (https://github.com/s-yata/darts-clone/blob/master/COPYING.md).
- Model: https://huggingface.co/metasequoiaime/pinyin-ime-reranker-4M
  file `sentence-model.safetensors`, 4,486,280 bytes,
  SHA-256 `86ac529510cb3b4968a5e6ade83ec8080f5b34a0a681e75c362accbbd387d1c1`.
  Apache-2.0, retaining embedded attribution and the upstream NOTICE.

The model is downloaded for building, not stored in Git. Original native
integration code and the C ABI bridge, including correction/whole-sentence
expansion code, are MIT-licensed; see LICENSE in this directory. This does not
change the licenses of vendored or upstream-derived files. See the root
NOTICE for repository-wide license scope.
