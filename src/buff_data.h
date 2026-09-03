#pragma once
// ============================================================================
// Buff Data Structures and Reading Logic
// ============================================================================
#include <cstdint>
#include <windows.h>

// Forward decls from il2cpp_api.h
extern void *Invoke(void *method, void *obj, void **params);
extern int ReadStr(void *s, char *b, int sz);

// ============================================================================
// Method handles (resolved in InitThread)
// ============================================================================
static void *g_getId = nullptr, *g_getDuration = nullptr,
            *g_getLifeTime = nullptr;
static void *g_getEnhanceCnt = nullptr;
static void *g_getAttrAdd = nullptr, *g_getAttrMul = nullptr;
static size_t g_loaderOffset = 0;

// Buff backing-field offsets (resolved dynamically in MainThread).
// Zero keeps the legacy offsets as a compatibility fallback.
static size_t g_buffDataOffset = 0;
static size_t g_buffIdOffset = 0;
static size_t g_buffBlackboardOffset = 0;
static size_t g_buffDataIconConfigOffset = 0;
static size_t g_buffIconSpritePathOffset = 0;

// AttributeModifierLoader field offsets (resolved dynamically, defaults = old hardcoded values)
static int g_offAmlAdd = 0x18;           // m_addition
static int g_offAmlMul = 0x10;           // m_multiplier
static int g_offAmlFinalAdd = 0x20;      // m_finalAddition
static int g_offAmlFinalScl = 0x28;      // m_finalMultiplier
static int g_offAmlBaseAdd = 0x38;       // m_baseAddition
static int g_offAmlBaseMul = 0x30;       // m_baseMultiplier
static int g_offAmlBaseFinalAdd = 0x40;  // m_baseFinalAddition
static int g_offAmlBaseFinalScl = 0x48;  // m_baseFinalMultiplier

// Index mapping for s_attributesToModify
static int32_t g_satmMap[96] = {0};
static int g_satmLen = 0;
static bool g_satmLoaded = false;
static void* g_satmField = nullptr;

// ============================================================================
// Data Structures
// ============================================================================
struct AttrMod {
  float add[96];
  float baseAdd[96];
  float finalAdd[96];
  float baseFinalAdd[96];
  float mul[96];
  float baseMul[96];
  float finalScl[96];
  float baseFinalScl[96];
};

struct BlackboardEntry {
  char key[64];
  double value;
};

struct ActiveBuff {
  void *buffObj;
  uint32_t instUid;
  char id[128];
  float duration;
  float lifeTime;
  int enhanceCnt;
  int trueEnhanceCnt;
  int source; // 0 = ProgressBar (buff), 1 = BuffIcon (debuff)

  AttrMod attrs;
  BlackboardEntry bb[8];
  int bbCount;
  char iconName[128];
};

// ============================================================================
// Global buff state
// ============================================================================
static ActiveBuff g_buffs[64];
static int g_buffCount = 0;
static CRITICAL_SECTION g_buffLock;

// Display order: UIDs of buffs in HUD display order
static uint32_t g_displayUids[64];
static int g_displayCount = 0;

// UIBuffNode resolution
static void *g_getBuffInstanceUid = nullptr;
static size_t g_orderedListOffset = 0;
static size_t g_uiStackBuffsOffset = 0;
static size_t g_gpuiOrderedListOffset = 0;
static size_t g_gpuiStackBuffsOffset = 0;

// ============================================================================
// Unboxing helpers
// ============================================================================
static float UnboxFloat(void *boxed) {
  __try {
    return boxed ? *(float *)((char *)boxed + 16) : 0.0f;
  } __except (1) {
    return 0.0f;
  }
}

static int32_t UnboxInt(void *boxed) {
  __try {
    return boxed ? *(int32_t *)((char *)boxed + 16) : 0;
  } __except (1) {
    return 0;
  }
}

// ============================================================================
// Array reading
// ============================================================================
static void ReadDoubleArray(void *arrayObj, float *outArray, int maxLen) {
  __try {
    if (!arrayObj)
      return;
    int32_t len = *(int32_t *)((char *)arrayObj + 24);
    if (len <= 0 || len > maxLen)
      return;
    double *data = (double *)((char *)arrayObj + 32);
    for (int i = 0; i < len; i++)
      outArray[i] = (float)data[i];
  } __except (1) {
  }
}

