//============================================================================
// Name        : IniParser.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Ini Parser
//============================================================================

#include <Pyros3D/Other/Export.h>
#include <iostream>
#include <fstream>
#include <string>
#include <map>

using namespace std;

#ifndef INIPARSER_H
#define	INIPARSER_H

namespace p3d {

	class IniParser {

	public:

		IniParser() {}

		bool ParseFile(const std::string &file)
		{
			string line;
			ifstream myfile(file.c_str());
			if (myfile.is_open())
			{

				bool groupSet = false;

				// Group Name
				string grpName;
				// Options
				std::map<string, string> options;
				uint32 countLines = 0;
				while (getline(myfile, line))
				{
					std::size_t foundBracket1 = line.find('[');
					std::size_t foundBracket2 = line.find(']');
					if (foundBracket1 < line.size() && foundBracket2 < line.size())
					{
						// Its a Group

						if (groupSet)
						{
							groupSet = false;
							INI[grpName] = options;
							options.clear();
						}

						grpName = line.substr(foundBracket1 + 1, foundBracket2 - 1);
						groupSet = true;
					}
					else {
						// Its a Value
						std::size_t foundEqual = line.find('=');
						if (foundEqual < line.size())
							options[line.substr(0, foundEqual)] = line.substr(foundEqual + 1, line.size());
					}
					countLines++;
				}

				INI[grpName] = options;
				options.clear();

				myfile.close();

				return countLines > 0;

			}
			else {

				return false;
			}
		}

		string GetValue(const string &Group, const string &Option)
		{
			string result = INI[Group][Option];
			if (result[result.size() - 1] == '\r') result.erase(result.size() - 1);
			return result;
		}

		void SetValue(const string &Group, const string &Option, const string &Value)
		{
			INI[Group][Option] = Value;
		}

		bool SaveToFile(const string &file)
		{
			ofstream myfile(file.c_str());
			if (myfile.is_open())
			{
				for (map<string, map<string, string> >::iterator i = INI.begin(); i != INI.end(); i++)
				{
					myfile << "[" << (*i).first << "]\n";
					for (map<string, string>::iterator k = (*i).second.begin(); k != (*i).second.end(); k++)
					{
						myfile << (*k).first << "=" << (*k).second << "\n";
					}
				}
				myfile.close();
				return true;
			}
			else {
				return false;
			}
		}

		const map<string, map<string, string> > &GetIniParsedMap() const { return INI; }

	private:

		// Map to Keep Configurations
		map<string, map<string, string> > INI;

	};

};

#endif /* INIPARSER_H */