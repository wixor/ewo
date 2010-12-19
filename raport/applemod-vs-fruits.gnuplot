set output "applemod-vs-fruits.tex"
set key right bottom
set xrange [1:200]
set yrange [-40:0]
plot 'applemod-vs-fruits.data' using 0:1 with lines title "cherry"  lw 2, \
     'applemod-vs-fruits.data' using 0:3 with lines title "bananna"  lw 2, \
     'applemod-vs-fruits.data' using 0:5 with lines title "orange"  lw 2, \
     'applemod-vs-fruits.data' using 0:7 with lines title "watermelon"  lw 2, \
     'applemod-vs-fruits.data' using 0:9 with lines title "pear"  lw 2, \
     'applemod-vs-fruits.data' using 0:4 with lines title "apple"  lw 2 lc 7
