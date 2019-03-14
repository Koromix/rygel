^title System Class

The System class is a grab-bag of functionality exposed by the VM, mostly for
use during development or debugging.

## Static Methods

### System.**clock**

Returns the number of seconds (including fractional seconds) since the program
was started. This is usually used for benchmarking.

### System.**gc**()

Requests that the VM perform an immediate garbage collection to free unused
memory.

### System.**print**()

Prints a single newline to the console.

### System.**print**(object)

Prints `object` to the console followed by a newline. If not already a string,
the object is converted to a string by calling `toString` on it.

    :::wren
    System.print("I like bananas") //> I like bananas

### System.**printAll**(sequence)

Iterates over `sequence` and prints each element, then prints a single newline
at the end. Each element is converted to a string by calling `toString` on it.

    :::wren
    System.printAll([1, [2, 3], 4]) //> 1[2, 3]4

### System.**write**(object)

Prints a single value to the console, but does not print a newline character
afterwards. Converts the value to a string by calling `toString` on it.

    :::wren
    System.write(4 + 5) //> 9

In the above example, the result of `4 + 5` is printed, and then the prompt is
printed on the same line because no newline character was printed afterwards.
