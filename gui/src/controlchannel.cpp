// SPDX-License-Identifier: LicenseRef-AGPL-3.0-only-OpenSSL
//
// Not part of upstream chiaki-ng. Added in the chiaki-ng-control-pipe fork on 2026-11-24.
// See include/controlchannel.h and docs/control-pipe.md.

#include <controlchannel.h>

#include <QByteArray>
#include <QMutexLocker>

#include <cstring>
#include <string>

#ifdef _WIN32
#include <windows.h>
#endif

// Messages are: type (1 byte), length (1 byte), payload (length bytes), little endian.
enum : uint8_t
{
	// from the other program
	MSG_HELLO = 0x01,     // u8 version
	MSG_STATE = 0x02,     // u16 buttons, u8 l2, u8 r2, i16 lx, i16 ly, i16 rx, i16 ry
	MSG_BYE = 0x03,       // let go of everything
	// to the other program
	EVT_HELLO = 0x81,     // u8 version
	EVT_STREAM = 0x82,    // u8 on, u8 reason, utf-8 text
	EVT_RUMBLE = 0x83,    // u8 left, u8 right
	EVT_TRIGGERS = 0x84,  // u8 type_left, u8 type_right, 10 bytes left, 10 bytes right
	EVT_HAPTICS = 0x85,   // u8 level (not sent yet)
	EVT_BLOCKED = 0x86,   // u8 on
	EVT_PIN = 0x87        // nothing
};

static ControlChannel *g_instance = nullptr;

ControlChannel *ControlChannel::Instance()
{
	return g_instance;
}

bool ControlChannel::Create(const QString &pipe_name)
{
#ifdef _WIN32
	if(g_instance)
		return false;
	if(pipe_name.isEmpty() || pipe_name.size() > 128)
		return false;
	for(const QChar c : pipe_name)
	{
		const ushort u = c.unicode();
		const bool ok = (u >= 'A' && u <= 'Z') || (u >= 'a' && u <= 'z') || (u >= '0' && u <= '9') || u == '_' || u == '-';
		if(!ok)
			return false;   // nothing that could point outside \\.\pipe\ on this PC
	}
	g_instance = new ControlChannel(pipe_name);
	return true;
#else
	(void)pipe_name;
	return false;
#endif
}

ControlChannel::ControlChannel(const QString &pipe_name)
	: QObject(nullptr), name(pipe_name)
{
	chiaki_controller_state_set_idle(&state);
#ifdef _WIN32
	stop_event = CreateEventW(nullptr, TRUE, FALSE, nullptr);
	thread = std::thread([this] { Run(); });
#endif
}

ControlChannel::~ControlChannel()
{
	stop.store(true);
#ifdef _WIN32
	if(stop_event)
		SetEvent((HANDLE)stop_event);
	if(thread.joinable())
		thread.join();
	if(stop_event)
		CloseHandle((HANDLE)stop_event);
#endif
}

ChiakiControllerState ControlChannel::State()
{
	QMutexLocker locker(&state_mutex);
	return state;
}

void ControlChannel::SetState(const ChiakiControllerState *new_state)
{
	{
		QMutexLocker locker(&state_mutex);
		if(new_state)
			state = *new_state;
		else
			chiaki_controller_state_set_idle(&state);
	}
	emit StateChanged();
}

