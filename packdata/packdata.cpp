/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/*                                                                                                   */
/*  <Ikusa Megami Zero> Chinese Translation Project                                                  */
/*  packdata.cpp                                                                                     */
/*  To pack alternative files into DATA7.ALF and generate a new SYS4INI.BIN file                     */
/*  Written by Xuan (xuan@moelab.org)                                                                */
/*  asmodean's code included                                                                         */
/*                                                                                                   */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */


#include <io.h>
#include <stdio.h>
#include <direct.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <string.h>
#include <string>

#include "dirent.h"
#include "lzss.h"

using namespace std;

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

void read_sect(int fd, unsigned char*& buff, unsigned long& len, unsigned char*& out_buff, unsigned long& out_len) {
	S4SECTHDR hdr;
	_read(fd, &hdr, sizeof(hdr));

	len  = hdr.length;
	buff = new unsigned char[len];
	_read(fd, buff, len);

	out_len  = hdr.original_length;
	out_buff = new unsigned char[out_len];
	//as::unlzss(buff, len, out_buff, out_len);
	unlzss(buff, len, out_buff, out_len);

	/*
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
	*/

}

struct arc_info_t {
	int    fd;
	string dir;
};

unsigned long get_file_size(int fd) {
	unsigned long current = _tell(fd);
	unsigned long filesize = _lseek(fd, 0L, SEEK_END);
	//unsigned long filesize = _tell(fd);
	_lseek(fd, current, SEEK_SET);
	return filesize;
}

