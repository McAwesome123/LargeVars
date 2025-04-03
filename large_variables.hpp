#pragma once

#include <climits>
#include <cmath>
#include <compare>
#include <cstdint>
#include <format>
#include <iosfwd>
#include <limits>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

// using namespace std;
// :3c

// An arbitrarily sized integer value.
// Theoretically can be as big as your memory allows, unless specifying a max size that is less than that.
// The value is ALWAYS treated as if it's signed. Thus, size 1 is limited to -128 - +127; size 2 is limited to -32768 - +32767; etc.
// When max size is specified, any bytes beyond the max size are truncated. This can result in overflow or underflow.
// When using binary operators, the number returned will have the max size of the left-hand side.
// Assignment operator overwrites max size. To preserve it, use copy_value().
class LargeInt
{
public:
	// Throwable class for when you forget to add an 'if (num == 0) { dont(); }'
	class div_by_zero : public std::logic_error
	{
	public:
		div_by_zero(const std::string& what_arg) : logic_error(what_arg)
		{}

		div_by_zero(const char* what_arg) : logic_error(what_arg)
		{}

		div_by_zero(const div_by_zero& other) = default;
	};

	// Throwable class for when an invalid float is given to the constructor (i.e nan or inf)
	class invalid_float_conversion : public std::logic_error
	{
	public:
		invalid_float_conversion(const std::string& what_arg) : logic_error(what_arg)
		{}

		invalid_float_conversion(const char* what_arg) : logic_error(what_arg)
		{}

		invalid_float_conversion(const invalid_float_conversion& other) = default;
	};

	// Default constructor that initializes the class with a value of 0.
	LargeInt() : value(0), size(1), max_size(0)
	{}

	// Almost a copy constructor except it doesn't copy max size.
	// If the number would take up more bytes than the max size, excess bytes are truncated.
	LargeInt(const LargeInt& other, size_t max_size) : value(other.value), size(other.size), max_size(max_size)
	{}

	// Constructor for using a byte vector (since that's basically what the class is).
	explicit LargeInt(const std::vector<uint8_t>& other) : value(other), size(other.size()), max_size(0)
	{}

	// Constructor for using a byte vector (since that's basically what the class is).
	// If the number would take up more bytes than the max size, excess bytes are truncated.
	LargeInt(const std::vector<uint8_t>&other, size_t max_size) : value(other), size(other.size()), max_size(max_size)
	{
		trim_size();
	}

	// me when LargeInt num = true;
	explicit LargeInt(bool val) : value({ static_cast<uint8_t>(val) }), size(sizeof(uint8_t)), max_size(0)
	{}

	// Conversion constructor for integer types that aren't a boolean.
	template<typename Integer, std::enable_if_t<std::is_integral<Integer>::value, bool> = true, std::enable_if_t<!std::is_same<Integer, bool>::value, bool> = true>
	LargeInt(Integer val) : LargeInt(val, 0)
	{}

	// Conversion constructor for floating point types.
	// Throws invalid_float_conversion if the float given is inf or NaN.
	template<typename FloatingPoint, std::enable_if_t<std::is_floating_point<FloatingPoint>::value, bool> = true>
	explicit LargeInt(FloatingPoint val) : LargeInt(val, 0)
	{}

	// me when LargeInt(true, n);
	LargeInt(bool val, size_t max_size) : value({ static_cast<uint8_t>(val) }), size(sizeof(uint8_t)), max_size(max_size)
	{
		// paranoia
		trim_size();
	}

	// Constructor for integer types that aren't a boolean.
	// If the number would take up more bytes than the max size, excess bytes are truncated.
	template<typename Integer, std::enable_if_t<std::is_integral<Integer>::value, bool> = true, std::enable_if_t<!std::is_same<Integer, bool>::value, bool> = true>
	LargeInt(Integer val, size_t max_size) : size(0), max_size(max_size)
	{
		const bool is_val_negative = val < 0;

		if (val == 0)
		{
			value.push_back(0);
			size++;
		}

		if constexpr (std::numeric_limits<Integer>::is_signed)
		{
			while (!too_large(size + 1) && val != 0 && val != -1)
			{
				const uint8_t num = static_cast<uint8_t>(val);
				value.push_back(num);
				val >>= byte_bits;
				size++;
			}
			if (val == -1)
			{
				value.push_back(UINT8_MAX);
				size++;
			}
		}
		else
		{
			// i blame gcc
			while (!too_large(size + 1) && val != 0)
			{
				const uint8_t num = static_cast<uint8_t>(val);
				value.push_back(num);
				val >>= byte_bits;
				size++;
			}
		}

		if (is_negative() && !is_val_negative)
		{
			value.push_back(0);
			size++;
		}

		trim_size();
	}

