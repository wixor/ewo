reset

set terminal epslatex color solid size 16cm,5cm font "" 10
set output "graph.tex"

set format x "$%g$"

set tmargin .1
set bmargin 1
set rmargin .5
set lmargin 7
unset key

set yrange [-0.5:9.5]
set ytics ("cross" 0, "triangle1" 1, "square" 2, "clover" 3, "circle" 4, "house" 5, "balls" 6, "4star" 7, "5star" 8, "arch" 9)
set xtics 1

plot 0 ls 0, \
     1 ls 0, \
     2 ls 0, \
     3 ls 0, \
     4 ls 0, \
     5 ls 0, \
     6 ls 0, \
     7 ls 0, \
     8 ls 0, \
     9 ls 0, \
     'graph' using  2:0 pt 8 lc 0, \
     'graph' using  3:0 pt 8 lc 0, \
     'graph' using  4:0 pt 8 lc 0, \
     'graph' using  5:0 pt 8 lc 0, \
     'graph' using  6:0 pt 8 lc 0, \
     'graph' using  7:0 pt 8 lc 0, \
     'graph' using  8:0 pt 8 lc 0, \
     'graph' using  9:0 pt 8 lc 0, \
     'graph' using 10:0 pt 8 lc 0, \
     'graph' using 11:0 pt 8 lc 0
