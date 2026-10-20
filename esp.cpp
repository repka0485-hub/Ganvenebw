#include "esp.h"

namespace {

constexpr int kMaxEntities = 64;

struct Entity {
  float pos[3];
  float hp;
};

struct Frame {
  float vp[16];  // view-projection, see project() for the assumed convention
  Entity ents[kMaxEntities];
  int count;
};

// FILL: acquire() must fill ONE temporally consistent Frame (view-projection + entities)
// for the target build. Procedure: fingerprint the target (platform, module, build id);
// trace one observable state change (e.g. hp) to its owner and update path; validate the
// resolved fields on >= 3 independent samples and across a level reload / process restart.
// Until then it reports "no data" instead of returning believable zeros.
bool acquire(Frame& out) {
  (void)out;
  return false;
}

// ASSUMPTION (validate with known world points, including one behind the camera):
// vp is row-major, clip = vp * (x, y, z, 1)^T, NDC y up, viewport origin top-left.
bool project(const float vp[16], const float p[3], const ImVec2& d, ImVec2& out) {
  const float x = vp[0] * p[0] + vp[1] * p[1] + vp[2] * p[2] + vp[3];
  const float y = vp[4] * p[0] + vp[5] * p[1] + vp[6] * p[2] + vp[7];
  const float w = vp[12] * p[0] + vp[13] * p[1] + vp[14] * p[2] + vp[15];
  if (w <= 1e-4f) return false;  // behind the camera
  out.x = (x / w * 0.5f + 0.5f) * d.x;
  out.y = (1.0f - (y / w * 0.5f + 0.5f)) * d.y;
  return true;
}

}  // namespace

void esp::draw(const ImVec2& display) {
  ImDrawList* dl = ImGui::GetForegroundDrawList();

  Frame f;
  if (!acquire(f)) {
    dl->AddText(ImVec2(16.0f, 16.0f), IM_COL32(255, 200, 0, 255), "espmod: hook active, no target data");
    return;
  }

  const int n = f.count < kMaxEntities ? f.count : kMaxEntities;
  for (int i = 0; i < n; ++i) {
    ImVec2 s;
    if (!project(f.vp, f.ents[i].pos, display, s)) continue;
    char buf[16];
    ImFormatString(buf, sizeof(buf), "%.0f", f.ents[i].hp);
    dl->AddCircle(s, 6.0f, IM_COL32(255, 64, 64, 255), 12, 2.0f);
    dl->AddText(ImVec2(s.x + 8.0f, s.y - 7.0f), IM_COL32(255, 255, 255, 255), buf);
  }
}
