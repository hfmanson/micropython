# Building Micropython for Tanmatsu

Follow [esp32 building instructions](../../README.md).

## Install esp32p4 target

```bash
$ cd esp-idf
$ ./install.sh esp32p4 # (or install.bat on Windows)
$ source export.sh     # (or export.bat on Windows)
```

## Build for Tanmatsu
```bash
$ cd ports/esp32
$ make submodules
$ make BOARD=tanmatsu
```
