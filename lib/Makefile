ifeq ($(shell uname),Darwin)
    EXT := dylib
	CFLAGS := "-mmacosx-version-min=10.11"
else
    EXT := a
	CFLAGS := ""
endif

PWD := $(shell pwd)

all: release

winrelease: target/x86_64-pc-windows-gnu/release/silentdragonlite.lib

target/x86_64-pc-windows-gnu/release/silentdragonlite.lib: src/lib.rs Cargo.toml
	SODIUM_LIB_DIR="$(PWD)/libsodium-mingw/" cargo build --lib --release --target x86_64-pc-windows-gnu

release: target/release/silentdragonlite.$(EXT)
debug: target/debug/silentdragonlite.$(EXT) 

target/release/silentdragonlite.$(EXT): src/lib.rs Cargo.toml
	LIBS="" CFLAGS=$(CFLAGS) cargo build --lib --release

target/debug/silentdragonlite.$(EXT): src/lib.rs Cargo.toml
	LIBS="" CFLAGS=$(CFLAGS) cargo build --lib

clean:
	rm -rf target