	// Constructor for floating point types.
	// Throws invalid_float_conversion if the float given is inf or NaN.
	// If the number would take up more bytes than the max size, excess bytes are truncated.
	template<typename FloatingPoint, std::enable_if_t<std::is_floating_point<FloatingPoint>::value, bool> = true>
	LargeInt(FloatingPoint val, size_t max_size) : size(0), max_size(max_size)
	{
		if (std::isinf(val))
		{
			throw invalid_float_conversion("Cannot convert infinity to LargeInt.");
		}
		else if (std::isnan(val))
		{
			throw invalid_float_conversion("Cannot convert NaN to LargeInt.");
		}

		val = trunc(val);
		const bool is_val_negative = val < 0;

		if (val == 0)
		{
			value.push_back(0);
			size++;
		}

		val = std::abs(val);

		while (!too_large(size + 1) && val != 0)
		{
			// fmod seems to give better results than casting to byte directly
			const uint8_t num = static_cast<uint8_t>(fmod(val, static_cast<FloatingPoint>(pow(2, byte_bits))));
			value.push_back(num);
			val = trunc(val / static_cast<FloatingPoint>(pow(2, byte_bits)));
			size++;
		}

		if (is_negative())
		{
			value.push_back(0);
			size++;
		}

		if (is_val_negative)
		{
			*this = -*this;
		}

		trim_size();
	}

	// Changes the maximum size of the value and truncates if it's too large.
	void change_max_size(size_t new_size)
	{
		max_size = new_size;
		trim_size();
	}

	// +x is the same as x so this just returns a copy of the value.
	LargeInt operator+() const
	{
		return *this;
	}

	// Creates a copy where the value is negated (based on the two's complement) and returns it.
	LargeInt operator-() const
	{
		LargeInt new_val = *this;

		bool carry = true;
		for (auto& iter : new_val.value)
		{
			iter ^= UINT8_MAX;

			if (carry)
			{
				const uint8_t current = iter;
				iter++;
				carry = iter < current;
			}
		}

		if (new_val.is_negative() && is_negative())
		{
			new_val.value.push_back(0);
			new_val.size++;
		}

		new_val.trim_size();
		return new_val;
	}

	// Copies the value without changing the max size.
	LargeInt &copy_value(const LargeInt &other)
	{
		value = other.value;

		trim_size();

		return *this;
	}

