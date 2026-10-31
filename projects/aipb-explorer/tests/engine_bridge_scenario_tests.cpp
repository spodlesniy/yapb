#include <cstdio>
#include <stdexcept>
#include <string>
#include <string_view>

#include <aipb/explorer/engine_bridge_scenario.h>

using namespace aipb::explorer::scenario;

static void check(bool condition) {
  if (!condition) {
    throw std::runtime_error("scenario contract regression");
  }
}

static std::string sample() {
  return R"(# Scenario controls are applied together during each action.
[scenario]
version=aipb-scenario-v1
name=ground_forward_01
map=de_dust2
timeout_ms=5000

[bot]
index=1
team=2
x=448
y=2368
z=-92
yaw=-105
pitch=0
vx=0
vy=0
vz=0
crouching=0

[equipment]
primary=none
primary_clip=0
primary_reserve=0
secondary=usp
secondary_clip=12
secondary_reserve=24
he=1
flash=2
smoke=1
defuse_kit=1
money=800
health=100
armor=50
helmet=1
selected=secondary

[action]
duration_ms=500
timeout_ms=700
forward=1
strafe=0.5
yaw_delta=45
pitch_delta=-5
jump=1
duck=1
attack=1
attack2=0
use=0
select=secondary

[action]
duration_ms=250
timeout_ms=500
forward=0
strafe=0
yaw_delta=0
)";
}

static bool rejected(
  const std::string &source,
  const std::string &name = "ground_forward_01"
) {
  Scenario result {};
  result.name = "previous";
  std::string error {};

  if (parseScenario(source, name, result, error)) {
    return false;
  }

  return result.name == "previous" && !error.empty();
}

static std::string replaceOnce(
  std::string source,
  std::string_view oldValue,
  std::string_view newValue
) {
  const auto at = source.find(oldValue);
  check(at != std::string::npos);
  source.replace(at, oldValue.size(), newValue);

  return source;
}

