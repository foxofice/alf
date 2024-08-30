/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/*                                                                                                   */
/*  <Ikusa Megami Zero> Chinese Translation Project                                                  */
/*  getdata.cpp                                                                                      */
/*  Modified version of asmodean's code to extract resources from ALF files                          */
/*  Modified by Xuan (xuan@moelab.org)                                                               */
/*                                                                                                   */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */


// exs4alf.cpp, v1.1 2009/04/26
// coded by asmodean

// contact: 
//   web:   http://asmodean.reverse.net
//   email: asmodean [at] hush.com
//   irc:   asmodean on efnet (irc.efnet.net)

// This tool extracts S4IC413 (sys4ini.bin + *.ALF) and S4AC422 (*.AAI + *.ALF)
// archives.

#include <io.h>
#include <stdio.h>
#include <direct.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <string.h>
#include <string>

#include "lzss.h"

using namespace std;

//#include "as-util.h"
//#include "as-lzss.h"

char * get_file_prefix(char * filename) {
	char * pch = strrchr(filename, '.');
	int nch = pch - filename;
	char *buf = new char[nch+1];
	strncpy(buf, filename, nch);
	buf[nch] = NULL;
	return buf;
}

struct S4HDR {
	char          signature_title[240]; // "S4IC413 <title>", "S4AC422 <title>"
	unsigned char unknown[60];
};

struct S4SECTHDR {
	unsigned long original_length;
	unsigned long original_length2; // why?
	unsigned long length;
};

struct S4TOCARCHDR {
	unsigned long entry_count;
};

struct S4TOCARCENTRY {
	// There's a bunch of junk following the name which I assume is
	// uninitialized memory...
	char filename[256];
};

typedef S4TOCARCHDR S4TOCFILHDR;

struct S4TOCFILENTRY {
	char          filename[64];
	unsigned long archive_index;
	unsigned long file_index; // within archive?
	unsigned long offset;
	unsigned long length;
};

void read_sect(int fd, unsigned char*& out_buff, unsigned long& out_len) {
	S4SECTHDR hdr;
	_read(fd, &hdr, sizeof(hdr));

	unsigned long  len  = hdr.length;
	unsigned char* buff = new unsigned char[len];
	_read(fd, buff, len);

	out_len  = hdr.original_length;
	out_buff = new unsigned char[out_len];
	//as::unlzss(buff, len, out_buff, out_len);
	unlzss(buff, len, out_buff, out_len);

	int fd1;
	if ((fd1 = _open("lzssdata.bin", _O_WRONLY | _O_BINARY | _O_CREAT, _S_IREAD | _S_IWRITE)) != -1) {
		if (_write(fd1, buff, len) != -1) {
			printf("Original lzss data saved\n");
		}
	}
	_close(fd1);
	int fd2;
	if ((fd2 = _open("lzssdata2.bin", _O_WRONLY | _O_BINARY | _O_CREAT, _S_IREAD | _S_IWRITE)) != -1) {
		if (_write(fd2, out_buff, out_len) != -1) {
			printf("extracted lzss data saved\n");
		}
	}
	_close(fd2);


	delete [] buff;
}

struct arc_info_t {
	int    fd;
	string dir;
};

int main(int argc, char** argv) {
	if (argc != 2) {
		fprintf(stderr, "exs4alf v1.01 by asmodean\n\n");
		fprintf(stderr, "usage: %s <sys4ini.bin>\n", argv[0]);
		return -1;
	}

	string in_filename(argv[1]);

	//int fd = as::open_or_die(in_filename, O_RDONLY | O_BINARY);
	int fd = _open(in_filename.c_str(), _O_RDONLY | _O_BINARY);
	if (fd == -1) {
		printf("Died!\n");
		return 1;
	}

	S4HDR hdr;
	_read(fd, &hdr, sizeof(hdr));

	// Hack for addon archives
	if (!memcmp(hdr.signature_title, "S4AC", 4)) {
		_lseek(fd, 268, SEEK_SET);
	}

	unsigned long  toc_len  = 0;
	unsigned char* toc_buff = NULL;
	read_sect(fd, toc_buff, toc_len);
	_close(fd);

	S4TOCARCHDR*   archdr     = (S4TOCARCHDR*) toc_buff; /* Note: unsigned char buf[4] = {0x10, 0x20, 0x30, 0x40};
	                                                      *       unsigned long *l = (unsigned long *)buf;
														  *       what is the value of l? it's the entry (address) of buf
														  *       what is the value of *l? it's 0x40302010
														  *       that's how it works
														  *       why? I don't know
														  */

	S4TOCARCENTRY* arcentries = (S4TOCARCENTRY*) (archdr + 1); /* Note: Pointer addition is not numeric addition
															    *       archdr + 1 means archdr + sizeof(archdr) * 1
																*       the same applies below
																* Reference: Pointer arithmetics
																*            http://www.cplusplus.com/doc/tutorial/pointers/
																*/

	S4TOCFILHDR*   filhdr     = (S4TOCFILHDR*) (arcentries + archdr->entry_count);
	S4TOCFILENTRY* filentries = (S4TOCFILENTRY*) (filhdr + 1);

	arc_info_t* arc_info = new arc_info_t[archdr->entry_count];

//	for (unsigned long i = 0; i < archdr->entry_count; i++) {
//		arc_info[i].fd = open(arcentries[i].filename, O_RDONLY | O_BINARY);
//		if (arc_info[i].fd != -1) {
//			arc_info[i].dir = as::get_file_prefix(arcentries[i].filename) + "/";
//			as::make_path(arc_info[i].dir);
//		} else {
//			fprintf(stderr, "%s: could not open (skipped!)\n", arcentries[i].filename);
//		}
//	}
	for (unsigned long i = 0; i < archdr->entry_count; i++) {
		arc_info[i].fd = _open(arcentries[i].filename, _O_RDONLY | _O_BINARY);
		if (arc_info[i].fd != -1) {
			arc_info[i].dir = (string)get_file_prefix(arcentries[i].filename) + "/";
			_mkdir(arc_info[i].dir.c_str());
		} else {
			fprintf(stderr, "%s: could not open (skipped!)\n", arcentries[i].filename);
		}
	}

	for (unsigned long i = 0; i < filhdr->entry_count; i++) {
		arc_info_t& arc = arc_info[filentries[i].archive_index];

		if (arc.fd == -1 || !filentries[i].length) {
			continue;
		}

		unsigned long  len  = filentries[i].length;
		unsigned char* buff = new unsigned char[len];
		_lseek(arc.fd, filentries[i].offset, SEEK_SET);
		_read(arc.fd, buff, len);

		//int out_fd = as::open_or_die(arc.dir + filentries[i].filename,
		//	O_CREAT | O_TRUNC | O_WRONLY | O_BINARY,
		//	S_IREAD | S_IWRITE);
		int out_fd = _open(((string)(arc.dir + filentries[i].filename)).c_str(), _O_CREAT | _O_TRUNC | _O_WRONLY | _O_BINARY, _S_IREAD | _S_IWRITE);
		if (fd == -1) {
			printf("Died!\n");
			return 1;
		}
		_write(out_fd, buff, len);
		_close(out_fd);

		delete [] buff;
	}

	delete [] arc_info;
	delete [] toc_buff;

	return 0;
}
