#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iterator>

// Lua 5.0 and its lexer internals are C. IWYU pragma: begin_keep
extern "C" {
#include "lobject.h"
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
#include "lzio.h"
#include "llex.h"
#include "lstring.h"
}
// IWYU pragma: end_keep

#include <algorithm>
#include <iostream>
#include <set>
#include <map>
#include <fstream>
#include <string>
#include <sstream>
#include <utility>
#include <vector>

#include <sys/stat.h>
#include <cerrno>

#include <game/crc32.h>

#define OUTPUT_FOLDER "object"
lua_State* L;

struct LoadF {
	FILE *f;
	char buff[LUAL_BUFFERSIZE];
};

char * g_filename;

unsigned int get_string_crc(const std::string & str)
{
	const unsigned char * s = (const unsigned char*) str.c_str();
	const unsigned char * end = s + str.size();
	unsigned int h = 0;

	while (s < end)
	{
		h *= 16777619;
		h ^= (unsigned char) *(unsigned char *) (s++);
	}

	return h;
}


static const char *getF (lua_State *L, void *ud, size_t *size) {
	LoadF *lf = (LoadF *)ud;

	if (feof(lf->f))
		return nullptr;

	*size = fread(lf->buff, 1, LUAL_BUFFERSIZE, lf->f);
	return (*size > 0) ? lf->buff : nullptr;
}

static void next (LexState * ls)
{
	ls->lastline = ls->linenumber;

	if (ls->lookahead.token != TK_EOS)
	{
		/* is there a look-ahead token? */
		ls->t = ls->lookahead;  /* use this one */
		ls->lookahead.token = TK_EOS;  /* and discharge it */
	}
	else
		ls->t.token = luaX_lex(ls, &ls->t.seminfo);  /* read next token */
}

static bool testnext (LexState * ls, int c)
{
	if (ls->t.token == c) 
	{
		next(ls);
		return true;
	}
	else 
		return false;
}

static void lookahead (LexState * ls)
{
	lua_assert(ls->lookahead.token == TK_EOS);
	ls->lookahead.token = luaX_lex(ls, &ls->lookahead.seminfo);
}

enum parse_state
{
	ST_START,
	ST_QUEST,
	ST_QUEST_WITH_OR_BEGIN,
	ST_STATELIST,
	ST_STATE_NAME,
	ST_STATE_BEGIN,
	ST_WHENLIST_OR_FUNCTION,
	ST_WHEN_NAME,
	ST_WHEN_WITH_OR_BEGIN,
	ST_WHEN_BODY,
	ST_FUNCTION_NAME,
	ST_FUNCTION_ARG,
	ST_FUNCTION_BODY,
};

struct LexState * pls = nullptr;
void errorline(int line , const char * str)
{
	std::cout.flush();
	if (g_filename)
		std::cerr << g_filename <<":";
	std::cerr << line<< ':';
	std::cerr << str << std::endl;
	abort();
}
void error(const char * str)
{
	std::cout.flush();
	if (g_filename)
		std::cout << g_filename <<":";
	if (pls)
	{
		std::cout << pls->linenumber << ':';
	}
	std::cerr << str << std::endl;
	abort();
}

#define assert(exp) if (!(exp)) error("assertion failure : " #exp)

std::ostream & operator << (std::ostream & ostr, const Token& tok)
{
	if (tok.token == TK_NAME)
		ostr << getstr(tok.seminfo.ts);
	else if (tok.token == TK_NUMBER)
		ostr << tok.seminfo.r;
	else if (tok.token == TK_STRING)
		ostr << '"' << getstr(tok.seminfo.ts) <<'"';
	else
		ostr << luaX_token2str(pls,tok.token);
	return ostr;
}

bool check_syntax(const std::string& str, const std::string& module)
{
	int ret = luaL_loadbuffer(L, str.c_str(), str.size(), module.c_str());

	if (ret)
	{
		std::cerr << str << std::endl;
		error((std::string("syntax error : ")+lua_tostring(L,-1) ).c_str());
		return false;
	}

	lua_pop(L,1);
	return true;
}

std::set<std::string> function_defs;
std::set<std::string> function_calls;

void RegisterDefFunction(const std::string& fname)
{
	function_defs.insert(fname);
}

