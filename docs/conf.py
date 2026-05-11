project = 'Toolbelt'
author = 'Yuval Peress'
copyright = f'2026, {author}'
release = '0.0.1'
exclude_patterns = [
    '**/*bazel*',
]
extensions = [
  "sphinxcontrib.mermaid",
  "sphinxcontrib.wavedrom",
  "sphinx_design",
  "sphinx_reredirects",
  "sphinx_sitemap",
]
html_theme = 'pydata_sphinx_theme'

templates_path = ['layout']

# The name for this set of Sphinx documents.  If None, it defaults to
# "<project> v<release> documentation".
html_title = 'Toolbelt'

# Tell Sphinx where to find static files
html_static_path = ['_static']

# Remove the left sidebar
html_sidebars = {
    "**": []
}

html_theme_options = {
    "show_prev_next": False,
}

# Tell Sphinx to include our specific CSS file
html_css_files = [
  'css/toolbelt.css',
  'https://cdnjs.cloudflare.com/ajax/libs/font-awesome/5.15.4/css/all.min.css',
]

html_js_files = [
  # Do not list toolbelt.js here. This will cause it to get loaded in <head>.
  # To improve load performance we modified //docs/layout/layout.html
  # to load toolbelt.js at the end of <body> instead.
  # "js/toolbelt.js",
]

# -- Options for sphinxcontrib-mermaid --
mermaid_init_js = '''
mermaid.initialize({
  // Mermaid is manually started in //docs/_static/js/toolbelt.js.
  startOnLoad: false,
  // sequenceDiagram Note text alignment
  noteAlign: "left",
  // Set mermaid theme to the current furo theme
  theme: "base",
});
'''

mermaid_version='11.7.0'
