p<!--  --># Workspace Map

This workspace contains two related codebases:

- `wii-pico-installer`: the TinyUSB device firmware in this folder.
- `wii-ras-installer`: the devkitPro/libogc host application in the sibling folder.

Suggested workflow:

1. Make protocol changes in the wii-pico-installer firmware first.
2. Mirror any protocol or contract changes in the wii-ras-installer host project.
3. Run the Pico smoke test or firmware build before validating the Wii side.
4. Run the Wii build after any host-side change.

Routing rule of thumb:

- USB descriptors, TinyUSB callbacks, and device-side state belong in the wii-pico-installer root.
- Wii-side USB discovery, protocol encoding, and startup self-test logic belong in the wii-ras-installer root.
- If a change touches the wire protocol, update the firmware, the Wii protocol wrapper, and the protocol notes together.
