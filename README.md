# RLZAP

This project is an implementation of the compressed relative index strategy outlined in the paper "RLZAP: Relative Lempel-Ziv with Adaptive Pointers".

## Compiling

To compile it:
```
git clone https://github.com/farruggia/rlzap
mkdir rlzap/build
cd rlzap/build
cmake ..
make
```

The project is written in `C++11`, so a "modern" compiler is required. No external dependency is required (unless additional options are requested - see section "Advanced usage")

## Usage

Relatively compress DNA file `input` against `reference`, using the RLZAP strategy, into `input_index`:

```
./rlzap_build input reference input.rlz
```

To use the RLZ strategy instead:

```
./rlz_build input reference input.rlz
```

To decompress `input_index` back into `input` (regardless of the compression strategy used):

```
./index_decompress input.rlzap reference input
```

To display characters ranging from the 50th to the 100th:

```
./index_extract input.rlzap reference 50 100
```

To relatively compress DLCP files instead, just use the `-A` option:
```
./index_build dlcp_input dlcp_reference dlcp_input.rlzap -A dlcp32
```

Compression/representation parameters (like DeltaBits or MaxLiteral) can be tuned by using the appropriate command-line options of `index_build`: just invoke the tool without any argument to show all the options.

## Advanced usage

### Using the project as a library

All the functionalities provided by this project are exposed through an API. Consult the header file `include/rlz/api.hpp` for the list of supported functions, or have a look at the files into the `example` directory for some examples on invoking the functions and successfully compile and link against RLZAP as a third-party library.

### Compilation options

The `CMakeLists` defines a list of build options:
* `RLZ_BINARIES`: enables or disables generating the binaries. `ON` by default, turn it `OFF` if building RLZAP as a library.
* `RLZ_BENCHMARK`: if `RLZ_BINARIES` is `ON`, builds the `benchmark` utility. Needs libPAPI as an external library.
* `RLZ_TESTS`: builds the unit tests.