# Imported dashboard and Gateway

Local smart-garage source was mapped to the team layout: web/ -> dashboard/, firmware/gateway/ -> gateway/. Tools and integration docs are included. The original Gateway skeleton (UART receive + ACK) is in git history (commit 345bbed, gateway/src/main.c). sensor-node/, shared/, cloud/ and existing team docs were preserved.

## Run

Web: cd dashboard, then npm.cmd ci, npm.cmd test, npm.cmd run build, npm.cmd run dev.
Gateway: open gateway/ in VS Code with PlatformIO. The existing gateway_local_n16r8 environment targets the local N16R8 module and simulated hardware. Do not assume the carrier board identity is verified by this import.

Local credentials, Wi-Fi passwords, broker runtime config, .env.local, build outputs and flash backups were intentionally excluded. Configure fresh ignored local settings before hardware/MQTT testing. See gateway/include/config.example.h and dashboard/.env.example. Never commit live credentials.

## Integration limits

The imported Gateway uses its own protocol implementation. Compatibility with the existing sensor-node/ and shared/protocol/ is NOT verified by copying files. Compare docs/protocol.md with docs/integration-contract.md before testing real UART. Existing handoff hardware details remain historical; the user's current module is N16R8 (16 MB flash, 8 MB PSRAM). Face recognition, sensor hardware, cloud logging and physical door control need separate validation. Historical verification notes from the source are not proof of testing this repository.

AI assistance was used to develop and arrange the imported code. Consult package manifests and original source comments for dependency licenses and attribution.
