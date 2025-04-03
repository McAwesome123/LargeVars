#include "large_variables.hpp"
#include "self_test.h"

#include <chrono>
#include <cstdint>
#include <format>
#include <iostream>
#include <memory>
#include <thread>
#include <utility>
#include <vector>

// sorry I can't be bothered to write comments for this piece of fuck file
// it tests various operations and it's multithreaded.
// by default it'll try to limit itself to using 75% of the available threads
// and spawn 8 times as many threads as that number is.
// yes, i did pull those numbers out of my ass.

constexpr uint64_t max_reported_errors = 1000;

#if defined (_WIN32)
	#include <Windows.h>
	#include <basetsd.h>
	#include <cmath>
	#include <errhandlingapi.h>
	#include <processthreadsapi.h>

	constexpr double thread_count_mult = 8.0;
	constexpr double affinity_count_mult = 0.75;
	constexpr size_t max_str_buffer = 32768;

	static void set_process_affinity()
	{
		using namespace std;

		#ifdef UNICODE
		using format_message_str = wchar_t[];
		#else
		using format_message_str = char[];
		#endif

		// win32 api is a fuck
		HANDLE process = GetCurrentProcess();
		DWORD_PTR process_affinity_mask;
		DWORD_PTR system_affinity_mask;

		if (!GetProcessAffinityMask(process, &process_affinity_mask, &system_affinity_mask))
		{
			unique_ptr<format_message_str> str = make_unique<format_message_str>(max_str_buffer);

			if (FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, nullptr, GetLastError(), MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), str.get(), max_str_buffer, nullptr) != 0)
			{
				wcout << "Error getting affinity mask: " << str.get() << "\n";
				return;
			}
			else
			{
				cout << "Error getting affinity mask: " << GetLastError() << "\n";
				return;
			}
		}

		if (process_affinity_mask != system_affinity_mask)
		{
			return;
		}

		unsigned int thread_shift_count = thread::hardware_concurrency();

		if (thread_shift_count == 0)
		{
			return;
		}

		thread_shift_count = static_cast<unsigned int>(trunc(thread_shift_count * (1 - affinity_count_mult)));

		process_affinity_mask = (process_affinity_mask >> thread_shift_count) & system_affinity_mask;

		if (!SetProcessAffinityMask(process, process_affinity_mask))
		{
			unique_ptr<format_message_str> str = make_unique<format_message_str>(max_str_buffer);

			if (FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, nullptr, GetLastError(), MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), str.get(), max_str_buffer, nullptr) != 0)
			{
				wcout << "Error setting affinity mask: " << str.get() << "\n";
				return;
			}
			else
			{
				cout << "Error setting affinity mask: " << GetLastError() << "\n";
				return;
			}
		}
	}
#else
	constexpr double thread_count_mult = 0.75;
	constexpr double affinity_count_mult = 1.0;

	static void set_process_affinity()
	{
		return;
	}
#endif

