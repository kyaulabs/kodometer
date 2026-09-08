# Kodometer artwork

`kodometer-brand-pack/` preserves the supplied Soft Iris brand pack 1.0, including its guide, SVG masters, PNG exports, website tokens, favicons, and checksum manifest. Start with [COLOR-USAGE.md](kodometer-brand-pack/COLOR-USAGE.md) and the [brand guide](kodometer-brand-pack/guide/kodometer-brand-guide.pdf). The pack's original handoff notes describe its pre-integration state.

## Native integration

- The widget browser and notifications use `kodometer.svg`, installed in the prefix's `share/icons/hicolor/scalable/apps/` directory from `icons/app-tile-dark.svg`.
- The popup uses the dark/light wordmarks. Overview and empty-state icons use the symbolic master through Kirigami's theme-aware icon loader.
- Compact meters default to Iris on dark surfaces and Deep Iris on light surfaces. General settings can select the desktop accent instead. Popup surfaces, text, controls, focus, and status colors remain theme-owned.
- General settings offer panel bars or donut charts. Both show the same session and weekly remaining quotas. Rings run clockwise from twelve o'clock, with percentages inside when space permits. Tooltips and accessibility descriptions identify each quota; unavailable values say **Not reported**, not 0%. Vertical panels stack the rings.
- Only the runtime SVG subset is embedded in the plugin. No font binary, website stylesheet, favicon, guide, or PNG export is loaded by Plasma. Website materials are retained for future site work; this repository has no website deployment to update.

`applet/assets/` contains the runtime copies. CMake preserves their directory paths in QRC resources so source-based tests and the compiled widget resolve the same URLs.

## Provider icons

`provider-icons/` preserves the eight supplied SVG files. The runtime files in `applet/assets/providers/` change only root canvas dimensions and the viewBox: the shorter axis gets equal transparent padding on both sides. Paths, gradients, and colors are unchanged. OpenAI's file maps to Codex. Vectors remain sharp at different panel sizes and display scales; raster conversion with ImageMagick is unnecessary.

The white OpenAI and lime OpenRouter marks have a dark backing for visibility on light desktops. Other supplied artwork keeps its original colors and tiles. Provider names and marks belong to their respective owners; inclusion does not imply endorsement or grant rights to those marks. The supplied pack does not distribute or license a body-text font.

## Checks

```sh
python3 -m unittest discover -s tests -p 'test_branding.py'
ctest --test-dir build --output-on-failure -R 'visual-primitives|applet-configuration|staged-install'
```

These check original hashes, unchanged artwork, centered square canvases, theme selection, SVG loading, compiled asset URLs, settings persistence, ring values, and prefix-sensitive icon installation. Appearance on arbitrary custom or translucent Plasma themes still depends on the inherited surface; the desktop-accent option remains available.
