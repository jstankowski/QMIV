#pragma once
#include "xCommonDefGSC.h"
#include "xCommonDefCORE.h"

namespace PMBB_NAMESPACE::GSC {

//===============================================================================================================================================================================================================

class xSphHarmEvaluator 
{
public:
  static constexpr int L = 3;
  static constexpr int NumCoeffs = (L + 1) * (L + 1);

  xSphHarmEvaluator(const flt32* SH_R, const flt32* SH_G, const flt32* SH_B) 
  {
    std::copy(SH_R, SH_R + NumCoeffs, this->SH_R.begin());
    std::copy(SH_G, SH_G + NumCoeffs, this->SH_G.begin());
    std::copy(SH_B, SH_B + NumCoeffs, this->SH_B.begin());
  }

  flt32V3 xEvaluateAtDirection(flt32 x, flt32 y, flt32 z) const 
  {
    flt32 theta = std::acos(std::clamp(z, -1.0f, 1.0f));
    flt32 phi = std::atan2(y, x);
    flt32 r = 0, g = 0, b = 0;

    for (int l = 0; l <= L; ++l)
      for (int m = -l; m <= l; ++m) 
      {
        int i = l * l + l + m;
        flt32 Y = xSH_Ylm(l, m, theta, phi);
        r += SH_R[i] * Y;
        g += SH_G[i] * Y;
        b += SH_B[i] * Y;
      }
    return { r, g, b };
  }

  static flt32 xSH_Ylm(int l, int m, flt32 theta, flt32 phi) {
    flt32 x = std::cos(theta);
    if      (m > 0) { return std::sqrt(2.0f) * K(l,  m) * std::cos( m * phi) * P(l,  m, x); }
    else if (m < 0) { return std::sqrt(2.0f) * K(l, -m) * std::sin(-m * phi) * P(l, -m, x); }
    else            { return                   K(l,  0) *                      P(l,  0, x); }
  }

private:
  std::array<flt32, NumCoeffs> SH_R, SH_G, SH_B;
  static flt32 K(int l, int m) 
  {
    flt32 num = (2.0f * l + 1) * std::tgamma(l - m + 1);
    flt32 denom = 4.0f * xc_Pi<flt32> * std::tgamma(l + m + 1);
    return std::sqrt(num / denom);
  }

  static flt32 P(int l, int m, flt32 x) 
  {
    if (m < 0 || m > l) { return 0.0f; }
    flt32 pmm = 1.0f;
    if (m > 0) 
    {
      flt32 somx2 = std::sqrt((1.0f - x) * (1.0f + x));
      flt32 fact = 1.0f;
      for (int i = 1; i <= m; ++i) 
      {
        pmm *= -fact * somx2;
        fact += 2.0f;
      }
    }

    if (l == m) { return pmm; }
    flt32 pmmp1 = x * (2 * m + 1) * pmm;
    if (l == m + 1) { return pmmp1; }
    flt32 pll = 0.0f;
    for (int ll = m + 2; ll <= l; ++ll) 
    {
      pll = ((2 * ll - 1) * x * pmmp1 - (ll + m - 1) * pmm) / (ll - m);
      pmm = pmmp1;
      pmmp1 = pll;
    }
    return pll;
  }
};

//===============================================================================================================================================================================================================

} //end of namespace PMBB



