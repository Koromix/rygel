# Projects

Most projects are licensed under the [AGPL 3.0 license](https://www.gnu.org/licenses/agpl-3.0.html), with a few exceptions listed below.

| Project    | Description                                                                | Build tool | Quality  | License  |
| ---------- | -------------------------------------------------------------------------- | ---------- | -------- | -------- |
| *blikk*    | Embeddable beginner-friendly language with static types, fast compilation  | Felix      | **WIP**  | AGPL 3   |
| *core*     | Base C++ libraries (such as libcc) and small wrappers (R, rapidjson...)    | Felix      | **Good** | *MIT*    |
| *cnoke*    | Simple alternative to cmake.js, without any dependency                     | Node.js    | **Good** | *MIT*    |
| *drd*      | Alternative PMSI MCO classifier, subprojects: libdrd, drdc and drdR        | Felix/R    | **Good** | AGPL 3   |
| *felix*    | Small build system made specifically for this repository                   | Felix      | **Good** | AGPL 3   |
| *goupile*  | Programmable electronic data capture application                           | Felix      | **WIP**  | AGPL 3   |
| *koffi*    | Fast and simple C FFI (foreign function interface) for Node.js             | Node.js    | **Good** | *MIT*    |
| *rekord*   | Public-key backup tool with deduplication                                  | Felix      | **WIP**  | AGPL 3   |
| *snaplite* | Support tool for SQLite snapshots made from core/libsqlite                 | Felix      | **Good** | AGPL 3   |
| *thop*     | Web-based institutional PMSI (MCO) reporting tool based on libdrd          | Felix      | **Good** | AGPL 3   |
| *web*      | Reusable utility functions and HTML/CSS/JS widgets                         | Felix      | **Good** | AGPL 3   |
| *webler*   | Simple markdown-to-HTML website generator (e.g. koromix.dev)               | Felix      | **Good** | AGPL 3   |

# How to build

## C++ projects

Most projects use a dedicated build tool called felix. To get started, you need to build
this tool. You can use the bootstrap scripts at the root of the repository to bootstrap it:

* Run `./bootstrap.sh` on Linux and macOS
* Run `bootstrap.bat` on Windows

This will create a felix binary at the root of the source tree. You can then start it to
build all projects defined in *FelixBuild.ini*: `felix` on Windows or `./felix` on Linux and macOS.

The following compilers are supported: GCC, Clang and MSVC (on Windows). If you
want to build Fast or LTO builds you also need to install Node.js in order to
transpile the JS code used in some projects.

Use `./felix --help` for more information.

As of now, R packages cannot be built using this method.

## Node.js projects

Refer to each project (Koffi, etc.) documentation for instructions on how to build these projects.

## R packages

Some packages provide an Rproject file and can be built by R CMD INSTALL. Open the
project file (e.g. *src/drd/drdR/drdR.Rproj*) in RStudio and use *Install and restart* in the
Build tab.

Provided the needed dependencies are available (including Rtools and Rcpp), it should just work!

# Mono repository

I've started using a single repository for all my projects in 2018 because it is easier to manage.
There are two killers features for me:

* Cross-project refactoring
* Simplified dependency management

You can find a more detailed rationale here: https://danluu.com/monorepo/
