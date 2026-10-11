# Aerosound Plugin UI System

The current AutoTrim UI on `feature/ara` is the canonical reference implementation for the Aerosound plugin family. New Aerosound plugins should inherit this visual language and interaction grammar unless a product-specific reason requires a deliberate exception.

The baseline is the latest light-blue / frosted AutoTrim interface with the current bottom-row controls and the global Bypass visual state. Do not use older dark mockups or previous AutoTrim layouts as references.

## Canonical base files

For every new Aerosound plugin, start from these files before inventing new visual rules:

- `AEROSOUND_UI_GUIDE.md` — the human-readable design / brand system.
- `Source/AerosoundTheme.h` — canonical colour, typography, geometry and timing tokens.
- `Source/AerosoundControls.h` — reusable control behaviour and compound-control patterns.
- Current AutoTrim `PluginEditor` on `feature/ara` — the reference for overall density, spacing, hierarchy, panel treatment and control proportions.

Product-specific controls may differ, but the surrounding visual grammar should still read immediately as Aerosound.

## Reference canvas and resize

- Reference editor size: 1100 x 760.
- Keep the 1100:760 aspect ratio unless the plugin genuinely needs a different form factor.
- Scale geometry and typography from the reference canvas with one shared scale factor.
- Paired controls must remain visually equal at every supported editor size.

## Brand hierarchy

- Product name is the primary title.
- `by Aerosound` sits directly beneath the product name and is visually secondary.
- Do not let the brand line compete with the main product name.

## Palette

Canonical colour tokens live in `Source/AerosoundTheme.h`.

- Ink: `#102333`.
- Muted ink: `#4B687C`.
- Main accent: `#2487C6`.
- Dark accent: `#146AA5`.
- Lines: `#93CAEB`.
- Sky: `#D9F3FF` through `#C7ECFB` / `#A0DAF3` to `#83C9EC`.
- Frosted panels: `#EFFAFF` with high opacity plus a soft white frost pass.
- Donate heart remains Aerosound measuring-red in the normal active UI.

## Background and panels

- Background uses a light sky gradient, atmospheric white haze and subtle blue waves.
- Main panels are matte/frosted.
- Static labels must be painted above the frost layer.
- Main panel corner radius: 18 reference pixels.

## Typography

- Primary UI font: Segoe UI.
- Meter/live compact numeric readouts: Courier New where stable character width prevents visual jumping.
- Section titles are bold and clearly stronger than metric labels.
- Functional labels remain readable; only truly inactive controls are dimmed.

## Controls and interaction

- Secondary utility actions use the same compact light/frosted button treatment as AutoTrim.
- Lower utility row family reference: `Reset -> Help -> Bypass -> Donate`.
- Brand icons are vector geometry, not Unicode glyphs.
- Popups should remain inside the plugin editor whenever practical and should not steal keyboard focus from the host DAW.

## Bypass state

- Bypass remains directly accessible in the editor.
- When Bypass is ON, the whole functional editor becomes grayscale and inactive.
- The Bypass control is the only control that remains active, coloured and clickable.
- Leaving Bypass restores the normal UI immediately without losing parameter values or state.

## Reuse rule

Use the current AutoTrim UI as a visual baseline, not as a code template for product-specific DSP/editor logic. Reuse themed components where sensible, while keeping AeroGate DSP and plugin logic independent.
