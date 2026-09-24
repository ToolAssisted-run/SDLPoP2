# Fleet runs (a many-core box)

Layout on the fleet machine: `~/fleet/{src,tools,tests}` (rsync of this repo), `explore` and `e2e` built there,
`spirit_pre.plan` (level 14: the prince turned into the spirit in room 7), the oracle build
(`chimera-core-dosbox-x`, branch pop2-tracer with `probepoke`) and `<workspace>/oracle/pop.hdd`.

1. `fleet.sh GAME_DIR`: 252 explorations (levels 1..14 x explorer seeds; hp 12, Ctrl presses; half of level 14 from
   the spirit prefix) -> `out/L<level>_<k>.{plan,log}`; logs list the rooms reached and any unreconstructed routine.
2. `capall.sh`: every plan (+100 idle ticks) through `capone.sh`: plan2script (POKE_HP=12) -> oracle capture ->
   strict e2e -> one line per run in `capall.out`.
