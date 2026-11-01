# Security

To report a security problem, email Shmøergh through [shmoergh.com](https://www.shmoergh.com) rather than opening a public issue. We'll reply as soon as we can and credit you in the fix if you'd like.

## What the plugin does and doesn't do

- It makes no network connections and collects no data.
- It reads and writes nothing on disk except the state your DAW stores in your project.
- It uses audio input only for the HOST L / HOST R jacks. In the standalone app, macOS asks for microphone permission before any input is used.
- Saved state is treated as untrusted. Values from a project or preset file are validated or clamped before use, and malformed data is ignored. The automated tests include deliberately corrupted sessions.

## Verifying downloads

Each release includes `SHA256SUMS.txt`. To check a download, run `shasum -a 256 Moduleur-*.zip` and compare the result with that file.
