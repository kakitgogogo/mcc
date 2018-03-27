#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <map>
#include <set>
#include <vector>
#include <iomanip>
#include <assert.h>
using namespace std;

set<string> terms;
multimap<string, vector<string> > sentence;
map<string, string> rsentence;
map<string, vector<string> > rightMap;
set<string> nterm;
set<string> term;
map<string, bool> toEmpty;
map<string, set<string> > First;
map<string, set<string> > Follow;
vector<vector<string> > rightSide;
map<string, map<string, set<string> > > Select;
string start;

void init(ifstream& fin) {
	string line;
	while(getline(fin, line)) {
		if(line.empty()) continue;
		istringstream is; 
		is.str(line); 

		string word;
		is >> word;
		if(word == "%%") return;
		assert(word == "%token");
		while(is >> word) {
			terms.insert(word);
		}
	} 
}

bool isterm(const string &s)
{
	return (s[0] == '\'' && s.back() == '\'') || terms.find(s) != terms.end();
}

bool isAllNterm(const vector<string>& right)
{
	for(auto& item:right)
	{
		if(isterm(item))
		{
			return false;
		}
	}
	return true;
}

bool isToEmpty(const string& s)
{
	auto it = sentence.find(s);
	int cnt = sentence.count(s);

	while(cnt--)
	{
		if(it->second.size() == 1 && it->second[0] == "'empty'")
		{
			return true;
		}
		if(isAllNterm(it->second))
		{
			bool flg = true;
			for(auto& item:it->second)
			{
				if(item == s || !isToEmpty(item))
				{
					flg = false;
					break;
				}
			}
			if(flg)
			{
				return true;
			}
		}
		++it;
	}
	return false;
}

void getFirst(const string& s)
{
	if(!First[s].empty()) return;
	auto it = sentence.find(s);
	int cnt = sentence.count(s);

	while(cnt--)
	{
		for(auto& item:it->second)
		{
			if(isterm(item))
			{
				First[s].insert(item);
				break;
			}
			else
			{
				if(item == s)
				{
					break;
				}
				getFirst(item);
				for(auto& f:First[item])
				{
					First[s].insert(f);
				}
				if(!toEmpty[item])
				{
					break;
				}
			}
		}
		++it;
	}
}

string getLeftSide(const vector<string>& right)
{
	string sen;
	for(auto& item:right)
	{
		sen += item + " ";
	}
	return rsentence[sen];
}

bool isLast(const vector<string>& right, const string& s)
{
	for(int i = right.size()-1; i >=0; --i)
	{
		if(s == right[i])
		{
			return true;
		}
		if(isterm(right[i]) || !toEmpty[right[i]])
		{
			return false;
		}
	}
	return false;
}

set<string> currentQuerySet;

void getFollow(const string& s)
{
	if(!Follow[s].empty()) return;
	if(s == start) Follow[s].insert("#");

	for(auto& it:rightSide)
	{
		for(int i = 0; i < it.size()-1; ++i)
		{
			if(s == it[i])
			{
				if(isterm(it[i+1]))
				{
					Follow[s].insert(it[i+1]);
				}
				else
				{
					for(auto& f:First[it[i+1]])
					{
						Follow[s].insert(f);
					}
				}
			}
		}

		if(isLast(it, s))
		{
			if(currentQuerySet.find(s) != currentQuerySet.end()) continue;
			currentQuerySet.insert(s);

			string left = getLeftSide(it);
			getFollow(left);
			for(auto& item:Follow[left])
			{
				Follow[s].insert(item);
			}
			currentQuerySet.clear();
		}
	}

	Follow[s].erase("'empty'");
}

bool isAllToEmpty(const vector<string>& right)
{
	for(auto& item:right)
	{
		if(!toEmpty[item]) return false;
	}
	return true;
}

string getRightSide(const vector<string>& right)
{
	string sen;
	for(auto& item:right)
	{
		sen += item + " ";
	}
	return sen;
}

void getSelect(const string& s)
{
	auto it = sentence.find(s);
	int cnt = sentence.count(s);

	while(cnt--)
	{
		string sen = getRightSide(it->second);
		for(auto& item:it->second)
		{
			if(isterm(item))
			{
				Select[s][sen].insert(item);
				break;
			}
			else
			{
				for(auto& f:First[item])
				{
					Select[s][sen].insert(f);
				}
				if(!toEmpty[item])
				{
					break;
				}
			}
		}
		if(isAllToEmpty(it->second))
		{
			for(auto& f:Follow[s])
			{
				Select[s][sen].insert(f);
			}
		}
		Select[s][sen].erase("'empty'");
		++it;
	}
}

