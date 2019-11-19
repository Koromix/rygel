![Rygel](/doc/images/rygel.jpg)

# Projects

| Project    | Description                                                                | Quality          |
| ---------- | -------------------------------------------------------------------------- | -----------------|
| _drd_      | Alternative PMSI MCO classifier, subprojects: libdrd, drdc and drdR        | Good             |
| _felix_    | Small build system made specifically for this repository                   | Good             |
| _goupile_  | Vastly simplified alternative to Voozanoo (from Epiconcept)                | Work in progress |
| _heimdall_ | Medical timeline visualization (proof-of-concept)                          | Proof-of-concept |
| _libcc_    | C++ container and system library                                           | Good             |
| _libgui_   | Simple OpenGL window creation                                              | Average          |
| _libweb_   | Reusable utility functions and HTML/CSS/JS widgets                         | Good             |
| _mael_     | Mael robot code for 'Coupe de France de Robotique 2020' competition        | Work in progress |
| _thop_     | Web-based institutional PMSI (MCO) reporting tool based on libdrd          | Good             |
| _webler_   | Simple markdown-to-HTML website generator (e.g. koromix.dev)               | Good             |
| _wrappers_ | Small C++ wrappers for library code (http, R)                              | Good             |

# Archived projects

| Project    | Description                                                                | Quality          |
| ---------- | -------------------------------------------------------------------------- | -----------------|
| _inPL_     | Dashboards and various tools for IPL's longevity checkup                   | Good             |
| _simPL_    | Medico-ecomomic modelling of IPL's longevity checkup                       | Proof-of-concept |

# Help

* [Build instructions](doc/build.md)
* [Notes on drd (rationale, drdc and drdR examples)](doc/drd.md)

# Mono repository

I've started using a single repository for all my projects a little over a year ago because it is easier to manage.
There are two killers features for me:

* Cross-project refactoring
* Simplified dependency management

You can find a more detailed rationale here: https://danluu.com/monorepo/
