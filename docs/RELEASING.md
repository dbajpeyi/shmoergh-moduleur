# Releasing

## Publishing a release

1. Update `VERSION` in `CMakeLists.txt` (for example `1.0.0`).
2. Add a section for the version at the top of `CHANGELOG.md`, headed `## v1.0.0 — <date>`, saying what changed in plain words. The release fails if the section is missing.
3. Publish it in one of two ways:
   - Push a matching tag: `git tag v1.0.0 && git push origin v1.0.0`
   - Or open **Actions → Build → Run workflow** on `main` and enter `v1.0.0`.

The workflow builds and tests on macOS and Linux and validates the Audio Unit with `auval`. It then publishes a GitHub release with three zips (AU, VST3, Standalone) and a `SHA256SUMS.txt`. Its text is the version's `CHANGELOG.md` section followed by the install notes in `.github/release-notes.md`.

To fix the notes of a release that is already out, edit `CHANGELOG.md` on `main`, then open **Actions → Release notes → Run workflow** and enter the version, or `all`. This only rewrites the text; the downloads stay as they are.

## Before distributing on the website

**Notarization.** Release builds are currently signed ad hoc, so macOS warns users the first time. To remove that step:

- Sign with a Developer ID certificate from the Shmøergh Apple Developer account.
- Enable the hardened runtime (`HARDENED_RUNTIME_ENABLED TRUE` in `juce_add_plugin`). Add the audio-input entitlement for the standalone app's HOST jacks.
- Notarize and staple each bundle with `xcrun notarytool` and `xcrun stapler`.
- Store the certificate and the notary credentials as GitHub Actions secrets. Only the release job should use them.

**JUCE licence.** The plugin is built with JUCE, which is dual-licensed under AGPLv3 and the JUCE licence. Before distributing binaries, register for the appropriate JUCE licence. JUCE's free tier covers small businesses below its revenue limit. If you don't, the binaries are covered by AGPLv3, which requires offering the complete source code with them.

**Plugin identity.** The Audio Unit is identified by manufacturer code `Shmo` and plugin code `Mdlr`, and the bundle ID is `com.shmoergh.moduleur`. Keep these stable from the first public release on; changing them stops DAW projects from finding the plugin.

## Updating the panel art

The background and the knob outlines come from the Figma design. To regenerate them after a design change, follow `tools/panel-art/README.md`.
