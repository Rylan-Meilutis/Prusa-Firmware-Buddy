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
typedef CoordCoreXYTag CoordTypeTag;

struct StepsCoreXYTag {};
typedef StepsCoreXYTag StepsTypeTag;

struct MStepsCoreXYTag {};
typedef MStepsCoreXYTag MStepsTypeTag;
#else // CARTESIAN
typedef CoordCartTag CoordTypeTag;
typedef StepsCartTag StepsTypeTag;
typedef MStepsCartTag MStepsTypeTag;
#endif

// AB logical positions
typedef struct XYval<float, CoordTypeTag> ab_pos_t;
typedef struct XYZval<float, CoordTypeTag> abc_pos_t;
typedef struct XYZEval<float, CoordTypeTag> abce_pos_t;

// AB positions in steps
typedef struct XYval<int32_t, StepsTypeTag> ab_steps_t;
typedef struct XYZval<int32_t, StepsTypeTag> abc_steps_t;
typedef struct XYZEval<int32_t, StepsTypeTag> abce_steps_t;

// AB positions in mini-steps
typedef struct XYval<int32_t, MStepsTypeTag> ab_msteps_t;
typedef struct XYZval<int32_t, MStepsTypeTag> abc_msteps_t;
typedef struct XYZEval<int32_t, MStepsTypeTag> abce_msteps_t;

// XYZE positions in steps
typedef struct XYval<int32_t, StepsCartTag> xy_steps_t;
typedef struct XYZval<int32_t, StepsCartTag> xyz_steps_t;
typedef struct XYZEval<int32_t, StepsCartTag> xyze_steps_t;

// XYZE positions in mini-steps
typedef struct XYval<int32_t, MStepsCartTag> xy_msteps_t;
typedef struct XYZval<int32_t, MStepsCartTag> xyz_msteps_t;
typedef struct XYZEval<int32_t, MStepsCartTag> xyze_msteps_t;