	// Adds two numbers.
	LargeInt operator+(LargeInt other) const
	{
		LargeInt new_val = *this;

		// If the sign of the two numbers is different, i.e we're effectively subtracting two numbers,
		// temporarily change the result's max size so that the carry bit is correctly discarded.
		if (new_val.is_negative() != other.is_negative())
		{
			new_val.change_max_size(std::max(size, other.size));
		}

		// If either number is negative, expand it to be the same size as the other
		// so the operation is carried out correctly without a bunch of extra checks
		while (new_val.is_negative() && new_val.size < other.size)
		{
			new_val.value.push_back(UINT8_MAX);
			new_val.size++;
		}

		while (other.is_negative() && other.size < new_val.size)
		{
			other.value.push_back(UINT8_MAX);
			other.size++;
		}

		size_t index = 0;
		bool carry = false;

		for (; (index == 0 || !too_large(index - 1)) && index < other.value.size(); index++)
		{
			// Expand the value so we don't index out of bounds
			if (index >= new_val.value.size())
			{
				new_val.value.push_back(0);
				new_val.size++;
			}

			// Since unsigned integer overflow is well defined, we should be able
			// to detect it by checking if the value *decreased* after adding to it.
			// If an overflow was detected, set a carry flag so we add 1 to the next byte.
			uint8_t current = new_val.value[index];
			if (carry)
			{
				new_val.value[index]++;
				carry = (new_val.value[index] < current);
				current = new_val.value[index];
			}
			new_val.value[index] += other.value[index];
			carry = carry || new_val.value[index] < current;
		}

		// Make sure the carry is processed even after we're done adding.
		for (; (index == 0 || !too_large(index - 1)) && carry; index++)
		{
			if (index >= new_val.value.size())
			{
				// Negative numbers work differently. Need to not infinitely add 1 to 255.
				if (is_negative() && other.is_negative())
				{
					new_val.value.push_back(UINT8_MAX);
					new_val.size++;
					carry = false;
					break;
				}
				else
				{
					new_val.value.push_back(0);
					new_val.size++;
				}
			}

			const uint8_t current = new_val.value[index];
			new_val.value[index]++;
			carry = (new_val.value[index] < current);
		}

		// Make sure the resulting value has a sign that makes sense.
		if (!is_negative() && !other.is_negative() && new_val.is_negative())
		{
			new_val.value.push_back(0);
			new_val.size++;
		}
		else if (is_negative() && other.is_negative() && !new_val.is_negative())
		{
			for (size_t i = new_val.value.size() - 1; i != SIZE_MAX; i--)
			{
				new_val.value[i] ^= UINT8_MAX;
			}

			carry = true;

			for (size_t i = 0; i < new_val.value.size() && carry; i++)
			{
				const uint8_t current = new_val.value[i];
				new_val.value[i]++;
				carry = (new_val.value[i] < current);
			}
		}

		new_val.trim_size();

		// Revert the max size change when subtracting numbers
		if (is_negative() != other.is_negative())
		{
			new_val.change_max_size(max_size);
			new_val.trim_size();
		}

		return new_val;
	}

	LargeInt& operator+=(const LargeInt& other)
	{
		*this = *this + other;
		return *this;
	}

	// Subtracts two numbers.
	LargeInt operator-(LargeInt other) const
	{
		// Save some stuff that we're about to change
		const size_t other_max_size = other.max_size;
		const bool other_was_negative = other.is_negative();
		other.change_max_size(other.size);

		// Negate the other number
		for (size_t i = other.value.size() - 1; i != SIZE_MAX; i--)
		{
			other.value[i] ^= UINT8_MAX;
		}

		bool carry = true;

		for (size_t i = 0; i < other.value.size() && carry; i++)
		{
			const uint8_t current = other.value[i];
			other.value[i]++;
			carry = (other.value[i] < current);
		}

		other.trim_size();

		// Revert the max size change
		other.change_max_size(other_max_size);

		// Expand the number if we happened to get the exact same value (like trying to negate -128)
		if (other_was_negative && other.is_negative())
		{
			other.value.push_back(0);
		}

		other.trim_size();

		// Now just add the two numbers together
		return *this + other;
	}

	LargeInt& operator-=(const LargeInt& other)
	{
		*this = *this - other;
		return *this;
	}

	// Multiplies two numbers.
	LargeInt operator*(const LargeInt& other) const
	{
		LargeInt new_val(0, max_size);

		// Expand the new value so we don't index out of bounds
		while (new_val.size < size + other.size)
		{
			new_val.value.push_back(0);
			new_val.size++;
		}

		// Take the absolute values (because negative numbers will probably fuck shit up)
		LargeInt abs_val = LargeInt(*this, 0).abs();
		LargeInt abs_other = LargeInt(other, 0).abs();

		// Sequentially multiply each set of two bytes together,
		// adding the result to the new value, bit shifted to be in the correct place.
		for (size_t right = 0; right < other.size; right++)
		{
			for (size_t left = 0; left < size; left++)
			{
				LargeInt result = static_cast<uint16_t>(abs_val.value[left]) * abs_other.value[right];
				result <<= ((left + right) * byte_bits);
				new_val += result;
			}
		}

		// Sort out negatives
		if (is_negative() != other.is_negative())
		{
			new_val = -new_val;
		}

		new_val.trim_size();
		return new_val;
	}

