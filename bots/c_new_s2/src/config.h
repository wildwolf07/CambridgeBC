#pragma once
#define CFG_SEED 2
// Feature switches; tools/variant.py --define overrides these for experiments.
#ifndef CFG_RALLY
#define CFG_RALLY 0
#endif
#ifndef CFG_CI_FOCUS
#define CFG_CI_FOCUS 1
#endif
#ifndef CFG_LINKED_CAUTION
#define CFG_LINKED_CAUTION 1
#endif
#ifndef CFG_SEED
#define CFG_SEED 0
#endif
#ifndef CFG_COMBAT_OLD
#define CFG_COMBAT_OLD 0
#endif
