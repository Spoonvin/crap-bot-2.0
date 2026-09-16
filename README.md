# Crap bot 2.0

Project under development.

## Dependencies

SDL3 and SDL3-image are only required when building the GUI.

## Build

```sh
make all
```

Run the search and UCI regression tests with `make test` (requires Python 3).

To build with the GUI, install SDL3 and SDL3-image, then run:

```sh
make USE_GUI=1
```

## Play against the engine

```sh
./app play
```

`play` is available only in a GUI build.

## Run as a UCI engine

```sh
./app
```

`./app uci` also starts the same UCI command loop.
