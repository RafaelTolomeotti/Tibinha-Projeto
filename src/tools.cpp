/*
 * YurOTS, a free game server emulator 
 * Official Repository on Github <https://github.com/RafaelTolomeotti/yurOTS-Tibinha>
 * Copyright (C) 2024 - RafaelTolomeotti <https://github.com/RafaelTolomeotti>
 * A fork of The Forgotten Server(Mark Samman) branch 1.2 and part of Nostalrius(Alejandro Mujica) repositories.
 *
 * The MIT License (MIT). Copyright © 2020 <YurOTS>
 * Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), 
 * to deal in the Software without restriction, including without limitation 
 * the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, 
 * and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
 * The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
*/

#include "otpch.h"

#include "tools.h"
#include "configmanager.h"

extern ConfigManager g_config;

void printXMLError(const std::string& where, const std::string& fileName, const pugi::xml_parse_result& result)
{
	std::cout << '[' << where << "] Failed to load " << fileName << ": " << result.description() << std::endl;

	FILE* file = fopen(fileName.c_str(), "rb");
	if (!file) {
		return;
	}

	char buffer[32768];
	uint32_t currentLine = 1;
	std::string line;

	size_t offset = static_cast<size_t>(result.offset);
	size_t lineOffsetPosition = 0;
	size_t index = 0;
	size_t bytes;
	do {
		bytes = fread(buffer, 1, 32768, file);
		for (size_t i = 0; i < bytes; ++i) {
			char ch = buffer[i];
			if (ch == '\n') {
				if ((index + i) >= offset) {
					lineOffsetPosition = line.length() - ((index + i) - offset);
					bytes = 0;
					break;
				}
				++currentLine;
				line.clear();
			} else {
				line.push_back(ch);
			}
		}
		index += bytes;
	} while (bytes == 32768);
	fclose(file);

	std::cout << "Line " << currentLine << ':' << std::endl;
	std::cout << line << std::endl;
	for (size_t i = 0; i < lineOffsetPosition; i++) {
		if (line[i] == '\t') {
			std::cout << '\t';
		} else {
			std::cout << ' ';
		}
	}
	std::cout << '^' << std::endl;
}

inline static uint32_t circularShift(int bits, uint32_t value)
{
	return (value << bits) | (value >> (32 - bits));
}

static void processSHA1MessageBlock(const uint8_t* messageBlock, uint32_t* H)
{
	uint32_t W[80];
	for (int i = 0; i < 16; ++i) {
		const size_t offset = i << 2;
		W[i] = messageBlock[offset] << 24 | messageBlock[offset + 1] << 16 | messageBlock[offset + 2] << 8 | messageBlock[offset + 3];
	}

	for (int i = 16; i < 80; ++i) {
		W[i] = circularShift(1, W[i - 3] ^ W[i - 8] ^ W[i - 14] ^ W[i - 16]);
	}

	uint32_t A = H[0], B = H[1], C = H[2], D = H[3], E = H[4];

	for (int i = 0; i < 20; ++i) {
		const uint32_t tmp = circularShift(5, A) + ((B & C) | ((~B) & D)) + E + W[i] + 0x5A827999;
		E = D; D = C; C = circularShift(30, B); B = A; A = tmp;
	}

	for (int i = 20; i < 40; ++i) {
		const uint32_t tmp = circularShift(5, A) + (B ^ C ^ D) + E + W[i] + 0x6ED9EBA1;
		E = D; D = C; C = circularShift(30, B); B = A; A = tmp;
	}

	for (int i = 40; i < 60; ++i) {
		const uint32_t tmp = circularShift(5, A) + ((B & C) | (B & D) | (C & D)) + E + W[i] + 0x8F1BBCDC;
		E = D; D = C; C = circularShift(30, B); B = A; A = tmp;
	}

	for (int i = 60; i < 80; ++i) {
		const uint32_t tmp = circularShift(5, A) + (B ^ C ^ D) + E + W[i] + 0xCA62C1D6;
		E = D; D = C; C = circularShift(30, B); B = A; A = tmp;
	}

	H[0] += A;
	H[1] += B;
	H[2] += C;
	H[3] += D;
	H[4] += E;
}

