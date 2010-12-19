raport.pdf: raport.tex applemod-vs-fruits.pdf square-vs-romb-popsize.pdf square-vs-romb-deprop.pdf square-vs-romb-surv.pdf
	pdflatex raport.tex
%.pdf: %.gnuplot %.data init.gnuplot
	gnuplot init.gnuplot $< && \
	ps2pdf -dEPSCrop $(basename $<).eps $@
