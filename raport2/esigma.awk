{ a = 0; b = 0; n = NF-1;
  for(i=2; i<=NF; i++) { a += $(i); b += $(i)*$(i); }
  a /= n; b /= n;
  printf("%s %f %f\n", $1, a, sqrt(b - a*a));
} 
