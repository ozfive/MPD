/*
 * Copyright 2003-2021 The Music Player Daemon Project
 * http://www.musicpd.org
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef MPD_OUTPUT_SNAPCAST_INTERNAL_HXX
#define MPD_OUTPUT_SNAPCAST_INTERNAL_HXX

#include "output/Interface.hxx"
#include "output/Timer.hxx"
#include "thread/Mutex.hxx"
#include "event/ServerSocket.hxx"
#include "util/AllocatedArray.hxx"
#include "util/IntrusiveList.hxx"

#include <memory>

struct ConfigBlock;
class SnapcastClient;
class PreparedEncoder;
class Encoder;

class SnapcastOutput final : AudioOutput, ServerSocket {
	/**
	 * True if the audio output is open and accepts client
	 * connections.
	 */
	bool open;

	/**
	 * The configured encoder plugin.
	 */
	std::unique_ptr<PreparedEncoder> prepared_encoder;
	Encoder *encoder = nullptr;

	AllocatedArray<std::byte> codec_header;

	/**
	 * Number of bytes which were fed into the encoder, without
	 * ever receiving new output.  This is used to estimate
	 * whether MPD should manually flush the encoder, to avoid
	 * buffer underruns in the client.
	 */
	size_t unflushed_input = 0;

	/**
	 * A #Timer object to synchronize this output with the
	 * wallclock.
	 */
	Timer *timer;

	/**
	 * A linked list containing all clients which are currently
	 * connected.
	 */
	IntrusiveList<SnapcastClient> clients;

public:
	/**
	 * This mutex protects the listener socket and the client
	 * list.
	 */
	mutable Mutex mutex;

	SnapcastOutput(EventLoop &_loop, const ConfigBlock &block);
	~SnapcastOutput() noexcept override;

	static AudioOutput *Create(EventLoop &event_loop,
				   const ConfigBlock &block) {
		return new SnapcastOutput(event_loop, block);
	}

	using ServerSocket::GetEventLoop;

	void Bind();
	void Unbind() noexcept;

	/**
	 * Check whether there is at least one client.
	 *
	 * Caller must lock the mutex.
	 */
	[[gnu::pure]]
	bool HasClients() const noexcept {
		return !clients.empty();
	}

	/**
	 * Check whether there is at least one client.
	 */
	[[gnu::pure]]
	bool LockHasClients() const noexcept {
		const std::lock_guard<Mutex> protect(mutex);
		return HasClients();
	}

	/**
	 * Caller must lock the mutex.
	 */
	void AddClient(UniqueSocketDescriptor fd) noexcept;

	/**
	 * Removes a client from the snapcast_output.clients linked list.
	 *
	 * Caller must lock the mutex.
	 */
	void RemoveClient(SnapcastClient &client) noexcept;

	/**
	 * Caller must lock the mutex.
	 *
	 * Throws on error.
	 */
	void OpenEncoder(AudioFormat &audio_format);

	const char *GetCodecName() const noexcept {
		return "pcm";
	}

	ConstBuffer<void> GetCodecHeader() const noexcept {
		ConstBuffer<std::byte> result(codec_header);
		return result.ToVoid();
	}

	/* virtual methods from class AudioOutput */
	void Enable() override {
		Bind();
	}

	void Disable() noexcept override {
		Unbind();
	}

	void Open(AudioFormat &audio_format) override;
	void Close() noexcept override;

	// TODO: void Interrupt() noexcept override;

	std::chrono::steady_clock::duration Delay() const noexcept override;

	// TODO: void SendTag(const Tag &tag) override;

	size_t Play(const void *chunk, size_t size) override;

	// TODO: void Drain() override;
	void Cancel() noexcept override;
	bool Pause() override;

private:
	void BroadcastWireChunk(ConstBuffer<void> payload,
				std::chrono::steady_clock::time_point t) noexcept;

	/* virtual methods from class ServerSocket */
	void OnAccept(UniqueSocketDescriptor fd,
		      SocketAddress address, int uid) noexcept override;
};

#endif
