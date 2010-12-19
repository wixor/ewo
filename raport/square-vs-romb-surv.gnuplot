set output "square-vs-romb-surv.tex"
set key right bottom
plot 'square-vs-romb-surv.data' using 0:1 with lines title "40\\\%"  lw 2 lc 1, \
     'square-vs-romb-surv.data' using 0:2 with lines title "50\\\%"  lw 2 lc 2, \
     'square-vs-romb-surv.data' using 0:3 with lines title "60\\\%"  lw 2 lc 3, \
     'square-vs-romb-surv.data' using 0:4 with lines title "70\\\%"  lw 2 lc 4, \
     'square-vs-romb-surv.data' using 0:5 with lines title "80\\\%"  lw 2 lc 5, \
     'square-vs-romb-surv.data' using 0:6 with lines title "90\\\%"  lw 2 lc 7
     
