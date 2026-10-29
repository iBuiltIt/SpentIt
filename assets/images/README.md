# Application artwork

The owner supplied the light/dark SpentIt artwork. On 2026-10-28 the owner approved making the external corner backgrounds transparent. The 1024 px PNGs are the corrected masters: only alpha changed; their RGB bytes match the original artwork exactly. The opaque light/dark tile backgrounds, coins and lettering remain intact. Smaller PNGs were regenerated with alpha-aware Lanczos resampling; their four extreme corner pixels are explicitly transparent.

Keep alpha when exporting. Do not flatten against white or remove the white interior of the light tile. Inspect exports against both light and dark backgrounds.

After updating PNG sizes, rebuild the platform containers from the repository root:

```sh
python3 tools/generate_macos_icons.py
python3 tools/generate_windows_icon.py
```

Both scripts wrap the existing PNG data without resampling. ICNS contains the same standard/Retina representations as the originally supplied files. ICO uses the light 16, 32, 64, 128 and 256 px images. Installed application icons use light; the welcome/runtime artwork follows system appearance. See [local packaging](../../packaging/README.md).
