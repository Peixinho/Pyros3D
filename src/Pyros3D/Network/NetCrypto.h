//============================================================================
// Name        : NetCrypto.h
// Author      : Duarte Peixinho
// Description : What the network session encrypts with. Private to the
//               engine (it names Monocypher); the session is its only user.
//
//               Key agreement: both ends make a fresh X25519 key pair for
//               each connection and derive the same 32-byte key from the
//               shared secret - so recording the traffic and learning a
//               password later decrypts nothing. A server may also hold a
//               long-term key; mixed in, it makes the key something only
//               the holder of that secret can compute, so a client that
//               pins the server's public key cannot be talked to by anyone
//               sitting in between.
//
//               Messages: XChaCha20-Poly1305. Each direction counts its
//               messages; the counter travels with the message (unreliable
//               ones arrive out of order, or not at all) and is the nonce,
//               so none repeats, and a window of the last 64 refuses a
//               message played back. On the wire:
//                   counter (8) | tag (16) | ciphertext
//============================================================================

#ifndef NETCRYPTO_H
#define NETCRYPTO_H

#include <Pyros3D/Other/Export.h>
#include <Pyros3D/Other/Global.h>
#include <Pyros3D/Core/Math/Math.h>
#include <string>
#include <vector>

namespace p3d {

	// Exported only so tools/tests can reach them; nothing public names them.
	struct PYROS3D_API NetKeyPair
	{
		uint8 secret[32];
		uint8 pub[32];
		NetKeyPair();						// zeroed: not a key
		static NetKeyPair Generate();		// from the system's entropy
		// From / to 64 hex characters of the secret (what a server keeps in
		// a file); the public half follows from it.
		static bool FromSecretHex(const std::string &hex, NetKeyPair &out);
		std::string SecretHex() const;
		std::string PublicHex() const;
		bool Valid() const;
	};

	// 64 hex characters -> 32 bytes. False on anything else.
	PYROS3D_API bool NetHexToKey(const std::string &hex, uint8 out[32]);
	PYROS3D_API std::string NetKeyToHex(const uint8 key[32]);

	class PYROS3D_API NetCipher
	{
	public:
		NetCipher() : ready(false), fromServer(false), sent(0), highest(0), window(0) { for (int i = 0; i < 32; i++) key[i] = 0; }
		~NetCipher() { Reset(); }

		// The session key both ends arrive at. ownSecret/theirPublic are
		// this connection's fresh keys; staticShared is the X25519 of the
		// server's long-term key with the client's fresh one (NULL when the
		// server has none). clientPublic/serverPublic bind the key to this
		// exchange. `server` says which direction this end sends in.
		void Establish(const uint8 ownSecret[32], const uint8 theirPublic[32], const uint8* staticShared,
			const uint8 clientPublic[32], const uint8 serverPublic[32], const bool server);
		void Reset();
		bool Ready() const { return ready; }

		// counter | tag | ciphertext
		void Seal(const uchar* plain, const size_t size, std::vector<uchar> &out);
		// False - and out untouched - when it was not sealed with this
		// key, was tampered with, or has been seen before.
		bool Open(const uchar* data, const size_t size, std::vector<uchar> &out);

		static const size_t Overhead = 24;

	private:
		uint8 key[32];
		bool ready, fromServer;
		uint64 sent;				// messages sealed
		uint64 highest, window;		// replay: the newest counter opened, and the 64 before it
	};

	// X25519(secret, public) - the static half of the agreement.
	PYROS3D_API void NetSharedSecret(const uint8 secret[32], const uint8 pub[32], uint8 out[32]);

}

#endif /* NETCRYPTO_H */
