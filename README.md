# MagicTrack (v1)

MagicTrack is a Logic-friendly text-driven tone shaping plugin (AU/VST3) with 8 permanent macros:

- `level`
- `brightness`
- `harshness`
- `body`
- `tighten`
- `crunch`
- `width`
- `space`

It ships with a no-LLM parser that maps commands like `brighter but not harsh` to macro deltas using `Resources/phrases.v1.json`.

## Hybrid Mode (Plugin + Sidecar)

MagicTrack now supports a v1 hybrid flow:

- Try local phrase parser first (instant, deterministic)
- If no phrase match and `LLM fallback` is enabled, call a local sidecar endpoint
- Sidecar returns JSON macro deltas, plugin validates/clamps/applies

No copy/paste is required between tools.

## Source Profiles

The plugin now includes a `Profile` control with tuned DSP ranges for:

- `General`
- `Guitar`
- `Vocal`
- `Upright Bass`

Each profile keeps the same 8 macro names and text commands, but changes macro-to-DSP wiring (EQ centers, compression behavior, saturation range, width range, and space range) for better defaults per source.

## Features in this scaffold

- Internal chain in processor order:
  - trim -> EQ (body/brightness/harshness shaping) -> harshness compressor -> compressor -> saturation -> width -> reverb mix
- Text parser:
  - phrase dictionary, longer phrases first
  - conjunction splitting (`and`, `but`, `however`, `though`)
  - intensity words (`slightly`, `really`, `super`, etc.)
  - negation heuristics (`not`, `less`, `without`, `no`)
  - caps/`!!!` boost
- Safety and UX:
  - clamp all macros to `0..100`
  - one-step Undo Last
  - A/B slots (store/recall)
  - telemetry label (`applied: brightness +10, harshness -8`)

## Build

Prereqs:

- macOS with Xcode + AU toolchain
- JUCE checkout
- CMake 3.22+

### What `JUCE_DIR` means

`JUCE_DIR` is just the folder where JUCE is cloned on your machine.
Example:

```bash
git clone https://github.com/juce-framework/JUCE.git /Users/sky/.openclaw/workspace/JUCE
```

Then use:

```bash
-DJUCE_DIR=/Users/sky/.openclaw/workspace/JUCE
```

Configure:

```bash
cd /Users/sky/.openclaw/workspace/magic-track
cmake -B build -S . -DJUCE_DIR=/absolute/path/to/JUCE
```

Build:

```bash
cmake --build build --config Release
```

## Quick parser test (no DAW)

```bash
cd /Users/sky/.openclaw/workspace/magic-track
cmake --build build --target magictrack_parser_cli
./build/magictrack_parser_cli "brighter but not harsh"
```

## Run Sidecar (for LLM fallback)

Start local sidecar:

```bash
cd /Users/sky/.openclaw/workspace/magic-track
python3 sidecar/magictrack_sidecar.py
```

Local Qwen via Ollama (default path used by sidecar):

```bash
ollama serve
```

By default sidecar will try:
- `http://127.0.0.1:11434/api/chat`
- model `llama3.2:3b`

Override model/endpoint if needed:

```bash
export MAGICTRACK_LOCAL_QWEN_MODEL="llama3.2:3b"
export MAGICTRACK_LOCAL_QWEN_OLLAMA_URL="http://127.0.0.1:11434/api/chat"
python3 sidecar/magictrack_sidecar.py
```

Optional OpenAI integration (otherwise sidecar uses deterministic heuristic fallback):

```bash
export OPENAI_API_KEY="<your_api_key>"
export MAGICTRACK_OPENAI_MODEL="gpt-4.1-mini"
python3 sidecar/magictrack_sidecar.py
```

Plugin UI defaults to:

- `LLM fallback (local sidecar)` enabled
- Sidecar URL `http://127.0.0.1:8787/interpret`

## Sidecar Tests

Run sidecar regression tests:

```bash
cd /Users/sky/.openclaw/workspace/magic-track/sidecar
python3 -m unittest -v tests/test_service.py
```

Heuristic rule config lives in:

- `/Users/sky/.openclaw/workspace/magic-track/sidecar/mt_sidecar/heuristics_rules.json`

## Notes

- The current harshness stage is a practical v1 approximation (fast compressor + harsh band EQ behavior) to keep the plugin responsive and stable.
- Next pass should tune exact macro wiring ranges by source type (guitar/bass/vocal/drums) and optionally add mode presets.
