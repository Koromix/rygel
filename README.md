# Projects

| Project    | Description                                                                | Quality          |
| ---------- | -------------------------------------------------------------------------- | -----------------|
| *drd*      | Alternative PMSI MCO classifier, subprojects: libdrd, drdc and drdR        | Good             |
| *felix*    | Small build system made specifically for this repository                   | Good             |
| *goupile*  | Vastly simplified alternative to Voozanoo (from Epiconcept)                | Work in progress |
| *heimdall* | Medical timeline visualization (proof-of-concept)                          | Proof-of-concept |
| *libcc*    | C++ container and system library                                           | Good             |
| *libgui*   | Simple OpenGL window creation                                              | Average          |
| *libweb*   | Reusable utility functions and HTML/CSS/JS widgets                         | Good             |
| *mael*     | Mael robot code for 'Coupe de France de Robotique 2020' competition        | Work in progress |
| *thop*     | Web-based institutional PMSI (MCO) reporting tool based on libdrd          | Good             |
| *webler*   | Simple markdown-to-HTML website generator (e.g. koromix.dev)               | Good             |
| *wrappers* | Small C++ wrappers for library code (http, R)                              | Good             |

# Archived projects

| Project    | Description                                                                | Quality          |
| ---------- | -------------------------------------------------------------------------- | -----------------|
| *inPL*     | Dashboards and various tools for IPL's longevity checkup                   | Good             |
| *simPL*    | Medico-ecomomic modelling of IPL's longevity checkup                       | Proof-of-concept |

# Help

* [Build instructions](doc/build.md)
* [Notes on drd (rationale, drdc and drdR examples)](doc/drd.md)

# Mono repository

I've started using a single repository for all my projects a little over a year ago because it is easier to manage.
There are two killers features for me:

* Cross-project refactoring
* Simplified dependency management

You can find a more detailed rationale here: https://danluu.com/monorepo/
