#include "I18n.h"
#include "Base/Algorithms.h"
#include "IO/FileSystem.h"

#include <stdarg.h>

#if defined(DEATH_TARGET_ANDROID)
#	include "Backends/Android/AndroidJniHelper.h"
#elif defined(DEATH_TARGET_APPLE)
#	include <CoreFoundation/CFPreferences.h>
#	include <CoreFoundation/CFPropertyList.h>
#	include <CoreFoundation/CFArray.h>
#	include <CoreFoundation/CFString.h>
#endif

#include <Environment.h>
#include <Utf8.h>
#include <Containers/GrowableArray.h>

using namespace Death;

namespace nCine
{
	static constexpr I18n::LanguageInfo SupportedLanguages[] = {
		{ "af", "Afrikaans"_s },
		{ "be", "БЕЛАРУСКАЯ"_s },
		{ "bg", "БЪЛГАРСКИ"_s },
		{ "cs", "Čeština"_s },
		{ "da", "Dansk"_s },
		{ "de", "Deutsch"_s },
		{ "en", "English"_s },
		{ "es", "Español"_s },
		{ "et", "Eesti"_s },
		{ "fi", "Suomi"_s },
		{ "fr", "Français"_s },
		{ "hr", "Hrvatski"_s },
		{ "hu", "Magyar"_s },
		{ "it", "Italiano"_s },
		{ "lt", "Lietuvių"_s },
		{ "lv", "Latviešu"_s },
		{ "mo", "ЛИМБА МОЛДОВЕНЯСКЭ"_s },
		{ "nl", "Nederlands"_s },
		{ "no", "Norsk"_s },
		{ "pl", "Polski"_s },
		{ "pt", "Português"_s },
		{ "ro", "Română"_s },
		{ "ru", "РУССКИЙ"_s },
		{ "sk", "Slovenčina"_s },
		{ "sl", "Slovenščina"_s },
		{ "sr", "Srpski"_s },
		{ "sv", "Svenska"_s },
		{ "tr", "Türkçe"_s },
		{ "uk", "УКРАЇНСЬКА"_s }
	};

	struct ValueToken : public I18n::ExpressionToken
	{
		ValueToken(const int value)
			: _value(value)
		{
		}

		int operator()(int n) const override
		{
			return _value;
		}

	protected:
		int _value;
	};

	struct VariableToken : public I18n::ExpressionToken
	{
		VariableToken()
		{
		}

		int operator()(int n) const override
		{
			return n;
		}
	};

	struct BinaryExpressionToken : public I18n::ExpressionToken
	{
		BinaryExpressionToken(ExpressionToken* left, ExpressionToken* right)
			:
			_left(left),
			_right(right)
		{
		}

		~BinaryExpressionToken()
		{
			delete _left;
			delete _right;
		}

	protected:
		I18n::ExpressionToken* _left;
		I18n::ExpressionToken* _right;
	};

	struct MultiplyToken : public BinaryExpressionToken
	{
		MultiplyToken(ExpressionToken* left, ExpressionToken* right)
			: BinaryExpressionToken(left, right)
		{
		}

		int operator()(int n) const override
		{
			return (*this->_left)(n) * (*this->_right)(n);
		}
	};

	struct DivideToken : public BinaryExpressionToken
	{
		DivideToken(ExpressionToken* left, ExpressionToken* right)
			: BinaryExpressionToken(left, right)
		{
		}

		int operator()(int n) const override
		{
			return (*this->_left)(n) / (*this->_right)(n);
		}
	};

	struct RemainderToken : public BinaryExpressionToken
	{
		RemainderToken(ExpressionToken* left, ExpressionToken* right)
			: BinaryExpressionToken(left, right)
		{
		}

		int operator()(int n) const override
		{
			return (*this->_left)(n) % (*this->_right)(n);
		}
	};

	struct AddToken : public BinaryExpressionToken
	{
		AddToken(ExpressionToken* left, ExpressionToken* right)
			: BinaryExpressionToken(left, right)
		{
		}

		int operator()(int n) const override
		{
			return (*this->_left)(n) + (*this->_right)(n);
		}
	};

	struct SubtractToken : public BinaryExpressionToken
	{
		SubtractToken(ExpressionToken* left, ExpressionToken* right)
			: BinaryExpressionToken(left, right)
		{
		}

		int operator()(int n) const override
		{
			return (*this->_left)(n) - (*this->_right)(n);
		}
	};

