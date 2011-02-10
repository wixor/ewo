raport2.pdf: raport2.tex 
	pdflatex raport2.tex
%.pdf: %.gnuplot %.data init.gnuplot
	gnuplot init.gnuplot $< && \
	ps2pdf -dEPSCrop $(basename $<).eps $@
