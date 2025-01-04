# Introduction

<div id="me">
    <img src="{{ ASSET static/niels.webp }}" alt="Photo of me" />
    <div>
        <p>Hi visitor!
        <p>My name is <b>Niels Martignène</b> (or <a href="https://github.com/Koromix" target="_blank">Koromix</a>), and I've been a programmer for over twenty years. It all began when I stumbled upon an old <a href="https://en.wikipedia.org/wiki/Amstrad_CPC#CPC6128" target="_blank">Amstrad CPC-6128</a> in my parents' attic, and found out you could use BASIC to program simple games on it.
        <p>Nowadays I work on various <b>C++ applications/libraries</b>, and some <b>web applications</b>. I also do some C and assembly programming. You can find information about some of the open-source projects I've worked on over the years on this website.
        <p>I'm <b>available for hire</b>, please contact me on <a href="mailto:niels.martignene@protonmail.com">niels.martignene@protonmail.com</a> if you are interested.
    </div>
</div>

# Main projects

## Goupile

Goupile is an **open-source electronic data capture** application that strives to make form creation and data entry both powerful and easy.

Find out more on the [page dedicated to Goupile](goupile).

## Koffi

Koffi is a **fast and easy-to-use JS to C FFI module** for [Node.js](https://nodejs.org/), featuring:

* Low-overhead and fast performance (see [benchmarks](https://koffi.dev/benchmarks))
* Support for primitive and aggregate data types (structs and fixed-size arrays), both by reference (pointer) and by value
* Javascript functions can be used as C callbacks (since 1.2.0)
* Well-tested code base for popular OS/architecture combinations

Find out more on the [page dedicated to Koffi](koffi).

## Rekkord

Rekkord is a multi-platform **backup tool with deduplication and asymmetric encryption**, with support for local and remote storage back-ends.

Find out more on the [page dedicated to Rekkord](rekkord).

## TyTools

TyTools is a collection of independent tools to manage, flash and communicate with **[Teensy](https://www.pjrc.com/teensy/) microcontrollers**.

Tool        | Type                      | Description
----------- | ------------------------- | ----------------------------------------------------
TyCommander | Qt GUI (static)           | Upload, monitor and communicate with multiple boards
TyUploader  | Qt GUI (static)           | Simple firmware / sketch uploader
tycmd       | Command-line<br>_No Qt !_ | Command-line tool to manage Teensy boards

Find out more on the [page dedicated to TyTools](tytools).

## libhs

libhs is a cross-platform C library to **enumerate HID and serial devices** and interact with them.

It is distributed as a *single-file header*. Drop the file in your project, add one define for the implementation and start to code without messing around with an arcane build system.

Find out more on the [page dedicated to libhs](libhs).

## Meestic

Meestic is a small utility made to control **the lights of MSI Delta 15 laptop keyboards** on Windows and Linux. It provides both a CLI and a system-tray GUI, and can fully replace the official MSI tool (which is not even available for Linux).

Find out more on the [page dedicated to Meestic](meestic).

# Other projects

The projects on this page are provided as-is, and are in **various states of completion**.

Project | Description | Language
------- | ----------- | --------
[Staks](misc#staks) | Simple stacker-like game for web browsers | JS
[Nestor](misc#nestor) | Nestor is a small HTTP server made for local testing with customizable headers | C++
[SeatSH](misc#seatsh) | SeatSH is a Windows service that enables you to launch graphical programs from an SSH connection on Windows 10, for automated test systems. | C++
[SaneBB](misc#sanebb) | Small (single-file) and fast BBCode parser in Java | Java
[DRD](misc#drd) | Alternative french PMSI MCO classifier<br>*Library (libdrd), command-line tool (drdc) and R package (drdR)* | C++