// M-A-I-N is here ^_^
int main(int argc, char** argv) {

	// Read list of alternative files from CN folder
	DIR *dir;
	dirent *ent;
	int cnfilecount = 0;
	S4TOCFILENTRY * cnfiles;
	if ((dir = opendir("CN\\")) != NULL) {
		while ((ent = readdir(dir)) != NULL) {
			string filename(ent->d_name);
			if (filename.length() > 5 && filename.compare(filename.length()-3, 3, "BIN") == 0) {
				cnfilecount++;
			}
		}
		if (cnfilecount == 0) {
			printf("No game files in CN folder\n");
			return -1;
		}
		cnfiles = new S4TOCFILENTRY[cnfilecount];
		cnfilecount = 0;
		rewinddir(dir);
		while ((ent = readdir(dir)) != NULL) {
			string filename(ent->d_name);
			if (filename.length() > 5 && filename.compare(filename.length()-3, 3, "BIN") == 0) {
				strcpy(cnfiles[cnfilecount++].filename, filename.c_str());
			}
		}
		closedir(dir);
	} else {
		printf("Failed to access the CN directory\n");
		return -1;
	}

	// Create or open DATA7.ALF for pack CN files in
	int data7_fd = _open("DATA7.ALF", _O_CREAT | _O_TRUNC | _O_WRONLY | _O_BINARY, _S_IREAD | _S_IWRITE);
	if (data7_fd == -1) {
		printf("Failed to create or open DATA7.ALF\n");
		return -1;
	}

	// Start packing
	unsigned long cnfile_offset = 0;
	for (int i = 0; i < cnfilecount; i++) {
		char path[20] = "CN\\";
		int cnfile_fd = _open(strcat(path, cnfiles[i].filename),  _O_RDONLY | _O_BINARY);
		if (cnfile_fd == -1) {
			printf("Failed to create or open %s\n", cnfiles[i].filename);
			return -1;
		}
		int cnfile_size = get_file_size(cnfile_fd);
		char *data7_buf = new char[cnfile_size];
		_read(cnfile_fd, data7_buf, cnfile_size);
		close(cnfile_fd);
		_write(data7_fd, data7_buf, cnfile_size);
		delete data7_buf;
		cnfiles[i].length = cnfile_size;
		cnfiles[i].offset = cnfile_offset;
		cnfiles[i].archive_index = 6;
		cnfile_offset += cnfile_size;
	}

	// Close DATA7.ALF
	close(data7_fd);

	// Open SYS4INI.BIN and read its content into memory
	int sys4_fd = _open("SYS4INI.BIN", _O_RDONLY | _O_BINARY);
	if (sys4_fd == -1) {
		printf("Failed to open SYS4INI.BIN\n");
		return -1;
	}
	unsigned long sys4_size = get_file_size(sys4_fd);
	char *sys4_buf = new char[sys4_size];
	_read(sys4_fd, sys4_buf, sys4_size);
	_lseek(sys4_fd, 0, SEEK_SET);

	/*
	_lseek(fd, 0L, SEEK_END);
	unsigned long filesize = _tell(fd);
	char *copybuf = new char[filesize];
	_lseek(fd, 0L, SEEK_SET);
	_read(fd, copybuf, filesize);
	_lseek(fd, 0L, SEEK_SET);


	int backup_fd = _open(((string)("NEW." + in_filename)).c_str(), _O_CREAT | _O_TRUNC | _O_WRONLY | _O_BINARY, _S_IREAD | _S_IWRITE);
	if (fd == -1) {
	printf("Died!\n");
	return 1;
	}
	_write(backup_fd, copybuf, filesize);
	_close(backup_fd);
	delete copybuf;
	*/

	// follow asmodean's routines to extract file entries
	S4HDR hdr;
	_read(sys4_fd, &hdr, sizeof(hdr));

	// Hack for addon archives
	/*
	if (!memcmp(hdr.signature_title, "S4AC", 4)) {
	_lseek(sys4_fd, 268, SEEK_SET);
	}
	*/

	unsigned long lzss_len = 0;
	unsigned char *lzss_buff = NULL;
	unsigned long  toc_len  = 0;
	unsigned char* toc_buff = NULL;
	read_sect(sys4_fd, lzss_buff, lzss_len, toc_buff, toc_len);
	_close(sys4_fd);

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
	/*
	for (unsigned long i = 0; i < archdr->entry_count; i++) {
	arc_info[i].fd = _open(arcentries[i].filename, _O_RDONLY | _O_BINARY);
	if (arc_info[i].fd != -1) {
	arc_info[i].dir = (string)get_file_prefix(arcentries[i].filename) + "/";
	_mkdir(arc_info[i].dir.c_str());
	} else {
	fprintf(stderr, "%s: could not open (skipped!)\n", arcentries[i].filename);
	}
	}
	*/

	// Find file entries of altervative files and update them
	for (unsigned long i = 0; i < filhdr->entry_count; i++) {
		arc_info_t& arc = arc_info[filentries[i].archive_index];

		/*
		if (arc.fd == -1 || !filentries[i].length) {
		continue;
		}
		*/

		for (int j = 0; j < cnfilecount; j++) {
			if (strcmp(filentries[i].filename, cnfiles[j].filename) == 0) {
				filentries[i].archive_index = 6UL;  // let them be indexed to DATA7.ALF
				filentries[i].offset = cnfiles[j].offset;
				filentries[i].length = cnfiles[j].length;
			}
		}

		/*
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
		*/
	}

	// Recompress and save to NEW.SYS4INI.BIN
	lzss(toc_buff, toc_len, lzss_buff, lzss_len);
	for (unsigned long i = 0; i < sys4_size - 312; i++) {
		sys4_buf[i+312] = lzss_buff[i];
	}
	int new_sys4_fd = _open("NEW.SYS4INI.BIN", _O_CREAT | _O_TRUNC | _O_WRONLY | _O_BINARY, _S_IREAD | _S_IWRITE);
	if (new_sys4_fd == -1) {
		printf("Failed to create or open NEW.SYS4INI.BIN");
		return -1;
	}
	_write(new_sys4_fd, sys4_buf, sys4_size);
	_close(new_sys4_fd);


	delete [] arc_info;
	delete [] toc_buff;
	delete [] lzss_buff;

	return 0;
}
