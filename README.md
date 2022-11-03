# Projects

| Project    | Description                                                                | Quality          |
| ---------- | -------------------------------------------------------------------------- | -----------------|
| *blikk*    | Embeddable beginner-friendly language with static types, fast compilation  | Work in progress |
| *core*     | Base C++ libraries (such as libcc) and small wrappers (R, rapidjson...)    | Good             |
| *drd*      | Alternative PMSI MCO classifier, subprojects: libdrd, drdc and drdR        | Good             |
| *felix*    | Small build system made specifically for this repository                   | Good             |
| *goupile*  | Programmable electronic data capture application                           | Work in progress |
| *heimdall* | Medical timeline visualization (proof-of-concept)                          | Proof-of-concept |
| *mael*     | Mael robot code for 'Coupe de France de Robotique 2022' competition        | Work in progress |
| *rekord*   | Public-key backup tool with deduplication                                  | Work in progress |
| *thop*     | Web-based institutional PMSI (MCO) reporting tool based on libdrd          | Good             |
| *web*      | Reusable utility functions and HTML/CSS/JS widgets                         | Good             |
| *webler*   | Simple markdown-to-HTML website generator (e.g. koromix.dev)               | Good             |

# How to build

This repository uses a dedicated build tool called felix. To get started, you need to build
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
