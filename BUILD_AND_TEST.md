# Building and Testing Taskwarrior

This guide will help you build and test Taskwarrior from source before making changes.

## Prerequisites

### Required Dependencies

1. **CMake 3.22 or later**
   ```bash
   # Ubuntu/Debian
   sudo apt-get install cmake
   
   # macOS (with Homebrew)
   brew install cmake
   
   # Verify version
   cmake --version
   ```

2. **C++ Compiler with C++17 support**
   - gcc 7.0+ or clang 6.0+
   ```bash
   # Ubuntu/Debian
   sudo apt-get install build-essential
   
   # macOS (usually pre-installed with Xcode)
   xcode-select --install
   
   # Verify
   g++ --version
   # or
   clang++ --version
   ```

3. **libuuid** (not needed on macOS)
   ```bash
   # Ubuntu/Debian
   sudo apt-get install uuid-dev
   
   # Fedora/RHEL
   sudo dnf install libuuid-devel
   ```

4. **libsqlite3-dev** (required for work intervals feature)
   ```bash
   # Ubuntu/Debian
   sudo apt-get install libsqlite3-dev
   
   # Fedora/RHEL
   sudo dnf install sqlite-devel
   
   # macOS (with Homebrew)
   brew install sqlite
   
   # Arch Linux
   sudo pacman -S sqlite
   ```

4. **Rust 1.64.0 or higher**
   ```bash
   # Install Rust (recommended method)
   curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | sh
   source $HOME/.cargo/env
   
   # Verify
   rustc --version
   cargo --version
   ```

### Optional Dependencies (for testing)

5. **Python 3** (for test suite)
   ```bash
   # Usually pre-installed, verify:
   python3 --version
   ```

6. **Git** (for submodules)
   ```bash
   sudo apt-get install git
   ```

## Step-by-Step Build Instructions

### 1. Clone the Repository (if not already done)

```bash
git clone https://github.com/GothenburgBitFactory/taskwarrior.git
cd taskwarrior
```

### 2. Initialize Git Submodules

Taskwarrior uses submodules for some dependencies:

```bash
git submodule update --init --recursive
```

This will download:
- `src/libshared` - Shared library
- `src/taskchampion-cpp/corrosion` - Rust/C++ bridge

### 3. Configure the Build

Create a build directory and configure with CMake:

```bash
# Create build directory and configure
cmake -S . -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo
```

**Build Types:**
- `RelWithDebInfo` - Optimized with debug symbols (recommended for development)
- `Debug` - Full debug symbols, no optimization (slower, easier to debug)
- `Release` - Fully optimized, no debug symbols (fastest, production)

**What this does:**
- Scans for dependencies (CMake, C++ compiler, Rust, libuuid)
- Generates build files
- May take 1-2 minutes on first run

### 4. Build the Project

```bash
# Build everything
cmake --build build
```

**Or build in parallel (faster):**
```bash
# Use all CPU cores
cmake --build build -j $(nproc)

# Or specify number of jobs
cmake --build build -j 4
```

**Or build just the main executable:**
```bash
cmake --build build --target task_executable
```

**Expected output:**
- The main executable will be at: `build/src/task`
- Build time: 2-5 minutes depending on your system

### 5. Verify the Build

Test that the executable works:

```bash
# Run the built taskwarrior
./build/src/task --version

# Should output something like:
# task 3.4.2
```

## Running Tests

### 1. Build Test Targets

Before running tests, build the test runner:

```bash
cmake --build build --target test_runner --target task_executable
```

### 2. Run All Tests

```bash
# Run all tests (serial, slower but easier to debug)
ctest --test-dir build

# Run tests in parallel (faster)
ctest --test-dir build -j $(nproc)

# With verbose output on failure (recommended)
ctest --test-dir build -j $(nproc) --output-on-failure
```

**Expected output:**
- Tests will run and show results
- Should see mostly passing tests
- May take 5-15 minutes depending on your system

### 3. Run Specific Tests

