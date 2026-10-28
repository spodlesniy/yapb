"""Regression checks for the D218 weapon-alias mapping and diagnostic command."""

from pathlib import Path
import re
import unittest

ROOT = Path(__file__).resolve().parents[3]


class ExplorerGiveWeaponTests(unittest.TestCase):
    def test_aliases_are_unambiguous(self):
        source = (ROOT / "src/support.cpp").read_text(encoding="utf-8")
        entries = re.findall(
            r'\{\s*Weapon::(\w+),\s*AliasInfo\s*\{\s*"(\w+)"', source
        )
        self.assertGreaterEqual(len(entries), 25)
        aliases = [alias for _, alias in entries]
        self.assertEqual(len(aliases), len(set(aliases)))
        self.assertIn(("AWP", "awp"), entries)
        self.assertIn(("M249", "m249"), entries)
        self.assertIn(("M4A1", "m4a1"), entries)

    def test_giveweapon_disables_world_weapon_respawn(self):
        control = (ROOT / "src/control.cpp").read_text(encoding="utf-8")
        command = control.split("int BotControl::cmdGiveWeapon () {", 1)[1].split(
            "\nint BotControl::cmdVersion () {", 1
        )[0]
        self.assertRegex(command, r"constexpr int kWeaponNoRespawn = 1 << 30;")
        create = command.index("pfnCreateNamedEntity (classname.chars ())")
        flag = command.index("item->v.spawnflags |= kWeaponNoRespawn;")
        spawn = command.index("MDLL_Spawn (item)")
        touch = command.index("MDLL_Touch (item, bot->ent ())")
        self.assertLess(create, flag)
        self.assertLess(flag, spawn)
        self.assertLess(spawn, touch)

    def test_giveweapon_routes_only_to_game_pickup(self):
        control = (ROOT / "src/control.cpp").read_text(encoding="utf-8")
        support = (ROOT / "src/support.cpp").read_text(encoding="utf-8")
        self.assertIn('"giveweapon <bot_index> <alias>"', control)
        self.assertIn("util.weaponAliasToId (name)", control)
        self.assertIn("MDLL_Spawn (item)", control)
        self.assertIn("MDLL_Touch (item, bot->ent ())", control)
        self.assertIn("bot->pev->weapons & cr::bit (id)", control)
        self.assertIn('bot->issueCommand ("drop")', control)
        self.assertIn("return -1;", support)


if __name__ == "__main__":
    unittest.main()