std::string transformToSHA1(const std::string& input)
{
	uint32_t H[] = {
		0x67452301,
		0xEFCDAB89,
		0x98BADCFE,
		0x10325476,
		0xC3D2E1F0
	};

	uint8_t messageBlock[64];
	size_t index = 0;

	uint32_t length_low = 0;
	uint32_t length_high = 0;
	for (char ch : input) {
		messageBlock[index++] = ch;

		length_low += 8;
		if (length_low == 0) {
			length_high++;
		}

		if (index == 64) {
			processSHA1MessageBlock(messageBlock, H);
			index = 0;
		}
	}

	messageBlock[index++] = 0x80;

	if (index > 56) {
		while (index < 64) {
			messageBlock[index++] = 0;
		}

		processSHA1MessageBlock(messageBlock, H);
		index = 0;
	}

	while (index < 56) {
		messageBlock[index++] = 0;
	}

	messageBlock[56] = length_high >> 24;
	messageBlock[57] = length_high >> 16;
	messageBlock[58] = length_high >> 8;
	messageBlock[59] = length_high;

	messageBlock[60] = length_low >> 24;
	messageBlock[61] = length_low >> 16;
	messageBlock[62] = length_low >> 8;
	messageBlock[63] = length_low;

	processSHA1MessageBlock(messageBlock, H);

	char hexstring[41];
	static const char hexDigits[] = {"0123456789abcdef"};
	for (int hashByte = 20; --hashByte >= 0;) {
		const uint8_t byte = H[hashByte >> 2] >> (((3 - hashByte) & 3) << 3);
		index = hashByte << 1;
		hexstring[index] = hexDigits[byte >> 4];
		hexstring[index + 1] = hexDigits[byte & 15];
	}
	return std::string(hexstring, 40);
}

uint8_t getLiquidColor(uint8_t type)
{
	uint8_t result = 0;
	switch (type)
	{
	case 1:
		result = 1;
		break;
	case 0:
		result = 0;
		break;
	case 6:
		result = 4;
		break;
	case 3:
	case 4:
	case 7:
		result = 3;
		break;
	case 9:
		result = 6;
		break;
	case 2:
	case 10:
		result = 7;
		break;
	case 5:
	case 11:
		result = 2;
		break;
	case 8:
	case 12:
		result = 5;
		break;
	default:
		result = 0;
		break;
	}
	return result;
}

void extractArticleAndName(std::string& data, std::string& article, std::string& name)
{
	std::string xarticle = data.substr(0, 3);
	if (xarticle == "an ")
	{
		name = data.substr(3, data.size());
		article = "an";
	} else {
		xarticle = data.substr(0, 2);
		if (xarticle == "a ")
		{
			name = data.substr(2, data.size());
			article = "a";
		} else {
			name = data;
			article = "";
		}
	}
}

std::string pluralizeString(std::string str)
{
	if (str == "meat") return "meat";

	int n = str.length();
	char ch = str[n - 1];
	char ch2 = str[n - 2];

	std::string str2;
	if (ch == 'y')
		str2 = str.substr(0, n - 1) + "ies";
	else if (ch == 'o' || ch == 's' || ch == 'x')
		str2 = str + "es";
	else if (ch == 'h'&& ch2 == 'c')
		str2 = str + "es";
	else if (ch == 'f')
		str2 = str.substr(0, n - 1) + "ves";
	else if (ch == 'e'&&ch2 == 'f')
		str2 = str.substr(0, n - 2) + "ves";
	else
		str2 = str + "s";

	return str2;
}

void replaceString(std::string& str, const std::string& sought, const std::string& replacement)
{
	size_t pos = 0;
	size_t start = 0;
	size_t soughtLen = sought.length();
	size_t replaceLen = replacement.length();

	while ((pos = str.find(sought, start)) != std::string::npos) {
		str = str.substr(0, pos) + replacement + str.substr(pos + soughtLen);
		start = pos + replaceLen;
	}
}

void trim_right(std::string& source, char t)
{
	source.erase(source.find_last_not_of(t) + 1);
}

void trim_left(std::string& source, char t)
{
	source.erase(0, source.find_first_not_of(t));
}

void toLowerCaseString(std::string& source)
{
	std::transform(source.begin(), source.end(), source.begin(), tolower);
}

std::string asLowerCaseString(std::string source)
{
	toLowerCaseString(source);
	return source;
}

std::string asUpperCaseString(std::string source)
{
	std::transform(source.begin(), source.end(), source.begin(), toupper);
	return source;
}

StringVec explodeString(const std::string& inString, const std::string& separator, int32_t limit/* = -1*/)
{
	StringVec returnVector;
	std::string::size_type start = 0, end = 0;

	while (--limit != -1 && (end = inString.find(separator, start)) != std::string::npos) {
		returnVector.push_back(inString.substr(start, end - start));
		start = end + separator.size();
	}

	returnVector.push_back(inString.substr(start));
	return returnVector;
}

IntegerVec vectorAtoi(const StringVec& stringVector)
{
	IntegerVec returnVector;
	for (const auto& string : stringVector) {
		returnVector.push_back(std::stoi(string));
	}
	return returnVector;
}

std::mt19937& getRandomGenerator()
{
	static std::random_device rd;
	static std::mt19937 generator(rd());
	return generator;
}

