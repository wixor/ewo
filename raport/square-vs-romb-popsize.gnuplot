set output "square-vs-romb-popsize.tex"
set key right bottom
plot 'square-vs-romb-popsize.data' using 0:1 with lines title "50"  lw 2 lc 1, \
     'square-vs-romb-popsize.data' using 0:2 with lines title "100"  lw 2 lc 2, \
     'square-vs-romb-popsize.data' using 0:3 with lines title "200"  lw 2 lc 3, \
     'square-vs-romb-popsize.data' using 0:4 with lines title "300"  lw 2 lc 4, \
     'square-vs-romb-popsize.data' using 0:5 with lines title "400"  lw 2 lc 5, \
     'square-vs-romb-popsize.data' using 0:6 with lines title "500"  lw 2 lc 7
     
