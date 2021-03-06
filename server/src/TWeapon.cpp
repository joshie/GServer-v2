#include "IDebug.h"

#include "TWeapon.h"
#include "TServer.h"
#include "TLevelItem.h"
#include "TNPC.h"
#include "IEnums.h"
#include "IUtil.h"

// -- Constructor: Default Weapons -- //
TWeapon::TWeapon(const signed char pId) : mModTime(0), mWeaponDefault(pId)
{
	mWeaponName = TLevelItem::getItemName(mWeaponDefault);
}

// -- Constructor: Weapon Script -- //
TWeapon::TWeapon(TServer *pServer, const CString& pName, const CString& pImage, const CString& pScript, const time_t pModTime, bool pSaveWeapon)
: mWeaponName(pName), mWeaponImage(pImage), mModTime(pModTime), mWeaponDefault(-1)
{
	// Update Weapon
	this->updateWeapon(pServer, pImage, pScript, pModTime, pSaveWeapon);
}

// -- Function: Load Weapon -- //
TWeapon * TWeapon::loadWeapon(const CString& pWeapon, TServer *server)
{
	// File Path
	CString fileName = server->getServerPath() << "weapons/" << pWeapon;

	// Load File
	CString fileData;
	fileData.load(fileName);
	fileData.removeAllI("\r");

	// Grab some information.
	bool has_script = (fileData.find("SCRIPT") != -1 ? true : false);
	bool has_scriptend = (fileData.find("SCRIPTEND") != -1 ? true : false);
	bool found_scriptend = false;

	// Parse into lines.
	std::vector<CString> fileLines = fileData.tokenize("\n");
	if (fileLines.size() == 0 || fileLines[0].trim() != "GRAWP001")
		return 0;

	// Definitions
	CString weaponImage, weaponName, weaponScript;
	std::vector<std::pair<CString, CString> > byteCode;

	// Parse File
	std::vector<CString>::iterator i = fileLines.begin();
	while (i != fileLines.end())
	{
		// Find Command
		CString curCommand = i->readString();

		// Parse Line
		if (curCommand == "REALNAME")
			weaponName = i->readString("");
		else if (curCommand == "IMAGE")
			weaponImage = i->readString("");
		else if (curCommand == "BYTECODE")
		{
			CString fname = i->readString("");
			CString bytecode;
			bytecode.load(server->getServerPath() << "weapon_bytecode/" << fname);

			if (!bytecode.isEmpty())
				byteCode.push_back(std::pair<CString, CString>(fname, bytecode));
		}
		else if (curCommand == "SCRIPT")
		{
			++i;
			while (i != fileLines.end())
			{
				if (*i == "SCRIPTEND")
				{
					found_scriptend = true;
					break;
				}
				weaponScript << *i << "\xa7";
				++i;
			}
		}
		if (i != fileLines.end()) ++i;
	}

	// Valid Weapon Name?
	if (weaponName.isEmpty())
		return 0;

	// Give a warning if our weapon was malformed.
	if (has_scriptend && !found_scriptend)
	{
		server->getServerLog().out("[%s] WARNING: Weapon %s is malformed.\n", server->getName().text(), weaponName.text());
		server->getServerLog().out("[%s] SCRIPTEND needs to be on its own line.\n", server->getName().text());
	}

	// Give a warning if both a script and a bytecode was found.
	if (!weaponScript.isEmpty() && !byteCode.empty())
		server->getServerLog().out("[%s] WARNING: Weapon %s includes both script and bytecode.  Using bytecode.\n", server->getName().text(), weaponName.text());

	TWeapon* ret = new TWeapon(server, weaponName, weaponImage, weaponScript, 0);
	if (byteCode.size() != 0)
		ret->mByteCode = byteCode;

	return ret;
}

