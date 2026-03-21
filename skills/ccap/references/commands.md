# ccap Command Reference For Agents

Use the CLI directly. Prefer JSON when available.

## Device Enumeration

Preferred:

```bash
ccap --list-devices --json
```

Fallback text mode:

```bash
ccap --list-devices
```

## Device Info

Preferred:

```bash
ccap --device-info 0 --json
```

If the device opens successfully, expect capability data.
If the device cannot be opened, expect a structured error or text error depending on mode.

## Video Metadata

Preferred:

```bash
ccap -i /path/to/video.mp4 --json
```

Use this when no camera is required and the task is only about file inspection.

## Frame Capture

One frame:

```bash
ccap -d 0 -c 1 -o ./captures
```

Batch capture:

```bash
ccap -d 0 -c 10 -o ./captures
```

## Operational Cautions

- Do not assume camera permission is granted.
- Do not default to preview in headless environments.
- Do not assume Linux supports the same video playback features as macOS or Windows.
- Prefer JSON parsing over text scraping when `--json` exists.