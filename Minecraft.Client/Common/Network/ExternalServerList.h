#pragma once

#include <cerrno>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
#include <windows.h>

class ExternalServerList {
public:
	typedef struct _Entry {
		std::string address;
		uint16_t port;
		std::string name;
	} Entry;

	static const uint32_t MaxEntries = 4096;

	static bool Read(std::vector<Entry>& entries) {
		entries.clear();
		FILE* file = nullptr;
		if (fopen_s(&file, "servers.db", "rb") != 0) return errno == ENOENT;

		char magic[4] = {};
		uint32_t version = 0;
		uint32_t count = 0;
		bool success = (fread(magic, 1, sizeof(magic), file) == sizeof(magic) && memcmp(magic, "MCSV", sizeof(magic)) == 0) && (fread(&version, sizeof(version), 1, file) == 1 && fread(&count, sizeof(count), 1, file) == 1) && version == 1 && count <= MaxEntries;

		std::vector<Entry> loadedEntries;
		if (success) {
			loadedEntries.reserve(count);
			for (uint32_t index = 0; index < count; ++index) {
				uint16_t addressLength = 0;
				uint16_t port = 0;
				uint16_t nameLength = 0;
				if (fread(&addressLength, sizeof(addressLength), 1, file) != 1 || addressLength == 0 || addressLength > 256) {
					success = false;
					break;
				}

				char address[257] = {};
				if (fread(address, 1, addressLength, file) != addressLength || fread(&port, sizeof(port), 1, file) != 1 || fread(&nameLength, sizeof(nameLength), 1, file) != 1 || nameLength > 256) {
					success = false;
					break;
				}

				char name[257] = {};
				if ((nameLength != 0 && fread(name, 1, nameLength, file) != nameLength) || port == 0) {
					success = false;
					break;
				}

				loadedEntries.push_back({ std::string(address, addressLength), port, std::string(name, nameLength) });
			}
		}

		if (fclose(file) != 0) success = false;
		if (success) entries.swap(loadedEntries);

		return success;
	}

	static bool Write(const std::vector<Entry>& entries) {
		if (entries.size() > MaxEntries) return false;

		FILE* file = nullptr;
		if (fopen_s(&file, "servers.db.tmp", "wb") != 0) return false;

		bool success = fwrite("MCSV", 1, 4, file) == 4;
		const uint32_t version = 1;
		const uint32_t count = static_cast<uint32_t>(entries.size());
		success = success && fwrite(&version, sizeof(version), 1, file) == 1 && fwrite(&count, sizeof(count), 1, file) == 1;

		for (size_t index = 0; success && index < entries.size(); ++index) {
			const Entry& entry = entries[index];
			if (entry.address.empty() || entry.address.size() > 256 || entry.name.size() > 256 || entry.port == 0) {
				success = false;
				break;
			}

			const uint16_t addressLength = static_cast<uint16_t>(entry.address.size());
			const uint16_t nameLength = static_cast<uint16_t>(entry.name.size());
			success = fwrite(&addressLength, sizeof(addressLength), 1, file) == 1 && fwrite(entry.address.data(), 1, addressLength, file) == addressLength && fwrite(&entry.port, sizeof(entry.port), 1, file) == 1 && fwrite(&nameLength, sizeof(nameLength), 1, file) == 1 && (nameLength == 0 || fwrite(entry.name.data(), 1, nameLength, file) == nameLength);
		}

		if (fclose(file) != 0) success = false;
		if (!success || !MoveFileExA("servers.db.tmp", "servers.db", MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
			DeleteFileA("servers.db.tmp");
			return false;
		}
		return true;
	}

	static bool Append(const Entry& entry) {
		std::vector<Entry> entries;
		if (!Read(entries)) return false;

		entries.push_back(entry);
		return Write(entries);
	}

	static bool MakeSessionId(const std::string& address, uint16_t port, uint64_t& sessionId) {
		if (address.empty() || address.size() > 15 || port == 0) return false;

		uint32_t ipAddress = 0;
		size_t position = 0;
		for (int octetIndex = 0; octetIndex < 4; ++octetIndex) {
			if (position >= address.size()) return false;

			uint32_t octet = 0;
			size_t digits = 0;
			while (position < address.size() && address[position] >= '0' && address[position] <= '9') {
				octet = octet * 10 + static_cast<uint32_t>(address[position] - '0');
				if (octet > 255) return false;

				++position;
				++digits;
			}
			if (digits == 0) return false;

			ipAddress = (ipAddress << 8) | octet;
			if (octetIndex < 3) {
				if (position >= address.size() || address[position] != '.') return false;

				++position;
			}
		}
		if (position != address.size()) return false;

		sessionId = 0x4C41000000000000ULL | (static_cast<uint64_t>(ipAddress) << 16) | static_cast<uint64_t>(port);
		return true;
	}
};