	LargeInt& operator*=(const LargeInt& other)
	{
		*this = *this * other;
		return *this;
	}

	// Divides two numbers
	LargeInt operator/(const LargeInt& other) const
	{
		if (other == 0)
		{
			// oopsies :3
			throw div_by_zero("LargeInt division by zero.");
		}
		else if (other == 1)
		{
			return *this;
		}
		else if (other == -1)
		{
			return -(*this);
		}

		// For the same reasons as multiplication, we take the absolutes
		const bool negative_result = (is_negative() != other.is_negative());
		LargeInt result(0, max_size);
		LargeInt left = LargeInt(*this, 0).abs();
		LargeInt right = LargeInt(other, 0).abs();

		// Since this is based on subtracting, this should make it more efficient
		size_t num_shifts = 0;
		while (right < left)
		{
			right <<= byte_bits;
			num_shifts++;
		}

		// Essentially, x / y is calculated from the amount of times
		// we can subtract y from x without getting a negative result.
		// Of course, we need to keep in mind that we're bit shifting y.
		while (num_shifts != SIZE_MAX)
		{
			while (left >= right)
			{
				left -= right;
				result += LargeInt(1) << (num_shifts * byte_bits);
			}

			right >>= byte_bits;
			num_shifts--;
		}

		result.recalculate_size();

		return (negative_result ? -result : result);
	}

	LargeInt& operator/=(const LargeInt& other)
	{
		*this = *this / other;
		return *this;
	}

	// Modulos two numbers
	LargeInt operator%(const LargeInt& other) const
	{
		if (other == 0)
		{
			// oopsies :3
			throw div_by_zero("LargeInt division by zero.");
		}
		else if (other == 1 || other == -1)
		{
			return LargeInt(0, max_size);
		}

		// For the same reasons as multiplication and division, we take the absolutes.
		LargeInt left = LargeInt(*this, 0).abs();
		LargeInt right = LargeInt(other, 0).abs();

		size_t num_shifts = 0;
		while (right < left)
		{
			right <<= byte_bits;
			num_shifts++;
		}

		// This works on the same basis as division,
		// except we don't keep track of how many times we subtract.
		while (num_shifts != SIZE_MAX)
		{
			while (left >= right)
			{
				left -= right;
			}

			right >>= byte_bits;
			num_shifts--;
		}

		left.recalculate_size();

		return (is_negative() ? -left : left);
	}

	LargeInt& operator%=(const LargeInt& other)
	{
		*this = *this % other;
		return *this;
	}

	// Does a bitwise and operation between two numbers.
	LargeInt operator&(const LargeInt& other) const
	{
		LargeInt new_val = *this;
		const bool val_is_negative = new_val.is_negative();

		for (size_t i = 0; (new_val.max_size == 0 || i < new_val.max_size) && (i < new_val.size || i < other.size); i++)
		{
			// If in bounds, don't need to do anything special.
			if (i < new_val.size && i < other.size)
			{
				new_val.value[i] &= other.value[i];
			}
			// If the other number is smaller and positive, gotta set the rest of this to 0.
			// Negative is ignored because any byte & 255 doesn't change.
			else if (i < new_val.size && !other.is_negative())
			{
				new_val.value[i] = 0;
			}
			// If this number is smaller and negative, add the rest of the other number's bytes.
			else if (i < other.size && val_is_negative)
			{
				new_val.value.push_back(other.value[i]);
			}
			else
			{
				break;
			}
		}

		new_val.recalculate_size();

		return new_val;
	}

	LargeInt& operator&=(const LargeInt& other)
	{
		*this = *this & other;
		return *this;
	}