	struct LogicalAndToken : public BinaryExpressionToken
	{
		LogicalAndToken(ExpressionToken* left, ExpressionToken* right)
			: BinaryExpressionToken(left, right)
		{
		}

		int operator()(int n) const override
		{
			return (*this->_left)(n) != 0 && (*this->_right)(n) != 0;
		}
	};

	struct LogicalOrToken : public BinaryExpressionToken
	{
		LogicalOrToken(ExpressionToken* left, ExpressionToken* right)
			: BinaryExpressionToken(left, right)
		{
		}

		int operator()(int n) const override
		{
			return (*this->_left)(n) != 0 || (*this->_right)(n) != 0;
		}
	};

	struct CompareEqualsToken : public BinaryExpressionToken
	{
		CompareEqualsToken(ExpressionToken* left, ExpressionToken* right)
			: BinaryExpressionToken(left, right)
		{
		}

		int operator()(int n) const override
		{
			return (*this->_left)(n) == (*this->_right)(n);
		}
	};

	struct CompareNotEqualsToken : public BinaryExpressionToken
	{
		CompareNotEqualsToken(ExpressionToken* left, ExpressionToken* right)
			: BinaryExpressionToken(left, right)
		{
		}

		int operator()(int n) const override
		{
			return (*this->_left)(n) != (*this->_right)(n);
		}
	};

	struct CompareLessToken : public BinaryExpressionToken
	{
		CompareLessToken(ExpressionToken* left, ExpressionToken* right)
			: BinaryExpressionToken(left, right)
		{
		}

		int operator()(int n) const override
		{
			return (*this->_left)(n) < (*this->_right)(n);
		}
	};

	struct CompareEqualsOrLessToken : public BinaryExpressionToken
	{
		CompareEqualsOrLessToken(ExpressionToken* left, ExpressionToken* right)
			: BinaryExpressionToken(left, right)
		{
		}

		int operator()(int n) const override
		{
			return (*this->_left)(n) <= (*this->_right)(n);
		}
	};

	struct CompareEqualsOrGreaterToken : public BinaryExpressionToken
	{
		CompareEqualsOrGreaterToken(ExpressionToken* left, ExpressionToken* right)
			: BinaryExpressionToken(left, right)
		{
		}

		int operator()(int n) const override
		{
			return (*this->_left)(n) >= (*this->_right)(n);
		}
	};

	struct CompareGreaterToken : public BinaryExpressionToken
	{
		CompareGreaterToken(ExpressionToken* left, ExpressionToken* right)
			: BinaryExpressionToken(left, right)
		{
		}

		int operator()(int n) const override
		{
			return (*this->_left)(n) > (*this->_right)(n);
		}
	};

	struct TernaryExpressionToken : public I18n::ExpressionToken
	{
		TernaryExpressionToken(ExpressionToken* comparison, ExpressionToken* left, ExpressionToken* right)
			:
			_comparison(comparison),
			_left(left),
			_right(right)
		{
		}

		~TernaryExpressionToken()
		{
			delete _comparison;
			delete _left;
			delete _right;
		}

		int operator()(int n) const override
		{
			return (*this->_comparison)(n) ? (*this->_left)(n) : (*this->_right)(n);
		}

	protected:
		I18n::ExpressionToken* _comparison;
		I18n::ExpressionToken* _left;
		I18n::ExpressionToken* _right;
	};

	I18n::ExpressionToken* I18n::Evaluator::ParseNumber(const char*& s, bool negative)
	{
		char* endPtr;
		int value = strtol(s, &endPtr, 10);
		s = endPtr;

		if (negative) {
			value = -value;
		}

		return new ValueToken(value);
	}

	I18n::ExpressionToken* I18n::Evaluator::ParseAtom(const char*& s)
	{
		while (*s == ' ') {
			s++;
		}

		if (*s == '(') {
			s++;
			ExpressionToken* atom = ParseTernary(s);
			while (*s == ' ') {
				s++;
			}
			if (*s == ')') {
				s++;
				return atom;
			}
			// Unbalanced brackets
			return new ValueToken(0);
		} else if (*s == 'n') {
			s++;
			return new VariableToken();
		} else if (std::isdigit(*s)) {
			return ParseNumber(s, false);
		} else if (*s == '-' && std::isdigit(*(s + 1))) {
			s++;
			return ParseNumber(s, true);
		}

		// Unexpected character
		return new ValueToken(0);
	}

