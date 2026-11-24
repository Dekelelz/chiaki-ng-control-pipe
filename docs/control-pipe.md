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
| `0x86` | BLOCKED | `u8 on`. Reserved; not sent yet. |
| `0x87` | PIN | none. The console asks for its login PIN. |

## What it does not do

It does not record, replay or generate input by itself, it does not read the game, and it changes
nothing about how the stream, registration or sign-in work. While this program's own menu is
open, input from the pipe is held back together with all other input.

## Licence

Like the rest of chiaki-ng: AGPL-3.0-only with the OpenSSL exception (`COPYING`). The code of this
addition is in `gui/include/controlchannel.h` and `gui/src/controlchannel.cpp`, with small hooks
in `gui/src/main.cpp`, `gui/src/streamsession.cpp` and `gui/include/streamsession.h`, each marked
"fork addition".
