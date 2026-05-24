# Generated Protocol Buffer Files

This directory contains the C/C++ source files generated from the SmartCity.proto definition using Nanopb.

## Current Status

The files `SmartCity.pb.c` and `SmartCity.pb.h` have been generated and are ready for use.

## Generation Process

The files were generated using the Nanopb generator from the `proto/SmartCity.proto` file.

### Prerequisites
- Python 3.x
- Nanopb package: `pip install nanopb`
- Google Protocol Buffers compiler (protoc): Available from https://github.com/protocolbuffers/protobuf/releases

### Generation Command
```bash
python -m nanopb.generator.nanopb_generator proto/SmartCity.proto --output-dir=src/generated_proto
```

### Files
- `SmartCity.pb.h`: Header file with struct definitions and function declarations
- `SmartCity.pb.c`: Implementation file with encoding/decoding functions

**Note:** These files should be committed to the repository to ensure the project can be built without requiring protoc on every build system.