void self_test_addition()
{
	using namespace std;

	cout << "\nRunning addition self test. This may take a while...\n";

	set_process_affinity();

	constexpr int32_t startA = INT16_MIN;
	constexpr int32_t stopA = INT16_MAX + 1;
	constexpr int32_t startB = INT16_MIN;
	constexpr int32_t stopB = INT16_MAX + 1;

	const auto start = chrono::high_resolution_clock::now();

	auto single_test = [](int32_t start, uint32_t step_size, vector<pair<int64_t, int64_t>> *failed_tests, uint64_t* num_failed_tests)
	{
		for (int32_t a = start; a < stopA; a += step_size)
		{
			for (int32_t b = startB; b < stopB; b++)
			{
				int64_t numA = static_cast<int64_t>(a);
				int64_t numB = static_cast<int64_t>(b);

				const int64_t result = numA + numB;

				LargeInt largeIntA = LargeInt(numA);
				LargeInt largeIntB = LargeInt(numB);

				LargeInt largeIntResult = largeIntA + largeIntB;

				LargeInt res = LargeInt(result);
				if (res != largeIntResult)
				{
					if (*num_failed_tests < max_reported_errors)
					{
						failed_tests->push_back(make_pair(numA, numB));
					}
					(*num_failed_tests)++;
				}
			}
		}
	};

	unsigned int thread_count = static_cast<unsigned int>(trunc(max(thread::hardware_concurrency() * thread_count_mult * affinity_count_mult, 4.0)));
	vector<jthread> threads = {};
	vector<vector<pair<int64_t, int64_t>>> failed_tests = {};
	vector<uint64_t> num_failed_tests = {};

	failed_tests.reserve(thread_count);
	num_failed_tests.reserve(thread_count);

	for (unsigned int count = 0; count < thread_count; count++)
	{
		failed_tests.push_back(vector<pair<int64_t, int64_t>>(vector<pair<int64_t, int64_t>>()));
		num_failed_tests.push_back(0);
		threads.push_back(jthread(single_test, startA + count, thread_count, &(*failed_tests.rbegin()), &(*num_failed_tests.rbegin())));
	}

	for (auto& iter : threads)
	{
		iter.join();
	}

	uint64_t total_failed_tests = 0;

	for (auto& iter : num_failed_tests)
	{
		total_failed_tests += iter;
	}

	const auto stop = chrono::high_resolution_clock::now();
	const auto duration = chrono::duration_cast<chrono::milliseconds>(stop - start);

	cout << "Tests finished. Took: " << duration.count() / 1000 << "." << format("{:03}", duration.count() % 1000) << "s." << endl;
	cout << "\nTotal tests done: " << (static_cast<uint64_t>(stopA) - startA) * (static_cast<uint64_t>(stopB) - startB) << endl;
	cout << "Total failed tests: " << total_failed_tests << endl;

	if (total_failed_tests > 0)
	{
		cout << "\nErrors encountered:\n";
		uint64_t num_errors_reported = max_reported_errors;
		for (auto outer_iter = failed_tests.begin(); outer_iter != failed_tests.end() && num_errors_reported > 0; outer_iter++)
		{
			for (auto inner_iter = outer_iter->begin(); inner_iter != outer_iter->end() && num_errors_reported > 0; inner_iter++, num_errors_reported--)
			{
				const int64_t numA = inner_iter->first;
				const int64_t numB = inner_iter->second;

				const int64_t result = numA + numB;

				LargeInt largeIntA = LargeInt(numA);
				LargeInt largeIntB = LargeInt(numB);

				LargeInt largeIntResult = largeIntA + largeIntB;

				cout << "Expected: " << numA << " + " << numB << " = " << result
					<< ", Got: " << largeIntA << " + " << largeIntB << " = " << largeIntResult << endl;
			}
		}
	}
}

void self_test_subtraction()
{
	using namespace std;

	cout << "\nRunning subtraction self test. This may take a while...\n";

	set_process_affinity();

	constexpr int32_t startA = INT16_MIN;
	constexpr int32_t stopA = INT16_MAX + 1;
	constexpr int32_t startB = INT16_MIN;
	constexpr int32_t stopB = INT16_MAX + 1;

	const auto start = chrono::high_resolution_clock::now();

	auto single_test = [](int32_t start, uint32_t step_size, vector<pair<int64_t, int64_t>> *failed_tests, uint64_t* num_failed_tests)
	{
		for (int32_t a = start; a < stopA; a += step_size)
		{
			for (int32_t b = startB; b < stopB; b++)
			{
				int64_t numA = static_cast<int64_t>(a);
				int64_t numB = static_cast<int64_t>(b);

				const int64_t result = numA - numB;

				LargeInt largeIntA = LargeInt(numA);
				LargeInt largeIntB = LargeInt(numB);

				LargeInt largeIntResult = largeIntA - largeIntB;

				if (LargeInt(result) != largeIntResult)
				{
					if (*num_failed_tests < max_reported_errors)
					{
						failed_tests->push_back(make_pair(numA, numB));
					}
					(*num_failed_tests)++;
				}
			}
		}
	};

	unsigned int thread_count = static_cast<unsigned int>(trunc(max(thread::hardware_concurrency() * thread_count_mult * affinity_count_mult, 4.0)));
	vector<jthread> threads = {};
	vector<vector<pair<int64_t, int64_t>>> failed_tests = {};
	vector<uint64_t> num_failed_tests = {};

	failed_tests.reserve(thread_count);
	num_failed_tests.reserve(thread_count);

	for (unsigned int count = 0; count < thread_count; count++)
	{
		failed_tests.push_back(vector<pair<int64_t, int64_t>>(vector<pair<int64_t, int64_t>>()));
		num_failed_tests.push_back(0);
		threads.push_back(jthread(single_test, startA + count, thread_count, &(*failed_tests.rbegin()), &(*num_failed_tests.rbegin())));
	}

	for (auto& iter : threads)
	{
		iter.join();
	}

	uint64_t total_failed_tests = 0;

	for (auto& iter : num_failed_tests)
	{
		total_failed_tests += iter;
	}

	const auto stop = chrono::high_resolution_clock::now();
	const auto duration = chrono::duration_cast<chrono::milliseconds>(stop - start);

	cout << "Tests finished. Took: " << duration.count() / 1000 << "." << format("{:03}", duration.count() % 1000) << "s." << endl;
	cout << "\nTotal tests done: " << (static_cast<uint64_t>(stopA) - startA) * (static_cast<uint64_t>(stopB) - startB) << endl;
	cout << "Total failed tests: " << total_failed_tests << endl;

	if (total_failed_tests > 0)
	{
		cout << "\nErrors encountered:\n";
		uint64_t num_errors_reported = max_reported_errors;
		for (auto outer_iter = failed_tests.begin(); outer_iter != failed_tests.end() && num_errors_reported > 0; outer_iter++)
		{
			for (auto inner_iter = outer_iter->begin(); inner_iter != outer_iter->end() && num_errors_reported > 0; inner_iter++, num_errors_reported--)
			{
				const int64_t numA = inner_iter->first;
				const int64_t numB = inner_iter->second;

				const int64_t result = numA - numB;

				LargeInt largeIntA = LargeInt(numA);
				LargeInt largeIntB = LargeInt(numB);

				LargeInt largeIntResult = largeIntA - largeIntB;

				cout << "Expected: " << numA << " - " << numB << " = " << result
					<< ", Got: " << largeIntA << " - " << largeIntB << " = " << largeIntResult << endl;
			}
		}
	}
}

