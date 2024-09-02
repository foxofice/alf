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
#include <vector>
#include <locale.h>
#include <Windows.h>
#include <codecvt>

using namespace std;

const int TITLE_LENGTH = 0x3C;
const _locale_t locale_japan = _create_locale(LC_CTYPE, "Japanese_Japan.932");
//const string SKIP1 = "MENUMOVE.BIN";
//const string SKIP2 = "BTLMENUMOVE.BIN";
#define OUTPUT_DIR	"output_text"

bool dir_exists(const char *dir)
{
	struct _stat64i32 s;
	if(_stat(dir, &s))
		return false;

	return (s.st_mode & _S_IFDIR) > 0;
}

/*==============================================================
 * 获取指定目录的所有文件
 * GetFiles()
 *==============================================================*/
bool GetFiles(const char *path, __out vector<string> &files)
{
	char full_name[MAX_PATH];
	sprintf_s(full_name, "%s\\*.BIN", path);

	WIN32_FIND_DATAA FindFileData;
	HANDLE hFind = FindFirstFileA(full_name, &FindFileData);

	if(hFind == INVALID_HANDLE_VALUE)
		return false;

	bool ret = true;

	while(true)
	{
		if(FindFileData.cFileName[0] != '.')	// 不是当前路径或者父目录的快捷方式
		{
			sprintf_s(full_name, "%s\\%s", path, FindFileData.cFileName);

			bool is_dir = (FindFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) > 0;

			if(!is_dir)
				files.push_back(full_name);
		}

		if(!FindNextFileA(hFind, &FindFileData))
		{
			DWORD err = GetLastError();
			if(err != ERROR_NO_MORE_FILES)
				ret = false;

			break;
		}
	}	// while

	FindClose(hFind);

	return ret;
}

void get_dir_and_filename(const char path[MAX_PATH], __out char dir[MAX_PATH], __out char filename[MAX_PATH])
{
	// 找到最后一个反斜杠的位置
	const char *lastSlash1 = strrchr(path, '\\');
	const char *lastSlash2 = strrchr(path, '/');

	const char *lastSlash = __max(lastSlash1, lastSlash2);

	if(lastSlash != nullptr)
	{
		// 复制目录部分
		size_t dirLength = lastSlash - path;
		CopyMemory(dir, path, dirLength);
		dir[dirLength] = '\0';

		// 复制文件名部分
		strcpy(filename, lastSlash + 1);
	}
	else
	{
		// 如果没有找到反斜杠，说明路径中没有目录部分
		dir[0] = '\0';
		strcpy(filename, path);
	}
}

long search_for_entry (int fd, __out bool &unicode) {
	long entry = -1;
	long offset = _tell(fd);
	_lseek(fd, 0, SEEK_SET);
	char *title = new char[TITLE_LENGTH];
	_read(fd, title, TITLE_LENGTH);

	bool ansi	= !memcmp(title, "SYS44", 5);
	unicode		= !memcmp(title, L"SYS55", 10);

	//if (memcmp(title, "SYS4415", 7) == 0 || memcmp(title, "SYS4424", 7) == 0) {
	if(ansi || unicode)
	{
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
    if (_read(fd, &testunit, sizeof(testunit))) {
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

int retrieve_text(int fd, long entry, const string &filename, bool unicode)
{
	char dir[MAX_PATH], file_name[MAX_PATH];
	get_dir_and_filename(filename.c_str(), dir, file_name);

	long offset = _tell(fd);
	//string fn = filename + ".UTF8.TXT";
	string fn = (string)dir + "\\" + OUTPUT_DIR + "\\" + file_name + ".UTF8.TXT";

	FILE *output_file = fopen(fn.c_str(), unicode ? "w" : "w, ccs=UTF-8");
	int text_count = 0;
	long next_entry = entry;

	vector<char> mb_str;
	vector<WCHAR> wc_str;

	while (is_text(fd, next_entry))
	{
		int textlen = get_text_length(fd, next_entry);
		mb_str.resize(textlen);

		_lseek(fd, next_entry, SEEK_SET);
		_read(fd, mb_str.data(), textlen);

		for (int i = 0; i < textlen; i++)
			mb_str[i] = ~mb_str[i];

		if(unicode)
		{
			try
			{
				wstring_convert<codecvt_utf8_utf16<wchar_t>> convert;
				wstring utf16_str(reinterpret_cast<wchar_t*>(mb_str.data()), textlen / sizeof(wchar_t));

				string utf8_str = convert.to_bytes(utf16_str);

				fprintf(output_file, "[%d][JPN][%s]\n[%d][CHN][]\n\n", ++text_count, utf8_str.c_str(), text_count);
			}
			catch(...)
			{
				printf("[error] retrieve text failed (%s)\n", filename.c_str());
				break;
			}
		}
		else
		{
			size_t textlen_b = strlen(mb_str.data());
			size_t textlen_c = _mbstrlen_l(mb_str.data(), locale_japan);

			if(textlen_c > 0)
			{
				wc_str.resize(textlen_c + 1);

				_mbstowcs_l(wc_str.data(), mb_str.data(), textlen_b, locale_japan);
				wstring output_str(wc_str.data());
				output_str = output_str.substr(0, textlen_c);
				fwprintf(output_file, L"[%d][JPN][%s]\n[%d][CHN][]\n\n", ++text_count, output_str.c_str(), text_count);
			}
		}

		next_entry += textlen;
	}
	fclose(output_file);
	_lseek(fd, offset, SEEK_SET);
	return 0;
}

int process_file(const string &filename, size_t num, size_t count)
{
	int fd;
	if ((fd = _open(filename.c_str(), _O_RDONLY | _O_BINARY)) == -1) {
		printf("[%llu/%llu] Failed to open %s\n", num, count, filename.c_str());
		return -1;
	}

	bool unicode = false;

	long entry = search_for_entry(fd, unicode);
	if (entry != -1) {
		retrieve_text(fd, entry, filename, unicode);

		printf("[%llu/%llu] Text retrieved from %s\n", num, count, filename.c_str());
	} else {
		printf("[%llu/%llu] No text in %s\n", num, count, filename.c_str());
	}
	_close(fd);
	return 0;
}

int main(int argc, char** argv) {
	// 创建输出目录
	if(!dir_exists(OUTPUT_DIR))
	{
		if(!CreateDirectoryA(OUTPUT_DIR, nullptr))
		{
			printf("Create directory failed.\n");
			getchar();
			return -1;
		}
	}

	if(argc == 1)
	{
		vector<string> files;
		files.reserve(1024);

		if(!GetFiles(".\\", files))
		{
			printf("get files failed!\n");
			getchar();
			return -1;
		}

		size_t count = files.size();

		for(size_t i=0; i<count; ++i)
			process_file(files[i], i + 1, count);

		printf("get text done!\n");
	} else if (argc == 2) {
		string filename(argv[1]);
		process_file(filename, 1, 1);

		printf("get text done!\n");
	} else {
		printf("Usage: %s [<filename>]\n", argv[0]);
	}

	getchar();
	return 0;
}
