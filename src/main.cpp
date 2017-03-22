//============================================================================
// Name        : Main.cpp
// Author      : Trung Huynh Duc
// Version     :
// Copyright   : Your copyright notice
// Description : Assignment1 reading floppy disk, FAT12
//============================================================================

#include <iostream>
#include <stdio.h>
#include <vector>
#include <string>
#include <algorithm>
#include <fstream>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

//#include <bitset>
using namespace std;

class Entry {

public:
	string name;
//	string Extension;
	int firstSec;
	int byteNum;

};
#define SECTORSIZE 512

//unsigned char buffer[SECTORSIZE];
FILE *flp;
vector<int> alreadyReadVec;
vector<Entry> entryList;

unsigned char FAT[4806]; // 512 * 9 = 4806;

/* Function to read one sector of a floppy image */
int readOnesec(FILE *fp, int secno, unsigned char buf[]) {
	int loc;

	loc = SECTORSIZE * secno;
	fseek(fp, loc, SEEK_SET);
	return fread(buf, SECTORSIZE, 1, fp);
}

int readSecsContiguous(FILE *fp, int secno, int length, unsigned char buf[]) //, unsigned char buf[])
		{
	int loc;
	loc = SECTORSIZE * secno;
	fseek(fp, loc, SEEK_SET);
	return fread(buf, SECTORSIZE, length, fp);
}

/* 2 byte values are stored in reverse order of what we want, so reverse
 and concatenate */
int get2byte(unsigned char buff[], int start) {
	int x;

	x = (buff[start + 1] << 8) | buff[start];
	return x;
}

int getYear(int bytes) {
	int year = bytes >> 9;
	year = year + 1980;
	return year;
}

int getMonth(int bytes) {

	int month = (bytes & 480) >> 5; // 480 = 111100000
	return month;
}
int getDay(int bytes) {

	int day = bytes & 31; // 11111
	return day;
}

int getHour(int bytes) {
	int hour = bytes >> 11;
	return hour;
}

int getMinute(int bytes) {

	int minutes = (bytes & 2016) >> 5; // 2016 = 11111100000
	return minutes;
}
int getSecond(int bytes) {

	int second = bytes & 31; // 11111
	return second * 2;
}

bool isSubdurectory(int bytes) {

	bytes = bytes & 16; // 16 = 10000

	if (bytes == 16) {
		return true;
	}

// testing only, not remove
//	if (bytes != 0) {
//		printf("-----Something wrong------- bytes: %d\n", bytes);
//	}

	return false;
}

void ReadBootsector(int secNum) {
	unsigned char buffer[SECTORSIZE];

	readOnesec(flp, secNum, buffer);
	printf("\n---------- Boot Sector-------------\n");

	printf("sectors per cluster: %d\n", buffer[13]);
	printf("bytes per sector: %d\n", get2byte(buffer, 11));
	printf("reserved sectors for the boot record: %d\n", get2byte(buffer, 0xe));
	printf("number of FATs: %d\n", buffer[0x10]);
	printf("maximum number of root directory entries: %d\n",
			get2byte(buffer, 0x11));

//	printf("Last sector in the root directory: %d\n",
//			buffer[0x11]);

	printf("number of sectors: %d\n", get2byte(buffer, 0x13));
	printf("sectors per FAT: %d\n", get2byte(buffer, 0x16));
	printf("sectors per track: %d\n", get2byte(buffer, 0x18));
	printf("number of surfaces: %d\n", get2byte(buffer, 0x1a));
	printf("----------End Boot Sector-------------\n");

}
int getFatNum(int fatNum) {

	if (fatNum % 2 == 0) {

		int eight = (3 * fatNum) / 2;
		int four = 1 + (3 * fatNum) / 2;
		eight = FAT[eight];
		four = FAT[four];
		four = (four & 15) << 8 | eight; // 00001111

		return four;
	} else {
		int four = (3 * fatNum) / 2;
		int eight = 1 + (3 * fatNum) / 2;
		eight = FAT[eight];
		four = FAT[four];
		four = (four & 240) >> 4; // 1111 0000

		eight = (eight << 4) | four;
		return eight;
	}
}