	I18n::ExpressionToken* I18n::Evaluator::ParseFactors(const char*& s)
	{
		I18n::ExpressionToken* left = ParseAtom(s);
		while (*s != '\0') {
			if (*s == '*') {
				s++;
				I18n::ExpressionToken* right = ParseAtom(s);
				left = new MultiplyToken(left, right);
				continue;
			} else if (*s == '/') {
				s++;
				I18n::ExpressionToken* right = ParseAtom(s);
				left = new DivideToken(left, right);
				continue;
			} else if (*s == '%') {
				s++;
				I18n::ExpressionToken* right = ParseAtom(s);
				left = new RemainderToken(left, right);
				continue;
			} else if (*s == ' ') {
				s++;
				continue;
			}
			break;
		}
		return left;
	}

	I18n::ExpressionToken* I18n::Evaluator::ParseSummands(const char*& s)
	{
		I18n::ExpressionToken* left = ParseFactors(s);
		while (*s != '\0') {
			if (*s == '+') {
				s++;
				I18n::ExpressionToken* right = ParseFactors(s);
				left = new AddToken(left, right);
				continue;
			} else if (*s == '-') {
				s++;
				I18n::ExpressionToken* right = ParseFactors(s);
				left = new SubtractToken(left, right);
				continue;
			} else if (*s == ' ') {
				s++;
				continue;
			}
			break;
		}
		return left;
	}

	I18n::ExpressionToken* I18n::Evaluator::ParseComparison(const char*& s)
	{
		I18n::ExpressionToken* left = ParseSummands(s);
		while (*s != '\0') {
			if (*s == '=' && *(s + 1) == '=') {
				s += 2;
				I18n::ExpressionToken* right = ParseSummands(s);
				left = new CompareEqualsToken(left, right);
				continue;
			} else if (*s == '!' && *(s + 1) == '=') {
				s += 2;
				I18n::ExpressionToken* right = ParseSummands(s);
				left = new CompareNotEqualsToken(left, right);
				continue;
			} else if (*s == '<' && *(s + 1) == '=') {
				s += 2;
				I18n::ExpressionToken* right = ParseSummands(s);
				left = new CompareEqualsOrLessToken(left, right);
				continue;
			} else if (*s == '>' && *(s + 1) == '=') {
				s += 2;
				I18n::ExpressionToken* right = ParseSummands(s);
				left = new CompareEqualsOrGreaterToken(left, right);
				continue;
			} else if (*s == '<') {
				s++;
				I18n::ExpressionToken* right = ParseSummands(s);
				left = new CompareLessToken(left, right);
				continue;
			} else if (*s == '>') {
				s++;
				I18n::ExpressionToken* right = ParseSummands(s);
				left = new CompareGreaterToken(left, right);
				continue;
			} else if (*s == ' ') {
				s++;
				continue;
			}
			break;
		}
		return left;
	}

	I18n::ExpressionToken* I18n::Evaluator::ParseCondition(const char*& s)
	{
		I18n::ExpressionToken* left = ParseComparison(s);
		while (*s != '\0') {
			if (*s == '&' && *(s + 1) == '&') {
				s += 2;
				I18n::ExpressionToken* right = ParseComparison(s);
				left = new LogicalAndToken(left, right);
				continue;
			} else if (*s == '|' && *(s + 1) == '|') {
				s += 2;
				I18n::ExpressionToken* right = ParseComparison(s);
				left = new LogicalOrToken(left, right);
				continue;
			} else if (*s == ' ') {
				s++;
				continue;
			}
			break;
		}
		return left;
	}

	I18n::ExpressionToken* I18n::Evaluator::ParseTernary(const char*& s)
	{
		I18n::ExpressionToken* comparison = ParseCondition(s);
		while (*s == ' ') {
			s++;
		}
		if (*s != '?') {
			return comparison;
		}
		s++;
		I18n::ExpressionToken* left = ParseTernary(s);
		while (*s == ' ') {
			s++;
		}
		if (*s != ':') {
			return comparison;
		}
		s++;
		I18n::ExpressionToken* right = ParseTernary(s);
		return new TernaryExpressionToken(comparison, left, right);
	}

	const I18n::ExpressionToken* I18n::Evaluator::Parse(const char* s)
	{
		I18n::ExpressionToken* result = ParseTernary(s);
		while (*s == ' ') {
			s++;
		}
		if (*s != '\0' && *s != ';' && *s != '\r' && *s != '\n') {
			delete result;
			return nullptr;
		}
		return result;
	}

	I18n& I18n::Get()
	{
		static I18n current;
		return current;
	}