void self_test_multiplication()
{
	using namespace std;

	cout << "\nRunning multiplication self test. This may take a while...\n";

	set_process_affinity();

	constexpr int32_t startA = INT16_MIN;
	constexpr int32_t stopA = INT16_MAX + 1;
	constexpr int32_t startB = INT16_MIN;
	constexpr int32_t stopB = INT16_MAX + 1;

	const auto start = chrono::high_resolution_clock::now();

	auto single_test = [](int32_t start, uint32_t step_size, vector<pair<int64_t, int64_t>>* failed_tests, uint64_t* num_failed_tests)
	{
		for (int32_t a = start; a < stopA; a += step_size)
		{
			for (int32_t b = startB; b < stopB; b++)
			{
				int64_t numA = static_cast<int64_t>(a);
				int64_t numB = static_cast<int64_t>(b);

				const int64_t result = numA * numB;

				LargeInt largeIntA = LargeInt(numA);
				LargeInt largeIntB = LargeInt(numB);

				LargeInt largeIntResult = largeIntA * largeIntB;

				if (LargeInt(result) != largeIntResult)
				{
					if (*num_failed_tests < max_reported_errors)
					{
						failed_tests->push_back(make_pair(numA, numB));
					}
					(*num_failed_tests)++;
				}
			}
		}
	};

	unsigned int thread_count = static_cast<unsigned int>(trunc(max(thread::hardware_concurrency() * thread_count_mult * affinity_count_mult, 4.0)));
	vector<jthread> threads = {};
	vector<vector<pair<int64_t, int64_t>>> failed_tests = {};
	vector<uint64_t> num_failed_tests = {};

	failed_tests.reserve(thread_count);
	num_failed_tests.reserve(thread_count);

	for (unsigned int count = 0; count < thread_count; count++)
	{
		failed_tests.push_back(vector<pair<int64_t, int64_t>>(vector<pair<int64_t, int64_t>>()));
		num_failed_tests.push_back(0);
		threads.push_back(jthread(single_test, startA + count, thread_count, &(*failed_tests.rbegin()), &(*num_failed_tests.rbegin())));
	}

	for (auto& iter : threads)
	{
		iter.join();
	}

	uint64_t total_failed_tests = 0;

	for (auto& iter : num_failed_tests)
	{
		total_failed_tests += iter;
	}

	const auto stop = chrono::high_resolution_clock::now();
	const auto duration = chrono::duration_cast<chrono::milliseconds>(stop - start);

	cout << "Tests finished. Took: " << duration.count() / 1000 << "." << format("{:03}", duration.count() % 1000) << "s." << endl;
	cout << "\nTotal tests done: " << (static_cast<uint64_t>(stopA) - startA) * (static_cast<uint64_t>(stopB) - startB) << endl;
	cout << "Total failed tests: " << total_failed_tests << endl;

	if (total_failed_tests > 0)
	{
		cout << "\nErrors encountered:\n";
		uint64_t num_errors_reported = max_reported_errors;
		for (auto outer_iter = failed_tests.begin(); outer_iter != failed_tests.end() && num_errors_reported > 0; outer_iter++)
		{
			for (auto inner_iter = outer_iter->begin(); inner_iter != outer_iter->end() && num_errors_reported > 0; inner_iter++, num_errors_reported--)
			{
				const int64_t numA = inner_iter->first;
				const int64_t numB = inner_iter->second;

				const int64_t result = numA * numB;

				LargeInt largeIntA = LargeInt(numA);
				LargeInt largeIntB = LargeInt(numB);

				LargeInt largeIntResult = largeIntA * largeIntB;

				cout << "Expected: " << numA << " * " << numB << " = " << result
					<< ", Got: " << largeIntA << " * " << largeIntB << " = " << largeIntResult << endl;
			}
		}
	}
}

