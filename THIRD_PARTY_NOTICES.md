# Prism for PS5 notices

Prism for PS5 is GPL-3.0-or-later. The parent Android app remains MIT.

- Native UI, core, graphics, runtime and build tools: BlackBearReloaded's ps5-homebrew-ui / ps5-native-app-boilerplate, GPL-3.0-or-later. See LICENSE and retained source headers.
- WebSocket, image fetch and system keyboard: Afterglow contributors, GPL-3.0-or-later. See UPSTREAM.md for the source revision.
- nlohmann JSON 3.12.0: Niels Lohmann, MIT. Full licence: src/third_party/JSON-LICENSE.txt, also shipped in the app's licences folder.
- stb_image, stb_image_write and stb_truetype: Sean Barrett and contributors, MIT or public domain. See src/third_party/stb/LICENSE. stb_image is pinned to f0569113c93ad095470c54bf34a17b36646bbbb5.
- Atkinson Hyperlegible Next, Braille Institute of America, and Lexend, The Lexend Project Authors: SIL Open Font License 1.1; third_party/fonts/Prism-LICENSE.txt. Font sources are shared with Android Prism; `make fonts` bakes their SDF atlases.
- PS5 Payload SDK 0.42 and musl objects: MIT, third_party/musl/COPYRIGHT. The public SDK is fetched and hash-checked by tools/setup-native-dependencies.sh.
- ps5-opengl / Mesa: the pinned release's LICENSE, THIRD_PARTY_NOTICES.md and LICENSES are copied into every package by tools/build.sh.

No proprietary Sony SDK, firmware or keys are shipped. No connection credentials are packaged.
