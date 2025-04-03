# LargeVars
A LargeInt class that can hold an arbitrary size integer (or at least as large as your available memory would allow).

This was written as a challenge to myself and is not guaranteed to be useful or usable.

The `large_variables.hpp` header file contains the actual class, while `main.cpp` contains random code using the class. `self_test.cpp` contains various tests that can be ran by using `--test` when executing the program.

In writing this, I have used MSVC on Windows for testing and debugging, however, it should work with GCC and on Linux as well.
