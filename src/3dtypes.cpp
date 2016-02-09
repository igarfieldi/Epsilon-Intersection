#include "ei/3dtypes.hpp"
#include "ei/3dintersection.hpp"

namespace ei {

    // ********************************************************************* //
    Sphere::Sphere( const Vec3& _p0, const Vec3& _p1, const Vec3& _p2 )
    {
        // The center of the circumscribed circle is at (barycentric coordinates)
        // v0*sin(2 alpha) + v1*sin(2 beta) + v2*sin(2 gamma) and has the radius
        // abc/4A.
        Vec3 c = _p0 - _p1;	float csq = lensq(c);
        Vec3 a = _p1 - _p2;	float asq = lensq(a);
        Vec3 b = _p2 - _p0;	float bsq = lensq(b);

        // One of the sides could be the longest side - the minimum sphere is
        // defined through only two points.
        // This can also handle the coplanar case.
        if( csq + bsq <= asq ) *this = Sphere(_p1, _p2);
        else if( asq + bsq <= csq ) *this = Sphere(_p1, _p0);
        else if( asq + csq <= bsq ) *this = Sphere(_p2, _p0);
        else {
            float area2Sq = 2*lensq(cross(a, c));
            center = 
                     _p0 * (-dot(c,b)*asq/area2Sq)
                   + _p1 * (-dot(c,a)*bsq/area2Sq)
                   + _p2 * (-dot(b,a)*csq/area2Sq);
            radius = sqrt(asq*bsq*csq/(2*area2Sq));
        }
    }

    // ********************************************************************* //
    Sphere::Sphere( const Vec3& _p0, const Vec3& _p1, const Vec3& _p2, const Vec3& _p3 )
    {
        // It is possible that not all 4 points lie on the surface of the sphere.
        // Just two of them could already define a sphere enclosing all other.
        // So we need to compute any combination of possible spheres (14), but
        // luckily we know a direct solution for any combination of 3 points.
        // This reduces the work to 4 cases: build a bounding sphere for 3 points
        // and have a look if the fourth point is inside.
        *this = Sphere( _p1, _p2, _p3 );
        float rsq = radius*radius;
        if( lensq(_p3 - center) > rsq ) {
            *this = Sphere( _p0, _p1, _p3 );
            if( lensq(_p2 - center) > rsq ) {
                *this = Sphere( _p0, _p2, _p3 );
                if( lensq(_p1 - center) > rsq ) {
                    *this = Sphere( _p1, _p2, _p3 );
                    if( lensq(_p0 - center) > rsq ) {
                        // All 4 points are on the boundary -> construct sphere
                        // from 4 points.
                        Vec3 a = _p1 - _p0;
                        Vec3 b = _p2 - _p0;
                        Vec3 c = _p3 - _p0;

                        Mat3x3 m = Mat3x3( _p1[0], _p1[1], _p1[2],
                                           _p2[0], _p2[1], _p2[2],
                                           _p3[0], _p3[1], _p3[2] );

                        float denominator = 0.5f / determinant( m );

                        Vec3 o = (lensq(c) * cross(a,b) +
                                  lensq(b) * cross(c,a) +
                                  lensq(a) * cross(b,c)) * denominator;

                        center = _p0 + o;
                        radius = len(o);
                    }
                }
            }
        }
    }

    // ********************************************************************* //
    struct SingleLinkedPointList
    {
        Vec3 p;
        int next;   ///< -1 for the last element
    };

