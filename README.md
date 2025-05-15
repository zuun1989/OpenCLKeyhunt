# KeyhuntCL

KeyhuntCL is an OpenCL-accelerated cryptocurrency puzzle solver based on the original [keyhunt](https://github.com/albertobsd/keyhunt.git) codebase with GPU acceleration features inspired by [BitCrack](https://github.com/brichard19/BitCrack.git).

## Features

- Search for private keys associated with Bitcoin addresses, RIPEMD160 hashes, or public key X points
- GPU acceleration using OpenCL for increased performance
- Optimized for NVIDIA GPUs, especially GTX 1060 3GB with CUDA 12.8
- Support for both CPU and GPU modes with automatic fallback
- Compatible with all original keyhunt functionality
- Configurable batch size and range parameters for optimal performance
- Bloom filter optimization for fast lookups
- Baby Step Giant Step (BSGS) support (simplified implementation)
- Multi-item-per-thread processing for improved GPU efficiency

## Requirements

- C compiler (GCC, Clang)
- OpenCL libraries and headers (if using GPU acceleration)
- NVIDIA CUDA 12.8 driver (for optimal performance on NVIDIA GPUs)
- CMake 3.10 or higher
- OpenCL-compatible GPU for GPU mode (optimized for NVIDIA GTX 1060 3GB)

## Building

### CPU-only Version

The default build creates a CPU-only version that works on any system:

```bash
mkdir build
cd build
cmake ..
make
```

### GPU-accelerated Version

To build with OpenCL GPU acceleration support (requires OpenCL development libraries):

```bash
mkdir build
cd build
cmake -DENABLE_OPENCL=ON ..
make
```

For NVIDIA GPUs, ensure you have the CUDA toolkit and drivers installed. The build is specifically optimized for the GTX 1060 but will work on other NVIDIA GPUs as well.

## Usage

KeyhuntCL maintains compatibility with the original keyhunt command-line interface while adding GPU acceleration options:

```
./keyhuntcl -m MODE [options]
```

### Modes

- `-m rmd160`: Search for RIPEMD160 hashes
- `-m address`: Search for Bitcoin addresses
- `-m xpoint`: Search for public key X points
- `-m bsgs`: Use Baby Step Giant Step algorithm (simplified)

### Common Options

- `-f FILE`: Input file with target hashes/addresses (one per line)
- `-r START:END`: Range of private keys to search (hex format)
- `-b BATCH_SIZE`: Number of keys to process per batch (default: 1048576)
- `-c`: Use CPU for computation (default if OpenCL is disabled)
- `-g DEVICE_ID`: Use GPU with specified device ID (requires OpenCL build)
- `-p PLATFORM_ID`: Use OpenCL platform with specified ID (default: 0)
- `-t THREADS`: Number of threads for CPU mode (default: number of CPU cores)
- `-l`: List available OpenCL platforms and devices

### Performance Tuning

For NVIDIA GTX 1060 3GB:
- Optimal batch size (`-b`): 16,777,216 
- Optimal thread block size is set automatically (256)
- Each thread processes 16 keys at once for better efficiency

### Examples

List available OpenCL devices:
```
./keyhuntcl -l
```

Search for Bitcoin addresses using GPU device 0:
```
./keyhuntcl -m address -f addresses.txt -g 0 -r 1:FFFFFFFF
```

Search for RIPEMD160 hashes using CPU with 8 threads:
```
./keyhuntcl -m rmd160 -f hashes.txt -c -t 8 -r 80000000:8FFFFFFF
```

## Performance

The GPU-accelerated version can achieve significant speedup compared to CPU-only mode:

- NVIDIA GTX 1060 3GB: ~450-550 million keys/second for RIPEMD160 searches
- CPU (8-core): ~8-15 million keys/second

Actual performance depends on specific hardware, driver versions, and search parameters.

## Building for NVIDIA GTX 1060 3GB

For optimal performance on the GTX 1060 3GB, ensure you have CUDA 12.8 drivers (or compatible) installed. The code contains specific optimizations for this GPU model, including:

1. Optimized thread block size (256 threads per block)
2. Multi-key processing per thread (16 keys per thread)
3. Efficient memory access patterns for NVIDIA's architecture
4. Word-level comparisons in hash matching functions

## License

This project follows the same licensing as the original keyhunt and BitCrack projects.
