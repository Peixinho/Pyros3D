//============================================================================
// Name        : NetCrypto.cpp
// Author      : Duarte Peixinho
// Description : See NetCrypto.h.
//============================================================================

#include "NetCrypto.h"
#include <monocypher.h>
#include <cstring>
#include <random>

namespace p3d {

	namespace {
		void RandomBytes(uint8* out, const size_t n)
		{
			// std::random_device is the operating system's generator on
			// every platform this builds for (getentropy / /dev/urandom,
			// BCryptGenRandom) - not a seeded PRNG.
			std::random_device rd;
			for (size_t i = 0; i < n; i += 4)
			{
				const uint32 v = (uint32)rd();
				for (size_t k = 0; k < 4 && i + k < n; k++) out[i + k] = (uint8)(v >> (8 * k));
			}
		}

		void Nonce(uint8 nonce[24], const bool fromServer, const uint64 counter)
		{
			std::memset(nonce, 0, 24);
			nonce[0] = fromServer ? 1 : 0;	// the two directions never share a nonce
			for (int i = 0; i < 8; i++) nonce[1 + i] = (uint8)(counter >> (8 * i));
		}
	}

	NetKeyPair::NetKeyPair() { std::memset(secret, 0, 32); std::memset(pub, 0, 32); }

	NetKeyPair NetKeyPair::Generate()
	{
		NetKeyPair k;
		RandomBytes(k.secret, 32);
		crypto_x25519_public_key(k.pub, k.secret);
		return k;
	}

	bool NetKeyPair::Valid() const
	{
		uint8 any = 0;
		for (int i = 0; i < 32; i++) any |= secret[i];
		return any != 0;
	}

	bool NetHexToKey(const std::string &hex, uint8 out[32])
	{
		if (hex.size() != 64) return false;
		for (int i = 0; i < 32; i++)
		{
			int v = 0;
			for (int k = 0; k < 2; k++)
			{
				const char c = hex[i * 2 + k];
				int d;
				if (c >= '0' && c <= '9') d = c - '0';
				else if (c >= 'a' && c <= 'f') d = c - 'a' + 10;
				else if (c >= 'A' && c <= 'F') d = c - 'A' + 10;
				else return false;
				v = v * 16 + d;
			}
			out[i] = (uint8)v;
		}
		return true;
	}

	std::string NetKeyToHex(const uint8 key[32])
	{
		static const char* digits = "0123456789abcdef";
		std::string s;
		for (int i = 0; i < 32; i++) { s += digits[key[i] >> 4]; s += digits[key[i] & 15]; }
		return s;
	}

	bool NetKeyPair::FromSecretHex(const std::string &hex, NetKeyPair &out)
	{
		if (!NetHexToKey(hex, out.secret)) return false;
		crypto_x25519_public_key(out.pub, out.secret);
		return out.Valid();
	}

	std::string NetKeyPair::SecretHex() const { return NetKeyToHex(secret); }
	std::string NetKeyPair::PublicHex() const { return NetKeyToHex(pub); }

	void NetSharedSecret(const uint8 secret[32], const uint8 pub[32], uint8 out[32])
	{
		crypto_x25519(out, secret, pub);
	}

	void NetCipher::Establish(const uint8 ownSecret[32], const uint8 theirPublic[32], const uint8* staticShared,
		const uint8 clientPublic[32], const uint8 serverPublic[32], const bool server)
	{
		// The raw X25519 output is not a key: hash it, with the static
		// secret when there is one and both public keys, into one.
		uint8 shared[32];
		crypto_x25519(shared, ownSecret, theirPublic);
		crypto_blake2b_ctx ctx;
		crypto_blake2b_init(&ctx, 32);
		crypto_blake2b_update(&ctx, (const uint8_t*)"pyros3d-session-1", 17);
		crypto_blake2b_update(&ctx, shared, 32);
		if (staticShared) crypto_blake2b_update(&ctx, staticShared, 32);
		crypto_blake2b_update(&ctx, clientPublic, 32);
		crypto_blake2b_update(&ctx, serverPublic, 32);
		crypto_blake2b_final(&ctx, key);
		crypto_wipe(shared, 32);
		fromServer = server;
		sent = 0;
		highest = 0;
		window = 0;
		ready = true;
	}

	void NetCipher::Reset()
	{
		crypto_wipe(key, 32);
		ready = false;
		sent = highest = window = 0;
	}

	void NetCipher::Seal(const uchar* plain, const size_t size, std::vector<uchar> &out)
	{
		const uint64 counter = ++sent;	// from 1: 0 is "nothing opened yet"
		out.resize(Overhead + size);
		for (int i = 0; i < 8; i++) out[i] = (uchar)(counter >> (8 * i));
		uint8 nonce[24];
		Nonce(nonce, fromServer, counter);
		crypto_aead_lock(size ? &out[Overhead] : NULL, &out[8], key, nonce, &out[0], 8, plain, size);
	}

	bool NetCipher::Open(const uchar* data, const size_t size, std::vector<uchar> &out)
	{
		if (!ready || size < Overhead) return false;
		uint64 counter = 0;
		for (int i = 0; i < 8; i++) counter |= (uint64)data[i] << (8 * i);
		if (counter == 0) return false;
		// Seen already, or older than the window remembers.
		if (counter <= highest)
		{
			const uint64 age = highest - counter;
			if (age >= 64 || (window & ((uint64)1 << age))) return false;
		}
		std::vector<uchar> plain(size - Overhead);
		uint8 nonce[24];
		Nonce(nonce, !fromServer, counter);	// sealed by the other end
		if (crypto_aead_unlock(plain.empty() ? NULL : &plain[0], data + 8, key, nonce, data, 8, data + Overhead, size - Overhead) != 0)
			return false;
		// Only now - a forged counter must not move the window.
		if (counter > highest)
		{
			const uint64 shift = counter - highest;
			window = shift >= 64 ? 0 : (window << shift);
			window |= 1;
			highest = counter;
		}
		else window |= (uint64)1 << (highest - counter);
		out.swap(plain);
		return true;
	}

}
