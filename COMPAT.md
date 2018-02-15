# Vectrexy Compatibility List

## Status Values
* Great
* Good
* Meh
* MissingOp
* ViaNotImplemented
* InvalidRead
* InvalidWrite
* IllegalInstr
* MaybeMusicOnly
* NoBoot - Black screen after title screen, or doesn't progress further

## Commercial Released Roms

| Rom                                                          | Status            | Comment                                                   |
| ------------------------------------------------------------ | ----------------- | --------------------------------------------------------- |
| 3-D Mine Storm (1983)                                        | NoBoot            | [VecxSame]                                                |
| 3-D Mine Storm (1983) [b1]                                   | ViaNotImplemented | Read InterruptEnable not implemented                      |
| 3-D Narrow Escape (1983)                                     | NoBoot            | [VecxSame]                                                |
| 3-D Narrow Escape (1983) [b1]                                | NoBoot            |                                                           |
| 3-D Narrow Escape (1983) [b2]                                | NoBoot            |                                                           |
| 3-D Narrow Escape (1983) [b3]                                | InvalidWrite      | Write to unmapped range of value $86 at address $c7ff     |
| Animaction (1983) (light pen)                                | InvalidWrite      | Writes to Cartridge ROM not allowed                       |
| Animaction (1983) (light pen) [b1]                           | ViaNotImplemented | Also IllegalInstr but maybe bug in cart (vecx same).      |
| Armor Attack (1982)                                          | Great             |                                                           |
| Armor Attack (1982) (Spinner Hack)                           | InvalidRead       | [VecxSame] Invalid Cartridge read at $10ad                |
| Armor Attack (1982) (Spinner Hack) [a1]                      | InvalidRead       | Invalid Cartridge read at $10ad                           |
| Armor Attack (1982) [b1]                                     | MissingOp         | Unhandled Op: SWI                                         |
| Art Master (1983) (light pen)                                | Good?             | Can't get past the first screen, maybe emulate light pen? |
| Art Master (1983) (light pen) [b1]                           | InvalidRead       | Invalid Cartridge read at $2803                           |
| Bedlam (1983)                                                | MissingOp         | Unhandled Op: CWAI                                        |
| Bedlam (1983) (Spinner Hack)                                 | MissingOp         | Unhandled Op: CWAI                                        |
| Bedlam (1983) [b1]                                           | InvalidRead       | [VecxSame] Invalid Cartridge read at $0ea3                |
| Bedlam (1983) [b2]                                           | ViaNotImplemented | A without handshake not implemented yet                   |
| Bedlam (1983) [b3]                                           | MissingOp         | Unhandled Op: CWAI                                        |
| Berzerk (1982)                                               | Good              | Characters are squished                                   |
| Berzerk (1982) (Bugfixed Prototype)                          | UNTESTED          |                                                           |
| Berzerk (1982) (Bugfixed Prototype) [o1]                     | UNTESTED          |                                                           |
| Blitz! - Action Football (1982)                              | Great             |                                                           |
| Blitz! - Action Football (1982) [b1]                         | UNTESTED          |                                                           |
| Blitz! - Action Football (1982) [b2]                         | UNTESTED          |                                                           |
| Clean Sweep (1982)                                           | InvalidRead       | [VecxSame] Invalid Cartridge read at $1051                |
| Clean Sweep (1982) [b1]                                      | InvalidRead       | Invalid Cartridge read at $4d20                           |
| Clean Sweep (Mr Boston Version) (1982)                       | InvalidRead       | Invalid Cartridge read at $20cc                           |
| Cosmic Chasm (1982)                                          | Great             |                                                           |
| Fortress of Narzod (1982)                                    | Great             |                                                           |
| Fortress of Narzod (1982) [b1]                               | MissingOp         | Unhandled Op: SWI                                         |
| Heads Up - Action Soccer (1983)                              | Great             |                                                           |
| Heads Up - Action Soccer (1983) [b1]                         | NoBoot            | [VecxSame] Stuck on title screen.                         |
| Hyperchase (1982)                                            | Great             |                                                           |
| Hyperchase (1982) [b1]                                       | IllegalInstr      | cpuOp.addrMode != AddressingMode::Illegal                 |
| Melody Master (1983) (light pen)                             | UNTESTED          |                                                           |
| Melody Master (1983) (light pen) [b1]                        | UNTESTED          |                                                           |
| Mine Storm (1982)                                            | Great             |                                                           |
| Mine Storm (1982) (Karrsoft Hack) [h1]                       | Great             |                                                           |
| Mine Storm (1982) (RLB Hack) [b1]                            | InvalidRead       | [VecxSame] Invalid Cartridge read at $7fc8                |
| Mine Storm (1982) (RLB Hack) [b1][o1]                        | InvalidRead       | Invalid Cartridge read at $7fc8                           |
| Mine Storm (1982) (RLB Hack) [b1][o2]                        | InvalidRead       | Invalid Cartridge read at $7fc8                           |
| Mine Storm (1982) (RLB Hack) [h1]                            | Good              | Invalid Cartridge read at $3409 on shoot spawned enemy    |
| Mine Storm (1982) (RLB Hack) [o1]                            | Good              |                                                           |
| Mine Storm (1982) (RLB Hack) [o2]                            | Good              |                                                           |
| Mine Storm II (1983)                                         | Good              |                                                           |
| Mine Storm II (1983) (Fred Taft Hack) [h1]                   | UNTESTED          |                                                           |
| Mine Storm II (1983) (Spinner Hack)                          | UNTESTED          |                                                           |
| Narrow Escape (2-D Version by Fred Taft) (1983) [h1]         | Great             |                                                           |
| Polar Rescue (1983)                                          | InvalidRead       | [VecxSame] Invalid Cartridge read at $2055                |
| Polar Rescue (1983) [a1]                                     | UNTESTED          |                                                           |
| Polar Rescue (1983) [a1][o1]                                 | UNTESTED          |                                                           |
| Polar Rescue (1983) [b1]                                     | UNTESTED          |                                                           |
| Pole Position (1982)                                         | Great             | Invalid reads (code error).                               |
| Pole Position (1982) (Spinner Hack)                          | UNTESTED          |                                                           |
| Pole Position (1982) [b1]                                    | UNTESTED          |                                                           |
| Pole Position (1982) [f1]                                    | UNTESTED          |                                                           |
| Rip-Off (1982)                                               | Great             |                                                           |
| Rip-Off (1982) [b1]                                          | UNTESTED          |                                                           |
| Scramble (1982)                                              | Great             | Invalid Cartridge read at $1000 (code error?)             |
| Scramble (1982) [b1]                                         | IllegalInstr      | [VecxSame] Lots of errors. Investigate.                   |
| Solar Quest (1982)                                           | Great             |                                                           |
| Solar Quest (1982) (Spinner Hack)                            | UNTESTED          |                                                           |
| Solar Quest (1982) [b1]                                      | UNTESTED          |                                                           |
| Space Wars (1982)                                            | Great             |                                                           |
| Space Wars (1982) [b1]                                       | UNTESTED          |                                                           |
| Spike (1983)                                                 | Great             |                                                           |
| Spinball (1983)                                              | Great             |                                                           |
| Spinball (1983) [b1]                                         | UNTESTED          |                                                           |
| Spinball (1983) [b2]                                         | UNTESTED          |                                                           |
| Star Castle (1983)                                           | Great             |                                                           |
| Star Castle (1983) (Spinner Hack)                            | UNTESTED          |                                                           |
| Star Castle (1983) [b1]                                      | UNTESTED          |                                                           |
| Star Hawk (1982)                                             | Great             |                                                           |
| Star Hawk (1982) [b1]                                        | UNTESTED          |                                                           |
| Star Ship (1982)                                             | Great             |                                                           |
| Star Trek - The Motion Picture (1982)                        | Good              | Font render not perfect, bars crooked at bottom           |
| Star Trek - The Motion Picture (1982) (controller hack) [h1] | UNTESTED          |                                                           |
| Star Trek - The Motion Picture (1982) [a1]                   | UNTESTED          |                                                           |
| Star Trek - The Motion Picture (1982) [b1]                   | UNTESTED          |                                                           |
| Star Trek - The Motion Picture (1982) [o1]                   | UNTESTED          |                                                           |
| Web Warp (1983)                                              | Great             |                                                           |
| Web Wars (1983)                                              | UNTESTED          |                                                           |
| Web Wars (1983) [b1]                                         | UNTESTED          |                                                           |

