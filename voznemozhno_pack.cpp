#include <stdint.h>
#include <cstdlib>
#include <cstdio>
#include <string.h>
#include "voznemozhno_pack.h"

int GetFileSize(const char *szFilename, size_t *pStFileSize)
{
	FILE* pFile = fopen(szFilename, "rb");
	if (!pFile) {
		printf("Failed to open buffer file for reading\r\n");
		return -1;
	}

	if (fseek(pFile, 0, SEEK_END)) {
		printf("Failed to seek buffer file\r\n");
		fclose(pFile);
		return -2;
	}

	long lSize = ftell(pFile);
	if (lSize == -1) {
		printf("Failed to get buffer file size\r\n");
		fclose(pFile);
		return -2;
	}

	fclose(pFile);

	*pStFileSize = (size_t)lSize;

	return 0;
}

int ReadFramebuffer(const char *szFilename, uint8_t *pBuffer, size_t stBufferSize)
{
	FILE* pFile = fopen(szFilename, "rb");
	if (!pFile) {
		printf("Failed to open buffer file for reading\r\n");
		return -1;
	}

	size_t stRead = fread(pBuffer, sizeof(uint8_t), stBufferSize, pFile);
	if (stRead != stBufferSize) {
		printf("Failed to read file. %zu != %zu\r\n", stRead, stBufferSize);
		fclose(pFile);
		return -2;
	}

	fclose(pFile);

	return 0;
}

int SaveFramebuffer(const char *szFilename, uint8_t *pBuffer, size_t stSize)
{
	FILE* pFile = fopen(szFilename, "wb");
	if (!pFile) {
		printf("Failed to open buffer file for writing\r\n");
		return -1;
	}

	size_t stWritten = fwrite(pBuffer, sizeof(uint8_t), stSize, pFile);
	if (stWritten != stSize) {
		printf("Failed to save file. %zu != %zu\r\n", stWritten, stSize);
		fclose(pFile);
		return -2;
	}

	fclose(pFile);

	return 0;
}

size_t FindMaxPos(uint16_t *pBuffer, size_t stBufferSize)
{
	size_t stFoundPos = 0;

	for (size_t stI = 1; stI < stBufferSize; stI++) {
		if (pBuffer[stI] > pBuffer[stFoundPos])
			stFoundPos = stI;
	}

	return stFoundPos;
}