	I18n::I18n()
		: _fileSize(0), _stringCount(0), _origTable(nullptr), _transTable(nullptr), _hashSize(0), _hashTable(nullptr), _pluralExp(nullptr), _pluralCount(0)
	{
	}

	I18n::~I18n()
	{
		if (_pluralExp != nullptr) {
			delete _pluralExp;
			_pluralExp = nullptr;
		}
	}

	void I18n::Unload()
	{
		_file = nullptr;
		_fileSize = 0;
		_stringCount = 0;
		_origTable = nullptr;
		_transTable = nullptr;
		_hashSize = 0;
		_hashTable = nullptr;

		if (_pluralExp != nullptr) {
			delete _pluralExp;
			_pluralExp = nullptr;
		}
	}

	bool I18n::LoadFromFile(const StringView& path)
	{
		return LoadFromFile(fs::Open(path, FileAccessMode::Read));
	}

	bool I18n::LoadFromFile(const std::unique_ptr<IFileStream>& fileHandle)
	{
		uint32_t fileSize = fileHandle->GetSize();
		if (fileSize < 32 || fileSize > 16 * 1024 * 1024) {
			return false;
		}

		_file = std::make_unique<char[]>(fileSize + 1);
		fileHandle->Read(_file.get(), fileSize);
		_file[fileSize] = '\0';
		_fileSize = fileSize;

		constexpr uint32_t SignatureLE = 0x950412de;
		constexpr uint32_t SignatureBE = 0xde120495;
		MoFileHeader* data = (MoFileHeader*)_file.get();
		if (!(data->Signature == SignatureLE || data->Signature == SignatureBE) || data->StringCount <= 0 ||
			data->OrigTableOffset + data->StringCount > fileSize || data->TransTableOffset + data->StringCount > fileSize ||
			data->HashTableOffset + data->HashTableSize > fileSize) {
			LOGE("Invalid \".mo\" file");
			Unload();
			return false;
		}

		_stringCount = data->StringCount;
		_origTable = (const StringDesc*)((char*)data + data->OrigTableOffset);
		_transTable = (const StringDesc*)((char*)data + data->TransTableOffset);
		_hashSize = data->HashTableSize;
		_hashTable = (_hashSize > 2 ? (const uint32_t*)((char*)data + data->HashTableOffset) : nullptr);

		uint32_t entryLength;
		const char* nullEntry = LookupTranslation("", &entryLength);
		ExtractPluralExpression(nullEntry, &_pluralExp, &_pluralCount);

		return true;
	}

	const char* I18n::LookupTranslation(const char* msgid, uint32_t* resultLength)
	{
		if (_hashTable != nullptr) {
			// Use the hashing table
			constexpr uint32_t HashWordBits = 32;

			const char* str = msgid;
			uint32_t hashValue = 0;
			uint32_t len = 0;
			while (*str != '\0') {
				hashValue <<= 4;
				hashValue += (uint8_t)*str++;
				uint32_t g = hashValue & ((uint32_t) 0xf << (HashWordBits - 4));
				if (g != 0) {
					hashValue ^= g >> (HashWordBits - 8);
					hashValue ^= g;
				}
				len++;
			}

			uint32_t idx = hashValue % _hashSize;
			uint32_t incr = 1 + (hashValue % (_hashSize - 2));

			while (true) {
				uint32_t nstr = _hashTable[idx];
				if (nstr == 0) {
					// Hash table entry is empty
					return nullptr;
				}
				nstr--;

				// Compare `msgid` with the original string at index `nstr`. We compare the lengths with `>=`, not `==`,
				// because plural entries are represented by strings with an embedded NULL.
				if (nstr < _stringCount && _origTable[nstr].Length >= len && (std::strcmp(msgid, _file.get() + _origTable[nstr].Offset) == 0)) {
					if (_transTable[nstr].Offset >= _fileSize) {
						return nullptr;
					}
					*resultLength = _transTable[nstr].Length;
					return (char*)(_file.get() + _transTable[nstr].Offset);
				}

				if (idx >= _hashSize - incr) {
					idx -= _hashSize - incr;
				} else {
					idx += incr;
				}
			}
		} else {
			// Binary search in the sorted array of messages
			size_t bottom = 0;
			size_t top = _stringCount;
			while (bottom < top) {
				size_t idx = (bottom + top) / 2;
				if (_origTable[idx].Offset >= _fileSize) {
					return nullptr;
				}
				int cmpVal = std::strcmp(msgid, (_file.get() + _origTable[idx].Offset));
				if (cmpVal < 0) {
					top = idx;
				} else if (cmpVal > 0) {
					bottom = idx + 1;
				} else {
					if (_transTable[idx].Offset >= _fileSize) {
						return nullptr;
					}
					*resultLength = _transTable[idx].Length;
					return (char*)(_file.get() + _transTable[idx].Offset);
				}
			}
		}

		return nullptr;
	}