vector<int> getVectorFat(unsigned char FAT[], int startNum) {

	vector<int> vect;
	int num;

	vect.push_back(startNum);
	num = getFatNum(startNum);

	while ((num < 4080) && num != 0) // condition end file and others... 4080 =  0xff0
	{

		vect.push_back(num);
		num = getFatNum(num);

	}

	for (int i = 0; i < vect.size(); i++) {

		vect[i] = vect[i] + 31;
	}
	return vect;
}

int readSesc(FILE *fp, vector<int> vect, unsigned char buf[]) {
	int loc;
	int lastByte = 0;
	for (int i = 0; i < vect.size(); i++) {

		unsigned char bufff[SECTORSIZE];

		loc = SECTORSIZE * vect[i];
		fseek(fp, loc, SEEK_SET);
		fread(bufff, SECTORSIZE, 1, fp);

		for (int y = 0; y < 512; y++) {

			buf[lastByte] = bufff[y];
			++lastByte;

		}
	}

	return lastByte - 1;
}

bool isIgnore(int status) {

//	return true;
	status = status & 15;
	if (status != 0) {
		return false;
	} else {
		return true;
	}

}

string trim(string& str) {

	if (str == "        " || str == "   " || str == "  " || str == " ")
		return "";

	size_t first = str.find_first_not_of(' ');
	size_t last = str.find_last_not_of(' ');

	return str.substr(first, (last - first + 1));

}

void listContentRootDiAndSubDi(int secNum, bool isRoot, string tab,
		string url) {

	unsigned char* buffer;
	if (isRoot) {
		buffer = new unsigned char[SECTORSIZE * 14];
		readSecsContiguous(flp, 19, 14, buffer);
	} else {
		vector<int> listSecOnFile = getVectorFat(FAT, secNum);
		buffer = new unsigned char[SECTORSIZE * listSecOnFile.size()];
		readSesc(flp, listSecOnFile, buffer);
	}

	int y = 0;

	tab = tab + '\t';
	while (buffer[y] != 0x00) {
		int logicalCluster = get2byte(buffer, y + 26);

		int first = buffer[y];
		int status = buffer[y + 11];

		if (first == 229 || !isIgnore(status)) {

			y = y + 32;
			continue;
		}
//		else{
//			printf("first bits------------------: %d\n", first);
//		}


		int date = get2byte(buffer, y + 24);
		int time = get2byte(buffer, y + 22);
		int physicalCluster = logicalCluster + 31;
		bool isSub = isSubdurectory(status);
		string fileName(buffer + y, buffer + y + 7);
		string Extension(buffer + y + 8, buffer + y + 10);
		int size = (((((buffer[y + 31] << 8) | buffer[y + 30]) << 8)
				| buffer[y + 29]) << 8) | buffer[y + 28];

		printf("%s", tab.c_str());
		printf("file name: %s\n", fileName.c_str());
		printf("%s", tab.c_str());
		printf("Extension: %s\n", Extension.c_str());
		printf("%s", tab.c_str());
		printf("status bits: %d\n", status);
		printf("%s", tab.c_str());
		printf("size: %d bytes \n", size);
		printf("%s", tab.c_str());
		printf("the first logical cluster: %d\n", logicalCluster);
		printf("%s", tab.c_str());
		printf("the first Physical cluster: %d\n", physicalCluster);
		printf("%s", tab.c_str());
		printf("Year-Month-Day: %d-%d-%d \n", getYear(date), getMonth(date),
				getDay(date));
		printf("%s", tab.c_str());
		printf("Hour-Minute-Second: %d-%d-%d \n", getHour(time),
				getMinute(time), getSecond(time));

		bool isNavigationSub = false;
		if (fileName == ".      " || fileName == "..     ")
			isNavigationSub = true;

		bool isAlreadyHave = false;
		for (int i = 0; i < alreadyReadVec.size(); i++) {

			if (alreadyReadVec[i] == logicalCluster) {
				isAlreadyHave = true;
				break;
			}

		}

		if (isSub && !isAlreadyHave && !isNavigationSub) {
			printf("%s", tab.c_str());
			printf("status is --SUBDIRECTORY--\n");

			alreadyReadVec.push_back(logicalCluster);
			printf("%s", tab.c_str());
			printf("--------Start Subdirectory-------------\n");

			listContentRootDiAndSubDi(logicalCluster, false, tab,
					url + "/" + fileName.c_str());

			printf("%s", tab.c_str());
			printf("--------End Subdirectory---------------\n");

		} else {
			printf("%s", tab.c_str());
			printf("status is not subdirectory\n");

			if (!isNavigationSub) {

				Entry e;
				if (isRoot) {
					e.name = trim(fileName) + "." + trim(Extension);

				} else {
					e.name = url + "/" + trim(fileName) + "." + trim(Extension);

				}
				e.firstSec = logicalCluster;
				e.byteNum = size;

				entryList.push_back(e);

			}

		}
		printf("%s", tab.c_str());
		printf("-----------------------\n");

		y = y + 32;
	}

	delete[] buffer;

}
// testing only, not remove;
//void getFAT() {
////	3072
//	for (int i = 0; i < 3072; i++) {
//		printf("%d:%d | ", i, getFatNum(i));
//	}
//}

