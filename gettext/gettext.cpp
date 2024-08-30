/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/*                                                                                                   */
/*  <Ikusa Megami Zero> Chinese Translation Project                                                  */
/*  gettext.cpp                                                                                      */
/*  To retrieve text from BIN files and save them in TXT files with UTF-8 encoding                   */
/*  Written by Xuan (xuan@moelab.org)                                                                */
/*                                                                                                   */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#include <io.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <string.h>
#include <string>
#include <locale.h>
#include "dirent.h"

using namespace std;

const int TITLE_LENGTH = 0x3C;
const _locale_t locale_japan = _create_locale(LC_CTYPE, "Japanese_Japan.932");
const string SKIP1 = "MENUMOVE.BIN";
const string SKIP2 = "BTLMENUMOVE.BIN";

long search_for_entry (int fd) {
	long entry = -1;
	long offset = _tell(fd);
	_lseek(fd, 0, SEEK_SET);
	char *title = new char[TITLE_LENGTH];
	_read(fd, title, TITLE_LENGTH);
	if (memcmp(title, "SYS4415", 7) == 0 || memcmp(title, "SYS4424", 7) == 0) {
		unsigned long dataunit;
		while (_read(fd, &dataunit, sizeof(dataunit)) != 0) {
			if (dataunit < 0xFFFFFFFF
				&& (dataunit & 0x000000FF) > 0x0000001F
				&& (dataunit & 0x0000FF00) > 0x00001F00
				&& (dataunit & 0x00FF0000) > 0x001F0000
				&& (dataunit & 0xFF000000) > 0x1F000000) {
				entry = _tell(fd) - 4;
				break;
			}
		}
	}
	delete [] title;
	_lseek(fd, offset, SEEK_SET);
	return entry;
}

bool is_text(int fd, long entry) {
	long offset = _tell(fd);
	_lseek(fd, entry + 3, SEEK_SET);
	char testunit;
	if (_read(fd, &testunit, sizeof(testunit) != 0)) {
		_lseek(fd, offset, SEEK_SET);
		if (testunit == 0x00) {
			return false;
		} else {
			return true;
		}
	} else {
		return false;
	}
}

int get_text_length(int fd, long entry) {
	long offset = _tell(fd);
	_lseek(fd, entry, SEEK_SET);
	unsigned long dataunit;
	int textlen = -1;
	while(_read(fd, &dataunit, sizeof(dataunit)) != 0) {
		for (int i = 3; i >= 0; i--) {
			if (dataunit >= 0xFF000000) {
				long end = _tell(fd);
				textlen = int(end - entry);
				break;
			}
		}
		if (textlen != -1) {
			break;
		}
	}
	_lseek(fd, offset, SEEK_SET);
	return textlen;
}

int retrieve_text(int fd, long entry, string filename) {
	long offset = _tell(fd);
	string fn = filename + ".UTF8.TXT";
	wstring fnw(fn.begin(), fn.end());
	FILE *output_file =_wfopen(fnw.c_str(), L"w, ccs=UTF-8");
	int text_count = 0;
	long next_entry = entry;
	while (is_text(fd, next_entry)) {
		int textlen = get_text_length(fd, next_entry);
		char *mb_str = new char[textlen];
		_lseek(fd, next_entry, SEEK_SET);
		_read(fd, mb_str, textlen);
		for (int i = 0; i < textlen; i++) {
			mb_str[i] = ~mb_str[i];
		}
		int textlen_b = strlen(mb_str);
		int textlen_c = _mbstrlen_l(mb_str, locale_japan);
		wchar_t *wc_str = new wchar_t[textlen_c+1];
		_mbstowcs_l(wc_str, mb_str, textlen_b, locale_japan);
		wstring output_str(wc_str);
		output_str = output_str.substr(0, textlen_c);
		fwprintf(output_file, L"[%d][JPN][%s]\n[%d][CHN][]\n\n", ++text_count, output_str.c_str(), text_count);
		next_entry += textlen;
		delete [] mb_str;
		delete [] wc_str;

	}
	fclose(output_file);
	_lseek(fd, offset, SEEK_SET);
	return 0;
}

int process_file(string filename) {
	int fd;
	if ((fd = _open(filename.c_str(), _O_RDONLY | _O_BINARY)) == -1) {
		printf("Failed to open %s\n", filename);
		return -1;
	}
	long entry = search_for_entry(fd);
	if (entry != -1) {
		retrieve_text(fd, entry, filename);
		printf("Text retrieved from %s\n", filename.c_str());
	} else {
		printf("No text in %s\n", filename.c_str());
	}
	close(fd);
}

int main(int argc, char** argv) {
	if (argc == 1) {
		DIR *dir;
		dirent *ent;
		if ((dir = opendir(".\\")) != NULL) {
			while ((ent = readdir(dir)) != NULL) {
				string filename(ent->d_name);
				if (filename.length() > 5 && filename.compare(filename.length()-3, 3, "BIN") == 0) {
					if (filename.compare(SKIP1) == 0 || filename.compare(SKIP2) == 0) {
						printf("Skipped %s\n", filename.c_str());
					} else {
						process_file(filename);
					}
				}
			}
			closedir(dir);
		}

	} else if (argc == 2) {
		string filename(argv[1]);
		process_file(filename);
	} else {
		printf("ha");
	}

	return 0;
}