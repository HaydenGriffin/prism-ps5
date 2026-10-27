# Native host tooling

All repository-owned format tooling is C or C++. Make and Bash are thin
orchestrators. No C# or .NET runtime is required.

The root build performs four native stages:

1. `prospero-clang18` compiles application sources and `native/app_crt.cpp`.
2. LLVM lld resolves objects, static archives, public SDK import stubs,
   constructors, TLS, and unwind information into an intermediate PIE.
3. `native/ps5-native-tool` converts that PIE to the PS5 ELF layout and wraps
   it in a deterministic development FSELF.
4. The app folder is assembled.

`native/libc_builder.cpp` independently reproduces the generated clean-room
runtime from the two manifests under `native/runtime/`. `make app` runs
`tools/rebuild-libc.sh` first, which builds it twice and checks both hashes.

`tools/setup-native-dependencies.sh` fetches the pinned public payload SDK and
static zlib archive into `.deps/native/`; neither is installed globally.

The format boundary and low-level commands are described in Afterglow's
[native tooling notes](https://github.com/HaydenGriffin/afterglow-ps5/blob/main/docs/platform/NATIVE_TOOLING.md),
where this tooling comes from.