	// Does a bitwise or operation between two numbers.
	LargeInt operator|(const LargeInt& other) const
	{
		LargeInt new_val = *this;
		const bool val_is_negative = new_val.is_negative();

		for (size_t i = 0; (new_val.max_size == 0 || i < new_val.max_size) && (i < new_val.size || i < other.size); i++)
		{
			// If within bounds, do nothing special.
			if (i < new_val.size && i < other.size)
			{
				new_val.value[i] |= other.value[i];
			}
			// If the other number is smaller and negative,
			// set the rest of the bytes to 255. They'll be trimmed later.
			else if (i < new_val.size && other.is_negative())
			{
				new_val.value[i] = UINT8_MAX;
			}
			// If this nubmer is smaller and not negative, add the rest of the other number's bytes.
			// If this number is negative, then it's ignored because adding extra 255 bytes doesn't change the value.
			else if (i < other.size && !val_is_negative)
			{
				new_val.value.push_back(other.value[i]);
			}
			else
			{
				break;
			}
		}

		new_val.trim_size();

		return new_val;
	}

	LargeInt& operator|=(const LargeInt& other)
	{
		*this = *this | other;
		return *this;
	}

	// Does a bitwise xor operation between two numbers.
	LargeInt operator^(const LargeInt& other) const
	{
		LargeInt new_val = *this;
		const bool val_is_negative = new_val.is_negative();

		for (size_t i = 0; (new_val.max_size == 0 || i < new_val.max_size) && (i < other.size || i < new_val.size); i++)
		{
			// If within bounds, do nothing special.
			if (i < new_val.size && i < other.size)
			{
				new_val.value[i] ^= other.value[i];
			}
			// If the other number is smaller and negative, invert this number's bits.
			else if (i < new_val.size && other.is_negative())
			{
				new_val.value[i] = ~new_val.value[i];
			}
			// If this number is smaller and negative, add the other number's bits inverted.
			else if (i < other.size && val_is_negative)
			{
				new_val.value.push_back(~other.value[i]);
				new_val.size++;
			}
			// If this number is smaller and positive, add the other number's bits
			else if (i < other.size)
			{
				new_val.value.push_back(other.value[i]);
				new_val.size++;
			}
			else
			{
				break;
			}
		}

		new_val.trim_size();

		return new_val;
	}

	LargeInt& operator^=(const LargeInt& other)
	{
		*this = *this ^ other;
		return *this;
	}

	// Left shifts the number by the specified amount of bits
	template<typename Integer, std::enable_if_t<std::is_integral<Integer>::value, bool> = true>
	LargeInt operator<<(const Integer& other) const
	{
		// If a negative number was given, do the opposite bit shift operation.
		if constexpr (std::is_signed<Integer>::value)
		{
			if (other < 0)
			{
				return *this >> -other;
			}
		}

		size_t other_size = static_cast<size_t>(other);
		LargeInt new_val = *this;
		const bool val_is_negative = new_val.is_negative();

		// If the number of shifts is larger than the max amount of bits, return a value of 0.
		if (new_val.max_size != 0 && other_size > new_val.max_size * byte_bits)
		{
			new_val.value.clear();
			new_val.value.push_back(0);
			return new_val;
		}

		// For multiples of 8 bits, simply add another byte to the beginning.
		for (; other_size >= byte_bits; other_size -= byte_bits)
		{
			new_val.value.insert(new_val.value.begin(), 0);
			new_val.size++;
		}

		for (; other_size > 0; other_size--)
		{
			// If the number isn't negative and we would shift a 1 from bit 7 to bit 8, expand the value to keep it positive.
			if (!val_is_negative && (new_val.value[new_val.value.size() - 1] & (1 << (byte_bits - 2))) != 0)
			{
				new_val.value.push_back(0);
				new_val.size++;
			}
			// If the number is negative and we would shift a 0 from bit 7 to bit 8, expand the value to keep it negative.
			else if (val_is_negative && (new_val.value[new_val.value.size() - 1] & (1 << (byte_bits - 2))) == 0)
			{
				new_val.value.push_back(UINT8_MAX);
				new_val.size++;
			}

			new_val.value[new_val.value.size() - 1] <<= 1;

			// Shift the rest of the bits
			for (size_t i = new_val.value.size() - 2; i != SIZE_MAX; i--)
			{
				const bool carry = ((new_val.value[i] & (1 << (byte_bits - 1))) != 0);
				if (carry)
				{
					new_val.value[i + 1] += 1;
				}
				new_val.value[i] <<= 1;
			}
		}

		new_val.trim_size();

		return new_val;
	}

