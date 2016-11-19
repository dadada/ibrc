FILES := $(basename $(shell find $(SOURCEDIR) -name '*.tex'))

.PHONY: all clean $(FILES) pdf ps

all: $(FILES)

clean: 
	rm -f *.aux *.blg *.out *.bbl *.log *.snm *.toc *.nav *.pdf *.ps *.dvi

# all files as PDF and PostScript
$(FILES): % : %.pdf %.ps

# all PDFs
pdf: $(addsuffix .pdf, $(FILES))

# all PostScripts
ps: $(addsuffix .ps, $(FILES))

# generic targets
%.pdf: %.tex
	pdflatex $^

%.ps: %.dvi
	dvips $^

%.dvi: %.tex
	latex $^

%.bbl: %.tex
	pdflatex $^
	bibtex $^

# a specific pdf with bibtex references
# <some_file>.pdf: <some_file>.bbl
# 	pdflatex $^
# 	pdflatex $^
