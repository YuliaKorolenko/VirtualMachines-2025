### Bytecode interpreter for Lama language


#### Running Tests

Regression tests are located in the `regression` directory. 

`.bc` - bytecode files
`.input` - input files for the tests
`.t `- expected output files for the tests

Performance test is located in the `performance` directory.

##### Run tests in CLion

1. Open the `HW2` folder in CLion.
2. Reload CMake project.
3. Select the CTest configuration: `Regression tests` or `Performance tests` depending on the test you want to run.
4. Press the green button.


##### Run tests manually

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