// The state message uses the common XInput button layout, plus two bits that layout leaves
// free for the two buttons only a PlayStation controller has. Sticks: right and up are positive.
static void DecodeState(const uint8_t *p, ChiakiControllerState *out)
{
	chiaki_controller_state_set_idle(out);
	const uint16_t b = (uint16_t)(p[0] | (p[1] << 8));
	uint32_t buttons = 0;
	if(b & 0x0001) buttons |= CHIAKI_CONTROLLER_BUTTON_DPAD_UP;
	if(b & 0x0002) buttons |= CHIAKI_CONTROLLER_BUTTON_DPAD_DOWN;
	if(b & 0x0004) buttons |= CHIAKI_CONTROLLER_BUTTON_DPAD_LEFT;
	if(b & 0x0008) buttons |= CHIAKI_CONTROLLER_BUTTON_DPAD_RIGHT;
	if(b & 0x0010) buttons |= CHIAKI_CONTROLLER_BUTTON_OPTIONS;
	if(b & 0x0020) buttons |= CHIAKI_CONTROLLER_BUTTON_SHARE;
	if(b & 0x0040) buttons |= CHIAKI_CONTROLLER_BUTTON_L3;
	if(b & 0x0080) buttons |= CHIAKI_CONTROLLER_BUTTON_R3;
	if(b & 0x0100) buttons |= CHIAKI_CONTROLLER_BUTTON_L1;
	if(b & 0x0200) buttons |= CHIAKI_CONTROLLER_BUTTON_R1;
	if(b & 0x0400) buttons |= CHIAKI_CONTROLLER_BUTTON_PS;
	if(b & 0x0800) buttons |= CHIAKI_CONTROLLER_BUTTON_TOUCHPAD;
	if(b & 0x1000) buttons |= CHIAKI_CONTROLLER_BUTTON_CROSS;
	if(b & 0x2000) buttons |= CHIAKI_CONTROLLER_BUTTON_MOON;
	if(b & 0x4000) buttons |= CHIAKI_CONTROLLER_BUTTON_BOX;
	if(b & 0x8000) buttons |= CHIAKI_CONTROLLER_BUTTON_PYRAMID;
	out->buttons = buttons;
	out->l2_state = p[2];
	out->r2_state = p[3];
	auto axis = [p](int i) { return (int16_t)(uint16_t)(p[i] | (p[i + 1] << 8)); };
	// the console's "up" is negative
	auto flip = [](int16_t v) { return (int16_t)(v == INT16_MIN ? INT16_MAX : -v); };
	out->left_x = axis(4);
	out->left_y = flip(axis(6));
	out->right_x = axis(8);
	out->right_y = flip(axis(10));
}

void ControlChannel::HandleMessage(uint8_t type, const uint8_t *payload, size_t size)
{
	switch(type)
	{
		case MSG_HELLO:
			attached.store(true);
			break;
		case MSG_STATE:
			if(size >= 12 && attached.load())
			{
				ChiakiControllerState s;
				DecodeState(payload, &s);
				SetState(&s);
			}
			break;
		case MSG_BYE:
			SetState(nullptr);
			break;
		default:
			break;   // a newer program may send more; skip what we don't know
	}
}

void ControlChannel::SendStream(bool on, uint8_t reason, const QString &reason_str)
{
	QByteArray p;
	p.append((char)(on ? 1 : 0));
	p.append((char)reason);
	p.append(reason_str.toUtf8().left(200));
	Write(EVT_STREAM, (const uint8_t *)p.constData(), (size_t)p.size());
}

void ControlChannel::SendRumble(uint8_t left, uint8_t right)
{
	const uint8_t p[2] = { left, right };
	Write(EVT_RUMBLE, p, sizeof(p));
}

void ControlChannel::SendTriggers(uint8_t type_left, const uint8_t *left, uint8_t type_right, const uint8_t *right)
{
	uint8_t p[22];
	p[0] = type_left;
	p[1] = type_right;
	memcpy(p + 2, left, 10);
	memcpy(p + 12, right, 10);
	Write(EVT_TRIGGERS, p, sizeof(p));
}

void ControlChannel::SendBlocked(bool blocked)
{
	const uint8_t p[1] = { (uint8_t)(blocked ? 1 : 0) };
	Write(EVT_BLOCKED, p, sizeof(p));
}

void ControlChannel::SendPinRequest()
{
	Write(EVT_PIN, nullptr, 0);
}

#ifdef _WIN32