	template<typename Integer, std::enable_if_t<std::is_integral<Integer>::value, bool> = true>
	LargeInt& operator<<=(Integer other)
	{
		*this = *this << other;

		return *this;
	}

	// Right shifts the number by the specified amount of bits
	template<typename Integer, std::enable_if_t<std::is_integral<Integer>::value, bool> = true>
	LargeInt operator>>(Integer other) const
	{
		// If a negative number was given, do the opposite bit shift operation.
		if constexpr (std::is_signed<Integer>::value)
		{
			if (other < 0)
			{
				return *this >> -other;
			}
		}

		size_t other_size = static_cast<size_t>(other);
		LargeInt new_val = *this;
		const bool val_is_negative = new_val.is_negative();

		// If the number of shifts is larger than the amount of bits,
		// return a value of 0 if positive or -1 if negative.
		if (other_size >= new_val.size * byte_bits)
		{
			if (val_is_negative)
			{
				new_val.value.clear();
				new_val.value.push_back(UINT8_MAX);
			}
			else
			{
				new_val.value.clear();
				new_val.value.push_back(0);
			}
			new_val.size = 1;
			return new_val;
		}

		// For multiples of 8 bits, simply remove a byte from the beginning.
		for (; other_size >= byte_bits; other_size -= byte_bits)
		{
			new_val.value.erase(new_val.value.begin());
			new_val.size--;
		}

		for (; other_size > 0; other_size--)
		{
			new_val.value[0] >>= 1;

			for (size_t i = 1; i < new_val.value.size(); i++)
			{
				if ((new_val.value[i] & 1) != 0)
				{
					new_val.value[i - 1] += (1 << (byte_bits - 1));
				}
				new_val.value[i] >>= 1;
			}

			// If the value is negative, make sure it remains negative.
			if (val_is_negative)
			{
				new_val.value[new_val.value.size() - 1] |= (1 << (byte_bits - 1));
			}
		}

		new_val.recalculate_size();

		return new_val;
	}

	template<typename Integer, std::enable_if_t<std::is_integral<Integer>::value, bool> = true>
	LargeInt& operator>>=(Integer other)
	{
		*this = *this >> other;

		return *this;
	}

	// Increment by 1
	LargeInt &operator++()
	{
		LargeInt new_val = *this;
		bool carry = true;

		// Add 1 to the first byte and check for overflow. If it overflowed, repeat with the next byte.
		for (auto iter = new_val.value.begin(); iter != new_val.value.end() && carry; iter++)
		{
			const uint8_t current = *iter;
			(*iter)++;
			carry = (*iter < current);
		}

		// If the number became negative, expand it so it's positive again.
		if (!is_negative() && new_val.is_negative())
		{
			new_val.value.push_back(0);
			new_val.size++;
		}

		new_val.trim_size();

		*this = new_val;
		return *this;
	}

	// :lksix:
	LargeInt operator++(int)
	{
		LargeInt old_val = *this;
		operator++();
		return old_val;
	}

	// Decrement by 1
	LargeInt &operator--()
	{
		LargeInt new_val = *this;
		bool carry = true;

		// Subtract 1 from the first byte and check for underflow. If it underflowed, repeat with the next byte.
		for (auto iter = new_val.value.begin(); iter != new_val.value.end() && carry; iter++)
		{
			const uint8_t current = *iter;
			(*iter)--;
			carry = (*iter > current);
		}

		// If the number became positive, expand it so it's negative again.
		if (is_negative() && !new_val.is_negative())
		{
			new_val.value.push_back(UINT8_MAX);
			new_val.size++;
		}

		new_val.trim_size();

		*this = new_val;
		return *this;
	}

	// :lksix:
	LargeInt operator--(int)
	{
		LargeInt old_val = *this;
		operator--();
		return old_val;
	}

	// Bitwise negates the number.
	LargeInt operator~() const
	{
		LargeInt new_val = *this;

		for (auto& iter : new_val.value)
		{
			iter = ~iter;
		}

		return new_val;
	}

