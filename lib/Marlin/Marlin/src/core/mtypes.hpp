/**
 * @brief Strong Types related to the motion system
 **/
#pragma once
#include <core/types.h>

struct CoordCartTag {};
struct StepsCartTag {};
struct MStepsCartTag {};

#if ENABLED(COREXY)
struct CoordCoreXYTag {};
using CoordTypeTag = CoordCoreXYTag;

struct StepsCoreXYTag {};
using StepsTypeTag = StepsCoreXYTag;

struct MStepsCoreXYTag {};
using MStepsTypeTag = MStepsCoreXYTag;
#else // CARTESIAN
using CoordTypeTag = CoordCartTag;
using StepsTypeTag = StepsCartTag;
using MStepsTypeTag = MStepsCartTag;
#endif

// AB logical positions
using ab_pos_t = XYval<float, CoordTypeTag>;
using abc_pos_t = XYZval<float, CoordTypeTag>;
using abce_pos_t = XYZEval<float, CoordTypeTag>;

// AB positions in steps
using ab_steps_t = XYval<int32_t, StepsTypeTag>;
using abc_steps_t = XYZval<int32_t, StepsTypeTag>;
using abce_steps_t = XYZEval<int32_t, StepsTypeTag>;

// AB positions in mini-steps
using ab_msteps_t = XYval<int32_t, MStepsTypeTag>;
using abc_msteps_t = XYZval<int32_t, MStepsTypeTag>;
using abce_msteps_t = XYZEval<int32_t, MStepsTypeTag>;

// XYZE positions in steps
using xy_steps_t = XYval<int32_t, StepsCartTag>;
using xyz_steps_t = XYZval<int32_t, StepsCartTag>;
using xyze_steps_t = XYZEval<int32_t, StepsCartTag>;

// XYZE positions in mini-steps
using xy_msteps_t = XYval<int32_t, MStepsCartTag>;
using xyz_msteps_t = XYZval<int32_t, MStepsCartTag>;
using xyze_msteps_t = XYZEval<int32_t, MStepsCartTag>;
