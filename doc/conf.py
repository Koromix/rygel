# -- Project information -----------------------------------------------------

project = 'Koffi'
copyright = '2022, Niels Martignène'
author = 'Niels Martignène'
version = '1.3.0'
revision = '1.3.0-rc.1'

# -- General configuration ---------------------------------------------------

extensions = [
    'myst_parser',
    'sphinx.ext.autosectionlabel'
]

# Add any paths that contain templates here, relative to this directory.
templates_path = ['_templates']

exclude_patterns = []

# -- Options for HTML output -------------------------------------------------

html_title = 'Koffi'

html_theme = 'furo'

html_static_path = ['_static']

html_theme_options = {
    'light_css_variables': {
        'color-brand-primary': '#FF6600',
        'color-brand-content': '#FF6600'
    },
    'dark_css_variables': {
        'color-brand-primary': '#FF6600',
        'color-brand-content': '#FF6600'
    }
}

html_link_suffix = ''

html_css_files = ['custom.css']

# -- MyST parser options -------------------------------------------------

myst_enable_extensions = [
    'linkify'
]

myst_heading_anchors = 3

myst_linkify_fuzzy_links = False

myst_number_code_blocks = ['c', 'js']
