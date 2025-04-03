// LargeVars.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include "large_variables.hpp"
#include "self_test.h"

#include <cfloat>
#include <cmath>
#include <cstdint>
#include <format>
#include <iostream>
#include <string>
#include <string.h>

int main(int argc, const char* const argv[])
{
	using namespace std;	// :3

	if (argc > 1 && strncmp(argv[1], "--test", sizeof("--test")) == 0)
	{
		// Estimated relative times:
		self_test_addition();			// 6x
		self_test_subtraction();		// 7x
		self_test_multiplication();		// 26x
		self_test_division();			// 24x
		self_test_modulo();				// 19x
		self_test_bitwise();			// 11x
		self_test_bitshift();			// 9x
		self_test_unary();				// 11x

		return 0;
	}

	cout << "Hello World!\n";

	cout << "\n";

	cout << LargeInt(-2) << "\n";
	cout << LargeInt(-1) << "\n";
	cout << LargeInt(0) << "\n";
	cout << LargeInt(1) << "\n";
	cout << LargeInt(2) << "\n";

	cout << "\n";

	cout << LargeInt(-2ll) << "\n";
	cout << LargeInt(-1ll) << "\n";
	cout << LargeInt(0ll) << "\n";
	cout << LargeInt(1ll) << "\n";
	cout << LargeInt(2ll) << "\n";

	cout << "\n";

	cout << LargeInt(static_cast<uint64_t>(-2ll)) << "\n";
	cout << LargeInt(static_cast<uint64_t>(-1ll)) << "\n";
	cout << LargeInt(0ull) << "\n";
	cout << LargeInt(1ull) << "\n";
	cout << LargeInt(2ull) << "\n";

	cout << "\n";

	cout << LargeInt(-2.08324) << "\n";
	cout << LargeInt(-1.120321) << "\n";
	cout << LargeInt(0.9999999999) << "\n";
	cout << LargeInt(1.5123) << "\n";
	cout << LargeInt(2.723) << "\n";

	cout << "\n";

	cout << LargeInt(3) << "\n";
	cout << (LargeInt(3) << 7) << "\n";
	cout << ((int)(LargeInt(3) << 7)) << "\n";
	cout << ((double)(LargeInt(3) << 7)) << "\n";
	cout << (3 << 7) << "\n";

	cout << "\n";

	cout << LargeInt(-5) << "\n";
	cout << (LargeInt(-5) << 3) << "\n";
	cout << ((int)(LargeInt(-5) << 3)) << "\n";
	cout << ((double)(LargeInt(-5) << 3)) << "\n";
	cout << (-5 << 3) << "\n";

	cout << "\n";

	cout << (LargeInt(FLT_MAX)) << "\n";
	cout << format("{:.0f}", FLT_MAX) << "\n";
	cout << std::format("{:.0f}", (float)LargeInt(FLT_MAX)) << "\n";
	cout << LargeInt(FLT_MAX - (float)LargeInt(FLT_MAX)) << "\n";

	cout << "\n";

	cout << (LargeInt(-FLT_MAX)) << "\n";
	cout << format("{:.0f}", -FLT_MAX) << "\n";
	cout << std::format("{:.0f}", (float)LargeInt(-FLT_MAX)) << "\n";
	cout << LargeInt(-FLT_MAX - (float)LargeInt(-FLT_MAX)) << "\n";

	cout << "\n";

	cout << (LargeInt(DBL_MAX)) << "\n";
	cout << format("{:.0f}", DBL_MAX) << "\n";
	cout << std::format("{:.0f}", (double)LargeInt(DBL_MAX)) << "\n";
	cout << LargeInt(DBL_MAX - (double)LargeInt(DBL_MAX)) << "\n";

	cout << "\n";

	cout << (LargeInt(DBL_MAX) + LargeInt(pow(2, 1023) * pow(2, -54))) << "\n";
	cout << format("{:.0f}", DBL_MAX + pow(2, 1023) * pow(2, -54)) << "\n";
	cout << std::format("{:.0f}", (double)(LargeInt(DBL_MAX) + LargeInt(pow(2, 1023) * pow(2, -54)))) << "\n";
	try
	{
		cout << LargeInt((DBL_MAX + pow(2, 1023) * pow(2, -54)) - (double)(LargeInt(DBL_MAX) + LargeInt(pow(2, 1023) * pow(2, -54)))) << "\n";
	}
	catch (const LargeInt::invalid_float_conversion& exc)
	{
		cout << "An exception occurred: " << exc.what() << "\n";
	}

	cout << "\n";

	cout << (LargeInt(DBL_MAX) + LargeInt(pow(2, 1023) * pow(2, -53))) << "\n";
	cout << format("{:.0f}", DBL_MAX + pow(2, 1023) * pow(2, -53)) << "\n";
	cout << std::format("{:.0f}", (double)(LargeInt(DBL_MAX) + LargeInt(pow(2, 1023) * pow(2, -53)))) << "\n";
	try
	{
		cout << LargeInt((DBL_MAX + pow(2, 1023) * pow(2, -53)) - (double)(LargeInt(DBL_MAX) + LargeInt(pow(2, 1023) * pow(2, -53)))) << "\n";
	}
	catch (const LargeInt::invalid_float_conversion& exc)
	{
		cout << "An exception occurred: " << exc.what() << "\n";
	}

	cout << "\n";

	cout << LargeInt(65535) << "\n";
	cout << LargeInt(65535) * 65535 << "\n";
	cout << LargeInt(65535) / 65535 << "\n";

	cout << "\n";

	cout << LargeInt(5) % 3 << "\n";
	cout << LargeInt(5) % -3 << "\n";
	cout << LargeInt(-5) % 3 << "\n";
	cout << LargeInt(-5) % -3 << "\n";
	cout << ~(LargeInt(-5) % -3) << "\n";
}

// Run program: Ctrl + F5 or Debug > Start Without Debugging menu
// Debug program: F5 or Debug > Start Debugging menu

// Tips for Getting Started: 
//   1. Use the Solution Explorer window to add/manage files
//   2. Use the Team Explorer window to connect to source control
//   3. Use the Output window to see build output and other messages
//   4. Use the Error List window to view errors
//   5. Go to Project > Add New Item to create new code files, or Project > Add Existing Item to add existing code files to the project
//   6. In the future, to open this project again, go to File > Open > Project and select the .sln file