void notLL1(const string& t, string err, bool want_exit=true)
{
	cout << "not ll(1): " << t << ": " << err << endl;
	if(want_exit)
		exit(1);
}

void printTable(string filename)
{
	ofstream fout(filename+".log");
	fout << "-------------------------- First: --------------------------" << endl;
	for(auto& nt:nterm)
	{
		fout << nt << ":";
		for(auto& item:First[nt])
		{
			fout << "  " << item;
		}
		fout << endl;
	}

	fout << "-------------------------- Follow: --------------------------" << endl;
	for(auto& nt:nterm)
	{
		fout << nt << ":";
		for(auto& item:Follow[nt])
		{
			fout << "  " << item;
		}
		fout << endl;
	}
#if 0
	fout << "-------------------------- Select: --------------------------" << endl;
	for(auto& nt:nterm)
	{
		for(auto& Set:Select[nt])
		{
			fout << nt << "->" << " " << Set.first << ":";
			for(auto& item:Set.second)
			{
				fout << "  " << item;
			}
			fout << endl;
		}
	}
#endif
}

int main(int argc, char* argv[])
{
	if(argc != 2) {
		cerr << "usage: ll1 filename" << endl;
		exit(1);
	}

	ifstream fin(argv[1]);
	init(fin);

	bool hasStart = false;
	string line;
	string left, last_left;
	while(getline(fin, line))
	{
		if(line.empty()) continue;
		istringstream is; 
		is.str(line + " |"); 

		string t, item, sen;
		is >> t;
		if(t[0] == '#')
		{
			continue;
		}
		else if(t == "|")
		{
			left = last_left;
		}
		else 
		{
			left = t;
			last_left = t;
			is >> item;
		}

		if(!hasStart)
		{
			start = left;
			hasStart = true;
		}

		vector<string> right;
		while(is >> item)
		{
			if(item == "|")
			{
				sentence.insert(make_pair(left, right));
				rsentence.insert(make_pair(sen, left));
				rightMap.insert(make_pair(sen, right));
				nterm.insert(left);
				rightSide.push_back(right);

				sen = "";
				right.clear();
				continue;
			}
			right.push_back(item);
			sen += item + " ";
			if(isterm(item)) term.insert(item);
		}
	}

	for(auto& nt:nterm)
	{
		set<string> hasCommon;
		auto it = sentence.find(nt);
		int cnt = sentence.count(nt);

		while(cnt--)
		{
			if(it->second[0] == nt)
			{
				notLL1(nt, "left recursive");
			}
			if(hasCommon.find(it->second[0]) != hasCommon.end())
			{
				notLL1(nt, "has left common element");
			}
			hasCommon.insert(it->second[0]);
			++it;
		}
	}

	for(auto& nt:nterm)
	{
		if(isToEmpty(nt))
		{
			toEmpty[nt] = true;
		}
		else
		{
			toEmpty[nt] = false;
		}
	}
	toEmpty["'empty'"] = true;

	for(auto& nt:nterm)
	{
		getFirst(nt);
	}

	for(auto& nt:nterm)
	{
		currentQuerySet.clear();
		getFollow(nt);
	}

	printTable(argv[1]);

	set<string> clashSet;
	for(auto& nt:nterm)
	{
		getSelect(nt);

		map<string, string> hasIntersection;
		map<string, int> clashCount;
		for(auto& Set:Select[nt])
		{
			for(auto& item:Set.second)
			{
				string str = nt + "(" + item + "): " + Set.first;
				cout << str << endl;
				if(hasIntersection.find(item) != hasIntersection.end())
				{
					if(clashCount[item] == 1) {
						notLL1(nt, "the selection set has an intersection", false);
						auto last = hasIntersection.find(item);
						string str1 = nt + "(" + last->first + "): " + last->second;
						clashSet.insert(str1);
					}
					clashSet.insert(str);
				}
				hasIntersection[item] = Set.first;
				++clashCount[item];
			}
		}
	}

	cout << "-----------------------------------------" << endl;
	if(clashSet.empty())
		cout << "\nparse complete. It's LL(1) grammer" << endl;
	else {
		cout << "\nparse complete. It isn't LL(1) grammer" << endl;
		for(auto c:clashSet) {
			cout << c << endl;
		}
	}
}