// FIXME: no proper buffer size checks
int VoznemozhnoPackFull(uint8_t *pBuffer, size_t stBufferSize, uint8_t *pResult, size_t stResultBufferSize, size_t *pStResultLength)
{
	uint16_t bufferSkipStats[256] = {0};

	size_t stBuffPtr = 0;
	uint8_t btSkipSize = 0;
	size_t stResultPtr = 0;
	bool bSkipWritten = false;

	while (stBuffPtr < stBufferSize) {
		if (pBuffer[stBuffPtr] == VPF_EMPTY) {
			if (btSkipSize > 1)
				bSkipWritten = false;
			// FIXME: we may not record all gaps if they are followed by the end of the buffer
			if (btSkipSize == 255) {
				btSkipSize = 0;
			}
			btSkipSize++;
			stBuffPtr++;
			continue;
		}
		if (!bSkipWritten) {
			bufferSkipStats[btSkipSize]++;
			bSkipWritten = true;
		}
		btSkipSize = 0;
		stBuffPtr++;
	}

	bool skipSizeSent[6] = {false};
	uint8_t skipSizes[6] = {0};

	for (size_t stI = 0; stI < sizeof(skipSizes); stI++) {
		size_t stSkip = FindMaxPos(bufferSkipStats, sizeof(bufferSkipStats) / sizeof(bufferSkipStats[0]));
		if (!bufferSkipStats[stSkip])
			break;
		skipSizes[stI] = stSkip;
		bufferSkipStats[stSkip] = 0;
	}

	stBuffPtr = 0;
	btSkipSize = 0;
	stResultPtr = 0;
	bSkipWritten = false;

	while (stBuffPtr < stBufferSize) {
		if (pBuffer[stBuffPtr] == VPF_EMPTY) {
			if (btSkipSize > 1)
				bSkipWritten = false;
			// FIXME: we may not record all gaps if they are followed by the end of the buffer
			if (btSkipSize == 255) {
				pResult[stResultPtr++] = VPF_SKIP1;
				pResult[stResultPtr++] = btSkipSize;
				btSkipSize = 0;
			}
			btSkipSize++;
			stBuffPtr++;
			continue;
		}
		if (!bSkipWritten) {
			for (size_t stI = 0; stI < sizeof(skipSizes); stI++) {
				if (!skipSizes[stI])
					break;
				if (btSkipSize == skipSizes[stI]) {
					switch (stI) {
						case 0: pResult[stResultPtr++] = VPF_SKIP_S0; break;
						case 1: pResult[stResultPtr++] = VPF_SKIP_S1; break;
						case 2: pResult[stResultPtr++] = VPF_SKIP_S2; break;
						case 3: pResult[stResultPtr++] = VPF_SKIP_S3; break;
						case 4: pResult[stResultPtr++] = VPF_SKIP_S4; break;
						case 5: pResult[stResultPtr++] = VPF_SKIP_S5; break;
					}
					if (!skipSizeSent[stI]) {
						pResult[stResultPtr++] = btSkipSize;
						skipSizeSent[stI] = true;
					}
					bSkipWritten = true;
					btSkipSize = 0;
				}
			}
			if (!bSkipWritten) {
				pResult[stResultPtr++] = VPF_SKIP1;
				pResult[stResultPtr++] = btSkipSize;
				bSkipWritten = true;
				btSkipSize = 0;
			}
		} else {
			while (btSkipSize) {
				pResult[stResultPtr++] = VPF_EMPTY;
				btSkipSize--;
			}
		}
		switch (pBuffer[stBuffPtr]) {
			case VPF_SKIP1:
			case VPF_SKIP_S0:
			case VPF_SKIP_S1:
			case VPF_SKIP_S2:
			case VPF_SKIP_S3:
			case VPF_SKIP_S4:
			case VPF_SKIP_S5:
			case VPF_ESCAPE:
			case VPF_END:
				pResult[stResultPtr++] = VPF_ESCAPE;
			default:
				pResult[stResultPtr++] = pBuffer[stBuffPtr];
		}
		stBuffPtr++;
	}

	pResult[stResultPtr++] = VPF_END;

	*pStResultLength = stResultPtr;

	return 0;
}

// FIXME: no proper buffer size checks
int VoznemozhnoUnpackFull(uint8_t *pPackedBuffer, size_t stPackedBufferSize, uint8_t *pFramebuffer, size_t stFramebufferSize, size_t *pStResultLength)
{
	if (pFramebuffer)
		memset(pFramebuffer, VPF_EMPTY, stFramebufferSize);

	uint8_t skipSizes[6] = {0};

	size_t stPackedPtr = 0;
	size_t stUnpackedPtr = 0;
	while (stPackedPtr < stPackedBufferSize) {
		switch (pPackedBuffer[stPackedPtr]) {
			case VPF_SKIP1: {
				stUnpackedPtr += pPackedBuffer[stPackedPtr + 1];
				stPackedPtr += 2;
				break;
			}
			case VPF_SKIP_S0:
				if (!skipSizes[0])
					skipSizes[0] = pPackedBuffer[++stPackedPtr];
				stUnpackedPtr += skipSizes[0];
				stPackedPtr++;
				break;
			case VPF_SKIP_S1:
				if (!skipSizes[1])
					skipSizes[1] = pPackedBuffer[++stPackedPtr];
				stUnpackedPtr += skipSizes[1];
				stPackedPtr++;
				break;
			case VPF_SKIP_S2:
				if (!skipSizes[2])
					skipSizes[2] = pPackedBuffer[++stPackedPtr];
				stUnpackedPtr += skipSizes[2];
				stPackedPtr++;
				break;
			case VPF_SKIP_S3:
				if (!skipSizes[3])
					skipSizes[3] = pPackedBuffer[++stPackedPtr];
				stUnpackedPtr += skipSizes[3];
				stPackedPtr++;
				break;
			case VPF_SKIP_S4:
				if (!skipSizes[4])
					skipSizes[4] = pPackedBuffer[++stPackedPtr];
				stUnpackedPtr += skipSizes[4];
				stPackedPtr++;
				break;
			case VPF_SKIP_S5:
				if (!skipSizes[5])
					skipSizes[5] = pPackedBuffer[++stPackedPtr];
				stUnpackedPtr += skipSizes[5];
				stPackedPtr++;
				break;
			case VPF_ESCAPE: {
				if (pFramebuffer)
					pFramebuffer[stUnpackedPtr++] = pPackedBuffer[++stPackedPtr];
				else
					stUnpackedPtr++, stPackedPtr++;
				stPackedPtr++;
				break;
			}
			case VPF_END:
				*pStResultLength = stUnpackedPtr;
				return 0;
			default:
				if (pFramebuffer)
					pFramebuffer[stUnpackedPtr++] = pPackedBuffer[stPackedPtr++];
				else
					stUnpackedPtr++, stPackedPtr++;
		}
	}

	return -1;
}