int32_t uniform_random(int32_t minNumber, int32_t maxNumber)
{
	static std::uniform_int_distribution<int32_t> uniformRand;
	if (minNumber == maxNumber) {
		return minNumber;
	} else if (minNumber > maxNumber) {
		std::swap(minNumber, maxNumber);
	}
	return uniformRand(getRandomGenerator(), std::uniform_int_distribution<int32_t>::param_type(minNumber, maxNumber));
}

int32_t normal_random(int32_t minNumber, int32_t maxNumber)
{
	static std::normal_distribution<float> normalRand(0.5f, 0.25f);
	if (minNumber == maxNumber) {
		return minNumber;
	} else if (minNumber > maxNumber) {
		std::swap(minNumber, maxNumber);
	}

	int32_t increment;
	const int32_t diff = maxNumber - minNumber;
	const float v = normalRand(getRandomGenerator());
	if (v < 0.0) {
		increment = diff / 2;
	} else if (v > 1.0) {
		increment = (diff + 1) / 2;
	} else {
		increment = round(v * diff);
	}
	return minNumber + increment;
}

bool boolean_random(double probability/* = 0.5*/)
{
	static std::bernoulli_distribution booleanRand;
	return booleanRand(getRandomGenerator(), std::bernoulli_distribution::param_type(probability));
}

void trimString(std::string& str)
{
	str.erase(str.find_last_not_of(' ') + 1);
	str.erase(0, str.find_first_not_of(' '));
}

std::string convertIPToString(uint32_t ip)
{
	char buffer[17];

	int res = sprintf(buffer, "%u.%u.%u.%u", ip & 0xFF, (ip >> 8) & 0xFF, (ip >> 16) & 0xFF, (ip >> 24));
	if (res < 0) {
		return {};
	}

	return buffer;
}

std::string formatDate(time_t time)
{
	const tm* tms = localtime(&time);
	if (!tms) {
		return {};
	}

	char buffer[20];
	int res = sprintf(buffer, "%02d/%02d/%04d %02d:%02d:%02d", tms->tm_mday, tms->tm_mon + 1, tms->tm_year + 1900, tms->tm_hour, tms->tm_min, tms->tm_sec);
	if (res < 0) {
		return {};
	}
	return {buffer, 19};
}

std::string formatDateShort(time_t time)
{
	const tm* tms = localtime(&time);
	if (!tms) {
		return {};
	}

	char buffer[12];
	size_t res = strftime(buffer, 12, "%d %b %Y", tms);
	if (res == 0) {
		return {};
	}
	return {buffer, 11};
}

Direction getDirection(const std::string& string)
{
	Direction direction = DIRECTION_NORTH;

	if (string == "north" || string == "n" || string == "0") {
		direction = DIRECTION_NORTH;
	} else if (string == "east" || string == "e" || string == "1") {
		direction = DIRECTION_EAST;
	} else if (string == "south" || string == "s" || string == "2") {
		direction = DIRECTION_SOUTH;
	} else if (string == "west" || string == "w" || string == "3") {
		direction = DIRECTION_WEST;
	} else if (string == "southwest" || string == "south west" || string == "south-west" || string == "sw" || string == "4") {
		direction = DIRECTION_SOUTHWEST;
	} else if (string == "southeast" || string == "south east" || string == "south-east" || string == "se" || string == "5") {
		direction = DIRECTION_SOUTHEAST;
	} else if (string == "northwest" || string == "north west" || string == "north-west" || string == "nw" || string == "6") {
		direction = DIRECTION_NORTHWEST;
	} else if (string == "northeast" || string == "north east" || string == "north-east" || string == "ne" || string == "7") {
		direction = DIRECTION_NORTHEAST;
	}

	return direction;
}

Position getNextPosition(Direction direction, Position pos)
{
	switch (direction) {
		case DIRECTION_NORTH:
			pos.y--;
			break;

		case DIRECTION_SOUTH:
			pos.y++;
			break;

		case DIRECTION_WEST:
			pos.x--;
			break;

		case DIRECTION_EAST:
			pos.x++;
			break;

		case DIRECTION_SOUTHWEST:
			pos.x--;
			pos.y++;
			break;

		case DIRECTION_NORTHWEST:
			pos.x--;
			pos.y--;
			break;

		case DIRECTION_NORTHEAST:
			pos.x++;
			pos.y--;
			break;

		case DIRECTION_SOUTHEAST:
			pos.x++;
			pos.y++;
			break;

		default:
			break;
	}

	return pos;
}

