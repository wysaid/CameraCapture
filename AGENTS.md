# CameraCapture Agent Entry

This repository includes a publish-ready ClawHub/OpenClaw skill for `ccap`.

If you need to install or use `ccap` as a tool for camera capture, device inspection, image capture, or video metadata inspection, read the published skill folder first:

- [skills/ccap/SKILL.md](skills/ccap/SKILL.md)

Use that skill when the task involves any of the following:

- install ccap on the current machine
- build CameraCapture from source
- use Homebrew to install ccap on macOS
- download and run a release binary when no build toolchain is available
- list camera devices
- inspect device capabilities
- capture frames with the CLI
- inspect video metadata

The `.github/skills/` directory is for repository-local development workflows.
The `skills/ccap/` directory is the standalone skill bundle intended for ClawHub/OpenClaw distribution.

Prefer the `ccap` CLI over calling internal source files directly.
Prefer structured JSON output when the command supports `--json`.
Do not assume camera permission, GUI availability, or video file playback support on every platform.