	const char* I18n::LookupPlural(int n, const char* translation, uint32_t translationLength)
	{
		int index = (*_pluralExp)(n);

		const char* p = translation;
		while (index-- > 0) {
			p = strchr(p, '\0');
			p++;
			if (p >= translation + translationLength) {
				return translation;
			}
		}
		return p;
	}

	StringView I18n::GetTranslationDescription()
	{
		uint32_t entryLength;
		const char* nullEntry = LookupTranslation("", &entryLength);
		if (nullEntry != nullptr) {
			StringView translationInfo = StringView(nullEntry, entryLength);
			StringView languageTeamBegin = translationInfo.find("Language-Team:"_s);
			if (languageTeamBegin != nullptr) {
				languageTeamBegin = translationInfo.suffix(languageTeamBegin.end());
				StringView languageTeamEnd = languageTeamBegin.findOr('\n', translationInfo.end());
				return translationInfo.slice(languageTeamBegin.begin(), languageTeamEnd.begin()).trimmed();
			}
		}

		return { };
	}

	Array<String> I18n::GetPreferredLanguages()
	{
		Array<String> preferred;

#if defined(DEATH_TARGET_ANDROID)
		String langId = AndroidJniWrap_Activity::getPreferredLanguage();
		if (!langId.empty()) {
			lowercaseInPlace(langId);
			arrayAppend(preferred, std::move(langId));
		}
#elif defined(DEATH_TARGET_APPLE)
		CFTypeRef preferences = CFPreferencesCopyAppValue(CFSTR("AppleLanguages"), kCFPreferencesCurrentApplication);
		if (preferences != nullptr && CFGetTypeID(preferences) == CFArrayGetTypeID()) {
			CFArrayRef prefArray = (CFArrayRef)preferences;
			int n = CFArrayGetCount(prefArray);
			char buffer[256];
			size_t size = 0;

			for (int i = 0; i < n; i++) {
				CFTypeRef element = CFArrayGetValueAtIndex(prefArray, i);
				if (element != nullptr && CFGetTypeID(element) == CFStringGetTypeID() && CFStringGetCString((CFStringRef)element, buffer, sizeof(buffer), kCFStringEncodingASCII)) {
					String langId = String(buffer);
					lowercaseInPlace(langId);
					arrayAppend(preferred, std::move(langId));
				} else {
					break;
				}
			}
		}
#elif defined(DEATH_TARGET_EMSCRIPTEN) || defined(DEATH_TARGET_UNIX)
		char* langRaw = ::getenv("LANG");
		if (langRaw == nullptr) {
			langRaw = ::getenv("LC_ALL");
			if (langRaw == nullptr) {
				langRaw = ::getenv("LC_MESSAGES");
			}
		}
		if (langRaw != nullptr) {
			String langId = langRaw;
			StringView suffix = langId.findAny(".@"_s);
			if (suffix != nullptr) {
				langId = langId.prefix(suffix.begin());
			}
			for (char& c : langId) {
				if (c == '_') c = '-';
			}
			lowercaseInPlace(langId);
			arrayAppend(preferred, std::move(langId));
		}
#elif defined(DEATH_TARGET_WINDOWS)
		if (Environment::IsWindows10) {
			// Get list of all preferred UI languages on Windows 10
			ULONG numberOfLanguages = 0;
			ULONG bufferSize = 0;
			if (::GetUserPreferredUILanguages(MUI_LANGUAGE_NAME, &numberOfLanguages, nullptr, &bufferSize)) {
				Array<wchar_t> languages(NoInit, bufferSize);
				if (::GetUserPreferredUILanguages(MUI_LANGUAGE_NAME, &numberOfLanguages, languages.data(), &bufferSize)) {
					wchar_t* buffer = languages.data();
					for (ULONG k = 0; k < numberOfLanguages; ++k) {
						String langId = Utf8::FromUtf16(buffer);
						lowercaseInPlace(langId);
						arrayAppend(preferred, std::move(langId));

						while (*buffer != L'\0') {
							buffer++;
						}
						buffer++;
					}
				}
			}
		} else {
			// Use the default user locale for Windows 8 and below
			wchar_t buffer[LOCALE_NAME_MAX_LENGTH];
			if (::GetUserDefaultLocaleName(buffer, LOCALE_NAME_MAX_LENGTH)) {
				String langId = Utf8::FromUtf16(buffer);
				lowercaseInPlace(langId);
				arrayAppend(preferred, std::move(langId));
			}
		}
#endif

		return preferred;
	}

