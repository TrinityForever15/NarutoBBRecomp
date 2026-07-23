# Phase 4 — local data extraction and first extended boot

Date: 2026-07-19.

## Local data preparation

`xdvdfs_extract_all.py` replaced the root-only extractor with a recursive,
resumable, and idempotent XDVDFS extractor. Against the user's own disc image,
it reproduced all 216 entries and verified their sizes. None of those files is
part of the public repository.

The script:

- walks subdirectories;
- detects common XGD partition bases;
- resumes partially copied large files;
- skips complete files on later runs;
- supports `--list` inventory mode.

```bash
python3 xdvdfs_extract_all.py <owned-disc-image> <local-destination>
python3 xdvdfs_extract_all.py <owned-disc-image> <local-destination> --list
```

## Boot with the local data directory

The `HostPathDevice` mount was correct; a `DiscImageDevice` integration was not
needed. The guest progressed beyond interrupt registration and made its first
VFS probe. The malformed-looking `ShaderDumpxe:\CompareBackEnds` path belonged
to an optional engine debug device and was not a missing game asset.

The temporary missing-function collector reported no new targets at that exact
point. The remaining hang therefore required runtime diagnosis rather than more
bulk asset work or manifest entries.

## Initial (later rejected) hypothesis

Because the GPU-vsync thread remained alive while the guest stopped progressing,
the first handoff suspected a missing vblank callback. The next report proved
that hypothesis false: the callback was delivered around 60 Hz. The true causes
were a lost wakeup during suspended thread creation and an embedded debug shader
compiler returning `E_FAIL`.

See `PHASE4_ROOT_CAUSE_REPORT.md`.
