set terminal postscript
set output "graph.ps"
set yrange [0:35]
set multiplot;
plot "graph" using 2:xticlabels(1)
plot "graph" using 3:xticlabels(1)
plot "graph" using 4:xticlabels(1)
plot "graph" using 5:xticlabels(1)
plot "graph" using 6:xticlabels(1)
plot "graph" using 7:xticlabels(1)
plot "graph" using 8:xticlabels(1)
plot "graph" using 9:xticlabels(1)
unset multiplot
