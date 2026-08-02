# Contributing

Thanks for helping improve AeroScope. Small fixes, hardware notes, UI ideas, screenshots, and bug reports are all welcome.

The goal is to keep contributing simple and friendly.

## Easy Contribution Flow

1. Fork the repo.
2. Make a focused change.
3. Open a pull request with a short explanation.
4. Mention whether you tested it on hardware.

For bigger changes, like a new data provider, display driver changes, or major UI behavior, opening an issue first is helpful.

## Checks

If you can, run:

```bash
pio run -e spotpear_s3_touch_lcd_28d
python -m pytest firmware/tests tools/tests
```

If you cannot run the checks, that is okay. Just say so in the pull request.

## Helpful Details

For display, touch, Wi-Fi, provider, or map changes, it helps to include:

- Board/screen variant tested.
- A photo, screenshot, or short serial log if useful.
- What changed from the user's point of view.

## Please Avoid

- Private Wi-Fi credentials.
- API keys.
- Exact home coordinates.
- Generated build/cache folders.
- Large unrelated formatting-only changes.

By contributing, you agree that your contribution may be distributed under this repository's license.
