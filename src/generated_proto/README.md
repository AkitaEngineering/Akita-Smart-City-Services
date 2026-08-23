# Generated Nanopb sources

`SmartCity.pb.c` and `SmartCity.pb.h` are generated from `proto/SmartCity.proto` plus `proto/SmartCity.options` with Nanopb 0.4.9.1. `SmartCity_compat.h` contains stable aliases used by the C++ module.

Regenerate from the repository root in a clean Python environment:

```bash
python -m nanopb.generator.nanopb_generator \
  -I proto -D src/generated_proto proto/SmartCity.proto
```

Review the generated diff and run the official firmware compile gate whenever the schema, options, Nanopb, or generator changes.
