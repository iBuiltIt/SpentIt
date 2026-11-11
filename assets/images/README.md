# Application artwork

The light artwork was regenerated and geometrically cleaned on 2026-11-04 with owner approval. Its white tile occupies 824 × 824 pixels on the 1024 px canvas, with equal 100 px transparent margins and tangent rounded corners. The coin stacks and black/red wordmark retain the original design. The dark artwork keeps that same coin-stack design on the owner's dark tile. Its white corner rim was removed on 2026-11-10.

The retained intermediate light artwork is `candidates/SpentIt_light_v2.png`; it is not a ready-to-use icon. Its generated outer fringe is excluded by the interior crop and geometric mask in `tools/refine_light_icon.swift`. The script composites onto opaque white inside the mask and renders at 4× resolution before producing all seven PNG sizes. To reproduce on macOS, from the repository root:

```sh
swift tools/refine_light_icon.swift
```

Image generation used the built-in imagegen tool, with the original light master as a reference: preserve three descending gold coin stacks and exact black/red “SpentIt”, rebuild the white tile with smooth corners and transparent margins. A second generation requested removal of the ragged fringe; precise deterministic masking was still needed. This replaces the light variant's earlier alpha-only maintenance approach.

The retained dark source is `candidates/SpentIt_dark_source.png`; it is not a ready-to-use icon. It is the owner's 1024 px artwork after the 2026-10-28 alpha-only correction. Its opaque silhouette is a rounded rectangle of radius 214, flush with the top and sides and four pixels short of the bottom. The corners of that silhouette are white, and fully transparent pixels still store white. `tools/refine_dark_icon.swift` paints the rim with the nearby dark tile color, clips to the silhouette at 4× resolution, and writes all seven PNG sizes. Transparent pixels are cleared. The coin stacks and the white/red wordmark stay inside the mask. To reproduce on macOS, from the repository root:

```sh
swift tools/refine_dark_icon.swift
```

Keep alpha when exporting. Do not flatten against white or remove the white interior of the light tile. Inspect exports against both light and dark backgrounds.

After updating PNG sizes, rebuild the platform containers from the repository root:

```sh
python3 tools/generate_macos_icons.py
python3 tools/generate_windows_icon.py
```

Both scripts wrap the existing PNG data without resampling. ICNS contains the same standard/Retina representations as the originally supplied files. ICO uses the light 16, 32, 64, 128 and 256 px images. Installed application icons use light; the welcome/runtime artwork follows system appearance. See [local packaging](../../packaging/README.md).