void self_test_division()
{
	using namespace std;

	cout << "\nRunning division self test. This may take a while...\n";

	set_process_affinity();

	constexpr int32_t startA = INT16_MIN;
	constexpr int32_t stopA = INT16_MAX + 1;
	constexpr int32_t startB = INT16_MIN;
	constexpr int32_t stopB = INT16_MAX + 1;

	const auto start = chrono::high_resolution_clock::now();

	auto single_test = [](int32_t start, uint32_t step_size, vector<pair<int64_t, int64_t>>* failed_tests, uint64_t* num_failed_tests)
	{
		for (int32_t a = start; a < stopA; a += step_size)
		{
			for (int32_t b = startB; b < stopB; b++)
			{
				if (b == 0)
				{
					continue;
				}

				int64_t numA = static_cast<int64_t>(a);
				int64_t numB = static_cast<int64_t>(b);

				const int64_t result = numA / numB;

				LargeInt largeIntA = LargeInt(numA);
				LargeInt largeIntB = LargeInt(numB);

				LargeInt largeIntResult = largeIntA / largeIntB;

				if (LargeInt(result) != largeIntResult)
				{
					if (*num_failed_tests < max_reported_errors)
					{
						failed_tests->push_back(make_pair(numA, numB));
					}
					(*num_failed_tests)++;
				}
			}
		}
	};

	unsigned int thread_count = static_cast<unsigned int>(trunc(max(thread::hardware_concurrency() * thread_count_mult * affinity_count_mult, 4.0)));
	vector<jthread> threads = {};
	vector<vector<pair<int64_t, int64_t>>> failed_tests = {};
	vector<uint64_t> num_failed_tests = {};

	failed_tests.reserve(thread_count);
	num_failed_tests.reserve(thread_count);

	for (unsigned int count = 0; count < thread_count; count++)
	{
		failed_tests.push_back(vector<pair<int64_t, int64_t>>(vector<pair<int64_t, int64_t>>()));
		num_failed_tests.push_back(0);
		threads.push_back(jthread(single_test, startA + count, thread_count, &(*failed_tests.rbegin()), &(*num_failed_tests.rbegin())));
	}

	for (auto& iter : threads)
	{
		iter.join();
	}

	uint64_t total_failed_tests = 0;

	for (auto& iter : num_failed_tests)
	{
		total_failed_tests += iter;
	}

	const auto stop = chrono::high_resolution_clock::now();
	const auto duration = chrono::duration_cast<chrono::milliseconds>(stop - start);

	cout << "Tests finished. Took: " << duration.count() / 1000 << "." << format("{:03}", duration.count() % 1000) << "s." << endl;
	cout << "\nTotal tests done: " << (static_cast<uint64_t>(stopA) - startA) * (static_cast<uint64_t>(stopB) - startB) << endl;
	cout << "Total failed tests: " << total_failed_tests << endl;

	if (total_failed_tests > 0)
	{
		cout << "\nErrors encountered:\n";
		uint64_t num_errors_reported = max_reported_errors;
		for (auto outer_iter = failed_tests.begin(); outer_iter != failed_tests.end() && num_errors_reported > 0; outer_iter++)
		{
			for (auto inner_iter = outer_iter->begin(); inner_iter != outer_iter->end() && num_errors_reported > 0; inner_iter++, num_errors_reported--)
			{
				const int64_t numA = inner_iter->first;
				const int64_t numB = inner_iter->second;

				const int64_t result = numA / numB;

				LargeInt largeIntA = LargeInt(numA);
				LargeInt largeIntB = LargeInt(numB);

				LargeInt largeIntResult = largeIntA / largeIntB;

				cout << "Expected: " << numA << " / " << numB << " = " << result
					<< ", Got: " << largeIntA << " / " << largeIntB << " = " << largeIntResult << endl;
			}
		}
	}
}

