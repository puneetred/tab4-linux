# 09 — Audio (88pm805 PMIC codec, Class-D speaker)

## Why it was silent even though "playback worked"

The `88pm805` codec driver is a **bare register dump**: no DAPM, no init
sequence. `speaker-test` runs without error (the I2S/SSP digital path clocks),
but the **entire analog output path is powered down** at boot:

```
0x01 MAIN_POWERUP      = 0x00
0x50 DIGITAL_BLOCK_EN_1= 0x00
0x51 DIGITAL_BLOCK_EN_2= 0x00
0x80 ANALOG_BLOCK_EN   = 0x00
0x89 POWER_AMP_ENABLE  = 0x00
```

In Android these registers were poked by the proprietary audio HAL using values
from a binary (`audio_path_config.xml`, not AXML — Marvell's own format).

## The fix: postmarketOS UCM2 values

postmarketOS ships a **UCM2** config for this exact card
(`pxa-88pm805-dkb-hifi`) — that's the authoritative register set. We extracted
its `HiFi` EnableSequence (+ Speaker device) and turned it into
`rootfs-overlay/etc/local.d/t4sound.start` (94 `amixer cset` lines), run at boot
by the OpenRC `local` service.

Key registers: `MAIN_POWERUP=3`, `DIGITAL_BLOCK_EN_1/2=31/21`,
`ANALOG_BLOCK_EN=45`, `CHARGE_PUMP=136/22/22`, and the **Class-D speaker amp**
`PM822_CLASS_D_1=243, PM822_MIS_CLASS_D_1=194, PM822_MIS_CLASS_D_2=112`.

## Ground truth: read the codec over regmap

```sh
cat /sys/kernel/debug/regmap/2-0038/registers    # 88PM805 codec page (I2C 0x38)
```
The Class-D amp regs (0x48/0x61/0x62) live on the **PMIC page** (regmap `2-0030`).

## Volume

The DAC output level is `PM805_CODEC_VOL_SEL_CHANNEL_3` (0–255). **This codec's
readback is unreliable** (reads can return 0 after a successful write), so
`rootfs-overlay/usr/local/bin/t4vol` tracks the level in `/run/t4vol` and drives
that control. Bound to the volume keys (docs/10).

**Do not** crank `VOL_SEL_CHANNEL_3` or `HEADPHONE_GAIN_A2A` to 255 — it
over-drives the amp into clipping (sounds "broken/noisy"). Reference level ≈183.

## Test

```sh
speaker-test -D plughw:0,0 -c 2 -t sine -f 440 -l 1
aplay -D plughw:0,0 file.wav
```

## Not done / notes

* Mic, headset detect, and earpiece paths are untested (speaker only).
* The card has several PCM endpoints (ssp/gssp pcm/nb/wb/ph) — `plughw:0,0` is
  the HiFi one.