Direction getDirectionTo(const Position& from, const Position& to)
{
	Direction dir;

	int32_t x_offset = Position::getOffsetX(from, to);
	if (x_offset < 0) {
		dir = DIRECTION_EAST;
		x_offset = std::abs(x_offset);
	} else {
		dir = DIRECTION_WEST;
	}

	int32_t y_offset = Position::getOffsetY(from, to);
	if (y_offset >= 0) {
		if (y_offset > x_offset) {
			dir = DIRECTION_NORTH;
		} else if (y_offset == x_offset) {
			if (dir == DIRECTION_EAST) {
				dir = DIRECTION_NORTHEAST;
			} else {
				dir = DIRECTION_NORTHWEST;
			}
		}
	} else {
		y_offset = std::abs(y_offset);
		if (y_offset > x_offset) {
			dir = DIRECTION_SOUTH;
		} else if (y_offset == x_offset) {
			if (dir == DIRECTION_EAST) {
				dir = DIRECTION_SOUTHEAST;
			} else {
				dir = DIRECTION_SOUTHWEST;
			}
		}
	}
	return dir;
}

struct MagicEffectNames {
	const char* name;
	MagicEffectClasses effect;
};

struct ShootTypeNames {
	const char* name;
	ShootType_t shoot;
};

struct CombatTypeNames {
	const char* name;
	CombatType_t combat;
};

struct AmmoTypeNames {
	const char* name;
	Ammo_t ammoType;
};

struct WeaponActionNames {
	const char* name;
	WeaponAction_t weaponAction;
};

struct SkullNames {
	const char* name;
	Skulls_t skull;
};

struct FluidNames {
	const char* name;
	FluidTypes_t fluidType;
};

MagicEffectNames magicEffectNames[] = {
	{"redspark",		CONST_ME_DRAWBLOOD},
	{"bluebubble",		CONST_ME_LOSEENERGY},
	{"poff",		CONST_ME_POFF},
	{"yellowspark",		CONST_ME_BLOCKHIT},
	{"explosionarea",	CONST_ME_EXPLOSIONAREA},
	{"explosion",		CONST_ME_EXPLOSIONHIT},
	{"firearea",		CONST_ME_FIREAREA},
	{"yellowbubble",	CONST_ME_YELLOW_RINGS},
	{"greenbubble",		CONST_ME_GREEN_RINGS},
	{"blackspark",		CONST_ME_HITAREA},
	{"teleport",		CONST_ME_TELEPORT},
	{"energy",		CONST_ME_ENERGYHIT},
	{"blueshimmer",		CONST_ME_MAGIC_BLUE},
	{"redshimmer",		CONST_ME_MAGIC_RED},
	{"greenshimmer",	CONST_ME_MAGIC_GREEN},
	{"fire",		CONST_ME_HITBYFIRE},
	{"greenspark",		CONST_ME_HITBYPOISON},
	{"mortarea",		CONST_ME_MORTAREA},
	{"greennote",		CONST_ME_SOUND_GREEN},
	{"rednote",		CONST_ME_SOUND_RED},
	{"poison",		CONST_ME_POISONAREA},
	{"yellownote",		CONST_ME_SOUND_YELLOW},
	{"purplenote",		CONST_ME_SOUND_PURPLE},
	{"bluenote",		CONST_ME_SOUND_BLUE},
	{"whitenote",		CONST_ME_SOUND_WHITE},
	{"stun",		CONST_ME_STUN},
	{"sleep",		CONST_ME_SLEEP},
	{"groundshaker",	CONST_ME_GROUNDSHAKER},
	{"hearts",		CONST_ME_HEARTS},
	{"fireattack",		CONST_ME_FIREATTACK},
	{"smallclouds",		CONST_ME_SMALLCLOUDS},
	{"holydamage",		CONST_ME_HOLYDAMAGE},
	{"icearea",		CONST_ME_ICEAREA},
	{"icetornado",		CONST_ME_ICETORNADO},
	{"iceattack",		CONST_ME_ICEATTACK},
	{"giantice",		CONST_ME_GIANTICE},
	{"critical",		CONST_ME_CRITICAL_DAMAGE},
};

ShootTypeNames shootTypeNames[] = {
	{"spear",		CONST_ANI_SPEAR},
	{"bolt",		CONST_ANI_BOLT},
	{"arrow",		CONST_ANI_ARROW},
	{"fire",		CONST_ANI_FIRE},
	{"energy",		CONST_ANI_ENERGY},
	{"poisonarrow",		CONST_ANI_POISONARROW},
	{"burstarrow",		CONST_ANI_BURSTARROW},
	{"throwingstar",	CONST_ANI_THROWINGSTAR},
	{"throwingknife",	CONST_ANI_THROWINGKNIFE},
	{"smallstone",		CONST_ANI_SMALLSTONE},
	{"death",		CONST_ANI_DEATH},
	{"largerock",		CONST_ANI_LARGEROCK},
	{"snowball",		CONST_ANI_SNOWBALL},
	{"powerbolt",		CONST_ANI_POWERBOLT},
	{"poison",		CONST_ANI_POISON},
	{"drillarrow",		CONST_ANI_DRILLARROW},
	{"drillbolt",		CONST_ANI_DRILLBOLT},
	{"heavyarrow",		CONST_ANI_HEAVYARROW},
	{"heavybolt",		CONST_ANI_HEAVYBOLT},
	{"destructionarrow",		CONST_ANI_DESTRUCTIONARROW},
	{"ancientarrow",		CONST_ANI_ANCIENTARROW},
	{"ancientbolt",		CONST_ANI_ANCIENTBOLT},
	{"assassinstar",		CONST_ANI_ASSASSINSTAR},
	{"ice",			CONST_ANI_ICE},
	{"holy",		CONST_ANI_HOLY},
	{"energyball",		CONST_ANI_ENERGYBALL},
	{"smallice",		CONST_ANI_SMALLICE},
	{"smallholy",		CONST_ANI_SMALLHOLY},
};

