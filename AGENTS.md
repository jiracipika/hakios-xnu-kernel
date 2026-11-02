# AGENTS.md — hakios-pixel7

Standing instructions for AI coding agents (zcode) working on this repository — written for the
user's main PC, where the Pixel 7 gets plugged in. Read fully before doing anything.
(Committed on the fork's `pixel7` branch because the server workspace's Mimosa hook currently
blocks parent-repo commits; the parent copy is identical on disk there.)

## What this repo is

Port of [HakiOS](https://github.com/oosexgang/hakios-xnu-kernel) (Apple's open XNU on Android
phones) to the **Google Pixel 7 (`panther`, Tensor G2 / `gs201`)**. The upstream codebases are
submodules under `forks/` pointing at our forks. Everything you need to understand the port is in
`docs/PORT-PLAN.md` (conversion surface, sequencing, risks — read it first), `docs/UPSTREAM-SYNC.md`
(branch policy), and `notes/sync-log.md`.

Non-obvious upstream facts (verified, do not re-derive): hakios-metal is Adreno-only (Pixel 7 is
Mali-G710); `hakios-mt6768` (Mali-G52 + MediaTek chainloader) is the working precedent for our boot
path; Project Silicium UEFI is Qualcomm-only, so we use a boot.img chainloader via `fastboot boot`.

## One-time machine setup (Linux, no root assumed)

```sh
sudo apt install -y git adb fastboot python3 curl unzip dpkg-dev   # adb/fastboot: or Google platform-tools zip
git clone https://github.com/jiracipika/hakios-pixel7 && cd hakios-pixel7
git submodule update --init --recursive
```

The kernel needs **theos with the iPhoneOS16.5 SDK** (not installed by our script):

```sh
git clone --recursive https://github.com/theos/theos.git ~/theos
# put iPhoneOS16.5.sdk into ~/theos/sdks/ (sources in theos/sdks/ README; upstream docs:
# forks/hakios-xnu-kernel/docs/getting-started/building.md)
```

`lld-link` needs [micromamba](https://mamba.readthedocs.io) (script expects `~/toolchain-dl/bin/micromamba`,
override with `MAMBA=`). Then everything else is automated — and ends with a live QEMU boot:

```sh
bash scripts/host-bootstrap.sh          # long; safe to re-run (idempotent, caches in ~/.local)
```

**Success signature of the QEMU gate**: `logs/qemu-open.log` shows `hakios-kernel: 25 drivers built
in`, `load_init_program: attempting to load /sbin/launchd`, then a panic `initproc failed to start`
+ stackshot. That panic is EXPECTED — the open ramdisk has no launchd. It proves the whole chain.

## Standing safety rules (non-negotiable)

1. **`fastboot flashing unlock` wipes the entire phone.** Never run it, or suggest running it,
   unless the user asks in the current conversation. Restate the wipe consequence before they decide.
2. **The default workflow flashes nothing**: `fastboot boot <image>` = RAM session, reboot returns
   to Android. Never flash any partition without an explicit instruction in the active conversation.
3. Never commit to any fork's `main` branch — forks mirror upstream (ff-only). Work goes on
   `pixel7` branches. `scripts/sync-upstream.sh` is the only thing that fast-forwards `main`.
4. Stop on HTTP 429. One heavy network operation at a time.
5. NEVER touch, list, or include the `automated-newsletter` project in any work on this repo.
6. Every commit message stays honest; every `device.conf` status line says `works` only for what
   was observed on the phone. Label findings verified / guessed / inferred.

## When the Pixel 7 is plugged in — run this exact order

0. **Verify it's the phone**: `adb devices` must list a non-emulator device whose
   `ro.product.device` is `panther` — `emulator-5554` is the SDK emulator, NOT the phone.
1. **Facts first** (all read-only):
   - `adb shell getprop` — save the full dump; note `ro.build.fingerprint`, `ro.boot.slot_suffix`,
     bootloader state.
   - `bash scripts/pull-panther-fdt.sh` — pulls the device tree; the single most valuable artifact.
     Then feed it to the scaffold: in `forks/hakios-xnu-kernel`, re-run `tools/new-device.py`
     against `devices/panther` with `--fdt` to fill the gs201 `soc.args`.
2. **Backup before anything else** (`bash scripts/backup-pixel7.sh ~/pixel7-backup`):
   unrooted scope is user data only — partitions are recovered from Google's factory image, so
   download the one matching the fingerprint (developers.google.com/android/images, panther) and
   keep it beside the backup. A true partition-image backup requires root; out of scope for a
   stock phone.
3. **Unlock decision**: user's alone, in-conversation, with the wipe warning restated (rule 1).
4. **Install**: build the panther boot image (PORT-PLAN §2.3 option B — chainloader in boot.img),
   then `fastboot boot out/panther.img`. Session in RAM; `adb reboot` returns Android anytime.
5. **Record everything**: run logs into the device repo's `notes/`, status into `device.conf`,
   then commit on the `pixel7` branch and push.

## Repo etiquette

- Parent repo (`hakios-pixel7`): docs, scripts, submodule pins. Forks: code. The 5-day sync runs
  on the server (workspace automation); on this PC run `scripts/sync-upstream.sh` manually before
  starting any port session.
- The server workspace has a Mimosa security hook that can block parent-repo commits when its
  scanner chokes on the untracked upstream build trees (its own ledger says coverage is partial;
  flagged files are Apple's tools, not our diffs). If a commit of clearly-authored files is
  blocked there, do not "fix" upstream code to satisfy the scanner — note the block and commit
  on the fork's `pixel7` branch instead, as was done for this file.
- Scripts here download from an https allowlist only (gnu.org, github.com, archive.ubuntu.com,
  mirrors.edge.kernel.org) — keep it that way when editing.
