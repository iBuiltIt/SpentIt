# Application artwork

The light artwork was regenerated and geometrically cleaned on 2026-11-04 with owner approval. Its white tile occupies 824 × 824 pixels on the 1024 px canvas, with equal 100 px transparent margins and tangent rounded corners. The coin stacks and black/red wordmark retain the original design. The dark artwork retains the owner's original artwork and the 2026-10-28 alpha-only correction.

The retained intermediate light artwork is `candidates/SpentIt_light_v2.png`; it is not a ready-to-use icon. Its generated outer fringe is excluded by the interior crop and geometric mask in `tools/refine_light_icon.swift`. The script composites onto opaque white inside the mask and renders at 4× resolution before producing all seven PNG sizes. To reproduce on macOS, from the repository root:

```sh
swift tools/refine_light_icon.swift
```

Image generation used the built-in imagegen tool, with the original light master as a reference: preserve three descending gold coin stacks and exact black/red “SpentIt”, rebuild the white tile with smooth corners and transparent margins. A second generation requested removal of the ragged fringe; precise deterministic masking was still needed. This replaces the light variant's earlier alpha-only maintenance approach.

Keep alpha when exporting. Do not flatten against white or remove the white interior of the light tile. Inspect exports against both light and dark backgrounds.

After updating PNG sizes, rebuild the platform containers from the repository root:

```sh
python3 tools/generate_macos_icons.py
python3 tools/generate_windows_icon.py
```

Both scripts wrap the existing PNG data without resampling. ICNS contains the same standard/Retina representations as the originally supplied files. ICO uses the light 16, 32, 64, 128 and 256 px images. Installed application icons use light; the welcome/runtime artwork follows system appearance. See [local packaging](../../packaging/README.md).
