UV = uv

.PHONY: all clean serve doxygen html

DOXYDIR  = ../doxygen

doxygen:
	@mkdir -p $(DOXYDIR)/xml
	doxygen Doxyfile.xml

html: doxygen
	rm -rf .venv
	$(UV) venv .venv
	$(UV) pip install sphinx breathe sphinx-autodoc-typehints lark tqdm furo
	.venv/bin/sphinx-build -b html . _build/html

clean:
	rm -rf _build $(DOXYDIR) .venv

serve: html
	python3 -m http.server --directory _build/html 8000

all: html
