
#include <metin2/game/text/locale.hpp>

#include <metin2/log/log.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <memory>
#include <string>
#include <utility>

namespace metin2::game::inline text {

typedef std::map< std::string, std::string > locale_string_map_type;

locale_string_map_type locale_string;


void locale_add(const char *key, const char *value)
{
	locale_string_map_type::const_iterator iter = locale_string.find( key );

	if( iter == locale_string.end() )
	{
		locale_string.insert( std::make_pair( key, value ) );
	}
}

const char * locale_find(const char *string)
{

	locale_string_map_type::const_iterator iter = locale_string.find( string );

	if( iter == locale_string.end() )
	{
		// untranslated: the (UTF-8) source text itself
		metin2::log::error("LOCALE_ERROR: \"{}\";", string);
		return string;
	}

	return iter->second.c_str();
}

const char *quote_find_end(const char *string)
{
	const char  *tmp = string;
	int         quote = 0;

	while (*tmp)
	{
		if (quote && *tmp == '\\' && *(tmp + 1))
		{
			// skip if the character after \ is "
			switch (*(tmp + 1))
			{
				case '"':
					tmp += 2;
					continue;
			}
		}
		else if (*tmp == '"')
		{
			quote = !quote;
		}
		else if (!quote && *tmp == ';')
			return (tmp);

		tmp++;
	}

	return (nullptr);
}

std::unique_ptr<char[]> locale_convert(const char *src, int len)
{
	const char	*tmp;
	int		i, j;
	char	*dest;
	int		start = 0;
	char	last_char = 0;

	if (!len)
		return nullptr;

	auto buf = std::make_unique<char[]>(len + 1);

	for (j = i = 0, tmp = src, dest = buf.get(); i < len; i++, tmp++)
	{
		if (*tmp == '"')
		{
			if (last_char != '\\')
				start = !start;
			else
				goto ENCODE;
		}
		else if (*tmp == ';')
		{
			if (last_char != '\\' && !start)
				break;
			else
				goto ENCODE;
		}
		else if (start)
		{
ENCODE:
			if (*tmp == '\\' && *(tmp + 1) == 'n')
			{
				*(dest++) = '\n';
				tmp++;
				last_char = '\n';
			}
			else
			{
				*(dest++) = *tmp;
				last_char = *tmp;
			}

			j++;
		}
	}

	if (!j)
		return nullptr;

	*dest = '\0';
	return (buf);
}

constexpr int NUM_LOCALES = 2;
void locale_init(const char *filename)
{
	FILE        *fp = fopen(filename, "rb");

	if (!fp) return;

	fseek(fp, 0L, SEEK_END);
	int i = ftell(fp); 
	fseek(fp, 0L, SEEK_SET);

	i++;

	auto buf = std::make_unique<char[]>(i);

	fread(buf.get(), i - 1, sizeof(char), fp);

	fclose(fp);

	const char * tmp;
	const char * end;

	std::unique_ptr<char[]>	strings[NUM_LOCALES];

	if (!buf)
	{
		metin2::log::error("locale_read: no file {}", filename);
		exit(1);
	}

	tmp = buf.get();

	do
	{
		for (i = 0; i < NUM_LOCALES; i++)
			strings[i].reset();

		if (*tmp == '"')
		{
			for (i = 0; i < NUM_LOCALES; i++)
			{
				if (!(end = quote_find_end(tmp)))
					break;

				strings[i] = locale_convert(tmp, end - tmp);
				tmp = ++end;

				while (*tmp == '\n' || *tmp == '\r' || *tmp == ' ') tmp++;

				if (i + 1 == NUM_LOCALES)
					break;

				if (*tmp != '"')
				{
					metin2::log::error("locale_init: invalid format filename {}", filename);
					break;
				}
			}

			if (strings[0] == nullptr || strings[1] == nullptr)
				break;

			locale_add(strings[0].get(), strings[1].get());
		}
		else
		{
			tmp = strchr(tmp, '\n');

			if (tmp)
				tmp++;
		}
	}
	while (tmp && *tmp);
}
}