// FIXME: no proper buffer size checks
int VoznemozhnoPackDiff(uint8_t *pBuffer, size_t stBufferSize, uint8_t *pResult, size_t stResultBufferSize, size_t *pStResultLength)
{
	size_t stBuffPtr = 0;
	uint16_t wSkipSize = 0;
	size_t stResultPtr = 0;
	bool bSkipWritten = false;

	while (stBuffPtr < stBufferSize) {
		if (pBuffer[stBuffPtr] == VPD_EMPTY) {
			if (wSkipSize > 1)
				bSkipWritten = false;
			wSkipSize++;
			stBuffPtr++;
			continue;
		}
		if (!bSkipWritten) {
			if (wSkipSize == 3) {
				pResult[stResultPtr++] = VPD_SKIP3;
			} else if (wSkipSize == 4) {
				pResult[stResultPtr++] = VPD_SKIP4;
			} else if (wSkipSize == 5) {
				pResult[stResultPtr++] = VPD_SKIP5;
			} else if (wSkipSize == 6) {
				pResult[stResultPtr++] = VPD_SKIP6;
			} else if (wSkipSize == 7) {
				pResult[stResultPtr++] = VPD_SKIP7;
			} else if (wSkipSize < 256) {
				pResult[stResultPtr++] = VPD_SKIP1;
				pResult[stResultPtr++] = (wSkipSize & 0xFF);
			} else if (wSkipSize < 512) {
				pResult[stResultPtr++] = VPD_SKIP256;
				pResult[stResultPtr++] = ((wSkipSize - 256) & 0xFF);
			} else if (wSkipSize < 768) {
				pResult[stResultPtr++] = VPD_SKIP512;
				pResult[stResultPtr++] = ((wSkipSize - 512) & 0xFF);
			} else {
				pResult[stResultPtr++] = VPD_SKIP2;
				pResult[stResultPtr++] = (wSkipSize & 0xFF);
				pResult[stResultPtr++] = ((wSkipSize >> 8) & 0xFF);
			}
			bSkipWritten = true;
			wSkipSize = 0;
		} else {
			while (wSkipSize) {
				pResult[stResultPtr++] = VPD_EMPTY;
				wSkipSize--;
			}
		}
		switch (pBuffer[stBuffPtr]) {
			case VPD_SKIP2:
			case VPD_SKIP1:
			case VPD_SKIP256:
			case VPD_SKIP512:
			case VPD_SKIP3:
			case VPD_SKIP4:
			case VPD_SKIP5:
			case VPD_SKIP6:
			case VPD_SKIP7:
			case VPD_ESCAPE:
			case VPD_END:
				pResult[stResultPtr++] = VPD_ESCAPE;
			default:
				pResult[stResultPtr++] = pBuffer[stBuffPtr];
		}
		stBuffPtr++;
	}
	pResult[stResultPtr++] = VPD_END;
	
	*pStResultLength = stResultPtr;

	return 0;
}

