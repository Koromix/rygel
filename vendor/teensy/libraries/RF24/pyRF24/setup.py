#!/usr/bin/env python

import os
import sys
import setuptools
import crossunixccompiler

version = ""


def process_configparams():
    version = ""  # using python keyword `global` is bad practice

    # NOTE current repo directory structure requires the use of
    # `python3 setup.py build` and `python3 setup.py install`
    # where `pip3 install ./pyRF24` copies pyRF24 directory to
    # `tmp` folder that doesn't have the needed `../Makefile.inc`
    # NOTE can't access "../Makefile.inc" from working dir because
    # it's relative. Brute force absolute path dynamically.
    script_dir = os.path.split(os.path.abspath(os.getcwd()))[0]
    abs_file_path = os.path.join(script_dir, "Makefile.inc")
    with open(abs_file_path) as f:
        config_lines = f.read().splitlines()

    cflags = os.getenv("CFLAGS", "")
    for line in config_lines:
        identifier, value = line.split('=', 1)
        if identifier == "CPUFLAGS":
            cflags += " " + value
        elif identifier == "HEADER_DIR":
            cflags += " -I" + os.path.dirname(value)
        elif identifier == "LIB_DIR":
            cflags += " -L" + value
        elif identifier == "LIB_VERSION":
            version = value
        elif identifier in ("CC", "CXX"):
            os.environ[identifier] = value

    os.environ["CFLAGS"] = cflags
    return version

if sys.version_info >= (3,):
    BOOST_LIB = "boost_python3"
else:
    BOOST_LIB = "boost_python"

version = process_configparams()
crossunixccompiler.register()

module_RF24 = setuptools.Extension("RF24",
                                   libraries=["rf24", BOOST_LIB],
                                   sources=["pyRF24.cpp"])

setuptools.setup(name="RF24",
                 version=version,
                 ext_modules=[module_RF24])