void self_test_modulo()
{
	using namespace std;

	cout << "\nRunning modulo self test. This may take a while...\n";

	set_process_affinity();

	constexpr int32_t startA = INT16_MIN;
	constexpr int32_t stopA = INT16_MAX + 1;
	constexpr int32_t startB = INT16_MIN;
	constexpr int32_t stopB = INT16_MAX + 1;

	const auto start = chrono::high_resolution_clock::now();

	auto single_test = [](int32_t start, uint32_t step_size, vector<pair<int64_t, int64_t>>* failed_tests, uint64_t* num_failed_tests)
	{
		for (int32_t a = start; a < stopA; a += step_size)
		{
			for (int32_t b = startB; b < stopB; b++)
			{
				if (b == 0)
				{
					continue;
				}

				int64_t numA = static_cast<int64_t>(a);
				int64_t numB = static_cast<int64_t>(b);

				const int64_t result = numA % numB;

				LargeInt largeIntA = LargeInt(numA);
				LargeInt largeIntB = LargeInt(numB);

				LargeInt largeIntResult = largeIntA % largeIntB;

				if (LargeInt(result) != largeIntResult)
				{
					if (*num_failed_tests < max_reported_errors)
					{
						failed_tests->push_back(make_pair(numA, numB));
					}
					(*num_failed_tests)++;
				}
			}
		}
	};

	unsigned int thread_count = static_cast<unsigned int>(trunc(max(thread::hardware_concurrency() * thread_count_mult * affinity_count_mult, 4.0)));
	vector<jthread> threads = {};
	vector<vector<pair<int64_t, int64_t>>> failed_tests = {};
	vector<uint64_t> num_failed_tests = {};

	failed_tests.reserve(thread_count);
	num_failed_tests.reserve(thread_count);

	for (unsigned int count = 0; count < thread_count; count++)
	{
		failed_tests.push_back(vector<pair<int64_t, int64_t>>(vector<pair<int64_t, int64_t>>()));
		num_failed_tests.push_back(0);
		threads.push_back(jthread(single_test, startA + count, thread_count, &(*failed_tests.rbegin()), &(*num_failed_tests.rbegin())));
	}

	for (auto& iter : threads)
	{
		iter.join();
	}

	uint64_t total_failed_tests = 0;

	for (auto& iter : num_failed_tests)
	{
		total_failed_tests += iter;
	}

	const auto stop = chrono::high_resolution_clock::now();
	const auto duration = chrono::duration_cast<chrono::milliseconds>(stop - start);

	cout << "Tests finished. Took: " << duration.count() / 1000 << "." << format("{:03}", duration.count() % 1000) << "s." << endl;
	cout << "\nTotal tests done: " << (static_cast<uint64_t>(stopA) - startA) * (static_cast<uint64_t>(stopB) - startB) << endl;
	cout << "Total failed tests: " << total_failed_tests << endl;

	if (total_failed_tests > 0)
	{
		cout << "\nErrors encountered:\n";
		uint64_t num_errors_reported = max_reported_errors;
		for (auto outer_iter = failed_tests.begin(); outer_iter != failed_tests.end() && num_errors_reported > 0; outer_iter++)
		{
			for (auto inner_iter = outer_iter->begin(); inner_iter != outer_iter->end() && num_errors_reported > 0; inner_iter++, num_errors_reported--)
			{
				const int64_t numA = inner_iter->first;
				const int64_t numB = inner_iter->second;

				const int64_t result = numA % numB;

				LargeInt largeIntA = LargeInt(numA);
				LargeInt largeIntB = LargeInt(numB);

				LargeInt largeIntResult = largeIntA % largeIntB;

				cout << "Expected: " << numA << " % " << numB << " = " << result
					<< ", Got: " << largeIntA << " % " << largeIntB << " = " << largeIntResult << endl;
			}
		}
	}
}

