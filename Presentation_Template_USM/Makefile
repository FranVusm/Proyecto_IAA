TEX := pdflatex
TEXFLAGS := -interaction=nonstopmode -halt-on-error -file-line-error
SOURCES := main.tex packages.sty config.sty $(wildcard slides/*.tex) $(wildcard images/*)

.PHONY: pdf clean

pdf: main.pdf

main.pdf: $(SOURCES)
	@command -v $(TEX) >/dev/null 2>&1 || { \
		echo 'Falta pdflatex. En Ubuntu instala: sudo apt install texlive-latex-base texlive-latex-recommended texlive-pictures texlive-lang-spanish'; \
		exit 1; \
	}
	$(TEX) $(TEXFLAGS) main.tex
	$(TEX) $(TEXFLAGS) main.tex

clean:
	rm -f main.aux main.log main.nav main.out main.snm main.toc main.vrb
