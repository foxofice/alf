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
#include <locale>
#include <codecvt>
#include <vector>
#include "dirent.h"

using namespace std;

const int TITLE_LENGTH = 0x3C;
const string SKIP1 = "MENUMOVE.BIN";
const string SKIP2 = "BTLMENUMOVE.BIN";

long search_for_entry(int fd) {
    long entry = -1;
    long offset = _tell(fd);
    _lseek(fd, 0, SEEK_SET);
    vector<char> title(TITLE_LENGTH);
    _read(fd, title.data(), TITLE_LENGTH);
    if (memcmp(title.data(), "SYS4415", 7) == 0 || memcmp(title.data(), "SYS4424", 7) == 0 || memcmp(title.data(), "SYS5502", 7)) {
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
    _lseek(fd, offset, SEEK_SET);
    return entry;
}

bool is_text(int fd, long entry) {
    long offset = _tell(fd);
    _lseek(fd, entry + 3, SEEK_SET);
    char testunit;
    if (_read(fd, &testunit, sizeof(testunit)) != 0) {
        _lseek(fd, offset, SEEK_SET);
        return testunit != 0x00;
    } else {
        return false;
    }
}

int get_text_length(int fd, long entry) {
    long offset = _tell(fd);
    _lseek(fd, entry, SEEK_SET);
    unsigned long dataunit;
    int textlen = -1;
    while (_read(fd, &dataunit, sizeof(dataunit)) != 0) {
        if ((dataunit & 0xFF000000) == 0xFF000000) {
            textlen = int(_tell(fd) - entry);
            break;
        }
    }
    _lseek(fd, offset, SEEK_SET);
    return textlen;
}

int retrieve_text(int fd, long entry, string filename) {
    long offset = _tell(fd);
    string fn = filename + ".UTF8.TXT";
    FILE *output_file = fopen(fn.c_str(), "w");
    int text_count = 0;
    long next_entry = entry;
    
    while (is_text(fd, next_entry)) {
        int textlen = get_text_length(fd, next_entry);
        vector<char> mb_str(textlen);
        _lseek(fd, next_entry, SEEK_SET);
        _read(fd, mb_str.data(), textlen);
        for (int i = 0; i < textlen; i++) {
            mb_str[i] = ~mb_str[i];
        }

        wstring_convert<codecvt_utf8_utf16<wchar_t>> convert;
        wstring utf16_str(reinterpret_cast<wchar_t*>(mb_str.data()), textlen / sizeof(wchar_t));

        string utf8_str = convert.to_bytes(utf16_str);

        fprintf(output_file, "[%d][JPN][%s]\n[%d][CHN][]\n\n", ++text_count, utf8_str.c_str(), text_count);
        next_entry += textlen;
    }
    fclose(output_file);
    _lseek(fd, offset, SEEK_SET);
    return 0;
}

int process_file(const string& filename) {
    int fd;
    if ((fd = _open(filename.c_str(), _O_RDONLY | _O_BINARY)) == -1) {
        printf("Failed to open %s\n", filename.c_str());
        return -1;
    }
    long entry = search_for_entry(fd);
    if (entry != -1) {
        retrieve_text(fd, entry, filename);
        printf("Text retrieved from %s\n", filename.c_str());
    } else {
        printf("No text in %s\n", filename.c_str());
    }
    _close(fd);
    return 0;
}

int main(int argc, char** argv) {
    if (argc == 1) {
        DIR *dir;
        dirent *ent;
        if ((dir = opendir(".\\")) != NULL) {
            while ((ent = readdir(dir)) != NULL) {
                string filename(ent->d_name);
                if (filename.length() > 5 && filename.compare(filename.length() - 3, 3, "BIN") == 0) {
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
        printf("Usage: %s [filename]\n", argv[0]);
    }

    return 0;
}