    static Sphere minimalBoundingSphere( SingleLinkedPointList* _points, uint32 _first, uint32 _n, uint32 _boundarySet )
    {
        Sphere mbs;
        eiAssertWeak(_boundarySet > 0, "Expected at least one point.");

        // If the boundary list is full or all points where added stop
        switch(_boundarySet)
        {
        case 1: mbs = Sphere(_points[_first].p, 0.0f);
            break;
        case 2: {
            Vec3 v0 = _points[_first].p; uint32 next = _points[_first].next;
            mbs = Sphere(v0, _points[next].p);
            break;
        }
        case 3: {
            Vec3 v0 = _points[_first].p; uint32 next = _points[_first].next;
            Vec3 v1 = _points[next].p; next = _points[next].next;
            mbs = Sphere(v0, v1, _points[next].p);
            break;
        }
        case 4: {
            Vec3 v0 = _points[_first].p; uint32 next = _points[_first].next;
            Vec3 v1 = _points[next].p; next = _points[next].next;
            Vec3 v2 = _points[next].p; next = _points[next].next;
            return Sphere(v0, v1, v2, _points[next].p);
        }
        }

        uint32 it = _first;
        uint32 last = _first; // At the beginning this is wrong but does not cause damage, afterwards it will be ovewritten by real numbers
        for(uint32 i = 0; i < _boundarySet; ++i) {last = it; it = _points[it].next;}
        for(uint32 i = _boundarySet; i < _n; ++i)
        {
            // Save next pointer to advance from this point even if the list is changed
            uint32 next = _points[it].next;
            eiAssert(it != 0xffffffff, "Iteration should not have stopped.");
            if(lensq(mbs.center - _points[it].p) > mbs.radius * mbs.radius)
            {
                // Move to front
                _points[last].next = _points[it].next;
                _points[it].next = _first;
                _first = it;
                // Rebuild the first i elements
                mbs = minimalBoundingSphere(_points, it, i, _boundarySet+1);
            }
            last = it;
            it = next;
        }

        return mbs;
    }

    Sphere::Sphere( const Vec3* _points, uint32 _numPoints )
    {
        eiAssert( _numPoints > 0, "The point list must have at least one point." );
        // Create a single linked list for the move to front heuristic
        SingleLinkedPointList* list = new SingleLinkedPointList[_numPoints];
        for(uint32 i = 0; i < _numPoints; ++i) {
            list[i].p = _points[i];
            list[i].next = i+1;
        }
        list[_numPoints-1].next = -1;
        *this = minimalBoundingSphere(list, 0, _numPoints, 1);
        delete[] list;
    }

    // ********************************************************************* //
    Box::Box( const OBox& _box )
    {
        // Effectively generate all 8 corners and find min/max coordinates.
        // Relative to the center two diagonal opposite corners only differ
        // in the sign (even after rotation).
        Vec3 diag = _box.sides * 0.5f;
        Vec3 trDiag;
        Mat3x3 rot(_box.orientation);
        trDiag = rot * Vec3(diag.x, diag.y, diag.z);
        min = ei::min(trDiag, -trDiag);
        max = ei::max(trDiag, -trDiag);
        trDiag = rot * Vec3(diag.x, diag.y, -diag.z);
        min = ei::min(trDiag, -trDiag, min);
        max = ei::max(trDiag, -trDiag, max);
        trDiag = rot * Vec3(diag.x, -diag.y, diag.z);
        min = ei::min(trDiag, -trDiag, min);
        max = ei::max(trDiag, -trDiag, max);
        trDiag = rot * Vec3(diag.x, -diag.y, -diag.z);
        min = ei::min(trDiag, -trDiag, min);
        max = ei::max(trDiag, -trDiag, max);
        min += _box.center;
        max += _box.center;
    }

    // ********************************************************************* //
    Box::Box( const Vec3* _points, uint32 _numPoints )
    {
        eiAssert( _numPoints > 0, "The point list must have at least one point." );
        min = max = *_points++;
        for( uint32 i = 1; i < _numPoints; ++i, ++_points )
        {
            min = ei::min(min, *_points);
            max = ei::max(max, *_points);
        }
    }

    // ********************************************************************* //
    OBox::OBox( const Quaternion& _orientation, const Box& _box ) :
        center((_box.min + _box.max) * 0.5f),
        orientation(_orientation)
    {
        // Project corner points to the cube sides by transforming them into
        // local space, such that the box is axis aligned again.
        Mat3x3 rotation(_orientation);
        // Since we already know the center we only need to track one extremal
        // point to find the side length.
        Vec3 bmin = _box.min - center;
        sides = abs(rotation * bmin);
        sides = max(sides, abs(rotation * Vec3(bmin.x,  bmin.y, -bmin.z)));
        sides = max(sides, abs(rotation * Vec3(bmin.x, -bmin.y,  bmin.z)));
        sides = max(sides, abs(rotation * Vec3(bmin.x, -bmin.y, -bmin.z)));
        sides *= 2.0f;
    }

