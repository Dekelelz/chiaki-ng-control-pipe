# The control pipe

This is an addition made in this copy of chiaki-ng on 24 November 2026. It is not part of the
official chiaki-ng and the official project does not support it.

Started with `--control-pipe <name>`, the program connects to the Windows named pipe
`\\.\pipe\<name>` and takes controller state from whatever program created that pipe. That state
is combined with the keyboard and with any real controller, the same way two controllers are
combined: it acts as one more controller. Without the option, nothing in this addition runs.

```
chiaki.exe --control-pipe my-pipe-name stream "<console nickname>" <console address>
```

Options go before the command. `<name>` is 1 to 128 characters of `A-Z a-z 0-9 _ -`. Windows only.

## Who can use it

Any program can, on the same PC. The program that wants to send input creates the pipe first and
then starts this one with the pipe's name. This program only ever connects out to that one pipe;
it opens nothing that others could connect to, and it does nothing over the network for this.

If you write such a program, protect your pipe: allow only your own Windows user, refuse remote
clients (`PIPE_REJECT_REMOTE_CLIENTS`), allow one instance, use a random name for every start,
and check that the process that connects is the one you started (`GetNamedPipeClientProcessId`).

The program reconnects by itself: if the pipe isn't there yet, or its owner goes away, it looks
for it again twice a second. Whenever the owner is gone, the state from the pipe is cleared, so
no button stays held.

## Messages

Both directions use the same frame: **type** (1 byte), **length** (1 byte), then **length** bytes
of payload. Numbers are little endian. A side that meets a type it doesn't know skips it.

### From the other program

| Type | Name | Payload |
|---|---|---|
| `0x01` | HELLO | `u8 version` (this document is version 1). Send it first; state is ignored until it arrives. |
| `0x02` | STATE | `u16 buttons, u8 l2, u8 r2, i16 left_x, i16 left_y, i16 right_x, i16 right_y` (12 bytes) |
| `0x03` | BYE | none. Lets go of everything; the connection may stay open. |

**Buttons** use the common XInput bit layout, with the two bits that layout leaves free used for
the two buttons only a PlayStation controller has:

| Bit | Button | Bit | Button |
|---|---|---|---|
| `0x0001` | D-pad up | `0x0100` | L1 |
| `0x0002` | D-pad down | `0x0200` | R1 |
| `0x0004` | D-pad left | `0x0400` | PS |
| `0x0008` | D-pad right | `0x0800` | Touchpad click |
| `0x0010` | Options | `0x1000` | Cross |
| `0x0020` | Share / Create | `0x2000` | Circle |
| `0x0040` | L3 | `0x4000` | Square |
| `0x0080` | R3 | `0x8000` | Triangle |

**Triggers** are 0 to 255. Every change of a trigger value is sent to the console as its own
event, so prefer 0 and 255 over a stream of in-between values.

**Sticks** are -32767 to 32767; right is positive and **up is positive** (the program turns "up"
around for the console, which counts it as negative).

Send a STATE when something changes, and again a few times a second while nothing does.

### To the other program

| Type | Name | Payload |
|---|---|---|
| `0x81` | HELLO | `u8 version`. Sent as soon as the pipe is connected. |
| `0x82` | STREAM | `u8 on` (1 = the session is connected, 0 = it ended), `u8 reason` (a `ChiakiQuitReason`, 0 when on), then the console's reason as UTF-8 text, if any |
| `0x83` | RUMBLE | `u8 left, u8 right` |
| `0x84` | TRIGGERS | `u8 type_left, u8 type_right`, 10 bytes left, 10 bytes right (adaptive-trigger effects, as the console sent them) |
| `0x85` | HAPTICS | `u8 level`. Reserved; not sent yet. |
| `0x86` | BLOCKED | `u8 on`. 1 while this program holds all input back (its own menu is open), 0 when input passes again. Also sent once right after each STREAM with on = 1, with the state at that moment. |
| `0x87` | PIN | none. The console asks for its login PIN. |

## What it does not do

It does not record, replay or generate input by itself, it does not read the game, and it changes
nothing about how the stream, registration or sign-in work. While another program is attached to the
pipe, this program's own keyboard-as-controller and mouse-as-touchpad mapping stands aside, so input has
one source; its menus and shortcuts work as always. While this program's own menu is
open, input from the pipe is held back together with all other input.

Closing the window during a stream while it was started with `--control-pipe` ends the stream
properly and then the program itself, instead of going back to the console list. The "Action on
Disconnect" setting is not used then: no question is shown and the console is not put to sleep,
because the window may be closed by the other program and not by a person looking at it. This
holds for every way of closing it, the stream menu's close button included. A stream that ends any
other way (the console went to rest mode, the network dropped) still goes back to the console list.

Started with `--control-pipe`, the log describes the first picture of each stream once
("Control pipe: first picture ..."): its size, pixel format, colour range, colour space, primaries
and transfer, as the decoder reported them. Nothing is sent anywhere.

Started with `--control-pipe`, this program keeps its settings, registered consoles and logs under
its own name, `chiaki-ng-control-pipe`, instead of chiaki-ng's. A chiaki-ng installed on the same
computer is never read or changed, and a console has to be registered once for this program too.

Started with `--control-pipe`, the console list asks fewer questions, because one click in the other
program already meant "play": when no PSN sign-in is saved, the PSN sign-in opens straight away instead
of the "Would you like to connect to PSN?" question (the Steam shortcut question isn't shown), and once
per run, when exactly one PS5 is found on this network (awake or in rest mode), it is opened as a
double-click would open it: connected, woken from rest mode first, or linked if it isn't yet. With two
or more, the player picks one as before. The log says "Control pipe: opening the one PS5 found". It
happens once per run only: after a stream ends, a wake that failed or a cancelled wait, the player picks, so
a PS5 put to rest from its own menu is never woken again by itself.

One change in this fork applies with or without the pipe: when the console answers that Remote Play
is already in use, this program reports that at once and does not ask again. (Other failures before
the first connection are retried as before.)

## Licence

Like the rest of chiaki-ng: AGPL-3.0-only with the OpenSSL exception (`COPYING`). The code of this
addition is in `gui/include/controlchannel.h` and `gui/src/controlchannel.cpp`, with small hooks
in `gui/src/main.cpp`, `gui/src/streamsession.cpp`, `gui/include/streamsession.h`,
`gui/src/qmlbackend.cpp`, `gui/include/qmlbackend.h` and `gui/src/qml/MainView.qml`, each marked
"fork addition".