	// Compares two numbers and returns the relevant ordering constant.
	// Uses weak ordering as substitutability is not guaranteed due to max size possibly differing.
	// Use is_exactly_equal() if you want to guarantee substitutability.
	std::weak_ordering operator<=>(const LargeInt& other) const noexcept
	{
		if (!is_negative() && other.is_negative())
		{
			return std::weak_ordering::greater;
		}
		else if (is_negative() && !other.is_negative())
		{
			return std::weak_ordering::less;
		}

		if (size > other.size)
		{
			return std::weak_ordering::greater;
		}
		else if (size < other.size)
		{
			return std::weak_ordering::less;
		}

		for (size_t i = value.size() - 1; i != SIZE_MAX; i--)
		{
			if (value[i] > other.value[i])
			{
				return std::weak_ordering::greater;
			}
			else if (value[i] < other.value[i])
			{
				return std::weak_ordering::less;
			}
		}

		return std::weak_ordering::equivalent;
	}

	// Compares two numbers and returns true if they have the same value and false otherwise.
	// Does not guarantee substitutability as max size may be different.
	// Use is_exactly_equal() if you want to guarantee substitutability.
	bool operator==(const LargeInt& other) const noexcept
	{
		if (is_negative() != other.is_negative())
		{
			return false;
		}

		if (size != other.size)
		{
			return false;
		}

		for (size_t i = value.size() - 1; i != SIZE_MAX; i--)
		{
			if (value[i] != other.value[i])
			{
				return false;
			}
		}

		return true;
	}

	// Checks if the given size would be larger than the max size of the number.
	bool too_large(size_t new_size) const noexcept
	{
		return max_size != 0 && max_size < new_size;
	}

	// Get a copy of the value vector.
	std::vector<uint8_t> get_value() const noexcept
	{
		return value;
	}

	// Get the value's size.
	size_t get_size() const noexcept
	{
		return size;
	}

	// Get the value's maximum size.
	size_t get_max_size() const noexcept
	{
		return max_size;
	}

	// Get whether the value is negative (<0) or not.
	bool is_negative() const noexcept
	{
		return (value[value.size() - 1] & (1 << (byte_bits - 1))) != 0;
	}

	// Get the absolute value of the number.
	LargeInt abs() const
	{
		return (is_negative() ? -(*this) : *this);
	}

	// Returns true if the two numbers have an equal value AND max size.
	bool is_exactly_equal(const LargeInt& other) const noexcept
	{
		if (max_size != other.max_size)
		{
			return false;
		}

		return *this == other;
	}

	inline friend std::ostream &operator<<(std::ostream &out, const LargeInt &num);

	// Boolean cast operator
	explicit operator bool() const noexcept
	{
		return size > 1 || value[0] != 0;
	}

	// Integer cast operator
	template<typename Integer, std::enable_if_t<std::is_integral<Integer>::value, bool> = true>
	explicit operator Integer() const noexcept
	{
		Integer num = 0;

		for (size_t i = 0; i < sizeof(Integer) && (i < size || is_negative()); i++)
		{
			if (i < size)
			{
				num += value[i] << byte_bits * i;
			}
			else
			{
				num += UINT8_MAX << byte_bits * i;
			}
		}

		return num;
	}

	// Floating point cast operator
	template<typename FloatingPoint, std::enable_if_t<std::is_floating_point<FloatingPoint>::value, bool> = true>
	explicit operator FloatingPoint() const
	{
		FloatingPoint num = 0;

		const bool val_is_negative = is_negative();
		LargeInt abs_val = LargeInt(*this, 0).abs();

		if (abs_val.size > 2 && (abs_val.size - 2) * byte_bits > std::numeric_limits<FloatingPoint>::max_exponent)
		{
			return std::numeric_limits<FloatingPoint>::infinity() * (FloatingPoint)(val_is_negative ? -1.0 : 1.0);
		}

		for (size_t i = abs_val.size - 1; i != SIZE_MAX && num >= std::numeric_limits<FloatingPoint>::lowest() && num <= std::numeric_limits<FloatingPoint>::max(); i--)
		{
			if (abs_val.value[i] != 0)
			{
				num += static_cast<FloatingPoint>(abs_val.value[i] * pow(2, byte_bits * i));
			}
		}

		return num * static_cast<FloatingPoint>(val_is_negative ? -1.0 : 1.0);
	}

