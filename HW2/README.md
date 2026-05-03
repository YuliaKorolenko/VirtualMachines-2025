## Bytecode interpreter for Lama language
This project implements a bytecode interpreter for the Lama educational programming language.

Lama specification: [lama-spec.pdf](https://github.com/PLTools/Lama/blob/1.30/lama-spec.pdf).
Lama compiler: [PLTools/Lama](https://github.com/PLTools/Lama).

### Project structure
The interpreter executes Lama stack-machine bytecode (`.bc` files).  
The original Lama runtime and garbage collector are reused from the `runtime/` directory.

```text
main.c        program entry point
vm.c          interpreter logic
stack.c       operand stack and call frame logic
bytefile.c    bytecode file loading and metadata
runtime/      Lama runtime and garbage collector
```

### Running Tests

Regression tests are located in the `regression` directory. 

`.bc` - bytecode files
`.input` - input files for the tests
`.t `- expected output files for the tests

Performance test is located in the `performance` directory.

#### Run tests in CLion

1. Open the `HW2` folder in CLion.
2. Reload CMake project.
3. Select the CTest configuration: `Regression tests` or `Performance tests` depending on the test you want to run.
4. Press the green button.


#### Run tests manually

To run the tests, first compile the project into an executable.

```
cmake -S . -B build
cmake --build build
```

Once the project is successfully compiled and `./build/HW2` is appeared, run the test script:

Run the following command to run regression tests:
``
python3 run_tests.py --exe ./build/HW2
``

Output should look like this:

```
========================================
Total tests: 75
Passed: 75
Failed: 0
Success rate: 100.00%
========================================
```

Run the following command to run performance tests:
``
bash performance.sh
``

Results:
```
[lamac -i ]
real 804.05

[lamac -s ]
real 227.95

[bytecode interpretation]
real 263.48
```

### Current limitations

1. The interpreter follows the integer representation used by the original Lama runtime.
2. The project currently supports x86-64 only. The bundled Lama runtime contains architecture-specific code and does not compile on arm64.
