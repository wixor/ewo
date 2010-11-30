#include "image.h"
#include <stdint.h>
#include <cstdio>
#include <vector>
#include <algorithm>

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

edgePoint findEdgeKarol(const Image& img, int x, int y, int W, int H, int epsilon, int range, int prec) // 0 <= y < H
{
    /* najpierw zmaksymalizujemy średnią wartość piksela po obu stronach prostych */
    int maxsharp = 0, maxangle = -1;
    int sumall = 0, cntall = (2*range+1)*(2*range+1);
    for (int dy=-range; dy<=range; dy++)
        for (int dx=-range; dx<=range; dx++)
            sumall += (uint)img[y+dy][x+dx];
    
    // return img[y][x] > epsilon ? edgePoint(x,y,255,0) : edgePoint(y,x,0,0);
    for (int L1 = -prec; L1 < prec; L1 ++) //prosta (x,y) < -- > (y+L1, x+prec-abs(L1))
        for (int L2 = L1+1; L2 < prec; L2 ++) //jak wyżej. prec odpowiada 90stopniom
        {
            /* liczymy ilość oraz, na pałę, sumę pikseli z danego obszaru */
            int sum[4] = {0,0,0,0}, cnt[4] = {0,0,0,0}; //proste dzielą to na 4 obszary
            for (int tx = x-range; tx <= x+range; tx ++) 
                for (int ty=y-range; ty <= y+range; ty++)
                {
                    uint down1 = (x-tx)*L1 < (y-ty)*(prec-abs(L1)),
                        down2 = (x-tx)*L2 < (y-ty)*(prec-abs(L2));
                    
                    // printf("  x=%d, y=%d, L1=%d, L2=%d, prec=%d. tx=%d, ty=%d: down1=%d, down2=%d\n", x,y,L1,L2,prec,tx,ty,down1,down2);
                    cnt[down1*2+down2] ++;
                    sum[down1*2+down2] += img[ty][tx]; // > epsilon ? img[ty][tx] : 0;
                }
            // printf("x=%d, y=%d, L1=%d, L2=%d. (%d,%d) (%d %d) (%d %d) (%d %d)\n", x,y,L1,L2,cnt[0],sum[0],cnt[1],sum[1],cnt[2],sum[2],cnt[3],sum[3]);
            for (int i=0; i<4; i++)
            {
                int sharp = std::abs((int)sum[i]/cnt[i] - (int)(sumall-sum[i])/(cntall-cnt[i])) * 5; // KAROL
                // int sharp = (int)Image::abs(sum[i], cntall-sum[i])*255/cntall; //WIXOR
                // int sharp = sum[i] == 0 ? 255 : (sum[i] == cntall ? 255 : 0); //WIXOR
                if (maxsharp < sharp)
                    maxsharp = sharp,
                    maxangle = (i==1||i==2 ? std::abs((int)L1-L2)*90/prec : 180-std::abs((int)L1-L2)*90/prec);
            }
        }
    
    return edgePoint(x,y,maxsharp,maxangle);
}

vector<edgePoint> findEdgePoints(const Image &img, 
                                 Image &contrastImg,
                                 int epsilon = 20, //taka wartość różnicy między obrazkami jest uznawana za pomijalną
                                 int range = 4, //jak duże otoczenie bierzemy (2*range X 2*range)
                                 int prec = 4 //z jaką dużą precyzją wyznaczamy kąt
                                 )
{
    Image img2 = img.contour();
    contrastImg = img;
    vector<edgePoint> retvec;
    int W = img.getWidth(), H = img.getHeight();
    for (int i=printf("i= %d\n", i), range; i<H-range; i++)
        for (int j=range; j<W-range; j++)
        {
            /* tutaj się zaczyna zabawa */
            /* najpierw sposób wixora: 
            liczymy ilość punktów kontrastowych, a następnie staramy się żeby wszystkie one były zawarte w pewnym półkącie */
            // ....
            /* teraz sposób mój:
            na początek maksymalizujemy różnicę średnich wartości */
            if (i%3 != 0 || j%3 != 0) { contrastImg[i][j] = 0; continue; }
            edgePoint tmp = findEdgeKarol(img, j, i, W, H, epsilon, range, prec); //NIE DZIALA
            retvec.push_back(tmp);
            contrastImg[i][j] = tmp.sharpness > 200 ? 255-std::abs((int)tmp.angle-90)*255/180 : 0;
        }
    return retvec;
}
int main()
{
    Image grape = Image::readPGM("gallery/grape.pgm");
    printf("opened\n");
    grape.contour().writePGM("gallery/grapeC.pgm");
    
    return 0;
}