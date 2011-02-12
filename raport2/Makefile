raport2.pdf: raport2.tex results-1/graph results-1/graph.gnuplot results-2/graph results-2/graph.gnuplot results-3/graph results-3/graph.gnuplot
	(cd results-1; gnuplot graph.gnuplot && ps2pdf -dEPSCrop graph.eps graph.pdf && sed -i 's/{graph}/{.\/results-1\/graph}/' graph.tex)
	(cd results-2; gnuplot graph.gnuplot && ps2pdf -dEPSCrop graph.eps graph.pdf && sed -i 's/{graph}/{.\/results-1\/graph}/' graph.tex)
	(cd results-3; gnuplot graph.gnuplot && ps2pdf -dEPSCrop graph.eps graph.pdf && sed -i 's/{graph}/{.\/results-1\/graph}/' graph.tex)
	pdflatex raport2.tex && pdflatex raport2.tex
