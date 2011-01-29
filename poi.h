#ifndef __POI_H__
#define __POI_H__

#include <cmath>
#include <cassert>
#include <vector>

#include "util.h"
#include "image.h"

struct POI : public Point
{
    float val;

    inline POI() { }
    inline POI(float x, float y, float val) : Point(x,y), val(val) { }
    inline POI(const Point &p, float val) : Point(p), val(val) { }
    inline POI(const POI &p) : Point(p.x, p.y), val(p.val) { }

    inline bool operator<(const POI &p) const {
        return val != p.val ? val > p.val : (Point)(*this) < (Point)p;
    } 
};

inline POI operator*(const Matrix &M, const POI &p) {
    return POI(M * (Point)p, p.val);
}

typedef std::vector<POI> POIvec;

class POIFinder
{
public:
    const Image *src;

    std::vector<float> scales;
    int steps;

    float threshold;
    int count;
    float tabuScale;

    Array2D<float> eval;
    POIvec all, selected;

    void evaluate();
    Image visualize();
    void getAll();
    void filter();
    void select();

    void run(void);
};

/* ----------------------------------------------------------------------- */

/* 
 * the ProximityMap universe accelerator
 *
 * we would like this to be a simple structure holding, for each pixel,
 * indices of all pois in increasing order of distance. there are two
 * problems with that:
 *   - there can be very many pois, so we can't store all of them.
 *     this is solved by storing only 'entries' of them.
 *   - the queries are made with non-integer coordinates.
 *     this is solved by subdividing pixels, 'detail' times in
 *     horizontal and vertical direction.
 * 
 * accounting for those considerations, the overall structure size is
 *   ( width * detail ) * ( height * detail ) * entries
 *
 * the structure is filled-in by using something similar to dijkstra's
 * algorithm. it is hoped, that it is correct (looks good). however,
 * filling speed is not very good. because of this, values of
 * 'entries' and (most importantly) 'detail' cannot be very large.
 * actually any 'detail' > 2 is of no practical value. value of
 * 'entries' can be kept around 10. 
 *
 * to support the pixel subdivision, there are two coordinate systems.
 * one, used internally, asks for an array of size widet x hedet, 
 * where widet = width*detail and hedet = height*detail. the array is
 * indexed using integer numbers called 'xi' and 'yi'. the user-visible
 * interface simulates array of size width x height, that can be accessed
 * using floating-point coordinates called 'x' and 'y'
 */

class ProximityMap
{
public:
    typedef unsigned short poiid_t;

private:
    int width, height, detail, entries;
    poiid_t *data;
    int npois;
   
    /* those are used only during map's building.
     * the array is freed after use */ 
    Array2D<poiid_t> usedEntries;
    int widet, hedet;

    /* the thing that you put on heap in dijkstra's algoritm */
    struct djk {
        float dist; /* distance of coordinates from poi */
        int xi, yi; /* the coordinates */
        poiid_t id; /* which poi's area is this */

        inline djk() { }
        inline djk(float dist, int xi, int yi, poiid_t id)
            : dist(dist), xi(xi), yi(yi), id(id) { }

        inline bool operator<(const djk &o) const {
            return dist != o.dist ? dist > o.dist :
                   id   != o.id   ? id   > o.id :
                   xi   != o.xi   ? xi   > o.xi :
                                    yi   > o.yi;
        }
    };
    
    /* helpers to build() */
    void pushEntry(const djk &o);
    bool canVisit(const djk &o) const;
    
public:
    ProximityMap();
    ~ProximityMap();

    void resize(int width, int height, int detail, int entries);
    void build(const POIvec &pois);

    inline int getWidth() const { return width; }
    inline int getHeight() const { return height; }
    inline int getDetail() const { return detail; }
    inline int getEntries() const { return entries; }
    inline int getNPois() const { return npois; }

    ColorImage visualize() const;

    /* this is internal array access interface.
     * _at(xi,yi) is an array if 'entries' poi ids.
     * this is exposed as public *only* for the caching module. */
    inline poiid_t *_at(int xi, int yi) {
        //assert(0 <= xi && xi < widet && 0 <= yi && yi <= hedet);
        return data + widet*entries*yi + entries*xi;
    }
    inline const poiid_t *_at(int xi, int yi) const {
        //assert(0 <= xi && xi < widet && 0 <= yi && yi <= hedet);
        return data + widet*entries*yi + entries*xi;
    }

    /* this is the official access interface.
     * at(x,y) is an array if 'entries' poi ids. */
    inline poiid_t *at(float x, float y) {
        return _at(roundf(x * detail), roundf(y * detail));
    }
    inline const poiid_t *at(float x, float y) const {
        return _at(roundf(x * detail), roundf(y * detail));
    }
};

#endif