## Commercial Unreleased Roms

| Rom                                                 | Status   | Comment   |
| --------------------------------------------------- | -------- | --------- |
| Dark Tower (1983) (Prototype)                       | Good?    | Not sure. |
| Dark Tower (1983) (Prototype) (Fred Taft Hack) [h1] | UNTESTED |           |
| Dark Tower (1983) (Prototype) [b1]                  | UNTESTED |           |
| Dark Tower (1983) (Prototype) [b2]                  | UNTESTED |           |
| Dark Tower (1983) (Prototype) [b3]                  | UNTESTED |           |
| Dual Vectrex Test #1 (1983) (PD)                    | UNTESTED |           |
| Dual Vectrex Test #1 (1983) (PD) [b1]               | UNTESTED |           |
| Dual Vectrex Test #2 (1983) (PD)                    | UNTESTED |           |
| Dual Vectrex Test #2 (1983) (PD) [b1]               | UNTESTED |           |
| Engine Analyzer (1983) (light pen)                  | UNTESTED |           |
| Spectrum I+ Demo (1982) (PD)                        |          |           |
| Test Rev. 4 (1982) (PD)                             |          |           |
| Test Rev. 4 (1982) (PD) [b1]                        |          |           |
| Tour De France (1983) (Prototype)                   |          |           |
| Vectrex BIOS (1982)                                 |          |           |