CombatTypeNames combatTypeNames[] = {
	{"physical",		COMBAT_PHYSICALDAMAGE},
	{"energy",		COMBAT_ENERGYDAMAGE},
	{"earth",		COMBAT_EARTHDAMAGE},
	{"poison",			COMBAT_EARTHDAMAGE},
	{"fire",		COMBAT_FIREDAMAGE},
	{"undefined",		COMBAT_UNDEFINEDDAMAGE},
	{"lifedrain",		COMBAT_LIFEDRAIN},
	{"manadrain",		COMBAT_MANADRAIN},
	{"healing",		COMBAT_HEALING},
	{"ice", 		COMBAT_ICEDAMAGE},
	{"holy", 		COMBAT_HOLYDAMAGE},
	{"death",	COMBAT_DEATHDAMAGE},
};

AmmoTypeNames ammoTypeNames[] = {
	{"spear",		AMMO_SPEAR},
	{"bolt",		AMMO_BOLT},
	{"arrow",		AMMO_ARROW},
	{"poisonarrow",		AMMO_ARROW},
	{"burstarrow",		AMMO_ARROW},
	{"throwingstar",	AMMO_THROWINGSTAR},
	{"throwingknife",	AMMO_THROWINGKNIFE},
	{"smallstone",		AMMO_STONE},
	{"largerock",		AMMO_STONE},
	{"snowball",		AMMO_SNOWBALL},
	{"powerbolt",		AMMO_BOLT},
};

WeaponActionNames weaponActionNames[] = {
	{"move",		WEAPONACTION_MOVE},
	{"removecharge",	WEAPONACTION_REMOVECHARGE},
	{"removecount",		WEAPONACTION_REMOVECOUNT},
};

SkullNames skullNames[] = {
	{"none",	SKULL_NONE},
	{"yellow",	SKULL_YELLOW},
	{"green",	SKULL_GREEN},
	{"white",	SKULL_WHITE},
	{"red",		SKULL_RED},
};

FluidNames fluidNames[] = {
	{"none",			FLUID_NONE},
	{"water",			FLUID_WATER},
	{"wine",			FLUID_WINE},
	{"beer",			FLUID_BEER},
	{"mud",				FLUID_MUD},
	{"blood",			FLUID_BLOOD},
	{"slime",			FLUID_SLIME},
	{"oil",				FLUID_OIL},
	{"urine",			FLUID_URINE},
	{"milk",			FLUID_MILK},
	{"manafluid",		FLUID_MANAFLUID},
	{"lifefluid",		FLUID_LIFEFLUID},
	{"lemonade",		FLUID_LEMONADE}
};

MagicEffectClasses getMagicEffect(const std::string& strValue)
{
	for (auto& magicEffectName : magicEffectNames) {
		if (strcasecmp(strValue.c_str(), magicEffectName.name) == 0) {
			return magicEffectName.effect;
		}
	}
	return CONST_ME_NONE;
}

ShootType_t getShootType(const std::string& strValue)
{
	for (size_t i = 0, size = sizeof(shootTypeNames) / sizeof(ShootTypeNames); i < size; ++i) {
		if (strcasecmp(strValue.c_str(), shootTypeNames[i].name) == 0) {
			return shootTypeNames[i].shoot;
		}
	}
	return CONST_ANI_NONE;
}

CombatType_t getCombatType(const std::string& strValue)
{
	for (size_t i = 0, size = sizeof(combatTypeNames) / sizeof(CombatTypeNames); i < size; ++i) {
		if (strcasecmp(strValue.c_str(), combatTypeNames[i].name) == 0) {
			return combatTypeNames[i].combat;
		}
	}
	return COMBAT_NONE;
}

std::string getCombatName(CombatType_t combatType)
{
	for (size_t i = 0, size = sizeof(combatTypeNames) / sizeof(CombatTypeNames); i < size; ++i) {
		if (combatTypeNames[i].combat == combatType) {
			return combatTypeNames[i].name;
		}
	}
	return "unknown";
}