void self_test_bitwise()
{
	using namespace std;

	cout << "\nRunning bitwise self test. This may take a while...\n";

	set_process_affinity();

	constexpr int32_t startA = INT16_MIN;
	constexpr int32_t stopA = INT16_MAX + 1;
	constexpr int32_t startB = INT16_MIN;
	constexpr int32_t stopB = INT16_MAX + 1;

	const auto start = chrono::high_resolution_clock::now();

	auto single_test = [](int32_t start, uint32_t step_size, vector<pair<int64_t, int64_t>>* failed_tests, uint64_t* num_failed_tests)
	{
		for (int32_t a = start; a < stopA; a += step_size)
		{
			for (int32_t b = startB; b < stopB; b++)
			{
				int64_t numA = static_cast<int64_t>(a);
				int64_t numB = static_cast<int64_t>(b);

				const int64_t resultA = numA & numB;
				const int64_t resultB = numA | numB;
				const int64_t resultC = numA ^ numB;
				const int64_t resultD = ~numA;

				LargeInt largeIntA = LargeInt(numA);
				LargeInt largeIntB = LargeInt(numB);

				LargeInt largeIntResultA = largeIntA & largeIntB;
				LargeInt largeIntResultB = largeIntA | largeIntB;
				LargeInt largeIntResultC = largeIntA ^ largeIntB;
				LargeInt largeIntResultD = ~largeIntA;

				if (LargeInt(resultA) != largeIntResultA || LargeInt(resultB) != largeIntResultB
					|| LargeInt(resultC) != largeIntResultC || LargeInt(resultD) != largeIntResultD)
				{
					if (*num_failed_tests < max_reported_errors)
					{
						failed_tests->push_back(make_pair(numA, numB));
					}
					(*num_failed_tests)++;
				}
			}
		}
	};

	unsigned int thread_count = static_cast<unsigned int>(trunc(max(thread::hardware_concurrency() * thread_count_mult * affinity_count_mult, 4.0)));
	vector<jthread> threads = {};
	vector<vector<pair<int64_t, int64_t>>> failed_tests = {};
	vector<uint64_t> num_failed_tests = {};

	failed_tests.reserve(thread_count);
	num_failed_tests.reserve(thread_count);

	for (unsigned int count = 0; count < thread_count; count++)
	{
		failed_tests.push_back(vector<pair<int64_t, int64_t>>(vector<pair<int64_t, int64_t>>()));
		num_failed_tests.push_back(0);
		threads.push_back(jthread(single_test, startA + count, thread_count, &(*failed_tests.rbegin()), &(*num_failed_tests.rbegin())));
	}

	for (auto& iter : threads)
	{
		iter.join();
	}

	uint64_t total_failed_tests = 0;

	for (auto& iter : num_failed_tests)
	{
		total_failed_tests += iter;
	}

	const auto stop = chrono::high_resolution_clock::now();
	const auto duration = chrono::duration_cast<chrono::milliseconds>(stop - start);

	cout << "Tests finished. Took: " << duration.count() / 1000 << "." << format("{:03}", duration.count() % 1000) << "s." << endl;
	cout << "\nTotal tests done: " << (static_cast<uint64_t>(stopA) - startA) * (static_cast<uint64_t>(stopB) - startB) << endl;
	cout << "Total failed tests: " << total_failed_tests << endl;

	if (total_failed_tests > 0)
	{
		cout << "\nErrors encountered:\n";
		uint64_t num_errors_reported = max_reported_errors;
		for (auto outer_iter = failed_tests.begin(); outer_iter != failed_tests.end() && num_errors_reported > 0; outer_iter++)
		{
			for (auto inner_iter = outer_iter->begin(); inner_iter != outer_iter->end() && num_errors_reported > 0; inner_iter++, num_errors_reported--)
			{
				const int64_t numA = inner_iter->first;
				const int64_t numB = inner_iter->second;

				const int64_t resultA = numA & numB;
				const int64_t resultB = numA | numB;
				const int64_t resultC = numA ^ numB;
				const int64_t resultD = ~numA;

				LargeInt largeIntA = LargeInt(numA);
				LargeInt largeIntB = LargeInt(numB);

				LargeInt largeIntResultA = largeIntA & largeIntB;
				LargeInt largeIntResultB = largeIntA | largeIntB;
				LargeInt largeIntResultC = largeIntA ^ largeIntB;
				LargeInt largeIntResultD = ~largeIntA;

				cout << "Expected: " << numA << " & " << numB << " = " << resultA
					<< ", " << numA << " | " << numB << " = " << resultB
					<< ", " << numA << " ^ " << numB << " = " << resultC
					<< ", ~" << numA << " = " << resultD
					<< ", Got: " << largeIntA << " & " << largeIntB << " = " << largeIntResultA
					<< ", " << largeIntA << " | " << largeIntB << " = " << largeIntResultB
					<< ", " << largeIntA << " ^ " << largeIntB << " = " << largeIntResultC
					<< ", ~" << largeIntA << " = " << largeIntResultD << endl;
			}
		}
	}
}

