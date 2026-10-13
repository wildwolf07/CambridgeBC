#pragma once
#define CFG_SEED 3
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
#ifndef P_HEAL
#define P_HEAL 0.33f
#endif
#ifndef P_BANK_MAX
#define P_BANK_MAX 60
#endif
#ifndef P_BANK_MID
#define P_BANK_MID 30
#endif
#ifndef P_BANK_MID_D
#define P_BANK_MID_D 10
#endif
#ifndef P_BANK_NEAR_D
#define P_BANK_NEAR_D 3
#endif
#ifndef P_GOLD_W1
#define P_GOLD_W1 0.3f
#endif
#ifndef P_GOLD_W2
#define P_GOLD_W2 0.2f
#endif
#ifndef P_GOLD_W3
#define P_GOLD_W3 0.08f
#endif
#ifndef P_CAPT_T
#define P_CAPT_T 6.f
#endif
#ifndef P_DOGPILE
#define P_DOGPILE 0.6f
#endif
#ifndef P_STICKY
#define P_STICKY 1.6f
#endif
