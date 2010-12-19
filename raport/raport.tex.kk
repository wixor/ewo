\documentclass[10pt,a4paper,leqno]{article}

\usepackage[polish]{babel}
\usepackage[utf8]{inputenc}
\usepackage[T1]{fontenc}
\usepackage{a4wide}
\usepackage{caption}
\usepackage{graphicx}
\usepackage{polski}
\usepackage{listings}

\lstset{language=Pascal}

\title{
    Projekt z algorytmów ewolucyjnych
}
\author{Wiktor Janas oraz Karol Konaszyński}

\begin{document}

\maketitle

\section{Abstrakt}

Hasłem przewodnim naszej pracy jest: Zastosowanie algorytmów ewolucyjnych do rozpoznawania obrazów. 
Problem rozpoznawania obrazów jest niezmiernie powszechnie spotykany; od analizy zdjęć satelitarnych poprzez monitorowania ruchu drogowego, po przemysł filmowy. 
Z drugiej strony jednak stanowi poważne wyzwanie dla informatyki, gdyż okazuje się niezwykle trudnym z powodu sposobu, 
w jaki ludzki mózg przetwarza i rozpoznaje obrazy. Przede wszystkim, wyzwaniem jest jednak precyzyjne jego sformułowanie; 
chodzi mianowicie o pojmowanie słowa "rozpoznawanie". Nasza interpretacja niniejszego zagadnienia polega na rozstrzygnięciu, 
do jakiego obiektu "podobny" jest kształtem przedmiot znajdujący się na obrazie. 
Przy tym rozpatrujemy tylko obrazy czarnobiałe. Tutaj ograniczymy się do takiego sformułowania, zostawiając słowo "podobny" jako określenie intuicyjne. 

\section{Opis podejścia do problemu}
Przyjęliśmy następujący schemat. Mamy daną bazę obrazów (obrazy "bazowe") o zadanych nazwach (to znaczy, że wiemy, co przedstawiają) 
i dla każdego obrazka, który chcemy rozpoznać ("zapytania"), przyrównujemy do niego wszystkie obrazków bazowych i znajdujemy ich odległość. 
Następnie spośród otrzymanych odległości, wybieramy najmniejszą i odpowiedzią dla tego zapytania jest nazwa obrazka minimalizującego tę odległość.
W tym akapicie skupimy się na szczegółach powyższego opisu.