    // ********************************************************************* //
    OBox::OBox( const Mat3x3& _orientation, const Box& _box ) :
        center((_box.min + _box.max) * 0.5f),
        orientation(_orientation)
    {
        // Project corner points to the cube sides by transforming them into
        // local space, such that the box is axis aligned again.
        // Since we already know the center we only need to track one extremal
        // point to find the side length.
        Vec3 bmin = _box.min - center;
        sides = abs(_orientation * bmin);
        sides = max(sides, abs(_orientation * Vec3(bmin.x,  bmin.y, -bmin.z)));
        sides = max(sides, abs(_orientation * Vec3(bmin.x, -bmin.y,  bmin.z)));
        sides = max(sides, abs(_orientation * Vec3(bmin.x, -bmin.y, -bmin.z)));
        sides *= 2.0f;
    }

    // ********************************************************************* //
    OBox::OBox( const Quaternion& _orientation, const Vec3* _points, uint32 _numPoints ) :
        orientation(_orientation)
    {
        eiAssert( _numPoints > 0, "The point list must have at least one point." );

        // Project all points to the cube sides by transforming them into local
        // space, such that the box is axis aligned again.
        Mat3x3 rotation(_orientation);
        Vec3 min, max;
        min = max = rotation * _points[0];
        for(uint32 i = 1; i < _numPoints; ++i)
        {
            Vec3 p = rotation * _points[i];
            min = ei::min(min, p);
            max = ei::max(max, p);
        }

        // Center known with respect to local rotation, go back to world space.
        center = transform((min + max) * 0.5f, conjugate(_orientation));
        sides = max - min;
    }

    // ********************************************************************* //
    OBox::OBox( const Vec3* _points, uint32 _numPoints )
    {
        if(_numPoints == 1)
        {
            sides = Vec3(0.0f);
            center = *_points;
            orientation = qidentity();
        } else if(_numPoints == 2) {
            Vec3 connection = _points[1] - _points[0];
            sides = Vec3(len(connection), 0.0f, 0.0f);
            orientation = Quaternion(connection/sides.x, Vec3(1.0f, 0.0f, 0.0f));
            center = _points[0] + 0.5f * connection;
        } else {
            sides = Vec3(1e30f);
            Quaternion testOrientation;
            // Try each combination of three vertices to setup an orientation
            for(uint32 i = 0; i < _numPoints-2; ++i)
            {
                for(uint32 j = i+1; j < _numPoints-1; ++j)
                {
                    Vec3 xAxis = normalize(_points[i] - _points[j]);
                    for(uint32 k = j+1; k < _numPoints; ++k)
                    {
                        Vec3 yAxis = cross(xAxis, _points[i] - _points[k]);
                        float l = len(yAxis);
                        if( l < 1e-6f ) testOrientation = Quaternion(xAxis, Vec3(1.0f, 0.0f, 0.0f)); // Colinear points
                        else {
                            yAxis /= l;
                            testOrientation = Quaternion(xAxis, yAxis, cross(xAxis, yAxis));
                        }
                        OBox currentFit(testOrientation, _points, _numPoints);
                        // If the new box is better than the current copy it.
                        if(prod(currentFit.sides) < prod(sides))
                            *this = currentFit;
                    }
                }
            }
        }
    }

