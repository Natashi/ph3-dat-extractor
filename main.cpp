#include "Archive.h"

int main(int argc, char* argv[]) {
    if (argc != 3) {
        printf("Usage: [Mode/Option] [Archive]\n");
        printf("    Available modes:\n");
        printf("        x   Extract archive\n");
        printf("        l   List archived files\n");
		printf("    Available options:\n");
		printf("        d   Dump archive header\n");
        return -1;
    }

    const char* option = argv[1];
    const char* pathArchive = argv[2];

	bool bExtract = strstr(option, "x") != nullptr;
	bool bList = strstr(option, "l") != nullptr;
	bool bOptDumpHeader = strstr(option, "d") != nullptr;

	if (bExtract && bList) {
		printf("Modes x and l cannot be used together\n");
		return -1;
	}
	else if (!bExtract && !bList) {
		printf("Mode x or l must be specified\n");
		return -1;
	}

    Archive archive;
    try {
        archive.Load(pathArchive, bOptDumpHeader);

		if (bList) {
			const Header& header = archive.GetHeader();
			printf("%d entries\n", header.countEntry);

			for (size_t iEntry = 0; iEntry < header.countEntry; ++iEntry) {
				const Entry& entry = archive.GetEntry(iEntry);

				std::wstring pathOutDir = archive.GetProperDirectoryName(&entry);
				std::wstring pathOutName = archive.GetProperFileName(&entry);
				std::wstring fullPath = pathOutDir + pathOutName;

				wprintf(L"%3d: %s\n", iEntry, fullPath.c_str());
			}
		}
        else if (bExtract) {
			const Header& header = archive.GetHeader();
            printf("%d entries\n", header.countEntry);

            for (size_t iEntry = 0; iEntry < header.countEntry; ++iEntry) {
				Entry& entry = archive.GetEntry(iEntry);

                bool bSuccess = archive.Extract(&entry);
                {
					std::wstring pathOutDir = archive.GetProperDirectoryName(&entry);
					std::wstring pathOutName = archive.GetProperFileName(&entry);
					std::wstring fullPath = pathOutDir + pathOutName;

                    wprintf(L"%3d-> %s: %s", iEntry, fullPath.c_str(), bSuccess ? L"" : L"Failed");
                    if (bSuccess) {
                        wprintf(L"[compress=%s, size=%d, stored=%d, offset=%08x]", 
							entry.compress == 1 ? L"zlib" : L"none",
                            entry.sizeFull, entry.sizeStored, entry.offset);
                    }
                    wprintf(L"\n");
                }
            }
        }
    }
    catch (std::exception& e) {
        printf("Error-> %s", e.what());
		return -2;
    }

    return 0;
}
