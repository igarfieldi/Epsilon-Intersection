#include "ei/fixedpoint.hpp"
#include "unittest.hpp"

#include <iostream>

using namespace ei;
using namespace std;

bool test_fixedpoint()
{
    bool result = true;

    // Casting
    Fix<32,16> f0(0.0f);
    Fix<32,16> f1(-0.0);
    Fix<32,16> f2 = static_cast<Fix<32,16>>(1);
    Fix<32,16> f3 = static_cast<Fix<32,16>>(0.1);
    Fix<32,31> f4(-0.25f);
    Fix<32,31> f5(0.1f);
    TEST( float(f0) == 0.0f, "Converting 0 to Fix<32,16> and back to float failed!" );
    TEST( double(f1) == 0.0f, "Converting 0 to Fix<32,16> and back to double failed!" );
    TEST( float(f2) == 1.0f, "Converting 1 to Fix<32,16> and back to float failed!" );
    TEST( approx(float(f3), 0.1f, 1e-5f), "Converting 0.1 to Fix<32,16> and back to float failed!" );
    TEST( float(f4) == -0.25f, "Converting -0.25f to Fix<32,31> and back to float failed!" );
    TEST( float(f5) == 0.1f, "Converting 0.1 to Fix<32,31> and back to float failed!" );

    // Fixed point unary minus
    TEST( float(-Fix<32,31>(-0.2)) == 0.2f, "Unary minus of Fix<32,31> failed!" );

    // Addition and subtraction
    Fix<32,16> f6 = static_cast<Fix<32,16>>(0.5f);
    Fix<32,16> f7 = static_cast<Fix<32,16>>(2.0f);
    TEST( (Fix<32,16>(1.1f)) == f2 + f3, "Addition f2 + f3 failed!" );
    TEST( f6 == f2 + -f6, "Addition f2 + -f6 failed!" );
    TEST( f6 == f2 - f6, "Subtraction f2 - f6 failed!" );
    TEST( f3 == f2 * f3, "Multiplication f2 * f3 failed!" );
    TEST( (Fix<32,16>(0.05f)) == f3 * f6, "Multiplication f3 * f6 failed!" );
    TEST( f7 == f2 / f6, "Division f2 / f6 failed!" );

    // Comparison
    Fix<32,16> f8 = static_cast<Fix<32,16>>(-8.0f);
    Fix<32,31> f9(0.5f);
    TEST( f0 == f1, "0 and -0 should be equal!" );
    TEST( f0 != f2, "f0 and f2 should be unequal!" );
    TEST( f3 <= f3, "f3 and f3 are equal!" );
    TEST( !(f3 < f3), "f3 and f3 are equal!" );
    TEST( f0 < f3, "f0 is smaller than f3!" );
    TEST( !(f0 > f3), "f0 is not larger than f3!" );
    TEST( f0 > f8, "f0 is greater than f8!" );
    TEST( f5 >= f4, "f5 is greater than f4!" );
    TEST( f4 < f5, "f4 is smaller than f5!" );
    TEST( f5 < f9, "f4 is smaller than f9!" );

    int i0 = Fix<32,16>::NUM_INT_DIGITS2;
    int i1 = Fix<32,16>::NUM_INT_DIGITS10;
    int i2 = Fix<32,16>::NUM_FRAC_DIGITS2;
    int i3 = Fix<32,16>::NUM_FRAC_DIGITS10;

//    char buf[128];
//    f4.toString(buf);

    return result;
}