    // ********************************************************************* //
    FastFrustum::FastFrustum(const Frustum& _frustum)
    {
        // Initialization of planes is difficult in the list, so the const-cast
        // only defers the initialization a bit.

        // Compute all 8 vertices (first get more help vectors).
        Vec3* v = const_cast<Vec3*>(vertices);
        Vec3 far = _frustum.f * _frustum.direction + _frustum.apex;
        Vec3 near = _frustum.n * _frustum.direction + _frustum.apex;
        // Get third axis and central off point
        Vec3 xAxis = cross(_frustum.up, _frustum.direction);
        Vec3 bottom = _frustum.b*_frustum.up;
        Vec3 top = _frustum.t*_frustum.up;
        Vec3 left =  _frustum.l * xAxis;
        Vec3 right =  _frustum.r * xAxis;
        float fton = _frustum.n / _frustum.f;
        v[0] = near + (left  + bottom) * fton;
        v[1] = near + (left  + top) * fton;
        v[2] = near + (right + bottom) * fton;
        v[3] = near + (right + top) * fton;
        v[4] = far  + left  + bottom;
        v[5] = far  + left  + top;
        v[6] = far  + right + bottom;
        v[7] = far  + right + top;

        // Create planes
        const_cast<DOP&>(nf) = DOP(_frustum.direction, near, far);
        // Use two vectors in the planes to derive the normal.
        const_cast<Plane&>(l) = Plane(normalize(cross(_frustum.up, v[4]-_frustum.apex)), v[4]);
        const_cast<Plane&>(r) = Plane(normalize(cross(v[7]-_frustum.apex, _frustum.up)), v[7]);
        const_cast<Plane&>(b) = Plane(normalize(cross(v[4]-_frustum.apex, xAxis)), v[4]);
        const_cast<Plane&>(t) = Plane(normalize(cross(xAxis, v[7]-_frustum.apex)), v[7]);
    }

    // ********************************************************************* //
    uint32 convexSet(Vec3* _points, uint32 _numPoints, float _threshold)
    {
        float tSq = _threshold * _threshold; // Compare squared numbers

        // Find duplicate points (brute force)
        for(uint32 i = 0; i < _numPoints; ++i)
        {
            for(uint32 j = i+1; j < _numPoints; ++j)
                if(lensq(_points[j] - _points[i]) <= tSq)
                {
                    // Whoops, j is the same as i -> remove j
                    _points[j--] = _points[--_numPoints];
                }
        }

        // For each point test if there is a separating plane to all other
        // points. If not discard it.
        for(uint32 i = 0; i < _numPoints; ++i)
        {
            // Up to 3 points define the plane direction
            Vec3 extrema[3];
            Plane p;
            float d = 0.0f;
            uint32 ne = 0;
            bool maySeparate = true;
            for(uint32 j = 0; j < _numPoints && maySeparate; ++j) if(i != j)
            {
                switch(ne) {
                case 0: {
                    extrema[ne++] = _points[j];
                } break;
                case 1: {
                    // If j is the second point in the extrema array, test
                    // for colinearity of i.
                    extrema[ne++] = _points[j];
                    if(distance(_points[i], Segment(extrema[0], extrema[1])) <= _threshold)
                        maySeparate = false;
                } break;
                case 2: {
                    // If j is the third test for coplanarity
                    extrema[ne++] = _points[j];
                    p = Plane(extrema[0], extrema[1], extrema[2]);
                    d = dot(_points[i], p.n) + p.d;
                    if(abs(d) <= _threshold)
                        maySeparate = false;
                } break;
                default: {
                    float dj = dot(_points[j], p.n) + p.d;
                    if(dj * d > 0.0f) // Not separated by current plane?
                    {
                        // If j is between the plane and i, it must be
                        // added to the extremal set and another point
                        // must be removed.
                        p = Plane(_points[j], extrema[1], extrema[2]);
                        d = dot(_points[i], p.n) + p.d;
                        dj = dot(extrema[0], p.n) + p.d;
                        if(d * dj < 0.0f) { extrema[0] = _points[j]; break; }
                        p = Plane(extrema[0], _points[j], extrema[2]);
                        d = dot(_points[i], p.n) + p.d;
                        dj = dot(extrema[1], p.n) + p.d;
                        if(d * dj < 0.0f) { extrema[1] = _points[j]; break; }
                        p = Plane(extrema[0], extrema[1], _points[j]);
                        d = dot(_points[i], p.n) + p.d;
                        dj = dot(extrema[2], p.n) + p.d;
                        if(d * dj < 0.0f) { extrema[2] = _points[j]; break; }
                        // For each side of the tetrahedron i and the 4th point
                        // are on the same side -> no separating plane
                        maySeparate = false;
                    }
                }}
            }
            if(!maySeparate)
            {
                _points[i--] = _points[--_numPoints];
            }
        }
        return _numPoints;
    }
}