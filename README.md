<a href="https://buymeacoffee.com/koromix" target="_blank"><img src="https://cdn.buymeacoffee.com/buttons/default-orange.png" alt="Buy Me A Coffee" height="41" width="174"></a>

# Projects

Most projects are licensed under the [GPL 3.0 license](https://www.gnu.org/licenses/gpl-3.0.html), with a few exceptions listed below.

| Project    | Description                                                                      | Build tool | Quality  | License   |
| ---------- | -------------------------------------------------------------------------------- | ---------- | -------- | --------- |
| *blikk*    | Embeddable beginner-friendly language with static types, fast compilation        | Felix      | **WIP**  | GPL 3     |
| *core*     | Base C++ libraries (such as libcc) and small wrappers (R, rapidjson...)          | Felix      | **Good** | MIT       |
| *cnoke*    | Simple alternative to cmake.js, without any dependency                           | Node.js    | **Good** | MIT       |
| *drd*      | Alternative PMSI MCO classifier, subprojects: libdrd, drdc and drdR              | Felix/R    | **Good** | GPL 3     |
| *felix*    | Small build system made specifically for this repository                         | Felix      | **Good** | GPL 3     |
| *goupile*  | Programmable electronic data capture application                                 | Felix      | **WIP**  | GPL 3     |
| *heimdall* | Medical timeline visualization (proof-of-concept)                                | Felix/R    | **POC**  | AGPL 3    |
| *hodler*   | Simple markdown-to-HTML website generator (e.g. koromix.dev)                     | Felix      | **Good** | GPL 3     |
| *jumbll*   | Backup media files to multiple disjoint disks                                    | Felix      | **Good** | GPL 3     |
| *koffi*    | Fast and simple C FFI (foreign function interface) for Node.js                   | Node.js    | **Good** | MIT       |
| *ludivine* | Platform underlying the Lignes de Vie project (https://ldv-recherche.fr)         | Felix      | **Good** | GPL 3     |
| *meestic*  | Control the keyboard lighting on MSI Delta 15 laptops                            | Felix      | **Good** | GPL 3     |
| *napka*    | List and map of mental healthcare resources in France                            | Node.js    | **WIP**  | GPL 3     |
| *rekkord*  | Backup tool with deduplication and asymmetric encryption                         | Felix      | **WIP**  | GPL 3     |
| *staks*    | Simple stacker-like game for web browsers                                        | Node.js    | **Good** | GPL 3     |
| *thop*     | Web-based institutional PMSI (MCO) reporting tool based on libdrd                | Felix      | **Good** | GPL 3     |
| *tytools*  | Independent tools to manage, flash and communicate with Teensy microcontrollers  | Felix      | **Good** | Unlicense |
| *web*      | Reusable utility functions and HTML/CSS/JS widgets                               | Felix      | **Good** | MIT       |

You can also visit the [attic](src/attic/) for a few more single-file tools.

# How to build

## C++ projects

Most projects use a dedicated build tool called felix. To get started, you need to build this tool. You can use the bootstrap scripts at the root of the repository to bootstrap it:

* Run `./bootstrap.sh` on Linux and macOS
* Run `bootstrap.bat` on Windows

This will create a felix binary at the root of the source tree. You can then start it to build all projects defined in *FelixBuild.ini*: `felix` on Windows or `./felix` on Linux and macOS.

The following compilers are supported: GCC, Clang and MSVC (on Windows). If you want to build Fast or LTO builds you also need to install Node.js in order to transpile the JS code used in some projects.

Use `./felix --help` for more information.

As of now, R packages cannot be built using this method.

## Node.js projects

Refer to each project (Koffi, etc.) documentation for instructions on how to build these projects.

## R packages

Some packages provide an Rproject file and can be built by R CMD INSTALL. Open the project file (e.g. *src/drd/drdR/drdR.Rproj*) in RStudio and use *Install and restart* in the Build tab.

Provided the needed dependencies are available (including Rtools and Rcpp), it should just work!

# Code style

## Mono repository

I've started using a single repository for all my projects in 2018 because it is easier to manage. There are two killers features for me:

* Cross-project refactoring
* Simplified dependency management

You can find a more detailed rationale here: https://danluu.com/monorepo/

## C++ flavor

Most projects are programmed in C++. I don't use the standard library much (with a few exceptions) and instead rely on my own homegrown cross-platform code, containers and abstractions.

My personal preference goes to a rather C-like C++ style:

- Careful use of templates (mainly for containers)
- Little object-oriented programming, tagged unions and code locality are preferred over inheritance and virtual methods
- Use of defer-like statements for cleanup, or RAII where it feels natural
- No exceptions, errors are logged to stderr (and/or a GUI when one exists) and propagated in the code with a simple return value (bool when possible, a dedicated enum when more information is relevant)
- Memory regions are used when many allocations are needed, when building many strings for example
- Heavy use of array and string views for function arguments
- Avoid dependencies unless a strong argument can be made for one, and vendorize them aggressively (with patches if needed)

I keep watch of all dependencies using GitHub notifications, RSS feeds, mailing lists and visual web page change detection (depending on the project), in order to update them as needed.

> [!NOTE]
> This does not apply to TyTools (and libhs), which are older than the other projects, and were made before I settled on my current programming style.