// FIXME: no proper buffer size checks
int VoznemozhnoUnpackDiff(uint8_t *pPackedBuffer, size_t stPackedBufferSize, uint8_t *pFramebuffer, size_t stFramebufferSize, size_t *pStResultLength)
{
	if (pFramebuffer)
		memset(pFramebuffer, VPD_EMPTY, stFramebufferSize);

	size_t stPackedPtr = 0;
	size_t stUnpackedPtr = 0;
	while (stPackedPtr < stPackedBufferSize) {
		switch (pPackedBuffer[stPackedPtr]) {
			case VPD_SKIP2: {
				stUnpackedPtr += pPackedBuffer[stPackedPtr + 1] | (pPackedBuffer[stPackedPtr + 2] << 8);
				stPackedPtr += 3;
				break;
			}
			case VPD_SKIP1: {
				stUnpackedPtr += pPackedBuffer[stPackedPtr + 1];
				stPackedPtr += 2;
				break;
			}
			case VPD_SKIP256: {
				stUnpackedPtr += (pPackedBuffer[stPackedPtr + 1] + 256);
				stPackedPtr += 2;
				break;
			}
			case VPD_SKIP512: {
				stUnpackedPtr += (pPackedBuffer[stPackedPtr + 1] + 512);
				stPackedPtr += 2;
				break;
			}
			case VPD_SKIP3: {
				stUnpackedPtr += 3;
				stPackedPtr++;
				break;
			}
			case VPD_SKIP4: {
				stUnpackedPtr += 4;
				stPackedPtr++;
				break;
			}
			case VPD_SKIP5: {
				stUnpackedPtr += 5;
				stPackedPtr++;
				break;
			}
			case VPD_SKIP6: {
				stUnpackedPtr += 6;
				stPackedPtr++;
				break;
			}
			case VPD_SKIP7: {
				stUnpackedPtr += 7;
				stPackedPtr++;
				break;
			}
			case VPD_ESCAPE: {
				if (pFramebuffer)
					pFramebuffer[stUnpackedPtr++] = pPackedBuffer[++stPackedPtr];
				else
					stUnpackedPtr++, stPackedPtr++;
				stPackedPtr++;
				break;
			}
			case VPD_END:
				*pStResultLength = stUnpackedPtr;
				return 0;
			default:
				if (pFramebuffer)
					pFramebuffer[stUnpackedPtr++] = pPackedBuffer[stPackedPtr++];
				else
					stUnpackedPtr++, stPackedPtr++;
		}
	}

	return -1;
}

int PackFull(const char *szInputBufferFilename, const char *szOutputBufferFilename)
{
	int nError = 0;
	
	size_t stInputBufferSize = 0;
	nError = GetFileSize(szInputBufferFilename, &stInputBufferSize);
	if (nError)
		return nError;

	if (stInputBufferSize < 1 || stInputBufferSize > 65535) {
		printf("Input file size should be 1-65535 bytes instead of %lu\r\n", stInputBufferSize);
		return -1;
	}

	uint8_t *pInputBuffer = (uint8_t *)malloc(stInputBufferSize);
	if (!pInputBuffer) {
		printf("Failed to allocate input buffer memory\r\n");
		return -1;
	}

	nError = ReadFramebuffer(szInputBufferFilename, pInputBuffer, stInputBufferSize);
	if (nError) {
		printf("Failed to read input file\r\n");
		free(pInputBuffer);
		return -1;
	}

	size_t stOutputBufferSize = stInputBufferSize * 4;
	uint8_t *pOutputBuffer = (uint8_t *)malloc(stOutputBufferSize);
	if (!pOutputBuffer) {
		printf("Failed to allocate output buffer memory\r\n");
		free(pInputBuffer);
		return -1;
	}

	size_t stPackedSize = 0;
	nError = VoznemozhnoPackFull(pInputBuffer, stInputBufferSize, pOutputBuffer, stOutputBufferSize, &stPackedSize);
	if (nError) {
		printf("Error %u from VoznemozhnoPackFull\r\n", nError);
		free(pInputBuffer);
		free(pOutputBuffer);
		return nError;
	}
	printf("VoznemozhnoPackFull: unpacked: %lu, packed: %lu\r\n", stInputBufferSize, stPackedSize);

	nError = SaveFramebuffer(szOutputBufferFilename, pOutputBuffer, stPackedSize);

	free(pInputBuffer);
	free(pOutputBuffer);

	if (nError) {
		printf("Failed to save full buffer\r\n");
		return nError;
	}

	return 0;
}

