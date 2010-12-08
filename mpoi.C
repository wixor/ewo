#include "mpoi.h"

inline bool onBoard(int x, int y, int W, int H) { return x>=0 && x<W && y>=0 && y<H; }

PoiFinder::PoiFinder(const Array2D<float> &src, int bounding = 1) //szukaj punktów napałowo
{
    const float R0 = 20.0;
    const float minimum = 20.0;
    
    H = src.getHeight(), W = src.getWidth();
    
    Array2D<char> tabu(W, H);
    memset(tabu[0], 0, W*H);
    
    std::priority_queue<pnt> best;
    
    for (int j=bounding; j<H-bounding; j++)
        for (int i=bounding; i<W-bounding; i++) 
            best.push(pnt(i,j,src[j][i]));
    
    while (true) //kolko skalujemy od R0 do 2*R0
    {
        if (best.empty()) break;
        pnt curr = best.top(); best.pop();
        if (tabu[curr.y][curr.x]) continue;
        if (curr.val < minimum) break;
        
        // fprintf(stderr, "%d %d %f\n", curr.x, curr.y, curr.val);
        poi.push_back(curr);
        curr.val = std::max(std::min(curr.val, 0.0f), 255.0f);
        float R = R0*3.0 - 2*curr.val*R0/255.0f;
        int iR = R;
        for (int y=-iR; y<=iR; y++)
            for (int x=-iR; x<=iR; x++)
                if (x*x+y*y <= (int)R*R && onBoard(x+curr.x,y+curr.y,W,H))
                    tabu[curr.y+y][curr.x+x] = 1;
    }
}
    
Image PoiFinder::toImage(float threshold = 50.0, int howmany = 1000000000)
{
    Image ret(W, H);
    ret.fill(0);
    for (int i=0; i<poi.size() && i<howmany && poi[i].val >= threshold; i++)
    {
        int x0 = poi[i].x, y0 = poi[i].y;
        for (int x=x0-1; x<=x0+1; x++)
            for (int y=y0-1; y<=y0+1; y++)
                if (onBoard(x,y,W,H))
                    ret[y][x] = 255;
    }
    fprintf(stderr, "to image done\n");
    return ret;
}

