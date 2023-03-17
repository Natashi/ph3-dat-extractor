#include "Archive.h"

#include "zlib.h"

using path_t = stdfs::path;

Archive::Archive() {
	nameByteSize = 1U;
}
Archive::~Archive() {
	if (archiveFile.is_open())
		archiveFile.close();
}

void Archive::Load(const std::string& path, bool bDumpHeader) {
	archiveFile.open(path, std::ios::binary | std::ios::in);
	if (!archiveFile.is_open())
		throw std::exception("Load: Cannot open file for reading");

	archiveFile.seekg(0, std::ios::end);
	fileSize = archiveFile.tellg();
	archiveFile.seekg(0, std::ios::beg);

	if (fileSize < sizeof(Header))
		throw std::exception("Load: Not a ph3 .DAT archive");

	archiveFile.read((char*)&header, sizeof(Header));

	if (memcmp(header.magic, "ArchiveFile", 0xB) != 0)
		throw std::exception("Load: Not a ph3 .DAT archive");

	size_t sizeInfoRead = 0;
	{
		std::stringstream infoField;
		archiveFile.seekg(sizeof(Header), std::ios::beg);
		if (header.headerCompressed) {
			constexpr const size_t CHUNK = 2048U;
			char in[CHUNK];
			char out[CHUNK];

			int returnState = 0;

			z_stream stream;
			stream.zalloc = Z_NULL;
			stream.zfree = Z_NULL;
			stream.opaque = Z_NULL;
			stream.avail_in = 0;
			stream.next_in = Z_NULL;
			returnState = inflateInit(&stream);
			if (returnState != Z_OK)
				throw std::exception("Load::Inflate: Error initializing zlib");

			size_t remain = header.sizeEntryInfo;
			try {
				size_t read = 0;
				do {
					archiveFile.read(in, CHUNK);
					read = archiveFile.gcount();
					if (remain < read) read = remain;

					if (read > 0) {
						stream.avail_in = read;
						stream.next_in = (Bytef*)in;

						do {
							stream.next_out = (Bytef*)out;
							stream.avail_out = CHUNK;

							returnState = inflate(&stream, Z_NO_FLUSH);
							switch (returnState) {
							case Z_NEED_DICT:
							case Z_DATA_ERROR:
							case Z_MEM_ERROR:
							case Z_STREAM_ERROR:
								throw returnState;
							}

							size_t availWrite = CHUNK - stream.avail_out;
							sizeInfoRead += availWrite;
							infoField.write(out, availWrite);
						} while (stream.avail_out == 0);
					}

					remain -= read;
				} while (read > 0 && remain > 0);
			}
			catch (int e) {
				const char* code;
				switch (returnState) {
				case Z_NEED_DICT:
					code = "Z_NEED_DICT";
					break;
				case Z_DATA_ERROR:
					code = "Z_DATA_ERROR";
					break;
				case Z_MEM_ERROR:
					code = "Z_MEM_ERROR";
					break;
				case Z_STREAM_ERROR:
					code = "Z_STREAM_ERROR";
					break;
				default:
					code = "Unknown";
					break;
				}

				inflateEnd(&stream);

				char error[512];
				sprintf_s(error, 512, "Load::Inflate: Error (%s)", code);

				throw std::exception(error);
			}

			inflateEnd(&stream);
			if (remain > 0)
				throw std::exception("Load: Archive possibly corrupt");
		}
		else {
			sizeInfoRead = header.sizeEntryInfo;
			size_t remain = sizeInfoRead;

			size_t read = 0;

			constexpr const size_t CHUNK = 2048U;
			char in[CHUNK];
			do {
				archiveFile.read(in, CHUNK);
				read = archiveFile.gcount();
				if (remain < read) read = remain;

				if (read > 0) {
					infoField.write(in, read);
				}

				remain -= read;
			} while (remain > 0);

			if (remain > 0)
				throw std::exception("Load: Archive possibly corrupt");
		}

		if (bDumpHeader) {
			std::ofstream out;
			out.open(".header", std::ios::binary);
			out << infoField.rdbuf();
			out.close();
		}

		{	
			// Earlier versions of Danmakufu used char* instead of wchar_t* to store names, try to guess

			size_t size = 0;
			infoField.seekg(4, std::ios::beg);
			infoField.read((char*)&size, 4);
			if (size == 0) {
				infoField.seekg(8, std::ios::beg);
				infoField.read((char*)&size, 4);
			}

			size_t readSize = size / 2;
			std::vector<char> tmp;
			tmp.resize(size);
			infoField.read((char*)&tmp[0], size);
			for (size_t i = 0U; i < readSize; ++i) {
				wchar_t ch = *((wchar_t*)tmp.data() + i);
				if ((ch & 0xff00) == 0) {
					nameByteSize = 2;
					break;
				}
			}
		}
		while (true) {
			infoField.seekg(0, std::ios::beg);

			size_t totalEntryBytes = 0;

			entries.resize(header.countEntry);
			for (size_t iEntry = 0; iEntry < header.countEntry; ++iEntry) {
				Entry entry;

				size_t entrySize = 0;
				infoField.read((char*)&entrySize, 4);
				totalEntryBytes += entrySize + 4;

				infoField.read((char*)&entry.directorySize, 4);
				if (entry.directorySize > 0) {
					entry.directory.resize(entry.directorySize * nameByteSize);
					infoField.read((char*)(entry.directory.data()), entry.directorySize * nameByteSize);
				}

				infoField.read((char*)&entry.nameSize, 4);
				entry.name.resize(entry.nameSize * nameByteSize);
				infoField.read((char*)(entry.name.data()), entry.nameSize * nameByteSize);

				infoField.read((char*)&entry.compress, 4 * 4);

				entries[iEntry] = entry;
			}

			// Check if the guess correct, retry with different char size if not
			{
				size_t pos = infoField.tellg();
				if (pos < totalEntryBytes) {
					if (nameByteSize == 1) nameByteSize = 2;
					else throw std::exception("Load: What the fuck");
				}
				else break;
			}
		}
	}
}

