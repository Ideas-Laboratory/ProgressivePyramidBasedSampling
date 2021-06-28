#ifndef DATE_CPP
#define DATE_CPP

#include "Date.h"
#include <iostream>
#include <cstdlib>
using namespace std;

Date::Date(const string& date)
{
	int year, month, day;
	sscanf_s(date.c_str(), "%d-%d-%d", &year, &month, &day);
	*this = Date(year, month, day);
}

Date::Date(int year, int month, int day) :year(year), month(month), day(day)
{
	if (year <= 0 || month <= 0 || month>12 || day <= 0 || day>daysOfMonth(year, month)) {
		cout << "Error invalid date: ";
		show();
		exit(-1);
	}
}

void Date::setDate(int year, int month, int day)
{
	this->year = year;
	this->month = month;
	this->day = day;
}

bool Date::isLeapYear(int year)const
{
	return year % 4 == 0 && year % 100 != 0 || year % 400 == 0;
}

int Date::daysOfMonth(int year, int month)const
{
	int days = 0;

	switch (month)
	{
	case 1:
	case 3:
	case 5:
	case 7:
	case 8:
	case 10:
	case 12:
		days = 31;
		break;
	case 4:
	case 6:
	case 9:
	case 11:
		days = 30;
		break;
	case 2:
		days = 28 + isLeapYear(year);
		break;
	}

	return days;
}

void Date::show()const
{
	cout << year << "-" << month << "-" << day << endl;
}

Date Date::changeDays(const int days)const
{
	int yearTemp = year;
	int monthTemp = month;
	int dayTemp = day;

	if (days>0) {
		dayTemp += days;

		while (dayTemp>daysOfMonth(yearTemp, monthTemp)) {
			dayTemp -= daysOfMonth(yearTemp, monthTemp);

			monthTemp++;
			if (monthTemp>12) {
				yearTemp++;
				monthTemp = 1;
			}
		}
	}
	else {
		dayTemp += days;

		while (dayTemp<1) {
			monthTemp--;
			if (monthTemp<1) {
				yearTemp--;
				monthTemp = 12;
			}
			dayTemp += daysOfMonth(yearTemp, monthTemp);
		}
	}

	return Date(yearTemp, monthTemp, dayTemp);
}

int Date::distance(const Date &d)const
{
	const int DAYS_OF_MONTH[] =
	{ 0,31,59,90,120,151,181,212,243,273,304,334,365 };

	int years = year - d.year;
	int months = DAYS_OF_MONTH[month] - DAYS_OF_MONTH[d.month];
	int days = day - d.day;

	int totalDays = years * 365 + years / 4 - years / 100 + years / 400
		+ months + days;

	return totalDays;
}

Date operator +(const Date &d, const int days)
{
	if (days == 0) {
		return d;
	}
	else
		return d.changeDays(days);
}

Date operator +(const int days, const Date &d)
{
	if (days == 0) {
		return d;
	}
	else
		return d.changeDays(days);
}

Date& Date::operator +=(int days)
{
	if (days == 0)
		return *this;
	else {
		*this = this->changeDays(days);
		return *this;
	}
}

Date& Date::operator ++()
{
	*this = this->changeDays(1);
	return *this;
}

Date Date::operator ++(int)
{
	Date dTemp(*this);
	++(*this);
	return dTemp;
}

Date operator -(const Date &d, const int days)
{
	if (days == 0) {
		return d;
	}
	else
		return d.changeDays(-days);
}

int operator -(const Date &d1, const Date &d2)
{
	if (d1<d2) {
		cout << "d1 should be larger than d2!" << endl;
		exit(-1);
	}
	else if (d1 == d2)
		return 0;
	else
		return d1.distance(d2);
}

Date& Date::operator -=(int days)
{
	if (days == 0)
		return *this;
	else {
		*this = this->changeDays(-days);
		return *this;
	}
}

Date& Date::operator--()
{
	*this = this->changeDays(-1);
	return *this;
}

Date Date::operator--(int)
{
	Date dTemp(*this);
	--(*this);
	return dTemp;
}

bool operator >(const Date &d1, const Date &d2)
{
	return d1.distance(d2)>0 ? true : false;
}

bool operator >=(const Date &d1, const Date &d2)
{
	return d1.distance(d2) >= 0 ? true : false;
}

bool operator <(const Date &d1, const Date &d2)
{
	return d1.distance(d2)<0 ? true : false;
}

bool operator <=(const Date &d1, const Date &d2)
{
	return d1.distance(d2) <= 0 ? true : false;
}

bool operator ==(const Date &d1, const Date &d2)
{
	return d1.distance(d2) == 0 ? true : false;
}

bool operator !=(const Date &d1, const Date &d2)
{
	return d1.distance(d2) != 0 ? true : false;
}

ostream& operator <<(ostream &out, const Date &d)
{
	out << d.getYear() << "-"
		<< d.getMonth() << "-"
		<< d.getDay() << endl;

	return out;
}

istream& operator >> (istream &in, Date &d)
{
	int year, month, day;

	cout << "Input year-month-day:" << endl;
	in >> year >> month >> day;

	d.setDate(year, month, day);

	return in;
}

#endif // DATE_CPP