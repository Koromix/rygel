<!-- Title: Home
     Created: 2017-01-15 -->

#tytools# TyTools

TyTools is a collection of **independent tools** and you only need one executable to use
any of them. The Qt-based GUI tools are statically compiled to make that possible.

Tool        | Type                      | Description
----------- | --------------------------------------------------------------------------------
TyCommander | Qt GUI (static)           | Upload, monitor and communicate with multiple boards
TyUpdater   | Qt GUI (static)           | Simple firmware / sketch uploader
tycmd       | Command-line<br>_No Qt !_ | Command-line tool to manage Teensy boards

Find out more on the [page dedicated to TyTools](tytools).

#libhs# C/C++ libraries

All my C/C++ libraries are distributed as **single-file headers**. Drop the file in your project,
add one define for the implementation and start to code. You don't have to mess around with arcane
build systems to use C and C++.

Library          | Description
---------------- | --------------------------------------------------------------------------------
[libhs](libhs)   | Enumerate and interact with USB serial and HID devices on Windows, OSX and Linux
