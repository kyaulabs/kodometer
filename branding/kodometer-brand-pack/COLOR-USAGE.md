# Kodometer — Soft Iris palette proposal

This palette covers the native Plasma 6 widget and its website. It does not specify a full replacement Plasma theme. Logo artwork and lettering remain as approved; no widget code has been changed.

## Brand colors

| Color | Hex | Role |
| --- | --- | --- |
| Iris | #A28BE0 | Signature logo accent; brand graphics on dark surfaces |
| Deep Iris | #7052B5 | Light-surface brand accent; website links and buttons |
| Pale Iris | #C2B0E8 | Highlight accent on dark brand surfaces |
| Ink | #252333 | Light-background logo and website headings |
| Cloud | #F7F5FB | Dark-background logo lettering and light website canvas |
| Mist | #B8B0CB | Supporting brand graphics and secondary website text on dark surfaces |
| Night | #1B1924 | Dark website canvas |

## Plasma widget application

- Full-color identity: Iris gauge accent; use a light or dark wordmark appropriate to its background.
- Symbolic panel icon: a single-color adaptation that follows the panel foreground. Do not rely on a fixed dark K on every panel.
- Popup background, cards, text, secondary text, selections, focus, controls and status messages: inherit their appropriate Plasma/Kirigami theme roles. The user's theme remains authoritative.
- Compact aggregate quota meters: proposed optional brand accent uses Iris on dark surfaces and Deep Iris on light surfaces. A system-accent mode should follow the desktop highlight color. Check actual contrast against the inherited surface, especially with custom or translucent panel themes.
- Meter tracks: derive a subdued track from the active theme/accent and validate visibility against the actual panel. The existing 18% alpha is a starting point, not a universal contrast guarantee.
- Provider-specific colors: retain distinct provider identities. Iris identifies Kodometer overall, not every provider.
- Warning, error and success states: use desktop semantic colors and pair them with labels/icons. Do not apply website status hex values to the widget.
- Widget interface typography: inherit the desktop font. Custom Neo Sans-inspired lettering is reserved for the logo.

These are design recommendations, not implemented settings or verified runtime behavior. Validate native appearance under light, dark and custom Plasma themes before shipping.

## Website palette

The companion CSS defines a ten-step iris scale, light/dark surfaces, text, borders, button states, links, focus and status colors. Website colors can be fixed because the website controls its backgrounds.

Keep most surfaces neutral; use Iris for identity and emphasis. Use Deep Iris for white-label buttons on light pages and Iris with Night labels on dark pages. Reserve green, amber, rose and blue for semantic status information.

Calculated contrast for specified opaque color pairs:

- Ink on Cloud: 14.20:1.
- White on Deep Iris: 5.89:1.
- Night on Iris: 6.03:1.
- Light-theme secondary text on Cloud: 5.89:1.
- Dark-theme secondary text on Night: 8.35:1.
- All defined status foreground/background pairs: at least 5.40:1.

These calculations apply to the exact CSS pairs, not generated preview pixels or arbitrary Plasma themes. Faint decorative borders are not substitutes for the stronger control-border color.