Ammo_t getAmmoType(const std::string& strValue)
{
	for (size_t i = 0, size = sizeof(ammoTypeNames) / sizeof(AmmoTypeNames); i < size; ++i) {
		if (strcasecmp(strValue.c_str(), ammoTypeNames[i].name) == 0) {
			return ammoTypeNames[i].ammoType;
		}
	}
	return AMMO_NONE;
}

WeaponAction_t getWeaponAction(const std::string& strValue)
{
	for (size_t i = 0, size = sizeof(weaponActionNames) / sizeof(WeaponActionNames); i < size; ++i) {
		if (strcasecmp(strValue.c_str(), weaponActionNames[i].name) == 0) {
			return weaponActionNames[i].weaponAction;
		}
	}
	return WEAPONACTION_NONE;
}

Skulls_t getSkullType(const std::string& strValue)
{
	for (size_t i = 0, size = sizeof(skullNames) / sizeof(SkullNames); i < size; ++i) {
		if (strcasecmp(strValue.c_str(), skullNames[i].name) == 0) {
			return skullNames[i].skull;
		}
	}
	return SKULL_NONE;
}

FluidTypes_t getFluidType(const std::string& strValue)
{
	for (size_t i = 0, size = sizeof(fluidNames) / sizeof(FluidNames); i < size; ++i) {
		if (strcasecmp(strValue.c_str(), fluidNames[i].name) == 0) {
			return fluidNames[i].fluidType;
		}
	}
	return FLUID_NONE;
}

std::string getSkillName(uint8_t skillid)
{
	switch (skillid) {
		case SKILL_FIST:
			return "fist fighting";

		case SKILL_CLUB:
			return "club fighting";

		case SKILL_SWORD:
			return "sword fighting";

		case SKILL_AXE:
			return "axe fighting";

		case SKILL_DISTANCE:
			return "distance fighting";

		case SKILL_SHIELD:
			return "shielding";

		case SKILL_FISHING:
			return "fishing";

		case SKILL_MAGLEVEL:
			return "magic level";

		case SKILL_LEVEL:
			return "level";

		default:
			return "unknown";
	}
}

std::string ucfirst(std::string str)
{
	for (char& i : str) {
		if (i != ' ') {
			i = toupper(i);
			break;
		}
	}
	return str;
}

std::string ucwords(std::string str)
{
	size_t strLength = str.length();
	if (strLength == 0) {
		return str;
	}

	str[0] = toupper(str.front());
	for (size_t i = 1; i < strLength; ++i) {
		if (str[i - 1] == ' ') {
			str[i] = toupper(str[i]);
		}
	}

	return str;
}

bool booleanString(const std::string& str)
{
	if (str.empty()) {
		return false;
	}

	char ch = tolower(str.front());
	return ch != 'f' && ch != 'n' && ch != '0';
}

std::string getWeaponName(WeaponType_t weaponType)
{
	switch (weaponType) {
		case WEAPON_SWORD: return "sword";
		case WEAPON_CLUB: return "club";
		case WEAPON_AXE: return "axe";
		case WEAPON_DISTANCE: return "distance";
		case WEAPON_WAND: return "wand";
		case WEAPON_AMMO: return "ammunition";
		case WEAPON_MELEE: return "melee";
		default: return std::string();
	}
}

size_t combatTypeToIndex(CombatType_t combatType)
{
	switch (combatType) {
		case COMBAT_PHYSICALDAMAGE:
			return 0;
		case COMBAT_ENERGYDAMAGE:
			return 1;
		case COMBAT_EARTHDAMAGE:
			return 2;
		case COMBAT_FIREDAMAGE:
			return 3;
		case COMBAT_UNDEFINEDDAMAGE:
			return 4;
		case COMBAT_LIFEDRAIN:
			return 5;
		case COMBAT_MANADRAIN:
			return 6;
		case COMBAT_HEALING:
			return 7;
		default:
			return 0;
	}
}

CombatType_t indexToCombatType(size_t v)
{
	return static_cast<CombatType_t>(1 << v);
}