int UnpackFull(const char *szInputBufferFilename, const char *szOutputBufferFilename)
{
	int nError = 0;
	size_t stInputBufferSize = 0;
	nError = GetFileSize(szInputBufferFilename, &stInputBufferSize);
	if (nError)
		return nError;

	if (stInputBufferSize < 1 || stInputBufferSize > 65535) {
		printf("Input file size should be 1-65535 bytes instead of %lu\r\n", stInputBufferSize);
		return -1;
	}

	uint8_t *pInputBuffer = (uint8_t *)malloc(stInputBufferSize);
	if (!pInputBuffer) {
		printf("Failed to allocate input buffer memory\r\n");
		return -1;
	}

	nError = ReadFramebuffer(szInputBufferFilename, pInputBuffer, stInputBufferSize);
	if (nError) {
		printf("Failed to read input file\r\n");
		free(pInputBuffer);
		return -1;
	}

	size_t stOutputBufferSize = 0;
	nError = VoznemozhnoUnpackFull(pInputBuffer, stInputBufferSize, NULL, 0, &stOutputBufferSize);
	if (nError) {
		printf("Error %u from VoznemozhnoUnpackFull\r\n", nError);
		free(pInputBuffer);
		return nError;
	}

	uint8_t *pOutputBuffer = (uint8_t *)malloc(stOutputBufferSize);
	if (!pOutputBuffer) {
		printf("Failed to allocate output buffer memory\r\n");
		free(pInputBuffer);
		return -1;
	}

	size_t stUnpackedSize = 0;
	nError = VoznemozhnoUnpackFull(pInputBuffer, stInputBufferSize, pOutputBuffer, stOutputBufferSize, &stUnpackedSize);
	if (nError) {
		printf("Error %u from VoznemozhnoUnpackFull\r\n", nError);
		free(pInputBuffer);
		free(pOutputBuffer);
		return nError;
	}
	printf("VoznemozhnoUnpackFull: packed: %lu, unpacked: %lu\r\n", stInputBufferSize, stUnpackedSize);
	
	nError = SaveFramebuffer(szOutputBufferFilename, pOutputBuffer, stUnpackedSize);

	free(pInputBuffer);
	free(pOutputBuffer);
	
	if (nError) {
		printf("Failed to save full buffer\r\n");
		return nError;
	}

	return 0;
}