void self_test_bitshift()
{
	using namespace std;

	cout << "\nRunning bit shift self test. This may take a while...\n";

	set_process_affinity();

	constexpr int32_t startA = INT32_MIN >> 5;
	constexpr int32_t stopA = INT32_MAX >> 5;
	constexpr int8_t startB = 0;
	constexpr int8_t stopB = 32;

	const auto start = chrono::high_resolution_clock::now();

	auto single_test = [](int32_t start, uint32_t step_size, vector<pair<int64_t, int64_t>>* failed_tests, uint64_t* num_failed_tests)
	{
		for (int32_t a = start; a < stopA; a += step_size)
		{
			for (int8_t b = startB; b < stopB; b++)
			{
				int64_t numA = static_cast<int64_t>(a);
				int64_t numB = static_cast<int64_t>(b);

				const int64_t resultA = numA << numB;
				const int64_t resultB = numA >> numB;

				LargeInt largeIntA = LargeInt(numA);

				LargeInt largeIntResultA = largeIntA << numB;
				LargeInt largeIntResultB = largeIntA >> numB;

				if (LargeInt(resultA) != largeIntResultA || LargeInt(resultB) != largeIntResultB)
				{
					if (*num_failed_tests < max_reported_errors)
					{
						failed_tests->push_back(make_pair(numA, numB));
					}
					(*num_failed_tests)++;
				}
			}
		}
	};

	unsigned int thread_count = static_cast<unsigned int>(trunc(max(thread::hardware_concurrency() * thread_count_mult * affinity_count_mult, 4.0)));
	vector<jthread> threads = {};
	vector<vector<pair<int64_t, int64_t>>> failed_tests = {};
	vector<uint64_t> num_failed_tests = {};

	failed_tests.reserve(thread_count);
	num_failed_tests.reserve(thread_count);

	for (unsigned int count = 0; count < thread_count; count++)
	{
		failed_tests.push_back(vector<pair<int64_t, int64_t>>(vector<pair<int64_t, int64_t>>()));
		num_failed_tests.push_back(0);
		threads.push_back(jthread(single_test, startA + count, thread_count, &(*failed_tests.rbegin()), &(*num_failed_tests.rbegin())));
	}

	for (auto& iter : threads)
	{
		iter.join();
	}

	uint64_t total_failed_tests = 0;

	for (auto& iter : num_failed_tests)
	{
		total_failed_tests += iter;
	}

	const auto stop = chrono::high_resolution_clock::now();
	const auto duration = chrono::duration_cast<chrono::milliseconds>(stop - start);

	cout << "Tests finished. Took: " << duration.count() / 1000 << "." << format("{:03}", duration.count() % 1000) << "s." << endl;
	cout << "\nTotal tests done: " << (static_cast<uint64_t>(stopA) - startA) * (static_cast<uint64_t>(stopB) - startB) << endl;
	cout << "Total failed tests: " << total_failed_tests << endl;

	if (total_failed_tests > 0)
	{
		cout << "\nErrors encountered:\n";
		uint64_t num_errors_reported = max_reported_errors;
		for (auto outer_iter = failed_tests.begin(); outer_iter != failed_tests.end() && num_errors_reported > 0; outer_iter++)
		{
			for (auto inner_iter = outer_iter->begin(); inner_iter != outer_iter->end() && num_errors_reported > 0; inner_iter++, num_errors_reported--)
			{
				const int64_t numA = inner_iter->first;
				const int64_t numB = inner_iter->second;

				const int64_t resultA = numA << numB;
				const int64_t resultB = numA >> numB;

				LargeInt largeIntA = LargeInt(numA);

				LargeInt largeIntResultA = largeIntA << numB;
				LargeInt largeIntResultB = largeIntA >> numB;

				cout << "Expected: " << numA << " << " << numB << " = " << resultA
					<< ", " << numA << " >> " << numB << " = " << resultB
					<< ", Got: " << largeIntA << " << " << numB << " = " << largeIntResultA
					<< ", " << largeIntA << " >> " << numB << " = " << largeIntResultB << endl;
			}
		}
	}
}