itemAttrTypes stringToItemAttribute(const std::string& str)
{
	if (str == "aid") {
		return ITEM_ATTRIBUTE_ACTIONID;
	} else if (str == "mid") {
		return ITEM_ATTRIBUTE_MOVEMENTID;
	} else if (str == "description") {
		return ITEM_ATTRIBUTE_DESCRIPTION;
	} else if (str == "text") {
		return ITEM_ATTRIBUTE_TEXT;
	} else if (str == "date") {
		return ITEM_ATTRIBUTE_DATE;
	} else if (str == "writer") {
		return ITEM_ATTRIBUTE_WRITER;
	} else if (str == "name") {
		return ITEM_ATTRIBUTE_NAME;
	} else if (str == "article") {
		return ITEM_ATTRIBUTE_ARTICLE;
	} else if (str == "pluralname") {
		return ITEM_ATTRIBUTE_PLURALNAME;
	} else if (str == "weight") {
		return ITEM_ATTRIBUTE_WEIGHT;
	} else if (str == "attack") {
		return ITEM_ATTRIBUTE_ATTACK;
	} else if (str == "defense") {
		return ITEM_ATTRIBUTE_DEFENSE;
	} else if (str == "armor") {
		return ITEM_ATTRIBUTE_ARMOR;
	} else if (str == "shootrange") {
		return ITEM_ATTRIBUTE_SHOOTRANGE;
	} else if (str == "owner") {
		return ITEM_ATTRIBUTE_OWNER;
	} else if (str == "duration") {
		return ITEM_ATTRIBUTE_DURATION;
	} else if (str == "decaystate") {
		return ITEM_ATTRIBUTE_DECAYSTATE;
	} else if (str == "corpseowner") {
		return ITEM_ATTRIBUTE_CORPSEOWNER;
	} else if (str == "charges") {
		return ITEM_ATTRIBUTE_CHARGES;
	} else if (str == "fluidtype") {
		return ITEM_ATTRIBUTE_FLUIDTYPE;
	} else if (str == "doorid") {
		return ITEM_ATTRIBUTE_DOORID;
	} else if (str == "lifeleech") {
		return ITEM_ATTRIBUTE_LIFELEECH;
	} else if (str == "manaleech") {
		return ITEM_ATTRIBUTE_MANALEECH;
	} else if (str == "critical") {
		return ITEM_ATTRIBUTE_CRITICAL;
	}
	return ITEM_ATTRIBUTE_NONE;
}

std::string getFirstLine(const std::string& str)
{
	std::string firstLine;
	firstLine.reserve(str.length());
	for (const char c : str) {
		if (c == '\n') {
			break;
		}
		firstLine.push_back(c);
	}
	return firstLine;
}