bool ControlChannel::Write(uint8_t type, const uint8_t *payload, size_t size)
{
	if(size > 255)
		return false;
	uint8_t buf[2 + 255];
	buf[0] = type;
	buf[1] = (uint8_t)size;
	if(size)
		memcpy(buf + 2, payload, size);

	QMutexLocker locker(&write_mutex);
	if(!pipe)
		return false;
	OVERLAPPED ov;
	memset(&ov, 0, sizeof(ov));
	ov.hEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
	if(!ov.hEvent)
		return false;
	bool ok = false;
	if(WriteFile((HANDLE)pipe, buf, (DWORD)(2 + size), nullptr, &ov) || GetLastError() == ERROR_IO_PENDING)
	{
		if(WaitForSingleObject(ov.hEvent, 200) == WAIT_OBJECT_0)
		{
			DWORD done = 0;
			ok = GetOverlappedResult((HANDLE)pipe, &ov, &done, FALSE) && done == (DWORD)(2 + size);
		}
		else
		{
			CancelIoEx((HANDLE)pipe, &ov);
			WaitForSingleObject(ov.hEvent, 1000);
		}
	}
	CloseHandle(ov.hEvent);
	return ok;
}

void ControlChannel::Run()
{
	const std::wstring path = L"\\\\.\\pipe\\" + name.toStdWString();
	while(!stop.load())
	{
		// SECURITY_SQOS_PRESENT | SECURITY_ANONYMOUS: whoever owns the pipe learns nothing about us
		HANDLE h = CreateFileW(path.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING,
				FILE_FLAG_OVERLAPPED | SECURITY_SQOS_PRESENT | SECURITY_ANONYMOUS, nullptr);
		if(h == INVALID_HANDLE_VALUE)
		{
			// the other program isn't there (yet, or any more): look again in a moment
			if(WaitForSingleObject((HANDLE)stop_event, 500) == WAIT_OBJECT_0)
				break;
			continue;
		}
		{
			QMutexLocker locker(&write_mutex);
			pipe = h;
		}
		const uint8_t version = PROTOCOL_VERSION;
		Write(EVT_HELLO, &version, 1);

		uint8_t buffer[1024];
		size_t have = 0;
		HANDLE read_event = CreateEventW(nullptr, TRUE, FALSE, nullptr);
		bool alive = read_event != nullptr;
		while(alive && !stop.load())
		{
			OVERLAPPED ov;
			memset(&ov, 0, sizeof(ov));
			ov.hEvent = read_event;
			ResetEvent(read_event);
			if(!ReadFile(h, buffer + have, (DWORD)(sizeof(buffer) - have), nullptr, &ov) && GetLastError() != ERROR_IO_PENDING)
				break;
			HANDLE waits[2] = { read_event, (HANDLE)stop_event };
			if(WaitForMultipleObjects(2, waits, FALSE, INFINITE) != WAIT_OBJECT_0)
			{
				CancelIoEx(h, &ov);
				WaitForSingleObject(read_event, 1000);
				break;
			}
			DWORD got = 0;
			if(!GetOverlappedResult(h, &ov, &got, FALSE) || got == 0)
				break;
			have += got;
			size_t at = 0;
			while(have - at >= 2)
			{
				const size_t n = buffer[at + 1];
				if(have - at - 2 < n)
					break;
				HandleMessage(buffer[at], buffer + at + 2, n);
				at += 2 + n;
			}
			memmove(buffer, buffer + at, have - at);
			have -= at;
		}
		if(read_event)
			CloseHandle(read_event);
		{
			QMutexLocker locker(&write_mutex);
			pipe = nullptr;
		}
		CloseHandle(h);
		// the other program is gone: nothing stays held
		attached.store(false);
		SetState(nullptr);
	}
}

#else // not Windows: the option is refused in Create(), so none of this is reached

bool ControlChannel::Write(uint8_t, const uint8_t *, size_t) { return false; }
void ControlChannel::Run() {}

#endif