	StringView I18n::GetLanguageName(const StringView& langId)
	{
		StringView baseLanguage = TryRemoveLanguageSpecifiers(langId);

		size_t bottom = 0;
		size_t top = countof(SupportedLanguages);
		while (bottom < top) {
			size_t index = (bottom + top) / 2;
			int cmpVal = std::strncmp(langId.data(), SupportedLanguages[index].Identifier, baseLanguage.size());
			if (cmpVal < 0) {
				top = index;
			} else if (cmpVal > 0) {
				bottom = index + 1;
			} else {
				return SupportedLanguages[index].Name;
			}
		}

		return { };
	}

	StringView I18n::TryRemoveLanguageSpecifiers(const StringView& langId)
	{
		StringView suffix = langId.findAny("-_.@"_s);
		return (suffix != nullptr ? langId.prefix(suffix.begin()) : langId);
	}

	void I18n::ExtractPluralExpression(const char* nullEntry, const ExpressionToken** pluralExp, uint32_t* pluralCount)
	{
		// Destroy previous evaluator
		if (*pluralExp != nullptr) {
			delete *pluralExp;
		}

		if (nullEntry != nullptr) {
			const char* plural = strstr(nullEntry, "plural=");
			const char* nplurals = strstr(nullEntry, "nplurals=");
			if (plural != nullptr && nplurals != nullptr) {
				nplurals += 9;
				while (*nplurals != '\0' && std::isspace(*nplurals)) {
					nplurals++;
				}
				char* endp;
				*pluralCount = strtoul(nplurals, &endp, 10);
				if (nplurals != endp) {
					plural += 7;
					*pluralExp = Evaluator::Parse(plural);
					if (*pluralExp != nullptr) {
						return;
					}
				}
			}
		}

		*pluralExp = new CompareNotEqualsToken(new VariableToken(), new ValueToken(1));
		*pluralCount = 2;
	}

	String _f(const char* text, ...)
	{
		uint32_t resultLength;
		const char* translated = I18n::Get().LookupTranslation(text, &resultLength);
		const char* format = (translated != nullptr ? translated : text);

		va_list args;
		va_start(args, text);
#if defined(DEATH_TARGET_WINDOWS) && !defined(DEATH_TARGET_MINGW)
		const int totalChars = _vscprintf(format, args);
		String result(NoInit, totalChars);
		vsnprintf_s(result.data(), totalChars + 1, totalChars, format, args);
		va_end(args);
		return result;
#else
		// TODO: Quickfix strange bug on Linux
		//const int totalChars = ::vsnprintf(nullptr, 0, format, args);
		//String result(NoInit, totalChars);
		//::vsnprintf(result.data(), totalChars + 1, format, args);

		char buffer[2048];
		::vsnprintf(buffer, sizeof(buffer), format, args);
		va_end(args);
		return buffer;
#endif
	}

	String _fn(const char* singular, const char* plural, int n, ...)
	{
		uint32_t resultLength;
		const char* translated = I18n::Get().LookupTranslation(singular, &resultLength);
		const char* format;
		if (translated != nullptr) {
			format = I18n::Get().LookupPlural(n, translated, resultLength);
		} else {
			format = (n == 1 ? singular : plural);
		}

		va_list args;
		va_start(args, n);
#if defined(DEATH_TARGET_WINDOWS) && !defined(DEATH_TARGET_MINGW)
		const int totalChars = _vscprintf(format, args);
		String result(NoInit, totalChars);
		vsnprintf_s(result.data(), totalChars + 1, totalChars, format, args);
		va_end(args);
		return result;
#else
		// TODO: Quickfix strange bug on Linux
		//const int totalChars = ::vsnprintf(nullptr, 0, format, args);
		//String result(NoInit, totalChars);
		//::vsnprintf(result.data(), totalChars + 1, format, args);

		char buffer[2048];
		::vsnprintf(buffer, sizeof(buffer), format, args);
		va_end(args);
		return buffer;
#endif
	}
}
