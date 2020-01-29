# Projects

| Project    | Description                                                                | Quality          |
| ---------- | -------------------------------------------------------------------------- | -----------------|
| *core*     | Base C++ libraries (such as libcc) and small wrappers (R, rapidjson...)    | Good             |
| *drd*      | Alternative PMSI MCO classifier, subprojects: libdrd, drdc and drdR        | Good             |
| *felix*    | Small build system made specifically for this repository                   | Good             |
| *goupile*  | Vastly simplified alternative to Voozanoo (from Epiconcept)                | Work in progress |
| *heimdall* | Medical timeline visualization (proof-of-concept)                          | Proof-of-concept |
| *mael*     | Mael robot code for 'Coupe de France de Robotique 2020' competition        | Work in progress |
| *thop*     | Web-based institutional PMSI (MCO) reporting tool based on libdrd          | Good             |
| *web*      | Reusable utility functions and HTML/CSS/JS widgets                         | Good             |

## Archived projects

| Project    | Description                                                                | Quality          |
| ---------- | -------------------------------------------------------------------------- | -----------------|
| *inPL*     | Dashboards and various tools for IPL's longevity checkup                   | Good             |
| *simPL*    | Medico-ecomomic modelling of IPL's longevity checkup                       | Proof-of-concept |
| *webler*   | Simple markdown-to-HTML website generator (e.g. koromix.dev)               | Good             |

# How to build

This repository uses a dedicated build tool called felix. To get started, you need to
build this tool. You can use the build scripts in build/felix to bootstrap it:

* Run `build/felix/build_posix.sh` on Linux and macOS
* Run `build/felix/build_win32.bat` on Windows

This will create a felix binary at the root of the source tree. You can then start
it to build all projects defined in *FelixBuild.ini*.

Use `felix --help` for other options.

As of now, R packages cannot be built using this method.

## R packages

Some packages provide an Rproject file and can be built by R CMD INSTALL. Open the
project file (e.g. *src/drd/drdR/drdR.Rproj*) in RStudio and use *Install and restart* in the
Build tab.

Provided the needed dependencies are available (including Rtools and Rcpp), it should just work!

# Mono repository

I've started using a single repository for all my projects a little over a year ago because it is easier to manage.
There are two killers features for me:

* Cross-project refactoring
* Simplified dependency management

You can find a more detailed rationale here: https://danluu.com/monorepo/
