import json
import os

# -- Project information -----------------------------------------------------

project = 'Goupile'
copyright = '2023, Niels Martignène'
author = 'Niels Martignène'
version = '3.0'

# -- General configuration ---------------------------------------------------

extensions = [
    'myst_parser',
    'sphinx.ext.autosectionlabel'
]

# Add any paths that contain templates here, relative to this directory.
templates_path = ['templates']

exclude_patterns = []

# -- Options for HTML output -------------------------------------------------

html_title = project

html_theme = 'furo'

html_static_path = ['static']

html_theme_options = {
    'light_css_variables': {
        'font-stack': 'Open Sans',
        'color-content-foreground': '#383838',
        'color-brand-primary': '#383838',
        'color-brand-content': '#e97713'
    },
    'dark_css_variables': {
        'font-stack': 'Open Sans',
        'color-content-foreground': '#ffffffdd',
        'color-brand-primary': '#ffffffdd',
        'color-brand-content': '#e97713'
    }
}

html_link_suffix = ''

html_css_files = [
    'opensans/OpenSans.css',
    'custom.css'
]

html_sidebars = {
    "**": [
        "logo.html",
        "sidebar/search.html",
        "sidebar/scroll-start.html",
        "sidebar/navigation.html",
        "sidebar/ethical-ads.html",
        "sidebar/scroll-end.html",
        "sidebar/variant-selector.html"
    ]
}

# -- MyST parser options -------------------------------------------------

myst_enable_extensions = [
    'linkify',
    'substitution',
    'strikethrough'
]

myst_heading_anchors = 3

myst_linkify_fuzzy_links = False

myst_number_code_blocks = ['c', 'js', 'sh', 'batch']
