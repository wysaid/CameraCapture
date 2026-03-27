---
name: ccap
description: "Install or use the ccap CLI for camera capture, webcam inspection, device listing, frame capture, and video metadata. Use when you need to work with CameraCapture on macOS, Linux, or Windows, especially for listing devices, checking device capabilities, capturing frames, inspecting video files, or choosing between existing install, Homebrew, source build, and release-binary fallback."
argument-hint: "install | list-devices | device-info | capture | video-info"
metadata: { "openclaw": { "emoji": "📷", "homepage": "https://github.com/wysaid/CameraCapture", "install": [{ "id": "brew", "kind": "brew", "formula": "wysaid/ccap/ccap", "bins": ["ccap"], "os": ["macos"], "label": "Install ccap (Homebrew)" }] } }
---

# ccap

Use this skill when an agent needs to install or operate `ccap` as a practical vision-input CLI.

This skill is designed for ClawHub/OpenClaw publication as a standalone skill folder.

## Typical Triggers

Use this skill when the user asks for things like:

- "list my cameras"
- "show device capabilities"
- "capture one frame from webcam 0"
- "inspect this mp4"
- "install ccap on this machine"
- "use CameraCapture from the CLI"

## What This Skill Is For

- Installing `ccap` on the current machine
- Reusing an existing `ccap` installation when already available
- Building `ccap` from source when local development tools exist
- Falling back to a release binary when no build toolchain is available but a matching release asset exists
- Listing camera devices
- Inspecting device capabilities
- Capturing one or more frames with the CLI
- Reading video metadata

## What This Skill Is Not For

- Editing CameraCapture library internals
- Explaining or modifying OpenClaw internals
- Assuming camera access permission is already granted
- Assuming preview is safe in CI, SSH, or headless environments
- Assuming all platforms support video-file playback equally

## Default Behavior

- Prefer the `ccap` CLI over internal source files.
- Prefer `--json` when the command supports it.
- Prefer non-preview workflows unless the task explicitly needs preview.
- Treat camera devices, permissions, GUI support, and backend support as environment-dependent.

## Procedure

1. Determine whether the task is installation, device inspection, capture, or video inspection.
2. Reuse an existing `ccap` binary if one is already available.
3. If installation is needed, follow the environment decision tree instead of inventing a platform-specific path.
4. Prefer a JSON-capable command when the task can be solved with structured output.
5. Verify the selected workflow with a minimal command before attempting larger capture or preview runs.
6. If the environment blocks the task, report the exact blocker: missing binary, missing permission, no device, unsupported platform path, or unavailable release asset.

## Environment Decision Tree

1. Check whether `ccap` is already installed and runnable.
2. If running on macOS and `brew` exists, prefer Homebrew installation.
3. Otherwise, if CMake and a compiler are available, build from source.
4. Otherwise, if network access is available and a matching GitHub Release asset exists, download and use that binary.
5. If none of the above is possible, stop and report the missing prerequisite instead of guessing.

See [installation details](./references/install.md).

## Stable Command Patterns

Use these first before improvising:

- List devices: `ccap --list-devices --json`
- Device info: `ccap --device-info 0 --json`
- Video info: `ccap -i /path/to/video.mp4 --json`
- Capture one frame: `ccap -d 0 -c 1 -o ./captures`
- Capture one frame from a named device: `ccap -d "OBS Virtual Camera" -c 1 -o ./captures`

More examples and fallback notes are in [command reference](./references/commands.md).

## Operational Rules

- If a command supports `--json`, prefer parsing JSON over scraping text output.
- If device enumeration succeeds but opening the device fails, treat that as a permission or device-access issue.
- If camera permission is denied, report the authorization requirement instead of repeated retries.
- If there is no camera device, consider a video-file workflow if that satisfies the task.
- If the environment is headless or remote, do not enable preview by default.
- If video playback is unsupported on the current platform or build, report it explicitly.

## Response Expectations

- Prefer reporting the exact command used.
- Prefer reporting whether the output is structured JSON or plain text.
- If installation was performed, state which path was chosen: existing binary, Homebrew, source build, or release binary.
- If the task failed, report the smallest actionable next step instead of a vague failure summary.

## Verification

After installation or selection of an existing binary, verify with one or more of:

```bash
ccap --version
ccap --list-devices
ccap --list-devices --json
ccap -i /path/to/video.mp4 --json
```

## Source Of Truth

Before guessing flags, build steps, or platform behavior, consult:

- [install notes](./references/install.md)
- [command reference](./references/commands.md)