// -- Function: Save Weapon -- //
bool TWeapon::saveWeapon(TServer* server)
{
	// Don't save default weapons / empty weapons
	if (this->isDefault() || mWeaponName.isEmpty())
		return false;

	// Prevent the loading/saving of filenames with illegal characters.
	CString name = mWeaponName;
	name.replaceAllI("\\", "_");
	name.replaceAllI("/", "_");
	name.replaceAllI("*", "@");
	name.replaceAllI(":", ";");
	name.replaceAllI("?", "!");
	CString filename = server->getServerPath() << "weapons/weapon" << name << ".txt";

	// Format the weapon script.
	CString script(mWeaponScript);
	if (script[script.length() - 1] != '\xa7')
		script << "\xa7";
	script.replaceAllI("\xa7", "\r\n");

	// Write the File.
	CString output = "GRAWP001\r\n";
	output << "REALNAME " << mWeaponName << "\r\n";
	output << "IMAGE " << mWeaponImage << "\r\n";
	for (unsigned int i = 0; i < mByteCode.size(); ++i)
		output << "BYTECODE " << mByteCode[i].first << "\r\n";
	if (!script.isEmpty())
	{
		output << "SCRIPT\r\n";
		output << script;
		output << "SCRIPTEND\r\n";
	}

	// Save it.
	return output.save(filename);
}

// -- Function: Get Player Packet -- //
CString TWeapon::getWeaponPacket() const
{
	if (this->isDefault())
		return CString() >> (char)PLO_DEFAULTWEAPON >> (char)mWeaponDefault;

	if (mByteCode.empty())
	{
		return CString() >> (char)PLO_NPCWEAPONADD
			>> (char)mWeaponName.length() << mWeaponName
			>> (char)NPCPROP_IMAGE >> (char)mWeaponImage.length() << mWeaponImage
			>> (char)NPCPROP_SCRIPT >> (short)mScriptClient.length() << mScriptClient;
	}
	else
	{
		CString out;
		out >> (char)PLO_NPCWEAPONADD >> (char)mWeaponName.length() << mWeaponName
			>> (char)NPCPROP_IMAGE >> (char)mWeaponImage.length() << mWeaponImage
			>> (char)NPCPROP_CLASS >> (short)0 << "\n";

		for (std::vector<std::pair<CString, CString> >::const_iterator i = mByteCode.begin(); i != mByteCode.end(); ++i)
		{
			CString b = i->second;

			unsigned char id = b.readGUChar();
			CString header = b.readChars(b.readGUShort());
			CString header2 = header.guntokenize();

			CString type = header2.readString("\n");
			CString name = header2.readString("\n");
			CString unknown = header2.readString("\n");
			CString hash = header2.readString("\n");

			// Get the mod time and send packet 197.
			CString smod = CString() >> (long long)time(0);
			smod.gtokenizeI();
			out >> (char)PLO_UNKNOWN197 << header << "," << smod << "\n";

			// Add to the output stream.
			out >> (char)PLO_RAWDATA >> (int)b.length() << "\n";
			out << b;
		}

		return out;
	}
}

// -- Function: Update Weapon Image/Script -- //
void TWeapon::updateWeapon(TServer *pServer, const CString& pImage, const CString& pCode, const time_t pModTime, bool pSaveWeapon)
{
	// Copy Data
	this->setFullScript(pCode);
	this->setImage(pImage);
	this->setModTime(pModTime == 0 ? time(0) : pModTime);
	
	// Remove Comments
	CString script = removeComments(pCode, "\xa7");

	// Trim Code
	std::vector<CString> code = script.tokenize("\xa7");
	script.clear();
	for (std::vector<CString>::iterator i = code.begin(); i != code.end(); ++i)
		script << (*i).trim() << "\xa7";
	
	// Parse Text
	if (pServer->hasNPCServer())
	{
		setServerScript(script.readString("//#CLIENTSIDE"));
		setClientScript(script.readString(""));
	}
	else setClientScript(script);

	// Save Weapon
	if (pSaveWeapon)
		saveWeapon(pServer);
}