int main() {
  try {
    const std::string valid = sample();
    Scenario result {};
    std::string error {};
    check(parseScenario(valid, "ground_forward_01", result, error));
    check(error.empty());
    check(result.name == "ground_forward_01" && result.map == "de_dust2");
    check(result.position[0] == 448 && result.position[2] == -92);
    check(result.botIndex == 1 && result.team == 2 && !result.crouching);
    check(result.equipment.secondary.type == "usp");
    check(result.equipment.secondary.clip == 12 && result.equipment.secondary.reserve == 24);
    check(result.equipment.he == 1 && result.equipment.flash == 2);
    check(result.equipment.smoke == 1 && result.equipment.defuseKit);
    check(result.equipment.helmet && result.equipment.armor == 50);
    check(result.actions.size() == 2 && result.actions[0].durationMs == 500);
    check(result.actions[0].timeoutMs == 700 && result.actions[1].timeoutMs == 500);
    check(result.actions[0].forward == 1.0f && result.actions[0].strafe == 0.5f);
    check(result.actions[0].yawDelta == 45.0f && result.actions[0].pitchDelta == -5.0f);
    check(result.actions[0].jump && result.actions[0].duck && result.actions[0].attack);
    check(result.equipment.has("defuse_kit") && result.equipment.has("secondary_clip"));
    check(validScenario(result));

    check(rejected(valid, "other_name"));
    check(rejected(replaceOnce(valid, "aipb-scenario-v1", "aipb-scenario-v0")));
    check(rejected(replaceOnce(valid, "name=ground_forward_01", "name=../escape")));
    check(rejected(replaceOnce(valid, "map=de_dust2", "map=../../../de_dust2")));
    check(rejected(replaceOnce(valid, "timeout_ms=5000", "timeout_ms=0")));
    check(rejected(replaceOnce(valid, "timeout_ms=5000", "timeout_ms=1000")));
    check(rejected(replaceOnce(valid, "timeout_ms=700", "timeout_ms=200")));
    check(rejected(replaceOnce(valid, "timeout_ms=700", "timeout_ms=700000")));
    check(rejected(replaceOnce(valid, "forward=1", "forward=1.01")));
    check(rejected(replaceOnce(valid, "strafe=0.5", "strafe=nan")));
    check(rejected(replaceOnce(valid, "x=448", "x=inf")));
    check(rejected(replaceOnce(valid, "yaw=-105", "yaw=181")));
    check(rejected(replaceOnce(valid, "pitch=0", "pitch=-90")));
    check(rejected(replaceOnce(valid, "team=2", "team=1")));
    check(rejected(replaceOnce(valid, "flash=2", "flash=3")));
    check(rejected(replaceOnce(valid, "primary=none", "primary=unknown")));
    check(rejected(replaceOnce(valid, "selected=secondary", "selected=primary")));
    check(rejected(replaceOnce(valid, "duration_ms=500", "duration_ms=0")));
    check(rejected(replaceOnce(valid, "duration_ms=500", "duration_ms=0.5")));
    check(rejected(replaceOnce(valid, "health=100", "health=-1")));
    check(rejected(replaceOnce(valid, "money=800", "money=16001")));
    check(rejected(replaceOnce(valid, "duck=1", "duck=2")));
    check(rejected(replaceOnce(valid, "forward=1", "unsupported=1")));
    check(rejected(replaceOnce(valid, "[equipment]", "[mystery]")));
    check(rejected(replaceOnce(valid, "version=aipb-scenario-v1",
      "version=aipb-scenario-v1\nversion=aipb-scenario-v1")));
    check(rejected(replaceOnce(valid, "duration_ms=500",
      "duration_ms=500\nduration_ms=500")));
    check(rejected(replaceOnce(valid, "duration_ms=250", "unused=250")));
    check(rejected(replaceOnce(valid, "name=ground_forward_01", "")));
    check(rejected(replaceOnce(valid, "x=448", "")));
    check(rejected(replaceOnce(valid, "y=2368", "")));
    check(rejected(replaceOnce(valid, "z=-92", "")));
    const auto noXyz = replaceOnce(replaceOnce(replaceOnce(valid,
      "x=448\n", ""), "y=2368\n", ""), "z=-92\n", "");
    const auto noPose = replaceOnce(replaceOnce(noXyz,
      "yaw=-105\n", ""), "pitch=0\n", "");
    check(parseScenario(noPose, "ground_forward_01", result, error));
    check(!result.hasBot("x") && !result.hasBot("y") && !result.hasBot("z"));
    check(!result.hasBot("yaw") && !result.hasBot("pitch"));
    check(validScenario(result));
    const auto onlyYaw = replaceOnce(noPose, "vz=0\n", "vz=0\nyaw=45\n");
    check(parseScenario(onlyYaw, "ground_forward_01", result, error));
    check(result.hasBot("yaw") && !result.hasBot("pitch") && result.yaw == 45);
    const auto onlyPitch = replaceOnce(noPose, "vz=0\n", "vz=0\npitch=-12\n");
    check(parseScenario(onlyPitch, "ground_forward_01", result, error));
    check(!result.hasBot("yaw") && result.hasBot("pitch") && result.pitch == -12);
    check(parseScenario(noXyz, "ground_forward_01", result, error));
    check(!result.hasBot("x") && result.hasBot("yaw") && result.hasBot("pitch"));
    const auto noAngles = replaceOnce(replaceOnce(valid,
      "yaw=-105\n", ""), "pitch=0\n", "");
    check(parseScenario(noAngles, "ground_forward_01", result, error));
    check(result.hasBot("x") && !result.hasBot("yaw") && !result.hasBot("pitch"));
    check(rejected(replaceOnce(noPose, "vz=0\n", "vz=0\nx=1\n")));
    check(rejected(replaceOnce(noPose, "vz=0\n", "vz=0\ny=1\nz=2\n")));
    Scenario invalidPartial = result;
    invalidPartial.specifiedBot.erase("z");
    check(!validScenario(invalidPartial));
    check(rejected(valid + "\n[action]\nforward=1\n"));
    check(rejected(std::string(65537, 'a')));
    check(rejected(replaceOnce(valid, "secondary=usp", "secondary=none")));
    check(rejected(replaceOnce(valid, "armor=50", "armor=0")));
    check(rejected(replaceOnce(valid, "secondary=usp", "")));

    // An absent equipment section uses valid defaults and an idle action is legal.
    const std::string basic = R"([scenario]
version=aipb-scenario-v1
name=wait
map=de_dust2
timeout_ms=500
[bot]
index=0
team=1
x=-16
y=0
z=0
yaw=0
pitch=0
[action]
duration_ms=250
)";
    check(parseScenario(basic, "wait", result, error));
    check(result.equipment.primary.type == "none");
    check(!result.equipment.has("primary") && !result.equipment.has("defuse_kit"));
    check(!result.equipment.has("secondary_clip") && !result.equipment.has("health"));
    check(result.equipment.selected == "knife");
    check(result.actions[0].timeoutMs == 275);
    check(result.actions[0].forward == 0 && result.actions[0].strafe == 0);
    check(rejected(replaceOnce(basic, "duration_ms=250",
      "duration_ms=250\ntimeout_ms=249")));
    check(parseScenario(replaceOnce(basic, "duration_ms=250",
      "duration_ms=250\ntimeout_ms=250"), "wait", result, error));
    check(result.actions[0].timeoutMs == 250);

    // Omitted team allows either playing team, but never unassigned players.
    const auto eitherTeam = replaceOnce(basic, "team=1\n", "");
    check(parseScenario(eitherTeam, "wait", result, error));
    check(result.team == 0 && !result.hasBot("team"));
    check(rejected(replaceOnce(basic, "team=1", "team=0")));
    check(rejected(replaceOnce(basic, "team=1", "team=3")));

    // The implicit 10-percent margin is rounded up and handles the upper duration.
    check(parseScenario(replaceOnce(basic, "duration_ms=250",
      "duration_ms=101"), "wait", result, error));
    check(result.actions[0].timeoutMs == 112);
    check(parseScenario(replaceOnce(basic, "duration_ms=250",
      "duration_ms=1"), "wait", result, error));
    check(result.actions[0].timeoutMs == 2);
    const auto longAction = replaceOnce(replaceOnce(basic,
      "timeout_ms=500", "timeout_ms=15000"), "duration_ms=250", "duration_ms=10000");
    check(parseScenario(longAction, "wait", result, error));
    check(result.actions[0].timeoutMs == 11000);
    check(parseScenario(longAction + "timeout_ms=11000\n", "wait", result, error));
    check(result.actions[0].timeoutMs == 11000);
    check(rejected(longAction + "timeout_ms=11001\n", "wait"));

    // A new parser call cannot retain earlier action values.
    check(parseScenario(valid, "ground_forward_01", result, error));
    check(result.actions.size() == 2);
    check(!safeIdentifier("../escape"));
    check(!safeIdentifier("bad/name"));
    check(safeIdentifier("de_dust2"));
    check(!knownWeapon("defuser", true));
    return 0;
  }
  catch (const std::exception &e) {
    std::fprintf(stderr, "scenario contract: %s\n", e.what());
    return 1;
  }
}
