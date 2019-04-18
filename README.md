![Rygel](/doc/images/rygel.jpg)

# Projects

## Working projects

* _src/drd_: Alternative PMSI classifier, with three subprojects: libdrd (classifier),
  drdc (CLI) and drdR (R package)
* _src/felix_: Small build system made specifically for this repository
* _src/heimdall_: Medical timeline visualization (proof-of-concept)
* _src/inPL_: Dashboards and various tools for IPL's longevity checkup
* _src/libcc_: C++ container and system library
* _src/libgui_: Simple OpenGL window creation
* _src/libweb_: Reusable utility functions and HTML/CSS/JS widgets
* _src/thop_: Web-based institutional PMSI reporting tool based on libdrd
* _src/webler_ : Simple markdown-to-HTML website generator (e.g. koromix.dev)
* _src/wrappers_: Small C++ wrappers for library code (http, R)

## Draft projects

* _src/goupil_: Vastly simplified alternative to Voozanoo (from Epiconcept)

# Help

* [Build instructions](doc/build.md)
* [Notes on drd (rationale, drdc and drdR examples)](doc/drd.md)

# Mono repository

I've started using a single repository for all my projects a little over
a year ago because it is easier to manage. There are two killers features for me:

* Cross-project refactoring
* Simplified dependency management

You can find a more detailed rationale here: https://danluu.com/monorepo/