```bash
# Run only C++ unit tests
ctest --test-dir build -R cpp

# Run only Python tests
ctest --test-dir build -R test.py

# Run only tests matching a pattern
ctest --test-dir build -R variant

# Rerun only failed tests
ctest --test-dir build --rerun-failed
```

### 4. Run a Single Test Manually

```bash
# Python tests can be run directly
cd test
python3 add.test.py

# Or from the test directory
./add.test.py
```

## Quick Test Workflow

Here's a complete workflow to verify everything works:

```bash
# 1. Clean start (if needed)
cd /workspaces/taskwarrior
git submodule update --init --recursive

# 2. Configure
cmake -S . -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo

# 3. Build
cmake --build build -j $(nproc)

# 4. Verify executable
./build/src/task --version

# 5. Build tests
cmake --build build --target test_runner -j $(nproc)

# 6. Run a quick test subset
ctest --test-dir build -R cpp --output-on-failure

# 7. If that works, run all tests
ctest --test-dir build -j $(nproc) --output-on-failure
```

## Common Issues and Solutions

### Issue: CMake can't find Rust

**Solution:**
```bash
# Make sure Rust is in your PATH
source $HOME/.cargo/env
# Then rerun cmake
cmake -S . -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo
```

### Issue: Missing libuuid

**Error:** `libuuid not found`

**Solution:**
```bash
# Ubuntu/Debian
sudo apt-get install uuid-dev

# Then rerun cmake
cmake -S . -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo
```

### Issue: Submodule errors

**Error:** `fatal: not a git repository`

**Solution:**
```bash
git submodule update --init --recursive
```

### Issue: Build fails with Rust errors

**Solution:**
```bash
# Update Rust toolchain
rustup update stable

# Clean and rebuild
rm -rf build
cmake -S . -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build
```

### Issue: Tests fail

**First, try:**
```bash
# Clean rebuild
rm -rf build
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --target test_runner --target task_executable
ctest --test-dir build --output-on-failure
```

**If specific tests fail:**
- Some tests may be flaky or environment-dependent
- Check if it's a known issue in the repository
- Focus on tests related to your changes

## Development Workflow

### After Making Changes

1. **Rebuild only what changed:**
   ```bash
   cmake --build build
   ```

2. **Test your changes:**
   ```bash
   # Run relevant tests
   ctest --test-dir build -R your_test_pattern
   
   # Or run the executable manually
   ./build/src/task <your-command>
   ```

3. **Full test before committing:**
   ```bash
   ctest --test-dir build -j $(nproc) --output-on-failure
   ```

### Using the Built Executable

```bash
# Use the built version directly
./build/src/task add "Test task"

# Or create an alias
alias task-dev='./build/src/task'

# Or add to PATH temporarily
export PATH="/workspaces/taskwarrior/build/src:$PATH"
```

## Build Artifacts

After building, you'll find:

- **Main executable:** `build/src/task`
- **Test runner:** `build/test/test_runner`
- **Other tools:** `build/src/calc`, `build/src/lex`
- **Build cache:** `build/CMakeCache.txt`
- **Compile commands:** `build/compile_commands.json` (for IDE support)

## Next Steps

Once you've successfully built and tested:

1. ✅ Verify all tests pass
2. ✅ Note any test failures (baseline)
3. ✅ Make your changes
4. ✅ Rebuild and test again
5. ✅ Compare results

## Additional Resources

- **Full development guide:** `doc/devel/contrib/development.md`
- **Contributing guide:** `CONTRIBUTING.md`
- **Installation guide:** `INSTALL`
- **Test documentation:** `test/README.md`

## Tips

1. **Use Debug build for development:**
   ```bash
   cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
   ```
   Makes debugging easier with better error messages.

2. **Use parallel builds:**
   ```bash
   cmake --build build -j $(nproc)
   ```
   Significantly faster on multi-core systems.

3. **Incremental builds:**
   CMake only rebuilds what changed, so subsequent builds are fast.

4. **Clean rebuild if needed:**
   ```bash
   rm -rf build
   cmake -S . -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo
   ```