\subsection{Znajdowanie punktów charakterystycznych}
Pierwszym etapem przetwarzania obrazka jest znalezienie na nim punktów charakterystycznych. Dla każdego punktu liczymy jego ``miarę ciekawości''.
Mianowicie, dla danej skali d i kierunku $\alpha$ wyznaczamy gradient tego punktu w tę stronę; 
intuicyjnie, jest to różnica średnich jasności otoczenia tego piksela i jego otoczenia przesuniętego o d w stronę $\alpha$. 
Formalniej, niech $avg(x,y,d)$ oznacza średnią jasność obrazu w kwadracie o środku w (x,y) i boku długości d. Wówczas:
\[
grad(x,y,d,\alpha) = avg(x,y,d) - avg(x+sin(\alpha)d, y+cos(\alpha)d, d)
\]
Dla danego kierunku, wyznaczamy średnią geometryczną gradientów dla określonego wektora skal. W naszej implementacji użyliśmy skal $d = 1, 3, 8$, 
zatem 
\[
grad(x,y,\alpha ) = \sqrt[3]{grad(x,y,1,\alpha )grad(x,y,3,\alpha )grad(x,y,8,\alpha )}
\]
[rysunek: dwa kwadraty na polu pikseli]
W ten sposób dostajemy, dla danego piksela, funkcję gradientu od kąta i badamy jej amplitudę. Jej wysokość to nasza szukana "miara ciekawości".
Interesującą obserwacją jest to, że dla skali równej 1 funkcja którą otrzymujemy jest sklejeniem kilku funkcji polisinusoidalnych.
Heurystyka stojąca za tym algorytmem jest prosta - szukamy punktów, w których obszar (w miarę) jednokolorowy przechodzi w obszar o innym kolorze. 
Amplituda funkcji gradientu mówi właśnie to, jak w otoczeniu piksela zmienia się kolor. 
Należy jednak uwzględnić to, że niektóre zmiany sś jedynie lokalne. Aby więc wyeleminować "szum", 
liczymy tę funkcję dla dwóch skal i mnożymy - jeżeli istotnie, zachodzi globalna zmiana koloru, nastąpi rezonans, 
gdy natomiast zmiana jest lokalna - funkcja większej skali ją "wyciszy". Według naszej całkowicie subiektywnej oceny skale 1,3,8 spradzają się najlepiej.
To jednak nie jest koniec. Chcemy bowiem mieć coś więcej - określoną liczbę punktów charakterystycznych, 
które będą opisywały nam kształt obiektu. Liczba ta jest parametrem alogorytmu, najczęściej przyjmowaliśmy 50. Musimy zatem wybrać ten zbiór spośród wszystkich pikseli obrazka. 
Do tego celu zastosowaliśmy następujący algorytm: w każdym kolejnym kroku wybieramy najciekawszy piksel na obrazku i dodajemy go do zbioru punktów interesujących.
Nastepnie jego otoczenie oznaczamy jako tabu i powtarzamy procedurę tak długo, aż wszystkie punkty zostaną tabu. 
Aby określić wielkość tabu, zastosowaliśmy heurystykę: ``im ciekawiej tym gęściej''. Mianowicie, 
wielkość tabu jest odwrotnie proporcjonalna do miary ciekawości punktu z pewnym współczynnikiem, który jest dobierany (na przykład za pomocą wyszukiwania binarnego)
tak, aby ostateczny zbiór punktów miał określoną wielkość.
[rysunek: Przykłady rezultatów wyszukiwania interesujących punktów dla pewnych obrazków]

\subsection{Zastosowanie ewolucji i kodowanie osobników}
Mając już punkty charakterystyczne, możemy zająć się badaniem podobienstwa obrazków. Mówimy, że obrazki są równoważne, jeżeli istnieje przekształcenie afiniczne 
przenoszące jeden na drugi. Miarą podobieństwa dwóch zbiorów punktów infimum, po wszystkich przekształceniach,
sumy ``odległości`` jednego z nich od przekształconego drugiego oraz w drugą stronę. 
Odległość natomiast dwóch zbiorów definiujemy jako średnią odległość pomiędzy punktem z pierwszego zbioru a najbliższym mu punktem z drugiego, Formalniej,
\[
similar(\bar{A}, \bar{B}) = inf\{dist(\bar{A} - M\bar{B})\ |\ M:\ afiniczne\}
\]gdzie:
\[
dist(\{a_1, ..., a_m\}, \{b_1, ..., b_n\}) = \frac{\sum_{i=1}^m min_{j=1..n}(||a_i - b_j||)}{m} + \frac{\sum_{i=1}^n min_{j=1..m}(||b_i - a_j||)}{n}
\]
Jednak aby znaleźć wielkość $similar$ musimy znaleźć przekształcenie minimalizujące odległość. I tutaj zastosujemy ewolucję. 
Naszymi osobnikami będą macierze afiniczne, czyli macierze postaci:
$\left( \begin{array}{ccc}
\star & \star & \star \\
\star & \star & \star \\
0 & 0 & 1
\end{array} \right)$, gdzie drugi minor główny jest złożeniem obrotu i skalowania, natomiast ostatnia kolumna odpowiada za translację.
Funkcją celu jest zatem 
\[
target = -dist(\bar{q}, M\bar{b})
\]
gdzie $\bar{q}$ jest wektorem punktów charakterystycznych obrazka z zapytania, zaś $\bar{b}$ - wektorem tychże dla obrazka z bazy.
Celem ewolucji jest zmaksymalizowanie funkcji celu, czyli znalezienie przekształcenia minimalizującego odległość.

