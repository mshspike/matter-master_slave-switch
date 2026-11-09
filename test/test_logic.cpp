// Host test: g++ -std=c++17 -Ilib/core test/test_logic.cpp lib/core/switch_logic.cpp && ./a.out
#include <cassert>
#include <cstdio>
#include "switch_logic.h"
using namespace sw;

static void normalModeTogglesOnlySelected() {
  Logic l;
  Result r = l.press(Input::Send);
  assert(r.count == 1 && r.cmds[0].outlet == 0 && r.cmds[0].on);
  r = l.press(Input::Send);
  assert(r.count == 1 && !r.cmds[0].on);
}

static void slaveModeFollowsMaster() {
  Logic l;  // outlet 0 master, outlet 1 slave by default
  l.setMode(Mode::Slave);
  Result r = l.press(Input::Send);
  assert(r.count == 2);
  assert(r.cmds[0].outlet == 0 && r.cmds[0].on);
  assert(r.cmds[1].outlet == 1 && r.cmds[1].on);
  r = l.press(Input::Send);
  assert(r.count == 2 && !r.cmds[0].on && !r.cmds[1].on);
}

static void slaveFollowsMasterStateEvenIfDrifted() {
  Logic l;
  l.setMode(Mode::Slave);
  l.on[1] = true;  // slave was left on, master is off
  Result r = l.press(Input::Send);
  assert(r.cmds[1].outlet == 1 && r.cmds[1].on);  // master turns on, slave forced on too
  r = l.press(Input::Send);
  assert(!r.cmds[0].on && !r.cmds[1].on);
}

static void noMasterFallsBackToSelected() {
  Logic l;
  for (auto& x : l.role) x = Role::None;
  l.setMode(Mode::Slave);
  Result r = l.press(Input::Send);
  assert(r.count == 1 && r.cmds[0].outlet == 0);
}

static void selectAndRoleCycle() {
  Logic l;
  for (int i = 0; i < kOutlets; i++) l.press(Input::Select);
  assert(l.selected == 0);
  l.press(Input::Role);
  assert(l.role[0] == Role::Slave);
  l.press(Input::Role);
  assert(l.role[0] == Role::None);
  l.press(Input::Role);
  assert(l.role[0] == Role::Master);
}

static void modeChangeOnlyRedrawsOnChange() {
  Logic l;
  assert(!l.setMode(Mode::Normal).redraw);
  assert(l.setMode(Mode::Slave).redraw);
}

int main() {
  normalModeTogglesOnlySelected();
  slaveModeFollowsMaster();
  slaveFollowsMasterStateEvenIfDrifted();
  noMasterFallsBackToSelected();
  selectAndRoleCycle();
  modeChangeOnlyRedrawsOnChange();
  std::puts("all tests passed");
  return 0;
}
