// SPDX-License-Identifier: LicenseRef-AGPL-3.0-only-OpenSSL
//
// Not part of upstream chiaki-ng. Added in the chiaki-ng-control-pipe fork on 2026-11-24.
//
// An optional local control channel. When the program is started with
// `--control-pipe <name>`, it connects to the named pipe \\.\pipe\<name>, which another
// program on the same PC has created, and takes controller state from it. That state is
// combined with the keyboard and with any real controller, exactly like one more controller.
// Without the option nothing here runs. The message format is described in docs/control-pipe.md.

#ifndef CHIAKI_CONTROLCHANNEL_H
#define CHIAKI_CONTROLCHANNEL_H

#include <chiaki/controller.h>

#include <QObject>
#include <QMutex>
#include <QString>

#include <atomic>
#include <cstdint>
#include <thread>

class ControlChannel : public QObject
{
	Q_OBJECT

	public:
		static const uint8_t PROTOCOL_VERSION = 1;

		// The one channel of this process, or nullptr when --control-pipe was not given.
		static ControlChannel *Instance();
		// Called once from main(). Returns false (and creates nothing) for a name that isn't
		// 1 to 128 characters of A-Z a-z 0-9 _ - or on a system without named pipes.
		static bool Create(const QString &pipe_name);

		~ControlChannel();

		// The state the other program holds right now; idle while nothing is connected.
		ChiakiControllerState State();
		bool Attached() const { return attached.load(); }

		// Reports to the other program. Safe from any thread; dropped while nothing is connected.
		void SendStream(bool on, uint8_t reason, const QString &reason_str);
		void SendRumble(uint8_t left, uint8_t right);
		void SendTriggers(uint8_t type_left, const uint8_t *left, uint8_t type_right, const uint8_t *right);
		void SendBlocked(bool blocked);
		void SendPinRequest();

	signals:
		// The state changed (or the other program left). Emitted from the channel's own thread:
		// connect with Qt::QueuedConnection.
		void StateChanged();

	private:
		explicit ControlChannel(const QString &pipe_name);
		void Run();
		void SetState(const ChiakiControllerState *new_state);
		void HandleMessage(uint8_t type, const uint8_t *payload, size_t size);
		bool Write(uint8_t type, const uint8_t *payload, size_t size);

		QString name;
		QMutex state_mutex;
		ChiakiControllerState state;
		std::atomic<bool> attached{false};
		std::atomic<bool> stop{false};
		std::thread thread;

		QMutex write_mutex;
		void *pipe = nullptr;        // HANDLE; guarded by write_mutex when written to or replaced
		void *stop_event = nullptr;  // HANDLE; set to end the thread
};

#endif // CHIAKI_CONTROLCHANNEL_H