void copyFiles() {

	if(entryList.size() ==0 ){
		printf("\n---------disk does not contain files---------\n");
		return;
	}

	printf("\n---------List all files in floppy disk---------\n");
	for (int i = 0; i < entryList.size(); i++) {

		Entry e = entryList[i];
//		testing only not remove
//		printf("name: %s\t", e.name.c_str());
//		printf("first sec: %d\t", e.firstSec);
//		printf("byteNum bytes: %d\n", e.byteNum);

		printf("File's name:%s\n", e.name.c_str());
	}
	printf("---------End list---------\n");

	string name;
	int firstSec = -1;
	int bytesNum;

	while (firstSec == -1) {

		cout
				<< "enter file's name, which you want to copy (EX:"+entryList[0].name+"):";
		cin >> name;
// find file's name in vector;
		for (int i = 0; i < entryList.size(); i++) {

			Entry e = entryList[i];

			if (e.name == name) {

				printf("name: %s\n", e.name.c_str());
				firstSec = e.firstSec;
				bytesNum = e.byteNum;
				break;
			}
		}
		if (firstSec == -1)
			cout << "\nenter wrong name:\n";
	}

// remove other parts to get file's name only
	vector<int> listSecOnFile = getVectorFat(FAT, firstSec);
	unsigned char buffertest[SECTORSIZE * listSecOnFile.size()];
	readSesc(flp, listSecOnFile, buffertest);

	while (name.find("/") != string::npos) {

		name = name.substr(name.find("/") + 1, name.length());
	}

// start enter pathname
	string pathname;
	struct stat sb;
	while (true) {
		cout << "enter pathname (EX:/Users/dle/Desktop):";
		cin >> pathname;

		if (stat(pathname.c_str(), &sb) == 0 && S_ISDIR(sb.st_mode)) {
			break;
		} else {
			cout << "not Exited pathname:\n";
		}
	}

// open file and copy data into file
	ofstream outfile(pathname + "/" + name);
	for (int i = 0; i < bytesNum; i++) {
		outfile << buffertest[i];
	}
	cout << "the file " + name + " is copied to " + pathname + "\n";
	outfile.close();
}

int main(int argc, char *argv[]) {



	string fileLocation = "";

	while (access( fileLocation.c_str(), F_OK ) == -1) // check file exited
	{

		cout<<"enter file's pathname: ";
		cin>>fileLocation;
		if((access( fileLocation.c_str(), F_OK ) == -1))
		cout<<"file's pathname is incorrect\n";

	}

	flp = fopen(fileLocation.c_str(), "r");

//	read boot sector
	ReadBootsector(0);
	readSecsContiguous(flp, 1, 9, FAT); // Fat 1;

// read root directory and sub-directories
	printf("\n----------Start Root Directory-------------\n");

	listContentRootDiAndSubDi(19, true, "", ""); // 19 root directory sector;

	printf("----------End Root Directory-------------\n");
//	getFAT(); testing only

// copy a file from floppy disk to local directory
	copyFiles();

	fclose(flp);
//	  free (buffer);
}