int PackDiff(const char *szInputBufferFilename, const char *szOutputBufferFilename)
{
	int nError = 0;
	size_t stInputBufferSize = 0;
	nError = GetFileSize(szInputBufferFilename, &stInputBufferSize);
	if (nError)
		return nError;
	
	if (stInputBufferSize < 1 || stInputBufferSize > 65535) {
		printf("Input file size should be 1-65535 bytes instead of %lu\r\n", stInputBufferSize);
		return -1;
	}

	uint8_t *pInputBuffer = (uint8_t *)malloc(stInputBufferSize);
	if (!pInputBuffer) {
		printf("Failed to allocate input buffer memory\r\n");
		return -1;
	}

	nError = ReadFramebuffer(szInputBufferFilename, pInputBuffer, stInputBufferSize);
	if (nError) {
		printf("Failed to read input file\r\n");
		free(pInputBuffer);
		return -1;
	}

	size_t stOutputBufferSize = stInputBufferSize * 4;
	uint8_t *pOutputBuffer = (uint8_t *)malloc(stOutputBufferSize);
	if (!pOutputBuffer) {
		printf("Failed to allocate output buffer memory\r\n");
		free(pInputBuffer);
		return -1;
	}

	size_t stPackedSize = 0;
	nError = VoznemozhnoPackDiff(pInputBuffer, stInputBufferSize, pOutputBuffer, stOutputBufferSize, &stPackedSize);
	if (nError) {
		printf("Error %u from VoznemozhnoPackDiff\r\n", nError);
		free(pInputBuffer);
		free(pOutputBuffer);
		return nError;
	}
	printf("VoznemozhnoPackDiff: unpacked: %lu, packed: %lu\r\n", stInputBufferSize, stPackedSize);
	
	nError = SaveFramebuffer(szOutputBufferFilename, pOutputBuffer, stPackedSize);

	free(pInputBuffer);
	free(pOutputBuffer);
	
	if (nError) {
		printf("Failed to save full buffer\r\n");
		return nError;
	}

	return 0;
}

int UnpackDiff(const char *szInputBufferFilename, const char *szOutputBufferFilename)
{
	int nError = 0;
	size_t stInputBufferSize = 0;
	nError = GetFileSize(szInputBufferFilename, &stInputBufferSize);
	if (nError)
		return nError;
	
	if (stInputBufferSize < 1 || stInputBufferSize > 65535) {
		printf("Input file size should be 1-65535 bytes instead of %lu\r\n", stInputBufferSize);
		return -1;
	}

	uint8_t *pInputBuffer = (uint8_t *)malloc(stInputBufferSize);
	if (!pInputBuffer) {
		printf("Failed to allocate input buffer memory\r\n");
		return -1;
	}

	nError = ReadFramebuffer(szInputBufferFilename, pInputBuffer, stInputBufferSize);
	if (nError) {
		printf("Failed to read input file\r\n");
		free(pInputBuffer);
		return -1;
	}

	size_t stOutputBufferSize = 0;
	nError = VoznemozhnoUnpackDiff(pInputBuffer, stInputBufferSize, NULL, 0, &stOutputBufferSize);
	if (nError) {
		printf("Error %u from VoznemozhnoUnpackDiff\r\n", nError);
		free(pInputBuffer);
		return nError;
	}

	uint8_t *pOutputBuffer = (uint8_t *)malloc(stOutputBufferSize);
	if (!pOutputBuffer) {
		printf("Failed to allocate output buffer memory\r\n");
		free(pInputBuffer);
		return -1;
	}

	size_t stUnpackedSize = 0;
	nError = VoznemozhnoUnpackDiff(pInputBuffer, stInputBufferSize, pOutputBuffer, stOutputBufferSize, &stUnpackedSize);
	if (nError) {
		printf("Error %u from VoznemozhnoUnpackDiff\r\n", nError);
		free(pInputBuffer);
		free(pOutputBuffer);
		return nError;
	}
	printf("VoznemozhnoUnpackDiff: packed: %lu, unpacked: %lu\r\n", stInputBufferSize, stUnpackedSize);
	
	nError = SaveFramebuffer(szOutputBufferFilename, pOutputBuffer, stUnpackedSize);

	free(pInputBuffer);
	free(pOutputBuffer);
	
	if (nError) {
		printf("Failed to save full buffer\r\n");
		return nError;
	}

	return 0;
}

int main()
{
	int nError = 0;

	if (nError = PackFull("buffer_full.bin", "buffer_full_packed.bin"))
		return nError;

	if (nError = UnpackFull("buffer_full_packed.bin", "buffer_full_unpacked.bin"))
		return nError;

	if (nError = PackDiff("buffer_diff.bin", "buffer_diff_packed.bin"))
		return nError;

	if (nError = UnpackDiff("buffer_diff_packed.bin", "buffer_diff_unpacked.bin"))
		return nError;

	printf("Done\r\n");

	return 0;
}
