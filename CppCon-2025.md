# DEMO

```bash
# https://www.godbolt.org/z/6odET7c1v

# SSH to QEMU
ssh -p 2222 bartek@localhost

# static (no Shadow Stack)
./build/examples/example-c++-static
./build/examples/example-c++-static 140
gdb --args ./build/examples/example-c++-static 140


# instrumented (Shadow Stack compiled-in)
SHST_DUMP_COLOR=always ./build/examples/example-c++-instrumented 348 2>&1 | c++filt
gdb --args ./build/examples/example-c++-instrumented 348

SHST_REACTION=report gdb --args ./build/examples/example-c++-instrumented 348
SHST_REACTION=heal ./build/examples/example-c++-instrumented 348
SHST_REACTION=quiet-heal ./build/examples/example-c++-instrumented 348

SHST_DUMP_HIDE_EQUAL=yes SHST_DUMP_AREA=actual SHST_DUMP_WIDTH=8 SHST_DUMP_COLOR=always ./build/examples/example-c++-instrumented 348 2>&1 | c++filt

# preload (Shadow Stack added externally)
LD_PRELOAD=./build/examples/libpreload-lib.so SHST_DUMP_COLOR=always ./build/examples/example-c++-dynamic 1020 2>&1 | c++filt
gdb --args ./build/examples/example-c++-dynamic 140
```