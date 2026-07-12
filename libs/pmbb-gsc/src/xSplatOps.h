#pragma once
#include "xCommonDefGSC.h"
#include "xGaussianCloud.h"

namespace PMBB_NAMESPACE::GSC {

//===============================================================================================================================================================================================================

class xSplatOps
{
public:
  static int32 xSphHarmCountFromDegree(int32 Degree);
  static int32 xSphHarmDegreeFromCount(int32 Count );
};
  
//===============================================================================================================================================================================================================

} //end of namespace PMBB