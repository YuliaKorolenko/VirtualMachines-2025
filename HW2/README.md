### Bytecode interpreter for Lama language


#### Running the Tests

To run the tests, first compile the project into an executable.
After building, the executable should be located at:

``
./cmake-build-debug/hw2
``

Once the project is successfully compiled, run the test script:

``
python3 run_tests.py
``

My output:

```
========================================
Total tests: 75
✅ Passed: 75
❌ Failed: 0
Success rate: 100.00%
========================================
```

#### Running the performance Tests

After building, the executable should be located at:
``
./cmake-build-debug/hw2
``

To run the performance tests, run the following command:
```
bash performance.sh
```

Results:
```
[lamac -i ]
real 804.05

[lamac -s ]
real 227.95

[bytecode interpretation]
real 263.48
```
