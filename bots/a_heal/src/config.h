#pragma once
#define CFG_KITE 0
#define CFG_NO_SOLO 0
#define CFG_SIM_HEAL 1
#define CFG_GOLD_ROUTE 0
#define CFG_HOME_OPEN 0
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
#ifndef CFG_SHOP_TRIP
#define CFG_SHOP_TRIP 1
#endif
#ifndef P_SHOP_BASE
#define P_SHOP_BASE 150
#endif
#ifndef CFG_BUILD
#define CFG_BUILD 3
#endif
#ifndef P_ENGAGE
#define P_ENGAGE 1.5f
#endif
#ifndef CFG_SIM
#define CFG_SIM 1
#endif
#ifndef P_SIM_MARGIN
#define P_SIM_MARGIN 0.0f
#endif
#ifndef CFG_SPACING
#define CFG_SPACING 0
#endif
#ifndef CFG_PAIR
#define CFG_PAIR 2
#endif
#ifndef CFG_CASSI_FOCUS
#define CFG_CASSI_FOCUS 1
#endif
#ifndef CFG_BUDDY
#define CFG_BUDDY 0
#endif
#ifndef CFG_SAFETY
#define CFG_SAFETY 1
#endif
#ifndef CFG_CALL
#define CFG_CALL 1
#endif
#ifndef P_CALL_BONUS
#define P_CALL_BONUS 0.7f
#endif
#ifndef CFG_NO_SOLO
#define CFG_NO_SOLO 1
#endif
#ifndef CFG_SQUAD
#define CFG_SQUAD 0
#endif
#ifndef CFG_COHESION
#define CFG_COHESION 0
#endif
#ifndef P_COHESION
#define P_COHESION 1.0f
#endif
#ifndef CFG_HUNT
#define CFG_HUNT 0
#endif
#ifndef P_EARLY_D
#define P_EARLY_D 4
#endif
#ifndef CFG_SAFE_STEP
#define CFG_SAFE_STEP 1
#endif
#ifndef CFG_TERRITORY
#define CFG_TERRITORY 1
#endif
#ifndef P_TERRITORY
#define P_TERRITORY 10
#endif
#ifndef CFG_LOWHP
#define CFG_LOWHP 1
#endif
#ifndef P_LOWHP
#define P_LOWHP 0.45f
#endif
#ifndef CFG_DUEL
#define CFG_DUEL 1
#endif
#ifndef CFG_SAFE_RECALL
#define CFG_SAFE_RECALL 1
#endif
#ifndef CFG_CAMP_STUN
#define CFG_CAMP_STUN 1
#endif
#ifndef P_STUN_MIN_STR
#define P_STUN_MIN_STR 60
#endif
#ifndef CFG_BANK2
#define CFG_BANK2 1
#endif
#ifndef P_ENDGAME
#define P_ENDGAME 600
#endif
#ifndef CFG_HIDDEN
#define CFG_HIDDEN 1
#endif
#ifndef P_CONTEST_PEN
#define P_CONTEST_PEN 1.0f
#endif
#ifndef CFG_THREAT_HEAL
#define CFG_THREAT_HEAL 1
#endif
#ifndef CFG_REGION
#define CFG_REGION 1
#endif
#ifndef P_REGION_R
#define P_REGION_R 10
#endif
#ifndef P_REGION_W
#define P_REGION_W 0.5f
#endif
#ifndef CFG_BANK_FIRST
#define CFG_BANK_FIRST 1
#endif
#ifndef CFG_BALL
#define CFG_BALL 0
#endif
#ifndef P_BALL_AREA
#define P_BALL_AREA 1000
#endif
#ifndef CFG_TEAMFIGHT
#define CFG_TEAMFIGHT 0
#endif
#ifndef P_CL_AE
#define P_CL_AE 6
#endif
#ifndef P_CL_AA
#define P_CL_AA 5
#endif
#ifndef P_PIN_K
#define P_PIN_K 3.f
#endif
#ifndef CFG_HITRUN
#define CFG_HITRUN 1
#endif
#ifndef CFG_ASSIST
#define CFG_ASSIST 0
#endif
#ifndef P_JOIN_D
#define P_JOIN_D 9
#endif
#ifndef P_JOIN_NEED
#define P_JOIN_NEED 0.5f
#endif
#ifndef CFG_NOWAIT
#define CFG_NOWAIT 0
#endif
#ifndef P_WAIT_SLACK
#define P_WAIT_SLACK 3
#endif
#ifndef CFG_LOCKFIX
#define CFG_LOCKFIX 1
#endif
#ifndef CFG_LOCKS
#define CFG_LOCKS 0
#endif
#ifndef P_LOCK_UNTIL
#define P_LOCK_UNTIL 1500
#endif
#ifndef CFG_GHOST
#define CFG_GHOST 0
#endif
#ifndef P_GHOST_T
#define P_GHOST_T 6
#endif
#ifndef P_GHOST_D
#define P_GHOST_D 12
#endif
#ifndef CFG_LANCH
#define CFG_LANCH 0
#endif
#ifndef P_FOCUS_W
#define P_FOCUS_W 1.4f
#endif
#ifndef CFG_FASTCG
#define CFG_FASTCG 0
#endif
#ifndef P_CG_R
#define P_CG_R 24
#endif
#ifndef P_CG_FUEL
#define P_CG_FUEL 0
#endif
#ifndef P_REGION_TOPK
#define P_REGION_TOPK 0
#endif
#ifndef CFG_ADAPT
#define CFG_ADAPT 1
#endif
#ifndef P_ADAPT_K
#define P_ADAPT_K 0.1f
#endif
#ifndef P_ADAPT_MIN
#define P_ADAPT_MIN -0.2f
#endif
#ifndef P_ADAPT_MAX
#define P_ADAPT_MAX 0.8f
#endif
#ifndef P_GHOST_ON
#define P_GHOST_ON 2
#endif
// v19 (ladder evidence, research/06): ranged units keep max range from enemy melee instead of trading point-blank.
#ifndef CFG_KITE
#define CFG_KITE 1
#endif
// Count a dash-capable enemy melee as threatening from 4 tiles (dash 2 + step + attack) instead of 2.
#ifndef P_KITE_DASH
#define P_KITE_DASH 0
#endif
// Fight sim counts Willow heals and Varg shields as extra HP (P_SIM_HEAL_F = Willow focus assumed).
#ifndef CFG_SIM_HEAL
#define CFG_SIM_HEAL 1
#endif
#ifndef P_SIM_HEAL_F
#define P_SIM_HEAL_F 110.f
#endif
// Gold routing in camp choice: per-champion multipliers on the gold weight.
#ifndef CFG_GOLD_ROUTE
#define CFG_GOLD_ROUTE 1
#endif
#ifndef P_GOLD_TAVI
#define P_GOLD_TAVI 2.5f
#endif
#ifndef P_GOLD_CARRY
#define P_GOLD_CARRY 1.4f
#endif
#ifndef P_GOLD_CASSI
#define P_GOLD_CASSI 0.2f
#endif
// Opening at home: camp value multiplier for enemy-half camps before P_HOME_UNTIL and afterwards.
#ifndef CFG_HOME_OPEN
#define CFG_HOME_OPEN 1
#endif
#ifndef P_HOME_UNTIL
#define P_HOME_UNTIL 120
#endif
#ifndef P_HOME_PEN
#define P_HOME_PEN 0.3f
#endif
#ifndef P_HOME_PEN_LATE
#define P_HOME_PEN_LATE 0.8f
#endif
// Melee threat radius for kiting (2 = step+strike reach; 1 = only flee when already adjacent).
#ifndef P_KITE_R
#define P_KITE_R 2
#endif
