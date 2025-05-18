# CLC Miner

A high-performance CLC mining implementation in C language.

## Features

- Multi-threaded mining
- Pre-generated keypair pool for improved performance
- Parallel keypair generation for faster startup
- AVX-512 SIMD optimizations
- Efficient secp256k1 and SHA256 implementations
- Configurable mining parameters
- Pool mining support
- Reward tracking and storage
- Real-time hash rate display

## Dependencies

- libcurl
- libsecp256k1
- libcrypto (OpenSSL)
- pthread
- AVX-512 capable CPU (for optimal performance)

## Building

1. Install dependencies:

```bash
# Ubuntu/Debian
sudo apt-get install libcurl4-openssl-dev libsecp256k1-dev libssl-dev

# CentOS/RHEL
sudo yum install libcurl-devel libsecp256k1-devel openssl-devel

2. Build the project:

make

## Configuration

Edit clcminer.conf to configure the miner:

toml
/*
# Server URL
server = "https://pool.clc.ix.tc"

# Directory to store mined coins
rewards_dir = "./rewards"

# Number of mining threads (-1 for auto)
thread = -1

# Job update interval in seconds
job_interval = 1

# Report interval in seconds
report_interval = 10

# Command to run when a coin is mined (use %cid% for coin ID)
on_mined = ""

# Pool configuration
pool_secret = ""

# Reporting configuration
[reporting]
report_server = "https://clc.ix.tc:3000"
report_user = ""

*/
## Usage

1. Configure the miner by editing `clcminer.toml`
2. Run the miner:

```bash
./cminer
```

## Performance Optimizations

The miner includes several performance optimizations:

- **Pre-generated Keypair Pool**: A 3GB pool of pre-generated keypairs is created at startup, eliminating the need to generate new keypairs during mining.
- **Parallel Keypair Generation**: Utilizes multiple threads to generate keypairs in parallel, significantly reducing startup time.
- **AVX-512 SIMD Instructions**: Utilizes AVX-512 instructions for faster hash comparisons and memory operations.
- **Multi-threaded Mining**: Efficiently utilizes all available CPU cores.
- **Lock-free Data Structures**: Minimizes thread contention for better scalability.
- **Batch Processing**: Processes data in batches to reduce overhead.

## License

MIT License 
