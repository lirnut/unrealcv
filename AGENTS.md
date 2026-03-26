# Repository Guidelines

## Project Structure & Module Organization
`Source/UnrealCV` contains the runtime plugin module, split into `Public/` headers and `Private/` implementation files by subsystem (`Actor/`, `Sensor/`, `Server/`, `Commands/`, `BPFunctionLib/`). `Source/UnrealCVEditor` holds editor-only tooling. Use `client/python` and `client/cxx` for external client libraries, `workflow/` for build and runtime test automation, `docs/` for Sphinx and Markdown documentation, and `examples/` for sample usage. Treat `Binaries/` and `Intermediate/` as generated output.

## Build, Test, and Development Commands
Build the plugin through the host UE project or the local harness:

```bash
cd workflow
python harness.py build
python harness.py full --headless
python harness.py test --pytest
```

`build` compiles the UE project, `full` runs build + launch + tests + log monitoring, and `test --pytest` adds the Python-side test suite. For the Python client, run `tox` from the repo root; it installs `client/python` in editable mode and executes `examples/commands_demo.py`.

## Coding Style & Naming Conventions
Follow Unreal Engine C++ conventions already used here: tabs in `.Build.cs`, UE-style class prefixes (`U`, `A`, `F`, `I`), PascalCase type and method names, and one class per matched `.h` / `.cpp` pair. Keep new files inside the existing subsystem folders. Blueprint libraries follow `*BPLib.h` / `*BPLib.cpp`; command handlers follow `*Handler.h` / `*Handler.cpp`. Python code in `client/python/unrealcv` uses PEP 8 and snake_case.

## Testing Guidelines
Prefer closed-loop validation with `workflow/harness.py`, especially for runtime features touching commands, sensors, or recording. Add or extend harness coverage in `workflow/test_runner.py` when you introduce new UnrealCV commands. For Python client changes, keep `tox` passing and add focused examples or regression checks near the affected API.

## Commit & Pull Request Guidelines
Recent history mixes informal commits (`up`, date stamps) with clearer Conventional Commit-style messages such as `feat(workflow): ...` and `fix(ProxyAnnotationSystem): ...`. Prefer the structured form: `feat(area): summary`, `fix(area): summary`, `docs: summary`. PRs should describe user-visible behavior, list validation steps run (`python harness.py test`, `tox`), link related issues, and include screenshots or log snippets when editor UI, recording, or rendering behavior changes.

## Security & Configuration Tips
Keep machine-specific paths in local workflow config files, not in tracked docs or source. Do not commit generated captures, packaged binaries, or absolute project paths unless the change intentionally updates shared automation defaults.