## Homebrew Roms

| Rom                                                          | Status         | Comment                                                          |
| ------------------------------------------------------------ | -------------- | ---------------------------------------------------------------- |
| 'NO' Sound by Chris Salomon (1998) (PD)                      | Good           | Text render incomplete                                           |
| 0ldsk00l Demo by Manu (2002) (PD)                            | NoBoot         |                                                                  |
| 3-D Demo by Chris (Mar 06) (2000) (PD)                       | InvalidRead    | Read from unmapped range at address $a813                        |
| 3-D Demo by Chris (Mar 08) (2000) (PD)                       | InvalidRead    | Read from unmapped range at address $c5d8                        |
| 3-D Scrolling Demo by Christopher Tumber (2001) (PD)         | Meh            | Mountain renders at first, but moving right screws up render     |
| 4-D Rotating Cube Demo (19xx) (PD)                           | Great          |                                                                  |
| 8Ball by Christopher Tumber (2001) (PD)                      | Good           | Text renders off screen                                          |
| Abyss Demo (1999) (PD)                                       | Good           | Writes to Cartridge ROM not allowed                              |
| All Good Things by John Dondzila (1996)                      | Good           | Writes to Cartridge ROM not allowed, small font render at bottom |
| All Your Base Demo by Manu (2001) (PD)                       | Great          |                                                                  |
| Alpha Demo (2001) (PD)                                       | Good?          | Only prints A to H                                               |
| Animation Demo by Manu (2002) (PD)                           | Good?          | Just a star spinning                                             |
| Arkanoid - Revenge of DOH Sound by Chris Salomon (1998) (PD) | NoBoot         |                                                                  |
| Arsek by Marq (2000) (PD)                                    | Good?          | Renders a skeleton face                                          |
| Axel F - Beverly Hills Cop by Chris Salomon (1998) (PD)      | MaybeMusicOnly |                                                                  |
| Axel F - Beverly Hills Cop by Chris Salomon (1998) (PD) [a1] | MaybeMusicOnly |                                                                  |
| Bach Prelude #1 by Jeff Woolsey (1985) (PD)                  | MaybeMusicOnly |                                                                  |
| Battle Earth Terror Hazard & Vecsports Boxing (2000) (PD)    | Great          |                                                                  |
| Battle Earth Terror Hazard V1 by Manu (2000) (PD)            | Good           | Bottom renders squished                                          |
| Battle Earth Terror Hazard V2 by Manu (2000) (PD)            | IllegalInstr   | cpuOp.addrMode != AddressingMode::Illegal                        |
| Battle Earth Terror Hazard V3 by Manu (2000) (PD)            | IllegalInstr   | cpuOp.addrMode != AddressingMode::Illegal                        |
| Battle Earth Terror Hazard V4 by Manu (2000) (PD)            | Good           | Bottom render cut off                                            |
| Battlezone Demo by Christopher Tumber (2001) (PD)            | Meh            | Rendering glitches: looks like beam off not working near edge    |
| BB2 Demo by Manu (2002) (PD)                                 | Good           | Font render glitched                                             |
| BCor-FTS Demo by Clay Cowgill (1997) (PD)                    | Good           | Bottom render cut off                                            |
| Bebop Best Sound by Chris Salomon (1998) (PD)                | MaybeMusicOnly |                                                                  |
| Bebop Sound by Chris Salomon (1998) (PD)                     | MaybeMusicOnly |                                                                  |
| Birds of Prey by John Dondzila (1999)                        | Good           | Invalid Cartridge read at $1bf4 (cart bug?)                      |
| Bubble Bobble (Diamond Room) by Chris Salomon (1998) (PD)    | MaybeMusicOnly |                                                                  |
| Bubble Bobble Sound 0 by Chris Salomon (1998) (PD)           | MaybeMusicOnly |                                                                  |
| Bubble Bobble Sound 1 by Chris Salomon (1998) (PD)           | MaybeMusicOnly |                                                                  |
| Bubble Bobble Sound 2 by Chris Salomon (1998) (PD)           | MaybeMusicOnly |                                                                  |
| Bubble Bobble Sound 2best by Chris Salomon (1998) (PD)       | MaybeMusicOnly |                                                                  |
| Bubble Bobble Sound 3 by Chris Salomon (1998) (PD)           | MaybeMusicOnly |                                                                  |
| Calibration Demo (19xx) (Christopher Tumber) (PD)            | Great          |                                                                  |
| CGM Example by Marq (2000) (PD)                              | Good?          |                                                                  |
| Collision Test by Manu (2000) (PD)                           | Great          |                                                                  |
| Commando - High Score Sound by Chris Salomon (1998) (PD)     | MaybeMusicOnly |                                                                  |
| Count05 Sound by Chris Salomon (1998) (PD)                   | MaybeMusicOnly |                                                                  |
| Curved Lines Demo (1998) (PD)                                | Good?          | Guessing rendering isn't exactly right                           |
| Disc Duel Demo (1997) (PD)                                   | InvalidWrite   | Writes to Cartridge ROM not allowed                              |
| Dr. Who Theme by Chris Salomon (1998) (PD)                   | MaybeMusicOnly |                                                                  |
| Etch-a-Sketch by Jeff Woolsey (1985) (light pen) (PD)        | UNTESTED       |                                                                  |
| Etch-a-Sketch by Jeff Woolsey (1985) (light pen) (PD) [b1]   | UNTESTED       |                                                                  |
| Exec Rom Dumper (2000) (Ronen Habot)                         | Good           | Bottom render cut off                                            |
| Fast Food Sound 1 by Chris Salomon (1998) (PD)               | MaybeMusicOnly |                                                                  |
| Fast Food Sound 2 by Chris Salomon (1998) (PD)               | MaybeMusicOnly |                                                                  |
| Ghosts 'n' Goblins Sound 7 by Chris Salomon (1998) (PD)      |                |                                                                  |
| Ghosts 'n' Goblins Sound 8 by Chris Salomon (1998) (PD)      |                |                                                                  |
| Ghosts 'n' Goblins Sound 9 by Chris Salomon (1998) (PD)      |                |                                                                  |
| Ghosts 'n' Goblins Sound 9best by Chris Salomon (1998) (PD)  |                |                                                                  |
| Ghouls 'n' Ghosts Sound by Chris Salomon (1998) (PD)         |                |                                                                  |
| Gravitrex by John Dondzila (2002)                            | Great          |                                                                  |
| Great Giana Sisters (Bonus) by Chris Salomon (1998) (PD)     |                |                                                                  |
| Great Giana Sisters (Ingame) by Chris Salomon (1998) (PD)    |                |                                                                  |
| Great Giana Sisters (Title) by Chris Salomon (1998) (PD)     |                |                                                                  |
| Hey, Music Lover Sound by Chris Salomon (1998) (PD)          |                |                                                                  |
| Jaws Theme Music by Chris Salomon (1998) (PD)                |                |                                                                  |
| Joystick Demo by Manu (2000) (PD)                            |                |                                                                  |
| Klax (Level Begin) Sound by Chris Salomon (1998) (PD)        |                |                                                                  |
| Labyrinth Rev 1 by Chris Salomon (1998) (PD)                 |                |                                                                  |
| Labyrinth Rev 2 by Chris Salomon (1998) (PD)                 |                |                                                                  |
| Labyrinth Rev 3 by Chris Salomon (1998) (PD)                 |                |                                                                  |
| Labyrinth Rev 4 (Vector Rouge) by Chris Salomon (1998) (PD)  |                |                                                                  |
| Line Demo (2001) (PD)                                        |                |                                                                  |
| Lines and Digitized Sounds Demo (1998) (PD)                  |                |                                                                  |
| Lunar Lander Demo by Thomas Landspurg (19xx) (PD)            |                |                                                                  |
| Mad Planetoids Demo by John Dondzila (2001) (PD)             |                |                                                                  |
| Madonna's Get Into the Groove by Chris Salomon (1998) (PD)   |                |                                                                  |
| Mike's Molecules Demo by Mike Blackwell (1985) (PD)          |                |                                                                  |
| Mike's Molecules Demo by Mike Blackwell (1985) (PD) [a1]     |                |                                                                  |
| Mike's Molecules Demo by Mike Blackwell (1985) (PD) [b1]     |                |                                                                  |
| Misfits 1 Music by Chris Salomon (1998) (PD)                 |                |                                                                  |
| Missile Lords by Christopher Tumber (2001) (PD)              |                |                                                                  |
| Monty on the Run HiScore Sound by Chris Salomon (1998) (PD)  |                |                                                                  |
| Monty on the Run Sound by Chris Salomon (1998) (PD)          |                |                                                                  |
| Monty on the Run Sound by Chris Salomon (1998) (PD) [a1]     |                |                                                                  |
| Moon Lander Demo by Clay Cowgill (1997) (PD)                 |                |                                                                  |
| Moon Lander Demo by Clay Cowgill (1997) (PD) [a1]            |                |                                                                  |
| Music Demo (2001) (PD)                                       |                |                                                                  |
| My Bonnie Lies Over The Ocean by Jeff Woolsey (1985) (PD)    |                |                                                                  |
| 'NO' Sound by Chris Salomon (1998) (PD)                      |                |                                                                  |
| Nona3 by Manu (Mar 10) (2003)                                |                |                                                                  |
| Omega Chase (Final Version) (1998) (PD)                      |                |                                                                  |
| Omega Chase by Christopher Tumber (2000) (PD)                |                |                                                                  |
| Pac-Mania Sound 1 by Chris Salomon (1998) (PD)               |                |                                                                  |
| Pac-Mania Sound 2 by Chris Salomon (1998) (PD)               |                |                                                                  |
| Pac-Mania Sound 4 by Chris Salomon (1998) (PD)               |                |                                                                  |
| Patriots by John Dondzila (1996)                             |                |                                                                  |
| Patriots by John Dondzila (1996) [a1]                        |                |                                                                  |
| Patriots by John Dondzila (1996) [b1]                        |                |                                                                  |
| Patriots III - BALListics Busters by John Dondzila (1996)    |                |                                                                  |
| Patriots III - BALListics Busters by John Dondzila (2002)    |                |                                                                  |
| Patriots Remix by John Dondzila (1999)                       |                |                                                                  |
| Philly Classic 3 Demo (2002) (PD)                            |                |                                                                  |
| Pokemon Demo 1 by Manu (2000) (PD)                           |                |                                                                  |
| Pokemon Demo 2 by Manu (2000) (PD)                           |                |                                                                  |
| Pokemon Demo 3 by Manu and Marq (2000) (PD)                  |                |                                                                  |
| Pokemon Demo 4 - Psyduck's Eyes by Manu (2000) (PD)          |                |                                                                  |
| POP Demo by Christopher Tumber (19xx) (PD)                   |                |                                                                  |
| Pythagorian Theorem by Rob Mitchell (2002) (PD)              |                |                                                                  |
| Rainbow Islands Sound by Chris Salomon (1998) (PD)           |                |                                                                  |
| Real Ghostbusters Sound by Chris Salomon (1998) (PD)         |                |                                                                  |
| Repulse by John Dondzila (1999)                              |                |                                                                  |
| Robin Hood Sound by Chris Salomon (1998) (PD)                |                |                                                                  |
| Rockaroids Remix - 3rd Rock by John Dondzila (1996)          |                |                                                                  |
| Rockaroids Remix - 3rd Rock by John Dondzila (1996) [a1]     |                |                                                                  |
| Rocket Sledge Demo (1985) (PD)                               |                |                                                                  |
| ROM Music Demo by Manu (2000) (PD)                           |                |                                                                  |
| Ronen's Game Cart (2000) (Ronen Habot)                       |                |                                                                  |
| Rotation Demo by Manu (2000) (PD)                            |                |                                                                  |
| Rounders by Ronen Habot (2000) (PD)                          |                |                                                                  |
| SAW Gallery 1 by Christopher Tumber (1999) (PD)              |                |                                                                  |
| SAW Gallery 2 by Christopher Tumber (1999) (PD)              |                |                                                                  |
| SAW Gallery 3 by Christopher Tumber (1999) (PD)              |                |                                                                  |
| Scorefont 3 by Manu (Aug 26) (2002)                          |                |                                                                  |
| Song Demo by Christopher Tumber (1998) (PD)                  |                |                                                                  |
| Sound1 Demo (19xx) (PD)                                      |                |                                                                  |
| Space16 Demo by Manu (2000) (PD)                             |                |                                                                  |
| Spike Goes Skiing (1998) (PD)                                |                |                                                                  |
| Spike Goes Skiing Demo V.03 (1998) (PD)                      |                |                                                                  |
| Spike Hoppin' by John Dondzila (1998)                        |                |                                                                  |
| Spike Hoppin' by John Dondzila (1998) [b1]                   |                |                                                                  |
| Spike Hoppin' by John Dondzila (1998) [b2]                   |                |                                                                  |
| Spike's Slam Pit Demo by Gauze (2001) (PD)                   |                |                                                                  |
| Spike's Water Balloons (Analog) by John Dondzila (2001) (PD) |                |                                                                  |
| Star Fire Spirits by John Dondzila (1999)                    |                |                                                                  |
| Star Fire Spirits by John Dondzila (1999) [a1]               |                |                                                                  |
| Star Seige by John Dondzila (1999)                           |                |                                                                  |
| Star Spangles Banner Music Demo by Jeff Woolsey (1985) (PD)  |                |                                                                  |
| Star Wars Sound by Chris Salomon (1998) (PD)                 |                |                                                                  |
| Star Wars Theme by Chris Salomon (1998) (PD)                 |                |                                                                  |
| Tank Demo 1 (19xx) (PD)                                      |                |                                                                  |
| Tank Demo 2 (2001) (PD)                                      |                |                                                                  |
| Taulukko Demo by Manu (2002) (PD)                            |                |                                                                  |
| Text Demo (2001) (PD)                                        |                |                                                                  |
| This is Just a Non-Playable Demo (1999) (PD)                 |                |                                                                  |
| Tono Demo by Marq & Antti (2000) (PD)                        |                |                                                                  |
| Tsunami by Christopher Tumber (2001) (PD)                    |                |                                                                  |
| Tsunami by Christopher Tumber (2001) (PD) [a1]               |                |                                                                  |
| Tsunami by Christopher Tumber (2001) (PD) [a2]               |                |                                                                  |
| Turrican 1 Sound by Chris Salomon (1998) (PD)                |                |                                                                  |
| Turrican 2 Sound by Chris Salomon (1998) (PD)                |                |                                                                  |
| Vaboom! by Ronen Habot (2000) (PD)                           |                |                                                                  |
| Vaboom! by Ronen Habot (2000) (PD) [a1]                      |                |                                                                  |
| Vecmania Part 1 (1999) (PD)                                  |                |                                                                  |
| Vecmania Part 1 (1999) (PD) [a1]                             |                |                                                                  |
| Vecmania Part 2 (1999) (PD)                                  |                |                                                                  |
| Vecsports Boxing Demo by Manu (2000) (PD)                    |                |                                                                  |
| Vecsports Boxing With Sound Demo by Manu (2000) (PD)         |                |                                                                  |
| VecSports Kingpin Bowling by Manu (V0.2) (2002)              |                |                                                                  |
| VecSports Kingpin Bowling by Manu (V0.3) (2002)              |                |                                                                  |
| VecSports Kingpin Bowling by Manu (V0.4) (Aug 26) (2002)     |                |                                                                  |
| VecSports Kingpin Bowling Demo by Manu (Aug 19) (2002)       |                |                                                                  |
| Vectopia by John Dondzila (2001) (PD)                        |                |                                                                  |
| Vector Vaders by John Dondzila (1996)                        |                |                                                                  |
| Vector Vaders by John Dondzila (1996) [a1]                   |                |                                                                  |
| Vector Vaders by John Dondzila (1996) [b1]                   |                |                                                                  |
| Vector Vaders Remix by John Dondzila (1999)                  |                |                                                                  |
| Vectrace (2000) (Ronen Habot)                                |                |                                                                  |
| Vectrace (2000) (Ronen Habot) [a1]                           |                |                                                                  |
| Vectrace (2000) (Ronen Habot) [a2]                           |                |                                                                  |
| Vectrex Bootstrap Loader by Jeff Woolsey (1985) (PD)         |                |                                                                  |
| Vectrex Demo 1 by Manu (2000) (PD)                           |                |                                                                  |
| Vectrex Demo 2 by Manu (2000) (PD)                           |                |                                                                  |
| Vectrex Demo 3 by Manu (2000) (PD)                           |                |                                                                  |
| Vectrex Maze by Chris Salomon (1998) (PD)                    |                |                                                                  |
| Vectrex Pong (1998) (PD)                                     |                |                                                                  |
| Vectrexians (1999) (PD)                                      |                |                                                                  |
| VecVoice Demo by Richard Hutchinson (2002)                   |                |                                                                  |
| Version Nine by Christopher Tumber (1999) (PD)               |                |                                                                  |
| Version Nine by Christopher Tumber (1999) (PD) [a1]          |                |                                                                  |
| Version Nine by Christopher Tumber (2000) (PD)               |                |                                                                  |
| Verzerk by Alex Herbert (2002)                               |                |                                                                  |
| Vexperience - B.E.T.H. & Vecsports Boxing by Manu (2000)     |                |                                                                  |
| V-Frogger by Chris Salomon (1998) (PD)                       |                |                                                                  |
| V-Frogger by Chris Salomon (1998) (PD) [a1]                  |                |                                                                  |
| V-Frogger by Chris Salomon, Sound by Kurt Woloch (2001) (PD) |                |                                                                  |
| Vimpula by Manu (2002) (PD)                                  |                |                                                                  |
| We Wish You a Merry Christmas by J. Woolsey (1985) (PD)      |                |                                                                  |
| We Wish You a Merry Christmas by J. Woolsey (1985) (PD) [a1] |                |                                                                  |
| We Wish You a Merry Christmas by J. Woolsey (1986) (PD)      |                |                                                                  |
| Wormhole by John Dondzila (2001) (PD)                        |                |                                                                  |
| YM001 Sound by Chris Salomon (1998) (PD)                     |                |                                                                  |
| Zombies from the Crypt Music by Chris Salomon (1998) (PD)    |                |                                                                  |
| ZZAP - Scrolling Space Game Demo (1989) (PD)                 |                |                                                                  |
