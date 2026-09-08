# Kodometer brand pack 1.0

Soft Iris identity for the Plasma 6 widget and accompanying website.

Start with `guide/kodometer-brand-guide.pdf` and `boards/kodometer-identity-board.svg`.
All logo masters are vector paths, without embedded images or font dependencies.
PNG logo/mark exports are transparent; app tiles and favicon tiles have backgrounds.
Dark/light names identify the intended background, except symbolic fallbacks where
dark/light identifies the foreground. Logo PNG widths appear in their filenames.

Use `plasma/kodometer-symbolic.svg` for theme-aware foreground recoloring. Fixed
light/dark alternatives and PNGs are included. Plasma must load symbolic artwork
through its theme-aware icon mechanism; plain image loaders may use the fallback.
Do not replace the live quota meter with a static icon unless that behavior is intended.
Repository integration, icon installation and runtime checks remain implementation work.

Favicons provide regular (not maskable) install icons. The manifest does not implement
offline support. Adjust asset paths and deployment metadata for the actual website.

Custom letterforms reference the supplied Neo Sans Pro Medium. The font binary is
not redistributed. SVG logo paths do not constitute an installable font or license
for using Neo Sans in body text. Website/body typography uses installed system fonts.

Brand colors and UI tokens are in website/colors.css. Do not apply its hard-coded
surfaces or semantic colors to the Plasma widget. See COLOR-USAGE.md.

Guide SVG/PNG pages are supplied for editing and reuse. The PDF is image-based.
The vector source artwork supersedes the earlier generated concept sheets.