void self_test_unary()
{
	using namespace std;

	cout << "\nRunning unary self test. This may take a while...\n";

	set_process_affinity();

	constexpr int64_t startNum = INT32_MIN;
	constexpr int64_t stopNum = INT32_MAX + 1ll;

	const auto start = chrono::high_resolution_clock::now();

	auto single_test = [](int64_t start, uint64_t step_size, vector<int64_t>* failed_tests, uint64_t* num_failed_tests)
	{
		for (int64_t num = start; num < stopNum; num += step_size)
		{
			int64_t resultA = num;
			resultA++;
			int64_t resultB = num;
			resultB--;

			LargeInt largeInt = LargeInt(num);

			LargeInt largeIntResultA = largeInt;
			largeIntResultA++;
			LargeInt largeIntResultB = largeInt;
			largeIntResultB--;

			if (LargeInt(resultA) != largeIntResultA || LargeInt(resultB) != largeIntResultB)
			{
				if (*num_failed_tests < max_reported_errors)
				{
					failed_tests->push_back(num);
				}
				(*num_failed_tests)++;
			}
		}
	};

	unsigned int thread_count = static_cast<unsigned int>(trunc(max(thread::hardware_concurrency() * thread_count_mult * affinity_count_mult, 4.0)));
	vector<jthread> threads = {};
	vector<vector<int64_t>> failed_tests = {};
	vector<uint64_t> num_failed_tests = {};

	failed_tests.reserve(thread_count);
	num_failed_tests.reserve(thread_count);

	for (unsigned int count = 0; count < thread_count; count++)
	{
		failed_tests.push_back(vector<int64_t>(vector<int64_t>()));
		num_failed_tests.push_back(0);
		threads.push_back(jthread(single_test, startNum + count, thread_count, &(*failed_tests.rbegin()), &(*num_failed_tests.rbegin())));
	}

	for (auto& iter : threads)
	{
		iter.join();
	}

	uint64_t total_failed_tests = 0;

	for (auto& iter : num_failed_tests)
	{
		total_failed_tests += iter;
	}

	const auto stop = chrono::high_resolution_clock::now();
	const auto duration = chrono::duration_cast<chrono::milliseconds>(stop - start);

	cout << "Tests finished. Took: " << duration.count() / 1000 << "." << format("{:03}", duration.count() % 1000) << "s." << endl;
	cout << "\nTotal tests done: " << static_cast<int64_t>(stopNum) - startNum << endl;
	cout << "Total failed tests: " << total_failed_tests << endl;

	if (total_failed_tests > 0)
	{
		cout << "\nErrors encountered:\n";
		uint64_t num_errors_reported = max_reported_errors;
		for (auto outer_iter = failed_tests.begin(); outer_iter != failed_tests.end() && num_errors_reported > 0; outer_iter++)
		{
			for (auto inner_iter = outer_iter->begin(); inner_iter != outer_iter->end() && num_errors_reported > 0; inner_iter++, num_errors_reported--)
			{
				int64_t resultAdd = *inner_iter;
				resultAdd++;
				int64_t resultSub = *inner_iter;
				resultSub--;

				LargeInt largeInt = LargeInt(*inner_iter);
				LargeInt largeIntResultAdd = largeInt;
				largeIntResultAdd++;
				LargeInt largeIntResultSub = largeInt;
				largeIntResultSub--;

				cout << "Expected: " << *inner_iter << "++ = " << resultAdd << "; "
					<< *inner_iter << "-- = " << resultSub
					<< ", Got: " << largeInt << "++ = " << largeIntResultAdd
					<< "; " << largeInt << "-- = " << largeIntResultSub << endl;
			}
		}
	}
}
