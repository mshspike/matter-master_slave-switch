#include "switch_logic.h"

namespace sw {

Result Logic::setMode(Mode m) {
  Result r;
  if (m != mode) {
    mode = m;
    r.redraw = true;
  }
  return r;
}

Result Logic::press(Input in) {
  Result r;
  switch (in) {
    case Input::Select:
      selected = (selected + 1) % kOutlets;
      r.redraw = true;
      break;
    case Input::Role:
      role[selected] = static_cast<Role>((static_cast<uint8_t>(role[selected]) + 1) % 3);
      r.redraw = true;
      break;
    case Input::Send:
      return sendToggle();
    case Input::Refresh:
      r.redraw = true;
      break;
  }
  return r;
}

// Normal mode: toggle the selected outlet only.
// Slave mode: toggle every master; every slave takes the state of the first master.
// With no master assigned, slave mode falls back to toggling the selected outlet.
Result Logic::sendToggle() {
  Result r;
  r.redraw = true;
  auto add = [&](uint8_t o, bool state) {
    on[o] = state;
    r.cmds[r.count++] = Command{o, state};
  };

  if (mode == Mode::Normal) {
    add(selected, !on[selected]);
    return r;
  }

  int firstMaster = -1;
  for (int i = 0; i < kOutlets; i++) {
    if (role[i] == Role::Master) { firstMaster = i; break; }
  }
  if (firstMaster < 0) {
    add(selected, !on[selected]);
    return r;
  }
  const bool target = !on[firstMaster];
  for (int i = 0; i < kOutlets; i++) {
    if (role[i] == Role::Master || role[i] == Role::Slave) add(i, target);
  }
  return r;
}

}  // namespace sw
