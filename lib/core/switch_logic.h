// Hardware-independent logic for the master/slave switch.
// No Arduino or Matter includes here, so it builds and tests on a PC.
#pragma once
#include <stdint.h>

namespace sw {

constexpr int kOutlets = 4;

enum class Mode : uint8_t { Normal = 0, Slave = 1 };
enum class Role : uint8_t { None = 0, Master = 1, Slave = 2 };
enum class Input : uint8_t { Select, Role, Send, Refresh };

// One on/off command for one outlet of the strip.
struct Command {
  uint8_t outlet;  // 0..kOutlets-1
  bool on;
};

struct Result {
  Command cmds[kOutlets];
  uint8_t count = 0;
  bool redraw = false;  // display content changed
};

class Logic {
 public:
  Mode mode = Mode::Normal;
  Role role[kOutlets] = {Role::Master, Role::Slave, Role::None, Role::None};
  bool on[kOutlets] = {false, false, false, false};  // last state we sent (assumed)
  uint8_t selected = 0;

  // The mode slide switch selects the mode.
  Result setMode(Mode m);
  // A click button was pressed.
  Result press(Input in);

 private:
  Result sendToggle();
};

}  // namespace sw