\subsection{Ewolucja}
Do niniejszego problemu wykorzystaliśmy ideę Differential Evolution. Pseudokod funkcji wykonującej ewolucję wygląda następująco:
\begin{lstlisting}
function evolve:
P <- random_affine_matrix_population(P, N)
while (not termination_condition)
  population_evaluation(P)
  replace(P, N, 1-survival_rate, de_prop)
  mutation(P, N, t_prop, r_prop, s_prop)

function random_affine_matrix_population
P <- {}
for i in 1 .. N 
  P <- P + random_translation*random_rotation*random_scaling
return P

function population_evaluation
for i in 1 .. N
  evaluate(P[i])
sortuj(P, functon target) 

function replace
remove_worst(P, N*replace_rate)
for i in 1 .. N*replace_rate
  if (random(de_prop))
    P <- P + de_crossover(P)
  else
    P <- P + roulette_selection(P)

function de_crossover
  x1 <- roulette_selection(P)
  x2 <- roulette_selection(P); x1 != x2
  x3 <- roulette_selection(P); x3 != x1 AND (x3 better than x2)
  return x1 + (x3-x2) * de_factor

function mutation
for i in 1 .. N
  if (random(t_prop))
    P[i] <- random_translation * P[i]
  if (random(s_prop))
    P[i] <- random_scaling * P[i]
  if (random(r_prop))
    P[i] <- random_rotation * P[i]

function termination_condition
if (generation_index > G OR 
  max_target(generation_index, generation_index-K) >= 
  max_target(generation_index-K, generation_index-2*K)) 
    return true
else return false

\end{lstlisting}

Skomentuję krótko działanie powyższych funkcji. Najpierw odbywa się losowa inicjacja populacji z parametrem N. Następnie wchodzimy w pętlę ewolucji,
która wykonuje się aż nie zostanie spełniony warunek zakończenia.
W pętli, pierwszą rzeczą jest ewaluacja każdego osobnika, czyli znalezienie dla niego wartości $target$. 
Potem następuje zastąpienie części populacji przez osobniki bądź otrzymane w wyniku krzyżowania DE (z określonym jako parametr prawdopodobieństwem),
bądź wybrane na zasadzie ruletki z całości populacji (zjawisko generational gap).
Na końcu działania ciała pętli następuje mutacja każdego osobnika, czyli przemnożenie go (z lewej strony) przez macierz losowego przesunięcia, 
skalowania bądź obrotu, z prawdopodobieństwami określonymi jako globalne parametry algorytmu.
Warunkiem zakończenia jest natomiast przekroczenie pewnej ilości pokoleń (parametr algorytmu, zwykle rzędu 200) bądź stwierdzeniem, 
że w czasie ostatnich K pokoleń nie nastąpiła poprawa w stosunku do poprzednich K pokoleń (K jest również parametrem). Precyzyjniej, najlepszy osobnik z ostatnich 
K pokoleń okazał się nielepszy niż najlepszy z poprzednich K pokoleń.

Skomentuję jeszcze dwa miejsca w tym algorytmie.
Krzyżowanie DE polega na wybraniu, na zasadzie ruletki, trzech różnych osobników $x_1, x_2, x_3$ a następnie utworzeniu z nich osobnika postaci
$x1 + de\_factor(x3-x2)$, gdzie de\_factor jest także parametrem, zaś operacja dodawania i mnożenia jest zwykłą operacją dodawania macierzy 
i mnożenia ich przez skalar.
Ważną obserwacją którą poczyniliśmy jest to, że nie warto dokonywać losowych obrotów wokół (domyślnie) punktu (0,0), 
gdyż to spowoduje duże zmiany w położeniu punktów oddalonych od środka układu współrzędnych. Aby tego uniknąć, obroty i skalowania wykonujemy względem punktu wybranego z małego otoczenia
mediany zbioru punktów; konkretniej, względem punktu $(median(\bar{X}), median(\bar{Y}))$, gdzie $\bar{X}, \bar{Y}$ są zbiorami współrzędnych x, y punktów ze zbioru.






















\end{document}
