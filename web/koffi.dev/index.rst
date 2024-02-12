Koffi
=====

Overview
--------

Koffi is a **fast and easy-to-use C FFI module for Node.js**, featuring:

* Low-overhead and fast performance (see :ref:`benchmarks<Benchmarks>`)
* Support for primitive and aggregate data types (structs and fixed-size arrays), both by reference (pointer) and by value
* Javascript functions can be used as C callbacks (since 1.2.0)
* Well-tested code base for :ref:`popular OS/architecture combinations<Supported platforms>`

Koffi requires a recent `Node.js <https://nodejs.org/>`_ version with N-API version 8 support, see :ref:`this page<Node.js>` for more information.

The source code is available here: https://github.com/Koromix/rygel/ (in the *src/koffi* subdirectory).

New releases are frequent, look at the :ref:`changelog<changelog>` for more information.

Table of contents
-----------------

.. toctree::
   :maxdepth: 3

   platforms
   start
   functions
   input
   pointers
   output
   unions
   variables
   callbacks
   misc
   packaging
   benchmarks
   contribute
   changelog
   migration

License
-------

This program is free software: you can redistribute it and/or modify it under the terms of the **MIT License**.

Find more information here: https://choosealicense.com/licenses/mit/
