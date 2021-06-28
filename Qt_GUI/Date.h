#ifndef DATE_H_INCLUDED
#define DATE_H_INCLUDED

#include <iostream>
using namespace std;

class Date
{
private:
	int year, month, day;
public:
	Date() {}
	Date(const string& date);
	Date(int year, int month, int day);
	Date(const Date &d) :year(d.year), month(d.month), day(d.day) {}
	~Date() {}

	int getYear()const { return year; }
	int getMonth()const { return month; }
	int getDay()const { return day; }

	void setDate(int year, int month, int day);
	bool isLeapYear(int year)const;
	int daysOfMonth(int year, int month)const;
	void show()const;
	Date changeDays(const int days)const;
	int distance(const Date &d)const;


	friend Date operator +(const Date &d, const int days);
	friend Date operator +(const int days, const Date &d);
	Date& operator +=(int days);
	Date& operator ++();
	Date operator ++(int);

	friend Date operator -(const Date &d, const int days);
	friend int operator -(const Date &d1, const Date &d2);
	Date& operator -=(int days);
	Date& operator --();
	Date operator --(int);

	friend bool operator >(const Date &d1, const Date &d2);
	friend bool operator >=(const Date &d1, const Date &d2);
	friend bool operator <(const Date &d1, const Date &d2);
	friend bool operator <=(const Date &d1, const Date &d2);
	friend bool operator ==(const Date &d1, const Date &d2);
	friend bool operator !=(const Date &d1, const Date &d2);

	friend ostream& operator <<(ostream &out, const Date &d);
	friend istream& operator >> (istream &in, Date &d);
};

#endif // DATE_H_INCLUDED

//版权声明：本文为CSDN博主「白夜鸦羽」的原创文章，遵循CC 4.0 BY - SA版权协议，转载请附上原文出处链接及本声明。
//原文链接：https ://blog.csdn.net/u013539342/article/details/28911733