bool Archive::Extract(Entry* entry) {
	if (!archiveFile.is_open())
		throw std::exception("Extract: Archive not loaded");

	if (entry == nullptr) return false;
	else {
		if (entry->nameSize == 0 || entry->offset > fileSize || entry->compress > 1)
			return false;

		if (entry->compress == 0 && (entry->sizeFull == 0 || ((entry->offset + entry->sizeFull) > fileSize)))
			return false;
		else if (entry->compress == 1 && (entry->sizeStored == 0 || ((entry->offset + entry->sizeStored) > fileSize)))
			return false;
	}

	std::wstring pathOutDir = GetProperDirectoryName(entry);
	std::wstring pathOutName = GetProperFileName(entry);
	{
		if (pathOutDir.size() > 0 && !stdfs::exists(pathOutDir))
			stdfs::create_directories(pathOutDir);
	}

	std::ofstream outFile;
	outFile.open(pathOutDir + pathOutName, std::ios::binary);
	if (!outFile.is_open()) return false;

	bool result = true;

	archiveFile.clear();
	archiveFile.seekg(entry->offset, std::ios::beg);
	if (entry->compress == 0) {
		entry->sizeStored = entry->sizeFull;

		uint32_t remain = entry->sizeFull;

		constexpr const size_t CHUNK = 2048U;
		char in[CHUNK];

		do {
			archiveFile.read(in, CHUNK);
			size_t read = archiveFile.gcount();
			if (remain < read) read = remain;

			if (read > 0)
				outFile.write(in, read);

			remain -= read;
		} while (remain > 0);
	}
	else if (entry->compress == 1) {
		size_t sizeData = 0U;

		constexpr const size_t CHUNK = 2048U;
		char in[CHUNK];
		char out[CHUNK];

		int zReturn = 0;

		z_stream stream;
		stream.zalloc = Z_NULL;
		stream.zfree = Z_NULL;
		stream.opaque = Z_NULL;
		stream.avail_in = 0;
		stream.next_in = Z_NULL;
		zReturn = inflateInit(&stream);
		if (zReturn != Z_OK) {
			result = false;
		}
		else {
			size_t remain = entry->sizeStored;
			try {
				size_t read = 0;
				do {
					archiveFile.read(in, CHUNK);
					read = archiveFile.gcount();
					if (remain < read) read = remain;

					if (read > 0) {
						stream.avail_in = read;
						stream.next_in = (Bytef*)in;

						do {
							stream.next_out = (Bytef*)out;
							stream.avail_out = CHUNK;

							zReturn = inflate(&stream, Z_NO_FLUSH);
							switch (zReturn) {
							case Z_NEED_DICT:
							case Z_DATA_ERROR:
							case Z_MEM_ERROR:
							case Z_STREAM_ERROR:
								throw zReturn;
							}

							size_t availWrite = CHUNK - stream.avail_out;
							sizeData += availWrite;
							outFile.write(out, availWrite);
						} while (stream.avail_out == 0);
					}

					remain -= read;
				} while (read > 0 && remain > 0);
			}
			catch (int& e) {
				result = false;
			}
		}

		inflateEnd(&stream);

		entry->sizeFull = sizeData;
	}

	outFile.close();
	return result;
}

const std::wstring Archive::GetProperDirectoryName(const Entry* entry) const {
	std::wstring res;
	switch (GetCharByteSize()) {
	case 2:
	{
		if (entry->directorySize > 0)
			res = std::wstring((wchar_t*)&entry->directory[0], entry->directorySize);
		break;
	}
	case 1:
	default:
	{
		std::string tmp;
		if (entry->directorySize > 0)
			tmp = std::string((char*)&entry->directory[0], entry->directorySize);
		res = std::wstring(tmp.begin(), tmp.end());
		break;
	}
	}
	if (!res.empty() && res.back() != L'/')
		res += L'/';
	return res;
}
const std::wstring Archive::GetProperFileName(const Entry* entry) const {
	switch (GetCharByteSize()) {
	case 2:
		return entry->nameSize > 0 ? std::wstring((wchar_t*)&entry->name[0], entry->nameSize) : L"";
	case 1:
	default:
	{
		std::string tmp;
		if (entry->nameSize > 0)
			tmp = std::string((char*)&entry->name[0], entry->nameSize);
		return std::wstring(tmp.begin(), tmp.end());
	}
	}
}