#include "image.h"
#include <stdint.h>
#include <cstdio>
#include <vector>
#include <algorithm>
#include <cmath>

using namespace std;
typedef unsigned int uint;
struct edgePoint
{
    int x, y; //współrzędne piksela na krawędzi
    int sharpness; //siła (0 - 255) kontrastu
    int angle; //kąt >w stopniach< (<90 = wierzchołek wypukły, itp)
    edgePoint() {}
    edgePoint(int nx, int ny, int nsh, int na) { x=nx; y=ny; sharpness=nsh; angle=na; }
};

bool nextToEdge(const Image& img, int x, int y, int eps)
{
    return  img[y-1][x-1]>=eps || img[y-1][x]>=eps || img[y-1][x+1]>=eps || 
            img[y][x-1]>=eps || img[y][x]>=eps || img[y][x+1]>=eps || 
            img[y+1][x-1]>=eps || img[y+1][x]>=eps || img[y+1][x+1]>=eps;
}

edgePoint findEdgeKarol(const Image& img, int x, int y, int W, int H, int epsilon, int range, int prec) // 0 <= y < H
{
    /* najpierw zmaksymalizujemy średnią wartość piksela po obu stronach prostych */
    int maxsharp = 0, maxangle = 10000;
    int sumall = 0, cntall = 0;
    for (int dy=-range; dy<=range; dy++)
        for (int dx=-range; dx<=range; dx++) if (dx != 0 || dy != 0)
            sumall += (int)img[y+dy][x+dx],
            (cntall += img[y+dy][x+dx] < epsilon ? 0 : 1);
    
    if (cntall < range || !nextToEdge(img,x,y,epsilon)) return edgePoint(x,y,0,0);
    // lecimy każdą półprostą i szukamy punktów "trochę" na prawo od niej
    
    double pi = 3.14159266;
    
    pair<double,double> tab[prec]; for (int i=0; i<prec; i++) tab[i] = make_pair(0,0);
    
    for (int tx=x-range; tx<=x+range; tx++)
        for (int ty=y-range; ty<=y+range; ty++) if (tx != x || ty != y)
        {
            if (img[ty][tx] < epsilon) continue;
            /*for (int dx=-2; dx<2; dx++)
                for (int dy=-2; dy<2; dy++) //kazdy z pikseli dzielimy na siatke 4x4
                {
                    double angle = (double)prec/pi/2.0 * (atan2((double)tx-x+(double)dx/4, (double)ty-y+(double)dy/4)+pi);
                    if (angle<0) continue;
                    int ind = (int)angle;
                    tab[ind].first += 0.0625;
                    tab[ind].second += 0.0625*img[ty][tx];
                }*/
            double angle = (double)prec/pi/2.0 * (atan2((double)tx-x, (double)ty-y)+pi);
            if (angle<0) continue;
            int ind = (int)angle;
            tab[ind].first += 1;
            tab[ind].second += img[ty][tx];
        }
    
    
    
    for (int i=0, pr = printf("  for x=%d, y=%d (all: %d) we have: ", x, y, cntall); i<prec || !printf("\n"); i++) printf("%.1lf ", tab[i].first);
    /* teraz, dla kazdego wycinka płaszczyzny prostymi mamy ilość punktów białych oraz sumę wartości */
    for (int i=0; i<prec; i++)
    {
        double tmpcnt = 0, tmpsum = 0;
        for (int j=i; j<i+prec/2; j++) //lecimy po wszystkich kątach ostrych / średnio ostrych
        {
            tmpcnt += tab[j%prec].first;
            tmpsum += tab[j%prec].second;
            if ((double)cntall - tmpcnt < 1) 
            {
                maxsharp = 255,
                maxangle = min(maxangle, (j-i+1)*180/prec);
                break;
            }
        }
    }
    if (maxsharp == 255) printf("for x=%d, y=%d angle=%d\n", x, y, maxangle);
    return edgePoint(x,y,maxsharp,maxangle);
}

vector<edgePoint> findEdgePoints(const Image &img, 
                                 Image &contrastImg,
                                 int epsilon = 20, //taka wartość różnicy między obrazkami jest uznawana za pomijalną
                                 int range = 4, //jak duże otoczenie bierzemy (2*range X 2*range)
                                 int prec = 16 //z jaką dużą precyzją wyznaczamy kąt
                                 )
{
    Image img2 = img.contour();
    contrastImg = img;
    vector<edgePoint> retvec;
    int W = img.getWidth(), H = img.getHeight();
    for (int i = range; i<H-range; i++)
        for (int j=range; j<W-range; j++)
        {
            /* tutaj się zaczyna zabawa */
            /* najpierw sposób wixora:
            liczymy ilość punktów kontrastowych, a następnie staramy się żeby wszystkie one były zawarte w pewnym półkącie */
            // ....
            /* teraz sposób mój:
            na początek maksymalizujemy różnicę średnich wartości */
            // if (i%3 != 0 || j%3 != 0) { contrastImg[i][j] = 0; continue; }
            edgePoint tmp = findEdgeKarol(img2, j, i, W, H, epsilon, range, prec); //NIE DZIALA
            retvec.push_back(tmp);
            contrastImg[i][j] = tmp.sharpness==0 ? 0 : 255 - 3*(tmp.angle-30); //tmp.sharpness > 200 ? 255-std::abs((int)tmp.angle-90)*255/180 : 0;
        }
    return retvec;
}

void eraseSingleDots(Image &img, int epsilon)
{
    int H = img.getHeight(), W = img.getWidth();
    for (int i=1; i<H-1; i++) for (int j=1; j<W-1; j++) if (img[i][j] < epsilon) img[i][j] = 0;
    for (int i=1; i<H-1; i++) 
        for (int j=1; j<W-1; j++)
        {
            if (!nextToEdge(img,j,i,epsilon))
                img[i][j] = 0;
        }
}
        

int main()
{
    int epsilon = 20;
    Image grape = Image::readPGM("gallery/grape.pgm");
    printf("opened\n");
    Image grapeC = grape.contour();
    eraseSingleDots(grapeC, epsilon);
    grapeC.writePGM("gallery/grapeC.pgm");
    Image grapeEdge(0,0);
    
    
    findEdgePoints(grape, grapeEdge, epsilon);
    grapeEdge.writePGM("gallery/grapeE.pgm");
    
    printf("W = %d, H = %d\n", grape.getWidth(), grape.getHeight());
    
    return 0;
}