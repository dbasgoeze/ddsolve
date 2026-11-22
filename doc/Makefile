UV = uv

.PHONY: all clean serve docs doxygen html

DOXYDIR  = _build/doxygen
BUILD_DIR = _build/html

doxygen:
	@mkdir -p $(DOXYDIR)/xml
	doxygen Doxyfile.xml

html: doxygen
	rm -rf .venv
	$(UV) venv .venv
	$(UV) pip install -r requirements.txt
	.venv/bin/sphinx-build -b html . $(BUILD_DIR)

docs: html

clean:
	rm -rf _build .venv

serve: html
	python3 -m http.server --directory $(BUILD_DIR) 8000

all: html