void RegisterUsedFunction(const std::string& fname)
{
	function_calls.insert(fname);
}

void CheckUsedFunction()
{
	bool hasError = false;
	std::set<std::string> error_func;

	for (__typeof(function_calls.begin()) it = function_calls.begin(); it != function_calls.end(); ++it)
	{
		if (function_defs.find(*it) == function_defs.end())
		{
			hasError = true;
			error_func.insert(*it);
		}
	}

	if (hasError)
	{
		std::cout << "Calls undeclared function! : " << std::endl;
		for (__typeof(error_func.begin()) it = error_func.begin(); it != error_func.end(); ++it)
		{
			std::cout << *it << std::endl;
		}
		abort();
	}
}

void load_quest_function_list(const char* filename)
{
	std::ifstream inf(filename);
	std::string s;

	while (!inf.eof())
	{
		inf >> s;
		if (inf.fail())
			break;

		RegisterDefFunction(s);
	}
}

struct AScript
{
	std::string when_condition;
	std::string when_argument;
	std::string script;

	AScript(std::string a, std::string b, std::string c) :
		when_condition(a),
		when_argument(b),
		script(c)
	{}

	AScript() {}
};

void parse(char * filename)
{
	std::ifstream inf(filename);
	LoadF lf;
	lf.f = fopen(filename,"r");
	ZIO z;
	luaZ_init(&z, getF,&lf,"quest");
	Mbuffer b;
	struct LexState lexstate;
	pls = &lexstate;
	luaZ_initbuffer(L,&b);
	lexstate.buff = &b;
	luaX_setinput(L, &lexstate, &z, luaS_new(L, zname(&z)));

	parse_state ps = ST_START;

	int nested = 0;

	std::string quest_name;
	std::string start_condition;

	std::string current_state_name;
	std::string current_when_name;
	std::string current_when_condition;

	std::string current_when_argument;

	std::set<std::string> define_state_name_set;
	std::map<int,std::string> used_state_name_map;

	std::map<std::string, std::map<std::string, std::string> > state_script_map;
	std::map<std::string, std::map<std::string, std::vector<AScript> > > state_arg_script_map;

	std::vector<std::pair<std::string, std::string> > when_name_arg_vector;

	std::string current_function_name;
	std::string current_function_arg;
	std::string all_functions;

	load_quest_function_list("quest_functions");

	while (1)
	{
		next(&lexstate);

		if (lexstate.t.token == TK_EOS) break;

		Token& t= lexstate.t;

		switch(ps)
		{
			case ST_START:
				{
					assert(nested==0);
					if (t.token == TK_QUEST)
						ps = ST_QUEST;
					else
						error("must start with 'quest'");
				}
				break;
			case ST_QUEST:
				{
					assert(nested==0);
					if (t.token == TK_NAME || t.token == TK_STRING)
					{
						quest_name = getstr(lexstate.t.seminfo.ts);
						std::cout << "QUEST : " << quest_name << std::endl;
						ps = ST_QUEST_WITH_OR_BEGIN;
					}
					else
						error("quest name must be given");
				}
				break;
			case ST_QUEST_WITH_OR_BEGIN:
				assert(nested==0);
				if (t.token == TK_WITH)
				{
					assert(nested==0);
					next(&lexstate);
					std::ostringstream os;
					os << (lexstate.t);
					next(&lexstate);
					while (lexstate.t.token!=TK_DO)
					{
						os << " " <<(lexstate.t);
						next(&lexstate);
					}
					start_condition = os.str();
					check_syntax("if "+start_condition+" then end", quest_name);
					std::cout << "\twith ";
					std::cout << start_condition;
					std::cout << std::endl;
					t = lexstate.t;
				}
				if (t.token == TK_DO)
				{
					ps = ST_STATELIST;
					nested++;
				}
				else
				{
					std::ostringstream os;
					os << "quest doesn't have begin-end clause. (" << t << ")";
					error(os.str().c_str());
				}
				break;
			case ST_STATELIST:
				{
					assert(nested==1);
					if (t.token == TK_STATE)
					{
						ps = ST_STATE_NAME;
					}
					else if (t.token == TK_END)
					{
						nested --;
						ps = ST_START;
					}
					else
					{
						error("expecting 'state'");
					}
				}
				break;
			case ST_STATE_NAME:
				{
					assert(nested==1);
					if (t.token == TK_NAME || t.token == TK_STRING)
					{
						current_state_name = getstr(t.seminfo.ts);
						define_state_name_set.insert(current_state_name);
						std::cout << "STATE : " << current_state_name << std::endl;
						ps = ST_STATE_BEGIN;
					}
					else
					{
						error("state name must be given");
					}
				}
				break;

			case ST_STATE_BEGIN:
				{
					assert(nested==1);

					if (t.token == TK_DO)
					{
						nested ++;
						ps = ST_WHENLIST_OR_FUNCTION;
					}
					else
					{
						error("state doesn't have begin-end clause.");
					}
				}
				break;

			case ST_WHENLIST_OR_FUNCTION:
				{
					assert(nested==2);

					if (t.token == TK_WHEN)
					{
						ps = ST_WHEN_NAME;

						when_name_arg_vector.clear();
					}
					else if (t.token == TK_END)
					{
						nested--;
						ps = ST_STATELIST;
					}
					else if (t.token == TK_FUNCTION)
					{
						ps = ST_FUNCTION_NAME;
					}
					else
					{
						error("expecting 'when' or 'function'");
					}
				}
				break;

			case ST_WHEN_NAME:
				{
					assert(nested==2);
					if (t.token == TK_NAME || t.token == TK_STRING || t.token == TK_NUMBER)
					{
						if (t.token == TK_NUMBER)
						{
							std::ostringstream os;
							os << (unsigned int)t.seminfo.r;
							current_when_name = os.str();
							lexstate.lookahead.token  = '.';
						}
						else
						{
							current_when_name = getstr(t.seminfo.ts);
							lookahead(&lexstate);
						}
						ps = ST_WHEN_WITH_OR_BEGIN;
						current_when_argument = "";
						if (lexstate.lookahead.token == '.')
						{
							next(&lexstate);
							current_when_name += '.';
							next(&lexstate);
							std::ostringstream os;
							t = lexstate.t;
							os << t;
							if (os.str() == "target")
							{
								current_when_argument = "." + current_when_name;
								current_when_argument.resize(current_when_argument.size()-1);
								current_when_name = "target";
							}
							else
							{
								current_when_name += os.str();
							}
							lookahead(&lexstate);
						}

						{
							// make when argument
							std::ostringstream os;
							while (lexstate.lookahead.token == '.')
							{
								next(&lexstate);
								os << '.';
								next(&lexstate);
								t = lexstate.t;
								os << t;
								lookahead(&lexstate);
							}
							current_when_argument += os.str();
						}
						std::cout << "WHEN  : " << current_when_name ;
						if (!current_when_argument.empty())
						{
							std::cout << " (";
							std::cout << current_when_argument.substr(1);
							std::cout << ")";
						}
					}
					else
					{
						error("when name must be given");
					}

					if (lexstate.lookahead.token == TK_OR)
					{
						// Multiple when names
						// push to somewhere -.-
						ps = ST_WHEN_NAME;
						when_name_arg_vector.push_back(make_pair(current_when_name, current_when_argument));

						next(&lexstate);
						std::cout << " or" << std::endl;
					}
					else
					{
						std::cout <<std::endl;
					}
				}
				break;
			case ST_WHEN_WITH_OR_BEGIN:
				{
					assert(nested==2);
					current_when_condition = "";
					if (t.token == TK_WITH)
					{
						// here comes the condition expression
						next(&lexstate);
						std::ostringstream os;
						os << (lexstate.t);
						next(&lexstate);
						while (lexstate.t.token!=TK_DO)
						{
							os << " " <<(lexstate.t);
							next(&lexstate);
						}
						current_when_condition = os.str();
						check_syntax("if "+current_when_condition+" then end", current_state_name+current_when_condition);
						std::cout << "\twith ";
						std::cout << current_when_condition;
						std::cout << std::endl;
						t = lexstate.t;
					}
					if (t.token == TK_DO)
					{
						ps = ST_WHEN_BODY;
						nested++;
					}
					else
					{
						std::ostringstream os;
						os << "when doesn't have begin-end clause. (" << t << ")";
						error(os.str().c_str());
					}

				}
				break;
			case ST_WHEN_BODY:
				{
					assert(nested==3);

					// output
					std::ostringstream os;
					int state_check = 0;
					__typeof(lexstate) prev = lexstate;
					std::string callname;
					bool registered = false;
					while (1)
					{
						if (lexstate.t.token == TK_DO || lexstate.t.token == TK_IF /*|| lexstate.t.token == TK_FOR*/ || lexstate.t.token == TK_BEGIN || lexstate.t.token == TK_FUNCTION)
						{
							nested++;
						}
						else if (lexstate.t.token == TK_END)
						{
							nested--;
						}

						if (!callname.empty())
						{
							lookahead(&lexstate);
							if (lexstate.lookahead.token == '(')
							{
								RegisterUsedFunction(callname);
								registered = true;
							}
							callname.clear();
						}
						else if (lexstate.t.token == '(')
						{ 
							if (!registered && prev.t.token == TK_NAME)
								RegisterUsedFunction(getstr(prev.t.seminfo.ts));
							registered = false;
						}

						if (lexstate.t.token == '.')
						{
							std::ostringstream fname;
							lookahead(&lexstate);
							fname << prev.t << '.' << lexstate.lookahead;
							callname = fname.str();
						}

						if (state_check)
						{
							state_check--;
							if (!state_check)
							{
								if (lexstate.t.token == TK_NAME || lexstate.t.token == TK_STRING)
								{
									used_state_name_map[lexstate.linenumber] = (getstr(lexstate.t.seminfo.ts));
									lexstate.t.token = TK_STRING;
								}
							}
						}

						if (lexstate.t.token == TK_NAME && ((strcmp(getstr(lexstate.t.seminfo.ts),"set_state")==0) || (strcmp(getstr(lexstate.t.seminfo.ts),"newstate")==0) || (strcmp(getstr(lexstate.t.seminfo.ts),"setstate")==0)))
						{
							state_check = 2;
						}
						if (nested==2) break;
						os << lexstate.t << ' ';
						prev = lexstate;
						next(&lexstate);
						if (lexstate.linenumber != lexstate.lastline)
							os << std::endl;
					}


					check_syntax(os.str(), current_state_name+current_when_condition);
					reverse(when_name_arg_vector.begin(), when_name_arg_vector.end());
					while (1)
					{
						if (current_when_argument.empty())
						{
							if (current_when_condition.empty())
								state_script_map[current_when_name][current_state_name]+=os.str();
							else
								state_script_map[current_when_name][current_state_name]+="if "+current_when_condition+" then "+os.str()+" return end ";
						}
						else
						{
							state_arg_script_map[current_when_name][current_state_name].push_back(AScript(current_when_condition, current_when_argument, os.str()));
						}

						if (!when_name_arg_vector.empty())
						{
							current_when_name = when_name_arg_vector.back().first;
							current_when_argument = when_name_arg_vector.back().second;
							when_name_arg_vector.pop_back();
						}
						else
							break;
					}

					ps = ST_WHENLIST_OR_FUNCTION;
				}
				break;
			case ST_FUNCTION_NAME:
				if (t.token == TK_NAME)
				{
					current_function_name = getstr(t.seminfo.ts);
					RegisterDefFunction(quest_name+"."+current_function_name);
					ps = ST_FUNCTION_ARG;
				}
				break;
			case ST_FUNCTION_ARG:
				{
					assert(t.token == '(');
					next(&lexstate);
					current_function_arg = '(';
					if (t.token != ')')
					{
						do 
						{
							if (t.token == TK_NAME)
							{
								current_function_arg += getstr(t.seminfo.ts);
								next(&lexstate);
								if (t.token != ')')
									current_function_arg += ',';
							}
							else
							{
								std::ostringstream os;
								os << "invalud argument name " << getstr(t.seminfo.ts) << " for function " << current_function_name;
								error(os.str().c_str());
							}
						} while (testnext(&lexstate, ','));
					}
					current_function_arg += ')';
					ps = ST_FUNCTION_BODY;
					nested ++;
				}
				break;

			case ST_FUNCTION_BODY:
				{
					assert(nested == 3);
					std::ostringstream os;
					__typeof(lexstate) prev = lexstate;
					bool registered = false;
					if (prev.t.token == '.')
						prev.t.token = TK_DO;
					std::string callname;
					while (nested >= 3)
					{
						if (lexstate.t.token == TK_DO || lexstate.t.token == TK_IF /*|| lexstate.t.token == TK_FOR*/ || lexstate.t.token == TK_BEGIN || lexstate.t.token == TK_FUNCTION)
						{
							nested++;
						}
						else if (lexstate.t.token == TK_END)
						{
							nested--;
						}

						if (!callname.empty())
						{
							lookahead(&lexstate);
							if (lexstate.lookahead.token == '(')
							{
								RegisterUsedFunction(callname);
								registered = true;
							}
							callname.clear();
						}
						else if (lexstate.t.token == '(')
						{ 
							if (!registered && prev.t.token == TK_NAME)
								RegisterUsedFunction(getstr(prev.t.seminfo.ts));
							registered = false;
						}

						if (lexstate.t.token == '.')
						{
							std::ostringstream fname;
							lookahead(&lexstate);
							fname << prev.t << '.' << lexstate.lookahead;
							callname = fname.str();
						}

						os << lexstate.t << ' ';
						if (nested == 2)
							break;
						prev = lexstate;
						next(&lexstate);
						if (lexstate.linenumber != lexstate.lastline)
							os << std::endl;

					}
					ps = ST_WHENLIST_OR_FUNCTION;
					all_functions += ',';
					all_functions += current_function_name;
					all_functions += "= function ";
					all_functions += current_function_arg;
					all_functions += os.str();
					std::cout << "FUNCTION " << current_function_name << current_function_arg << std::endl;
				}
				break;

		} // end of switch

	}
	assert(nested==0);
	for (std::map<int,std::string>::iterator it = used_state_name_map.begin();it!=used_state_name_map.end();++it)
	{
		if (define_state_name_set.find(it->second)==define_state_name_set.end())
		{
			errorline(it->first,("state name not found : "+it->second).c_str());
		}
	}

	if (!define_state_name_set.empty())
	{
		if (0 != mkdir(OUTPUT_FOLDER "/state", S_IRWXU))
		{
			if (errno != EEXIST)
			{
				perror("cannot create directory");
				exit(1);
			}
		}

		std::ofstream ouf((std::string(OUTPUT_FOLDER "/state/")+quest_name).c_str());
		ouf << quest_name << "={[\"start\"]=0";
		std::set<std::string> :: iterator it;

		std::map<std::string, int> state_crc;
		std::set<int> crc_set;

		state_crc["start"] = 0;
		for (it = define_state_name_set.begin(); it != define_state_name_set.end(); ++it)
		{
			int crc = (int) GetCRC32((*it).c_str(), (*it).size());

			if (crc_set.find(crc) == crc_set.end())
			{
				crc_set.insert(crc);	
			}
			else
			{
				++crc;

				while (crc_set.find(crc) != crc_set.end()) 
					++crc;

				printf("WARN: state CRC conflict occur! state index may differ in next compile time.\n");
				crc_set.insert(crc);
			}
			state_crc.insert(make_pair(*it, crc));
		}


		for (it = define_state_name_set.begin(); it!=define_state_name_set.end(); ++it)
		{
			if (*it != "start")
			{
				ouf << ",[\"" << *it << "\"]=" <<state_crc[*it];
			}
		}

		// Record quest functions
		ouf << all_functions;

		ouf << "}";
	}

	if (!start_condition.empty())
	{
		if (0 != mkdir(OUTPUT_FOLDER "/begin_condition", S_IRWXU))
		{
			if (errno != EEXIST)
			{
				perror("cannot create directory");
				exit(1);
			}
		}

		std::ofstream ouf((std::string(OUTPUT_FOLDER "/begin_condition/")+quest_name).c_str());

		ouf << "return " <<start_condition;
		ouf.close();
	}

	{
		std::map<std::string, std::map<std::string, std::vector<AScript> > >::iterator it;

		for (it = state_arg_script_map.begin(); it != state_arg_script_map.end(); ++it)
		{
			std::string second_name;
			std::string path;

			if (it->first.find('.')==it->first.npos)
			{
				// one like login
				std::string s(it->first);
				transform(s.begin(),s.end(),s.begin(),::tolower);
				mkdir(OUTPUT_FOLDER "/notarget",0755);
				mkdir((OUTPUT_FOLDER "/notarget/"+s).c_str(),0755);
				path = OUTPUT_FOLDER "/notarget/"+s+"/";
				second_name = s;
			}
			else
			{
				// two like [WHO].Kill
				std::string s = it->first;
				transform(s.begin(),s.end(),s.begin(),::tolower);
				int i = s.find('.');
				mkdir((OUTPUT_FOLDER "/"+it->first.substr(0,i)).c_str(),0755);
				mkdir((OUTPUT_FOLDER "/"+it->first.substr(0,i)+"/"+s.substr(i+1,s.npos)).c_str(),0755);
				path = OUTPUT_FOLDER "/" + it->first.substr(0,i) + "/"+s.substr(i+1,s.npos)+"/";
				second_name = s.substr(i+1,s.npos);
			}

			std::map<std::string,std::vector<AScript> >::iterator it2;
			for (it2 = it->second.begin(); it2 != it->second.end(); ++it2)
			{
				for (int i = 0; i < it2->second.size(); ++i)
				{
					std::ostringstream os;
					os << i;
					{
						std::ofstream ouf((path+quest_name+"."+it2->first+"."+os.str()+"."+"script").c_str());
						copy(it2->second[i].script.begin(),it2->second[i].script.end(), std::ostreambuf_iterator<char>(ouf));
					}
					{
						std::ofstream ouf((path+quest_name+"."+it2->first+"."+os.str()+"."+"when").c_str());
						if (!it2->second[i].when_condition.empty())
						{
							ouf << "return ";
							copy(it2->second[i].when_condition.begin(),it2->second[i].when_condition.end(), std::ostreambuf_iterator<char>(ouf));
						}
					}
					{
						std::ofstream ouf((path+quest_name+"."+it2->first+"."+os.str()+"."+"arg").c_str());
						copy(it2->second[i].when_argument.begin()+1,it2->second[i].when_argument.end(), std::ostreambuf_iterator<char>(ouf));
					}
				}
			}
		}
	}

	{
		std::map<std::string,std::map<std::string,std::string> >::iterator it;

		for (it = state_script_map.begin();it!=state_script_map.end();++it)
		{
			std::string path;
			if (it->first.find('.')==it->first.npos)
			{
				// one
				std::string s = it->first;
				transform(s.begin(),s.end(),s.begin(),::tolower);
				mkdir(OUTPUT_FOLDER "/notarget",0755);
				mkdir((OUTPUT_FOLDER "/notarget/"+s).c_str(),0755);
				path = OUTPUT_FOLDER "/notarget/"+s+"/";
			}
			else
			{
				// two like [WHO].Kill
				std::string s = it->first;
				transform(s.begin(),s.end(),s.begin(),::tolower);
				int i = s.find('.');
				mkdir((OUTPUT_FOLDER "/"+it->first.substr(0,i)).c_str(),0755);
				mkdir((OUTPUT_FOLDER "/"+it->first.substr(0,i)+"/"+s.substr(i+1,s.npos)).c_str(),0755);
				path = OUTPUT_FOLDER "/" + it->first.substr(0,i) + "/"+s.substr(i+1,s.npos)+"/";
			}

			std::map<std::string,std::string>::iterator it2;
			for (it2 = it->second.begin();it2!=it->second.end();++it2)
			{
				std::ofstream ouf((path+quest_name+"."+it2->first).c_str());
				copy(it2->second.begin(),it2->second.end(), std::ostreambuf_iterator<char>(ouf));
			}
		}
	}

	CheckUsedFunction();    
}

int main(int argc, char* argv[])
{
	mkdir(OUTPUT_FOLDER, 0700);
	L = lua_open();
	luaX_init(L);

	if (argc > 1)
	{
		for (int i = 1; i < argc; ++i)
		{
			g_filename = argv[i];
			parse(argv[i]);
		}
	}

	lua_close(L);
	return 0;
}

