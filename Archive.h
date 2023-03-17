#pragma once
#include "pch.h"

#pragma pack(push, 1)
struct Header {
	char magic[0xB];	//"ArchiveFile"
	uint32_t countEntry;
	uint8_t headerCompressed;
	uint32_t sizeEntryInfo;
};
#pragma pack(pop)

struct Entry {
	size_t directorySize;
	std::vector<byte> directory;
	size_t nameSize;
	std::vector<byte> name;

	uint32_t compress;
	uint32_t sizeFull;
	uint32_t sizeStored;
	uint32_t offset;
};

class Archive {
public:
	Archive();
	~Archive();

	void Load(const std::string& path, bool bDumpHeader);
	bool Extract(Entry* entry);

	size_t GetEntryCount() const { return entries.size(); }
	size_t GetCharByteSize() const { return nameByteSize; }

	const Header& GetHeader() const { return header; }
	const Entry& GetEntry(size_t index) const { return entries[index]; }
	Entry& GetEntry(size_t index) { return entries[index]; }

	const std::wstring GetProperDirectoryName(const Entry* entry) const;
	const std::wstring GetProperFileName(const Entry* entry) const;
private:
	std::fstream archiveFile;
	std::streampos fileSize;
	size_t nameByteSize;

	Header header;
	std::vector<Entry> entries;
};