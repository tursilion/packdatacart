// Quick and dirty tool to pack data into a TI cartridge
//
// packdatacart <outfile> <program> <data1> <data2> <data3> ...
// 
// Program first ensures that the program is padded to 8k (warn if bigger).
// 
// For each datafile in the list:
// -Report the address and the bank switch for it
// -for each 8k page, copy the first 128 bytes of the program, then 8192-128 bytes of the data
// -pad the last block of each file to 8k, no sharing
// -after the last file, report the next free page
//
// For now, assumes your program is 8k and that the startup code (which MUST start
// with an instruction to switch in the first cartridge bank) is in the first
// 128 bytes. These 128 bytes are copied to every bank to guarantee startup.
//
// Creates non-inverted carts for now
//

#include <stdio.h>
#include <errno.h>
#include <string.h>

#define PAGESIZE 8192
#define HDRSIZE 128

unsigned char buf[PAGESIZE];
unsigned char header[HDRSIZE];
FILE *fIn, *fOut;

// got to stuff my credits somewhere
const unsigned char *EGOSTRING = (const unsigned char *)"Made with packdatacart by Tursi - http://harmlesslion.com - ";

void egofill(unsigned char *buf, const unsigned char *datastr, int size) {
	const unsigned char *pStr = datastr;
	while (size--) {
		*(buf++)=*(pStr++);
		if (*pStr == '\0') pStr = datastr;
	}
}

int main(int argc, char *argv[]) {
	if (argc < 4) {
		printf("packdatacart <outfile> <program> <data1> <data2> <data3> ...\n");
		printf("Program is padded to 8k, then banks are appended.\n");
		printf("First 128 bytes of program is copied to each bank\n");
		printf("to handle startup.\n");
		return -1;
	}

	fOut = fopen(argv[1], "wb");
	if (NULL == fOut) {
		printf("Failed to open outfile, code %d\n", errno);
		return 1;
	}

	// get the program 
	fIn = fopen(argv[2], "rb");
	if (NULL == fIn) {
		printf("Failed to open program file, code %d\n", errno);
		return 2;
	}
	egofill(buf, EGOSTRING, PAGESIZE);
	fread(buf, 1, PAGESIZE, fIn);

	fgetc(fIn);
	if (!feof(fIn)) {
		printf("Warning - program file may be too large - only %d bytes read!\n", PAGESIZE);
	}
	fclose(fIn);
	
	// save off the header data
	memcpy(header, buf, HDRSIZE);

	// save the first page
	fwrite(buf, 1, PAGESIZE, fOut);

	// now loop the data files
	int bank = 0x6002;
	int adr = PAGESIZE;
	for (int idx = 3; idx<argc; idx++) {
		fIn = fopen(argv[idx], "rb");
		if (NULL == fIn) {
			printf("Failed to open data file %s, code %d\n", argv[idx], errno);
			return 3;
		}

		printf("Opened data file '%s'\n", argv[idx]);
		printf("- starting address %X (page >%04X)\n", adr, bank);

		while (!feof(fIn)) {
			egofill(buf, EGOSTRING, PAGESIZE);
			memcpy(buf, header, HDRSIZE);
			if (0 == fread(&buf[HDRSIZE], 1, PAGESIZE-HDRSIZE, fIn)) break;
			fwrite(buf, 1, PAGESIZE, fOut);
			++bank;
			adr+=PAGESIZE;
		}

		fclose(fIn);
		printf("- ending address %X (page >%04X)\n", adr, bank);
	}

	// pad to a power of 2
	int x = 8192;
	while (x < adr) {
		x*=2;
	}

	if (x > adr) {
		printf("Padding to %dk\n", x/1024);
		if (x > 2*1024*1024) printf("* Warning - 2MB is max cartridge board so far.\n");
		if (x > 32*1024*1024) printf("* Warning - 32MB is max posible with current bank switch.\n");

		egofill(buf, EGOSTRING, PAGESIZE);
		memcpy(buf, header, HDRSIZE);
		while (adr < x) {
			fwrite(buf, 1, PAGESIZE, fOut);
			++bank;
			adr+=PAGESIZE;
		}
	}

	fclose(fOut);
	printf("Finished writing '%s'.\n", argv[1]);

	return 0;
}