	// String cast operator
	// Converts the number to bcd and then appends every byte in hex to a string.
	explicit operator std::string() const
	{
		auto [bcd_num, sign] = this->convert_to_bcd();

		std::string output = (sign < 0 ? "-" : "") + std::format("{:x}", *bcd_num.value.rbegin());

		for (auto byte = bcd_num.value.rbegin() + 1; byte != bcd_num.value.rend(); byte++)
		{
			if (byte != bcd_num.value.rbegin())
			{
				// Make sure there's padding of 2 zeros because this doesn't give leading zeros.
				output += std::format("{:02x}", *byte);
			}
		}

		return output;
	}

protected:
	std::vector<uint8_t> value;
	size_t size;
	size_t max_size;

	const static uint8_t byte_bits = 8;

	// Trim the number if it's above the maximum size.
	void trim_size()
	{
		recalculate_size();

		if (!too_large(size))
		{
			return;
		}

		while (too_large(size))
		{
			value.pop_back();
			size--;
		}
	}

	// Trims unnecessary 0 and 255 bytes and recalculates the number's size.
	void recalculate_size()
	{
		if (value.size() < 1)
		{
			value.push_back(0);
		}

		// If there's a leading 0 byte and the following byte does not have bit 8 set to 1, trim the byte.
		// If there's a leading 255 byte and the following byte does not have bit 8 set to 0, trim the byte.
		// Neither of these should change the actual value of the number.
		while (value.size() > 1
			   && ((*value.rbegin() == 0 && (*(value.rbegin() + 1) & (1 << (byte_bits - 1))) == 0)
				   || (*value.rbegin() == UINT8_MAX && (*(value.rbegin() + 1) & (1 << (byte_bits - 1))) != 0)))
		{
			value.pop_back();
		}

		size = value.size();
	}

	// Converts the number to binary coded decimal and returns it
	// along with +1 or -1 to signify if it's negative or positive.
	// This is the only case where LargeInt is treated as unsigned.
	std::pair<LargeInt, int8_t> convert_to_bcd() const
	{
		const bool val_is_negative = this->is_negative();
		const LargeInt abs_val = LargeInt(*this, 0).abs();
		LargeInt new_val = 0;

		for (auto byte = abs_val.value.rbegin(); byte != abs_val.value.rend(); byte++)
		{
			#ifdef _MSC_VER
			#pragma warning(suppress : 6293)	// shut the fuck up :3
			#endif
			for (uint8_t bit = byte_bits - 1; bit != UINT8_MAX; bit--)
			{
				for (auto val = new_val.value.rbegin(); val != new_val.value.rend(); val++)
				{
					// If the top half of the byte is >4, add 3 to it.
					if ((*val >> 4) > 4)
					{
						*val += 3 << 4;

						// If it's the last byte, also expand the number.
						// Resets the iterator as push_back invalidates it.
						if (val == new_val.value.rbegin())
						{
							new_val.value.push_back(0);
							val = new_val.value.rbegin() + 1;
						}
					}

					// If the bottom half of the byte is >4, add 3 to it.
					if ((*val & (UINT8_MAX >> 4)) > 4)
					{
						*val += 3;
					}
				}

				// Shift the value to the left and add the next bit (from the left) from the original value.
				new_val <<= 1;
				if (*byte & (1 << bit))
				{
					new_val.value[0] += 1;
				}
			}
		}

		new_val.recalculate_size();

		// Unlike every other case, this number is ALWAYS UNSIGNED and as such leading 0s need to be removed.
		// Whether the number is positive or negative is returned as a +1 or -1 in a pair with the number.
		if (new_val.value[new_val.size - 1] == 0 && new_val.size > 1)
		{
			new_val.value.pop_back();
		}

		return std::make_pair(new_val, static_cast<int8_t>(val_is_negative ? -1 : +1));
	}
};

std::ostream &operator<<(std::ostream &out, const LargeInt &num)
{
	out << (std::string)num;

	return out;
}