// ============================================================================
// ReadBuffData — reads all buff fields from a game buff object
// ============================================================================
static void ReadBuffData(void *objPtr, ActiveBuff *out) {
  __try {
    out->buffObj = *(void **)objPtr;
    out->instUid = *(uint32_t *)((char *)objPtr + 8);
    out->id[0] = 0;
    out->iconName[0] = 0;
    out->duration = 0;
    out->lifeTime = 0;
    out->enhanceCnt = 0;
    out->trueEnhanceCnt = 0;

    out->bbCount = 0;
    memset(&out->attrs, 0, sizeof(out->attrs));

    if (!out->buffObj)
      return;
    void *r;
    if (g_getId && (r = Invoke(g_getId, out->buffObj)))
      ReadStr(r, out->id, sizeof(out->id));
    if (g_getDuration)
      out->duration = UnboxFloat(Invoke(g_getDuration, out->buffObj));
    if (g_getLifeTime)
      out->lifeTime = UnboxFloat(Invoke(g_getLifeTime, out->buffObj));
    if (g_getEnhanceCnt) {
      void *boxedCnt = Invoke(g_getEnhanceCnt, out->buffObj);
      if (boxedCnt) {
        out->enhanceCnt = *(int *)((char *)boxedCnt + 16);
        out->trueEnhanceCnt = out->enhanceCnt;
      }
    }

    if (g_loaderOffset > 0) {
      void *loader = *(void **)((char *)out->buffObj + g_loaderOffset);
      if (loader) {
        auto readArr = [](void *loader, int fieldOff, float *dst, int max) {
          void *arr = *(void **)((char *)loader + fieldOff);
          if (!arr) return;
          int32_t len = *(int32_t *)((char *)arr + 0x18);
          if (len <= 0 || len > max) return;
          double *data = (double *)((char *)arr + 0x20);
          for (int j = 0; j < len && j < max; j++)
            dst[j] = (float)data[j];
        };
        readArr(loader, g_offAmlAdd, out->attrs.add, 96);
        readArr(loader, g_offAmlBaseAdd, out->attrs.baseAdd, 96);
        readArr(loader, g_offAmlFinalAdd, out->attrs.finalAdd, 96);
        readArr(loader, g_offAmlBaseFinalAdd, out->attrs.baseFinalAdd, 96);
        readArr(loader, g_offAmlMul, out->attrs.mul, 96);
        readArr(loader, g_offAmlBaseMul, out->attrs.baseMul, 96);
        readArr(loader, g_offAmlFinalScl, out->attrs.finalScl, 96);
        readArr(loader, g_offAmlBaseFinalScl, out->attrs.baseFinalScl, 96);
      }
    }

    // Read blackboard DataPair[]. The backing field moved after the game update,
    // so prefer the runtime-resolved offset and retain +0x158 for older builds.
    void *bb = nullptr;
    if (g_buffBlackboardOffset > 0)
      bb = *(void **)((char *)out->buffObj + g_buffBlackboardOffset);
    else
      bb = *(void **)((char *)out->buffObj + 0x158);
    if (bb) {
      // ActionBlackboard implementations expose their direct Blackboard via a
      // backing field. Resolve it from the concrete runtime class, then use the
      // standard List<T> layout. Retain the legacy direct array path as fallback.
      static void *s_actionBbClass = nullptr;
      static int s_directBbOffset = -2;
      void *directBb = nullptr;
      if (il2cpp_object_get_class) {
        void *bbClass = il2cpp_object_get_class(bb);
        if (bbClass && bbClass != s_actionBbClass) {
          const char *directNames[] = {
            "<directBlackboard>k__BackingField", "m_directBlackboard",
            "directBlackboard"
          };
          s_actionBbClass = bbClass;
          s_directBbOffset = FindFieldInHierarchy(bbClass, directNames, 3);
        }
        if (s_directBbOffset >= 0)
          directBb = *(void **)((char *)bb + s_directBbOffset);
      }

      void *dpArr = nullptr;
      int32_t dpLen = 0;
      if (directBb) {
        dpArr = *(void **)((char *)directBb + 0x10); // List<T>._items
        dpLen = *(int32_t *)((char *)directBb + 0x18); // List<T>._size
      }
      if (!dpArr) {
        dpArr = *(void **)((char *)bb + 0x40);
        if (dpArr)
          dpLen = *(int32_t *)((char *)dpArr + 0x18);
      }
      if (dpArr) {
        if (dpLen > 0 && dpLen <= 8) {
          char *base = (char *)dpArr + 0x20;
          out->bbCount = 0;
          for (int d = 0; d < dpLen && out->bbCount < 8; d++) {
            char *ep = base + d * 32;
            void *sref = *(void **)(ep);
            if (!sref || (uintptr_t)sref < 0x10000) continue;
            int32_t slen = *(int32_t *)((char *)sref + 0x10);
            if (slen <= 0 || slen >= 60) continue;
            wchar_t *wc = (wchar_t *)((char *)sref + 0x14);
            WideCharToMultiByte(CP_UTF8, 0, wc, slen,
              out->bb[out->bbCount].key, 63, NULL, NULL);
            out->bb[out->bbCount].value = *(double *)(ep + 8);
            out->bbCount++;
          }
        }
      }
    }

    // Read icon name through dynamically resolved Buff -> BuffData ->
    // BuffIconConfig fields, with legacy offsets for older builds.
    void *mdata = nullptr;
    if (g_buffDataOffset > 0)
      mdata = *(void **)((char *)out->buffObj + g_buffDataOffset);
    else
      mdata = *(void **)((char *)out->buffObj + 0x18);
    if (mdata) {
      void *iconCfg = nullptr;
      if (g_buffDataIconConfigOffset > 0)
        iconCfg = *(void **)((char *)mdata + g_buffDataIconConfigOffset);
      else
        iconCfg = *(void **)((char *)mdata + 0x18);
      if (iconCfg && (uintptr_t)iconCfg > 0x10000) {
        void *iconStr = nullptr;
        if (g_buffIconSpritePathOffset > 0)
          iconStr = *(void **)((char *)iconCfg + g_buffIconSpritePathOffset);
        else
          iconStr = *(void **)((char *)iconCfg + 0x10);
        if (iconStr && (uintptr_t)iconStr > 0x10000) {
          int32_t sl = *(int32_t *)((char *)iconStr + 0x10);
          if (sl > 0 && sl < 120) {
            wchar_t *wc = (wchar_t *)((char *)iconStr + 0x14);
            WideCharToMultiByte(CP_UTF8, 0, wc, sl, out->iconName, 127, NULL, NULL);
          }
        }
      }
    }
  } __except (1) {
  }
}