const char* getReturnMessage(ReturnValue value)
{
	switch (value) {
		case RETURNVALUE_DESTINATIONOUTOFREACH:
			return "Destination is out of reach.";

		case RETURNVALUE_NOTMOVEABLE:
			return "You cannot move this object.";

		case RETURNVALUE_DROPTWOHANDEDITEM:
			return "Drop the double-handed object first.";

		case RETURNVALUE_BOTHHANDSNEEDTOBEFREE:
			return "Both hands need to be free.";

		case RETURNVALUE_CANNOTBEDRESSED:
			return "You cannot dress this object there.";

		case RETURNVALUE_PUTTHISOBJECTINYOURHAND:
			return "Put this object in your hand.";

		case RETURNVALUE_PUTTHISOBJECTINBOTHHANDS:
			return "Put this object in both hands.";

		case RETURNVALUE_CANONLYUSEONEWEAPON:
			return "You may only use one weapon.";

		case RETURNVALUE_TOOFARAWAY:
			return "Too far away.";

		case RETURNVALUE_FIRSTGODOWNSTAIRS:
			return "First go downstairs.";

		case RETURNVALUE_FIRSTGOUPSTAIRS:
			return "First go upstairs.";

		case RETURNVALUE_NOTENOUGHCAPACITY:
			return "This object is too heavy for you to carry.";

		case RETURNVALUE_CONTAINERNOTENOUGHROOM:
			return "You cannot put more objects in this container.";

		case RETURNVALUE_NEEDEXCHANGE:
		case RETURNVALUE_NOTENOUGHROOM:
			return "There is not enough room.";

		case RETURNVALUE_CANNOTPICKUP:
			return "You cannot take this object.";

		case RETURNVALUE_CANNOTTHROW:
			return "You cannot throw there.";

		case RETURNVALUE_THEREISNOWAY:
			return "There is no way.";

		case RETURNVALUE_THISISIMPOSSIBLE:
			return "This is impossible.";

		case RETURNVALUE_PLAYERISPZLOCKED:
			return "You can not enter a protection zone after attacking another player.";

		case RETURNVALUE_PLAYERISNOTINVITED:
			return "You are not invited.";

		case RETURNVALUE_CREATUREDOESNOTEXIST:
			return "Creature does not exist.";

		case RETURNVALUE_DEPOTISFULL:
			return "You cannot put more items in this depot.";

		case RETURNVALUE_CANNOTUSETHISOBJECT:
			return "You cannot use this object.";

		case RETURNVALUE_PLAYERWITHTHISNAMEISNOTONLINE:
			return "A player with this name is not online.";

		case RETURNVALUE_NOTREQUIREDLEVELTOUSERUNE:
			return "You do not have the required magic level to use this rune.";

		case RETURNVALUE_YOUAREALREADYTRADING:
			return "You are already trading.";

		case RETURNVALUE_THISPLAYERISALREADYTRADING:
			return "This player is already trading.";

		case RETURNVALUE_YOUMAYNOTLOGOUTDURINGAFIGHT:
			return "You may not logout during or immediately after a fight!";

		case RETURNVALUE_DIRECTPLAYERSHOOT:
			return "You are not allowed to shoot directly on players.";

		case RETURNVALUE_NOTENOUGHLEVEL:
			return "You do not have enough level.";

		case RETURNVALUE_NOTENOUGHMAGICLEVEL:
			return "You do not have enough magic level.";

		case RETURNVALUE_NOTENOUGHMANA:
			return "You do not have enough mana.";

		case RETURNVALUE_NOTENOUGHSOUL:
			return "You do not have enough soul.";

		case RETURNVALUE_YOUAREEXHAUSTED:
			return "You are exhausted.";

		case RETURNVALUE_CANONLYUSETHISRUNEONCREATURES:
			return "You can only use this rune on creatures.";

		case RETURNVALUE_PLAYERISNOTREACHABLE:
			return "Player is not reachable.";

		case RETURNVALUE_CREATUREISNOTREACHABLE:
			return "Creature is not reachable.";

		case RETURNVALUE_ACTIONNOTPERMITTEDINPROTECTIONZONE:
			return "This action is not permitted in a protection zone.";

		case RETURNVALUE_YOUMAYNOTATTACKTHISPLAYER:
			return "You may not attack this player.";

		case RETURNVALUE_YOUMAYNOTATTACKTHISCREATURE:
			return "You may not attack this creature.";

		case RETURNVALUE_YOUMAYNOTATTACKAPERSONINPROTECTIONZONE:
			return "You may not attack a person in a protection zone.";

		case RETURNVALUE_YOUMAYNOTATTACKAPERSONWHILEINPROTECTIONZONE:
			return "You may not attack a person while you are in a protection zone.";

		case RETURNVALUE_YOUCANONLYUSEITONCREATURES:
			return "You can only use it on creatures.";

		case RETURNVALUE_TURNSECUREMODETOATTACKUNMARKEDPLAYERS:
			return "Turn secure mode off if you really want to attack unmarked players.";

		case RETURNVALUE_YOUNEEDPREMIUMACCOUNT:
			return "You need a premium account.";

		case RETURNVALUE_YOUNEEDTOLEARNTHISSPELL:
			return "You need to learn this spell first.";

		case RETURNVALUE_YOURVOCATIONCANNOTUSETHISSPELL:
			return "Your vocation cannot use this spell.";

		case RETURNVALUE_YOUNEEDAWEAPONTOUSETHISSPELL:
			return "You need to equip a weapon to use this spell.";

		case RETURNVALUE_PLAYERISPZLOCKEDLEAVEPVPZONE:
			return "You can not leave a pvp zone after attacking another player.";

		case RETURNVALUE_PLAYERISPZLOCKEDENTERPVPZONE:
			return "You can not enter a pvp zone after attacking another player.";

		case RETURNVALUE_ACTIONNOTPERMITTEDINANOPVPZONE:
			return "This action is not permitted in a non pvp zone.";

		case RETURNVALUE_YOUCANNOTLOGOUTHERE:
			return "You can not logout here.";

		case RETURNVALUE_YOUNEEDAMAGICITEMTOCASTSPELL:
			return "You need a magic item to cast this spell.";

		case RETURNVALUE_CANNOTCONJUREITEMHERE:
			return "You cannot conjure items here.";

		case RETURNVALUE_YOUNEEDTOSPLITYOURSPEARS:
			return "You need to split your spears first.";

		case RETURNVALUE_NAMEISTOOAMBIGIOUS:
			return "Name is too ambigious.";

		case RETURNVALUE_CANONLYUSEONESHIELD:
			return "You may use only one shield.";

		case RETURNVALUE_NOPARTYMEMBERSINRANGE:
			return "No party members in range.";

		case RETURNVALUE_YOUARENOTTHEOWNER:
			return "You are not the owner.";

		default: // RETURNVALUE_NOTPOSSIBLE, etc
			return "Sorry, not possible.";
	}
}

void getFilesInDirectory(const boost::filesystem::path& root, const std::string& ext, std::vector<boost::filesystem::path>& ret)
{
	if (!boost::filesystem::exists(root)) {
		return;
	}

	if (boost::filesystem::is_directory(root))
	{
		boost::filesystem::recursive_directory_iterator it(root);
		boost::filesystem::recursive_directory_iterator endit;
		while (it != endit)
		{
			if (boost::filesystem::is_regular_file(*it) && it->path().extension() == ext)
			{
				ret.push_back(it->path().filename());
			}
			++it;
		}
	}
}
